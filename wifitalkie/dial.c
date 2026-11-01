// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ashhar Farhan
//
// dial.c — the frequency stepped by the keys, a kilohertz at a press.
//
// A room is a kilohertz, so UP and DOWN step DIAL_STEP_HZ. Held, the key
// repeats faster and faster (button.c), and the step grows too: ten
// kilohertz once it has been held DIAL_TENS_AFTER_MS, a hundred after
// DIAL_HUNDREDS_AFTER_MS, so that 7 MHz to 21 MHz is some seconds, not
// minutes. A step lands on a whole number of itself, so a frequency off the
// grid comes onto it at the first press, and held steps stop on round
// numbers. The dial stops at DIAL_LEAST_HZ and DIAL_MOST_HZ; it does not
// wrap. It knows nothing of the ESP32, so tests/test_units.c tries it.

#include "dial.h"

#define TENS 10
#define HUNDREDS 100

uint32_t dial_step(uint32_t hz, int up, long held_ms) {
	uint32_t step = DIAL_STEP_HZ;
	uint32_t below;

	if (held_ms >= DIAL_TENS_AFTER_MS)
		step = DIAL_STEP_HZ * TENS;
	if (held_ms >= DIAL_HUNDREDS_AFTER_MS)
		step = DIAL_STEP_HZ * HUNDREDS;
	below = hz / step * step;
	if (up)
		hz = below + step;
	else if (below == hz)
		hz = below - step;
	else
		hz = below;
	if (hz < DIAL_LEAST_HZ || hz > DIAL_MOST_HZ + step) {
		if (up)
			return DIAL_MOST_HZ;
		return DIAL_LEAST_HZ;
	}
	if (hz > DIAL_MOST_HZ)
		return DIAL_MOST_HZ;
	return hz;
}
