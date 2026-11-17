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
// Writing the flash holds the loop up while the file comes in, some
// seconds: the watchdog is fed at each piece, and the speaker may click.
// The screen is not drawn by the loop then, so each piece that moves the
// percentage draws it, screen_update_shown(), and so do the end and a
// failure; the browser shows its own bar as it sends (portal.cpp).

#include <Arduino.h>
#include <Update.h>

#include "esp_ota_ops.h"
#include "esp_random.h"
#include "esp_task_wdt.h"

#include "console.h"
#include "portal.h"
#include "screen.h"
#include "update.h"

#define CODE_LEAST 1000
#define CODE_SPAN 9000
#define RESTART_AFTER_MS 3000
#define TRIAL_MS 30000

static int is_open;
static int code;
static size_t expected;
static int percent_said;
static int percent_shown;
static int is_done;
static long done_ms;
static int is_trial_over;
static char said[UPDATE_TEXT_ROOM];

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
	int percent;

	if (!Update.isRunning())
		return 0;
	esp_task_wdt_reset();
	if (Update.write((uint8_t *)data, length) != length) {
		update_abort(Update.errorString());
		return 0;
	}
	if (expected > 0) {
		percent = (int)(Update.progress() * 100 / expected);
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
