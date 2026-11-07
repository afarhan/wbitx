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
//     MORSE WPM      the Morse speed for entering text, large; UP and
//                    DOWN step it, from HALVES_WPM_LEAST to HALVES_WPM_MOST
//     CALLSIGN       entered a character at a time, below
//     MY.SBITX.NET   the code to add on the website, the service's state;
//                    ENTER asks for a new code, ENTER again makes it
//     WIFI           the networks heard, strongest first, with their
//                    signal, * by those remembered; ENTER joins one,
//                    with its key remembered, or entered if it is not;
//                    ENTER held enters its key afresh; the last item
//                    scans again. Then a page says whether it joined.
//     INFO           the network, the address, the signal, the build
//     SLEEP          the talkie asleep, until any key wakes it (sleep.c)
//
// Text, such as a callsign, is entered on a page of its own:
//
//     Enter your callsign:      the prompt, a line
//     VU2ES_                    the text so far, up to ENTRY_ROOM - 1,
//                               its end on two lines, and a cursor
//     HSVIFU?ELR.APWJ           halves.c's two lines of letters, LINE_GAP
//                               dots apart, the two offered in reverse
//     BDXNCKYTZGQM/O-           video, the chosen framed; or its line of
//                               figures in their place
//     PAUSE: TAKE LETTER        the help, changing every HELP_EVERY_MS
//
// UP and DOWN key a letter as its Morse, UP a dit and DOWN a dah, one
// element a press (halves.c); DOWN held brings the figures. The gap between
// letters, at settings.wpm, adds the chosen to the text. ENTER pressed while a letter is being
// chosen drops it; with none, it keeps the text and goes back. ENTER held
// takes the last character off, and the one before it, while it is held.
// MENU goes back and leaves the text as it was.
//
// A WiFi key is entered as before, until the captive portal takes it over:
// all of printable ASCII, large, the chosen in the middle; UP and DOWN
// scroll them, faster when held, ENTER adds the chosen, ENTER held takes
// characters off, and MENU keeps the key and joins.
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
#include "halves.h"
#include "keys.h"
#include "known.h"
#include "microphone.h"
#include "pins.h"
#include "network.h"
#include "ptt.h"
#include "service.h"
#include "settings.h"
#include "screen.h"
#include "sleep.h"
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
#define PROMPT_ROWS 1
#define TEXT_ROW 1
#define TEXT_ROWS 2
#define LETTERS_Y 30
#define LINE_GAP 14
#define FIGURES_Y (LETTERS_Y + LINE_GAP / 2)
#define LETTER_PITCH 8
#define LETTER_EDGE 2
#define WPM_STEP 1
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
#define PAGE_AUDIO 11
#define PAGE_WPM 12
#define PAGE_SLEEP 13

struct item {
	const char *name;
	int page;
};

static const struct item items[] = {
	{"FREQUENCY", PAGE_DIAL},
	{"VOLUME", PAGE_VOLUME},
	{"AUDIO", PAGE_AUDIO},
	{"MORSE WPM", PAGE_WPM},
	{"CALLSIGN", PAGE_CALLSIGN},
	{"MY.SBITX.NET", PAGE_SBITX},
	{"WIFI", PAGE_WIFI},
	{"INFO", PAGE_INFO},
	{"SLEEP", PAGE_SLEEP},
};

#define ITEMS ((int)(sizeof(items) / sizeof(items[0])))
#define ITEM_COLUMNS ((DISPLAY_WIDE - ITEM_X) / FONT_STEP)
#define MENU_ROWS (DISPLAY_ROWS - MENU_FIRST_ROW)

// A page of choices, the pattern for a setting with a few values: the mark
// begins on the value in use, UP and DOWN move it, ENTER takes the value
// marked and goes back to the menu, MENU goes back with nothing changed.
struct choices {
	int page;
	const char *title;
	const char *const *names;
	int count;
	int (*in_use)(void);
	void (*take)(int chosen);
};

static const struct choices *choices_of(int on_page);
static int marked;

static long drawn_ms;
static long save_ms;
static int is_save_due;
static char last_key[TEXT_ROOM];
static long touched_ms;
static int page = PAGE_HOME;
static int chosen;
static int menu_top;
static struct entry entry;
static struct halves halves;

