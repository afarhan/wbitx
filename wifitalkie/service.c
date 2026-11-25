// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ashhar Farhan
//
// service.c — the talkie's line to my.sbitx.net, as a radio's daemon keeps it.
//
// To my.sbitx.net the talkie is a radio of its owner's account, one that
// has VOIP online and nothing else: the service is not changed for it. What
// is said here is what clients/remote/zbitx_remote.c in zbitxre says, and
// PROTOCOL.md there has it exactly, under "Authentication" and "Reaching a
// radio through my.sbitx.net".
//
// THE HANDSHAKE
//
// The service speaks first, CHALLENGE and 32 hex digits. The talkie makes a
// nonce, and answers AUTH radio <callsign> <nonce> <proof>; the service
// answers WELCOME with the name the owner gave the talkie on the website and
// its own proof, which is checked, so that the talkie is not talking to an
// impostor. Then "radio", and OK. auth.c makes the proofs. Each step is
// waited for, STEP_WAIT_MS at most, and the name looked up first may take
// longer still; so the line is made on a task of its own, the connector,
// and the loop goes on meanwhile, the keys and the screen with it. The
// connector is given a copy of the settings to make it with, and the loop
// leaves the line alone until it says it is done. While it works the
// status says which step it is on, CONNECTING: LOOKING UP and so on.
//
// A step may wait on longer than it should: a name looked up while the
// WiFi goes, a send that never drains. If the WiFi goes, or the connector
// is not done in CONNECT_MOST_MS, its socket is shut, which wakes a send
// or a recv, and the line is tried again as after any failure. If it is
// still not done STUCK_MOST_MS after that, it is stuck where nothing wakes
// it, inside lwIP, and the talkie starts afresh: a task deleted there would
// leave lwIP to signal what it had freed.
//
// ERROR AUTH means the website does not know the talkie yet, or no longer:
// SERVICE says the key to add there, and the service is asked again every
// REFUSED_RETRY_MS. Any other failure is tried again after RETRY_FIRST_MS,
// and longer each time, to RETRY_MOST_MS.
//
// ONCE IT IS UP
//
// The talkie says its grid, GRID with nothing after it if none is set, and
// ALLOWS voip text streaming, as a radio with VOIP online does, with text
// too, and anyone may listen to it, as to an sBitx (voip.c, WHAT IS
// LISTENED TO: AUDIO ON and AUDIO OFF, and TUNED <FREQ> VOIP said here), and
// then nothing but PING a minute, and the words of text the screen sends
// (service_send_word()); a line not heard from for SILENCE_MOST_MS is
// taken to be lost. The service answers ALLOWS with VOIP STREAM <id>
// <nonce>, which voip.c takes up, and with TIME, the screen's clock; VOIP
// ROOM and VOIP BUSY go to voip.c too, for what it shows, and TEXT and
// TEXT SENT to the screen (PROTOCOL.md "Text, a word at a time").
// Whatever else the service says is shown on the console as it is, for
// this first talkie is also a way of seeing the service.
//
// THE STRETCH
//
// The answers are keyed by the cookie stretched with the callsign, 50,000
// rounds of HMAC-SHA256, some seconds on the C3. It is done here, a few
// rounds at each turn of the loop, before the first connection and again
// when the callsign or the cookie has changed, and kept in the settings.
//
// The line is plain TCP, not TLS: the author's ruling for all of
// my.sbitx.net's radio connections, amateur radio being unencrypted by law.

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>

#include "esp_random.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lwip/netdb.h"
#include "lwip/sockets.h"

#include "auth.h"
#include "console.h"
#include "network.h"
#include "service.h"
#include "settings.h"
#include "text.h"
#include "screen.h"
#include "voip.h"

