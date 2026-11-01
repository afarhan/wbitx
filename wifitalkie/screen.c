// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ashhar Farhan
//
// screen.c — what the display shows and what the keys do.
//
// The display shows one page at a time. The home page, the talkie at rest:
//
//     VU2ESE                 TX      the callsign; TX or RX, the old lamp
//     ONLINE VU2ESE-WIFITAL          the service
//        < 21080 >                   the channel, in kHz, large; the arrows
//                                    while UP and DOWN step it
//     21080 KHZ, 2 HERE              the room
//     VOL [#########     ] 50        the volume
//     HEARD VU3XYZ                   who speaks, or the last key pressed
//
// On it UP and DOWN set the volume, VOLUME_STEP a press, repeating while
// held. ENTER gives them to the channel instead, dial.c's kilohertz steps,
// faster the longer they are held; ENTER again, or DIAL_IDLE_MS untouched,
// gives them back to the volume. MENU opens the menu.
//
// The menu is a list, UP and DOWN to choose, ENTER to open; each page it
// opens goes back to it with MENU, and the menu to the home page. MENU
// held goes home from anywhere, and so does MENU_IDLE_MS untouched:
//
//     FREQUENCY      the channel, large, UP and DOWN as on the home page
//     VOLUME         the volume, large
//     CALLSIGN       to come: entered a letter at a time
//     MY.SBITX.NET   the code to add on the website, the service's state;
//                    ENTER asks for a new code, ENTER again makes it
//     WIFI           to come: the networks heard, and their keys
//     INFO           the network, the address, the signal, the build
//
// Volume and channel are written to the settings SAVE_AFTER_MS after the
// last press, not at each: a flash write holds the interrupts up; voip.c
// follows settings.hz at once, and tells my.sbitx.net of the new room.
// Every press is also said on the console, KEY UP, KEY MENU HELD and so
// on, for trying the keys.
//
// The page is drawn again every DRAW_EVERY_MS; display.c sends only what
// changed, a page of the display a turn.

#include <stdio.h>
#include <string.h>

#include "auth.h"
#include "console.h"
#include "dial.h"
#include "display.h"
#include "font.h"
#include "keys.h"
#include "network.h"
#include "ptt.h"
#include "service.h"
#include "settings.h"
#include "screen.h"
#include "voip.h"

#define DRAW_EVERY_MS 200
#define SAVE_AFTER_MS 2000
#define VOLUME_STEP 5
#define DIAL_IDLE_MS 5000
#define MENU_IDLE_MS 30000
#define TEXT_ROOM 64
#define COLUMNS 21
#define BIG_SCALE 3
#define BIG_ROW 2
#define ARROW_ROW 3
#define ARROW_RIGHT_X 122
#define TITLE_ROW 0
#define CALL_ROW 0
#define SERVICE_ROW 1
#define ROOM_ROW 5
#define VOLUME_ROW 6
#define LAST_ROW 7
#define LAMP_X 116
#define BAR_X 24
#define BAR_WIDE 80
#define VOLUME_NUMBER_X 108
#define MENU_FIRST_ROW 1
#define MARK_X 0
#define ITEM_X 8
#define HZ_A_KHZ 1000

#define PAGE_HOME 0
#define PAGE_MENU 1
#define PAGE_DIAL 2
#define PAGE_VOLUME 3
#define PAGE_CALLSIGN 4
#define PAGE_SBITX 5
#define PAGE_NEW_CODE 6
#define PAGE_WIFI 7
#define PAGE_INFO 8

struct item {
	const char *name;
	int page;
};

static const struct item items[] = {
	{"FREQUENCY", PAGE_DIAL},
	{"VOLUME", PAGE_VOLUME},
	{"CALLSIGN", PAGE_CALLSIGN},
	{"MY.SBITX.NET", PAGE_SBITX},
	{"WIFI", PAGE_WIFI},
	{"INFO", PAGE_INFO},
};

#define ITEMS ((int)(sizeof(items) / sizeof(items[0])))

static long drawn_ms;
static long save_ms;
static int is_save_due;
static char last_key[TEXT_ROOM];
static int is_dialing;
static long touched_ms;
static int page = PAGE_HOME;
static int chosen;

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

