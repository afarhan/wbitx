// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ashhar Farhan
//
// verbs.h — what can be typed at the talkie. See verbs.c.

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

void verbs_do(const char *line, long now);
void verbs_poll(long now);

#ifdef __cplusplus
}
#endif
