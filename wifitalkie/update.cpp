// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ashhar Farhan
//
// update.cpp — new firmware, sent from a browser to the setup portal.
//
// The C3's USB can only be a serial port, never a drive to copy a file to,
// so the new firmware comes over the WiFi instead, on the setup portal's
// Firmware tab (portal.cpp). Each time the portal opens, update_open()
// chooses a code of four figures, which the talkie's Setup page shows and
// the Firmware tab asks for before it takes a file: the access point has
// no key, and anyone near could otherwise send the talkie firmware of
// their own. A wrong code is refused, and so is any file once the portal
// is closed.
//
// The file is wifitalkie.ino.bin, as `make` builds it. The flash holds two
// slots for the firmware, the min_spiffs partitions: the file is written
// to the one not running, as it comes, by the Arduino Update library,
// which checks it is whole and is a C3's firmware before the slot is
// marked to start next. Then the talkie starts again into it.
//
// The new firmware is on trial: verifyRollbackLater() keeps the Arduino
// core from passing it at once, and update_poll() passes it only once it
// has run TRIAL_MS. If it fails to start, or is stopped by the watchdog
// before then, the bootloader goes back to the old one at the next start.
// A firmware written by cable is not on trial and is left alone.
//
// FROM GITHUB.COM
//
// update_from_github() takes the newest firmware without a cable or a
// telephone: firmware/wifitalkie.ino.bin as it is on the main branch of
// github.com/afarhan/wbitx, over https from raw.githubusercontent.com,
// checked against the certificates of the authorities ESP-IDF carries
// (esp_crt_bundle_attach), by ESP-IDF's esp_https_ota, which writes it to
// the slot not running and marks it to start next, on trial as above. It
// reads firmware/version.txt first, beside it, the time GitHub's was
// built (make release), and goes no further unless it is later than
// FIRMWARE_BUILT, the running one's (built.h, made by make): UP TO DATE.
// So a talkie flashed by cable with something newer than GitHub's is not
// taken back. The firmware's own description cannot tell: its date is the
// core library's, not the sketch's. The talkie has to be
// joined to a network; no code is asked, the file coming only from there.
// A firmware still on trial cannot be replaced until it has passed it, the
// first TRIAL_MS after starting.
//
// Writing the flash holds the loop up while the file comes in, some
// seconds: the watchdog is fed at each piece, and the speaker may click.
// The screen is not drawn by the loop then, so each piece that moves the
// percentage draws it, screen_update_shown(), and so do the end and a
// failure; the browser shows its own bar as it sends (portal.cpp).

#include <Arduino.h>
#include <Update.h>

#include "esp_app_desc.h"
#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_https_ota.h"
#include "esp_ota_ops.h"
#include "esp_random.h"
#include "esp_task_wdt.h"

#include "console.h"
#include "network.h"
#include "portal.h"
#include "screen.h"
#include "update.h"

#define CODE_LEAST 1000
#define CODE_SPAN 9000
#define RESTART_AFTER_MS 3000
#define TRIAL_MS 30000
#define GITHUB_URL "https://raw.githubusercontent.com/afarhan/wbitx/main/firmware/wifitalkie.ino.bin"
#define GITHUB_VERSION_URL "https://raw.githubusercontent.com/afarhan/wbitx/main/firmware/version.txt"
#define VERSION_ROOM 32
#define NOT_THERE (-2)

#if __has_include("built.h")
#include "built.h"
#else
#define FIRMWARE_BUILT 0LL
#endif
#define GITHUB_TIMEOUT_MS 15000
#define GITHUB_RX_ROOM 4096
#define GITHUB_TX_ROOM 1024

static int is_open;
static int code;
static size_t expected;
static int percent_said;
static int percent_shown;
static int is_done;
static long done_ms;
static int is_trial_over;
static char said[UPDATE_TEXT_ROOM];

static void progress(int read, int total);

// the Arduino core asks this at start: true, so that a new firmware stays
// on trial until update_poll() passes it
extern "C" bool verifyRollbackLater(void) {
	return true;
}

// the portal opening: a new code
void update_open(void) {
	code = CODE_LEAST + (int)(esp_random() % CODE_SPAN);
	is_open = 1;
	said[0] = 0;
	console_say("SETUP: THE FIRMWARE CODE IS %d", code);
}