static void save_soon(long now) {
	is_save_due = 1;
	save_ms = now + SAVE_AFTER_MS;
}

static void volume_step(const struct key_press *press, long now) {
	int volume = (int)settings.volume;

	if (press->key == KEY_UP)
		volume += VOLUME_STEP;
	else
		volume -= VOLUME_STEP;
	if (volume < 0)
		volume = 0;
	if (volume > SETTINGS_LEVEL_MOST)
		volume = SETTINGS_LEVEL_MOST;
	settings.volume = (uint32_t)volume;
	save_soon(now);
}

static void dial_turn(const struct key_press *press, long now) {
	settings.hz = dial_step(settings.hz, press->key == KEY_UP, press->held_ms);
	save_soon(now);
}

static int is_up_or_down(const struct key_press *press) {
	return press->key == KEY_UP || press->key == KEY_DOWN;
}

static void take_home(const struct key_press *press, long now) {
	if (press->key == KEY_MENU) {
		page = PAGE_MENU;
		chosen = 0;
		is_dialing = 0;
		return;
	}
	if (press->key == KEY_ENTER)
		is_dialing = !is_dialing;
	if (!is_up_or_down(press))
		return;
	if (is_dialing)
		dial_turn(press, now);
	else
		volume_step(press, now);
}

static void take_menu(const struct key_press *press) {
	if (press->key == KEY_MENU)
		page = PAGE_HOME;
	if (press->key == KEY_UP)
		chosen = (chosen + ITEMS - 1) % ITEMS;
	if (press->key == KEY_DOWN)
		chosen = (chosen + 1) % ITEMS;
	if (press->key == KEY_ENTER)
		page = items[chosen].page;
}

static void take_page(const struct key_press *press, long now) {
	if (page == PAGE_NEW_CODE) {
		if (press->key == KEY_ENTER) {
			service_new_key();
			console_say("NEW CODE MADE; ADD IT AT MY.SBITX.NET IN PLACE OF THE OLD");
		}
		if (press->key == KEY_ENTER || press->key == KEY_MENU)
			page = PAGE_SBITX;
		return;
	}
	if (page == PAGE_SBITX && press->key == KEY_ENTER) {
		page = PAGE_NEW_CODE;
		return;
	}
	if (page == PAGE_DIAL && is_up_or_down(press)) {
		dial_turn(press, now);
		return;
	}
	if (page == PAGE_VOLUME && is_up_or_down(press)) {
		volume_step(press, now);
		return;
	}
	if (press->key == KEY_MENU || press->key == KEY_ENTER)
		page = PAGE_MENU;
}

static void take(const struct key_press *press, long now) {
	say_key(press);
	touched_ms = now;
	if (press->key == KEY_MENU && press->what == BUTTON_HELD) {
		page = PAGE_HOME;
		is_dialing = 0;
		return;
	}
	if (page == PAGE_HOME)
		take_home(press, now);
	else if (page == PAGE_MENU)
		take_menu(press);
	else
		take_page(press, now);
}

// text cut to the screen's width
static void line(int row, const char *text) {
	char cut[COLUMNS + 1];

	snprintf(cut, sizeof(cut), "%s", text);
	display_text(0, row, 1, cut);
}

// text large and in the middle of the screen
static void big(const char *text) {
	int wide = (int)strlen(text) * FONT_STEP * BIG_SCALE;

	display_text((DISPLAY_WIDE - wide) / 2, BIG_ROW, BIG_SCALE, text);
}

static void big_frequency(void) {
	char text[TEXT_ROOM];

	snprintf(text, sizeof(text), "%u", (unsigned)(settings.hz / HZ_A_KHZ));
	big(text);
}

static void volume_bar(void) {
	char text[TEXT_ROOM];

	display_text(0, VOLUME_ROW, 1, "VOL");
	display_bar(BAR_X, VOLUME_ROW, BAR_WIDE, (int)settings.volume * BAR_WIDE / SETTINGS_LEVEL_MOST);
	snprintf(text, sizeof(text), "%u", (unsigned)settings.volume);
	display_text(VOLUME_NUMBER_X, VOLUME_ROW, 1, text);
}

