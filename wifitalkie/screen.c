// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ashhar Farhan
//
// screen.c — what the display shows and what the keys do.
//
// The main screen, for now the only one:
//
//     VU2ESE                 TX      the callsign; TX or RX, the old lamp
//     ONLINE VU2ESE-WIFITAL          the service
//          21080                     the channel, in kHz, large
//     21080 KHZ, 2 HERE              the room
//     VOL [#########     ] 50        the volume
//     HEARD VU3XYZ                   who speaks, or the last key pressed
//
// UP and DOWN set the volume, VOLUME_STEP a press, repeating while held.
// It is written to the settings SAVE_AFTER_MS after the last press, not at
// each: a flash write holds the interrupts up. Every press is also said on
// the console, KEY UP, KEY MENU HELD and so on, for trying the keys. The
// channel, MENU and ENTER are still to come.
//
// The screen is drawn again every DRAW_EVERY_MS; display.c sends only what
// changed, a page a turn.

#include <stdio.h>
#include <string.h>

#include "console.h"
#include "display.h"
#include "font.h"
#include "keys.h"
#include "ptt.h"
#include "service.h"
#include "settings.h"
#include "screen.h"
#include "voip.h"

#define DRAW_EVERY_MS 200
#define SAVE_AFTER_MS 2000
#define VOLUME_STEP 5
#define TEXT_ROOM 64
#define COLUMNS 21
#define BIG_SCALE 3
#define BIG_ROW 2
#define CALL_ROW 0
#define SERVICE_ROW 1
#define ROOM_ROW 5
#define VOLUME_ROW 6
#define LAST_ROW 7
#define LAMP_X 116
#define BAR_X 24
#define BAR_WIDE 80
#define VOLUME_NUMBER_X 108
#define HZ_A_KHZ 1000

static long drawn_ms;
static long save_ms;
static int is_save_due;
static char last_key[TEXT_ROOM];

void screen_open(void) {
	keys_open();
	display_open();
}

static void say_key(const struct key_press *press) {
	const char *how = "";

	if (press->what == BUTTON_REPEATED)
		how = " AGAIN";
	if (press->what == BUTTON_HELD)
		how = " HELD";
	snprintf(last_key, sizeof(last_key), "KEY %s%s", keys_name(press->key), how);
	console_say("%s", last_key);
}

static void volume_step(int by, long now) {
	int volume = (int)settings.volume + by;

	if (volume < 0)
		volume = 0;
	if (volume > SETTINGS_LEVEL_MOST)
		volume = SETTINGS_LEVEL_MOST;
	settings.volume = (uint32_t)volume;
	is_save_due = 1;
	save_ms = now + SAVE_AFTER_MS;
}

static void take(const struct key_press *press, long now) {
	say_key(press);
	if (press->key == KEY_UP)
		volume_step(VOLUME_STEP, now);
	if (press->key == KEY_DOWN)
		volume_step(-VOLUME_STEP, now);
}

// text cut to the screen's width
static void line(int row, const char *text) {
	char cut[COLUMNS + 1];

	snprintf(cut, sizeof(cut), "%s", text);
	display_text(0, row, 1, cut);
}

static void draw(void) {
	char text[TEXT_ROOM];
	char heard[TEXT_ROOM];
	int wide;

	display_clear();
	line(CALL_ROW, settings.callsign);
	if (ptt_is_lit() && ptt_is_down())
		display_text(LAMP_X, CALL_ROW, 1, "TX");
	else if (ptt_is_lit())
		display_text(LAMP_X, CALL_ROW, 1, "RX");
	service_describe(text, sizeof(text));
	line(SERVICE_ROW, text);
	snprintf(text, sizeof(text), "%u", (unsigned)(settings.hz / HZ_A_KHZ));
	wide = (int)strlen(text) * FONT_STEP * BIG_SCALE;
	display_text((DISPLAY_WIDE - wide) / 2, BIG_ROW, BIG_SCALE, text);
	voip_describe(text, sizeof(text));
	line(ROOM_ROW, text);
	display_text(0, VOLUME_ROW, 1, "VOL");
	display_bar(BAR_X, VOLUME_ROW, BAR_WIDE, (int)settings.volume * BAR_WIDE / SETTINGS_LEVEL_MOST);
	snprintf(text, sizeof(text), "%u", (unsigned)settings.volume);
	display_text(VOLUME_NUMBER_X, VOLUME_ROW, 1, text);
	if (voip_hearing(heard, sizeof(heard))) {
		snprintf(text, sizeof(text), "HEARD %s", heard);
		line(LAST_ROW, text);
	} else {
		line(LAST_ROW, last_key);
	}
}

void screen_poll(long now) {
	struct key_press press;

	if (keys_poll(now, &press))
		take(&press, now);
	if (is_save_due && now >= save_ms) {
		is_save_due = 0;
		settings_save();
	}
	if (now - drawn_ms >= DRAW_EVERY_MS) {
		drawn_ms = now;
		draw();
	}
	display_poll();
}