// the portal closing: a file half sent is dropped
void update_close(void) {
	if (!is_open)
		return;
	if (Update.isRunning())
		update_abort("CLOSED");
	is_open = 0;
}

int update_is_open(void) {
	return is_open;
}

int update_code(void) {
	return code;
}

// how a sending goes, for the screen; empty before one
void update_said(char *text, int room) {
	snprintf(text, room, "%s", said);
}

int update_begin(size_t total) {
	if (!is_open || is_done)
		return 0;
	if (Update.isRunning())
		Update.abort();
	expected = total;
	percent_said = -1;
	percent_shown = 0;
	if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
		update_abort(Update.errorString());
		return 0;
	}
	snprintf(said, sizeof(said), "RECEIVING");
	console_say("UPDATE: RECEIVING %u BYTES", (unsigned)total);
	screen_update_shown();
	return 1;
}

int update_take(const uint8_t *data, size_t length) {
	if (!Update.isRunning())
		return 0;
	esp_task_wdt_reset();
	if (Update.write((uint8_t *)data, length) != length) {
		update_abort(Update.errorString());
		return 0;
	}
	progress((int)Update.progress(), (int)expected);
	return 1;
}

// 1 if the file was whole and is to start next
int update_end(void) {
	size_t written;

	if (!Update.isRunning())
		return 0;
	esp_task_wdt_reset();
	written = Update.progress();
	if (!Update.end(true)) {
		update_abort(Update.errorString());
		return 0;
	}
	is_done = 1;
	done_ms = (long)millis();
	percent_shown = 100;
	snprintf(said, sizeof(said), "DONE: RESTARTING");
	console_say("UPDATE: %u BYTES WRITTEN, RESTARTING", (unsigned)written);
	screen_update_shown();
	return 1;
}

void update_abort(const char *why) {
	if (Update.isRunning())
		Update.abort();
	snprintf(said, sizeof(said), "FAILED: %s", why);
	console_say("UPDATE FAILED: %s", why);
	screen_update_shown();
}

// how much of the file is written, 0 to 100, for the screen's bar
int update_percent(void) {
	return percent_shown;
}

void update_poll(long now) {
	const esp_partition_t *running;
	esp_ota_img_states_t state;

	if (is_done && now - done_ms >= RESTART_AFTER_MS) {
		portal_close();
		esp_restart();
	}
	if (is_trial_over || now < TRIAL_MS)
		return;
	is_trial_over = 1;
	running = esp_ota_get_running_partition();
	if (esp_ota_get_state_partition(running, &state) == ESP_OK && state == ESP_OTA_IMG_PENDING_VERIFY) {
		esp_ota_mark_app_valid_cancel_rollback();
		console_say("UPDATE: THE NEW FIRMWARE RAN %d S, KEPT", TRIAL_MS / 1000);
	}
}

// the time GitHub's firmware was built, from firmware/version.txt; -1 if
// github.com was not reached, NOT_THERE if it has no such file
static long long github_built(void) {
	esp_http_client_config_t http = {};
	esp_http_client_handle_t client;
	char text[VERSION_ROOM];
	int status;
	int got = -1;

	http.url = GITHUB_VERSION_URL;
	http.crt_bundle_attach = esp_crt_bundle_attach;
	http.timeout_ms = GITHUB_TIMEOUT_MS;
	http.buffer_size = GITHUB_RX_ROOM;
	http.buffer_size_tx = GITHUB_TX_ROOM;
	client = esp_http_client_init(&http);
	if (client == NULL)
		return -1;
	if (esp_http_client_open(client, 0) == ESP_OK) {
		esp_http_client_fetch_headers(client);
		status = esp_http_client_get_status_code(client);
		if (status == 200)
			got = esp_http_client_read(client, text, sizeof(text) - 1);
		else
			got = NOT_THERE;
		if (status != 200)
			console_say("UPDATE: GITHUB SAYS %d FOR version.txt", status);
	}
	esp_http_client_close(client);
	esp_http_client_cleanup(client);
	if (got == NOT_THERE)
		return NOT_THERE;
	if (got <= 0)
		return -1;
	text[got] = 0;
	return strtoll(text, NULL, 10);
}

