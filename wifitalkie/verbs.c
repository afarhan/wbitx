// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ashhar Farhan
//
// verbs.c — what can be typed at the talkie from the laptop, a verb a line.
//
// For this first talkie, the console is its only way to be set: there are
// no buttons but the PTT and no screen. A verb is a word, in capitals or
// not, with what it needs after it; a word with spaces in it, an SSID or a
// password, goes in double quotes. Each verb answers on a line of its own,
// in capitals, and says what it was set to, or why not.
//
//     WIFI <ssid> <password>    join this access point, and keep it
//     SCAN                      the access points in hearing
//     RADIO ON|OFF              the WiFi radio itself, on or off
//     CALL <callsign>           the owner's callsign at my.sbitx.net
//     GRID <locator>            the grid said to my.sbitx.net
//     FREQ <hz>                 the dial: its kilohertz is the room
//     SERVER <host> [<port>]    my.sbitx.net 8081 unless set
//     KEY                       the cookie, to add the talkie on the website
//     NEW_KEY                   a new cookie; typed twice within 5 s
//     VOIP ONLINE|OFFLINE       on line to the repeater, or not
//     LINK INTERNET|LOCAL       by my.sbitx.net, or talkie to talkie by
//                               ESP-NOW off the internet (local.c)
//     PTT ON|OFF                speak, as ENTER held on the home page does
//     TONE ON|OFF               1 kHz in place of the microphone
//     SIDETONE [0-10]           the Morse sidetone's level, and heard for 500 ms
//     VOL <0-100>  MIC <0-100>  the speaker's level and the microphone's
//     LEVELS ON|OFF             the microphone's levels each second
//     VOX <1-10>|OFF            speak by the voice, without the PTT: the
//                               sensitivity, 10 the most (vox.c)
//     GATE ON|OFF               the gaps between words as comfort noise
//     KEYS                      each key's pin as it is now, DOWN or up
//     PIN <0-21>                how a GPIO is set now: its pulls, input,
//                               output, function, and the level read
//     SCREEN                    the page shown, the last key, the display's
//                               pages sent and failed
//     RECORD                    3 s of the microphone, in hex, after
//     MICPIN <0-4>              read the microphone on another ADC1 pin
//     DISPLAY ON|OFF            the OLED, and its I2C, on or off
//     AUDIO ON|OFF              GPIO1: the preamplifiers and the amplifier
//     BATTERY                   the cell's volts, or USB
//     BATTERY <volts>|CLEAR     the cell is this, by a meter: the board's
//                               offset is set to make it so (battery.c)
//     STATUS                    all of it
//     SLEEP                     sleep until a key is pressed (sleep.c)
//     RESTART                   start the talkie again
//
// A verb that changes what the line to my.sbitx.net is made with, CALL,
// SERVER, NEW_KEY and VOIP, has it made again. Whatever is set is kept
// across power-ups, but PTT, TONE, LEVELS, RECORD, MICPIN, DISPLAY, AUDIO and RADIO, which are for now.

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "esp_system.h"
#include "driver/gpio.h"

#include "auth.h"
#include "battery.h"
#include "console.h"
#include "display.h"
#include "keys.h"
#include "microphone.h"
#include "local.h"
#include "network.h"
#include "ptt.h"
#include "screen.h"
#include "service.h"
#include "settings.h"
#include "sleep.h"
#include "speaker.h"
#include "verbs.h"
#include "voip.h"
#include "vox.h"

#define SIDETONE_HEARD_MS 500

#define WORD_ROOM 80
#define SAID_ROOM 200
#define TEXT_ROOM 96
#define NEW_KEY_AGAIN_MS 5000
#define MS_A_SECOND 1000
#define HZ_MOST 4000000000UL

static long new_key_asked_ms = -NEW_KEY_AGAIN_MS;
static int is_levels_shown;
static long levels_shown_ms;

// the next word of a line, a word in double quotes taken whole; 0 when there
// is none
static int next_word(const char **at, char *word, int room) {
	const char *p = *at;
	int used = 0;
	char end = ' ';

	while (*p == ' ' || *p == '\t')
		p++;
	if (*p == 0)
		return 0;
	if (*p == '"') {
		end = '"';
		p++;
	}
	while (*p != 0 && *p != end && !(end == ' ' && *p == '\t')) {
		if (used < room - 1)
			word[used++] = *p;
		p++;
	}
	if (*p == end)
		p++;
	word[used] = 0;
	*at = p;
	return 1;
}

static void capitals(char *word) {
	int i;

	for (i = 0; word[i] != 0; i++)
		word[i] = (char)toupper((unsigned char)word[i]);
}

