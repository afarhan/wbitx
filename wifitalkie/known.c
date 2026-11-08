// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ashhar Farhan
//
// known.c — the WiFi networks the talkie has joined, and their keys.
//
// Up to SETTINGS_KNOWN of them are kept in the settings, the one joined
// last first. A network joined is remembered: moved to the front if it was
// there, its key as it was joined with, or put at the front with the one
// joined longest ago forgotten to make room. network.cpp remembers each
// network as it is joined, tries the others when the one it was given is
// not heard, and the screen joins one remembered without its key being
// entered again. The setup portal (portal.cpp) adds to it and forgets
// from it. It knows nothing of the ESP32, so tests/test_units.c tries
// it.

#include <stdio.h>
#include <string.h>

#include "known.h"

// where ssid is in the list, or -1
int known_find(const struct settings *s, const char *ssid) {
	int i;

	if (ssid[0] == 0)
		return -1;
	for (i = 0; i < SETTINGS_KNOWN; i++) {
		if (!strcmp(s->known_ssid[i], ssid))
			return i;
	}
	return -1;
}

// 1 if the list changed, and so wants saving
int known_remember(struct settings *s, const char *ssid, const char *password) {
	int at = known_find(s, ssid);
	int i;

	if (ssid[0] == 0)
		return 0;
	if (at == 0 && !strcmp(s->known_password[0], password))
		return 0;
	if (at < 0)
		at = SETTINGS_KNOWN - 1;
	for (i = at; i > 0; i--) {
		memcpy(s->known_ssid[i], s->known_ssid[i - 1], sizeof(s->known_ssid[i]));
		memcpy(s->known_password[i], s->known_password[i - 1], sizeof(s->known_password[i]));
	}
	snprintf(s->known_ssid[0], sizeof(s->known_ssid[0]), "%s", ssid);
	snprintf(s->known_password[0], sizeof(s->known_password[0]), "%s", password);
	return 1;
}

// the i'th forgotten, those after it moved up; 1 if there was one
int known_forget(struct settings *s, int i) {
	if (i < 0 || i >= SETTINGS_KNOWN || s->known_ssid[i][0] == 0)
		return 0;
	for (; i < SETTINGS_KNOWN - 1; i++) {
		memcpy(s->known_ssid[i], s->known_ssid[i + 1], sizeof(s->known_ssid[i]));
		memcpy(s->known_password[i], s->known_password[i + 1], sizeof(s->known_password[i]));
	}
	s->known_ssid[i][0] = 0;
	s->known_password[i][0] = 0;
	return 1;
}

// how many are remembered
int known_count(const struct settings *s) {
	int i;

	for (i = 0; i < SETTINGS_KNOWN && s->known_ssid[i][0] != 0; i++)
		;
	return i;
}
