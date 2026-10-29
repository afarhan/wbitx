// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ashhar Farhan
//
// pins.h — where the talkie's parts go on an ESP32-C3 SuperMini.
//
// The microphone has to be on ADC1, GPIO0 to GPIO4: ADC2 cannot be read
// while the WiFi is on. GPIO9 is the board's BOOT button, which is free as
// an input once the board has started, and GPIO8 its blue lamp, lit by
// pulling it low. These are a first guess, to be checked against the board
// the author has.

#pragma once

#define PIN_MICROPHONE 3
#define PIN_SPEAKER 4
#define PIN_PTT 9
#define PIN_LAMP 8
#define LAMP_LIT_LEVEL 0
