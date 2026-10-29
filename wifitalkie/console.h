// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ashhar Farhan
//
// console.h — lines to and from the laptop over USB. See console.cpp.

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#define CONSOLE_LINE_ROOM 160

void console_begin(void);
int console_line(char *line, int room);
void console_say(const char *format, ...) __attribute__((format(printf, 1, 2)));

#ifdef __cplusplus
}
#endif
