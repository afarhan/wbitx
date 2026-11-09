// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ashhar Farhan
//
// screen.c — what the display shows and what the keys do.
//
// The display shows one page at a time, in two fonts (font.c): the main,
// Helvetica Bold of ten dots, for what a page says, and a small one for
// its title and help. Each but the home page has its title in the middle
// of the top, a rule under it, and up to five lines below, LINE_PITCH
// dots apart; with help at the foot, four. The menu shows five items at a
// time, each with what it is set to at the right.
//
// The home page, the talkie at rest:
//
//     3.9V    VU2ESE    o 90%       the supply, as on the menu; the
//     -------------------------      callsign; a dot, filled when online
//                                    at my.sbitx.net; the WiFi's signal
//          21080                     the channel, in kHz, large
//     2 here                  TX     who speaks, or how many are in the
//                                    room, or, not online, a few words on
//                                    why (service_brief()); TX or RX
//     Vol [#########     ] 50        the volume
//
// On it ENTER is the PTT: the talkie speaks while it is held (ptt.c). UP
// and DOWN set the volume, VOLUME_STEP a press, repeating while held. MENU
// opens the menu.
//
// The menu is a list, UP and DOWN to choose, ENTER to open; each page it
// opens goes back to it with MENU, and the menu to the home page. MENU
// held goes home from anywhere, and so does MENU_IDLE_MS untouched. Its
// title shows the supply at the left, 3.9V, or USB, and the WiFi's signal
// at the right, 90%. A name ending in "..." opens what is beyond:
//
//     Frequency      the channel, large; UP and DOWN step it in dial.c's
//                    kilohertz steps, faster the longer they are held
//     WiFi           the networks heard, strongest first, with their
//                    signal, * by those remembered; ENTER joins one that
//                    is open or remembered; one with a key not known, or
//                    ENTER held for a new key, opens Setup; the last item
//                    scans again. Then a page says whether it joined.
//     Contacts       the callsigns kept, added and deleted at Setup
//     Sleep          the talkie asleep, until any key wakes it (sleep.c)
//     Settings...    a list of its own, MENU back to the menu:
//         Audio          the in-built mic or the handsfree's
//         Morse speed    for entering text, large; UP and DOWN step it,
//                        from HALVES_WPM_LEAST to HALVES_WPM_MOST
//         Callsign       entered a character at a time, below
//         Setup...       the setup portal (portal.cpp), open while the
//                        page is shown: what to join on a telephone and
//                        where to go. It opens by itself at power-up when
//                        the talkie knows no network.
//         Theme          Black, lit dots on black, or White, the display
//                        turned over
//         Info           the network, the address, the signal, the
//                        callsign, the battery
//
// The volume is UP and DOWN on the home page.
//
// Info and Setup stay up; the others go home when idle.
//
// The callsign is entered on a page of its own:
//
//            CALLSIGN               the title
//     -------------------------
//     VU2ES_                    the text so far, up to ENTRY_ROOM - 1,
//                               its end if it is long, and a cursor
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
// A WiFi key is not entered on the talkie: at SETUP, on a telephone.
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
#include "contacts.h"
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
#include "portal.h"
#include "ptt.h"
#include "service.h"
#include "settings.h"
#include "screen.h"
#include "speaker.h"
#include "sleep.h"
#include "voip.h"

#define DRAW_EVERY_MS 200
#define SAVE_AFTER_MS 2000
#define VOLUME_STEP 5
#define MENU_IDLE_MS 30000
#define TEXT_ROOM 64
#define RULE_Y 9
#define LINES_Y 11
#define LINE_PITCH 11
#define MENU_ROWS 5
#define LIST_ROWS 4
#define HELP_Y 57
#define BIG_SCALE 3
#define PAGE_BIG_Y 18
#define HOME_BIG_Y 12
#define HOME_STATE_Y 39
#define HOME_VOLUME_Y 52
#define ONLINE_Y 1
#define ONLINE_SIZE 7
#define ONLINE_GAP 3
#define BAR_X 22
#define BAR_WIDE 80
#define MARK_X 0
#define ITEM_X 8
#define VALUE_GAP 4
#define HZ_A_KHZ 1000
#define LETTERS_Y 25
#define LINE_GAP 13
#define FIGURES_Y (LETTERS_Y + LINE_GAP / 2)
#define LETTER_PITCH 8
#define LETTER_EDGE 2
#define WPM_STEP 1
#define HELP_EVERY_MS 2000
#define JOIN_SHOWN_MS 20000

