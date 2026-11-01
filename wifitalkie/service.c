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
// waited for, STEP_WAIT_MS at most, during which nothing else is done: there
// is nothing to hear or say before the line is up.
//
// ERROR AUTH means the website does not know the talkie yet, or no longer:
// SERVICE says the key to add there, and the service is asked again every
// REFUSED_RETRY_MS. Any other failure is tried again after RETRY_FIRST_MS,
// and longer each time, to RETRY_MOST_MS.
//
// ONCE IT IS UP
//
// The talkie says its grid, GRID with nothing after it if none is set, and
// ALLOWS voip, as a radio with VOIP online does, and then nothing but PING
// a minute; a line not heard from for SILENCE_MOST_MS is taken to be lost.
// The service answers ALLOWS with VOIP STREAM <id> <nonce>, which voip.c
// takes up; VOIP ROOM and VOIP BUSY go to voip.c too, for what it shows.
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
#include "lwip/netdb.h"
#include "lwip/sockets.h"

#include "auth.h"
#include "console.h"
#include "network.h"
#include "service.h"
#include "settings.h"
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
#define REFUSED_RETRY_MS 15000
#define RETRY_FIRST_MS 5000
#define RETRY_MORE_MS 5000
#define RETRY_MOST_MS 15000
#define PING_EVERY_MS 60000
#define SILENCE_MOST_MS 150000
#define STRETCH_ROUNDS_A_TURN 100
#define MS_PER_SECOND 1000
#define US_PER_MS 1000

#define AUTH_OK 0
#define AUTH_REFUSED 1
#define AUTH_NO_ANSWER 2

enum state {
	STATE_OFF,
	STATE_STRETCHING,
	STATE_WAITING,
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
static int is_allows_told;
static char name[NAME_ROOM];
static char failed_because[TEXT_ROOM];
static char status[STATUS_ROOM];
static int is_restart_asked;

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
	gathered_used = 0;
	voip_close();
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

static int resolve(void) {
	struct addrinfo hints;
	struct addrinfo *found = NULL;
	char port[PORT_ROOM];

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	snprintf(port, sizeof(port), "%u", (unsigned)settings.port);
	if (getaddrinfo(settings.host, port, &hints, &found) != 0 || found == NULL)
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
	fcntl(fd, F_SETFL, O_NONBLOCK);
	if (connect(fd, (struct sockaddr *)&service_address, sizeof(service_address)) < 0
			&& errno != EINPROGRESS) {
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
		close(fd);
		no_answer("no connection");
		return NOT_OPEN;
	}
	fcntl(fd, F_SETFL, 0);
	setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &wait, sizeof(wait));
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
	auth_proof(settings.stretched, "radio", settings.callsign, challenge, nonce, proof);
	snprintf(line, sizeof(line), "AUTH radio %s %s %s\n", settings.callsign, nonce, proof);
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
	auth_proof(settings.stretched, "server", settings.callsign, challenge, nonce, proof);
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

static void go_online(long now) {
	char said[STATUS_ROOM];
	char shown[AUTH_COOKIE_SHOWN_ROOM];
	char line[LINE_ROOM];
	int result;

	say_status("CONNECTING");
	line_fd = dial();
	result = AUTH_NO_ANSWER;
	if (line_fd != NOT_OPEN)
		result = authenticate();
	if (result == AUTH_REFUSED) {
		console_say("SERVICE SAYS %s", failed_because);
		wait_to_retry(now, REFUSED_RETRY_MS);
		auth_cookie_show(settings.cookie, shown);
		snprintf(said, sizeof(said), "ADD KEY %s AT MY.SBITX.NET", shown);
		say_status(said);
		return;
	}
	if (result == AUTH_OK && (send_text("radio\n") < 0
			|| read_line_waiting(line, sizeof(line)) < 0 || strcmp(line, "OK"))) {
		no_answer("no OK to radio");
		result = AUTH_NO_ANSWER;
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
	snprintf(said, sizeof(said), "ONLINE %s", name);
	say_status(said);
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
	if (sscanf(line, "VOIP ROOM %ld %d", &channel, &count) == 2) {
		voip_room_heard(channel, count);
		return;
	}
	if (!strncmp(line, "VOIP BUSY ", strlen("VOIP BUSY "))) {
		voip_busy_heard(line + strlen("VOIP BUSY "), now);
		return;
	}
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
		if (send_text("ALLOWS voip\n") < 0) {
			lost(now, "ALLOWS could not be sent");
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
	have_a_cookie();
	say_status("OFF");
}

// a new cookie, for NEW_KEY and the screen's NEW CODE: the old one is
// forgotten, a new one made at the next turn, and the line made again
void service_new_key(void) {
	settings.cookie[0] = 0;
	settings.stretched_for[0] = 0;
	settings_save();
	service_restart();
}

// after a change to what the line is made with: it is made again
void service_restart(void) {
	is_restart_asked = 1;
}

void service_poll(long now) {
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

int service_is_online(void) {
	return state == STATE_ONLINE;
}

void service_describe(char *text, int room) {
	snprintf(text, room, "%s", status);
}
