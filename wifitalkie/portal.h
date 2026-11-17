// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ashhar Farhan
//
// portal.h — the setup portal. See portal.cpp.

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#define PORTAL_CONFIGURE 0
#define PORTAL_UPDATE 1

void portal_open(int how);
void portal_close(void);
void portal_poll(long now);
int portal_is_open(void);
void portal_name(char *text, int room);
int portal_joined(char *ssid, int room);

#ifdef __cplusplus
}
#endif