#define NOT_OPEN (-1)
#define LINE_ROOM 512
#define TEXT_ROOM 256
#define STATUS_ROOM 64
#define NAME_ROOM 40
#define CHALLENGE_ROOM 72
#define NONCE_BYTES 16
#define NONCE_HEX_ROOM (2 * NONCE_BYTES + 1)
#define PORT_ROOM 8
#define STEP_WAIT_MS 4000
#define SBITX_ADDRESS "104.236.247.211"   // my.sbitx.net, 7 November 2026
#define REFUSED_RETRY_MS 15000
#define RETRY_FIRST_MS 5000
#define RETRY_MORE_MS 5000
#define RETRY_MOST_MS 15000
#define PING_EVERY_MS 60000
#define SILENCE_MOST_MS 150000
#define STRETCH_ROUNDS_A_TURN 100
#define MS_PER_SECOND 1000
#define US_PER_MS 1000
#define CONNECTOR_STACK 8192
#define CONNECTOR_PRIORITY 1
#define CONNECT_MOST_MS 30000
#define STUCK_MOST_MS 60000

#define AUTH_OK 0
#define AUTH_REFUSED 1
#define AUTH_NO_ANSWER 2

enum state {
	STATE_OFF,
	STATE_STRETCHING,
	STATE_WAITING,
	STATE_CONNECTING,
	STATE_ONLINE
};

static enum state state = STATE_OFF;
static struct auth_stretch stretch;
static long stretch_began_ms;
static int line_fd = NOT_OPEN;
static char gathered[LINE_ROOM];
static int gathered_used;
static struct sockaddr_in service_address;
static long retry_ms;
static long retry_after_ms = RETRY_FIRST_MS;
static long next_ping_ms;
static long last_heard_ms;
static char grid_told[SETTINGS_GRID_ROOM];
static int is_grid_told;
#define TUNED_EVERY_LEAST_MS 1000
static uint32_t tuned_told;	// FREQ said in TUNED while the sound is on; 0 none
static long tuned_told_ms;
static int is_allows_told;
static char name[NAME_ROOM];
static char failed_because[TEXT_ROOM];
static char status[STATUS_ROOM];
static int is_refused;
static int is_restart_asked;
static TaskHandle_t connector;
static struct settings asked;
static volatile int is_connect_done;
static int connect_result;
static volatile int dialing_fd = NOT_OPEN;
static const char *volatile connect_step = "";
static long connect_began_ms;
static long shut_ms;

static uint8_t random_byte(void) {
	return (uint8_t)esp_random();
}

static void say_status(const char *text) {
	if (!strcmp(text, status))
		return;
	snprintf(status, sizeof(status), "%s", text);
	console_say("SERVICE %s", status);
}

static void close_line(void) {
	if (line_fd != NOT_OPEN)
		close(line_fd);
	line_fd = NOT_OPEN;
	dialing_fd = NOT_OPEN;
	gathered_used = 0;
	voip_close();
	voip_audio_off();
}

static int no_answer(const char *because) {
	snprintf(failed_because, sizeof(failed_because), "%s", because);
	return AUTH_NO_ANSWER;
}

static int send_text(const char *text) {
	size_t length = strlen(text);

	if (send(line_fd, text, length, 0) != (ssize_t)length)
		return -1;
	return 0;
}

// a line, waited for STEP_WAIT_MS at most, without its end
static int read_line_waiting(char *line, int room) {
	int used = 0;
	char c;

	while (recv(line_fd, &c, 1, 0) == 1) {
		if (c == '\n') {
			line[used] = 0;
			return 0;
		}
		if (c != '\r' && used < room - 1)
			line[used++] = c;
	}
	return -1;
}

// the service's address: my.sbitx.net's is SBITX_ADDRESS, not looked up,
// for a lookup took seconds; any other host is looked up
static int resolve(void) {
	struct addrinfo hints;
	struct addrinfo *found = NULL;
	char port[PORT_ROOM];

	if (!strcmp(asked.host, SETTINGS_DEFAULT_HOST)) {
		memset(&service_address, 0, sizeof(service_address));
		service_address.sin_family = AF_INET;
		service_address.sin_port = htons((uint16_t)asked.port);
		return inet_pton(AF_INET, SBITX_ADDRESS, &service_address.sin_addr) == 1 ? 0 : -1;
	}
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	snprintf(port, sizeof(port), "%u", (unsigned)asked.port);
	if (getaddrinfo(asked.host, port, &hints, &found) != 0 || found == NULL)
		return -1;
	memcpy(&service_address, found->ai_addr, sizeof(service_address));
	freeaddrinfo(found);
	return 0;
}