static void draw_home(void) {
	char text[TEXT_ROOM];
	char heard[TEXT_ROOM];

	line(CALL_ROW, settings.callsign);
	if (ptt_is_lit() && ptt_is_down())
		display_text(LAMP_X, CALL_ROW, 1, "TX");
	else if (ptt_is_lit())
		display_text(LAMP_X, CALL_ROW, 1, "RX");
	service_describe(text, sizeof(text));
	line(SERVICE_ROW, text);
	big_frequency();
	if (is_dialing) {
		display_text(0, ARROW_ROW, 1, "<");
		display_text(ARROW_RIGHT_X, ARROW_ROW, 1, ">");
	}
	voip_describe(text, sizeof(text));
	line(ROOM_ROW, text);
	volume_bar();
	if (voip_hearing(heard, sizeof(heard))) {
		snprintf(text, sizeof(text), "HEARD %s", heard);
		line(LAST_ROW, text);
	} else {
		line(LAST_ROW, last_key);
	}
}

static void draw_menu(void) {
	int i;

	line(TITLE_ROW, "MENU");
	for (i = 0; i < ITEMS; i++) {
		if (i == chosen)
			display_text(MARK_X, MENU_FIRST_ROW + i, 1, ">");
		display_text(ITEM_X, MENU_FIRST_ROW + i, 1, items[i].name);
	}
}

static void draw_sbitx(void) {
	char text[TEXT_ROOM];
	char shown[AUTH_COOKIE_SHOWN_ROOM];

	line(0, "MY.SBITX.NET");
	service_describe(text, sizeof(text));
	line(1, text);
	line(3, "ADD THIS CODE THERE:");
	auth_cookie_show(settings.cookie, shown);
	line(4, shown);
	line(6, "ENTER: A NEW CODE");
	line(7, "MENU: BACK");
}

static void draw_new_code(void) {
	line(0, "MAKE A NEW CODE?");
	line(2, "THE OLD ONE WILL NO");
	line(3, "LONGER LET THIS");
	line(4, "TALKIE IN.");
	line(6, "ENTER: YES");
	line(7, "MENU: NO");
}

static void draw_info(void) {
	char text[TEXT_ROOM];
	char address[TEXT_ROOM];

	line(0, "INFO");
	snprintf(text, sizeof(text), "WIFI %s", settings.ssid);
	line(1, text);
	network_address(address, sizeof(address));
	if (address[0] == 0)
		snprintf(address, sizeof(address), "NOT JOINED");
	line(2, address);
	if (network_is_up()) {
		snprintf(text, sizeof(text), "SIGNAL %d DBM", network_rssi());
		line(3, text);
	}
	snprintf(text, sizeof(text), "CALL %s", settings.callsign);
	line(4, text);
	line(6, "BUILT " __DATE__);
	line(7, "MENU: BACK");
}

static void draw(void) {
	display_clear();
	if (page == PAGE_HOME)
		draw_home();
	if (page == PAGE_MENU)
		draw_menu();
	if (page == PAGE_DIAL) {
		line(TITLE_ROW, "FREQUENCY, KHZ");
		big_frequency();
		line(LAST_ROW, "UP/DOWN, MENU: BACK");
	}
	if (page == PAGE_VOLUME) {
		line(TITLE_ROW, "VOLUME");
		volume_bar();
		line(LAST_ROW, "UP/DOWN, MENU: BACK");
	}
	if (page == PAGE_CALLSIGN || page == PAGE_WIFI) {
		line(TITLE_ROW, items[chosen].name);
		line(3, "TO COME");
		line(LAST_ROW, "MENU: BACK");
	}
	if (page == PAGE_SBITX)
		draw_sbitx();
	if (page == PAGE_NEW_CODE)
		draw_new_code();
	if (page == PAGE_INFO)
		draw_info();
}

void screen_poll(long now) {
	struct key_press press;

	if (keys_poll(now, &press))
		take(&press, now);
	if (is_dialing && now - touched_ms >= DIAL_IDLE_MS)
		is_dialing = 0;
	if (page != PAGE_HOME && now - touched_ms >= MENU_IDLE_MS)
		page = PAGE_HOME;
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
