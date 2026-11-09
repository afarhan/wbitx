// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ashhar Farhan
//
// display.c — the 0.96 inch OLED, 128 by 64 dots, on I2C.
//
// The module is an SSD1306 at address 0x3c, or 0x3d with its jumper moved;
// both are tried. Some modules sold as the same are an SH1106, which has
// 132 columns and shows ours two in: COLUMN_OFFSET is 2 for one of those.
// Both are written a page at a time, a page being a row of eight dots high
// across the screen, a byte a column with the lowest bit at the top.
//
// What is drawn goes into a frame in memory; display_poll() sends the
// display one page that is not yet as the frame has it, each time it is
// called. A page is 129 bytes, about 3 ms at 400 kHz; the whole screen at
// once would hold the loop up some 25 ms, near what the speaker's ring
// holds. So a screen redrawn reaches the display over eight turns of the
// loop, and a page not changed is not sent at all.
//
// display_off() turns the panel off and sends it nothing more, so that no
// I2C runs near the microphone; display_on() turns it on and sends it all
// again.

#include <stdio.h>
#include <string.h>

#include "driver/i2c_master.h"

#include "console.h"
#include "display.h"
#include "font.h"
#include "pins.h"

#define ADDRESS_FIRST 0x3c
#define ADDRESS_SECOND 0x3d
#define SPEED_HZ 400000
#define WAIT_MS 20
#define PROBE_MS 50
#define GLITCH_CYCLES 7
#define COLUMN_OFFSET 0
#define COMMANDS 0x00
#define DATA 0x40
#define PAGE_SET 0xb0
#define COLUMN_LOW_SET 0x00
#define COLUMN_HIGH_SET 0x10
#define NIBBLE 0x0f
#define NIBBLE_BITS 4
#define PANEL_OFF 0xae
#define PANEL_ON 0xaf
#define SHOW_NORMAL 0xa6
#define SHOW_INVERSE 0xa7

static const uint8_t start[] = {
	COMMANDS,
	0xae,		// off while it is set up
	0xd5, 0x80,	// its clock
	0xa8, 0x3f,	// 64 rows
	0xd3, 0x00,	// no vertical shift
	0x40,		// first row at the top
	0x8d, 0x14,	// its charge pump on
	0x20, 0x02,	// page addressing, which the SH1106 also has
	0xa0,		// columns right to left and
	0xc0,		// rows bottom to top: the module is mounted upside down
	0xda, 0x12,	// how its rows are wired
	0x81, 0xcf,	// contrast
	0xd9, 0xf1,	// precharge
	0xdb, 0x40,	// the level a dot is off at
	0xa4,		// show the memory
	0xa6,		// dots lit for ones
	0xaf,		// on
};

static i2c_master_bus_handle_t bus;
static i2c_master_dev_handle_t device;
static int is_open;
static int is_off;
static uint8_t frame[DISPLAY_ROWS][DISPLAY_WIDE];
static uint8_t sent[DISPLAY_ROWS][DISPLAY_WIDE];
static int is_sent[DISPLAY_ROWS];
static int next_page;
static long pages_sent;
static long pages_failed;

int display_open(void) {
	i2c_master_bus_config_t bus_config = {
		.i2c_port = -1,
		.sda_io_num = PIN_DISPLAY_SDA,
		.scl_io_num = PIN_DISPLAY_SCL,
		.clk_source = I2C_CLK_SRC_DEFAULT,
		.glitch_ignore_cnt = GLITCH_CYCLES,
		.flags.enable_internal_pullup = 1,
	};
	i2c_device_config_t device_config = {
		.dev_addr_length = I2C_ADDR_BIT_LEN_7,
		.device_address = ADDRESS_FIRST,
		.scl_speed_hz = SPEED_HZ,
	};

	if (i2c_new_master_bus(&bus_config, &bus) != ESP_OK) {
		console_say("DISPLAY: NO I2C");
		return 0;
	}
	if (i2c_master_probe(bus, ADDRESS_FIRST, PROBE_MS) != ESP_OK) {
		device_config.device_address = ADDRESS_SECOND;
		if (i2c_master_probe(bus, ADDRESS_SECOND, PROBE_MS) != ESP_OK) {
			console_say("DISPLAY: NONE AT 0X3C OR 0X3D ON GPIO%d AND GPIO%d",
				PIN_DISPLAY_SDA, PIN_DISPLAY_SCL);
			return 0;
		}
	}
	if (i2c_master_bus_add_device(bus, &device_config, &device) != ESP_OK
			|| i2c_master_transmit(device, start, sizeof(start), WAIT_MS) != ESP_OK) {
		console_say("DISPLAY: IT DID NOT START");
		return 0;
	}
	is_open = 1;
	console_say("DISPLAY AT 0X%02X", (unsigned)device_config.device_address);
	return 1;
}

void display_clear(void) {
	memset(frame, 0, sizeof(frame));
}

static void dot(int x, int y) {
	if (x < 0 || x >= DISPLAY_WIDE || y < 0 || y >= DISPLAY_HIGH)
		return;
	frame[y / DISPLAY_ROW_HIGH][x] |= (uint8_t)(1 << (y % DISPLAY_ROW_HIGH));
}

// a box wide by high dots, its top left at x, y, filled
void display_fill(int x, int y, int wide, int high) {
	int i;
	int j;

	for (i = 0; i < wide; i++) {
		for (j = 0; j < high; j++)
			dot(x + i, y + j);
	}
}

