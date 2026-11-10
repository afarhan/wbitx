// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ashhar Farhan
//
// local.h — talkie to talkie by ESP-NOW, off the internet. See local.c.

#pragma once

#include <stdint.h>

#include "voip_packet.h"

#ifdef __cplusplus
extern "C" {
#endif

#define LOCAL_CHANNEL 1

int local_open(void);
void local_close(void);
int local_is_open(void);
int local_send(const uint8_t packet[VOIP_PACKET_BYTES]);
int local_take(uint8_t packet[VOIP_PACKET_BYTES]);
int local_rssi(void);
void local_describe(char *text, int room);

#ifdef __cplusplus
}
#endif
