// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ashhar Farhan
//
// vox.c — speaking without the PTT: the voice keys the talkie.
//
// settings.vox is the sensitivity, VOX_OFF or 1 to VOX_LEVEL_MOST, 10 the
// most sensitive. Each of the microphone's samples, at 8000 a second, as
// microphone.c makes it and before speech.c's gate, is given here; its
// size is followed as an envelope over a few milliseconds, and the
// background's as a slow mean of the envelope that falls quickly and rises
// slowly, so that a word does not raise it. The background is not learnt
// while the talkie is keyed, nor while it is held off.
//
// The voice keys the talkie when the envelope has been over the threshold
// for VOX_ATTACK_MS on end, so that a knock or a click does not; and it
// lets go VOX_HANG_MS after the envelope was last over it, so that the
// gaps between words and a breath do not cut the over. The threshold is
// the background times the level's own figure, from twelve times at 1 to
// under twice at 10, and never under VOX_LEAST, in 16-bit samples' size:
// the microphone's noise was 13 to 25 at MIC 50, the author's measure of
// 1 November 2026, and speech at 5 cm some hundreds.
//
// Held off, it neither keys nor learns: voip.c holds it off while a voice
// is heard and a little after, so that the speaker does not key the
// talkie through its own microphone. ptt.c keys the talkie by it as by the
// button, with the same minute at most to an over.

#include "vox.h"

#define ENVELOPE_BITS 8
#define ENVELOPE_SHIFT 5
#define BACKGROUND_FALL_SHIFT 6
#define BACKGROUND_RISE_SHIFT 13
// the background is kept with this many bits more than the envelope: a
// step of a 2^-13 of the difference is less than one of the envelope's
#define BACKGROUND_EXTRA_BITS 8
#define TIMES_PART 10
#define MS_A_SECOND 1000

// the threshold over the background, in tenths, for each level, 1 to
// VOX_LEVEL_MOST
static const int times[VOX_LEVEL_MOST] = {120, 96, 76, 60, 48, 38, 30, 25, 21, 18};

void vox_begin(struct vox *v) {
	v->envelope = 0;
	v->background = 0;
	v->above = 0;
	v->hang = 0;
	v->is_keyed = 0;
}

// the level the envelope must be over, in samples' size; 0 when VOX is off
int vox_threshold(const struct vox *v, int level) {
	int32_t threshold;

	if (level <= VOX_OFF)
		return 0;
	if (level > VOX_LEVEL_MOST)
		level = VOX_LEVEL_MOST;
	threshold = (v->background >> (ENVELOPE_BITS + BACKGROUND_EXTRA_BITS)) * times[level - 1] / TIMES_PART;
	return threshold < VOX_LEAST ? VOX_LEAST : (int)threshold;
}

// the envelope, in samples' size, for the screen's meter
int vox_size(const struct vox *v) {
	return (int)(v->envelope >> ENVELOPE_BITS);
}

void vox_take(struct vox *v, int16_t sample, int level, int is_held_off, int rate_hz) {
	int32_t size = sample < 0 ? -(int32_t)sample : sample;
	int64_t difference;

	v->envelope += ((size << ENVELOPE_BITS) - v->envelope) >> ENVELOPE_SHIFT;
	if (level <= VOX_OFF || is_held_off) {
		v->above = 0;
		v->hang = 0;
		v->is_keyed = 0;
		return;
	}
	if (!v->is_keyed) {
		difference = ((int64_t)v->envelope << BACKGROUND_EXTRA_BITS) - v->background;
		v->background += (int32_t)(difference >> (difference < 0 ? BACKGROUND_FALL_SHIFT : BACKGROUND_RISE_SHIFT));
	}
	if (vox_size(v) >= vox_threshold(v, level)) {
		if (v->above < VOX_ATTACK_MS * rate_hz / MS_A_SECOND)
			v->above++;
		if (v->above >= VOX_ATTACK_MS * rate_hz / MS_A_SECOND) {
			v->is_keyed = 1;
			v->hang = VOX_HANG_MS * rate_hz / MS_A_SECOND;
		}
		return;
	}
	v->above = 0;
	if (v->is_keyed && --v->hang <= 0)
		v->is_keyed = 0;
}

int vox_is_keyed(const struct vox *v) {
	return v->is_keyed;
}
