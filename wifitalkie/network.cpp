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
// carried from home to a hotspot finds its way without being told. A
// network the driver says is not heard at all (reason 201) is left after
// NOT_HEARD_MS, not JOIN_WAIT_MS, so that ten remembered are gone through
// quickly. With none set, the remembered are tried all the same.
//
// Going from one to the next, the join being tried must be stopped first,
// and WiFi.disconnect() does not stop it: it stops only a join made, and
// the library's own reconnecting starts the old one again after each
// failure. The driver then refuses the next one's name ("sta is
// connecting, cannot set config") and goes on trying the old, while the
// console says JOINING the next: on VU2XZ, 2026-11-11, Airtel_71hh was
// tried over and over and the remembered Sasi never. So begin() turns the
// reconnecting off, stops the driver itself, esp_wifi_disconnect(), lets
// it settle, and sets the next, up to BEGIN_TRIES times.
//
// network_ap_begin() puts up an access point of the talkie's own beside
// the one it joins, for the setup portal (portal.cpp), and network_ap_end()
// takes it down. While it is up the turn of the remembered is held: a
// join being tried hops the radio from channel to channel, and the
// telephone on the talkie's access point would lose it. A join asked for
// at the portal is still made.
//
// network_scan_begin() lists the networks in hearing for the screen,
// without holding the loop up: the driver scans by itself, network_poll()
// takes what it found, each name once, at its strongest, strongest first,
// up to SCAN_MOST. A join being tried has to be stopped for the scan, and
// is begun again after.
//
// network_local() leaves the WiFi for Local, talkie to talkie by ESP-NOW
// (local.c), and comes back to it: while settings.is_local no network is
// joined or tried, and the radio is held on LOCAL_CHANNEL, where every
// talkie in Local is. In Local the radio also sends at LOCAL_TX_POWER, not
// TX_POWER: the low power was for the access point's join on the SuperMini
// (above), and at 8.5 dBm two talkies heard each other only a few metres
// apart, the author's trial of 2026-11-10; and Espressif's Long Range
// protocol, LR, is let in beside 802.11 b/g/n, for local.c to send at its
// slow rate and the talkies to hear each other at it. Back on the WiFi
// both are as they were. A scan for the WiFi page, or the setup portal's
// access point, moves it for a while; network_join() puts it back.
//
// network_off() stops the radio altogether, until network_on(); it is for
// measuring what the radio puts into the microphone.
//
// This is C++ for the WiFi library; the rest of the talkie asks it in C.

#include <Arduino.h>
#include <WiFi.h>

#include <string.h>

#include "esp_wifi.h"

#include "console.h"
#include "known.h"
#include "local.h"
#include "network.h"
#include "settings.h"

#define HOSTNAME "wifitalkie"
#define TX_POWER WIFI_POWER_8_5dBm
#define LOCAL_TX_POWER WIFI_POWER_19_5dBm
#define PROTOCOLS (WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N)
#define JOIN_WAIT_MS 20000
#define NOT_HEARD_MS 3000
#define REASON_NOT_HEARD 201
#define SCAN_MOST 12
#define BEGIN_TRIES 5
#define BEGIN_SETTLE_MS 50

static int was_up;
static int reason_said;
static int tried;
static long join_began_ms;
static int is_scanning;
static int is_join_after_scan;
static int is_held;
static int is_ap_up;
static volatile int is_not_heard;
static char scanned_ssid[SCAN_MOST][SETTINGS_SSID_ROOM];
static int scanned_rssi[SCAN_MOST];
static int scanned_open[SCAN_MOST];
static int scanned_count;
// the network begin() was last given, for the screen: while not up, the
// one being tried, which may be a remembered one, not the one set
static char trying[SETTINGS_SSID_ROOM];

// why a join failed, as the WiFi driver says it (esp_wifi_types.h's
// wifi_err_reason_t: 201 no access point of that name heard, 15 and 204 the
// key's handshake failed, 202 refused, 200 its beacons lost), once for each
// change of reason, so that a join tried over and over does not fill the
// console
static int is_off;

static void on_lost(arduino_event_id_t event, arduino_event_info_t info) {
	int reason = info.wifi_sta_disconnected.reason;

	(void)event;
	if (reason == REASON_NOT_HEARD)
		is_not_heard = 1;
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
	if (settings.is_local)
		network_local(1);
	else
		network_join();
}

// no network joined, and the radio on the channel every talkie in Local
// is on
static void hold_local_channel(void) {
	WiFi.disconnect();
	if (esp_wifi_set_protocol(WIFI_IF_STA, PROTOCOLS | WIFI_PROTOCOL_LR) != ESP_OK)
		console_say("LOCAL: LONG RANGE NOT SET");
	if (esp_wifi_set_channel(LOCAL_CHANNEL, WIFI_SECOND_CHAN_NONE) != ESP_OK)
		console_say("LOCAL: CHANNEL %d NOT SET", LOCAL_CHANNEL);
	WiFi.setTxPower(LOCAL_TX_POWER);
}