static const char *entry_help[] = {
	"UP: DIT  DOWN: DAH",
	"HOLD DOWN: FIGURES",
	"PAUSE: TAKE LETTER",
	"ENTER: DONE",
	"HOLD ENTER: DELETE",
	"MENU: CANCEL",
};

#define ENTRY_HELPS ((int)(sizeof(entry_help) / sizeof(entry_help[0])))

static const char *key_help[] = {
	"ENTER: ADD",
	"HOLD ENTER: DELETE",
	"MENU: SAVE",
	"HOLD MENU: CANCEL",
};

#define KEY_HELPS ((int)(sizeof(key_help) / sizeof(key_help[0])))

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
	if (to == PAGE_CALLSIGN) {
		entry_begin(&entry, ENTRY_CALLSIGN, settings.callsign, SETTINGS_CALLSIGN_ROOM - 1);
		halves_begin(&halves, (int)settings.wpm);
	}
	if (to == PAGE_WIFI_KEY)
		entry_begin(&entry, ENTRY_ANY, "", SETTINGS_PASSWORD_ROOM - 1);
	if (to == PAGE_WIFI && page == PAGE_MENU) {
		network_scan_begin();
		wifi_chosen = 0;
		wifi_top = 0;
	}
	if (choices_of(to) != NULL)
		marked = choices_of(to)->in_use();
	ptt_lend(to != PAGE_HOME);
	page = to;
}

