// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ashhar Farhan
//
// contacts.h — the callsigns the talkie keeps. See contacts.c.

#pragma once

#include "settings.h"

#ifdef __cplusplus
extern "C" {
#endif

int contacts_callsign(const char *typed, char call[SETTINGS_CALLSIGN_ROOM]);
int contacts_count(const struct settings *s);
int contacts_find(const struct settings *s, const char *callsign);
int contacts_add(struct settings *s, const char *callsign);
int contacts_forget(struct settings *s, int i);
int contacts_replace(struct settings *s, int i, const char *callsign);

#ifdef __cplusplus
}
#endif