// a connection to the service, made within STEP_WAIT_MS, whose reads then
// wait STEP_WAIT_MS at most
static int dial(void) {
	struct timeval wait;
	fd_set writable;
	int error = 0;
	socklen_t error_length = sizeof(error);
	int fd;

	if (resolve() < 0) {
		no_answer("its name was not found");
		return NOT_OPEN;
	}
	fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd < 0) {
		no_answer("no socket");
		return NOT_OPEN;
	}
	dialing_fd = fd;
	fcntl(fd, F_SETFL, O_NONBLOCK);
	if (connect(fd, (struct sockaddr *)&service_address, sizeof(service_address)) < 0
			&& errno != EINPROGRESS) {
		dialing_fd = NOT_OPEN;
		close(fd);
		no_answer("the connection was refused");
		return NOT_OPEN;
	}
	FD_ZERO(&writable);
	FD_SET(fd, &writable);
	wait.tv_sec = STEP_WAIT_MS / MS_PER_SECOND;
	wait.tv_usec = 0;
	if (select(fd + 1, NULL, &writable, NULL, &wait) != 1
			|| getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &error_length) < 0 || error != 0) {
		dialing_fd = NOT_OPEN;
		close(fd);
		no_answer("no connection");
		return NOT_OPEN;
	}
	fcntl(fd, F_SETFL, 0);
	setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &wait, sizeof(wait));
	setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &wait, sizeof(wait));
	return fd;
}

static int authenticate(void) {
	uint8_t random_bytes[NONCE_BYTES];
	char nonce[NONCE_HEX_ROOM];
	char line[LINE_ROOM];
	char challenge[CHALLENGE_ROOM];
	char proof[SHA256_HEX_ROOM];
	char theirs[SHA256_HEX_ROOM + 16];
	int i;

	name[0] = 0;
	if (read_line_waiting(line, sizeof(line)) < 0)
		return no_answer("no CHALLENGE came");
	if (sscanf(line, "CHALLENGE %64s", challenge) != 1)
		return no_answer("not a CHALLENGE");
	esp_fill_random(random_bytes, sizeof(random_bytes));
	for (i = 0; i < NONCE_BYTES; i++)
		snprintf(nonce + 2 * i, 3, "%02x", random_bytes[i]);
	auth_proof(asked.stretched, "radio", asked.callsign, challenge, nonce, proof);
	snprintf(line, sizeof(line), "AUTH radio %s %s %s\n", asked.callsign, nonce, proof);
	if (send_text(line) < 0)
		return no_answer("AUTH could not be sent");
	if (read_line_waiting(line, sizeof(line)) < 0)
		return no_answer("no answer to AUTH");
	if (!strncmp(line, "ERROR AUTH", strlen("ERROR AUTH"))) {
		snprintf(failed_because, sizeof(failed_because), "%s", line);
		return AUTH_REFUSED;
	}
	if (sscanf(line, "WELCOME %39s %80s", name, theirs) != 2)
		return no_answer("not a WELCOME");
	auth_proof(asked.stretched, "server", asked.callsign, challenge, nonce, proof);
	if (strlen(theirs) != strlen(proof) || strcmp(proof, theirs))
		return no_answer("the service's proof is wrong");
	return AUTH_OK;
}

static void wait_to_retry(long now, long after) {
	close_line();
	state = STATE_WAITING;
	retry_ms = now + after;
}

static void lost(long now, const char *because) {
	console_say("SERVICE LOST: %s", because);
	wait_to_retry(now, retry_after_ms);
	retry_after_ms += RETRY_MORE_MS;
	if (retry_after_ms > RETRY_MOST_MS)
		retry_after_ms = RETRY_MOST_MS;
	say_status("NO ANSWER FROM SERVER");
}

// on the connector: the line made, authenticated and made a radio's
static int connect_line(void) {
	char line[LINE_ROOM];
	int result;

	connect_step = "LOOKING UP";
	line_fd = dial();
	if (line_fd == NOT_OPEN)
		return AUTH_NO_ANSWER;
	connect_step = "SIGNING IN";
	result = authenticate();
	connect_step = "MAKING A RADIO";
	if (result == AUTH_OK && (send_text("radio\n") < 0
			|| read_line_waiting(line, sizeof(line)) < 0 || strcmp(line, "OK"))) {
		no_answer("no OK to radio");
		result = AUTH_NO_ANSWER;
	}
	return result;
}

