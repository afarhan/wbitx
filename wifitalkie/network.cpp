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
// near its access point. It is set before each join as well as after, so
// that the first frames of a join, the most current the talkie draws, are
// not sent at full power: on a weak 3.3 V regulator they browned it out.
//
// Each network joined is remembered with its key, known.c, up to
// SETTINGS_KNOWN of them. When the one set is not joined within
// JOIN_WAIT_MS, the others remembered are tried in turn, each for as long,
// and then the one set again; one joined so becomes the one set. A talkie
// carried from home to a hotspot finds its way without being told.
//
// network_scan_begin() lists the networks in hearing for the screen,
// without holding the loop up: the driver scans by itself, network_poll()
// takes what it found, each name once, at its strongest, strongest first,
// up to SCAN_MOST. A join being tried has to be stopped for the scan, and
// is begun again after.
//
// network_off() stops the radio altogether, until network_on(); it is for
// measuring what the radio puts into the microphone.
//
// This is C++ for the WiFi library; the rest of the talkie asks it in C.

#include <Arduino.h>
#include <WiFi.h>

#include <string.h>

#include "console.h"
#include "known.h"
#include "network.h"
#include "settings.h"

#define HOSTNAME "wifitalkie"
#define TX_POWER WIFI_POWER_8_5dBm
#define JOIN_WAIT_MS 20000
#define SCAN_MOST 12

static int was_up;
static int reason_said;
static int tried;
static long join_began_ms;
static int is_scanning;
static int is_join_after_scan;
static char scanned_ssid[SCAN_MOST][SETTINGS_SSID_ROOM];
static int scanned_rssi[SCAN_MOST];
static int scanned_open[SCAN_MOST];
static int scanned_count;

// why a join failed, as the WiFi driver says it (esp_wifi_types.h's
// wifi_err_reason_t: 201 no access point of that name heard, 15 and 204 the
// key's handshake failed, 202 refused, 200 its beacons lost), once for each
// change of reason, so that a join tried over and over does not fill the
// console
static int is_off;

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
	WiFi.setTxPower(TX_POWER);
	WiFi.setHostname(HOSTNAME);
	WiFi.setSleep(false);
	WiFi.setAutoReconnect(true);
	WiFi.onEvent(on_lost, ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
	network_join();
}

static void begin(const char *ssid, const char *password) {
	WiFi.disconnect();
	WiFi.setTxPower(TX_POWER);
	WiFi.begin(ssid, password);
	WiFi.setTxPower(TX_POWER);
	join_began_ms = (long)millis();
	console_say("WIFI JOINING %s", ssid);
}

// the network set, from the start of the turn of those remembered
void network_join(void) {
	tried = 0;
	if (settings.ssid[0] == 0) {
		WiFi.disconnect();
		console_say("WIFI NOT SET: WIFI <ssid> <password>");
		return;
	}
	begin(settings.ssid, settings.password);
}

// the next remembered network after the one tried, or the one set again
static void try_next(void) {
	while (++tried <= SETTINGS_KNOWN) {
		if (settings.known_ssid[tried - 1][0] != 0
				&& strcmp(settings.known_ssid[tried - 1], settings.ssid)) {
			begin(settings.known_ssid[tried - 1], settings.known_password[tried - 1]);
			return;
		}
	}
	network_join();
}

// joined: one remembered becomes the one set, and the one set is
// remembered
static void joined(void) {
	int is_changed = 0;

	if (tried > 0) {
		snprintf(settings.ssid, sizeof(settings.ssid), "%s", settings.known_ssid[tried - 1]);
		snprintf(settings.password, sizeof(settings.password), "%s", settings.known_password[tried - 1]);
		is_changed = 1;
		tried = 0;
	}
	if (known_remember(&settings, settings.ssid, settings.password))
		is_changed = 1;
	if (is_changed)
		settings_save();
}

void network_off(void) {
	is_off = 1;
	WiFi.mode(WIFI_OFF);
	console_say("RADIO OFF");
}

void network_on(void) {
	is_off = 0;
	WiFi.mode(WIFI_STA);
	WiFi.setTxPower(TX_POWER);
	WiFi.setSleep(false);
	console_say("RADIO ON");
	network_join();
}