static int is_on_off(const char *word, int *is_on) {
	char given[WORD_ROOM];

	snprintf(given, sizeof(given), "%s", word);
	capitals(given);
	if (!strcmp(given, "ON") || !strcmp(given, "ONLINE")) {
		*is_on = 1;
		return 1;
	}
	if (!strcmp(given, "OFF") || !strcmp(given, "OFFLINE")) {
		*is_on = 0;
		return 1;
	}
	return 0;
}

static void say_battery(void) {
	int millivolts;
	int what = microphone_battery(&millivolts);
	int code;
	int pin_millivolts;

	microphone_battery_raw(&code, &pin_millivolts);
	if (what == MICROPHONE_BATTERY)
		console_say("BATTERY %d.%02d V (CODE %d, PIN %d MV, OFFSET %d MV)", millivolts / 1000,
			millivolts % 1000 / 10, code, pin_millivolts, (int)settings.battery_offset_mv);
	else if (what == MICROPHONE_USB)
		console_say("BATTERY ON USB");
	else
		console_say("BATTERY NOT READ YET");
}

// BATTERY <volts>, as 3.95, from a meter on the cell: the offset that
// makes the reading so, kept; BATTERY CLEAR puts it back to 0
static void battery_set(const char *word) {
	int millivolts;
	int code;
	int pin_millivolts;
	char *end;
	double volts;

	if (!strcmp(word, "CLEAR") || !strcmp(word, "clear")) {
		settings.battery_offset_mv = 0;
		settings_save();
		say_battery();
		return;
	}
	volts = strtod(word, &end);
	if (*end != 0 || volts < 3.0 || volts > 4.5) {
		console_say("BATTERY: <VOLTS> FROM 3.0 TO 4.5, OR CLEAR");
		return;
	}
	if (microphone_battery(&millivolts) != MICROPHONE_BATTERY) {
		console_say("BATTERY NOT READ YET");
		return;
	}
	microphone_battery_raw(&code, &pin_millivolts);
	settings.battery_offset_mv = battery_offset_for(pin_millivolts, (int)(volts * 1000 + 0.5));
	settings_save();
	say_battery();
}

static void say_status(void) {
	char text[TEXT_ROOM];
	char shown[AUTH_COOKIE_SHOWN_ROOM];
	struct microphone_levels levels;

	console_say("LINK %s", settings.is_local ? "LOCAL" : "INTERNET");
	network_describe(text, sizeof(text));
	console_say("WIFI %s", text);
	console_say("CALL %s  GRID %s  FREQ %u", settings.callsign, settings.grid, (unsigned)settings.hz);
	console_say("SERVER %s %u", settings.host, (unsigned)settings.port);
	auth_cookie_show(settings.cookie, shown);
	console_say("KEY %s", shown);
	service_describe(text, sizeof(text));
	console_say("SERVICE %s", text);
	if (settings.is_voip_online)
		voip_describe(text, sizeof(text));
	else
		snprintf(text, sizeof(text), "OFFLINE");
	console_say("VOIP %s", text);
	if (settings.is_local) {
		local_describe(text, sizeof(text));
		console_say("LOCAL %s", text);
	}
	voip_counts(text, sizeof(text));
	console_say("PACKETS %s", text);
	speaker_describe(text, sizeof(text));
	console_say("SPEAKER %s", text);
	microphone_levels(&levels);
	if (settings.vox == VOX_OFF)
		console_say("VOX OFF");
	else
		console_say("VOX %u", (unsigned)settings.vox);
	console_say("GATE %s", settings.is_gate ? "ON" : "OFF");
	console_say("MIC %u, PEAK %d RMS %d, ADC %d A SECOND (%d READ, %d STRANGE, %u RESTARTS)",
		(unsigned)settings.mic, levels.peak, levels.rms, levels.samples_a_second,
		levels.results, levels.strangers, levels.restarts);
	say_battery();
}

static void level_set(const char *what, const char *word, uint32_t *level) {
	char *end;
	long value = strtol(word, &end, 10);

	if (*word == 0 || *end != 0 || value < 0 || value > SETTINGS_LEVEL_MOST) {
		console_say("%s: 0 TO 100", what);
		return;
	}
	*level = (uint32_t)value;
	settings_save();
	console_say("%s %u", what, (unsigned)*level);
}

static void new_key(long now) {
	if (now - new_key_asked_ms > NEW_KEY_AGAIN_MS) {
		new_key_asked_ms = now;
		console_say("NEW_KEY: TYPE IT AGAIN WITHIN 5 S TO MAKE A NEW KEY");
		return;
	}
	new_key_asked_ms = -NEW_KEY_AGAIN_MS;
	service_new_key();
	console_say("NEW_KEY MAKING; ADD IT AT MY.SBITX.NET IN PLACE OF THE OLD");
}