static void connector_task(void *unused) {
	(void)unused;
	for (;;) {
		ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
		connect_result = connect_line();
		connect_step = "";
		is_connect_done = 1;
	}
}

static void go_online(long now) {
	say_status("CONNECTING");
	memcpy(&asked, &settings, sizeof(asked));
	is_connect_done = 0;
	connect_step = "";
	connect_began_ms = now;
	shut_ms = 0;
	state = STATE_CONNECTING;
	xTaskNotifyGive(connector);
}

// the connector not done: its socket shut if the WiFi went or it took too
// long, and the talkie started afresh if that did not wake it
static void watch_connector(long now) {
	int fd = dialing_fd;

	if (shut_ms == 0 && (!network_is_up() || now - connect_began_ms >= CONNECT_MOST_MS)) {
		console_say("SERVICE CONNECTOR SHUT AT %s", connect_step);
		shut_ms = now;
		if (fd != NOT_OPEN)
			shutdown(fd, SHUT_RDWR);
		return;
	}
	if (shut_ms == 0 || now - shut_ms < STUCK_MOST_MS)
		return;
	console_say("SERVICE CONNECTOR STUCK AT %s: RESTARTING", connect_step);
	esp_restart();
}

// the connector done: online, or waiting to try again
static void connected(long now) {
	char said[STATUS_ROOM];
	char shown[AUTH_COOKIE_SHOWN_ROOM];
	int result = connect_result;

	state = STATE_WAITING;
	is_refused = result == AUTH_REFUSED;
	if (result == AUTH_REFUSED) {
		console_say("SERVICE SAYS %s", failed_because);
		wait_to_retry(now, REFUSED_RETRY_MS);
		auth_cookie_show(settings.cookie, shown);
		snprintf(said, sizeof(said), "ADD KEY %s AT MY.SBITX.NET", shown);
		say_status(said);
		return;
	}
	if (result != AUTH_OK) {
		lost(now, failed_because);
		return;
	}
	fcntl(line_fd, F_SETFL, O_NONBLOCK);
	state = STATE_ONLINE;
	next_ping_ms = now + PING_EVERY_MS;
	last_heard_ms = now;
	retry_after_ms = RETRY_FIRST_MS;
	is_grid_told = 0;
	is_allows_told = 0;
	tuned_told = 0;
	snprintf(said, sizeof(said), "ONLINE %s", name);
	say_status(said);
}

// TIME, TEXT and TEXT SENT, and a word refused, for the screen; 1 if line
// was one of them
static int text_line_heard(const char *line, long now) {
	char from[TEXT_PARTY_ROOM];
	char to[TEXT_PARTY_ROOM];
	char word[TEXT_WORD_ROOM];
	long long unix_s;
	int wpm;
	int count;

	if (sscanf(line, "TIME %lld", &unix_s) == 1) {
		screen_time_heard(unix_s, now);
		return 1;
	}
	if (sscanf(line, "TEXT SENT %d", &count) == 1) {
		screen_text_sent(count, now);
		return 1;
	}
	if (sscanf(line, "TEXT %10s %10s %d %40s", from, to, &wpm, word) == 4) {
		screen_text_heard(from, to, wpm, word, now);
		return 1;
	}
	if (!strncmp(line, "ERROR TEXT ", strlen("ERROR TEXT "))) {
		console_say("SERVICE SAYS %s", line);
		screen_text_refused(line + strlen("ERROR TEXT "), now);
		return 1;
	}
	return 0;
}

// a word of text to a callsign or a room's kHz, sent on the line; 0 if it
// was, -1 if the talkie is not on line
int service_send_word(const char *to, int wpm, const char *word) {
	char line[LINE_ROOM];

	if (state != STATE_ONLINE)
		return -1;
	snprintf(line, sizeof(line), "TEXT %s %d %s\n", to, wpm, word);
	return send_text(line);
}

