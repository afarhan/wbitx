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
//          21080                     the target: a slot's kHz, or a
//                                    contact's callsign, large
//     2 here                  TX     who speaks, or how many are in the
//                                    room, or, not online, a few words on
//                                    why (service_brief()); TX or RX
//     Vol [#########     ] 50        the volume
//
// The PTT key speaks to the target (ptt.c); ENTER is the screen's on every
// page. On the home page ENTER opens the target's menu, MENU opens the
// menu, and UP and DOWN are the Morse paddles (morse.c): each word goes to
// the target at the gap after it, the letters keyed shown in place of the
// state line, "> HEL .-". A word heard is shown there for HEARD_SHOWN_MS,
// "VU2XZ: HELLO". The volume is in the target's menu. Until calls by
// callsign are made, a contact as the target speaks in the room of the
// slot last chosen.
//
// TEXT (text.c, PROTOCOL.md "Text, a word at a time") goes a word at a
// time over my.sbitx.net's line (service.c), to a callsign or a slot's
// room. A word sent is kept once TEXT SENT says it went, in brackets if
// nobody had it or it could not be sent. A word heard is kept, and played
// as Morse at its sender's speed, at the sidetone's level, unless its
// contact, or the slot of its room, has Read text off; a blocked contact's
// is dropped. All of it is kept in RAM only. The menu shows each contact's
// time, UTC, of its last word or voice, by my.sbitx.net's clock.
//
// The menu is a list, UP and DOWN to choose, round the ends, ENTER to open;
// each page it opens goes back to it with MENU, and the menu to the home
// page. MENU held goes home from anywhere, and so does MENU_IDLE_MS
// untouched. Its title shows the supply at the left, 3.9V, or USB, and the
// WiFi's signal at the right, 90%. A name ending in "..." opens what is
// beyond. It opens on Settings..., and UP from there is Add Callsign...:
//
//     Settings...    a list of its own, MENU back to the menu, below
//     F1  21090      the three frequency slots, with their kHz
//     F2   7074
//     F3  14200
//     VU2XZ          the contacts, in the order they were added; the
//     ...            target has a * after its name, a blocked one Blocked
//     Add Callsign...  a callsign entered, as the callsign is, below
//
// ENTER on a slot or a contact opens its menu, its name as the title; a
// slot opened becomes the target then and there, a contact only by PTT:
//
//     Frequency      a slot's only: its kHz, large; UP and DOWN step it
//                    in dial.c's kilohertz steps, faster the longer they
//                    are held, and the room with it if it is the target
//     PTT            makes it the target, and goes home
//     Text...        its text, words heard and sent, under headers
//                    from>to, the newest at the foot, UP and DOWN to
//                    scroll; the PTT key turns to sending, a word at a
//                    time, keyed (sent at the gap after it, or ENTER) or
//                    on the keyboard (ENTER held), and back
//     Volume         the volume on its bar, UP and DOWN to step it
//     Read text      On or Off: whether its text is to be heard as Morse
//     Block          a contact's only, On or Off: not heard at all
//     Edit callsign  a contact's only: entered again; to nothing deletes it
//     Delete...      a contact's only: asks, and ENTER deletes it
//
// Read text and Block are kept in the settings, for each slot and contact
// (contacts.c moves a contact's with it). Contacts are added and deleted
// at Setup too. The settings:
//
//     WiFi           the networks heard, strongest first, with their
//                    signal, * by those remembered; ENTER joins one that
//                    is open or remembered; one with a key not known, or
//                    ENTER held for a new key, opens Setup; the last item
//                    scans again. Then a page says whether it joined.
//     Sleep          the talkie asleep, until any key wakes it (sleep.c);
//                    also the main menu's first row, and MENU held
//                    SLEEP_HOLD_MS on the home page
//     Audio          the in-built mic or the handsfree's
//     Typing         text on the keyboard, or keyed as Morse
//     Morse speed    for entering text, large; UP and DOWN step it,
//                    from MORSE_WPM_LEAST to MORSE_WPM_MOST
//     Sidetone       the Morse keyer's tone, 0 (off) to 10, on a bar as
//                    the volume is (struct level); UP and DOWN step
//                    it, and it is heard at each step
//     VOX            speaking by the voice, without the PTT (vox.c):
//                    the sensitivity, Off or 1 to 10, large; UP and
//                    DOWN step it; ENTER turns it off, or on again; below,
//                    the voice's level now as a bar, the level that keys
//                    as a mark across it, and KEYED while the voice keys
//                    the talkie
//     Callsign       entered a character at a time, below
//     Configure...   the setup portal (portal.cpp) for the callsign, the
//                    networks and the contacts, open while the page is
//                    shown: what to join on a telephone and where to go.
//                    It opens by itself at power-up when the talkie
//                    knows no network.
//     Update...      a list of two (update.cpp):
//         From github.com  the newest firmware on github.com/afarhan/wbitx,
//                    ENTER to begin, over the WiFi joined; how far on a
//                    bar, then Up to date, or restarting into it
//         Locally    the setup portal for the firmware only: what to join
//                    on a telephone, where to go, and the code it asks
//                    for; then how far the file sent has got
//     Device key     the talkie's key, to add at my.sbitx.net as one
//                    of the owner's radios, and whether it is in yet
//     Theme          Black, lit dots on black, or White, the display
//                    turned over
//     Info           the network, the address, the signal, the
//                    callsign, the battery
//
// Info, Configure, the two Update pages and Device key stay up; the others
// go home when idle.
//
// The callsign is entered on a page of its own, on the keyboard (entry.c,
// draw_keyboard()) or, with settings.is_morse_typing, keyed as Morse:
//
//            CALLSIGN               the title
//     -------------------------
//     VU2ES_                    the text so far, up to ENTRY_ROOM - 1,
//                               its end if it is long, and a cursor
//        .-.                    the elements keyed of the next, large
//     UP: DIT  DOWN: DAH        the help, changing every HELP_EVERY_MS
//
// UP and DOWN are an iambic keyer's paddles, UP the dits and DOWN the dahs
// (morse.c), at settings.wpm, each element heard as a sidetone; the gap
// between letters adds the character to the text, if it is of the set.
// ENTER pressed while a character is being keyed drops it; with none, it
// keeps the text and goes back. ENTER held takes the last character off,
// and the one before it, while it is held. MENU goes back and leaves the
// text as it was.
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
#include <stdlib.h>
#include <string.h>

#include "auth.h"
#include "console.h"
#include "contacts.h"
#include "dial.h"
#include "display.h"
#include "enrol.h"
#include "entry.h"
#include "font.h"
#include "keys.h"
#include "morse.h"
#include "known.h"
#include "local.h"
#include "microphone.h"
#include "pins.h"
#include "network.h"
#include "portal.h"
#include "ptt.h"
#include "service.h"
#include "settings.h"
#include "screen.h"
#include "update.h"
#include "speaker.h"
#include "text.h"
#include "sleep.h"
#include "voip.h"
#include "vox.h"

#define DRAW_EVERY_MS 200
#define SAVE_AFTER_MS 2000
#define VOLUME_STEP 5
#define MENU_IDLE_MS 30000
#define TEXT_ROOM 64
#define RULE_Y 9
#define LINES_Y 11
#define LINE_PITCH 10
#define MENU_ROWS 5
#define LIST_ROWS 4
#define HELP_Y 55
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
#define CODE_Y 30
// the keyboard's: a key's width, its rows' tops and pitch, the last row's
// foot clear of the help
#define KEY_PITCH 11
#define KEYS_Y 14
#define KEY_ROW_PITCH 10
#define LETTER_EDGE 2
#define WPM_STEP 1
#define HELP_EVERY_MS 2000
#define JOIN_SHOWN_MS 20000
#define JOIN_FAILED_MS 6000
// a network heard weaker than this, in dBm, may not hear the talkie
#define WEAK_RSSI -75

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
#define PAGE_SETTINGS 15
#define PAGE_THEME 16
#define PAGE_ACTIVATE 18
#define PAGE_VOX 21
#define PAGE_TYPING 22
#define PAGE_CONTACT 23
#define PAGE_CONTACT_DELETE 24
#define PAGE_SIDETONE 25
#define PAGE_TARGET 26
#define PAGE_TEXT 27
#define PAGE_VOLUME 28
#define PAGE_UPDATE 29
#define PAGE_UPDATE_GITHUB 30
#define PAGE_UPDATE_LOCAL 31
#define PAGE_WIFI_KEY 32
#define PAGE_GRID 33
// a grid square of six, MK97FK; four will do
#define GRID_LONGEST 6
// the sidetone heard at each step of its level
#define SIDETONE_STEP_MS 250
// a word on the entry page, why the text was not kept, shown this long
#define ENTRY_SAID_MS 2000
#define VOX_METER_MOST 600
// a word heard shown on the home page this long
#define HEARD_SHOWN_MS 10000
// words sent and not yet answered by TEXT SENT
#define PENDING_MOST 8
// the Text page's lines, wrapped, and how many are shown
#define TEXT_LINES 96
#define TEXT_LINE_ROOM 32
#define TEXT_SHOWN 4
// what a target's menu holds: a slot all but the last three, a contact
// all but the first
#define TARGET_FREQUENCY 0
#define TARGET_PTT 1
#define TARGET_TEXT 2
#define TARGET_VOLUME 3
#define TARGET_READ 4
#define TARGET_BLOCK 5
#define TARGET_EDIT 6
#define TARGET_DELETE 7
#define TARGET_ITEMS 8
// the main menu's rows: Settings..., the slots, the contacts, Add Callsign...
#define ROW_SLEEP 0
#define ROW_SETTINGS 1
#define ROW_SLOTS 2
// MENU held this long on the home page puts the talkie to sleep
#define SLEEP_HOLD_MS 3000
#define ROW_CONTACTS (ROW_SLOTS + SETTINGS_SLOTS)

