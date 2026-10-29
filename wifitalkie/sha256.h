// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ashhar Farhan
//
// sha256.h — SHA-256 and the keyed hash made of it, HMAC. See sha256.c.

#pragma once

#include <stddef.h>
#include <stdint.h>

#define SHA256_BYTES 32
#define SHA256_HEX_ROOM (2 * SHA256_BYTES + 1)

// In the talkie, ESP-IDF's WiFi has a hmac_sha256() of its own, linked into
// the same program; this one is given another name, and the file it is
// taken from, zbitxre's clients/remote/sha256.c, is otherwise as it was.
#define hmac_sha256 sha256_hmac

void sha256(const uint8_t *data, size_t length, uint8_t hash[SHA256_BYTES]);
void hmac_sha256(const uint8_t *key, size_t key_length, const uint8_t *data,
	size_t length, uint8_t hash[SHA256_BYTES]);
void hmac_sha256_hex(const char *key, const char *text, char hex[SHA256_HEX_ROOM]);
void pbkdf2_sha256_hex(const char *secret, const char *salt, int rounds,
	char hex[SHA256_HEX_ROOM]);