static void heard(const char *line, long now) {
	char token[LINE_ROOM];
	char nonce[LINE_ROOM];
	long channel;
	int count;

	last_heard_ms = now;
	if (!strcmp(line, "PONG"))
		return;
	if (sscanf(line, "VOIP STREAM %16s %64s", token, nonce) == 2) {
		voip_open(&service_address, token, nonce, settings.stretched);
		return;
	}
	if (sscanf(line, "AUDIO ON %16s %64s", token, nonce) == 2) {
		voip_audio_on(token, nonce, settings.stretched, now);
		tuned_told = 0;
		return;
	}
	if (!strcmp(line, "AUDIO OFF")) {
		voip_audio_off();
		return;
	}
	if (sscanf(line, "VOIP ROOM %ld %d", &channel, &count) == 2) {
		voip_room_heard(channel, count);
		return;
	}
	if (!strncmp(line, "VOIP BUSY ", strlen("VOIP BUSY "))) {
		voip_busy_heard(line + strlen("VOIP BUSY "), now);
		return;
	}
	if (text_line_heard(line, now))
		return;
	console_say("SERVICE SAYS %s", line);
}

static void line_poll(long now) {
	char line[LINE_ROOM];
	char *end;
	int got;

	got = recv(line_fd, gathered + gathered_used, sizeof(gathered) - 1 - gathered_used, 0);
	if (got == 0) {
		lost(now, "the service closed the line");
		return;
	}
	if (got < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
		lost(now, "the line failed");
		return;
	}
	if (got > 0)
		gathered_used += got;
	gathered[gathered_used] = 0;
	while ((end = strchr(gathered, '\n')) != NULL) {
		*end = 0;
		snprintf(line, sizeof(line), "%s", gathered);
		line[strcspn(line, "\r")] = 0;
		gathered_used -= (int)(end + 1 - gathered);
		memmove(gathered, end + 1, gathered_used + 1);
		heard(line, now);
		if (state != STATE_ONLINE)
			return;
	}
	if (gathered_used >= (int)sizeof(gathered) - 1)
		gathered_used = 0;
	if (!is_grid_told || strcmp(grid_told, settings.grid)) {
		snprintf(line, sizeof(line), "GRID %s\n", settings.grid);
		snprintf(grid_told, sizeof(grid_told), "%s", settings.grid);
		is_grid_told = 1;
		if (send_text(line) < 0) {
			lost(now, "GRID could not be sent");
			return;
		}
	}
	if (!is_allows_told) {
		is_allows_told = 1;
		if (send_text("ALLOWS voip text streaming\n") < 0) {
			lost(now, "ALLOWS could not be sent");
			return;
		}
	}
	// where the talkie is tuned, for those listening; no oftener than a second
	if (voip_audio_is_on() && settings.hz != 0 && settings.hz != tuned_told
			&& now - tuned_told_ms >= TUNED_EVERY_LEAST_MS) {
		snprintf(line, sizeof(line), "TUNED %u VOIP\n", (unsigned)settings.hz);
		tuned_told = settings.hz;
		tuned_told_ms = now;
		if (send_text(line) < 0) {
			lost(now, "TUNED could not be sent");
			return;
		}
	}
	if (now >= next_ping_ms) {
		next_ping_ms = now + PING_EVERY_MS;
		if (send_text("PING\n") < 0) {
			lost(now, "PING could not be sent");
			return;
		}
	}
	if (now - last_heard_ms > SILENCE_MOST_MS)
		lost(now, "nothing heard for too long");
}

// the cookie, made the first time and after NEW_KEY
static void have_a_cookie(void) {
	char shown[AUTH_COOKIE_SHOWN_ROOM];

	if (auth_is_cookie(settings.cookie))
		return;
	auth_cookie_make(settings.cookie, random_byte);
	settings_save();
	auth_cookie_show(settings.cookie, shown);
	console_say("KEY %s (NEW: ADD IT AT MY.SBITX.NET)", shown);
}

