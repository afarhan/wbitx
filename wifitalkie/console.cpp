// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ashhar Farhan
//
// console.cpp — lines to and from the laptop, over the ESP32-C3's own USB.
//
// The C3's USB port is a serial port to the laptop, /dev/ttyACM0, with the
// sketch built for "USB CDC on boot"; Serial is that port. The laptop types
// a verb a line, verbs.c does it, and whatever the talkie has to say comes
// back a line at a time, console_say(). tools/talkie.py is the laptop's
// end; any serial terminal will do.
//
// Nothing here waits. A line is gathered from what has come so far, and is
// taken when its end comes; a line too long is cut short. With no laptop
// listening, what is said is thrown away rather than waited on: Serial's
// wait for room is set to nothing, since a talkie in somebody's hand has no
// laptop, and the speaker must not stop for want of one.
//
// The rest of the talkie is C, and this is the one part that has to be
// C++, for Serial.

#include <Arduino.h>
#include <stdarg.h>

#include "console.h"

#define CONSOLE_BAUD 115200
#define SAY_ROOM 256

static char gathered[CONSOLE_LINE_ROOM];
static int gathered_used;

void console_begin(void) {
	Serial.setTxTimeoutMs(0);
	Serial.begin(CONSOLE_BAUD);
}

// 1 with a whole line, without its end
int console_line(char *line, int room) {
	int c;

	while (Serial.available() > 0) {
		c = Serial.read();
		if (c == '\r' || c == '\n') {
			if (gathered_used == 0)
				continue;
			gathered[gathered_used] = 0;
			snprintf(line, room, "%s", gathered);
			gathered_used = 0;
			return 1;
		}
		if (gathered_used < CONSOLE_LINE_ROOM - 1)
			gathered[gathered_used++] = (char)c;
	}
	return 0;
}

void console_say(const char *format, ...) {
	char text[SAY_ROOM];
	va_list args;

	va_start(args, format);
	vsnprintf(text, sizeof(text), format, args);
	va_end(args);
	Serial.print(text);
	Serial.print("\r\n");
}
