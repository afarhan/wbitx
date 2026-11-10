// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ashhar Farhan
//
// local.c — talkie to talkie by ESP-NOW, off the internet.
//
// With the link set to Local (Menu, Link; the verb LINK), the talkie
// joins no network: network.cpp leaves the WiFi and holds the radio on
// LOCAL_CHANNEL, the same on every talkie, and voip.c sends its packets
// here instead of to my.sbitx.net. Each goes out as an ESP-NOW broadcast,
// to every talkie in hearing; there is no repeater, so only those in
// hearing of each other talk, some tens of metres indoors.
//
// The packet is voip_packet.c's, 196 bytes, inside ESP-NOW's 250: an id of
// its own, LOCAL_ID in voip.c, the sender's callsign written in, the
// frequency, which is the room as it is online, the number, the speech.
// Its tag is made with a key every talkie knows, so it says only that the
// packet came whole, not who sent it. Broadcast frames cannot be
// encrypted by ESP-NOW: anyone near with an ESP32 can hear, as on an open
// radio channel.
//
// Each is sent at Espressif's Long Range rate, LOCAL_RATE, 250 kbps, its
// slowest and farthest, the author's choice of 2026-11-10; the talkies'
// radios are let to hear it by network.cpp. The speech is 64 kbps of
// mu-law, a packet of 196 bytes every 20 ms, about 80 kbps with its head
// and tag, some 8 ms of air a packet at this rate: one talker at a time
// has room.
//
// The radio's task hands what is heard to on_heard(); it is put in a
// queue of HEARD_KEPT packets, and voip.c takes them out in the loop, with
// local_take(). A packet that is not a talkie's size is not kept. The
// signal each came in at is kept, the last and the weakest since the last
// local_describe(), for the home page while one is heard and for STATUS:
// how far two talkies are from losing each other.

#include <stdio.h>
#include <string.h>

#include "esp_now.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#include "console.h"
#include "local.h"

#define HEARD_KEPT 8
#define MAC_BYTES 6
#define LOCAL_RATE WIFI_PHY_RATE_LORA_250K

static const uint8_t everyone[MAC_BYTES] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
#define NO_RSSI 0
#define QUARTERS_A_DB 4

static QueueHandle_t heard;
static int is_open;
static volatile int last_rssi = NO_RSSI;
static volatile int weakest_rssi = NO_RSSI;

static void on_heard(const esp_now_recv_info_t *info, const uint8_t *data, int length) {
	int rssi;

	if (length != VOIP_PACKET_BYTES || heard == NULL)
		return;
	if (info != NULL && info->rx_ctrl != NULL) {
		rssi = info->rx_ctrl->rssi;
		last_rssi = rssi;
		if (weakest_rssi == NO_RSSI || rssi < weakest_rssi)
			weakest_rssi = rssi;
	}
	xQueueSend(heard, data, 0);
}

// the signal of the last packet heard, in dBm; 0 if none yet
int local_rssi(void) {
	return last_rssi;
}

// for STATUS: the power sent at, and the signal heard, the last and the
// weakest since it was last asked
void local_describe(char *text, int room) {
	int8_t quarters = 0;

	esp_wifi_get_max_tx_power(&quarters);
	snprintf(text, room, "SENDING AT %d.%02d DBM, HEARD AT %d DBM, WEAKEST %d DBM",
		quarters / QUARTERS_A_DB, quarters % QUARTERS_A_DB * 25, last_rssi, weakest_rssi);
	weakest_rssi = NO_RSSI;
}

// ESP-NOW begun, with every talkie as the one peer; 0 if it would not
int local_open(void) {
	esp_now_peer_info_t peer;

	if (is_open)
		return 1;
	if (heard == NULL)
		heard = xQueueCreate(HEARD_KEPT, VOIP_PACKET_BYTES);
	if (heard == NULL || esp_now_init() != ESP_OK) {
		console_say("LOCAL: ESP-NOW DID NOT START");
		return 0;
	}
	esp_now_register_recv_cb(on_heard);
	memset(&peer, 0, sizeof(peer));
	memcpy(peer.peer_addr, everyone, MAC_BYTES);
	peer.channel = 0;
	peer.ifidx = WIFI_IF_STA;
	peer.encrypt = false;
	if (esp_now_add_peer(&peer) != ESP_OK) {
		console_say("LOCAL: NO PEER");
		esp_now_deinit();
		return 0;
	}
	esp_now_rate_config_t rate = {
		.phymode = WIFI_PHY_MODE_LR,
		.rate = LOCAL_RATE,
		.ersu = false,
		.dcm = false,
	};
	if (esp_now_set_peer_rate_config(everyone, &rate) != ESP_OK)
		console_say("LOCAL: LONG RANGE RATE NOT SET");
	xQueueReset(heard);
	is_open = 1;
	console_say("LOCAL: ON CHANNEL %d, LONG RANGE", LOCAL_CHANNEL);
	return 1;
}

void local_close(void) {
	if (!is_open)
		return;
	esp_now_unregister_recv_cb();
	esp_now_deinit();
	is_open = 0;
	console_say("LOCAL: CLOSED");
}

int local_is_open(void) {
	return is_open;
}

// 1 if the packet was handed to the radio
int local_send(const uint8_t packet[VOIP_PACKET_BYTES]) {
	return is_open && esp_now_send(everyone, packet, VOIP_PACKET_BYTES) == ESP_OK;
}

// 1 with a packet heard, 0 with none waiting
int local_take(uint8_t packet[VOIP_PACKET_BYTES]) {
	return is_open && heard != NULL && xQueueReceive(heard, packet, 0) == pdTRUE;
}
