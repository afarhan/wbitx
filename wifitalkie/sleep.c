// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ashhar Farhan
//
// sleep.c — the talkie asleep, and woken by a key.
//
// sleep_now() turns off what draws: the display, the WiFi, the speaker's
// timer, and GPIO1, which powers the microphones' preamplifiers and holds
// the PAM8403 out of shutdown. Then the C3 goes into light sleep, until
// any of the five keys is pressed, and on waking it starts again from the
// beginning, as if powered up: it joins the WiFi and the room afresh.
//
// Light sleep and not deep sleep: deep sleep wakes only on GPIO0 to GPIO5,
// and the keys are on GPIO8, 9, 10, 20 and 21. The C3 takes about 130 uA
// in light sleep; the SuperMini's red lamp, about 1 mA, is the most of it.
//
// The C3 sets its pins otherwise while it sleeps, unless told not to: the
// keys keep their pull-ups and GPIO1 stays low. A key still down, or
// bouncing, would wake it at once, so it waits for all to be up for
// QUIET_MS first.

#include "driver/gpio.h"
#include "esp_rom_sys.h"
#include "esp_sleep.h"
#include "esp_system.h"

#include "console.h"
#include "display.h"
#include "microphone.h"
#include "network.h"
#include "pins.h"
#include "sleep.h"
#include "speaker.h"

#define QUIET_MS 200
#define QUIET_MOST_MS 5000
#define SAID_MS 50

static const int wakers[] = {PIN_KEY_MENU, PIN_KEY_UP, PIN_KEY_DOWN, PIN_KEY_ENTER, PIN_KEY_PTT};
#define WAKERS ((int)(sizeof(wakers) / sizeof(wakers[0])))

static int is_any_down(void) {
	int i;

	for (i = 0; i < WAKERS; i++)
		if (gpio_get_level(wakers[i]) == 0)
			return 1;
	return 0;
}

// all keys up for QUIET_MS on end, or 0 after QUIET_MOST_MS
static int wait_quiet(void) {
	int quiet = 0;
	int waited;

	for (waited = 0; waited < QUIET_MOST_MS; waited++) {
		quiet = is_any_down() ? 0 : quiet + 1;
		if (quiet >= QUIET_MS)
			return 1;
		esp_rom_delay_us(1000);
	}
	return 0;
}

void sleep_now(void) {
	int i;

	if (!wait_quiet()) {
		console_say("SLEEP: A KEY IS HELD DOWN, NOT SLEEPING");
		return;
	}
	console_say("SLEEPING: ANY KEY WAKES");
	esp_rom_delay_us(SAID_MS * 1000);
	display_off();
	speaker_mute(1);
	microphone_power(0);
	network_off();

	gpio_sleep_sel_dis(PIN_MIC_POWER);
	for (i = 0; i < WAKERS; i++) {
		gpio_sleep_sel_dis(wakers[i]);
		gpio_wakeup_enable(wakers[i], GPIO_INTR_LOW_LEVEL);
	}
	esp_sleep_enable_gpio_wakeup();
	esp_light_sleep_start();
	esp_restart();
}