static void freq_set(const char *word) {
	char *end;
	unsigned long hz = strtoul(word, &end, 10);

	if (*word == 0 || *end != 0 || hz == 0 || hz > HZ_MOST) {
		console_say("FREQ: IN HZ, FREQ 7074000");
		return;
	}
	settings.hz = (uint32_t)hz;
	// the target's slot with it, if the target is a slot
	if (settings.target < SETTINGS_SLOTS)
		settings.slot_hz[settings.target] = settings.hz;
	settings_save();
	console_say("FREQ %u, ROOM %u KHZ", (unsigned)settings.hz,
		(unsigned)((settings.hz + 500) / 1000));
}

void verbs_do(const char *line, long now) {
	const char *at = line;
	char verb[WORD_ROOM];
	char first[WORD_ROOM] = "";
	char second[WORD_ROOM] = "";
	char said[SAID_ROOM];
	int is_on;

	if (!next_word(&at, verb, sizeof(verb)))
		return;
	capitals(verb);
	next_word(&at, first, sizeof(first));
	next_word(&at, second, sizeof(second));

	if (!strcmp(verb, "WIFI")) {
		if (first[0] == 0) {
			network_describe(first, sizeof(first));
			console_say("WIFI %s", first);
			return;
		}
		snprintf(settings.ssid, sizeof(settings.ssid), "%s", first);
		snprintf(settings.password, sizeof(settings.password), "%s", second);
		settings_save();
		network_join();
		return;
	}
	if (!strcmp(verb, "RADIO") && is_on_off(first, &is_on)) {
		if (is_on)
			network_on();
		else
			network_off();
		return;
	}
	if (!strcmp(verb, "SCAN")) {
		network_scan();
		return;
	}
	if (!strcmp(verb, "CALL")) {
		capitals(first);
		snprintf(settings.callsign, sizeof(settings.callsign), "%s", first);
		settings_save();
		service_restart();
		console_say("CALL %s", settings.callsign);
		return;
	}
	if (!strcmp(verb, "GRID")) {
		snprintf(settings.grid, sizeof(settings.grid), "%s", first);
		settings_save();
		console_say("GRID %s", settings.grid);
		return;
	}
	if (!strcmp(verb, "FREQ")) {
		freq_set(first);
		return;
	}
	if (!strcmp(verb, "SERVER")) {
		if (first[0] == 0) {
			console_say("SERVER %s %u", settings.host, (unsigned)settings.port);
			return;
		}
		snprintf(settings.host, sizeof(settings.host), "%s", first);
		settings.port = SETTINGS_DEFAULT_PORT;
		if (second[0] != 0)
			settings.port = (uint32_t)strtoul(second, NULL, 10);
		settings_save();
		service_restart();
		console_say("SERVER %s %u", settings.host, (unsigned)settings.port);
		return;
	}
	if (!strcmp(verb, "KEY")) {
		auth_cookie_show(settings.cookie, first);
		console_say("KEY %s", first);
		return;
	}
	if (!strcmp(verb, "NEW_KEY")) {
		new_key(now);
		return;
	}
	if (!strcmp(verb, "VOIP") && is_on_off(first, &is_on)) {
		settings.is_voip_online = (uint32_t)is_on;
		settings_save();
		service_restart();
		if (is_on)
			console_say("VOIP ONLINE");
		else
			console_say("VOIP OFFLINE");
		return;
	}
	if (!strcmp(verb, "PTT") && is_on_off(first, &is_on)) {
		ptt_set(is_on);
		return;
	}
	if (!strcmp(verb, "TONE") && is_on_off(first, &is_on)) {
		voip_tone(is_on);
		if (is_on)
			console_say("TONE ON");
		else
			console_say("TONE OFF");
		return;
	}
	if (!strcmp(verb, "SIDETONE")) {
		if (first[0] != 0 && isdigit((unsigned char)first[0]) && atoi(first) <= SETTINGS_SIDETONE_MOST) {
			settings.sidetone = (uint32_t)atoi(first);
			settings_save();
		}
		speaker_tone(SIDETONE_HEARD_MS);
		console_say("SIDETONE %u", (unsigned)settings.sidetone);
		return;
	}
	if (!strcmp(verb, "VOL")) {
		level_set("VOL", first, &settings.volume);
		return;
	}
	if (!strcmp(verb, "MIC")) {
		level_set("MIC", first, &settings.mic);
		return;
	}
	if (!strcmp(verb, "LEVELS") && is_on_off(first, &is_on)) {
		is_levels_shown = is_on;
		return;
	}
	if (!strcmp(verb, "DISPLAY") && is_on_off(first, &is_on)) {
		if (is_on) {
			display_on();
			console_say("DISPLAY ON");
		} else {
			display_off();
			console_say("DISPLAY OFF");
		}
		return;
	}
	if (!strcmp(verb, "MICPIN") && first[0] != 0) {
		microphone_pin(atoi(first));
		return;
	}
	if (!strcmp(verb, "LINK") && (!strcasecmp(first, "LOCAL") || !strcasecmp(first, "INTERNET"))) {
		settings.is_local = !strcasecmp(first, "LOCAL");
		settings_save();
		network_local((int)settings.is_local);
		console_say("LINK %s", settings.is_local ? "LOCAL" : "INTERNET");
		return;
	}
	if (!strcmp(verb, "GATE") && is_on_off(first, &is_on)) {
		settings.is_gate = (uint32_t)is_on;
		settings_save();
		console_say("GATE %s", is_on ? "ON" : "OFF");
		return;
	}
	if (!strcmp(verb, "VOX")) {
		char *end;
		long level = strtol(first, &end, 10);

		if (!strcasecmp(first, "OFF")) {
			level = VOX_OFF;
		} else if (first[0] == 0 || *end != 0 || level < 1 || level > VOX_LEVEL_MOST) {
			console_say("VOX: 1 TO %d, 10 THE MOST SENSITIVE, OR OFF", VOX_LEVEL_MOST);
			return;
		}
		settings.vox = (uint32_t)level;
		settings_save();
		if (level == VOX_OFF)
			console_say("VOX OFF");
		else
			console_say("VOX %ld", level);
		return;
	}
	if (!strcmp(verb, "PIN") && first[0] != 0) {
		gpio_io_config_t config;
		int pin = atoi(first);

		if (pin < 0 || pin >= GPIO_NUM_MAX || gpio_get_io_config((gpio_num_t)pin, &config) != ESP_OK) {
			console_say("PIN %s: NO SUCH PIN", first);
			return;
		}
		console_say("PIN %d: PULL-UP %s, PULL-DOWN %s, INPUT %s, OUTPUT %s, FUNCTION %u, READS %d",
			pin, config.pu ? "ON" : "off", config.pd ? "ON" : "off", config.ie ? "ON" : "off",
			config.oe ? "ON" : "off", (unsigned)config.fun_sel, gpio_get_level((gpio_num_t)pin));
		return;
	}
	if (!strcmp(verb, "KEYS")) {
		keys_describe(said, sizeof(said));
		console_say("KEYS %s", said);
		return;
	}
	if (!strcmp(verb, "SCREEN")) {
		screen_describe(first, sizeof(first));
		console_say("SCREEN %s", first);
		return;
	}
	if (!strcmp(verb, "RECORD")) {
		microphone_record();
		return;
	}
	if (!strcmp(verb, "AUDIO") && is_on_off(first, &is_on)) {
		microphone_power(is_on);
		console_say("AUDIO %s", is_on ? "ON" : "OFF");
		return;
	}
	if (!strcmp(verb, "BATTERY")) {
		if (first[0] != 0)
			battery_set(first);
		else
			say_battery();
		return;
	}
	if (!strcmp(verb, "STATUS")) {
		say_status();
		return;
	}
	if (!strcmp(verb, "SLEEP")) {
		sleep_now();
		return;
	}
	if (!strcmp(verb, "RESTART")) {
		console_say("RESTARTING");
		esp_restart();
		return;
	}
	console_say("? %s: WIFI SCAN LINK RADIO CALL GRID FREQ SERVER KEY NEW_KEY VOIP PTT TONE VOL MIC LEVELS VOX GATE KEYS PIN SCREEN RECORD MICPIN DISPLAY AUDIO BATTERY STATUS SLEEP RESTART", verb);
}

void verbs_poll(long now) {
	struct microphone_levels levels;
	char heard[16];
	int is_hearing;

	if (!is_levels_shown || now - levels_shown_ms < MS_A_SECOND)
		return;
	levels_shown_ms = now;
	microphone_levels(&levels);
	is_hearing = voip_hearing(heard, sizeof(heard));
	console_say("LEVELS PEAK %d RMS %d ADC %d A SECOND (%d READ, %d STRANGE, %u RESTARTS) VOX %d OVER %d BACKGROUND %d%s%s",
		levels.peak, levels.rms, levels.samples_a_second, levels.results, levels.strangers,
		levels.restarts, levels.vox_peak, levels.vox_threshold, levels.vox_background,
		microphone_vox() ? " KEYED" : "", is_hearing ? " HEARING" : "");
}