// a box wide by high dots, its top left at x, y, turned over: lit dots go
// dark and dark ones lit, for reverse video
void display_invert(int x, int y, int wide, int high) {
	int i;
	int j;

	for (i = x; i < x + wide; i++) {
		for (j = y; j < y + high; j++) {
			if (i >= 0 && i < DISPLAY_WIDE && j >= 0 && j < DISPLAY_HIGH)
				frame[j / DISPLAY_ROW_HIGH][i] ^= (uint8_t)(1 << (j % DISPLAY_ROW_HIGH));
		}
	}
}

// text with its top at the row's, each dot of the font scale by scale
void display_text(int x, int row, int scale, const char *t) {
	display_text_at(x, row * DISPLAY_ROW_HIGH, scale, t);
}

// the letter c of font, its left at x, its top row at y
static void letter(int font, int x, int y, int scale, char c) {
	const struct glyph *g = font_glyph(font, c);
	int column;
	int bit;
	int i;
	int j;

	for (column = 0; column < g->wide; column++) {
		for (bit = 0; bit < FONT_MAIN_DEEP; bit++) {
			if (!(g->columns[column] >> bit & 1))
				continue;
			for (i = 0; i < scale; i++) {
				for (j = 0; j < scale; j++)
					dot(x + (g->left + column) * scale + i, y + bit * scale + j);
			}
		}
	}
}

static void text(int font, int x, int y, int scale, const char *t) {
	for (; *t != 0; t++) {
		letter(font, x, y, scale, *t);
		x += font_glyph(font, *t)->advance * scale;
	}
}

static int text_wide(int font, const char *t, int scale) {
	const struct glyph *g = NULL;
	int across = 0;

	for (; *t != 0; t++) {
		g = font_glyph(font, *t);
		across += g->advance * scale;
	}
	// the last letter's own width, not its advance
	if (g != NULL)
		across += (g->left + g->wide - g->advance) * scale;
	return across > 0 ? across : 0;
}

// text in the main font, its top at y, a dot's, not a row's
void display_text_at(int x, int y, int scale, const char *t) {
	text(FONT_MAIN, x, y, scale, t);
}

// how wide text in the main font is, in dots
int display_text_wide(const char *t, int scale) {
	return text_wide(FONT_MAIN, t, scale);
}

// text in the small font, for titles and help
void display_small_at(int x, int y, const char *t) {
	text(FONT_SMALL, x, y, 1, t);
}

int display_small_wide(const char *t) {
	return text_wide(FONT_SMALL, t, 1);
}

// one letter of the small font in the middle of a cell FONT_WIDE dots
// wide, for letters kept in columns
void display_letter_at(int x, int y, int scale, char c) {
	const struct glyph *g = font_glyph(FONT_SMALL, c);

	letter(FONT_SMALL, x + (FONT_WIDE - g->wide) / 2 * scale, y, scale, c);
}

// a bar wide dots across, its top at y and as high as a capital, an
// outline with filled of them lit
void display_bar(int x, int y, int wide, int filled) {
	int top = y;
	int bottom = top + FONT_MAIN_HIGH - 1;
	int i;
	int j;

	for (i = 0; i < wide; i++) {
		dot(x + i, top);
		dot(x + i, bottom);
		for (j = top; j <= bottom && i < filled; j++)
			dot(x + i, j);
	}
	for (j = top; j <= bottom; j++) {
		dot(x, j);
		dot(x + wide - 1, j);
	}
}

static void send_page(int page) {
	uint8_t where[] = {
		COMMANDS,
		(uint8_t)(PAGE_SET | page),
		(uint8_t)(COLUMN_LOW_SET | (COLUMN_OFFSET & NIBBLE)),
		(uint8_t)(COLUMN_HIGH_SET | (COLUMN_OFFSET >> NIBBLE_BITS)),
	};
	uint8_t data[DISPLAY_WIDE + 1];

	data[0] = DATA;
	memcpy(&data[1], frame[page], DISPLAY_WIDE);
	if (i2c_master_transmit(device, where, sizeof(where), WAIT_MS) != ESP_OK
			|| i2c_master_transmit(device, data, sizeof(data), WAIT_MS) != ESP_OK) {
		pages_failed++;
		return;
	}
	pages_sent++;
	memcpy(sent[page], frame[page], DISPLAY_WIDE);
	is_sent[page] = 1;
}

// for the verb SCREEN: whether it was found, and the pages sent to it and
// those whose sending failed, since it started
void display_describe(char *text, int room) {
	if (!is_open) {
		snprintf(text, room, "DISPLAY NOT FOUND");
		return;
	}
	snprintf(text, room, "DISPLAY %ld PAGES SENT, %ld FAILED", pages_sent, pages_failed);
}

void display_poll(void) {
	int tried;
	int page;

	if (!is_open || is_off)
		return;
	for (tried = 0; tried < DISPLAY_ROWS; tried++) {
		page = next_page;
		next_page = (next_page + 1) % DISPLAY_ROWS;
		if (is_sent[page] && !memcmp(sent[page], frame[page], DISPLAY_WIDE))
			continue;
		send_page(page);
		return;
	}
}

static void panel(uint8_t command) {
	uint8_t sent_now[] = {COMMANDS, command};

	i2c_master_transmit(device, sent_now, sizeof(sent_now), WAIT_MS);
}

// the whole display turned over, lit dots dark and dark lit, for the
// white theme; the display does it, the frame is not changed
void display_white(int is_white) {
	if (!is_open)
		return;
	panel(is_white ? SHOW_INVERSE : SHOW_NORMAL);
}

void display_off(void) {
	if (!is_open || is_off)
		return;
	panel(PANEL_OFF);
	is_off = 1;
}

void display_on(void) {
	if (!is_open || !is_off)
		return;
	memset(is_sent, 0, sizeof(is_sent));
	panel(PANEL_ON);
	is_off = 0;
}
