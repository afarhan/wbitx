// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ashhar Farhan
//
// network.h — the WiFi access point. See network.cpp.

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

void network_open(void);
void network_join(void);
void network_poll(long now);
void network_scan(void);
void network_off(void);
void network_on(void);
int network_is_up(void);
void network_address(char *text, int room);
int network_rssi(void);
void network_describe(char *text, int room);

#ifdef __cplusplus
}
#endif
