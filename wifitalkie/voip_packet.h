// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ashhar Farhan
//
// voip_packet.h — the packet of my.sbitx.net's repeater. See voip_packet.c.

#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define VOIP_STREAM_ID_BYTES 8
#define VOIP_CALLSIGN_BYTES 10
#define VOIP_CALLSIGN_ROOM (VOIP_CALLSIGN_BYTES + 1)
#define VOIP_HEAD_BYTES (1 + 1 + VOIP_CALLSIGN_BYTES + 4 + 4)
#define VOIP_SOUND_BYTES 160
#define VOIP_TAG_BYTES 8
#define VOIP_PACKET_BYTES (VOIP_STREAM_ID_BYTES + VOIP_HEAD_BYTES + VOIP_SOUND_BYTES + VOIP_TAG_BYTES)
#define VOIP_KIND_TUNED 0
#define VOIP_KIND_SPEECH 3
#define VOIP_CODEC_MULAW 0
#define VOIP_AHEAD_MOST 3000
#define VOIP_HZ_A_CHANNEL 1000

#define VOIP_OPENED 0
#define VOIP_NOT_OURS 1
#define VOIP_WRONG_TAG 2
#define VOIP_NOT_SPEECH 3

// what a packet of speech sent to the talkie says
struct voip_heard {
	char callsign[VOIP_CALLSIGN_ROOM];
	uint32_t hz;
	uint32_t number;
	const uint8_t *sound;
};

int voip_id_of_hex(const char *hex, uint8_t id[VOIP_STREAM_ID_BYTES]);
long voip_channel_of(uint32_t hz);
void voip_packet_make(uint8_t packet[VOIP_PACKET_BYTES], const uint8_t id[VOIP_STREAM_ID_BYTES],
	const char *key, int kind, uint32_t hz, uint32_t number, const uint8_t *sound);
void voip_packet_make_as(uint8_t packet[VOIP_PACKET_BYTES], const uint8_t id[VOIP_STREAM_ID_BYTES],
	const char *key, const char *callsign, int kind, uint32_t hz, uint32_t number,
	const uint8_t *sound);
int voip_packet_open(const uint8_t *packet, size_t length, const uint8_t id[VOIP_STREAM_ID_BYTES],
	const char *key, struct voip_heard *heard);
int voip_is_ahead(int is_begun, uint32_t last, uint32_t number);

#ifdef __cplusplus
}
#endif
