// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ashhar Farhan
//
// auth.h — the talkie's cookie and the answers made with it. See auth.c.

#pragma once

#include <stdint.h>

#include "sha256.h"

#ifdef __cplusplus
extern "C" {
#endif

#define AUTH_COOKIE_LENGTH 12
#define AUTH_COOKIE_ROOM (AUTH_COOKIE_LENGTH + 1)
#define AUTH_COOKIE_GROUP 4
#define AUTH_COOKIE_SHOWN_ROOM (AUTH_COOKIE_LENGTH + AUTH_COOKIE_LENGTH / AUTH_COOKIE_GROUP)
#define AUTH_STRETCH_ROUNDS 50000
#define AUTH_SALT_ROOM 64

// PBKDF2 done a few rounds at a time, so that the loop goes on meanwhile
struct auth_stretch {
	char secret[AUTH_COOKIE_ROOM];
	uint8_t round[SHA256_BYTES];
	uint8_t all[SHA256_BYTES];
	int rounds_done;
};

int auth_is_cookie(const char *text);
void auth_cookie_make(char cookie[AUTH_COOKIE_ROOM], uint8_t (*random_byte)(void));
void auth_cookie_show(const char *cookie, char shown[AUTH_COOKIE_SHOWN_ROOM]);
void auth_stretch_begin(struct auth_stretch *s, const char *cookie, const char *callsign);
int auth_stretch_step(struct auth_stretch *s, int rounds);
void auth_stretch_hex(const struct auth_stretch *s, char hex[SHA256_HEX_ROOM]);
void auth_proof(const char *stretched, const char *side, const char *callsign,
	const char *challenge, const char *nonce, char hex[SHA256_HEX_ROOM]);
void auth_stream_key(const char *stretched, const char *words, const char *nonce,
	char hex[SHA256_HEX_ROOM]);

#ifdef __cplusplus
}
#endif
