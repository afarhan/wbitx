// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ashhar Farhan
//
// pins.h — where the talkie's parts go on an ESP32-C3 SuperMini.
//
// The author's map of 31 October 2026, for his PCB. The microphones have to
// be on ADC1, GPIO0 to GPIO4: ADC2 cannot be read while the WiFi is on. The
// front panel, the display and four keys, is GPIO6 to GPIO10 and GPIO20, in
// a row on one edge, for one connector; the keys, top to bottom, MENU, UP,
// DOWN, ENTER. GPIO9 is also the board's BOOT button, so BOOT is UP once
// the board has started; GPIO8 is also its blue lamp, which lights while
// MENU is held, and the talkie's lamp is now on the display. ENTER is the
// PTT on the home page; the PTT key speaks on every page. GPIO4 and GPIO5
// are the headset's left and right.
// GPIO0 is the headset's microphone. GPIO1 powers both microphones'
// preamplifiers and holds the PAM8403's shutdown up through 4.7k: low,
// all three are off. The HW-104 module ties shutdown (pin 12) to PVDD
// (pin 13) in its copper, so pin 12 is lifted off its pad. GPIO2 reads the 5 V line through a 100k and 100k divider: the
// battery. GPIO21 is the PTT key, to ground; it is UART0's TX, driven
// by the ROM at reset, so the key wants 1k in series.

#pragma once

#define PIN_MICROPHONE 3
#define PIN_HEADSET_MIC 0
#define PIN_SPEAKER_LEFT 4
#define PIN_SPEAKER_RIGHT 5
#define PIN_DISPLAY_SDA 6
#define PIN_DISPLAY_SCL 7
#define PIN_KEY_MENU 8
#define PIN_KEY_UP 9
#define PIN_KEY_DOWN 10
#define PIN_KEY_ENTER 20
#define PIN_KEY_PTT 21
#define PIN_MIC_POWER 1
#define PIN_BATTERY 2

// 0 on the hand-wired talkie, whose divider is on the 5 V line, the cell
// through a Schottky; 1 on the wificomms board, whose divider is on the
// cell itself, R11 and R12 made 10k and 10k (battery.c), and the USB
// cannot be told from it. Every wificomms board has its R11 and R12
// changed to 10k by hand, the author's of 2026-11-11, so 1 is for all.
#define BATTERY_DIVIDER_ON_CELL 1
