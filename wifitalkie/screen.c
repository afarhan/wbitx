// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ashhar Farhan
//
// screen.c — what the display shows and what the keys do.
//
// The display shows one page at a time. The home page, the talkie at rest:
//
//     VU2ESE         TX  o  ||||     the callsign; TX or RX; a dot, filled
//                                    when online at my.sbitx.net; the
//                                    WiFi's signal in four bars
//          21080                     the channel, in kHz, large
//     2 HERE                         how many are in the room, or, not
//                                    online, what the service is doing
//     VOL [#########     ] 50        the volume
//     HEARD VU3XYZ                   who speaks, or the last key pressed
//
// The bars: four at BARS_FOUR_DBM and above, then one fewer for each
// BARS_STEP_DB below it, none when the WiFi is not joined.
//
// On it ENTER is the PTT: the talkie speaks while it is held (ptt.c). UP
// and DOWN set the volume, VOLUME_STEP a press, repeating while held. MENU
// opens the menu.
//
// The menu is a list, UP and DOWN to choose, ENTER to open; each page it
// opens goes back to it with MENU, and the menu to the home page. MENU
// held goes home from anywhere, and so does MENU_IDLE_MS untouched:
//
//     FREQUENCY      the channel, large; UP and DOWN step it in dial.c's
//                    kilohertz steps, faster the longer they are held
//     VOLUME         the volume, large
//     CALLSIGN       entered a character at a time, below
//     MY.SBITX.NET   the code to add on the website, the service's state;
//                    ENTER asks for a new code, ENTER again makes it
//     WIFI           the networks heard, strongest first, with their
//                    signal, * by those remembered; ENTER joins one,
//                    with its key remembered, or entered if it is not;
//                    ENTER held enters its key afresh; the last item
//                    scans again. Then a page says whether it joined.
//     INFO           the network, the address, the signal, the build
//
// Text is entered on a page of its own, with entry.c: the text so far on
// top, and below it the characters to choose from, large, the chosen one
// in the middle. UP and DOWN scroll them, faster when held; ENTER adds the
// chosen to the text, and ENTER held takes the last character off, and the
// one before it, while it is held; MENU keeps the text, and MENU held
// leaves it as it was and goes home. The help at the bottom changes every
// HELP_EVERY_MS.
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
#include "entry.h"
#include "font.h"
#include "keys.h"
#include "known.h"
#include "network.h"
#include "ptt.h"
#include "service.h"
#include "settings.h"
#include "screen.h"
#include "voip.h"

#define DRAW_EVERY_MS 200
#define SAVE_AFTER_MS 2000
#define VOLUME_STEP 5
#define MENU_IDLE_MS 30000
#define TEXT_ROOM 64
#define COLUMNS 21
#define BIG_SCALE 3
#define BIG_ROW 2
#define TITLE_ROW 0
#define CALL_ROW 0
#define ROOM_ROW 5
#define VOLUME_ROW 6
#define LAST_ROW 7
#define LAMP_X 76
#define ONLINE_X 94
#define ONLINE_SIZE 7
#define BARS_X 106
#define BARS 4
#define BAR_STEP_X 5
#define BAR_THICK 3
#define BAR_RISE 2
#define BARS_FOUR_DBM (-55)
#define BARS_STEP_DB 10
#define BAR_X 24
#define BAR_WIDE 80
#define VOLUME_NUMBER_X 108
#define MENU_FIRST_ROW 1
#define MARK_X 0
#define ITEM_X 8
#define HZ_A_KHZ 1000
#define TEXT_ROW 2
#define CHOICE_ROW 4
#define CHOICE_SCALE 2
#define CHOICES_EACH_SIDE 4
#define CHOSEN_MARK_ROW 6
#define HELP_EVERY_MS 2000
#define LIST_ROWS 6
#define LIST_FIRST_ROW 1
#define NAME_COLUMNS 13
#define KNOWN_X 92
#define RSSI_X 104
#define JOIN_SHOWN_MS 20000

#define PAGE_HOME 0
#define PAGE_MENU 1
#define PAGE_DIAL 2
#define PAGE_VOLUME 3
#define PAGE_CALLSIGN 4
#define PAGE_SBITX 5
#define PAGE_NEW_CODE 6
#define PAGE_WIFI 7
#define PAGE_INFO 8
#define PAGE_WIFI_KEY 9
#define PAGE_WIFI_JOINING 10

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
static long touched_ms;
static int page = PAGE_HOME;
static int chosen;
static struct entry entry;

