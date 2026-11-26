// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ashhar Farhan
//
// speaker.h — the speaker, driven by PWM. See speaker.c.

#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void speaker_open(void);
void speaker_take(const uint8_t *ulaw, long now);
void speaker_poll(long now);
void speaker_mute(int is_muted);
void speaker_tone(int ms);
int speaker_is_ready(void);
void speaker_tone_at(int ms, uint32_t level);
void speaker_ready(int is_readying);
int speaker_is_toning(void);
void speaker_brief(char *text, int room);
void speaker_describe(char *text, int room);

#ifdef __cplusplus
}
#endif