static void wpm_step(const struct key_press *press, long now) {
	int wpm = (int)settings.wpm + (press->key == KEY_UP ? WPM_STEP : -WPM_STEP);

	if (wpm < HALVES_WPM_LEAST)
		wpm = HALVES_WPM_LEAST;
	if (wpm > HALVES_WPM_MOST)
		wpm = HALVES_WPM_MOST;
	settings.wpm = (uint32_t)wpm;
	save_soon(now);
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

// AUDIO: the in-built mic or the handsfree's, kept across power-ups
static const char *const audio_names[] = {"IN-BUILT", "HANDSFREE"};

static int audio_in_use(void) {
	return settings.is_headset ? 1 : 0;
}

static void audio_take(int chosen_one) {
	if ((uint32_t)chosen_one == settings.is_headset)
		return;
	settings.is_headset = (uint32_t)chosen_one;
	settings_save();
	microphone_pin(settings.is_headset ? PIN_HEADSET_MIC : PIN_MICROPHONE);
}

static const struct choices all_choices[] = {
	{PAGE_AUDIO, "AUDIO", audio_names, 2, audio_in_use, audio_take},
};

#define ALL_CHOICES ((int)(sizeof(all_choices) / sizeof(all_choices[0])))

static const struct choices *choices_of(int on_page) {
	int i;

	for (i = 0; i < ALL_CHOICES; i++) {
		if (all_choices[i].page == on_page)
			return &all_choices[i];
	}
	return NULL;
}

static void take_choices(const struct choices *these, const struct key_press *press) {
	if (press->key == KEY_UP)
		marked = (marked + these->count - 1) % these->count;
	if (press->key == KEY_DOWN)
		marked = (marked + 1) % these->count;
	if (press->key == KEY_ENTER)
		these->take(marked);
	if (press->key == KEY_ENTER || press->key == KEY_MENU)
		go(PAGE_MENU);
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
	if (press->key == KEY_ENTER && items[chosen].page == PAGE_SLEEP) {
		if (is_save_due) {
			is_save_due = 0;
			settings_save();
		}
		go(PAGE_HOME);
		sleep_now();
		return;
	}
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

// ENTER pressed on a page of text entry, no letter being chosen: the text
// is kept
static void accept(long now) {
	while (page == PAGE_CALLSIGN && entry.length > 0 && entry.text[entry.length - 1] == ' ')
		entry_delete(&entry);
	if (page == PAGE_CALLSIGN && entry.length > 0) {
		snprintf(settings.callsign, sizeof(settings.callsign), "%s", entry.text);
		settings_save();
		service_restart();
		console_say("CALL %s", settings.callsign);
	}
	go(PAGE_MENU);
}

// a WiFi key: UP and DOWN scroll, ENTER adds, ENTER held takes off, MENU
// joins with it
static void take_key(const struct key_press *press, long now) {
	if (is_up_or_down(press))
		entry_scroll(&entry, press->key == KEY_UP);
	if (press->key == KEY_ENTER && press->what == BUTTON_PRESSED)
		entry_add(&entry);
	else if (press->key == KEY_ENTER)
		entry_delete(&entry);
	if (press->key == KEY_MENU && press->what == BUTTON_PRESSED)
		join(entry.text, now);
}

// UP and DOWN are keyed in screen_poll(); ENTER drops the letter, or keeps the
// text, and held takes characters off
static void take_entry(const struct key_press *press, long now) {
	if (page == PAGE_WIFI_KEY) {
		take_key(press, now);
		return;
	}
	if (press->key == KEY_ENTER && press->what == BUTTON_PRESSED) {
		if (halves.at >= 0)
			halves_abort(&halves);
		else
			accept(now);
	} else if (press->key == KEY_ENTER) {
		halves_abort(&halves);
		entry_delete(&entry);
	}
	if (press->key == KEY_MENU && press->what == BUTTON_PRESSED)
		go(PAGE_MENU);
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
	if (choices_of(page) != NULL) {
		take_choices(choices_of(page), press);
		return;
	}
	if (page == PAGE_VOLUME && is_up_or_down(press)) {
		volume_step(press, now);
		return;
	}
	if (page == PAGE_WPM && is_up_or_down(press)) {
		wpm_step(press, now);
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

static void draw_choices(const struct choices *these) {
	int i;

	line(TITLE_ROW, these->title);
	for (i = 0; i < these->count; i++) {
		if (i == marked)
			display_text(MARK_X, MENU_FIRST_ROW + i, 1, ">");
		display_text(ITEM_X, MENU_FIRST_ROW + i, 1, these->names[i]);
	}
	line(LAST_ROW, "ENTER: TAKE MENU: BACK");
}

// what a menu item is set to, for the menu's line; nothing for those that
// are not settings
static void item_value(int of_page, char *text, int room) {
	const struct choices *these = choices_of(of_page);

	text[0] = 0;
	if (these != NULL)
		snprintf(text, room, "%s", these->names[these->in_use()]);
	if (of_page == PAGE_DIAL)
		snprintf(text, room, "%u", (unsigned)(settings.hz / HZ_A_KHZ));
	if (of_page == PAGE_VOLUME)
		snprintf(text, room, "%u", (unsigned)settings.volume);
	if (of_page == PAGE_WPM)
		snprintf(text, room, "%u", (unsigned)settings.wpm);
	if (of_page == PAGE_CALLSIGN)
		snprintf(text, room, "%s", settings.callsign);
	if (of_page == PAGE_SBITX)
		snprintf(text, room, "%s", service_is_online() ? "ONLINE" : "OFFLINE");
	if (of_page == PAGE_WIFI)
		snprintf(text, room, "%s", settings.ssid);
}

static void draw_menu(void) {
	int i;

	char value[TEXT_ROOM];
	char text[TEXT_ROOM];

	// the list scrolls to keep the chosen shown
	if (chosen < menu_top)
		menu_top = chosen;
	if (chosen >= menu_top + MENU_ROWS)
		menu_top = chosen - MENU_ROWS + 1;
	line(TITLE_ROW, "MENU");
	for (i = menu_top; i < ITEMS && i < menu_top + MENU_ROWS; i++) {
		if (i == chosen)
			display_text(MARK_X, MENU_FIRST_ROW + i - menu_top, 1, ">");
		item_value(items[i].page, value, sizeof(value));
		if (value[0] != 0)
			snprintf(text, sizeof(text), "%s:%s", items[i].name, value);
		else
			snprintf(text, sizeof(text), "%s", items[i].name);
		text[ITEM_COLUMNS] = 0;
		display_text(ITEM_X, MENU_FIRST_ROW + i - menu_top, 1, text);
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

// text on rows from row, broken at spaces where it can be, cut after rows
static void wrapped(int row, int rows, const char *text) {
	char cut[COLUMNS + 1];
	int length;
	int i;

	for (; *text != 0 && rows > 0; row++, rows--) {
		length = (int)strlen(text);
		if (length > COLUMNS) {
			length = COLUMNS;
			for (i = COLUMNS; i > 0; i--) {
				if (text[i] == ' ') {
					length = i;
					break;
				}
			}
		}
		snprintf(cut, sizeof(cut), "%.*s", length, text);
		display_text(0, row, 1, cut);
		text += length;
		while (*text == ' ')
			text++;
	}
}

// a box one dot wide round x, y, wide by high
static void frame(int x, int y, int wide, int high) {
	display_fill(x, y, wide, 1);
	display_fill(x, y + high - 1, wide, 1);
	display_fill(x, y, 1, high);
	display_fill(x + wide - 1, y, 1, high);
}

// one of halves.c's characters at x, y: in reverse video if it is
// offered, framed if it is the chosen
static void letter_at(int x, int y, char c, int is_chosen, int is_offered) {
	char one[2] = {c, 0};

	display_text_at(x + 1, y, 1, one);
	if (is_offered)
		display_invert(x, y - LETTER_EDGE, LETTER_PITCH - 1, FONT_HIGH + 2 * LETTER_EDGE);
	else if (is_chosen)
		frame(x, y - LETTER_EDGE, LETTER_PITCH - 1, FONT_HIGH + 2 * LETTER_EDGE);
}

// the prompt, the text's end and a cursor, and halves.c's letters, or its
// figures
static void draw_entry(const char *prompt, long now) {
	char text[TEXT_ROWS * COLUMNS + 1];
	int shown = entry.length;
	int left;
	int which;
	int at;

	wrapped(TITLE_ROW, PROMPT_ROWS, prompt);
	if (shown > TEXT_ROWS * COLUMNS - 1)
		shown = TEXT_ROWS * COLUMNS - 1;
	snprintf(text, sizeof(text), "%s_", entry.text + entry.length - shown);
	line(TEXT_ROW, text);
	if ((int)strlen(text) > COLUMNS)
		line(TEXT_ROW + 1, text + COLUMNS);
	if (halves.is_figures) {
		left = (DISPLAY_WIDE - HALVES_FIGURES_ACROSS * LETTER_PITCH) / 2;
		for (at = 0; at < HALVES_FIGURES_ACROSS; at++)
			letter_at(left + at * LETTER_PITCH, FIGURES_Y, HALVES_FIGURES[at],
				halves.at == at, halves_offered(&halves, 0, at));
	} else {
		left = (DISPLAY_WIDE - HALVES_ACROSS * LETTER_PITCH) / 2;
		for (which = 0; which < 2; which++) {
			for (at = 0; at < HALVES_ACROSS; at++)
				letter_at(left + at * LETTER_PITCH, LETTERS_Y + which * LINE_GAP,
					halves.lines[which][at], halves.at == at && halves.line == which,
					halves_offered(&halves, which, at));
		}
	}
	line(LAST_ROW, entry_help[now / HELP_EVERY_MS % ENTRY_HELPS]);
}

// a WiFi key: its end, a cursor, and the characters large, the chosen in
// the middle
static void draw_key(const char *title, long now) {
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
	line(LAST_ROW, key_help[now / HELP_EVERY_MS % KEY_HELPS]);
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
	char text[TEXT_ROOM];

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
	if (choices_of(page) != NULL)
		draw_choices(choices_of(page));
	if (page == PAGE_WPM) {
		line(TITLE_ROW, "MORSE SPEED, WPM");
		snprintf(text, sizeof(text), "%u", (unsigned)settings.wpm);
		big(text);
		line(LAST_ROW, "UP/DOWN, MENU: BACK");
	}
	if (page == PAGE_VOLUME) {
		line(TITLE_ROW, "VOLUME");
		volume_bar();
		line(LAST_ROW, "UP/DOWN, MENU: BACK");
	}
	if (page == PAGE_CALLSIGN)
		draw_entry("Enter your callsign:", now);
	if (page == PAGE_WIFI_KEY)
		draw_key(wifi_ssid, now);
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
	char letter;

	if (keys_poll(now, &press))
		take(&press, now);
	if (page == PAGE_CALLSIGN
			&& halves_keys(&halves, keys_is_down(KEY_UP), keys_is_down(KEY_DOWN), now, &letter))
		entry_put(&entry, letter);
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

// for the verb SCREEN: the page shown, the last key, and the display's
// pages sent
void screen_describe(char *text, int room) {
	char shown[TEXT_ROOM];

	display_describe(shown, sizeof(shown));
	snprintf(text, room, "PAGE %d, %s, %s", page, last_key, shown);
}
