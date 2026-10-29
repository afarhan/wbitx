// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ashhar Farhan
//
// voip_tool.c — the talkie's answers and packets, made on this computer, for
// tests/test_service.py to hold against the service's own code.
//
//     voip_tool stretch <cookie> <callsign>
//     voip_tool proof <stretched> <radio|server> <callsign> <challenge> <nonce>
//     voip_tool key <stretched> <words> <nonce>
//     voip_tool make <id> <key> <kind> <hz> <number> [<sound in hex>]
//     voip_tool open <id> <key> <packet in hex>
//
// Each prints its result in hex on one line; open prints
// "OPENED <callsign> <hz> <number> <sound>", or why the packet was not
// opened.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../wifitalkie/auth.h"
#include "../wifitalkie/voip_packet.h"

#define STEP_ROUNDS 1000
#define HEX_BASE 16

static int bytes_of_hex(const char *hex, uint8_t *bytes, size_t room) {
	char pair[3];
	size_t count = strlen(hex) / 2;
	size_t i;

	if (count > room)
		return -1;
	for (i = 0; i < count; i++) {
		pair[0] = hex[2 * i];
		pair[1] = hex[2 * i + 1];
		pair[2] = 0;
		bytes[i] = (uint8_t)strtoul(pair, NULL, HEX_BASE);
	}
	return (int)count;
}

static void print_hex(const uint8_t *bytes, size_t count) {
	size_t i;

	for (i = 0; i < count; i++)
		printf("%02x", bytes[i]);
}

static int stretch(char **argv) {
	struct auth_stretch s;
	char hex[SHA256_HEX_ROOM];

	auth_stretch_begin(&s, argv[2], argv[3]);
	while (!auth_stretch_step(&s, STEP_ROUNDS))
		continue;
	auth_stretch_hex(&s, hex);
	printf("%s\n", hex);
	return 0;
}

static int make(int argc, char **argv) {
	uint8_t id[VOIP_STREAM_ID_BYTES];
	uint8_t sound[VOIP_SOUND_BYTES];
	uint8_t packet[VOIP_PACKET_BYTES];
	const uint8_t *given = NULL;

	if (voip_id_of_hex(argv[2], id) < 0)
		return 1;
	if (argc > 7) {
		if (bytes_of_hex(argv[7], sound, sizeof(sound)) != VOIP_SOUND_BYTES)
			return 1;
		given = sound;
	}
	voip_packet_make(packet, id, argv[3], atoi(argv[4]), (uint32_t)strtoul(argv[5], NULL, 10),
		(uint32_t)strtoul(argv[6], NULL, 10), given);
	print_hex(packet, sizeof(packet));
	printf("\n");
	return 0;
}

static int open_packet(char **argv) {
	static const char *why[] = { "OPENED", "NOT_OURS", "WRONG_TAG", "NOT_SPEECH" };
	uint8_t id[VOIP_STREAM_ID_BYTES];
	uint8_t packet[2 * VOIP_PACKET_BYTES];
	struct voip_heard heard;
	int length;
	int result;

	if (voip_id_of_hex(argv[2], id) < 0)
		return 1;
	length = bytes_of_hex(argv[4], packet, sizeof(packet));
	if (length < 0)
		return 1;
	result = voip_packet_open(packet, (size_t)length, id, argv[3], &heard);
	if (result != VOIP_OPENED) {
		printf("%s\n", why[result]);
		return 0;
	}
	printf("OPENED %s %u %u ", heard.callsign, heard.hz, heard.number);
	print_hex(heard.sound, VOIP_SOUND_BYTES);
	printf("\n");
	return 0;
}

int main(int argc, char **argv) {
	char hex[SHA256_HEX_ROOM];

	if (argc == 4 && !strcmp(argv[1], "stretch"))
		return stretch(argv);
	if (argc == 7 && !strcmp(argv[1], "proof")) {
		auth_proof(argv[2], argv[3], argv[4], argv[5], argv[6], hex);
		printf("%s\n", hex);
		return 0;
	}
	if (argc == 5 && !strcmp(argv[1], "key")) {
		auth_stream_key(argv[2], argv[3], argv[4], hex);
		printf("%s\n", hex);
		return 0;
	}
	if ((argc == 7 || argc == 8) && !strcmp(argv[1], "make"))
		return make(argc, argv);
	if (argc == 5 && !strcmp(argv[1], "open"))
		return open_packet(argv);
	fprintf(stderr, "usage: see the head of tests/voip_tool.c\n");
	return 2;
}
