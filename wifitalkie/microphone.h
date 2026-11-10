// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ashhar Farhan
//
// microphone.h — the microphone, read by the ADC. See microphone.c.

#pragma once

#include <stdint.h>

#include "telephone.h"

#ifdef __cplusplus
extern "C" {
#endif

// what the last whole second of the microphone was
struct microphone_levels {
	int peak;
	int rms;
	int samples_a_second;
	int results;
	int strangers;
	unsigned restarts;
};

void microphone_open(void);
void microphone_poll(long now);
int microphone_frame(int16_t samples[TELEPHONE_FRAME_SAMPLES]);
void microphone_levels(struct microphone_levels *levels);
void microphone_record(void);
void microphone_pin(int pin);
void microphone_power(int is_on);

#define MICROPHONE_UNKNOWN 0
#define MICROPHONE_BATTERY 1
#define MICROPHONE_USB 2
int microphone_battery(int *millivolts);
void microphone_battery_raw(int *code, int *pin_millivolts);

#ifdef __cplusplus
}
#endif
