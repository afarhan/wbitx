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
#define SETTINGS_CALLSIGN_ROOM 16
#define SETTINGS_GRID_ROOM 12
#define SETTINGS_HOST_ROOM 64
#define SETTINGS_STRETCHED_FOR_ROOM (SETTINGS_CALLSIGN_ROOM + AUTH_COOKIE_ROOM + 1)
#define SETTINGS_DEFAULT_HOST "my.sbitx.net"
#define SETTINGS_DEFAULT_PORT 8081
#define SETTINGS_DEFAULT_VOLUME 50
#define SETTINGS_DEFAULT_MIC 50
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
};

extern struct settings settings;

void settings_load(void);
void settings_save(void);

#ifdef __cplusplus
}
#endif