#define PAGE_HOME 0
#define PAGE_MENU 1
#define PAGE_DIAL 2
#define PAGE_CALLSIGN 4
#define PAGE_WIFI 7
#define PAGE_INFO 8
#define PAGE_SETUP 9
#define PAGE_WIFI_JOINING 10
#define PAGE_AUDIO 11
#define PAGE_WPM 12
#define PAGE_SLEEP 13
#define PAGE_CONTACTS 14
#define PAGE_SETTINGS 15
#define PAGE_THEME 16

struct item {
	const char *name;
	int page;
};

// The menu and, a level below it, the settings; a name ending in an
// ellipsis opens a list or a page of its own beyond
static const struct item main_items[] = {
	{"Frequency", PAGE_DIAL},
	{"WiFi", PAGE_WIFI},
	{"Contacts", PAGE_CONTACTS},
	{"Sleep", PAGE_SLEEP},
	{"Settings...", PAGE_SETTINGS},
};

static const struct item settings_items[] = {
	{"Audio", PAGE_AUDIO},
	{"Morse speed", PAGE_WPM},
	{"Callsign", PAGE_CALLSIGN},
	{"Setup...", PAGE_SETUP},
	{"Theme", PAGE_THEME},
	{"Info", PAGE_INFO},
};

struct menu {
	const char *title;
	const struct item *items;
	int count;
};

#define MAIN_MENU 0
#define SETTINGS_MENU 1
#define MENUS 2

static const struct menu menus[MENUS] = {
	{"Menu", main_items, (int)(sizeof(main_items) / sizeof(main_items[0]))},
	{"Settings", settings_items, (int)(sizeof(settings_items) / sizeof(settings_items[0]))},
};
#define MENU_ROWS 5

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
static int level;
static int chosen[MENUS];
static int menu_top[MENUS];
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

static const char *wifi_help[] = {
	"ENTER: JOIN",
	"HOLD ENTER: NEW KEY",
	"NEW KEYS: AT SETUP",
	"*: REMEMBERED",
	"MENU: BACK",
};

#define WIFI_HELPS ((int)(sizeof(wifi_help) / sizeof(wifi_help[0])))

static int wifi_chosen;
static int wifi_top;
static char wifi_ssid[SETTINGS_SSID_ROOM];
static long joining_since_ms;
static int contacts_top;
static int is_begun;

void screen_open(void) {
	keys_open();
	display_open();
	display_white(settings.is_white != 0);
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
	return on_page == PAGE_CALLSIGN;
}

// to another page; ENTER is the PTT on the home page, the screen's on the
// rest
static void go(int to) {
	if (to == PAGE_CALLSIGN) {
		entry_begin(&entry, ENTRY_CALLSIGN, settings.callsign, SETTINGS_CALLSIGN_ROOM - 1);
		halves_begin(&halves, (int)settings.wpm);
	}
	if (page == PAGE_SETUP && to != PAGE_SETUP)
		portal_close();
	if (to == PAGE_SETUP)
		portal_open();
	if (to == PAGE_CONTACTS)
		contacts_top = 0;
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
static const char *const audio_names[] = {"In-built", "Handsfree"};

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

// THEME: lit dots on black, or dark dots on white, the whole display
// turned over by the display itself (display_white())
static const char *const theme_names[] = {"Black", "White"};

static int theme_in_use(void) {
	return settings.is_white ? 1 : 0;
}

static void theme_take(int chosen_one) {
	if ((uint32_t)chosen_one == settings.is_white)
		return;
	settings.is_white = (uint32_t)chosen_one;
	settings_save();
	display_white(chosen_one);
}

static const struct choices all_choices[] = {
	{PAGE_AUDIO, "Audio", audio_names, 2, audio_in_use, audio_take},
	{PAGE_THEME, "Theme", theme_names, 2, theme_in_use, theme_take},
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
		level = MAIN_MENU;
		chosen[MAIN_MENU] = 0;
		go(PAGE_MENU);
		return;
	}
	if (is_up_or_down(press))
		volume_step(press, now);
}

