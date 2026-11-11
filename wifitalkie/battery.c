// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ashhar Farhan
//
// battery.c — the cell's volts from GPIO2's, on the wificomms board.
//
// GPIO2 is a strapping pin and must read high at reset, and the SuperMini
// holds it up through 4.7k to its 3V3, which cannot come off. So the cell's
// divider, R11 from the cell and R12 to ground, is made low, 10k and 10k,
// the author's of 2026-11-10, and the pin sits between the two pulls: from
// 2.49 V at a cell of 3.7 V to 2.66 V at 4.4 V, 0.80 of the rail and more,
// over the 0.75 the C3 takes for a high, and under the ADC's top of about
// 2.95 V. The pin moves some 242 mV for a volt of the cell.
//
// By the currents into the pin, the cell's volts are
//
//   cell = pin (1 + R11/R12 + R11/Rp) - rail R11/Rp
//
// The rail was first taken to be BATTERY_RAIL_MV throughout; it is not.
// On the bench supply, off the USB, the author's of 2026-11-11, the
// display read the cell 0.5 V low at 3.7 V and true from 4.8 V up: the
// rail, worked back from each reading, sags from 3.1 V at 4.8 V to some
// 2.8 V at 3.6 V, a quarter of a volt for each volt of the cell, through
// the Schottky and the SuperMini's regulator. Each 0.1 V the rail is low
// reads the cell 0.21 V low. So below the knee, where the sag line meets
// BATTERY_RAIL_MV, the rail is taken as BATTERY_SAG_AT_0_MV plus
// BATTERY_SAG_PER_MILLE of the cell, and the pin's equation is solved for
// the cell with the rail in it:
//
//   cell = (pin (1 + R11/R12 + R11/Rp) - sag0 R11/Rp) / (1 + slope R11/Rp)
//
// That reads the bench's points back within 0.06 V, as near as a display
// of tenths cut short and a supply's own dial can tell. Not yet seen on
// the scope: the rail itself at 3.7 V. Where the rail sags further, in the
// WiFi's bursts, the cell is read low, and the talkie errs to sleeping early.
//
// A small error in the pull-up, 5% of 4.7k, or in the rail moves the
// reading by a third of a volt, near enough the same at every cell's
// volts; so an offset, set once a board by the verb BATTERY <volts>
// against a meter, is added. battery_offset_for() works it out.

#include <stdint.h>

#include "battery.h"

#define OHMS_ACROSS ((int64_t)BATTERY_BOTTOM_OHMS * BATTERY_PULLUP_OHMS)
#define PIN_GAIN ((int64_t)BATTERY_BOTTOM_OHMS * BATTERY_PULLUP_OHMS \
	+ (int64_t)BATTERY_TOP_OHMS * BATTERY_PULLUP_OHMS \
	+ (int64_t)BATTERY_TOP_OHMS * BATTERY_BOTTOM_OHMS)
#define RAIL_SHARE ((int64_t)BATTERY_RAIL_MV * BATTERY_TOP_OHMS * BATTERY_BOTTOM_OHMS)
#define SAG_SHARE ((int64_t)BATTERY_SAG_AT_0_MV * BATTERY_TOP_OHMS * BATTERY_BOTTOM_OHMS * 1000)
#define SAG_ACROSS (OHMS_ACROSS * 1000 \
	+ (int64_t)BATTERY_TOP_OHMS * BATTERY_BOTTOM_OHMS * BATTERY_SAG_PER_MILLE)
// the cell's millivolts where the sag reaches the regulated rail
#define KNEE_MV ((BATTERY_RAIL_MV - BATTERY_SAG_AT_0_MV) * 1000 / BATTERY_SAG_PER_MILLE)

// the cell's millivolts, from the pin's by the ADC's calibration and the
// board's offset; never below 0
int battery_cell_mv(int pin_mv, int offset_mv) {
	int64_t cell = ((int64_t)pin_mv * PIN_GAIN - RAIL_SHARE + OHMS_ACROSS / 2) / OHMS_ACROSS;

	if (cell < KNEE_MV)
		cell = ((int64_t)pin_mv * PIN_GAIN * 1000 - SAG_SHARE + SAG_ACROSS / 2) / SAG_ACROSS;
	cell += offset_mv;
	return cell < 0 ? 0 : (int)cell;
}

// the offset that makes the pin's millivolts read as cell_mv
int battery_offset_for(int pin_mv, int cell_mv) {
	return cell_mv - battery_cell_mv(pin_mv, 0);
}
