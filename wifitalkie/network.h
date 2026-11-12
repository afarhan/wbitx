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
void network_scan_begin(void);
int network_is_scanning(void);
int network_scanned_count(void);
void network_scanned(int i, char *ssid, int room, int *rssi, int *is_open);
void network_ssid(char *text, int room);
void network_trying(char *text, int room);
void network_local(int is_on);
void network_off(void);
void network_on(void);
int network_is_up(void);
void network_address(char *text, int room);
int network_rssi(void);
void network_describe(char *text, int room);
int network_ap_begin(const char *name);
void network_ap_end(void);
void network_ap_address(char *text, int room);
int network_ap_clients(void);

#ifdef __cplusplus
}
#endif