static const char *entry_help[] = {
	"ENTER: ADD",
	"HOLD ENTER: DELETE",
	"MENU: SAVE",
	"HOLD MENU: CANCEL",
};

#define ENTRY_HELPS ((int)(sizeof(entry_help) / sizeof(entry_help[0])))

static const char *wifi_help[] = {
	"ENTER: JOIN",
	"HOLD ENTER: NEW KEY",
	"*: REMEMBERED",
	"MENU: BACK",
};

#define WIFI_HELPS ((int)(sizeof(wifi_help) / sizeof(wifi_help[0])))

static int wifi_chosen;
static int wifi_top;
static char wifi_ssid[SETTINGS_SSID_ROOM];
static long joining_since_ms;

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

static int is_entering(int on_page) {
	return on_page == PAGE_CALLSIGN || on_page == PAGE_WIFI_KEY;
}

// to another page; ENTER is the PTT on the home page, the screen's on the
// rest
static void go(int to) {
	if (to == PAGE_CALLSIGN)
		entry_begin(&entry, ENTRY_CALLSIGN, settings.callsign, SETTINGS_CALLSIGN_ROOM - 1);
	if (to == PAGE_WIFI_KEY)
		entry_begin(&entry, ENTRY_ANY, "", SETTINGS_PASSWORD_ROOM - 1);
	if (to == PAGE_WIFI && page == PAGE_MENU) {
		network_scan_begin();
		wifi_chosen = 0;
		wifi_top = 0;
	}
	ptt_lend(to != PAGE_HOME);
	page = to;
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
		go(PAGE_MENU);
		chosen = 0;
		return;
	}
	if (is_up_or_down(press))
		volume_step(press, now);
}

static void take_menu(const struct key_press *press) {
	if (press->key == KEY_MENU)
		go(PAGE_HOME);
	if (press->key == KEY_UP)
		chosen = (chosen + ITEMS - 1) % ITEMS;
	if (press->key == KEY_DOWN)
		chosen = (chosen + 1) % ITEMS;
	if (press->key == KEY_ENTER)
		go(items[chosen].page);
}

// the network chosen, wifi_ssid, set and joined with this key
static void join(const char *password, long now) {
	snprintf(settings.ssid, sizeof(settings.ssid), "%s", wifi_ssid);
	snprintf(settings.password, sizeof(settings.password), "%s", password);
	settings_save();
	network_join();
	joining_since_ms = now;
	go(PAGE_WIFI_JOINING);
}

// a network chosen from the list: joined if it is open or remembered, its
// key asked for if not, or if is_new_key
static void pick(int i, int is_new_key, long now) {
	int rssi;
	int is_open;
	int known;

	network_scanned(i, wifi_ssid, sizeof(wifi_ssid), &rssi, &is_open);
	known = known_find(&settings, wifi_ssid);
	if (is_open)
		join("", now);
	else if (known >= 0 && !is_new_key)
		join(settings.known_password[known], now);
	else
		go(PAGE_WIFI_KEY);
}

static void take_wifi(const struct key_press *press, long now) {
	int count = network_scanned_count();
	int choices = count + 1;

	if (press->key == KEY_MENU) {
		go(PAGE_MENU);
		return;
	}
	if (network_is_scanning())
		return;
	if (press->key == KEY_UP)
		wifi_chosen = (wifi_chosen + choices - 1) % choices;
	if (press->key == KEY_DOWN)
		wifi_chosen = (wifi_chosen + 1) % choices;
	if (wifi_chosen < wifi_top)
		wifi_top = wifi_chosen;
	if (wifi_chosen >= wifi_top + LIST_ROWS)
		wifi_top = wifi_chosen - LIST_ROWS + 1;
	if (press->key != KEY_ENTER || press->what == BUTTON_REPEATED)
		return;
	if (wifi_chosen == count) {
		network_scan_begin();
		wifi_chosen = 0;
		wifi_top = 0;
		return;
	}
	pick(wifi_chosen, press->what == BUTTON_HELD, now);
}