// to Local, is_on, or back to the WiFi; settings.is_local is set first
void network_local(int is_on) {
	if (is_on) {
		WiFi.setAutoReconnect(false);
		hold_local_channel();
		local_open();
	} else {
		local_close();
		esp_wifi_set_protocol(WIFI_IF_STA, PROTOCOLS);
		WiFi.setTxPower(TX_POWER);
		WiFi.setAutoReconnect(true);
		network_join();
	}
}

static void begin(const char *ssid, const char *password) {
	int is_set = 0;
	int i;

	WiFi.setAutoReconnect(false);
	for (i = 0; i < BEGIN_TRIES && !is_set; i++) {
		esp_wifi_disconnect();
		vTaskDelay(pdMS_TO_TICKS(BEGIN_SETTLE_MS));
		WiFi.STA.begin();
		WiFi.setTxPower(TX_POWER);
		is_set = WiFi.STA.connect(ssid, password);
	}
	WiFi.setAutoReconnect(true);
	WiFi.setTxPower(TX_POWER);
	join_began_ms = (long)millis();
	snprintf(trying, sizeof(trying), "%s", ssid);
	if (is_set)
		console_say("WIFI JOINING %s", ssid);
	else
		console_say("WIFI %s NOT BEGUN: THE DRIVER WOULD NOT LET GO", ssid);
}

// the network set, from the start of the turn of those remembered
static void try_next(void);

void network_join(void) {
	tried = 0;
	is_not_heard = 0;
	if (settings.is_local) {
		hold_local_channel();
		return;
	}
	if (settings.ssid[0] == 0 && known_count(&settings) > 0) {
		try_next();
		return;
	}
	if (settings.ssid[0] == 0) {
		WiFi.disconnect();
		console_say("WIFI NOT SET: WIFI <ssid> <password>, OR SETUP");
		return;
	}
	begin(settings.ssid, settings.password);
}

// the next remembered network after the one tried, or the one set again
static void try_next(void) {
	is_not_heard = 0;
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
	local_close();
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
	if (settings.is_local)
		local_open();
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
		if (!is_held)
			network_join();
	}
}

void network_poll(long now) {
	int is_up = network_is_up();

	if (is_scanning)
		scan_poll();
	if (is_off)
		return;
	if (!is_up && !is_scanning && !is_held && !settings.is_local && (settings.ssid[0] != 0 || known_count(&settings) > 0)
			&& (now - join_began_ms >= JOIN_WAIT_MS || (is_not_heard && now - join_began_ms >= NOT_HEARD_MS)))
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

// the network being tried while none is joined, for the screen; nothing
// while one is up, in Local, with the radio off, or with none to try
void network_trying(char *text, int room) {
	text[0] = 0;
	if (!network_is_up() && !is_off && !settings.is_local && !is_held)
		snprintf(text, room, "%s", trying);
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
	if (settings.is_local) {
		snprintf(text, room, "NOT JOINED: LINK LOCAL");
		return;
	}
	if (settings.ssid[0] == 0) {
		snprintf(text, room, "NOT SET");
		return;
	}
	if (!network_is_up()) {
		snprintf(text, room, "JOINING %s (STATUS %d)", trying[0] ? trying : settings.ssid, (int)WiFi.status());
		return;
	}
	snprintf(text, room, "UP %s %s RSSI %d", WiFi.SSID().c_str(),
		WiFi.localIP().toString().c_str(), (int)WiFi.RSSI());
}

// the talkie's own access point, open, named name, beside the one joined;
// the turn of the remembered held while it is up
int network_ap_begin(const char *name) {
	is_held = 1;
	if (!network_is_up())
		WiFi.disconnect();
	WiFi.mode(WIFI_AP_STA);
	if (!WiFi.softAP(name)) {
		console_say("SETUP: THE ACCESS POINT DID NOT START");
		network_ap_end();
		return 0;
	}
	WiFi.setTxPower(TX_POWER);
	is_ap_up = 1;
	console_say("SETUP: ACCESS POINT %s AT %s", name, WiFi.softAPIP().toString().c_str());
	return 1;
}

void network_ap_end(void) {
	if (is_ap_up)
		WiFi.softAPdisconnect(true);
	is_ap_up = 0;
	is_held = 0;
	WiFi.mode(WIFI_STA);
	WiFi.setTxPower(TX_POWER);
	if (!network_is_up())
		network_join();
}

void network_ap_address(char *text, int room) {
	snprintf(text, room, "%s", WiFi.softAPIP().toString().c_str());
}

// the telephones and such on the talkie's access point
int network_ap_clients(void) {
	return is_ap_up ? (int)WiFi.softAPgetStationNum() : 0;
}