// the percentage from bytes read of total, said and shown when it moves
static void progress(int read, int total) {
	int percent;

	if (total <= 0)
		return;
	percent = (int)((long long)read * 100 / total);
	if (percent > 99)
		percent = 99;
	if (percent / 10 != percent_said / 10) {
		percent_said = percent;
		console_say("UPDATE: %d%%", percent);
	}
	if (percent != percent_shown) {
		percent_shown = percent;
		snprintf(said, sizeof(said), "RECEIVING %d%%", percent);
		screen_update_shown();
	}
}

static void github_failed(const char *why) {
	snprintf(said, sizeof(said), "FAILED: %s", why);
	console_say("UPDATE FROM GITHUB FAILED: %s", why);
	screen_update_shown();
}

// the newest firmware from github.com, written and started into; it holds
// the loop up until it is done, failed, or found to be the one running
void update_from_github(void) {
	esp_http_client_config_t http = {};
	esp_https_ota_config_t ota = {};
	esp_https_ota_handle_t handle = NULL;
	esp_app_desc_t found;
	long long newest;
	esp_err_t err;
	int total;

	if (is_done || Update.isRunning())
		return;
	percent_said = -1;
	percent_shown = 0;
	if (!network_is_up()) {
		github_failed("NO WIFI");
		return;
	}
	snprintf(said, sizeof(said), "CHECKING");
	console_say("UPDATE: CHECKING %s", GITHUB_VERSION_URL);
	screen_update_shown();
	esp_task_wdt_reset();
	newest = github_built();
	esp_task_wdt_reset();
	if (newest == NOT_THERE) {
		github_failed("NONE ON GITHUB");
		return;
	}
	if (newest < 0) {
		github_failed("GITHUB NOT REACHED");
		return;
	}
	if (newest <= FIRMWARE_BUILT) {
		snprintf(said, sizeof(said), "UP TO DATE");
		console_say("UPDATE: GITHUB HAS %lld, RUNNING %lld: UP TO DATE", newest, FIRMWARE_BUILT);
		screen_update_shown();
		return;
	}
	console_say("UPDATE: GITHUB HAS %lld, RUNNING %lld: TAKING IT", newest, FIRMWARE_BUILT);
	http.url = GITHUB_URL;
	http.crt_bundle_attach = esp_crt_bundle_attach;
	http.timeout_ms = GITHUB_TIMEOUT_MS;
	http.buffer_size = GITHUB_RX_ROOM;
	http.buffer_size_tx = GITHUB_TX_ROOM;
	http.keep_alive_enable = true;
	ota.http_config = &http;
	esp_task_wdt_reset();
	err = esp_https_ota_begin(&ota, &handle);
	esp_task_wdt_reset();
	if (err == ESP_ERR_OTA_ROLLBACK_INVALID_STATE) {
		github_failed("ON TRIAL, TRY IN 30 S");
		return;
	}
	if (err != ESP_OK) {
		github_failed(esp_err_to_name(err));
		return;
	}
	err = esp_https_ota_get_img_desc(handle, &found);
	if (err != ESP_OK) {
		esp_https_ota_abort(handle);
		github_failed("NOT A FIRMWARE");
		return;
	}
	total = esp_https_ota_get_image_size(handle);
	console_say("UPDATE: RECEIVING %d BYTES", total);
	snprintf(said, sizeof(said), "RECEIVING 0%%");
	screen_update_shown();
	while ((err = esp_https_ota_perform(handle)) == ESP_ERR_HTTPS_OTA_IN_PROGRESS) {
		esp_task_wdt_reset();
		progress(esp_https_ota_get_image_len_read(handle), total);
	}
	if (err != ESP_OK || !esp_https_ota_is_complete_data_received(handle)) {
		esp_https_ota_abort(handle);
		github_failed(err != ESP_OK ? esp_err_to_name(err) : "CUT SHORT");
		return;
	}
	err = esp_https_ota_finish(handle);
	if (err != ESP_OK) {
		github_failed(err == ESP_ERR_OTA_VALIDATE_FAILED ? "NOT WHOLE" : esp_err_to_name(err));
		return;
	}
	is_done = 1;
	done_ms = (long)millis();
	percent_shown = 100;
	snprintf(said, sizeof(said), "DONE: RESTARTING");
	console_say("UPDATE: %d BYTES FROM GITHUB WRITTEN, RESTARTING", total);
	screen_update_shown();
}

// the screen leaving the GitHub page: what was said is forgotten, unless
// it is done and about to restart
void update_forget(void) {
	if (!is_done && !Update.isRunning())
		said[0] = 0;
}