// MENU pressed on a page of text entry: the text is kept
static void accept(long now) {
	if (page == PAGE_WIFI_KEY) {
		join(entry.text, now);
		return;
	}
	if (page == PAGE_CALLSIGN && entry.length > 0) {
		snprintf(settings.callsign, sizeof(settings.callsign), "%s", entry.text);
		settings_save();
		service_restart();
		console_say("CALL %s", settings.callsign);
	}
	go(PAGE_MENU);
}

static void take_entry(const struct key_press *press, long now) {
	if (is_up_or_down(press))
		entry_scroll(&entry, press->key == KEY_UP);
	if (press->key == KEY_ENTER && press->what == BUTTON_PRESSED)
		entry_add(&entry);
	else if (press->key == KEY_ENTER)
		entry_delete(&entry);
	if (press->key == KEY_MENU)
		accept(now);
}

static void take_page(const struct key_press *press, long now) {
	if (page == PAGE_NEW_CODE) {
		if (press->key == KEY_ENTER) {
			service_new_key();
			console_say("NEW CODE MADE; ADD IT AT MY.SBITX.NET IN PLACE OF THE OLD");
		}
		if (press->key == KEY_ENTER || press->key == KEY_MENU)
			go(PAGE_SBITX);
		return;
	}
	if (page == PAGE_SBITX && press->key == KEY_ENTER) {
		go(PAGE_NEW_CODE);
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
		go(PAGE_MENU);
}

static void take(const struct key_press *press, long now) {
	say_key(press);
	touched_ms = now;
	if (press->key == KEY_MENU && press->what == BUTTON_HELD) {
		go(PAGE_HOME);
		return;
	}
	if (is_entering(page)) {
		take_entry(press, now);
		return;
	}
	if (page == PAGE_WIFI) {
		take_wifi(press, now);
		return;
	}
	if (press->key == KEY_ENTER && press->what != BUTTON_PRESSED)
		return;
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

// a dot, filled when online at my.sbitx.net, a ring when not
static void online_mark(void) {
	int edge = ONLINE_SIZE - 1;

	if (service_is_online()) {
		display_fill(ONLINE_X + 1, 0, ONLINE_SIZE - 2, ONLINE_SIZE);
		display_fill(ONLINE_X, 1, ONLINE_SIZE, ONLINE_SIZE - 2);
		return;
	}
	display_fill(ONLINE_X + 1, 0, ONLINE_SIZE - 2, 1);
	display_fill(ONLINE_X + 1, edge, ONLINE_SIZE - 2, 1);
	display_fill(ONLINE_X, 1, 1, ONLINE_SIZE - 2);
	display_fill(ONLINE_X + edge, 1, 1, ONLINE_SIZE - 2);
}

// the WiFi's signal, bars rising left to right; those not earned a line
static void signal_bars(void) {
	int earned = 0;
	int rssi = network_rssi();
	int high;
	int i;

	if (network_is_up()) {
		earned = BARS - (BARS_FOUR_DBM - rssi + BARS_STEP_DB - 1) / BARS_STEP_DB;
		if (rssi >= BARS_FOUR_DBM)
			earned = BARS;
		if (earned < 0)
			earned = 0;
	}
	for (i = 0; i < BARS; i++) {
		high = 1;
		if (i < earned)
			high = (i + 1) * BAR_RISE;
		display_fill(BARS_X + i * BAR_STEP_X, DISPLAY_ROW_HIGH - high, BAR_THICK, high);
	}
}

// how many are in the room, or why not online
static void room_line(void) {
	char text[TEXT_ROOM];
	char why[TEXT_ROOM];

	if (!service_is_online()) {
		service_describe(text, sizeof(text));
		line(ROOM_ROW, text);
		return;
	}
	if (voip_busy(why, sizeof(why))) {
		snprintf(text, sizeof(text), "BUSY %s", why);
		line(ROOM_ROW, text);
		return;
	}
	if (voip_here() >= 0) {
		snprintf(text, sizeof(text), "%d HERE", voip_here());
		line(ROOM_ROW, text);
	}
}

static void draw_home(void) {
	char text[TEXT_ROOM];
	char heard[TEXT_ROOM];

	line(CALL_ROW, settings.callsign);
	if (ptt_is_lit() && ptt_is_down())
		display_text(LAMP_X, CALL_ROW, 1, "TX");
	else if (ptt_is_lit())
		display_text(LAMP_X, CALL_ROW, 1, "RX");
	online_mark();
	signal_bars();
	big_frequency();
	room_line();
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

// the text so far, its end if it is longer than the screen, and a cursor
static void draw_entry(const char *title, long now) {
	char text[COLUMNS + 1];
	char one[2] = "";
	int x = (DISPLAY_WIDE - (2 * CHOICES_EACH_SIDE + 1) * FONT_STEP * CHOICE_SCALE) / 2;
	int chosen_x = x + CHOICES_EACH_SIDE * FONT_STEP * CHOICE_SCALE;
	int shown = entry.length;
	int i;

	line(TITLE_ROW, title);
	if (shown > COLUMNS - 1)
		shown = COLUMNS - 1;
	snprintf(text, sizeof(text), "%s_", entry.text + entry.length - shown);
	line(TEXT_ROW, text);
	for (i = -CHOICES_EACH_SIDE; i <= CHOICES_EACH_SIDE; i++) {
		one[0] = entry_around(&entry, i);
		one[1] = 0;
		display_text(x, CHOICE_ROW, CHOICE_SCALE, one);
		x += FONT_STEP * CHOICE_SCALE;
	}
	if (entry_around(&entry, 0) == ' ')
		display_text(chosen_x - FONT_STEP * 2, CHOSEN_MARK_ROW, 1, "SPACE");
	else
		display_text(chosen_x + FONT_STEP / 2, CHOSEN_MARK_ROW, 1, "^");
	line(LAST_ROW, entry_help[now / HELP_EVERY_MS % ENTRY_HELPS]);
}

static void draw_wifi(long now) {
	char name[NAME_COLUMNS + 1];
	char ssid[SETTINGS_SSID_ROOM];
	char text[TEXT_ROOM];
	int count = network_scanned_count();
	int row = LIST_FIRST_ROW;
	int rssi;
	int is_open;
	int i;

	line(TITLE_ROW, "WIFI");
	if (network_is_scanning()) {
		line(3, "LISTENING FOR");
		line(4, "NETWORKS...");
		return;
	}
	for (i = wifi_top; i <= count && row < LIST_FIRST_ROW + LIST_ROWS; i++, row++) {
		if (i == wifi_chosen)
			display_text(MARK_X, row, 1, ">");
		if (i == count) {
			display_text(ITEM_X, row, 1, "SCAN AGAIN");
			continue;
		}
		network_scanned(i, ssid, sizeof(ssid), &rssi, &is_open);
		snprintf(name, sizeof(name), "%s", ssid);
		display_text(ITEM_X, row, 1, name);
		if (known_find(&settings, ssid) >= 0)
			display_text(KNOWN_X, row, 1, "*");
		snprintf(text, sizeof(text), "%d", rssi);
		display_text(RSSI_X, row, 1, text);
	}
	line(LAST_ROW, wifi_help[now / HELP_EVERY_MS % WIFI_HELPS]);
}

static void draw_joining(long now) {
	char text[TEXT_ROOM];

	line(TITLE_ROW, "WIFI");
	line(1, wifi_ssid);
	network_ssid(text, sizeof(text));
	if (network_is_up() && !strcmp(text, wifi_ssid)) {
		line(3, "JOINED");
		network_address(text, sizeof(text));
		line(4, text);
	} else if (now - joining_since_ms < JOIN_SHOWN_MS) {
		line(3, "JOINING...");
	} else {
		line(3, "NOT JOINED.");
		line(4, "IS THE KEY RIGHT?");
	}
	line(LAST_ROW, "MENU: BACK");
}

static void draw(long now) {
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
	if (page == PAGE_CALLSIGN)
		draw_entry("CALLSIGN", now);
	if (page == PAGE_WIFI_KEY)
		draw_entry(wifi_ssid, now);
	if (page == PAGE_WIFI)
		draw_wifi(now);
	if (page == PAGE_WIFI_JOINING)
		draw_joining(now);
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
	if (page != PAGE_HOME && now - touched_ms >= MENU_IDLE_MS)
		go(PAGE_HOME);
	if (is_save_due && now >= save_ms) {
		is_save_due = 0;
		settings_save();
	}
	if (now - drawn_ms >= DRAW_EVERY_MS) {
		drawn_ms = now;
		draw(now);
	}
	display_poll();
}
