// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ashhar Farhan
//
// power.c — the talkie awake, or idle and listening on a little current.
//
// The author's scheme of 2026-11-25, for sixteen hours on the xyfr-nano's
// 350 mAh: while somebody uses the talkie everything is on, 70 to 90 mA.
// After IDLE_AFTER_MS with no key pressed, on the home page (screen_is_resting()),
// and nothing being said, heard or played, it goes idle:
//
//   - the display off;
//   - the microphones' preamplifiers and the PAM8403 off (GPIO1), and the
//     ADC stopped, so VOX does not key it while idle;
//   - the speaker's timer stopped (speaker_mute());
//   - the WiFi in modem sleep, the radio off between the access point's
//     beacons and woken for each DTIM, which the house's routers send with
//     every beacon, DTIM 1: what comes for the talkie meanwhile is held by
//     the access point and taken at the next, at most some 100 ms late;
//   - and, with power management in our own libraries (sdk/), the C3 in
//     light sleep whenever nothing is to be done, from 160 MHz down to 40
//     when it is: the loop turns every IDLE_LOOP_MS, not every tick. A key
//     or the PTT wakes it at once (GPIO wakeup).
//
// Light sleep is not taken while the USB is plugged into a computer: the
// C3's USB console is powered down in it and the computer loses the port.
// Plugged in, the talkie idles in modem sleep alone; on the cell it sleeps.
//
// Anything wakes it fully, everything on as before: a key, the PTT,
// speech heard (voip.c, power_wake() as its first packet comes), or Morse
// heard to be played (screen.c). A key that wakes it does only that. Once
// what woke it is over, nothing said, heard or played for BUSY_GRACE_MS,
// and no key pressed, it is idle again. In Local, talkie to talkie by
// ESP-NOW, the radio has to listen all the time, and it stays awake.
//
// The C3 sets its pins otherwise while it sleeps (CONFIG_PM_SLP_DISABLE_
// GPIO), unless told not to: every pin of the talkie's is told to keep its
// setting, so that the keys keep their pull-ups and GPIO1 stays low.
//
// The battery is read by the microphone's ADC, and so only while awake.

#include <stdio.h>

#include "freertos/FreeRTOS.h"

#include "driver/gpio.h"
#include "driver/usb_serial_jtag.h"
#include "esp_pm.h"
#include "esp_sleep.h"
#include "esp_wifi.h"

#include "console.h"
#include "display.h"
#include "microphone.h"
#include "network.h"
#include "pins.h"
#include "power.h"
#include "ptt.h"
#include "screen.h"
#include "settings.h"
#include "speaker.h"
#include "voip.h"

#define IDLE_AFTER_MS 30000
#define BUSY_GRACE_MS 3000
#define IDLE_LOOP_MS 100
#define TEXT_ROOM 16
#define AWAKE_MHZ 160
#define IDLE_LEAST_MHZ 40

static const int kept_pins[] = {
	PIN_MICROPHONE, PIN_HEADSET_MIC, PIN_SPEAKER_LEFT, PIN_SPEAKER_RIGHT,
	PIN_DISPLAY_SDA, PIN_DISPLAY_SCL, PIN_KEY_MENU, PIN_KEY_UP, PIN_KEY_DOWN,
	PIN_KEY_ENTER, PIN_KEY_PTT, PIN_MIC_POWER, PIN_BATTERY,
};
#define KEPT_PINS ((int)(sizeof(kept_pins) / sizeof(kept_pins[0])))

static const int wakers[] = {PIN_KEY_MENU, PIN_KEY_UP, PIN_KEY_DOWN, PIN_KEY_ENTER, PIN_KEY_PTT};
#define WAKERS ((int)(sizeof(wakers) / sizeof(wakers[0])))

static int is_idle;
static int is_light_sleep;
static long busy_ms;
static long idle_since_ms;
static long idle_total_ms;
static unsigned wakes;

static void pm_set(int is_sleeping) {
	esp_pm_config_t config = {
		.max_freq_mhz = AWAKE_MHZ,
		.min_freq_mhz = is_sleeping ? IDLE_LEAST_MHZ : AWAKE_MHZ,
		.light_sleep_enable = is_sleeping ? true : false,
	};

	if (esp_pm_configure(&config) != ESP_OK)
		console_say("POWER: THE POWER MANAGEMENT WOULD NOT BE SET");
	is_light_sleep = is_sleeping;
}

void power_open(void) {
	int i;

	for (i = 0; i < KEPT_PINS; i++)
		gpio_sleep_sel_dis(kept_pins[i]);
	pm_set(0);
}

// 1 while something is being said, heard or played, or the talkie is
// off its home page, or in Local
static int is_busy(void) {
	char hearing[TEXT_ROOM];

	return ptt_is_down() || voip_hearing(hearing, sizeof(hearing)) || speaker_is_toning()
		|| settings.is_local || !screen_is_resting();
}

// nothing busy for BUSY_GRACE_MS and no key for IDLE_AFTER_MS
static int is_quiet(long now) {
	return now - busy_ms >= BUSY_GRACE_MS && now - screen_touched_ms() >= IDLE_AFTER_MS;
}

static void go_idle(long now) {
	int i;

	is_idle = 1;
	idle_since_ms = now;
	display_off();
	speaker_mute(1);
	microphone_idle(1);
	network_save_power(1);
	for (i = 0; i < WAKERS; i++)
		gpio_wakeup_enable(wakers[i], GPIO_INTR_LOW_LEVEL);
	esp_sleep_enable_gpio_wakeup();
	console_say("POWER: IDLE");
}

static void go_awake(long now) {
	int i;

	is_idle = 0;
	wakes++;
	idle_total_ms += now - idle_since_ms;
	if (is_light_sleep)
		pm_set(0);
	for (i = 0; i < WAKERS; i++)
		gpio_wakeup_disable(wakers[i]);
	network_save_power(0);
	microphone_idle(0);
	if (!ptt_is_down())
		speaker_mute(0);
	display_on();
	console_say("POWER: AWAKE");
}

void power_wake(long now) {
	busy_ms = now;
	if (is_idle)
		go_awake(now);
}

int power_is_idle(void) {
	return is_idle;
}

void power_poll(long now) {
	if (is_busy())
		busy_ms = now;
	if (is_idle && (ptt_key_is_down() || ptt_is_down() || settings.is_local)) {
		power_wake(now);
		return;
	}
	if (!is_idle && is_quiet(now))
		go_idle(now);
	// light sleep only off the computer's USB, which it would cut off
	if (is_idle) {
		int is_sleep_wanted = !usb_serial_jtag_is_connected();

		if (is_sleep_wanted != is_light_sleep)
			pm_set(is_sleep_wanted);
	}
}

// how long the loop gives up each turn, in ticks
int power_loop_ticks(void) {
	return is_idle ? (int)pdMS_TO_TICKS(IDLE_LOOP_MS) : 1;
}

void power_describe(char *text, int room) {
	snprintf(text, room, "%s%s, %u WAKES, IDLE %ld S IN ALL",
		is_idle ? "IDLE" : "AWAKE", is_idle && is_light_sleep ? " (LIGHT SLEEP)" : "",
		wakes, idle_total_ms / 1000);
}