struct item {
	const char *name;
	int page;
};

// The menu and, a level below it, the settings; a name ending in an
// ellipsis opens a list or a page of its own beyond. The main menu's rows
// are made as it is drawn (main_rows()); this is only its title.
static const struct item main_items[] = {
	{"Settings...", PAGE_SETTINGS},
};

static const struct item settings_items[] = {
	{"WiFi", PAGE_WIFI},
	{"Sleep", PAGE_SLEEP},
	{"Audio", PAGE_AUDIO},
	{"Typing", PAGE_TYPING},
	{"Morse speed", PAGE_WPM},
	{"Sidetone", PAGE_SIDETONE},
	{"VOX", PAGE_VOX},
	{"Callsign", PAGE_CALLSIGN},
	{"Grid", PAGE_GRID},
	{"Configure...", PAGE_SETUP},
	{"Update...", PAGE_UPDATE},
	{"Device key", PAGE_ACTIVATE},
	{"Theme", PAGE_THEME},
	{"Info", PAGE_INFO},
};

static const struct item update_items[] = {
	{"From github.com", PAGE_UPDATE_GITHUB},
	{"Locally", PAGE_UPDATE_LOCAL},
};

struct menu {
	const char *title;
	const struct item *items;
	int count;
};

#define MAIN_MENU 0
#define SETTINGS_MENU 1
#define UPDATE_MENU 2
#define MENUS 3

