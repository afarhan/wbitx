// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ashhar Farhan
//
// known.h — the WiFi networks the talkie has joined. See known.c.

#pragma once

#include "settings.h"

#ifdef __cplusplus
extern "C" {
#endif

int known_find(const struct settings *s, const char *ssid);
int known_remember(struct settings *s, const char *ssid, const char *password);
int known_forget(struct settings *s, int i);
int known_count(const struct settings *s);

#ifdef __cplusplus
}
#endif
