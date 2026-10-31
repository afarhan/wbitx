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
//     ptt.c            the PTT and the lamp
//     screen.c         the display and the keys: display.c, font.c, keys.c,
//                      button.c
//     pins.h           where the parts go

#include <Arduino.h>

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

void setup() {
	console_begin();
	settings_load();
	telephone_open();
	ptt_open();
	screen_open();
	speaker_open();
	microphone_open();
	service_open();
	network_open();
	console_say("WIFITALKIE READY: TYPE STATUS");
}

void loop() {
	char line[CONSOLE_LINE_ROOM];
	long now = (long)millis();

	if (console_line(line, sizeof(line)))
		verbs_do(line, now);
	verbs_poll(now);
	network_poll(now);
	ptt_poll(now);
	microphone_poll(now);
	service_poll(now);
	voip_poll(now);
	speaker_poll(now);
	screen_poll(now);
	vTaskDelay(TICKS_GIVEN_UP);
}