static const struct menu menus[MENUS] = {
	{"Menu", main_items, (int)(sizeof(main_items) / sizeof(main_items[0]))},
	{"Settings", settings_items, (int)(sizeof(settings_items) / sizeof(settings_items[0]))},
	{"Update", update_items, (int)(sizeof(update_items) / sizeof(update_items[0]))},
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
// OK or CANCEL, from the keyboard, shown on its own for FLASH_MS
static const char *flashed;
static long flashed_ms;
#define FLASH_MS 800
static long save_ms;
static int is_save_due;
static long menu_down_ms;   // MENU went down on the home page, or 0
static char last_key[TEXT_ROOM];
static long touched_ms;
static long polled_ms;
static int page = PAGE_HOME;
static int level;
static int chosen[MENUS];
static int menu_top[MENUS];
static struct entry entry;
static struct morse morse;

static const char *entry_help[] = {
	"UP: DIT  DOWN: DAH",
	"PAUSE: TAKE LETTER",
	"ENTER: DONE",
	"HOLD ENTER: DELETE",
	"MENU: CANCEL",
};

#define ENTRY_HELPS ((int)(sizeof(entry_help) / sizeof(entry_help[0])))

static const char *keyboard_help[] = {
	"UP/DOWN: CHOOSE",
	"ENTER: TYPE IT",
	"MENU: DELETE",
	"HOLD ENTER: OK",
	"HOLD MENU: CANCEL",
};

#define KEYBOARD_HELPS ((int)(sizeof(keyboard_help) / sizeof(keyboard_help[0])))

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
// the key last tried on wifi_ssid, to be edited if it did not join, and
// the signal the network was heard at in the scan
static char wifi_key[SETTINGS_PASSWORD_ROOM];
static int wifi_rssi;
static long joining_since_ms;
// the Activate page put up by itself, once, for a talkie with no callsign
static int is_activate_shown;
// the target opened, a slot or SETTINGS_TARGET_CONTACT + a contact's
// place, for its menu and the pages it opens; and where its menu was
// opened from, the menu or the home page
static uint32_t opened;
static int opened_from;
static int target_chosen;
static int target_top;
// the contact being edited on PAGE_CONTACT, -1 for one being added
static int contact_editing;
// a contact edited to nothing, and so deleted
static int is_contact_gone;
// the text, words heard and sent, and the player of those heard
static struct text kept;
static struct text_play play;
// on the Text page: sending, and the PTT key as it was; the lines it shows
// and how far they are scrolled back from the newest
static int is_text_sending;
static int was_ptt_key;
static char text_lines[TEXT_LINES][TEXT_LINE_ROOM];
static int text_is_header[TEXT_LINES];
static int text_back;
// the word last heard, for the home page
static char heard_from[TEXT_PARTY_ROOM];
static char heard_word[TEXT_WORD_ROOM];
static long heard_ms;
// words sent, to whom, awaiting TEXT SENT, oldest first
struct pending {
	char to[TEXT_PARTY_ROOM];
	char word[TEXT_WORD_ROOM];
};
static struct pending pending[PENDING_MOST];
static int pending_count;
// for the times: the PTT as it was, and the voice heard
static int was_ptt;
static char was_hearing[TEXT_PARTY_ROOM];
static const char *entry_said;
static long entry_said_ms;
static int is_begun;

void screen_open(void) {
	// ENTER is the PTT on the home page, the screen's on every other page
	ptt_lend(page != PAGE_HOME);
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
	return on_page == PAGE_CALLSIGN || on_page == PAGE_CONTACT || on_page == PAGE_GRID
		|| on_page == PAGE_WIFI_KEY
		|| (on_page == PAGE_TEXT && is_text_sending);
}

// where a page of text entry goes back to
static int entered_from(int on_page) {
	if (on_page == PAGE_CONTACT && contact_editing >= 0 && !is_contact_gone)
		return PAGE_TARGET;
	if (on_page == PAGE_TEXT)
		return PAGE_TARGET;
	if (on_page == PAGE_WIFI_KEY)
		return PAGE_WIFI;
	return PAGE_MENU;
}

static int is_contact(uint32_t target) {
	return target >= SETTINGS_TARGET_CONTACT;
}

static int contact_of(uint32_t target) {
	return (int)(target - SETTINGS_TARGET_CONTACT);
}

// a target's name: F2 for a slot, the callsign for a contact
static void target_name(uint32_t target, char *text, int room) {
	if (is_contact(target))
		snprintf(text, room, "%s", settings.contacts[contact_of(target)]);
	else
		snprintf(text, room, "F%u", (unsigned)target + 1);
}

// what text calls a target: a contact's callsign, a slot's kHz
static void target_party(uint32_t target, char *text, int room) {
	if (is_contact(target))
		snprintf(text, room, "%s", settings.contacts[contact_of(target)]);
	else
		snprintf(text, room, "%u", (unsigned)(settings.slot_hz[target] / HZ_A_KHZ));
}

// 1 if the target's text is not read out as Morse
static int is_quiet(uint32_t target) {
	if (is_contact(target))
		return (settings.contacts_quiet >> contact_of(target)) & 1;
	return (settings.slots_quiet >> target) & 1;
}

static void quiet_toggle(uint32_t target) {
	if (is_contact(target))
		settings.contacts_quiet ^= 1u << contact_of(target);
	else
		settings.slots_quiet ^= 1u << target;
}

static int is_blocked(uint32_t target) {
	return is_contact(target) && ((settings.contacts_blocked >> contact_of(target)) & 1);
}

// the items of the opened target's menu, in items; how many
static int target_items(int *items) {
	int n = 0;

	if (!is_contact(opened))
		items[n++] = TARGET_FREQUENCY;
	items[n++] = TARGET_PTT;
	items[n++] = TARGET_TEXT;
	items[n++] = TARGET_VOLUME;
	items[n++] = TARGET_READ;
	if (is_contact(opened)) {
		items[n++] = TARGET_BLOCK;
		items[n++] = TARGET_EDIT;
		items[n++] = TARGET_DELETE;
	}
	return n;
}

// the main menu's rows, and what a row is: ROW_SETTINGS, a target, or
// -1 for Add Callsign...
static int main_rows(void) {
	int count = contacts_count(&settings);

	return ROW_CONTACTS + count + (count < SETTINGS_CONTACTS ? 1 : 0);
}

static int row_target(int row) {
	if (row >= ROW_SLOTS && row < ROW_CONTACTS)
		return row - ROW_SLOTS;
	if (row >= ROW_CONTACTS && row < ROW_CONTACTS + contacts_count(&settings))
		return SETTINGS_TARGET_CONTACT + row - ROW_CONTACTS;
	return -1;
}

// to another page; ENTER is the screen's on every page, the PTT key the
// talkie's
static void go(int to) {
	if (to == PAGE_CALLSIGN || to == PAGE_CONTACT) {
		entry_begin(&entry, ENTRY_CALLSIGN,
			to == PAGE_CALLSIGN ? settings.callsign
			: contact_editing >= 0 ? settings.contacts[contact_editing] : "",
			SETTINGS_CALLSIGN_ROOM - 1);
	}
	if (to == PAGE_GRID)
		entry_begin(&entry, ENTRY_CALLSIGN, settings.grid, GRID_LONGEST);
	if (to == PAGE_TEXT || to == PAGE_HOME)
		entry_begin(&entry, ENTRY_ANY, "", TEXT_WORD_ROOM - 1);
	// from a join that failed, the key that was tried, to be put right
	if (to == PAGE_WIFI_KEY)
		entry_begin(&entry, ENTRY_ANY, page == PAGE_WIFI_JOINING ? wifi_key : "",
			SETTINGS_PASSWORD_ROOM - 1);
	if (to == PAGE_TEXT && page != PAGE_TEXT) {
		text_back = 0;
		was_ptt_key = ptt_key_is_down();
	}
	if (to != PAGE_TEXT) {
		is_text_sending = 0;
		ptt_key_lend(0);
	}
	if (is_entering(to) || to == PAGE_HOME) {
		morse_begin(&morse, (int)settings.wpm);
		entry_said = NULL;
	}
	if ((page == PAGE_SETUP || page == PAGE_UPDATE_LOCAL) && to != page)
		portal_close();
	if (page == PAGE_UPDATE_GITHUB && to != page)
		update_forget();
	if (to == PAGE_SETUP)
		portal_open(PORTAL_CONFIGURE);
	if (to == PAGE_UPDATE_LOCAL)
		portal_open(PORTAL_UPDATE);
	if (to == PAGE_CONTACT)
		is_contact_gone = 0;
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

	if (wpm < MORSE_WPM_LEAST)
		wpm = MORSE_WPM_LEAST;
	if (wpm > MORSE_WPM_MOST)
		wpm = MORSE_WPM_MOST;
	settings.wpm = (uint32_t)wpm;
	save_soon(now);
}

// the level VOX goes back to when ENTER turns it on again
#define VOX_LEVEL_FIRST 5
static int vox_last = VOX_LEVEL_FIRST;

// ENTER: VOX off, or on again at the level it had
static void vox_toggle(long now) {
	if (settings.vox != VOX_OFF) {
		vox_last = (int)settings.vox;
		settings.vox = VOX_OFF;
	} else {
		settings.vox = (uint32_t)vox_last;
	}
	save_soon(now);
}

static void vox_step(const struct key_press *press, long now) {
	int level = (int)settings.vox + (press->key == KEY_UP ? 1 : -1);

	if (level < VOX_OFF)
		level = VOX_OFF;
	if (level > VOX_LEVEL_MOST)
		level = VOX_LEVEL_MOST;
	settings.vox = (uint32_t)level;
	if (level != VOX_OFF)
		vox_last = level;
	save_soon(now);
}

// a level set with a bar: its label, the setting, its least and most and
// a press's step, and what is done once it has stepped, if anything. The
// volume on the home page and the sidetone on its page are each one.
struct level {
	const char *label;
	uint32_t *value;
	int least;
	int most;
	int step;
	void (*stepped)(void);
};

// the sidetone heard at its new level
static void sidetone_heard(void) {
	speaker_tone(SIDETONE_STEP_MS);
}

static const struct level volume_level = {"Vol", &settings.volume, 0, SETTINGS_LEVEL_MOST,
	VOLUME_STEP, NULL};
static const struct level sidetone_level = {"Tone", &settings.sidetone, 0, SETTINGS_SIDETONE_MOST,
	1, sidetone_heard};

// UP a step up, DOWN a step down, within the level's least and most
static void level_step(const struct level *l, const struct key_press *press, long now) {
	int value = (int)*l->value + (press->key == KEY_UP ? l->step : -l->step);

	if (value < l->least)
		value = l->least;
	if (value > l->most)
		value = l->most;
	*l->value = (uint32_t)value;
	if (l->stepped != NULL)
		l->stepped();
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

// TYPING: text chosen on the keyboard, or keyed as Morse
static const char *const typing_names[] = {"Keyboard", "Morse"};

static int typing_in_use(void) {
	return settings.is_morse_typing ? 1 : 0;
}

static void typing_take(int chosen_one) {
	if ((uint32_t)chosen_one == settings.is_morse_typing)
		return;
	settings.is_morse_typing = (uint32_t)chosen_one;
	settings_save();
}

static const struct choices all_choices[] = {
	{PAGE_AUDIO, "Audio", audio_names, 2, audio_in_use, audio_take},
	{PAGE_THEME, "Theme", theme_names, 2, theme_in_use, theme_take},
	{PAGE_TYPING, "Typing", typing_names, 2, typing_in_use, typing_take},
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

// the opened slot's frequency stepped, and the room with it if the slot
// is the target
static void dial_turn(const struct key_press *press, long now) {
	if (is_contact(opened))
		return;
	settings.slot_hz[opened] = dial_step(settings.slot_hz[opened], press->key == KEY_UP, press->held_ms);
	if (settings.target == opened)
		settings.hz = settings.slot_hz[opened];
	save_soon(now);
}

static int is_up_or_down(const struct key_press *press) {
	return press->key == KEY_UP || press->key == KEY_DOWN;
}

// the target's menu opened, from the menu or the home page
static void open_target(uint32_t target, int from) {
	opened = target;
	opened_from = from;
	target_chosen = 0;
	target_top = 0;
	go(PAGE_TARGET);
}

static void take_home(const struct key_press *press, long now) {
	if (press->key == KEY_MENU) {
		level = MAIN_MENU;
		chosen[MAIN_MENU] = 0;
		menu_top[MAIN_MENU] = 0;
		go(PAGE_MENU);
		return;
	}
	// ENTER is the PTT here (ptt.c); UP and DOWN set the volume
	if (is_up_or_down(press))
		level_step(&volume_level, press, now);
}

// the Text page, reading: UP back to older lines, DOWN on to newer, MENU
// back to the target's menu; the PTT key turns to sending (screen_poll())
static void take_text(const struct key_press *press) {
	if (press->key == KEY_UP)
		text_back++;
	if (press->key == KEY_DOWN && text_back > 0)
		text_back--;
	if (press->key == KEY_MENU && press->what == BUTTON_PRESSED)
		go(PAGE_TARGET);
}

// the settings saved if they are due, home, and asleep until a key wakes it
static void asleep(void) {
	if (is_save_due) {
		is_save_due = 0;
		settings_save();
	}
	go(PAGE_HOME);
	sleep_now();
}

// the main menu: UP and DOWN choose, round the ends, so that UP from the
// first is Add Callsign...; ENTER opens Settings..., a target's menu, or
// a callsign to be entered; MENU goes home
static void take_main(const struct key_press *press) {
	int rows = main_rows();
	int *at = &chosen[MAIN_MENU];
	int target;

	if (*at >= rows)
		*at = rows - 1;
	if (press->key == KEY_MENU)
		go(PAGE_HOME);
	if (press->key == KEY_UP)
		*at = (*at + rows - 1) % rows;
	if (press->key == KEY_DOWN)
		*at = (*at + 1) % rows;
	if (press->key != KEY_ENTER)
		return;
	if (*at == ROW_SLEEP) {
		asleep();
		return;
	}
	if (*at == ROW_SETTINGS) {
		level = SETTINGS_MENU;
		chosen[SETTINGS_MENU] = 0;
		menu_top[SETTINGS_MENU] = 0;
		return;
	}
	target = row_target(*at);
	// a slot opened is the target at once, as a radio's memory chosen is
	// the one it is on; a contact is opened for its text as well, and is
	// the target only by its PTT
	if (target >= 0 && !is_contact((uint32_t)target) && (uint32_t)target != settings.target) {
		settings.target = (uint32_t)target;
		settings_targets();
		settings_save();
	}
	if (target >= 0) {
		open_target((uint32_t)target, PAGE_MENU);
		return;
	}
	contact_editing = -1;
	go(PAGE_CONTACT);
}

// UP and DOWN choose, ENTER opens, MENU goes up a level, or home
static void take_menu(const struct key_press *press) {
	const struct menu *in = &menus[level];
	int *at = &chosen[level];
	int to;

	if (level == MAIN_MENU) {
		take_main(press);
		return;
	}
	to = in->items[*at].page;

	if (press->key == KEY_MENU && level == SETTINGS_MENU) {
		level = MAIN_MENU;
		return;
	}
	if (press->key == KEY_MENU && level == UPDATE_MENU) {
		level = SETTINGS_MENU;
		return;
	}
	if (press->key == KEY_ENTER && to == PAGE_UPDATE) {
		level = UPDATE_MENU;
		chosen[UPDATE_MENU] = 0;
		menu_top[UPDATE_MENU] = 0;
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
		asleep();
		return;
	}
	if (press->key == KEY_ENTER)
		go(to);
}

// the network chosen, wifi_ssid, set and joined with this key
static void join(const char *password, long now) {
	snprintf(settings.ssid, sizeof(settings.ssid), "%s", wifi_ssid);
	snprintf(settings.password, sizeof(settings.password), "%s", password);
	snprintf(wifi_key, sizeof(wifi_key), "%s", password);
	settings_save();
	network_watch(wifi_ssid);
	network_join();
	joining_since_ms = now;
	go(PAGE_WIFI_JOINING);
}

// a network chosen from the list: joined if it is open or remembered; if
// not, or if is_new_key, its key entered on the keyboard (PAGE_WIFI_KEY)
static void pick(int i, int is_new_key, long now) {
	int rssi;
	int is_open;
	int known;

	network_scanned(i, wifi_ssid, sizeof(wifi_ssid), &rssi, &is_open);
	wifi_rssi = rssi;
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

// a contact entered: added, or the one edited changed, or deleted if it
// was edited to nothing; 0 with entry_said why not
static int contact_keep(long now) {
	char call[SETTINGS_CALLSIGN_ROOM];
	int is_kept;

	if (entry.length == 0 && contact_editing < 0)
		return 1;
	if (entry.length == 0) {
		console_say("CONTACT %s DELETED", settings.contacts[contact_editing]);
		contacts_forget(&settings, contact_editing);
		settings_targets();
		settings_save();
		is_contact_gone = 1;
		chosen[MAIN_MENU] = ROW_SETTINGS;
		return 1;
	}
	entry_said_ms = now;
	if (!contacts_callsign(entry.text, call)) {
		entry_said = "NOT A CALLSIGN";
		return 0;
	}
	is_kept = contact_editing < 0 ? contacts_add(&settings, call)
		: contacts_replace(&settings, contact_editing, call);
	if (!is_kept) {
		entry_said = "ALREADY THERE";
		return 0;
	}
	if (contact_editing < 0)
		chosen[MAIN_MENU] = ROW_CONTACTS + contacts_count(&settings) - 1;
	settings_save();
	console_say("CONTACT %s", call);
	return 1;
}

// a word sent to a target, kept as sent once TEXT SENT says it went; one
// that cannot be sent, the talkie not on line, kept at once in brackets
static void send_word(uint32_t target, const char *word, long now) {
	char party[TEXT_PARTY_ROOM];
	char shown[TEXT_WORD_ROOM + 2];
	struct pending *p;

	if (word[0] == 0)
		return;
	target_party(target, party, sizeof(party));
	text_touch(&kept, party, now);
	if (pending_count == PENDING_MOST || service_send_word(party, (int)settings.wpm, word) < 0) {
		snprintf(shown, sizeof(shown), "[%s]", word);
		text_add(&kept, settings.callsign, party, shown);
		return;
	}
	p = &pending[pending_count++];
	snprintf(p->to, sizeof(p->to), "%s", party);
	snprintf(p->word, sizeof(p->word), "%s", word);
}

// the word typed on the Text page sent to its target, and the entry
// emptied for the next
static void send_entry(long now) {
	send_word(opened, entry.text, now);
	entry_begin(&entry, ENTRY_ANY, "", TEXT_WORD_ROOM - 1);
	morse_abort(&morse);
}

// the grid typed, MK97 or MK97FK, kept and said to the service, which puts
// the talkie on the map by it; empty clears it. Anything else is refused
static int is_grid(const char *grid) {
	size_t length = strlen(grid);
	size_t i;

	if (length != 4 && length != GRID_LONGEST)
		return 0;
	for (i = 0; i < length; i++) {
		char c = grid[i];
		int is_ok = i == 2 || i == 3 ? c >= '0' && c <= '9'
			: i < 2 ? c >= 'A' && c <= 'R' : c >= 'A' && c <= 'X';

		if (!is_ok)
			return 0;
	}
	return 1;
}

static int grid_keep(long now) {
	if (entry.length > 0 && !is_grid(entry.text)) {
		entry_said = "NOT A GRID";
		entry_said_ms = now;
		return 0;
	}
	snprintf(settings.grid, sizeof(settings.grid), "%s", entry.text);
	settings_save();
	console_say("GRID %s", settings.grid);
	return 1;
}

// ENTER pressed on a page of text entry, no letter being chosen: the text
// is kept
static void accept(long now) {
	if (page == PAGE_WIFI_KEY) {
		join(entry.text, now);
		return;
	}
	while (entry.length > 0 && entry.text[entry.length - 1] == ' ')
		entry_delete(&entry);
	if (page == PAGE_CONTACT && !contact_keep(now))
		return;
	if (page == PAGE_TEXT) {
		send_entry(now);
		return;
	}
	if (page == PAGE_GRID && !grid_keep(now))
		return;
	if (page == PAGE_CALLSIGN && entry.length > 0) {
		snprintf(settings.callsign, sizeof(settings.callsign), "%s", entry.text);
		settings_save();
		service_restart();
		console_say("CALL %s", settings.callsign);
	}
	go(entered_from(page));
}

// MENU held on the keyboard: the text left as it was; on the Text page the
// word dropped and back to reading
static void abandon(void) {
	if (page == PAGE_TEXT) {
		is_text_sending = 0;
		entry_begin(&entry, ENTRY_ANY, "", TEXT_WORD_ROOM - 1);
		drawn_ms -= DRAW_EVERY_MS;
		return;
	}
	go(entered_from(page));
}

// the keyboard: UP and DOWN go back and on along the strip of its three
// layers, round its ends; ENTER types the chosen key, MENU takes the last
// character off; ENTER held is ENTRY_OK, MENU held ENTRY_CANCEL. ENTER and
// MENU are holding keys while the keyboard is up, so each acts as it comes
// up (screen_poll())
static int keyboard_key(const struct key_press *press) {
	int is_short = press->what == BUTTON_PRESSED;

	if (press->key == KEY_UP)
		entry_key_move(&entry, -1);
	if (press->key == KEY_DOWN)
		entry_key_move(&entry, 1);
	if (press->key == KEY_ENTER && is_short)
		entry_key_take(&entry);
	if (press->key == KEY_MENU && is_short)
		entry_delete(&entry);
	if (press->key == KEY_ENTER && press->what == BUTTON_HELD)
		return ENTRY_OK;
	if (press->key == KEY_MENU && press->what == BUTTON_HELD)
		return ENTRY_CANCEL;
	return ENTRY_GOING;
}

// the keyboard's ENTRY_OK keeps the text, its ENTRY_CANCEL leaves it
static void take_keyboard(const struct key_press *press, long now) {
	int said = keyboard_key(press);

	if (said == ENTRY_OK) {
		accept(now);
		// not if it was not kept, entry_said saying why
		if (entry_said == NULL || entry_said_ms != now)
			flashed = "OK";
	}
	if (said == ENTRY_CANCEL) {
		abandon();
		flashed = "CANCEL";
	}
	if (said != ENTRY_GOING) {
		flashed_ms = now;
		drawn_ms = now - DRAW_EVERY_MS;
	}
}

static void take_entry(const struct key_press *press, long now) {
	if (!settings.is_morse_typing) {
		take_keyboard(press, now);
		return;
	}
	if (press->key == KEY_ENTER && press->what == BUTTON_PRESSED) {
		if (morse.length > 0)
			morse_abort(&morse);
		else
			accept(now);
	} else if (press->key == KEY_ENTER) {
		morse_abort(&morse);
		entry_delete(&entry);
	}
	if (press->key == KEY_MENU && press->what == BUTTON_PRESSED)
		go(entered_from(page));
}

// a target's menu: UP and DOWN choose, round the ends; ENTER takes the
// item: Frequency opens the dial, PTT makes it the target and goes home,
// Text opens the text, Volume its bar, Read text and Block turn over, Edit
// enters the callsign again, Delete asks; MENU goes back
static void take_target(const struct key_press *press, long now) {
	int items[TARGET_ITEMS];
	int count = target_items(items);

	if (press->what != BUTTON_PRESSED)
		return;
	if (press->key == KEY_MENU) {
		go(opened_from);
		return;
	}
	if (press->key == KEY_UP)
		target_chosen = (target_chosen + count - 1) % count;
	if (press->key == KEY_DOWN)
		target_chosen = (target_chosen + 1) % count;
	if (press->key != KEY_ENTER)
		return;
	switch (items[target_chosen]) {
	case TARGET_FREQUENCY:
		go(PAGE_DIAL);
		break;
	case TARGET_PTT:
		settings.target = opened;
		settings_targets();
		settings_save();
		go(PAGE_HOME);
		break;
	case TARGET_TEXT:
		is_text_sending = 0;
		go(PAGE_TEXT);
		break;
	case TARGET_VOLUME:
		go(PAGE_VOLUME);
		break;
	case TARGET_READ:
		quiet_toggle(opened);
		save_soon(now);
		break;
	case TARGET_BLOCK:
		settings.contacts_blocked ^= 1u << contact_of(opened);
		save_soon(now);
		break;
	case TARGET_EDIT:
		contact_editing = contact_of(opened);
		go(PAGE_CONTACT);
		break;
	case TARGET_DELETE:
		go(PAGE_CONTACT_DELETE);
		break;
	}
}

// DELETE?: ENTER deletes the opened contact and goes back to the menu,
// MENU keeps it
static void take_contact_delete(const struct key_press *press) {
	if (press->what != BUTTON_PRESSED)
		return;
	if (press->key == KEY_ENTER && is_contact(opened)) {
		console_say("CONTACT %s DELETED", settings.contacts[contact_of(opened)]);
		contacts_forget(&settings, contact_of(opened));
		settings_targets();
		settings_save();
		level = MAIN_MENU;
		chosen[MAIN_MENU] = ROW_SETTINGS;
		go(PAGE_MENU);
	}
	if (press->key == KEY_MENU)
		go(PAGE_TARGET);
}

static void take_page(const struct key_press *press, long now) {
	if (page == PAGE_DIAL && is_up_or_down(press)) {
		dial_turn(press, now);
		return;
	}
	if (page == PAGE_VOLUME && is_up_or_down(press)) {
		level_step(&volume_level, press, now);
		return;
	}
	// a target's pages go back to its menu
	if ((page == PAGE_DIAL || page == PAGE_VOLUME)
			&& (press->key == KEY_MENU || press->key == KEY_ENTER)) {
		go(PAGE_TARGET);
		return;
	}
	if (choices_of(page) != NULL) {
		take_choices(choices_of(page), press);
		return;
	}
	// From github.com: ENTER begins, and holds the loop until done
	if (page == PAGE_UPDATE_GITHUB && press->key == KEY_ENTER) {
		update_from_github();
		touched_ms = now;
		return;
	}
	if (page == PAGE_SIDETONE && is_up_or_down(press)) {
		level_step(&sidetone_level, press, now);
		return;
	}
	if (page == PAGE_WPM && is_up_or_down(press)) {
		wpm_step(press, now);
		return;
	}
	if (page == PAGE_VOX && is_up_or_down(press)) {
		vox_step(press, now);
		return;
	}
	if (page == PAGE_VOX && press->key == KEY_ENTER) {
		vox_toggle(now);
		return;
	}
	if (press->key == KEY_MENU || press->key == KEY_ENTER)
		go(PAGE_MENU);
}

static int join_failed(long now);

static void take(const struct key_press *press, long now) {
	say_key(press);
	touched_ms = now;
	if (is_entering(page) && !settings.is_morse_typing) {
		take_keyboard(press, now);
		return;
	}
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
	if (page == PAGE_WIFI_JOINING && press->key == KEY_ENTER
			&& press->what == BUTTON_PRESSED && join_failed(now)) {
		go(PAGE_WIFI_KEY);
		return;
	}
	if (page == PAGE_CONTACT_DELETE) {
		take_contact_delete(press);
		return;
	}
	if (page == PAGE_TARGET) {
		take_target(press, now);
		return;
	}
	if (page == PAGE_TEXT) {
		take_text(press);
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

// a callsign large, as the frequency is, or smaller if it is too wide
static void big_callsign(const char *call, int y) {
	int scale = BIG_SCALE;

	while (scale > 1 && display_text_wide(call, scale) > DISPLAY_WIDE)
		scale--;
	display_text_at((DISPLAY_WIDE - display_text_wide(call, scale)) / 2,
		y + (BIG_SCALE - scale) * FONT_HIGH / 2, scale, call);
}

static void big_frequency(int y) {
	char text[TEXT_ROOM];

	snprintf(text, sizeof(text), "%u", (unsigned)(settings.hz / HZ_A_KHZ));
	big(text, y);
}

// the level's label, its bar, filled from its least to its most, and its
// value at the right
static void level_bar(const struct level *l, int y) {
	char text[TEXT_ROOM];
	int value = (int)*l->value;

	display_text_at(0, y, 1, l->label);
	display_bar(BAR_X, y, BAR_WIDE, (value - l->least) * BAR_WIDE / (l->most - l->least));
	snprintf(text, sizeof(text), "%d", value);
	display_text_at(DISPLAY_WIDE - display_text_wide(text, 1), y, 1, text);
}

static void status_row(const char *middle, int is_online_shown);
static void draw_main(void);

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

	if (page == PAGE_HOME && (entry.length > 0 || morse.length > 0)) {
		snprintf(text, sizeof(text), "> %s %s", entry.text, morse.code);
	} else if (heard_from[0] != 0 && polled_ms - heard_ms < HEARD_SHOWN_MS) {
		snprintf(text, sizeof(text), "%s: %s", heard_from, heard_word);
	} else if (voip_hearing(why, sizeof(why)) && settings.is_local) {
		snprintf(text, sizeof(text), "Heard %s %d", why, local_rssi());
	} else if (voip_hearing(why, sizeof(why))) {
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

// with the WiFi up but the key not yet added at my.sbitx.net, the home
// page says where to add it in place of the frequency and the room
static void draw_home(void) {
	char shown[AUTH_COOKIE_SHOWN_ROOM];

	status_row(settings.callsign, 1);
	if (service_is_unregistered()) {
		service_code(shown);
		line(0, "Register on");
		line(1, "my.sbitx.net with");
		line(2, shown);
	} else if (is_contact(settings.target)) {
		big_callsign(settings.contacts[contact_of(settings.target)], HOME_BIG_Y);
		state_line(HOME_STATE_Y);
	} else {
		big_frequency(HOME_BIG_Y);
		state_line(HOME_STATE_Y);
	}
	level_bar(&volume_level, HOME_VOLUME_Y);
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
	if (of_page == PAGE_WPM)
		snprintf(text, room, "%u", (unsigned)settings.wpm);
	if (of_page == PAGE_SIDETONE)
		snprintf(text, room, settings.sidetone == 0 ? "Off" : "%u", (unsigned)settings.sidetone);
	if (of_page == PAGE_VOX)
		snprintf(text, room, settings.vox == VOX_OFF ? "Off" : "%u", (unsigned)settings.vox);
	if (of_page == PAGE_CALLSIGN)
		snprintf(text, room, "%s", settings.callsign);
	if (of_page == PAGE_GRID)
		snprintf(text, room, "%s", settings.grid);
	// the network joined, or the one being tried, which may be a
	// remembered one and not the one set
	if (of_page == PAGE_WIFI) {
		network_ssid(text, room);
		if (text[0] == 0)
			network_trying(text, room);
		if (text[0] == 0)
			snprintf(text, room, "%s", settings.ssid);
	}
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

	if (level == MAIN_MENU) {
		draw_main();
		return;
	}
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
	network_ssid(address, sizeof(address));
	if (address[0] == 0)
		network_trying(address, sizeof(address));
	if (address[0] == 0)
		snprintf(address, sizeof(address), "%s", settings.ssid);
	snprintf(text, sizeof(text), "Wifi %s", address);
	line(0, text);
	network_trying(address, sizeof(address));
	if (address[0] != 0)
		snprintf(address, sizeof(address), "Joining...");
	else
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

// the help of a page of text entry, or for a while why its text was not kept
static void entry_help_now(const char *const *helps, int count, long now) {
	if (entry_said != NULL && now - entry_said_ms < ENTRY_SAID_MS)
		help(entry_said);
	else
		help(helps[now / HELP_EVERY_MS % count]);
}

// the text's end and a cursor, and under it the chosen key's layer, the
// chosen key in reverse video and those not open left out; no title, for
// room, but the heading in the text's place while it is empty
static void draw_keyboard(const char *heading, long now) {
	char text[TEXT_ROOM];
	int shown = entry.length;
	int left = (DISPLAY_WIDE - ENTRY_KEYBOARD_ACROSS * KEY_PITCH) / 2;
	int first = entry.key_at - entry.key_at % ENTRY_KEYS;
	int at;

	snprintf(text, sizeof(text), "%s_", entry.text);
	while (shown > 0 && display_text_wide(text + entry.length - shown, 1) > DISPLAY_WIDE)
		shown--;
	if (entry.length == 0)
		display_text_at(0, 0, 1, heading);
	else
		display_text_at(0, 0, 1, text + entry.length - shown);
	rule(RULE_Y);
	for (at = first; at < first + ENTRY_KEYS; at++) {
		int x = left + at % ENTRY_KEYBOARD_ACROSS * KEY_PITCH;
		int y = KEYS_Y + at % ENTRY_KEYS / ENTRY_KEYBOARD_ACROSS * KEY_ROW_PITCH;
		char key = entry_key_at(&entry, at);
		const char *label = entry_key_label(key);

		if (!entry_key_is_open(&entry, at))
			continue;
		if (label != NULL)
			display_small_at(x + (KEY_PITCH - display_small_wide(label)) / 2, y, label);
		else
			display_letter_at(x + (KEY_PITCH - FONT_WIDE) / 2, y, 1, key);
		if (at == entry.key_at)
			display_invert(x, y - LETTER_EDGE, KEY_PITCH - 1, FONT_HIGH + 2 * LETTER_EDGE);
	}
	entry_help_now(keyboard_help, KEYBOARD_HELPS, now);
}

// the title, the text's end and a cursor, and the elements keyed of the
// next character, large
static void draw_entry(const char *heading, long now) {
	char text[TEXT_ROOM];
	int shown = entry.length;

	if (!settings.is_morse_typing) {
		draw_keyboard(heading, now);
		return;
	}
	title(heading);
	snprintf(text, sizeof(text), "%s_", entry.text);
	// its end, if it is too long to show whole
	while (shown > 0 && display_text_wide(text + entry.length - shown, 1) > DISPLAY_WIDE)
		shown--;
	line(0, text + entry.length - shown);
	big(morse.code, CODE_Y);
	entry_help_now(entry_help, ENTRY_HELPS, now);
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

// why a join failed, in two lines, from the driver's reason
// (esp_wifi_types.h's wifi_err_reason_t) and the signal: the key's
// handshake not finished is a wrong key, or, heard weakly, perhaps frames
// lost on the way (network.c's TX_POWER)
static void why_not(int reason, int rssi, const char **a, const char **b) {
	int is_weak = rssi != 0 && rssi < WEAK_RSSI;

	*b = "";
	if (reason == 15 || reason == 204 || reason == 23) {
		*a = is_weak ? "Key not taken: weak" : "The key is wrong.";
		*b = is_weak ? "signal, or wrong key" : "ENTER to put it right";
	} else if (reason == 2 || reason == 4 || reason == 200 || reason == 205) {
		*a = "No answer from it:";
		*b = "too far from it?";
	} else if (reason == 201) {
		*a = "Not heard: out of";
		*b = "range, or it is off";
	} else if (reason == 202 || reason == 203) {
		*a = "It turned the";
		*b = "talkie away.";
	} else if (reason == 210 || reason == 211) {
		*a = "Its security is";
		*b = "not one we can do.";
	} else if (reason != 0) {
		*a = "Not joined:";
		*b = "the driver said";
	} else {
		*a = "Not joined.";
		*b = is_weak ? "The signal is weak." : "Is the key right?";
	}
}

// joining the network chosen; joined, with its address; or, once it has
// failed (a failure heard and JOIN_FAILED_MS gone, or JOIN_SHOWN_MS), why,
// and the signal, for the key to be edited with ENTER
static int join_failed(long now) {
	int rssi;
	char text[TEXT_ROOM];

	network_ssid(text, sizeof(text));
	if (network_is_up() && !strcmp(text, wifi_ssid))
		return 0;
	if (now - joining_since_ms >= JOIN_SHOWN_MS)
		return 1;
	return network_failure(wifi_ssid, &rssi) != 0 && now - joining_since_ms >= JOIN_FAILED_MS;
}

static void draw_joining(long now) {
	char text[TEXT_ROOM];
	const char *a;
	const char *b;
	int rssi = 0;
	int reason = network_failure(wifi_ssid, &rssi);

	if (rssi == 0)
		rssi = wifi_rssi;
	title("WiFi");
	snprintf(text, sizeof(text), "%s", wifi_ssid);
	fit(text, DISPLAY_WIDE);
	line(0, text);
	network_ssid(text, sizeof(text));
	if (network_is_up() && !strcmp(text, wifi_ssid)) {
		line(1, "Joined");
		network_address(text, sizeof(text));
		line(2, text);
		help("MENU: BACK");
		return;
	}
	if (!join_failed(now)) {
		line(1, "Joining...");
		help("MENU: BACK");
		return;
	}
	why_not(reason, rssi, &a, &b);
	if (reason > 0 && !strcmp(b, "the driver said"))
		snprintf(text, sizeof(text), "reason %d", reason);
	else
		snprintf(text, sizeof(text), "%s", b);
	line(1, a);
	line(2, text);
	if (rssi != 0) {
		snprintf(text, sizeof(text), "Signal %d dBm%s", rssi, rssi < WEAK_RSSI ? ", weak" : "");
		line(3, text);
	}
	help(now / HELP_EVERY_MS % 2 ? "MENU: BACK" : "ENTER: EDIT THE KEY");
}

// new firmware coming in from the portal (update.cpp): how far, on a
// bar, and what to do when it is in or has failed
static void draw_update(const char *said, int is_github) {
	char text[TEXT_ROOM];
	int percent = update_percent();
	int wide;

	title(is_github ? "From github.com" : "Update locally");
	if (!strncmp(said, "DONE", 4)) {
		line(0, "Updated.");
		line(1, "Restarting...");
		if (!is_github) {
			line(2, "Close the page");
			line(3, "on the phone.");
		}
		return;
	}
	if (!strcmp(said, "CHECKING")) {
		line(0, "Asking github.com");
		line(1, "for the newest...");
		return;
	}
	if (!strcmp(said, "UP TO DATE")) {
		line(0, "Up to date:");
		line(1, "this is the newest.");
		help("MENU: BACK");
		return;
	}
	if (!strncmp(said, "FAILED", 6)) {
		line(0, "Update failed:");
		snprintf(text, sizeof(text), "%s", said + strlen("FAILED: "));
		fit(text, DISPLAY_WIDE);
		line(1, text);
		line(2, "Old firmware kept.");
		help(is_github ? "ENTER: AGAIN   MENU: BACK" : "TRY AGAIN, OR MENU: CLOSE");
		return;
	}
	line(0, "Receiving...");
	wide = DISPLAY_WIDE - display_text_wide("100%", 1) - VALUE_GAP;
	display_bar(0, line_y(1), wide, percent * wide / 100);
	snprintf(text, sizeof(text), "%d%%", percent);
	display_text_at(DISPLAY_WIDE - display_text_wide(text, 1), line_y(1), 1, text);
	if (!is_github) {
		line(2, "Keep the phone");
		line(3, "page open.");
	}
}

// drawn and sent at once, for update.cpp, which holds the loop up while
// the file comes in
void screen_update_shown(void) {
	char said[UPDATE_TEXT_ROOM];
	int i;

	update_said(said, sizeof(said));
	display_clear();
	draw_update(said, page == PAGE_UPDATE_GITHUB);
	for (i = 0; i < DISPLAY_ROWS; i++)
		display_poll();
}

// From github.com: what ENTER does, or how the update goes
static void draw_github(void) {
	char said[UPDATE_TEXT_ROOM];

	update_said(said, sizeof(said));
	if (said[0] != 0) {
		draw_update(said, 1);
		return;
	}
	title("From github.com");
	line(0, "Get the newest");
	line(1, "firmware from");
	line(2, "github.com?");
	help(network_is_up() ? "ENTER: BEGIN   MENU: BACK" : "NO WIFI   MENU: BACK");
}

// the portal open, Configure or Update locally: what to join and where
// to go, and for Update the code it asks; then how a file sent goes
static void draw_setup(int is_update) {
	char text[TEXT_ROOM];

	update_said(text, sizeof(text));
	if (is_update && text[0] != 0) {
		draw_update(text, 0);
		return;
	}
	title(is_update ? "Update locally" : "Configure");
	if (!portal_is_open()) {
		line(1, "It did not start.");
		help("MENU: BACK");
		return;
	}
	line(0, "On a phone, join");
	portal_name(text, sizeof(text));
	line(1, text);
	line(2, "open 192.168.4.1");
	if (is_update) {
		snprintf(text, sizeof(text), "Update code %d", update_code());
		fit(text, DISPLAY_WIDE);
		line(3, text);
	}
	if (network_ap_clients() > 0) {
		snprintf(text, sizeof(text), "%d JOINED   MENU: CLOSE", network_ap_clients());
		help(text);
	} else {
		help("MENU: CLOSE");
	}
}

// the key to add at my.sbitx.net, and whether it is in yet
static void draw_activate(void) {
	char shown[AUTH_COOKIE_SHOWN_ROOM];
	char text[TEXT_ROOM];

	service_code(shown);
	// new from its box: where to add it, and the code to type there
	if (settings.callsign[0] == 0) {
		title("Activate");
		line(0, "On a phone, go to");
		line(1, "my.sbitx.net/wbitx");
		line(2, "and enter the code");
		line(3, shown);
		help(network_is_up() ? "WAITING FOR IT..." : "JOIN A WIFI FIRST");
		return;
	}
	title("Device key");
	line(0, shown);
	if (service_is_online()) {
		line(1, "Registered");
	} else {
		service_brief(text, sizeof(text));
		line(1, text);
		line(2, "Register it on");
		line(3, "my.sbitx.net");
	}
	help("MENU: BACK");
}

// when party last spoke or was spoken to, HH:MM UTC, or nothing
static void last_time(const char *party, char *text, int room) {
	long long at = text_last(&kept, party);

	text[0] = 0;
	if (at > 0)
		snprintf(text, room, "%02d:%02d", (int)(at / 3600 % 24), (int)(at / 60 % 60));
}

// the main menu: Settings..., the slots with their kHz, the contacts with
// the time, UTC, each last spoke or was spoken to, and Add Callsign...
// while there is room; the target marked with a * after its name
static void draw_main(void) {
	char name[TEXT_ROOM];
	char value[TEXT_ROOM];
	int rows = main_rows();
	int *top = &menu_top[MAIN_MENU];
	int at = chosen[MAIN_MENU];
	int target;
	int i;

	if (at >= rows)
		at = chosen[MAIN_MENU] = rows - 1;
	if (at < *top)
		*top = at;
	if (at >= *top + MENU_ROWS)
		*top = at - MENU_ROWS + 1;
	status_row("Menu", 0);
	for (i = *top; i < rows && i < *top + MENU_ROWS; i++) {
		target = row_target(i);
		value[0] = 0;
		if (i == ROW_SLEEP) {
			snprintf(name, sizeof(name), "Sleep");
		} else if (i == ROW_SETTINGS) {
			snprintf(name, sizeof(name), "Settings...");
		} else if (target < 0) {
			snprintf(name, sizeof(name), "Add Callsign...");
		} else {
			target_name((uint32_t)target, name, sizeof(name));
			if ((uint32_t)target == settings.target)
				snprintf(name + strlen(name), sizeof(name) - strlen(name), " *");
			if (!is_contact((uint32_t)target))
				snprintf(value, sizeof(value), "%u", (unsigned)(settings.slot_hz[target] / HZ_A_KHZ));
			else if (is_blocked((uint32_t)target))
				snprintf(value, sizeof(value), "Blocked");
			else
				last_time(settings.contacts[contact_of((uint32_t)target)], value, sizeof(value));
		}
		item(i - *top, i == at, name, value);
	}
}

// a target's menu, its name as the title, five rows, scrolled to keep the
// chosen shown
static void draw_target(void) {
	static const char *const names[TARGET_ITEMS] = {
		"Frequency", "PTT", "Text...", "Volume", "Read text", "Block", "Edit callsign", "Delete...",
	};
	int items[TARGET_ITEMS];
	int count = target_items(items);
	char text[TEXT_ROOM];
	int i;

	if (target_chosen >= count)
		target_chosen = count - 1;
	if (target_chosen < target_top)
		target_top = target_chosen;
	if (target_chosen >= target_top + MENU_ROWS)
		target_top = target_chosen - MENU_ROWS + 1;
	target_name(opened, text, sizeof(text));
	status_row(text, 0);
	for (i = target_top; i < count && i < target_top + MENU_ROWS; i++) {
		text[0] = 0;
		if (items[i] == TARGET_FREQUENCY)
			snprintf(text, sizeof(text), "%u", (unsigned)(settings.slot_hz[opened] / HZ_A_KHZ));
		if (items[i] == TARGET_VOLUME)
			snprintf(text, sizeof(text), "%u", (unsigned)settings.volume);
		if (items[i] == TARGET_READ)
			snprintf(text, sizeof(text), is_quiet(opened) ? "Off" : "On");
		if (items[i] == TARGET_BLOCK)
			snprintf(text, sizeof(text), is_blocked(opened) ? "On" : "Off");
		item(i - target_top, i == target_chosen, names[items[i]], text);
	}
}

// a line put after the n there are, the oldest let go when they are full;
// how many there are now
static int text_line_add(int n, const char *line, int length, int is_header) {
	if (n == TEXT_LINES) {
		n--;
		memmove(text_lines, text_lines[1], sizeof(text_lines[0]) * n);
		memmove(text_is_header, text_is_header + 1, sizeof(text_is_header[0]) * n);
	}
	snprintf(text_lines[n], TEXT_LINE_ROOM, "%.*s", length, line);
	text_is_header[n] = is_header;
	return n + 1;
}

// the Text page's lines: the party's runs, each a header from>to and its
// words wrapped to the display's width; how many
static int text_wrap(const char *party) {
	int found[TEXT_RUNS];
	int runs = text_runs_of(&kept, party, found, TEXT_RUNS);
	int n = 0;
	int i;

	for (i = 0; i < runs; i++) {
		const struct text_run *run = text_run(&kept, found[i]);
		const char *at = run->words;
		char header[TEXT_LINE_ROOM];

		snprintf(header, sizeof(header), "%s>%s", run->from, run->to);
		n = text_line_add(n, header, (int)strlen(header), 1);
		while (*at != 0) {
			char line[TEXT_LINE_ROOM];
			int length = 0;
			int fits = 0;

			// as many words as fit, or as much of a long one
			while (at[length] != 0 && length < TEXT_LINE_ROOM - 1) {
				line[length] = at[length];
				line[length + 1] = 0;
				if (display_text_wide(line, 1) > DISPLAY_WIDE)
					break;
				length++;
				if (at[length] == ' ' || at[length] == 0)
					fits = length;
			}
			if (fits == 0)
				fits = length > 0 ? length : 1;
			n = text_line_add(n, at, fits, 0);
			at += fits;
			while (*at == ' ')
				at++;
		}
	}
	return n;
}

static const char *text_help[] = {
	"UP/DOWN: SCROLL",
	"PTT: SEND",
	"MENU: BACK",
};

#define TEXT_HELPS ((int)(sizeof(text_help) / sizeof(text_help[0])))

// the Text page: reading, the party's newest lines, or as far back as
// scrolled, each header in reverse video; sending, the word being entered,
// as the callsign is, headed with whom it goes to
static void draw_text(long now) {
	char party[TEXT_PARTY_ROOM];
	char heading[TEXT_ROOM];
	int count;
	int first;
	int i;

	target_party(opened, party, sizeof(party));
	if (is_text_sending) {
		snprintf(heading, sizeof(heading), "To %s", party);
		draw_entry(heading, now);
		return;
	}
	count = text_wrap(party);
	if (text_back > count - TEXT_SHOWN)
		text_back = count > TEXT_SHOWN ? count - TEXT_SHOWN : 0;
	first = count - TEXT_SHOWN - text_back;
	if (first < 0)
		first = 0;
	title(party);
	if (count == 0)
		line(1, "Nothing yet");
	for (i = 0; i < TEXT_SHOWN && first + i < count; i++) {
		if (text_is_header[first + i])
			item(i, 1, text_lines[first + i], "");
		else
			line(i, text_lines[first + i]);
	}
	help(text_help[now / HELP_EVERY_MS % TEXT_HELPS]);
}

static void draw_contact_delete(void) {
	title("Delete?");
	if (is_contact(opened))
		big(settings.contacts[contact_of(opened)], PAGE_BIG_Y);
	help("ENTER: DELETE  MENU: NO");
}

// the voice's level against the level that keys, on the VOX page
static void vox_meter(int y) {
	int size;
	int threshold;
	int mark;

	microphone_vox_meter(&size, &threshold);
	if (size > VOX_METER_MOST)
		size = VOX_METER_MOST;
	display_text_at(0, y, 1, "Mic");
	display_bar(BAR_X, y, BAR_WIDE, size * BAR_WIDE / VOX_METER_MOST);
	if (settings.vox == VOX_OFF)
		return;
	if (threshold > VOX_METER_MOST)
		threshold = VOX_METER_MOST;
	mark = BAR_X + threshold * (BAR_WIDE - 1) / VOX_METER_MOST;
	display_fill(mark, y - 2, 1, FONT_HIGH + 4);
}

static void draw_vox(void) {
	char text[TEXT_ROOM];

	title("VOX");
	if (settings.vox == VOX_OFF)
		snprintf(text, sizeof(text), "Off");
	else
		snprintf(text, sizeof(text), "%u", (unsigned)settings.vox);
	big(text, PAGE_BIG_Y);
	vox_meter(line_y(3));
	if (settings.vox == VOX_OFF)
		help("ENTER: ON   MENU: BACK");
	else if (microphone_vox())
		help("KEYED");
	else
		help("ENTER: OFF  UP/DN: LEVEL");
}

static void draw(long now) {
	char text[TEXT_ROOM];

	display_clear();
	if (flashed != NULL && now - flashed_ms < FLASH_MS) {
		big(flashed, (DISPLAY_HIGH - FONT_MAIN_HIGH * BIG_SCALE) / 2);
		return;
	}
	flashed = NULL;
	if (page == PAGE_HOME)
		draw_home();
	if (page == PAGE_MENU)
		draw_menu();
	if (page == PAGE_DIAL && !is_contact(opened)) {
		snprintf(text, sizeof(text), "F%u, kHz", (unsigned)opened + 1);
		title(text);
		snprintf(text, sizeof(text), "%u", (unsigned)(settings.slot_hz[opened] / HZ_A_KHZ));
		big(text, PAGE_BIG_Y);
		help("UP/DOWN   MENU: BACK");
	}
	if (page == PAGE_VOLUME) {
		title("Volume");
		level_bar(&volume_level, line_y(1));
		help("UP/DOWN   MENU: BACK");
	}
	if (page == PAGE_TARGET)
		draw_target();
	if (choices_of(page) != NULL)
		draw_choices(choices_of(page));
	if (page == PAGE_WPM) {
		title("Morse speed, WPM");
		snprintf(text, sizeof(text), "%u", (unsigned)settings.wpm);
		big(text, PAGE_BIG_Y);
		help("UP/DOWN   MENU: BACK");
	}
	if (page == PAGE_SIDETONE) {
		title("Sidetone");
		level_bar(&sidetone_level, line_y(1));
		help("UP/DOWN   MENU: BACK");
	}
	if (page == PAGE_VOX)
		draw_vox();
	if (page == PAGE_CALLSIGN)
		draw_entry("Callsign", now);
	if (page == PAGE_GRID)
		draw_entry("Grid square", now);
	if (page == PAGE_CONTACT)
		draw_entry(contact_editing < 0 ? "New contact" : "Contact", now);
	if (page == PAGE_CONTACT_DELETE)
		draw_contact_delete();
	if (page == PAGE_TEXT)
		draw_text(now);
	if (page == PAGE_WIFI)
		draw_wifi(now);
	if (page == PAGE_WIFI_KEY) {
		char prompt[TEXT_ROOM + SETTINGS_SSID_ROOM];

		snprintf(prompt, sizeof(prompt), "Enter Key for %s", wifi_ssid);
		fit(prompt, DISPLAY_WIDE);
		draw_entry(prompt, now);
	}
	if (page == PAGE_WIFI_JOINING)
		draw_joining(now);
	if (page == PAGE_SETUP || page == PAGE_UPDATE_LOCAL)
		draw_setup(page == PAGE_UPDATE_LOCAL);
	if (page == PAGE_UPDATE_GITHUB)
		draw_github();
	if (page == PAGE_INFO)
		draw_info();
	if (page == PAGE_ACTIVATE)
		draw_activate();
}

void screen_poll(long now) {
	struct key_press press;
	char letter;
	char hearing[TEXT_PARTY_ROOM];
	int is_hearing;
	int is_keying;
	int ms;

	polled_ms = now;

	// MENU held SLEEP_HOLD_MS on the home page: asleep once it is let go
	if (page == PAGE_HOME && keys_is_down(KEY_MENU)) {
		if (!menu_down_ms)
			menu_down_ms = now;
		else if (now - menu_down_ms >= SLEEP_HOLD_MS) {
			menu_down_ms = 0;
			asleep();
		}
	} else {
		menu_down_ms = 0;
	}

	{
		int is_kb = is_entering(page) && !settings.is_morse_typing;

		keys_set_kind(KEY_ENTER, is_kb ? BUTTON_HOLDING : BUTTON_HOLDING_REPEATING);
	}
	if (keys_poll(now, &press))
		take(&press, now);
	// on a network with no callsign: the code and where to add it, once;
	// told its callsign (enrol.c), home
	if (enrol_is_waiting() && !is_activate_shown
			&& (page == PAGE_HOME || page == PAGE_WIFI_JOINING)) {
		is_activate_shown = 1;
		go(PAGE_ACTIVATE);
	}
	if (enrol_took()) {
		is_activate_shown = 0;
		go(PAGE_HOME);
	}
	if (portal_joined(wifi_ssid, sizeof(wifi_ssid))) {
		joining_since_ms = now;
		touched_ms = now;
		go(PAGE_WIFI_JOINING);
	}
	// the PTT key, on the Text page, turns between reading and sending
	if (page == PAGE_TEXT) {
		int is_key = ptt_key_is_down();

		ptt_key_lend(1);
		if (is_key && !was_ptt_key) {
			if (is_text_sending && entry.length > 0)
				send_entry(now);
			is_text_sending = !is_text_sending;
			entry_begin(&entry, ENTRY_ANY, "", TEXT_WORD_ROOM - 1);
			morse_begin(&morse, (int)settings.wpm);
			touched_ms = now;
			drawn_ms = now - DRAW_EVERY_MS;
		}
		was_ptt_key = is_key;
	}
	is_keying = is_entering(page) && settings.is_morse_typing;
	// the paddles, each element heard as it begins
	// the speaker held ready for a tone: on the home page only while the
	// paddles are in use, the rest of the time it rests
	speaker_ready((is_keying && (page != PAGE_HOME || keys_is_down(KEY_UP) || keys_is_down(KEY_DOWN)
			|| morse.state != MORSE_IDLE || morse.length > 0 || entry.length > 0))
		|| page == PAGE_SIDETONE || text_play_is_busy(&play));
	if (is_keying) {
		switch (morse_keys(&morse, keys_is_down(KEY_UP), keys_is_down(KEY_DOWN), now, &letter)) {
		case MORSE_DIT:
			speaker_tone((int)(MORSE_DIT_UNITS * morse.unit_ms));
			drawn_ms = now - DRAW_EVERY_MS;
			touched_ms = now;
			break;
		case MORSE_DAH:
			speaker_tone((int)(MORSE_DAH_UNITS * morse.unit_ms));
			drawn_ms = now - DRAW_EVERY_MS;
			touched_ms = now;
			break;
		case MORSE_LETTER:
			if (strchr(entry.set, letter))
				entry_put(&entry, letter);
			drawn_ms = now - DRAW_EVERY_MS;
			break;
		case MORSE_WORD:
			// the gap between words sends the word, from the home page to
			// the target, from the Text page to its target
			if (page == PAGE_HOME) {
				send_word(settings.target, entry.text, now);
				entry_begin(&entry, ENTRY_ANY, "", TEXT_WORD_ROOM - 1);
			}
			if (page == PAGE_TEXT && entry.length > 0)
				send_entry(now);
			drawn_ms = now - DRAW_EVERY_MS;
			break;
		case MORSE_UNKNOWN:
			drawn_ms = now - DRAW_EVERY_MS;
			break;
		}
	}
	// words heard, played while no voice is heard and nothing is keyed
	is_hearing = voip_hearing(hearing, sizeof(hearing));
	ms = text_play_next(&play, now, !is_hearing && !ptt_is_down()
		&& !keys_is_down(KEY_UP) && !keys_is_down(KEY_DOWN));
	if (ms > 0)
		speaker_tone(ms);
	// when each last spoke: the target as the PTT goes down, a voice as it
	// is first heard
	if (ptt_is_down() && !was_ptt) {
		char party[TEXT_PARTY_ROOM];

		target_party(settings.target, party, sizeof(party));
		text_touch(&kept, party, now);
	}
	was_ptt = ptt_is_down();
	if (is_hearing && strcmp(hearing, was_hearing))
		text_touch(&kept, hearing, now);
	snprintf(was_hearing, sizeof(was_hearing), "%s", is_hearing ? hearing : "");
	if (!is_begun) {
		is_begun = 1;
		touched_ms = now;
		// new from its box: the WiFi page, the key typed on the keyboard
		if (settings.ssid[0] == 0 && known_count(&settings) == 0) {
			network_scan_begin();
			wifi_chosen = 0;
			wifi_top = 0;
			go(PAGE_WIFI);
		}
	}
	if (page != PAGE_HOME && page != PAGE_INFO && page != PAGE_SETUP
			&& page != PAGE_UPDATE_GITHUB && page != PAGE_UPDATE_LOCAL
			&& page != PAGE_ACTIVATE
			// a talkie being set up, with no network yet, stays on the WiFi
			// pages
			&& !(settings.ssid[0] == 0 && known_count(&settings) == 0
				&& (page == PAGE_WIFI || page == PAGE_WIFI_KEY || page == PAGE_WIFI_JOINING))
			&& now - touched_ms >= MENU_IDLE_MS)
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

// my.sbitx.net's time, said as the talkie comes on line
void screen_time_heard(long long unix_s, long now) {
	text_clock_set(&kept, unix_s, now);
	console_say("TIME %lld", unix_s);
}

// a word heard: kept, and played as Morse unless its slot or contact has
// Read text off; nothing of a blocked contact's
void screen_text_heard(const char *from, const char *to, int wpm, const char *word, long now) {
	int contact = contacts_find(&settings, from);
	int is_quiet_now = 0;
	int i;

	if (contact >= 0 && is_blocked(SETTINGS_TARGET_CONTACT + (uint32_t)contact))
		return;
	text_add(&kept, from, to, word);
	text_touch(&kept, from, now);
	if (to[0] >= '0' && to[0] <= '9') {
		text_touch(&kept, to, now);
		for (i = 0; i < SETTINGS_SLOTS; i++)
			if (settings.slot_hz[i] / HZ_A_KHZ == (uint32_t)atol(to) && is_quiet((uint32_t)i))
				is_quiet_now = 1;
	} else if (contact >= 0) {
		is_quiet_now = is_quiet(SETTINGS_TARGET_CONTACT + (uint32_t)contact);
	}
	if (!is_quiet_now)
		text_play_add(&play, word, wpm);
	snprintf(heard_from, sizeof(heard_from), "%s", from);
	snprintf(heard_word, sizeof(heard_word), "%s", word);
	heard_ms = now;
	drawn_ms = now - DRAW_EVERY_MS;
}

// the word sent first and not yet answered, kept as sent, or in brackets
// if nobody had it
static void pending_done(int is_had) {
	char shown[TEXT_WORD_ROOM + 2];

	if (pending_count == 0)
		return;
	snprintf(shown, sizeof(shown), is_had ? "%s" : "[%s]", pending[0].word);
	text_add(&kept, settings.callsign, pending[0].to, shown);
	pending_count--;
	memmove(pending, pending + 1, sizeof(pending[0]) * (size_t)pending_count);
}

// TEXT SENT: how many had the word
void screen_text_sent(int count, long now) {
	pending_done(count > 0);
	drawn_ms = now - DRAW_EVERY_MS;
}

// a word refused, too fast or not a word: not sent
void screen_text_refused(const char *why, long now) {
	(void)why;
	pending_done(0);
	drawn_ms = now - DRAW_EVERY_MS;
}
