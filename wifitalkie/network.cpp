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
// The radio sends at TX_POWER, 8.5 dBm, not the C3's full 20. Many C3
// SuperMini boards are known for an antenna badly matched to the chip; on
// the author's first board on 31 October 2026, at full power, it heard the
// access points but was never heard back (the driver's reason 2, the login
// timed out); at 8.5 dBm it joined, at once with a hotspot heard at -44 dBm,
// and only now and then with the house's, heard at -75. A talkie is used
// near its access point.
//
// network_off() stops the radio altogether, until network_on(); it is for
// measuring what the radio puts into the microphone.
//
// This is C++ for the WiFi library; the rest of the talkie asks it in C.

#include <Arduino.h>
#include <WiFi.h>

#include "console.h"
#include "network.h"
#include "settings.h"

#define HOSTNAME "wifitalkie"
#define TX_POWER WIFI_POWER_8_5dBm

static int was_up;
static int reason_said;

// why a join failed, as the WiFi driver says it (esp_wifi_types.h's
// wifi_err_reason_t: 201 no access point of that name heard, 15 and 204 the
// key's handshake failed, 202 refused, 200 its beacons lost), once for each
// change of reason, so that a join tried over and over does not fill the
// console
static void on_lost(arduino_event_id_t event, arduino_event_info_t info) {
	int reason = info.wifi_sta_disconnected.reason;

	(void)event;
	if (reason == reason_said)
		return;
	reason_said = reason;
	console_say("WIFI NOT JOINED: REASON %d, RSSI %d", reason, (int)info.wifi_sta_disconnected.rssi);
}

void network_open(void) {
	WiFi.mode(WIFI_STA);
	WiFi.setHostname(HOSTNAME);
	WiFi.setSleep(false);
	WiFi.setAutoReconnect(true);
	WiFi.onEvent(on_lost, ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
	network_join();
}

void network_join(void) {
	WiFi.disconnect();
	if (settings.ssid[0] == 0) {
		console_say("WIFI NOT SET: WIFI <ssid> <password>");
		return;
	}
	WiFi.begin(settings.ssid, settings.password);
	WiFi.setTxPower(TX_POWER);
	console_say("WIFI JOINING %s", settings.ssid);
}

void network_off(void) {
	WiFi.mode(WIFI_OFF);
	console_say("RADIO OFF");
}

void network_on(void) {
	WiFi.mode(WIFI_STA);
	WiFi.setSleep(false);
	console_say("RADIO ON");
	network_join();
}

void network_poll(long now) {
	int is_up = network_is_up();

	(void)now;
	if (is_up == was_up)
		return;
	was_up = is_up;
	reason_said = 0;
	if (is_up)
		console_say("WIFI UP %s %s", settings.ssid, WiFi.localIP().toString().c_str());
	else
		console_say("WIFI DOWN");
}

// the access points in hearing, strongest first; the WiFi stops a few
// seconds for it, so this is for the verb SCAN, not for when speaking
void network_scan(void) {
	int found;
	int i;

	// a scan is refused while a join is being tried; the join is begun
	// again after
	WiFi.disconnect();
	found = WiFi.scanNetworks();
	if (found <= 0) {
		console_say("SCAN: NONE HEARD (%d)", found);
		network_join();
		return;
	}
	for (i = 0; i < found; i++)
		console_say("SCAN %s RSSI %d CHANNEL %d", WiFi.SSID(i).c_str(), (int)WiFi.RSSI(i),
			(int)WiFi.channel(i));
	WiFi.scanDelete();
	network_join();
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
		snprintf(text, room, "JOINING %s (STATUS %d)", settings.ssid, (int)WiFi.status());
		return;
	}
	snprintf(text, room, "UP %s %s RSSI %d", settings.ssid,
		WiFi.localIP().toString().c_str(), (int)WiFi.RSSI());
}
