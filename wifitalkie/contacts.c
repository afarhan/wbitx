// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ashhar Farhan
//
// contacts.c — the callsigns the talkie keeps, up to SETTINGS_CONTACTS.
//
// They are entered and deleted at the setup portal (portal.cpp), and
// listed on the screen's menu, in the order they were added, to be
// chosen, entered, edited and deleted there. Each has two flags, a bit
// in contacts_quiet and contacts_blocked at its place, which move with
// it; and the target, settings.target, follows a contact moved up, or
// goes to F1 if its contact is deleted. A callsign
// is kept in capitals, letters, figures and the stroke only, as typed on a
// telephone it may come in small letters or with spaces round it; one
// already there is not added twice. It knows nothing of the ESP32, so
// tests/test_units.c tries it.

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "contacts.h"

int contacts_count(const struct settings *s) {
	int i;

	for (i = 0; i < SETTINGS_CONTACTS && s->contacts[i][0] != 0; i++)
		;
	return i;
}

// where callsign is, or -1
int contacts_find(const struct settings *s, const char *callsign) {
	int i;

	if (callsign[0] == 0)
		return -1;
	for (i = 0; i < SETTINGS_CONTACTS; i++) {
		if (!strcmp(s->contacts[i], callsign))
			return i;
	}
	return -1;
}

// typed into call, in capitals, without spaces round it: 1 if it is a
// callsign, letters, figures and the stroke only, and fits; for the
// talkie's own callsign at the portal too
int contacts_callsign(const char *typed, char call[SETTINGS_CALLSIGN_ROOM]) {
	int length = 0;

	while (*typed == ' ')
		typed++;
	for (; *typed != 0 && *typed != ' '; typed++) {
		if (!isalnum((unsigned char)*typed) && *typed != '/')
			return 0;
		if (length == SETTINGS_CALLSIGN_ROOM - 1)
			return 0;
		call[length++] = (char)toupper((unsigned char)*typed);
	}
	while (*typed == ' ')
		typed++;
	call[length] = 0;
	return *typed == 0 && length > 0;
}

// 1 if added; 0 if it is not a callsign, is there already, or there is no
// room
int contacts_add(struct settings *s, const char *callsign) {
	char call[SETTINGS_CALLSIGN_ROOM];
	int count = contacts_count(s);

	if (!contacts_callsign(callsign, call) || count == SETTINGS_CONTACTS || contacts_find(s, call) >= 0)
		return 0;
	snprintf(s->contacts[count], sizeof(s->contacts[count]), "%s", call);
	s->contacts_quiet &= ~(1u << count);
	s->contacts_blocked &= ~(1u << count);
	return 1;
}

// mask with bit i taken out, those above it moved down
static uint32_t without_bit(uint32_t mask, int i) {
	uint32_t below = mask & ((1u << i) - 1);

	return below | ((mask >> (i + 1)) << i);
}

// the i'th taken out, those after it moved up; 1 if there was one
int contacts_forget(struct settings *s, int i) {
	uint32_t target = SETTINGS_TARGET_CONTACT + (uint32_t)i;

	if (i < 0 || i >= SETTINGS_CONTACTS || s->contacts[i][0] == 0)
		return 0;
	s->contacts_quiet = without_bit(s->contacts_quiet, i);
	s->contacts_blocked = without_bit(s->contacts_blocked, i);
	if (s->target == target)
		s->target = 0;
	else if (s->target > target)
		s->target--;
	for (; i < SETTINGS_CONTACTS - 1; i++)
		memcpy(s->contacts[i], s->contacts[i + 1], sizeof(s->contacts[i]));
	s->contacts[i][0] = 0;
	return 1;
}

// the i'th made callsign, in its place; 1 if it was, 0 if there is no
// i'th, callsign is not a callsign, or another is it already
int contacts_replace(struct settings *s, int i, const char *callsign) {
	char call[SETTINGS_CALLSIGN_ROOM];
	int found;

	if (i < 0 || i >= contacts_count(s) || !contacts_callsign(callsign, call))
		return 0;
	found = contacts_find(s, call);
	if (found >= 0 && found != i)
		return 0;
	snprintf(s->contacts[i], sizeof(s->contacts[i]), "%s", call);
	return 1;
}