// the scan done: each name once, at its strongest, strongest first
static void scan_taken(int found) {
	int rssi;
	int i;
	int j;

	scanned_count = 0;
	for (i = 0; i < found; i++) {
		rssi = (int)WiFi.RSSI(i);
		if (WiFi.SSID(i).length() == 0)
			continue;
		for (j = 0; j < scanned_count; j++) {
			if (!strcmp(scanned_ssid[j], WiFi.SSID(i).c_str()))
				break;
		}
		if (j < scanned_count && scanned_rssi[j] >= rssi)
			continue;
		if (j == scanned_count) {
			if (scanned_count == SCAN_MOST)
				continue;
			scanned_count++;
		}
		snprintf(scanned_ssid[j], sizeof(scanned_ssid[j]), "%s", WiFi.SSID(i).c_str());
		scanned_rssi[j] = rssi;
		scanned_open[j] = WiFi.encryptionType(i) == WIFI_AUTH_OPEN;
	}
	for (i = 1; i < scanned_count; i++) {
		for (j = i; j > 0 && scanned_rssi[j] > scanned_rssi[j - 1]; j--) {
			char ssid[SETTINGS_SSID_ROOM];
			int swapped = scanned_rssi[j];

			memcpy(ssid, scanned_ssid[j], sizeof(ssid));
			memcpy(scanned_ssid[j], scanned_ssid[j - 1], sizeof(ssid));
			memcpy(scanned_ssid[j - 1], ssid, sizeof(ssid));
			scanned_rssi[j] = scanned_rssi[j - 1];
			scanned_rssi[j - 1] = swapped;
			swapped = scanned_open[j];
			scanned_open[j] = scanned_open[j - 1];
			scanned_open[j - 1] = swapped;
		}
	}
	console_say("WIFI SCAN: %d HEARD", scanned_count);
}

static void scan_poll(void) {
	int found = WiFi.scanComplete();

	if (found == WIFI_SCAN_RUNNING)
		return;
	is_scanning = 0;
	if (found > 0)
		scan_taken(found);
	else
		scanned_count = 0;
	WiFi.scanDelete();
	if (is_join_after_scan) {
		is_join_after_scan = 0;
		network_join();
	}
}

void network_poll(long now) {
	int is_up = network_is_up();

	if (is_scanning)
		scan_poll();
	if (is_off)
		return;
	if (!is_up && !is_scanning && settings.ssid[0] != 0 && now - join_began_ms >= JOIN_WAIT_MS)
		try_next();
	if (is_up == was_up)
		return;
	was_up = is_up;
	reason_said = 0;
	if (is_up) {
		joined();
		console_say("WIFI UP %s %s", settings.ssid, WiFi.localIP().toString().c_str());
	} else {
		join_began_ms = now;
		console_say("WIFI DOWN");
	}
}

void network_scan_begin(void) {
	if (is_scanning)
		return;
	if (!network_is_up()) {
		WiFi.disconnect();
		is_join_after_scan = 1;
	}
	scanned_count = 0;
	WiFi.scanDelete();
	if (WiFi.scanNetworks(true) == WIFI_SCAN_FAILED) {
		console_say("WIFI SCAN: IT DID NOT START");
		if (is_join_after_scan) {
			is_join_after_scan = 0;
			network_join();
		}
		return;
	}
	is_scanning = 1;
}

int network_is_scanning(void) {
	return is_scanning;
}

int network_scanned_count(void) {
	return scanned_count;
}

// the i'th network heard, strongest first
void network_scanned(int i, char *ssid, int room, int *rssi, int *is_open) {
	snprintf(ssid, room, "%s", scanned_ssid[i]);
	*rssi = scanned_rssi[i];
	*is_open = scanned_open[i];
}

// the network joined now, which may be a remembered one, not the one set
void network_ssid(char *text, int room) {
	text[0] = 0;
	if (network_is_up())
		snprintf(text, room, "%s", WiFi.SSID().c_str());
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

// the address and the signal, for the screen; nothing while not up
void network_address(char *text, int room) {
	text[0] = 0;
	if (network_is_up())
		snprintf(text, room, "%s", WiFi.localIP().toString().c_str());
}

int network_rssi(void) {
	if (!network_is_up())
		return 0;
	return (int)WiFi.RSSI();
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
