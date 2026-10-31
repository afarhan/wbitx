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
// MENU is held, and the talkie's lamp is now on the display. The PTT is GPIO21, UART0's TX, which the ROM drives at reset:
// it has 1k in series. GPIO2 (MUTE), GPIO5 (the right channel) and GPIO0
// (the headset's microphone) are for what is still to be built.

#pragma once

#define PIN_MICROPHONE 3
#define PIN_SPEAKER 4
#define PIN_DISPLAY_SDA 6
#define PIN_DISPLAY_SCL 7
#define PIN_KEY_MENU 8
#define PIN_KEY_UP 9
#define PIN_KEY_DOWN 10
#define PIN_KEY_ENTER 20
#define PIN_PTT 21
