// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ashhar Farhan
//
// network.cpp — the WiFi access point the talkie joins.
//
// One access point, the one set with the verb WIFI and kept in the
// settings. The Arduino WiFi library joins it and joins it again whenever
// it is lost; this says when it is up and down, on the console, and what it
// is, for STATUS.
//
// The radio's power saving is turned off. With it on, the C3 sleeps between
// the access point's beacons and takes what was sent meanwhile in bursts, a
// tenth of a second apart, which is more than the speaker's queue is meant
// to take up; a talkie that is speaking or listening is busy anyway.
//
// This is C++ for the WiFi library; the rest of the talkie asks it in C.

#include <Arduino.h>
#include <WiFi.h>

#include "console.h"
#include "network.h"
#include "settings.h"

#define HOSTNAME "wifitalkie"

static int was_up;

void network_open(void) {
	WiFi.mode(WIFI_STA);
	WiFi.setHostname(HOSTNAME);
	WiFi.setSleep(false);
	WiFi.setAutoReconnect(true);
	network_join();
}

void network_join(void) {
	WiFi.disconnect();
	if (settings.ssid[0] == 0) {
		console_say("WIFI NOT SET: WIFI <ssid> <password>");
		return;
	}
	WiFi.begin(settings.ssid, settings.password);
	console_say("WIFI JOINING %s", settings.ssid);
}

void network_poll(long now) {
	int is_up = network_is_up();

	(void)now;
	if (is_up == was_up)
		return;
	was_up = is_up;
	if (is_up)
		console_say("WIFI UP %s %s", settings.ssid, WiFi.localIP().toString().c_str());
	else
		console_say("WIFI DOWN");
}

int network_is_up(void) {
	return WiFi.status() == WL_CONNECTED;
}

void network_describe(char *text, int room) {
	if (settings.ssid[0] == 0) {
		snprintf(text, room, "NOT SET");
		return;
	}
	if (!network_is_up()) {
		snprintf(text, room, "JOINING %s", settings.ssid);
		return;
	}
	snprintf(text, room, "UP %s %s RSSI %d", settings.ssid,
		WiFi.localIP().toString().c_str(), (int)WiFi.RSSI());
}
