// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ashhar Farhan
//
// voip_packet.c — the packet of my.sbitx.net's repeater, made and opened.
//
// The same 196 bytes go each way by UDP, as accounts/voip.py in zbitxre
// lays them out:
//
//      0   8   the stream's id, given on the TCP line by VOIP STREAM
//      8   1   kind: 0 where the talkie is tuned, 3 speech
//      9   1   codec: 0 mu-law, 8000 a second, 20 ms
//     10  10   callsign, noughts after it; empty going up, the service
//              writes in the speaker's from the account
//     20   4   the dial's frequency in Hz, the more significant byte first
//     24   4   the packet's number, likewise
//     28 160   sound
//    188   8   tag: the first eight bytes of HMAC-SHA256 of all before it
//
// The key of the tag is the stream's up key for what the talkie sends and
// its down key for what it is sent, each the 64 hex digits auth.c makes,
// used as they are written. The tag is compared without stopping at the
// first byte that differs, so that how long the comparing takes says
// nothing of where a forged tag goes wrong.
//
// A packet numbered at or behind the last taken, or too far ahead of it,
// is not taken: voip_is_ahead() says which, as the radio's daemon does.

#include <string.h>

#include "sha256.h"
#include "voip_packet.h"

#define KIND_AT 0
#define CODEC_AT 1
#define CALLSIGN_AT 2
#define HZ_AT (CALLSIGN_AT + VOIP_CALLSIGN_BYTES)
#define NUMBER_AT (HZ_AT + 4)
#define SOUND_AT (VOIP_STREAM_ID_BYTES + VOIP_HEAD_BYTES)
#define TAG_AT (SOUND_AT + VOIP_SOUND_BYTES)
#define HEX_DIGITS_A_BYTE 2
#define HEX_BASE 16

static void put_be32(uint8_t *at, uint32_t value) {
	at[0] = (uint8_t)(value >> 24);
	at[1] = (uint8_t)(value >> 16);
	at[2] = (uint8_t)(value >> 8);
	at[3] = (uint8_t)value;
}

static uint32_t be32(const uint8_t *at) {
	return (uint32_t)at[0] << 24 | (uint32_t)at[1] << 16 | (uint32_t)at[2] << 8 | at[3];
}

static int hex_value(char digit) {
	if (digit >= '0' && digit <= '9')
		return digit - '0';
	if (digit >= 'a' && digit <= 'f')
		return digit - 'a' + 10;
	if (digit >= 'A' && digit <= 'F')
		return digit - 'A' + 10;
	return -1;
}

// 0 with the id, -1 if the text is not 16 hex digits
int voip_id_of_hex(const char *hex, uint8_t id[VOIP_STREAM_ID_BYTES]) {
	int high;
	int low;
	int i;

	if (strlen(hex) != HEX_DIGITS_A_BYTE * VOIP_STREAM_ID_BYTES)
		return -1;
	for (i = 0; i < VOIP_STREAM_ID_BYTES; i++) {
		high = hex_value(hex[HEX_DIGITS_A_BYTE * i]);
		low = hex_value(hex[HEX_DIGITS_A_BYTE * i + 1]);
		if (high < 0 || low < 0)
			return -1;
		id[i] = (uint8_t)(high * HEX_BASE + low);
	}
	return 0;
}

long voip_channel_of(uint32_t hz) {
	return (long)((hz + VOIP_HZ_A_CHANNEL / 2) / VOIP_HZ_A_CHANNEL);
}

// sound is NULL for a packet of kind 0, whose sound is silence
void voip_packet_make(uint8_t packet[VOIP_PACKET_BYTES], const uint8_t id[VOIP_STREAM_ID_BYTES],
		const char *key, int kind, uint32_t hz, uint32_t number, const uint8_t *sound) {
	voip_packet_make_as(packet, id, key, "", kind, hz, number, sound);
}

// the same, with the callsign of who sends it written in, as the service
// does going down; for Local (local.c), where there is no service
void voip_packet_make_as(uint8_t packet[VOIP_PACKET_BYTES], const uint8_t id[VOIP_STREAM_ID_BYTES],
		const char *key, const char *callsign, int kind, uint32_t hz, uint32_t number,
		const uint8_t *sound) {
	uint8_t *head = packet + VOIP_STREAM_ID_BYTES;
	uint8_t hash[SHA256_BYTES];
	size_t length = strlen(callsign);

	memset(packet, 0, VOIP_PACKET_BYTES);
	memcpy(packet, id, VOIP_STREAM_ID_BYTES);
	memcpy(head + CALLSIGN_AT, callsign, length < VOIP_CALLSIGN_BYTES ? length : VOIP_CALLSIGN_BYTES);
	head[KIND_AT] = (uint8_t)kind;
	head[CODEC_AT] = VOIP_CODEC_MULAW;
	put_be32(head + HZ_AT, hz);
	put_be32(head + NUMBER_AT, number);
	if (sound != NULL)
		memcpy(packet + SOUND_AT, sound, VOIP_SOUND_BYTES);
	hmac_sha256((const uint8_t *)key, strlen(key), packet, TAG_AT, hash);
	memcpy(packet + TAG_AT, hash, VOIP_TAG_BYTES);
}

int voip_packet_open(const uint8_t *packet, size_t length, const uint8_t id[VOIP_STREAM_ID_BYTES],
		const char *key, struct voip_heard *heard) {
	const uint8_t *head = packet + VOIP_STREAM_ID_BYTES;
	uint8_t hash[SHA256_BYTES];
	uint8_t differing = 0;
	int i;

	if (length != VOIP_PACKET_BYTES || memcmp(packet, id, VOIP_STREAM_ID_BYTES) != 0)
		return VOIP_NOT_OURS;
	hmac_sha256((const uint8_t *)key, strlen(key), packet, TAG_AT, hash);
	for (i = 0; i < VOIP_TAG_BYTES; i++)
		differing |= (uint8_t)(hash[i] ^ packet[TAG_AT + i]);
	if (differing)
		return VOIP_WRONG_TAG;
	if (head[KIND_AT] != VOIP_KIND_SPEECH || head[CODEC_AT] != VOIP_CODEC_MULAW)
		return VOIP_NOT_SPEECH;
	memcpy(heard->callsign, head + CALLSIGN_AT, VOIP_CALLSIGN_BYTES);
	heard->callsign[VOIP_CALLSIGN_BYTES] = 0;
	heard->hz = be32(head + HZ_AT);
	heard->number = be32(head + NUMBER_AT);
	heard->sound = packet + SOUND_AT;
	return VOIP_OPENED;
}

int voip_is_ahead(int is_begun, uint32_t last, uint32_t number) {
	if (!is_begun)
		return 1;
	return (uint32_t)(number - last - 1) < VOIP_AHEAD_MOST;
}