// UP and DOWN choose, ENTER opens, MENU goes up a level, or home
static void take_menu(const struct key_press *press) {
	const struct menu *in = &menus[level];
	int *at = &chosen[level];
	int to = in->items[*at].page;

	if (press->key == KEY_MENU && level == SETTINGS_MENU) {
		level = MAIN_MENU;
		return;
	}
	if (press->key == KEY_MENU)
		go(PAGE_HOME);
	if (press->key == KEY_UP)
		*at = (*at + in->count - 1) % in->count;
	if (press->key == KEY_DOWN)
		*at = (*at + 1) % in->count;
	if (press->key == KEY_ENTER && to == PAGE_SETTINGS) {
		level = SETTINGS_MENU;
		chosen[SETTINGS_MENU] = 0;
		menu_top[SETTINGS_MENU] = 0;
		return;
	}
	if (press->key == KEY_ENTER && to == PAGE_SLEEP) {
		if (is_save_due) {
			is_save_due = 0;
			settings_save();
		}
		go(PAGE_HOME);
		sleep_now();
		return;
	}
	if (press->key == KEY_ENTER)
		go(to);
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

// a network chosen from the list: joined if it is open or remembered; if
// not, or if is_new_key, SETUP is opened for its key to be entered there
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
		go(PAGE_SETUP);
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

// UP and DOWN are keyed in screen_poll(); ENTER drops the letter, or keeps the
// text, and held takes characters off
static void take_entry(const struct key_press *press, long now) {
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

// the list of contacts moved a line, kept within it
static void contacts_scroll(int is_up) {
	int most = contacts_count(&settings) - LIST_ROWS;

	contacts_top += is_up ? -1 : 1;
	if (contacts_top > most)
		contacts_top = most;
	if (contacts_top < 0)
		contacts_top = 0;
}

static void take_page(const struct key_press *press, long now) {
	if (page == PAGE_DIAL && is_up_or_down(press)) {
		dial_turn(press, now);
		return;
	}
	if (choices_of(page) != NULL) {
		take_choices(choices_of(page), press);
		return;
	}
	if (page == PAGE_WPM && is_up_or_down(press)) {
		wpm_step(press, now);
		return;
	}
	if (page == PAGE_CONTACTS && is_up_or_down(press)) {
		contacts_scroll(press->key == KEY_UP);
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

// the dot row of the top of line i below the rule
static int line_y(int i) {
	return LINES_Y + i * LINE_PITCH;
}

// text on line i below the rule, in the main font; what goes past the
// edge is not shown
static void line(int i, const char *text) {
	display_text_at(0, line_y(i), 1, text);
}

// text cut from its end to fit in room dots
static void fit(char *text, int room) {
	int length;

	for (length = (int)strlen(text); length > 0 && display_text_wide(text, 1) > room; length--)
		text[length - 1] = 0;
}

// an item of a list on line i: a mark if it is the chosen, its name, and
// what it is set to at the right, the name cut to leave it room
static void item(int i, int is_chosen, const char *name, const char *value) {
	char cut[TEXT_ROOM];
	int y = line_y(i);
	int value_x = DISPLAY_WIDE - display_text_wide(value, 1);

	if (is_chosen)
		display_text_at(MARK_X, y, 1, ">");
	snprintf(cut, sizeof(cut), "%s", name);
	fit(cut, value_x - ITEM_X - (value[0] != 0 ? VALUE_GAP : 0));
	display_text_at(ITEM_X, y, 1, cut);
	display_text_at(value_x, y, 1, value);
}

static void rule(int y) {
	display_fill(0, y, DISPLAY_WIDE, 1);
}

// the title in the middle of the top, and the rule under it
static void title(const char *text) {
	display_text_at((DISPLAY_WIDE - display_text_wide(text, 1)) / 2, 0, 1, text);
	rule(RULE_Y);
}

// the help, in the small font at the foot
static void help(const char *text) {
	display_small_at(0, HELP_Y, text);
}

// text large and in the middle of the screen
static void big(const char *text, int y) {
	display_text_at((DISPLAY_WIDE - display_text_wide(text, BIG_SCALE)) / 2, y, BIG_SCALE, text);
}

static void big_frequency(int y) {
	char text[TEXT_ROOM];

	snprintf(text, sizeof(text), "%u", (unsigned)(settings.hz / HZ_A_KHZ));
	big(text, y);
}

static void volume_bar(int y) {
	char text[TEXT_ROOM];

	display_text_at(0, y, 1, "Vol");
	display_bar(BAR_X, y, BAR_WIDE, (int)settings.volume * BAR_WIDE / SETTINGS_LEVEL_MOST);
	snprintf(text, sizeof(text), "%u", (unsigned)settings.volume);
	display_text_at(DISPLAY_WIDE - display_text_wide(text, 1), y, 1, text);
}

static void status_row(const char *middle, int is_online_shown);

// a dot at x, filled when online at my.sbitx.net, a ring when not
static void online_mark(int x) {
	int edge = ONLINE_SIZE - 1;
	int y = ONLINE_Y;

	if (service_is_online()) {
		display_fill(x + 1, y, ONLINE_SIZE - 2, ONLINE_SIZE);
		display_fill(x, y + 1, ONLINE_SIZE, ONLINE_SIZE - 2);
		return;
	}
	display_fill(x + 1, y, ONLINE_SIZE - 2, 1);
	display_fill(x + 1, y + edge, ONLINE_SIZE - 2, 1);
	display_fill(x, y + 1, 1, ONLINE_SIZE - 2);
	display_fill(x + edge, y + 1, 1, ONLINE_SIZE - 2);
}

// who speaks; or how many are in the room; or, not online, a few words
// on why
static void state_line(int y) {
	char text[TEXT_ROOM];
	char why[TEXT_ROOM];

	if (voip_hearing(why, sizeof(why))) {
		snprintf(text, sizeof(text), "Heard %s", why);
	} else if (!service_is_online()) {
		service_brief(text, sizeof(text));
	} else if (voip_busy(why, sizeof(why))) {
		snprintf(text, sizeof(text), "Busy %s", why);
	} else if (voip_here() >= 0) {
		snprintf(text, sizeof(text), "%d here", voip_here());
	} else {
		text[0] = 0;
	}
	if (ptt_is_lit())
		fit(text, DISPLAY_WIDE - display_text_wide("TX", 1) - VALUE_GAP);
	display_text_at(0, y, 1, text);
	if (ptt_is_lit() && ptt_is_down())
		display_text_at(DISPLAY_WIDE - display_text_wide("TX", 1), y, 1, "TX");
	else if (ptt_is_lit())
		display_text_at(DISPLAY_WIDE - display_text_wide("RX", 1), y, 1, "RX");
}

static void draw_home(void) {
	status_row(settings.callsign, 1);
	big_frequency(HOME_BIG_Y);
	state_line(HOME_STATE_Y);
	volume_bar(HOME_VOLUME_Y);
}

static void draw_choices(const struct choices *these) {
	int i;

	title(these->title);
	for (i = 0; i < these->count && i < LIST_ROWS; i++)
		item(i, i == marked, these->names[i], "");
	help("ENTER: TAKE   MENU: BACK");
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
	if (of_page == PAGE_WPM)
		snprintf(text, room, "%u", (unsigned)settings.wpm);
	if (of_page == PAGE_CALLSIGN)
		snprintf(text, room, "%s", settings.callsign);
	if (of_page == PAGE_CONTACTS)
		snprintf(text, room, "%d", contacts_count(&settings));
	if (of_page == PAGE_WIFI)
		snprintf(text, room, "%s", settings.ssid);
	if (of_page == PAGE_CONTACTS && contacts_count(&settings) == 0)
		text[0] = 0;
	// a value too long for the line, a network's name, is cut
	fit(text, DISPLAY_WIDE / 2);
}

// the top row of the home page and the menus: the supply at the left,
// 3.9V, USB, or -- before it is read; the WiFi's signal at the right, as a
// percentage, 2 for each dB above -100, or -- when not joined, with the
// online mark before it if is_online_shown; middle in the middle, cut to
// fit between them; and the rule under it all
static void status_row(const char *middle, int is_online_shown) {
	char left[TEXT_ROOM];
	char right[TEXT_ROOM];
	char cut[TEXT_ROOM];
	int millivolts;
	int supply = microphone_battery(&millivolts);
	int percent;
	int right_x;
	int side;

	if (supply == MICROPHONE_BATTERY)
		snprintf(left, sizeof(left), "%d.%dV", millivolts / 1000, millivolts % 1000 / 100);
	else if (supply == MICROPHONE_USB)
		snprintf(left, sizeof(left), "USB");
	else
		snprintf(left, sizeof(left), "--");
	display_text_at(0, 0, 1, left);
	if (network_is_up()) {
		percent = 2 * (network_rssi() + 100);
		if (percent > 100)
			percent = 100;
		if (percent < 0)
			percent = 0;
		snprintf(right, sizeof(right), "%d%%", percent);
	} else {
		snprintf(right, sizeof(right), "--");
	}
	right_x = DISPLAY_WIDE - display_text_wide(right, 1);
	display_text_at(right_x, 0, 1, right);
	if (is_online_shown) {
		right_x -= ONLINE_SIZE + ONLINE_GAP;
		online_mark(right_x);
	}
	side = display_text_wide(left, 1);
	if (DISPLAY_WIDE - right_x > side)
		side = DISPLAY_WIDE - right_x;
	snprintf(cut, sizeof(cut), "%s", middle);
	fit(cut, DISPLAY_WIDE - 2 * (side + ONLINE_GAP));
	display_text_at((DISPLAY_WIDE - display_text_wide(cut, 1)) / 2, 0, 1, cut);
	rule(RULE_Y);
}

static void draw_menu(void) {
	const struct menu *in = &menus[level];
	char value[TEXT_ROOM];
	int *top = &menu_top[level];
	int at = chosen[level];
	int i;

	// the list scrolls to keep the chosen shown
	if (at < *top)
		*top = at;
	if (at >= *top + MENU_ROWS)
		*top = at - MENU_ROWS + 1;
	status_row(in->title, 0);
	for (i = *top; i < in->count && i < *top + MENU_ROWS; i++) {
		item_value(in->items[i].page, value, sizeof(value));
		item(i - *top, i == at, in->items[i].name, value);
	}
}

static void draw_info(void) {
	int millivolts;
	char text[TEXT_ROOM];
	char address[TEXT_ROOM];

	title("Info");
	snprintf(text, sizeof(text), "Wifi %s", settings.ssid);
	line(0, text);
	network_address(address, sizeof(address));
	if (address[0] == 0)
		snprintf(address, sizeof(address), "Not joined");
	line(1, address);
	if (network_is_up()) {
		snprintf(text, sizeof(text), "Signal %d dBm", network_rssi());
		line(2, text);
	}
	snprintf(text, sizeof(text), "Call %s", settings.callsign);
	line(3, text);
	if (microphone_battery(&millivolts) == MICROPHONE_BATTERY)
		snprintf(text, sizeof(text), "Battery %d.%02d V", millivolts / 1000, millivolts % 1000 / 10);
	else
		snprintf(text, sizeof(text), "On USB");
	line(4, text);
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
	display_letter_at(x + 1, y, 1, c);
	if (is_offered)
		display_invert(x, y - LETTER_EDGE, LETTER_PITCH - 1, FONT_HIGH + 2 * LETTER_EDGE);
	else if (is_chosen)
		frame(x, y - LETTER_EDGE, LETTER_PITCH - 1, FONT_HIGH + 2 * LETTER_EDGE);
}

// the title, the text's end and a cursor, and halves.c's letters, or its
// figures
static void draw_entry(const char *heading, long now) {
	char text[TEXT_ROOM];
	int shown = entry.length;
	int left;
	int which;
	int at;

	title(heading);
	snprintf(text, sizeof(text), "%s_", entry.text);
	// its end, if it is too long to show whole
	while (shown > 0 && display_text_wide(text + entry.length - shown, 1) > DISPLAY_WIDE)
		shown--;
	line(0, text + entry.length - shown);
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
	help(entry_help[now / HELP_EVERY_MS % ENTRY_HELPS]);
}

static void draw_wifi(long now) {
	char ssid[SETTINGS_SSID_ROOM];
	char text[TEXT_ROOM];
	int count = network_scanned_count();
	int row = 0;
	int rssi;
	int is_open;
	int i;

	title("WiFi");
	if (network_is_scanning()) {
		line(1, "Listening for");
		line(2, "networks...");
		return;
	}
	for (i = wifi_top; i <= count && row < LIST_ROWS; i++, row++) {
		if (i == count) {
			item(row, i == wifi_chosen, "Scan again", "");
			continue;
		}
		network_scanned(i, ssid, sizeof(ssid), &rssi, &is_open);
		snprintf(text, sizeof(text), "%s%d", known_find(&settings, ssid) >= 0 ? "* " : "", rssi);
		item(row, i == wifi_chosen, ssid, text);
	}
	help(wifi_help[now / HELP_EVERY_MS % WIFI_HELPS]);
}

static void draw_joining(long now) {
	char text[TEXT_ROOM];

	title("WiFi");
	line(0, wifi_ssid);
	network_ssid(text, sizeof(text));
	if (network_is_up() && !strcmp(text, wifi_ssid)) {
		line(1, "Joined");
		network_address(text, sizeof(text));
		line(2, text);
	} else if (now - joining_since_ms < JOIN_SHOWN_MS) {
		line(1, "Joining...");
	} else {
		line(1, "Not joined.");
		line(2, "Is the key right?");
	}
	help("MENU: BACK");
}

static void draw_setup(void) {
	char text[TEXT_ROOM];

	title("Setup");
	if (!portal_is_open()) {
		line(1, "It did not start.");
		help("MENU: BACK");
		return;
	}
	line(0, "On a phone, join");
	portal_name(text, sizeof(text));
	line(1, text);
	line(2, "then open");
	line(3, "192.168.4.1");
	if (network_ap_clients() > 0) {
		snprintf(text, sizeof(text), "%d JOINED   MENU: CLOSE", network_ap_clients());
		help(text);
	} else {
		help("MENU: CLOSE");
	}
}

static void draw_contacts(long now) {
	int count = contacts_count(&settings);
	int i;

	title("Contacts");
	if (count == 0) {
		line(0, "None yet.");
		line(1, "Add them at Setup.");
		help("MENU: BACK");
		return;
	}
	for (i = 0; i < LIST_ROWS && contacts_top + i < count; i++)
		line(i, settings.contacts[contacts_top + i]);
	help(now / HELP_EVERY_MS % 2 ? "ADD THEM AT SETUP" : "UP/DOWN   MENU: BACK");
}

static void draw(long now) {
	char text[TEXT_ROOM];

	display_clear();
	if (page == PAGE_HOME)
		draw_home();
	if (page == PAGE_MENU)
		draw_menu();
	if (page == PAGE_DIAL) {
		title("Frequency, kHz");
		big_frequency(PAGE_BIG_Y);
		help("UP/DOWN   MENU: BACK");
	}
	if (choices_of(page) != NULL)
		draw_choices(choices_of(page));
	if (page == PAGE_WPM) {
		title("Morse speed, WPM");
		snprintf(text, sizeof(text), "%u", (unsigned)settings.wpm);
		big(text, PAGE_BIG_Y);
		help("UP/DOWN   MENU: BACK");
	}
	if (page == PAGE_CALLSIGN)
		draw_entry("Callsign", now);
	if (page == PAGE_WIFI)
		draw_wifi(now);
	if (page == PAGE_WIFI_JOINING)
		draw_joining(now);
	if (page == PAGE_SETUP)
		draw_setup();
	if (page == PAGE_CONTACTS)
		draw_contacts(now);
	if (page == PAGE_INFO)
		draw_info();
}

void screen_poll(long now) {
	struct key_press press;
	char letter;

	if (keys_poll(now, &press))
		take(&press, now);
	if (portal_joined(wifi_ssid, sizeof(wifi_ssid))) {
		joining_since_ms = now;
		touched_ms = now;
		go(PAGE_WIFI_JOINING);
	}
	if (page == PAGE_CALLSIGN
			&& halves_keys(&halves, keys_is_down(KEY_UP), keys_is_down(KEY_DOWN), now, &letter))
		entry_put(&entry, letter);
	if (!is_begun) {
		is_begun = 1;
		touched_ms = now;
		if (settings.ssid[0] == 0 && known_count(&settings) == 0)
			go(PAGE_SETUP);
	}
	if (page != PAGE_HOME && page != PAGE_INFO && page != PAGE_SETUP && now - touched_ms >= MENU_IDLE_MS)
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
