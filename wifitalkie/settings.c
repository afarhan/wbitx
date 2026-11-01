// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ashhar Farhan
//
// settings.c — what the talkie keeps across power-ups, in the ESP32's NVS.
//
// One struct, settings, read whole at power-up and written whole whenever a
// verb changes any of it; the verbs are few and typed by a person, so the
// writing is seldom. Each field has a key of its own in the namespace
// "talkie", so that a field added later finds the others where they were.
// A field never written keeps its default.
//
// The cookie is kept here, and its stretch beside it with what it was made
// for, "<callsign> <cookie>", so that the stretch is not done again at
// every power-up; service.c does it again when the two no longer match.
//
// Writing to the flash holds up the speaker's timer for a moment; a verb
// typed in the middle of an over may click.

#include <stdio.h>
#include <string.h>

#include "nvs.h"

#include "settings.h"

#define NAMESPACE "talkie"

struct settings settings;

static void text_get(nvs_handle_t handle, const char *key, char *value, size_t room) {
	size_t length = room;

	if (nvs_get_str(handle, key, value, &length) != ESP_OK)
		return;
	value[room - 1] = 0;
}

static void number_get(nvs_handle_t handle, const char *key, uint32_t *value) {
	uint32_t got;

	if (nvs_get_u32(handle, key, &got) == ESP_OK)
		*value = got;
}

void settings_load(void) {
	nvs_handle_t handle;

	memset(&settings, 0, sizeof(settings));
	snprintf(settings.host, sizeof(settings.host), "%s", SETTINGS_DEFAULT_HOST);
	settings.port = SETTINGS_DEFAULT_PORT;
	settings.volume = SETTINGS_DEFAULT_VOLUME;
	settings.mic = SETTINGS_DEFAULT_MIC;
	settings.is_vox = SETTINGS_DEFAULT_VOX;
	settings.is_voip_online = 1;
	if (nvs_open(NAMESPACE, NVS_READONLY, &handle) != ESP_OK)
		return;
	text_get(handle, "ssid", settings.ssid, sizeof(settings.ssid));
	text_get(handle, "password", settings.password, sizeof(settings.password));
	text_get(handle, "callsign", settings.callsign, sizeof(settings.callsign));
	text_get(handle, "grid", settings.grid, sizeof(settings.grid));
	text_get(handle, "host", settings.host, sizeof(settings.host));
	number_get(handle, "port", &settings.port);
	number_get(handle, "hz", &settings.hz);
	text_get(handle, "cookie", settings.cookie, sizeof(settings.cookie));
	text_get(handle, "stretched", settings.stretched, sizeof(settings.stretched));
	text_get(handle, "stretched_for", settings.stretched_for, sizeof(settings.stretched_for));
	number_get(handle, "voip", &settings.is_voip_online);
	number_get(handle, "volume", &settings.volume);
	number_get(handle, "mic", &settings.mic);
	number_get(handle, "vox", &settings.is_vox);
	nvs_close(handle);
}

void settings_save(void) {
	nvs_handle_t handle;

	if (nvs_open(NAMESPACE, NVS_READWRITE, &handle) != ESP_OK)
		return;
	nvs_set_str(handle, "ssid", settings.ssid);
	nvs_set_str(handle, "password", settings.password);
	nvs_set_str(handle, "callsign", settings.callsign);
	nvs_set_str(handle, "grid", settings.grid);
	nvs_set_str(handle, "host", settings.host);
	nvs_set_u32(handle, "port", settings.port);
	nvs_set_u32(handle, "hz", settings.hz);
	nvs_set_str(handle, "cookie", settings.cookie);
	nvs_set_str(handle, "stretched", settings.stretched);
	nvs_set_str(handle, "stretched_for", settings.stretched_for);
	nvs_set_u32(handle, "voip", settings.is_voip_online);
	nvs_set_u32(handle, "volume", settings.volume);
	nvs_set_u32(handle, "mic", settings.mic);
	nvs_set_u32(handle, "vox", settings.is_vox);
	nvs_commit(handle);
	nvs_close(handle);
}
