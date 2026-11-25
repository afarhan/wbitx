// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ashhar Farhan
//
// enrol.c — the talkie learning its callsign from my.sbitx.net.
//
// A talkie new from its box has no callsign. Its owner adds it at
// my.sbitx.net/wbitx, the author's flow of 2026-11-25: signed in, or with
// an account made there, they type the code the talkie shows (Device key).
// Meanwhile the talkie, once on a network, asks the website every ASK_MS
// who it is: POST /talkie/whoami, https, with its code (the cookie), and
// the answer is a line, OK and the callsign of the account it was added to,
// or WAIT (accounts.py's whoami()). Told, it keeps the callsign and asks
// the service for the line again with it, and is on line. A talkie with a
// callsign that the service refuses asks too, every REFUSED_ASK_MS, so
// that one moved to another account finds its callsign by itself.
//
// The ask is made on a task of its own, ENROL_STACK, as the TLS handshake
// takes a second or two on the C3, and the loop goes on meanwhile; the
// loop takes up the answer.

#include <stdio.h>
#include <string.h>

#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "console.h"
#include "enrol.h"
#include "network.h"
#include "service.h"
#include "settings.h"

#define ASK_MS 5000
#define REFUSED_ASK_MS 30000
#define TIMEOUT_MS 15000
#define ENROL_STACK 8192
#define ENROL_PRIORITY 1
#define BODY_ROOM 64
#define ANSWER_ROOM 64
#define URL_ROOM 128

static volatile int is_asking;
static volatile int is_answered;
static char answer[ANSWER_ROOM];
static long next_ms;
static int is_took;

static void ask_task(void *unused) {
	esp_http_client_config_t http = {0};
	esp_http_client_handle_t client;
	char url[URL_ROOM];
	char body[BODY_ROOM];
	int got = -1;

	(void)unused;
	snprintf(url, sizeof(url), "https://%s/talkie/whoami", settings.host);
	snprintf(body, sizeof(body), "code=%s", settings.cookie);
	http.url = url;
	http.method = HTTP_METHOD_POST;
	http.crt_bundle_attach = esp_crt_bundle_attach;
	http.timeout_ms = TIMEOUT_MS;
	client = esp_http_client_init(&http);
	answer[0] = 0;
	if (client != NULL) {
		esp_http_client_set_header(client, "Content-Type", "application/x-www-form-urlencoded");
		if (esp_http_client_open(client, (int)strlen(body)) == ESP_OK
				&& esp_http_client_write(client, body, (int)strlen(body)) == (int)strlen(body)) {
			esp_http_client_fetch_headers(client);
			if (esp_http_client_get_status_code(client) == 200)
				got = esp_http_client_read(client, answer, sizeof(answer) - 1);
		}
		esp_http_client_close(client);
		esp_http_client_cleanup(client);
	}
	answer[got > 0 ? got : 0] = 0;
	answer[strcspn(answer, "\r\n")] = 0;
	is_answered = 1;
	is_asking = 0;
	vTaskDelete(NULL);
}

// the answer taken up: a callsign, kept if it is new
static void take_answer(void) {
	const char *call = answer + 3;

	is_answered = 0;
	if (strncmp(answer, "OK ", 3) || *call == 0 || strlen(call) >= sizeof(settings.callsign))
		return;
	if (!strcmp(call, settings.callsign))
		return;
	snprintf(settings.callsign, sizeof(settings.callsign), "%s", call);
	settings_save();
	service_restart();
	is_took = 1;
	console_say("ENROL: MY.SBITX.NET SAYS THIS TALKIE IS %s", call);
}

// 1 while the talkie has no callsign, or one the service refuses
static int is_due(void) {
	return settings.callsign[0] == 0 || service_is_unregistered();
}

void enrol_poll(long now) {
	char shown[AUTH_COOKIE_SHOWN_ROOM];

	if (is_answered)
		take_answer();
	if (!is_due() || is_asking || !network_is_up() || now - next_ms < 0)
		return;
	next_ms = now + (settings.callsign[0] == 0 ? ASK_MS : REFUSED_ASK_MS);
	service_code(shown);	// the cookie made, if there is none yet
	is_asking = 1;
	if (xTaskCreate(ask_task, "enrol", ENROL_STACK, NULL, ENROL_PRIORITY, NULL) != pdPASS)
		is_asking = 0;
}

// 1 while it waits to be added at my.sbitx.net, for the screen
int enrol_is_waiting(void) {
	return settings.callsign[0] == 0 && network_is_up();
}

// 1, once, when it has just been told its callsign
int enrol_took(void) {
	int was = is_took;

	is_took = 0;
	return was;
}
