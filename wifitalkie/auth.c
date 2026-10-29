// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ashhar Farhan
//
// auth.c — the talkie's cookie, and the answers to my.sbitx.net made with it.
//
// To my.sbitx.net the talkie is a radio, and it proves who it is as a
// radio's daemon does, clients/remote/zbitx_remote.c in zbitxre; PROTOCOL.md
// there, under "Authentication", has it exactly.
//
// THE COOKIE
//
// Twelve letters from COOKIE_LETTERS, which leave out the ones that are
// read for others (i, l, o), each as likely as another: a random byte too
// large to divide evenly among the letters is thrown away and another
// taken. The owner types it at my.sbitx.net, shown in fours, abcd-jklm-wxyz,
// to add the talkie as one of their radios. It is never sent.
//
// THE STRETCH
//
// The key of every answer is PBKDF2 with HMAC-SHA256 of the cookie, 50,000
// rounds, salted with "zbitx-1 " and the callsign in small letters, used as
// the 64 hex digits it is written in. On the ESP32-C3 the rounds take some
// seconds, so they are done a few at a time from the loop, and the result
// is kept in the settings until the callsign or the cookie changes. It is
// sha256.c's pbkdf2_sha256_hex() taken apart; tests/test_units.c checks the
// two agree.
//
// THE ANSWERS
//
// The proof of the handshake is the keyed hash, with the stretch, of
// "zbitx-auth-1 radio <callsign> <challenge> <nonce>", and the service's
// own the same with "server". The keys of a VoIP stream are the keyed hash
// of "zbitx-voip-up-1 <nonce>" and "zbitx-voip-down-1 <nonce>", in hex.

#include <stdio.h>
#include <string.h>

#include "auth.h"

#define COOKIE_LETTERS "abcdefghjkmnpqrstuvwxyz"
#define SALT_BEGINS "zbitx-1 "
#define BLOCK_NUMBER_BYTES 4
#define BYTE_VALUES 256
#define TEXT_ROOM 256

int auth_is_cookie(const char *text) {
	int i;

	if (strlen(text) != AUTH_COOKIE_LENGTH)
		return 0;
	for (i = 0; i < AUTH_COOKIE_LENGTH; i++) {
		if (strchr(COOKIE_LETTERS, text[i]) == NULL)
			return 0;
	}
	return 1;
}

void auth_cookie_make(char cookie[AUTH_COOKIE_ROOM], uint8_t (*random_byte)(void)) {
	int letters = (int)strlen(COOKIE_LETTERS);
	int evenly = BYTE_VALUES - BYTE_VALUES % letters;
	int made = 0;
	uint8_t byte;

	while (made < AUTH_COOKIE_LENGTH) {
		byte = random_byte();
		if (byte < evenly)
			cookie[made++] = COOKIE_LETTERS[byte % letters];
	}
	cookie[AUTH_COOKIE_LENGTH] = 0;
}

void auth_cookie_show(const char *cookie, char shown[AUTH_COOKIE_SHOWN_ROOM]) {
	int used = 0;
	int i;

	for (i = 0; i < AUTH_COOKIE_LENGTH && cookie[i] != 0; i++) {
		if (i > 0 && i % AUTH_COOKIE_GROUP == 0)
			shown[used++] = '-';
		shown[used++] = cookie[i];
	}
	shown[used] = 0;
}

// the first round: the keyed hash of the salt with the block number 1
void auth_stretch_begin(struct auth_stretch *s, const char *cookie, const char *callsign) {
	uint8_t salted[AUTH_SALT_ROOM + BLOCK_NUMBER_BYTES];
	size_t length;
	size_t i;

	memset(s, 0, sizeof(*s));
	snprintf(s->secret, sizeof(s->secret), "%s", cookie);
	snprintf((char *)salted, AUTH_SALT_ROOM, "%s%s", SALT_BEGINS, callsign);
	length = strlen((char *)salted);
	for (i = 0; i < length; i++) {
		if (salted[i] >= 'A' && salted[i] <= 'Z')
			salted[i] = (uint8_t)(salted[i] - 'A' + 'a');
	}
	memset(salted + length, 0, BLOCK_NUMBER_BYTES);
	salted[length + BLOCK_NUMBER_BYTES - 1] = 1;
	hmac_sha256((const uint8_t *)s->secret, strlen(s->secret), salted,
		length + BLOCK_NUMBER_BYTES, s->round);
	memcpy(s->all, s->round, sizeof(s->all));
	s->rounds_done = 1;
}

// up to rounds more rounds; 1 once all are done
int auth_stretch_step(struct auth_stretch *s, int rounds) {
	int i;

	while (rounds > 0 && s->rounds_done < AUTH_STRETCH_ROUNDS) {
		hmac_sha256((const uint8_t *)s->secret, strlen(s->secret), s->round,
			sizeof(s->round), s->round);
		for (i = 0; i < SHA256_BYTES; i++)
			s->all[i] ^= s->round[i];
		s->rounds_done++;
		rounds--;
	}
	return s->rounds_done >= AUTH_STRETCH_ROUNDS;
}

void auth_stretch_hex(const struct auth_stretch *s, char hex[SHA256_HEX_ROOM]) {
	int i;

	for (i = 0; i < SHA256_BYTES; i++)
		snprintf(hex + 2 * i, 3, "%02x", s->all[i]);
}

void auth_proof(const char *stretched, const char *side, const char *callsign,
		const char *challenge, const char *nonce, char hex[SHA256_HEX_ROOM]) {
	char text[TEXT_ROOM];

	snprintf(text, sizeof(text), "zbitx-auth-1 %s %s %s %s", side, callsign, challenge, nonce);
	hmac_sha256_hex(stretched, text, hex);
}

void auth_stream_key(const char *stretched, const char *words, const char *nonce,
		char hex[SHA256_HEX_ROOM]) {
	char text[TEXT_ROOM];

	snprintf(text, sizeof(text), "%s %.64s", words, nonce);
	hmac_sha256_hex(stretched, text, hex);
}
