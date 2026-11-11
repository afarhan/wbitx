// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ashhar Farhan
//
// vox.h — speaking without the PTT: the voice keys the talkie. See vox.c.

#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define VOX_OFF 0
#define VOX_LEVEL_MOST 10
#define VOX_ATTACK_MS 30
#define VOX_HANG_MS 1200
#define VOX_LEAST 40

struct vox {
	int32_t envelope;
	int32_t background;
	int above;
	int hang;
	int is_keyed;
};

void vox_begin(struct vox *v);
void vox_take(struct vox *v, int16_t sample, int level, int is_held_off, int rate_hz);
int vox_is_keyed(const struct vox *v);
int vox_threshold(const struct vox *v, int level);
int vox_size(const struct vox *v);

#ifdef __cplusplus
}
#endif
