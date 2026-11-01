// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ashhar Farhan
//
// voip.h — the talkie's VoIP stream with my.sbitx.net. See voip.c.

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

struct sockaddr_in;

void voip_open(const struct sockaddr_in *service, const char *id_hex, const char *nonce,
	const char *stretched);
void voip_close(void);
void voip_poll(long now);
void voip_room_heard(long channel, int count);
void voip_busy_heard(const char *why, long now);
void voip_tone(int is_on);
void voip_describe(char *text, int room);
void voip_counts(char *text, int room);
int voip_here(void);
int voip_busy(char *why, int room_size);
int voip_hearing(char *callsign, int room);

#ifdef __cplusplus
}
#endif
