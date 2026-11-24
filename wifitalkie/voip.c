// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ashhar Farhan
//
// voip.c — the talkie's VoIP stream: what it says by UDP, and what it hears.
//
// The author's design for my.sbitx.net's repeater: the dial is the room. The
// talkie's dial is FREQ, which it has no radio to tune; the channel is FREQ
// rounded to the nearest kilohertz, and whoever speaks there is heard here.
// The first to speak holds the channel, a minute at most. accounts/voip.py
// in zbitxre says what the service does, and voip_packet.c lays out the
// packet.
//
// The service gives the stream on the TCP line, VOIP STREAM <id> <nonce>;
// service.c hands it here, and the two keys are made of the nonce, the up
// key for what the talkie sends and the down key for what it is sent. The
// packets go to the service's address and port, the same as the line's.
//
// WHAT IS SENT
//
// A packet of kind 0, where the talkie is tuned, every TUNED_EVERY_MS and
// within TUNED_SOON_MS of FREQ moving to another kilohertz: it puts the
// talkie in its channel and keeps the way back through the home's router
// open. With the PTT down, each twenty milliseconds of the microphone's,
// as microphone.c makes it, goes as a packet of speech; with TONE on, a
// steady 1 kHz goes in its place, to try the way up without a microphone.
// Every packet is numbered, one after another, both kinds together.
//
// With a contact the target, speech goes to its callsign, not the dial:
// the frequency of each packet of speech is voip_call_hz() of the
// contact's callsign, and the repeater sends it to that account's radios
// alone. The packets of kind 0 still say the dial, so the talkie stays in
// its channel and hears it as before. A contact whose callsign cannot be
// called that way, past six letters, is not sent to.
//
// VOX: while the PTT is up, the last PREROLL_FRAMES of the microphone's are
// kept, and when the voice keys the talkie (ptt_is_vox()) they go first,
// so that the word that keyed it is not cut: the voice is some tens of
// milliseconds old by the time vox.c is sure of it. The voice is held off
// keying the talkie while a voice is heard and VOX_QUIET_MS after, so that
// the speaker does not key it through the microphone; and so while the
// Morse sidetone sounds and VOX_QUIET_MS after, or it keys the talkie,
// whose PTT mutes the speaker and cuts the tone short.
//
// WHAT IS HEARD
//
// A packet is taken if it is the stream's, its tag is right by the down key,
// it is speech, its number is ahead of the last and not too far, and its
// frequency is in the talkie's channel, or is a call (voip_is_call()),
// which the repeater sends only to the account it calls; the speech then goes to the
// speaker's queue. While the PTT is down nothing is taken: the talkie is
// one way at a time, as a walkie-talkie is. The speaker's callsign, which
// the service wrote in from the speaker's account, is said on the console
// when they begin, HEARD VU2ESE, and when they have been quiet for
// OVER_GAP_MS, OVER VU2ESE.
//
// LOCAL
//
// With the link set to Local the same packets go to local.c instead, as
// ESP-NOW broadcasts to every talkie in hearing, and come from it: the id
// LOCAL_ID, the tag made with LOCAL_KEY, which every talkie knows, and the
// sender's callsign written in by the talkie itself. No packets of kind 0
// are sent, there being no repeater to tell. As at the repeater, the first
// to speak holds the channel: while one is heard, another's speech is
// dropped until OVER_GAP_MS after the first stops. The numbers are each
// sender's own, so they are followed from the start of each over.

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>

#include "lwip/sockets.h"

#include "auth.h"
#include "console.h"
#include "local.h"
#include "microphone.h"
#include "ptt.h"
#include "settings.h"
#include "speaker.h"
#include "telephone.h"
#include "voip.h"
#include "voip_packet.h"

#define NOT_OPEN (-1)
#define TUNED_EVERY_MS 15000
#define TUNED_SOON_MS 1000
#define BUSY_SHOWN_MS 3000
#define OVER_GAP_MS 1000
#define PREROLL_FRAMES 8
#define VOX_QUIET_MS 600
#define TEXT_ROOM 48
#define TONE_STEPS 8

