// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ashhar Farhan
//
// settings.h — what the talkie keeps across power-ups. See settings.c.

#pragma once

#include <stdint.h>

#include "auth.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SETTINGS_SSID_ROOM 33
#define SETTINGS_PASSWORD_ROOM 65
#define SETTINGS_KNOWN 10
#define SETTINGS_CONTACTS 20
#define SETTINGS_CALLSIGN_ROOM 16
#define SETTINGS_GRID_ROOM 12
#define SETTINGS_HOST_ROOM 64
#define SETTINGS_STRETCHED_FOR_ROOM (SETTINGS_CALLSIGN_ROOM + AUTH_COOKIE_ROOM + 1)
#define SETTINGS_DEFAULT_HOST "my.sbitx.net"
#define SETTINGS_DEFAULT_PORT 8081
#define SETTINGS_DEFAULT_VOLUME 50
#define SETTINGS_DEFAULT_MIC 50
#define SETTINGS_DEFAULT_GATE 1
#define SETTINGS_DEFAULT_WPM 12
#define SETTINGS_SIDETONE_MOST 10
#define SETTINGS_DEFAULT_SIDETONE 5
#define SETTINGS_LEVEL_MOST 100

struct settings {
	char ssid[SETTINGS_SSID_ROOM];
	char password[SETTINGS_PASSWORD_ROOM];
	char callsign[SETTINGS_CALLSIGN_ROOM];
	char grid[SETTINGS_GRID_ROOM];
	char host[SETTINGS_HOST_ROOM];
	uint32_t port;
	uint32_t hz;
	char cookie[AUTH_COOKIE_ROOM];
	char stretched[SHA256_HEX_ROOM];
	char stretched_for[SETTINGS_STRETCHED_FOR_ROOM];
	uint32_t is_voip_online;
	uint32_t volume;
	uint32_t mic;
	uint32_t is_gate;            // speech.c's gate: the gaps as comfort noise, GATE
	uint32_t vox;                // VOX sensitivity, 0 off or 1 to VOX_LEVEL_MOST (vox.c)
	uint32_t wpm;                // the Morse keyer's words a minute, for text
	uint32_t sidetone;           // the Morse keyer's sidetone, 0 (off) to SETTINGS_SIDETONE_MOST
	uint32_t is_headset;         // the mic of the handsfree, not the in-built one
	uint32_t is_white;           // the display's theme: dark dots on white
	uint32_t is_morse_typing;    // text keyed as Morse, not chosen on the keyboard
	uint32_t is_local;           // talkie to talkie by ESP-NOW, off the internet
	int32_t battery_offset_mv;   // added to the cell's reading, set by BATTERY <volts>
	char known_ssid[SETTINGS_KNOWN][SETTINGS_SSID_ROOM];
	char known_password[SETTINGS_KNOWN][SETTINGS_PASSWORD_ROOM];
	char contacts[SETTINGS_CONTACTS][SETTINGS_CALLSIGN_ROOM];
};

extern struct settings settings;

void settings_load(void);
void settings_save(void);

#ifdef __cplusplus
}
#endif
