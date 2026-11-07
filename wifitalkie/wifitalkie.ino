// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ashhar Farhan
//
// wifitalkie.ino — a walkie-talkie on WiFi, through my.sbitx.net's repeater.
//
// An ESP32-C3 with a microphone, a speaker and a PTT, which to my.sbitx.net
// is one more radio of its owner's account, in the mode VOIP: whoever
// speaks on the kilohertz of its FREQ is heard here, and what is said here
// with the PTT down is heard by every radio on that kilohertz with VOIP
// online. It has no radio of its own. For this first one, it is set from
// the laptop, a verb a line over USB, verbs.c has them, and from its own
// display and four keys, screen.c.
//
// One loop and no tasks of its own: each module is polled in turn, and none
// waits, but the handshake with my.sbitx.net, when the line is being made
// and there is nothing to hear. The speaker's timer interrupt and the ADC's
// DMA are the only things that run by themselves. The loop gives the
// processor up for a tick each turn, so that the idle task runs and the
// WiFi's tasks are not kept waiting.
//
//     console.cpp      lines to and from the laptop
//     verbs.c          what can be typed
//     settings.c       what is kept across power-ups
//     network.cpp      the WiFi access point
//     service.c        the line to my.sbitx.net, the handshake
//     auth.c           the cookie, the stretch, the proofs and keys
//     voip.c           the stream by UDP, what is sent and heard
//     voip_packet.c    the packet
//     microphone.c     the ADC, down to 8000 a second
//     speaker.c        the queue and the PWM, playback.c the 62500 a second
//     telephone.c      mu-law and the filter
//     ptt.c            the PTT, the ENTER key on the home page, and the lamp
//     screen.c         the display and the keys: display.c, font.c, keys.c,
//                      button.c
//     pins.h           where the parts go

#include <Arduino.h>

#include "esp_system.h"

#include "console.h"
#include "microphone.h"
#include "network.h"
#include "ptt.h"
#include "screen.h"
#include "service.h"
#include "settings.h"
#include "speaker.h"
#include "telephone.h"
#include "verbs.h"
#include "voip.h"

#define TICKS_GIVEN_UP 1

// a probe for now: any poll that keeps the loop SLOW_MS or more is said
#define SLOW_MS 150
#define TIMED(call) do { long began_ = (long)millis(); call; \
	if ((long)millis() - began_ >= SLOW_MS) \
		console_say("SLOW: %s %ld MS", #call, (long)millis() - began_); } while (0)

void setup() {
	console_begin();
	settings_load();
	telephone_open();
	screen_open();
	speaker_open();
	microphone_open();
	service_open();
	network_open();
	console_say("WIFITALKIE READY: TYPE STATUS");
	if (esp_reset_reason() == ESP_RST_BROWNOUT)
		console_say("STARTED AFTER A BROWNOUT: THE SUPPLY SAGGED");
}

void loop() {
	char line[CONSOLE_LINE_ROOM];
	long now = (long)millis();

	if (console_line(line, sizeof(line)))
		verbs_do(line, now);
	TIMED(verbs_poll(now));
	TIMED(network_poll(now));
	TIMED(ptt_poll(now));
	TIMED(microphone_poll(now));
	TIMED(service_poll(now));
	TIMED(voip_poll(now));
	TIMED(speaker_poll(now));
	TIMED(screen_poll(now));
	long turned = (long)millis();
	vTaskDelay(TICKS_GIVEN_UP);
	if ((long)millis() - turned >= SLOW_MS)
		console_say("SLOW: vTaskDelay %ld MS", (long)millis() - turned);
}