// 1 kHz at 8000 a second, about 10 dB below full scale
static const int16_t tone_steps[TONE_STEPS] = {
	0, 7071, 10000, 7071, 0, -7071, -10000, -7071
};

#define LOCAL_KEY "wifitalkie-local-1"

static const uint8_t LOCAL_ID[VOIP_STREAM_ID_BYTES] = {'W', 'T', 'L', 'O', 'C', 'A', 'L', '1'};

static int fd = NOT_OPEN;
static uint8_t id[VOIP_STREAM_ID_BYTES];
static char up_key[SHA256_HEX_ROOM];
static char down_key[SHA256_HEX_ROOM];
static uint32_t number_sent;
static uint32_t number_taken;
static int is_taken_begun;
static long tuned_ms;
static long channel_told = -1;
static int is_tuned_begun;
static char room[TEXT_ROOM];
static int here = -1;
static char busy[TEXT_ROOM];
static long busy_ms;
static long polled_ms;
static int is_tone;
static int tone_at;
static int was_talking;
static int16_t preroll[PREROLL_FRAMES][TELEPHONE_FRAME_SAMPLES];
static int preroll_at;
static int preroll_count;
static int is_hearing;
static long heard_ms;
static long toned_ms;
static char speaker_callsign[VOIP_CALLSIGN_ROOM];
static uint32_t count_sent;
static uint32_t count_heard;
static uint32_t count_refused;
static uint32_t count_dropped;

void voip_open(const struct sockaddr_in *service, const char *id_hex, const char *nonce,
		const char *stretched) {
	voip_close();
	if (voip_id_of_hex(id_hex, id) < 0) {
		console_say("VOIP STREAM NOT UNDERSTOOD");
		return;
	}
	auth_stream_key(stretched, "zbitx-voip-up-1", nonce, up_key);
	auth_stream_key(stretched, "zbitx-voip-down-1", nonce, down_key);
	fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (fd < 0 || connect(fd, (const struct sockaddr *)service, sizeof(*service)) < 0) {
		console_say("VOIP NO SOCKET");
		voip_close();
		return;
	}
	fcntl(fd, F_SETFL, O_NONBLOCK);
	number_sent = 0;
	is_taken_begun = 0;
	is_tuned_begun = 0;
	channel_told = -1;
	console_say("VOIP STREAM OPEN");
}

void voip_close(void) {
	if (fd != NOT_OPEN)
		close(fd);
	fd = NOT_OPEN;
	room[0] = 0;
	here = -1;
	busy[0] = 0;
}

void voip_room_heard(long channel, int count) {
	char said[TEXT_ROOM];

	here = count;
	snprintf(said, sizeof(said), "%ld KHZ, %d HERE", channel, count);
	if (!strcmp(said, room))
		return;
	snprintf(room, sizeof(room), "%s", said);
	console_say("ROOM %s", room);
}

void voip_busy_heard(const char *why, long now) {
	int i;

	snprintf(busy, sizeof(busy), "%.40s", why);
	for (i = 0; busy[i] != 0; i++) {
		if (busy[i] >= 'a' && busy[i] <= 'z')
			busy[i] = (char)(busy[i] - 'a' + 'A');
	}
	busy_ms = now;
	console_say("BUSY %s", busy);
}

void voip_tone(int is_on) {
	is_tone = is_on;
}

// whether packets can go: the stream with my.sbitx.net, or Local
static int is_linked(void) {
	return settings.is_local ? local_is_open() : fd != NOT_OPEN;
}

// where speech goes: the dial, or with a contact the target, its callsign;
// 0 nowhere
static uint32_t speech_hz(void) {
	if (settings.is_local || settings.target < SETTINGS_TARGET_CONTACT)
		return settings.hz;
	return voip_call_hz(settings.contacts[settings.target - SETTINGS_TARGET_CONTACT]);
}

