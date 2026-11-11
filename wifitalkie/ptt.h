// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ashhar Farhan
//
// ptt.h — the push-to-talk and the lamp. See ptt.c.

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

void ptt_poll(long now);
void ptt_set(int is_down);
int ptt_is_down(void);
int ptt_is_vox(void);
void ptt_lamp(int is_lit);
int ptt_is_lit(void);
void ptt_lend(int is_lending);

#ifdef __cplusplus
}
#endif