// the stretch for this callsign and cookie, done a few rounds a turn; 1 when
// it is there
static int is_stretched(long now) {
	char wanted[SETTINGS_STRETCHED_FOR_ROOM];

	have_a_cookie();
	snprintf(wanted, sizeof(wanted), "%s %s", settings.callsign, settings.cookie);
	if (!strcmp(wanted, settings.stretched_for) && strlen(settings.stretched) == 2 * SHA256_BYTES)
		return 1;
	if (state != STATE_STRETCHING) {
		auth_stretch_begin(&stretch, settings.cookie, settings.callsign);
		stretch_began_ms = now;
		state = STATE_STRETCHING;
		say_status("STRETCHING THE KEY");
	}
	if (!auth_stretch_step(&stretch, STRETCH_ROUNDS_A_TURN))
		return 0;
	auth_stretch_hex(&stretch, settings.stretched);
	snprintf(settings.stretched_for, sizeof(settings.stretched_for), "%s", wanted);
	settings_save();
	console_say("SERVICE KEY STRETCHED IN %ld MS", now - stretch_began_ms);
	state = STATE_WAITING;
	retry_ms = now;
	return 1;
}

void service_open(void) {
	xTaskCreate(connector_task, "connector", CONNECTOR_STACK, NULL, CONNECTOR_PRIORITY, &connector);
	have_a_cookie();
	say_status("OFF");
}

// a new cookie, for NEW_KEY and the screen's NEW CODE: the old one is
// forgotten, a new one made at the next turn, and the line made again
static void have_a_cookie(void);

// a new code, made at once, the old forgotten
void service_new_key(void) {
	settings.cookie[0] = 0;
	settings.stretched_for[0] = 0;
	have_a_cookie();
	service_restart();
}

// the code to add at my.sbitx.net, made first if there is none yet
void service_code(char shown[AUTH_COOKIE_SHOWN_ROOM]) {
	have_a_cookie();
	auth_cookie_show(settings.cookie, shown);
}

// after a change to what the line is made with: it is made again
void service_restart(void) {
	is_restart_asked = 1;
}

void service_poll(long now) {
	if (state == STATE_CONNECTING && !is_connect_done)
		watch_connector(now);
	if (state == STATE_CONNECTING && !is_connect_done)
		return;
	if (state == STATE_CONNECTING)
		connected(now);
	if (is_restart_asked) {
		is_restart_asked = 0;
		close_line();
		state = STATE_OFF;
		retry_after_ms = RETRY_FIRST_MS;
	}
	if (!settings.is_voip_online || settings.callsign[0] == 0 || !network_is_up()) {
		if (state != STATE_OFF) {
			close_line();
			state = STATE_OFF;
		}
		if (!settings.is_voip_online)
			say_status("OFF");
		else if (settings.callsign[0] == 0)
			say_status("SET CALL FIRST");
		else
			say_status("WAITING FOR WIFI");
		return;
	}
	if (state == STATE_OFF) {
		state = STATE_WAITING;
		retry_ms = now;
	}
	if (state != STATE_ONLINE && !is_stretched(now))
		return;
	if (state == STATE_WAITING && now >= retry_ms)
		go_online(now);
	if (state == STATE_ONLINE)
		line_poll(now);
}

// a few words for the home page, in place of the status, which can be
// long: the code to add is on the MY.SBITX.NET page, not here
void service_brief(char *text, int room) {
	const char *said = "Offline";

	if (state == STATE_ONLINE)
		said = "Online";
	else if (settings.is_local)
		said = settings.callsign[0] != 0 ? "Local" : "Local: no callsign";
	else if (!settings.is_voip_online)
		said = "VoIP off";
	else if (settings.callsign[0] == 0)
		said = "Set the callsign";
	else if (!network_is_up())
		said = "No WiFi";
	else if (is_refused)
		said = "Not registered";
	else if (state == STATE_CONNECTING)
		said = "Connecting";
	snprintf(text, room, "%s", said);
}

// 1 when the WiFi is up but my.sbitx.net refused the key: the talkie is
// not yet a radio of anyone's there
int service_is_unregistered(void) {
	return is_refused && network_is_up() && settings.callsign[0] != 0;
}

int service_is_online(void) {
	return state == STATE_ONLINE;
}

void service_describe(char *text, int room) {
	const char *step = connect_step;

	if (state == STATE_CONNECTING && step[0] != 0)
		snprintf(text, room, "%s: %s", status, step);
	else
		snprintf(text, room, "%s", status);
}