static void send_packet(int kind, const uint8_t *sound) {
	uint8_t packet[VOIP_PACKET_BYTES];
	uint32_t hz = kind == VOIP_KIND_SPEECH ? speech_hz() : settings.hz;

	number_sent++;
	if (settings.is_local) {
		voip_packet_make_as(packet, LOCAL_ID, LOCAL_KEY, settings.callsign, kind, settings.hz,
			number_sent, sound);
		if (local_send(packet))
			count_sent++;
		return;
	}
	voip_packet_make(packet, id, up_key, kind, hz, number_sent, sound);
	if (send(fd, packet, sizeof(packet), 0) == (ssize_t)sizeof(packet))
		count_sent++;
}

// one frame of the microphone's sent as speech, or the tone in its place
static void speak_frame(int16_t samples[TELEPHONE_FRAME_SAMPLES]) {
	uint8_t sound[TELEPHONE_FRAME_SAMPLES];
	int i;

	for (i = 0; i < TELEPHONE_FRAME_SAMPLES; i++) {
		if (is_tone) {
			samples[i] = tone_steps[tone_at];
			tone_at = (tone_at + 1) % TONE_STEPS;
		}
		sound[i] = telephone_ulaw_of(samples[i]);
	}
	send_packet(VOIP_KIND_SPEECH, sound);
}

static void speak(int is_talking) {
	int16_t samples[TELEPHONE_FRAME_SAMPLES];
	int i;

	while (microphone_frame(samples)) {
		if (!is_talking) {
			memcpy(preroll[preroll_at], samples, sizeof(samples));
			preroll_at = (preroll_at + 1) % PREROLL_FRAMES;
			if (preroll_count < PREROLL_FRAMES)
				preroll_count++;
			continue;
		}
		if (!is_linked() || speech_hz() == 0)
			continue;
		if (preroll_count > 0 && ptt_is_vox()) {
			for (i = preroll_count; i > 0; i--)
				speak_frame(preroll[(preroll_at - i + PREROLL_FRAMES) % PREROLL_FRAMES]);
		}
		preroll_count = 0;
		speak_frame(samples);
	}
	if (is_talking)
		preroll_count = 0;
}

static void tell_where_tuned(long now) {
	long channel = voip_channel_of(settings.hz);

	if (fd == NOT_OPEN || settings.hz == 0 || settings.is_local)
		return;
	if (is_tuned_begun && now - tuned_ms < TUNED_EVERY_MS
			&& (channel == channel_told || now - tuned_ms < TUNED_SOON_MS))
		return;
	send_packet(VOIP_KIND_TUNED, NULL);
	is_tuned_begun = 1;
	tuned_ms = now;
	channel_told = channel;
}

static void take(const uint8_t *packet, int length, int is_talking, long now) {
	struct voip_heard heard;

	if (voip_packet_open(packet, (size_t)length, id, down_key, &heard) != VOIP_OPENED) {
		count_refused++;
		return;
	}
	if (!voip_is_ahead(is_taken_begun, number_taken, heard.number)) {
		count_dropped++;
		return;
	}
	is_taken_begun = 1;
	number_taken = heard.number;
	if ((!voip_is_call(heard.hz) && voip_channel_of(heard.hz) != voip_channel_of(settings.hz))
			|| is_talking) {
		count_dropped++;
		return;
	}
	count_heard++;
	speaker_take(heard.sound, now);
	if (!is_hearing || strcmp(heard.callsign, speaker_callsign)) {
		snprintf(speaker_callsign, sizeof(speaker_callsign), "%s", heard.callsign);
		console_say("HEARD %s", speaker_callsign);
	}
	is_hearing = 1;
	heard_ms = now;
}

