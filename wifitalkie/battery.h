// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ashhar Farhan
//
// battery.h — the cell's volts from GPIO2's, on the wificomms board. See battery.c.

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#define BATTERY_TOP_OHMS 10000      // R11, the cell to GPIO2
#define BATTERY_BOTTOM_OHMS 10000   // R12, GPIO2 to ground
#define BATTERY_PULLUP_OHMS 4700    // on the SuperMini, GPIO2 to 3V3
#define BATTERY_RAIL_MV 3100        // the SuperMini's 3V3, by the scope
// below a cell of about 4.8 V the 3V3 sags with it, the author's bench
// supply of 2026-11-11: rail = BATTERY_SAG_AT_0_MV + cell * BATTERY_SAG_PER_MILLE / 1000
#define BATTERY_SAG_AT_0_MV 1848
#define BATTERY_SAG_PER_MILLE 260

int battery_cell_mv(int pin_mv, int offset_mv);
int battery_offset_for(int pin_mv, int cell_mv);

#ifdef __cplusplus
}
#endif