// a packet from another talkie in Local: taken as one from the repeater,
// but the first heard holds the channel, and each sender's numbers are
// followed from the start of its over
static void take_local(const uint8_t *packet, int is_talking, long now) {
	struct voip_heard heard;

	if (voip_packet_open(packet, VOIP_PACKET_BYTES, LOCAL_ID, LOCAL_KEY, &heard) != VOIP_OPENED) {
		count_refused++;
		return;
	}
	if (voip_channel_of(heard.hz) != voip_channel_of(settings.hz) || is_talking) {
		count_dropped++;
		return;
	}
	if (is_hearing && strcmp(heard.callsign, speaker_callsign)) {
		count_dropped++;
		return;
	}
	if (!is_hearing)
		is_taken_begun = 0;
	if (!voip_is_ahead(is_taken_begun, number_taken, heard.number)) {
		count_dropped++;
		return;
	}
	is_taken_begun = 1;
	number_taken = heard.number;
	count_heard++;
	speaker_take(heard.sound, now);
	if (!is_hearing) {
		snprintf(speaker_callsign, sizeof(speaker_callsign), "%s", heard.callsign);
		console_say("HEARD %s", speaker_callsign);
	}
	is_hearing = 1;
	heard_ms = now;
}

void voip_poll(long now) {
	uint8_t packet[VOIP_PACKET_BYTES + 1];
	int is_talking = ptt_is_down();
	int got;

	polled_ms = now;
	if (speaker_is_toning())
		toned_ms = now;
	microphone_vox_hold(is_hearing || now - heard_ms < VOX_QUIET_MS || now - toned_ms < VOX_QUIET_MS);
	if (is_talking != was_talking) {
		was_talking = is_talking;
		speaker_mute(is_talking);
		if (is_talking && !is_linked())
			console_say("PTT: NOT ONLINE, NOTHING IS SENT");
		if (is_talking && settings.is_local && settings.callsign[0] == 0)
			console_say("PTT: LOCAL, BUT NO CALLSIGN SET");
		if (is_talking && settings.hz == 0)
			console_say("PTT: SET FREQ FIRST");
	}
	speak(is_talking);
	tell_where_tuned(now);
	while (fd != NOT_OPEN && (got = recv(fd, packet, sizeof(packet), 0)) > 0)
		take(packet, got, is_talking, now);
	while (settings.is_local && local_take(packet))
		take_local(packet, is_talking, now);
	if (is_hearing && now - heard_ms > OVER_GAP_MS) {
		is_hearing = 0;
		console_say("OVER %s", speaker_callsign);
	}
	ptt_lamp(is_talking || is_hearing);
}

void voip_describe(char *text, int room_size) {
	if (settings.is_local) {
		if (local_is_open())
			snprintf(text, room_size, "LOCAL ON CHANNEL %d", LOCAL_CHANNEL);
		else
			snprintf(text, room_size, "LOCAL, NOT STARTED");
		return;
	}
	if (fd == NOT_OPEN) {
		snprintf(text, room_size, "NO STREAM");
		return;
	}
	if (busy[0] != 0 && polled_ms - busy_ms < BUSY_SHOWN_MS) {
		snprintf(text, room_size, "BUSY %s", busy);
		return;
	}
	if (room[0] != 0) {
		snprintf(text, room_size, "%s", room);
		return;
	}
	snprintf(text, room_size, "ONLINE");
}

// how many are in the room, as the service last said, or -1 not knowing
int voip_here(void) {
	return here;
}

// whether my.sbitx.net said the room was busy, a moment ago, and why
int voip_busy(char *why, int room_size) {
	snprintf(why, room_size, "%s", busy);
	return busy[0] != 0 && polled_ms - busy_ms < BUSY_SHOWN_MS;
}

// whether an over is being heard, and whose
int voip_hearing(char *callsign, int room_size) {
	snprintf(callsign, room_size, "%s", speaker_callsign);
	return is_hearing;
}

void voip_counts(char *text, int room_size) {
	snprintf(text, room_size, "SENT %u HEARD %u REFUSED %u DROPPED %u",
		(unsigned)count_sent, (unsigned)count_heard, (unsigned)count_refused,
		(unsigned)count_dropped);
}
