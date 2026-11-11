// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ashhar Farhan
//
// portal.cpp — the setup portal: web pages the talkie serves from an
// access point of its own, for the WiFi networks and the contacts.
//
// portal_open() puts up an open access point, TALKIE-<callsign>, or
// TALKIE-<the last of its MAC> before a callsign is set (network.cpp), and
// answers every name asked of its DNS with its own address, so that a
// telephone joining it is taken to the first page by itself, as at a
// hotel's WiFi; any plain http address typed in comes to the same page. A
// browser that turns the address into https finds nothing, there being no
// https here, and http://192.168.4.1 has to be typed. The screen's SETUP
// page opens it and closes it, and opens it at power-up when the talkie
// knows no network at all. It is open only while that page is shown: the
// access point has no key, and anyone near could change the talkie's
// settings while it is up.
//
// The pages:
//
//     /            with no callsign set, the callsign page, first; with
//                  one, the networks:
//     /            the callsign as the title, with a pencil to change it;
//                  "Choose a network to join": the networks remembered,
//                  up to SETTINGS_KNOWN, each with its signal, or
//                  "Offline" if it is not heard; then the others heard,
//                  strongest first, each with its signal and a lock if it
//                  has a key; then "Another network" for one not heard or
//                  hidden. Each is a row to tap, with a chevron. Below,
//                  CONTACTS, with how many there are.
//     /network     one network, ?s=<its name>: its key, or the key kept
//                  for it left as it is if the box is left empty, an eye
//                  to show the key as typed, JOIN, and DELETE for one
//                  remembered
//     /callsign    the talkie's callsign, to set or change, and its code
//                  for my.sbitx.net, with MAKE A NEW CODE (service.c)
//     /contacts    the callsigns kept, up to SETTINGS_CONTACTS, each with
//                  DELETE, and ADD (contacts.c)
//     /update      new firmware (update.cpp): the update code shown on
//                  the talkie's Setup page, then, at /update?code=<it>,
//                  the file to send; sent, a page saying it is done and
//                  the talkie restarting, or why it failed
//
// One portal for all of it: each page is under one of four tabs, a bar at
// the top, the author's of 2026-11-11: User, the callsign and the
// activation code with MAKE A NEW CODE; WiFi, the networks (also /wifi);
// Contacts; and Firmware. A talkie with no callsign opens on User.
//
// A talkie new from its box, with no callsign or no network, is taken
// through the pages as a wizard of three steps, the author's of
// 2026-11-09: STEP 1 the callsign, STEP 2 the network, and STEP 3, on the
// page after JOIN, the activation code and what to do with it: back on
// the telephone's usual WiFi, at my.sbitx.net, make an account for the
// callsign, or sign in, and add a radio with this code. The code is also
// on the talkie, at Settings, Activate.
//
// JOIN keeps the network with its key (known.c) as the one set, says on
// the page that the talkie is joining it, and JOIN_CLOSE_MS later closes
// the access point and joins it as an ordinary WiFi client; the screen
// goes from SETUP to its page that says whether it joined
// (portal_joined()). The keys kept are never sent back.
//
// The other forms are answered by a redirect, with a line at the top of
// the page saying what was done.
//
// portal_poll() answers one DNS question and one web request a turn of the
// loop; a page is a few kilobytes, sent in a few milliseconds.

#include <Arduino.h>
#include <DNSServer.h>
#include <WebServer.h>
#include <WiFi.h>

#include "console.h"
#include "contacts.h"
#include "known.h"
#include "network.h"
#include "portal.h"
#include "service.h"
#include "settings.h"
#include "update.h"

#define DNS_PORT 53
#define WEB_PORT 80
#define NAME_PREFIX "TALKIE-"
#define NAME_ROOM 33
#define TEXT_ROOM 64
#define JOIN_CLOSE_MS 3000
#define PAGE_ROOM 6000

static WebServer server(WEB_PORT);
static DNSServer dns;
static int is_open;
static int is_first_time;
static int is_server_begun;
static char name[NAME_ROOM];
static String notice;
static long close_at_ms;
static int is_join_due;
static int is_joined_said;
static char joining[SETTINGS_SSID_ROOM];

// text made safe to put in the page
static String html(const char *text) {
	String out;

	for (; *text != 0; text++) {
		if (*text == '<')
			out += "&lt;";
		else if (*text == '>')
			out += "&gt;";
		else if (*text == '&')
			out += "&amp;";
		else if (*text == '"')
			out += "&quot;";
		else if (*text == '\'')
			out += "&#39;";
		else
			out += *text;
	}
	return out;
}

// text made safe to put in an address, after ?s=
static String in_address(const char *text) {
	const char *hex = "0123456789ABCDEF";
	String out;

	for (; *text != 0; text++) {
		unsigned char c = (unsigned char)*text;

		if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
			out += (char)c;
		} else {
			out += '%';
			out += hex[c >> 4];
			out += hex[c & 15];
		}
	}
	return out;
}

// the tab a page is under, for the bar of tabs at its top
#define TAB_NONE 0
#define TAB_USER 1
#define TAB_WIFI 2
#define TAB_CONTACTS 3
#define TAB_FIRMWARE 4
static int tab;

struct tab_link {
	int tab;
	const char *name;
	const char *to;
};

static const struct tab_link tabs[] = {
	{TAB_USER, "User", "/callsign"},
	{TAB_WIFI, "WiFi", "/wifi"},
	{TAB_CONTACTS, "Contacts", "/contacts"},
	{TAB_FIRMWARE, "Firmware", "/update"},
};
#define TABS ((int)(sizeof(tabs) / sizeof(tabs[0])))

static void begin_page(String &page, const char *heading) {
	int i;

	page.reserve(PAGE_ROOM);
	page += "<!doctype html><html><head><meta charset=utf-8>"
		"<meta name=viewport content='width=device-width,initial-scale=1'>"
		"<title>wifitalkie</title><style>"
		"body{font-family:sans-serif;margin:0 auto;padding:12px;max-width:480px;background:#fff;color:#111}"
		"h1{font-size:1.3em}h2{font-size:1.05em;margin:1.4em 0 .3em;color:#444}"
		"a{color:inherit;text-decoration:none}"
		".row{display:flex;align-items:center;gap:10px;padding:14px 8px;border-bottom:1px solid #ddd}"
		"a.row:active{background:#eef}.row .name{flex:1;font-size:1.05em}"
		".row small{color:#555}.chev{font-size:1.4em;color:#888}"
		".off{color:#a00}"
		".bars{display:inline-flex;align-items:flex-end;gap:2px;height:14px}"
		".bars i{display:block;width:4px;background:#111}"
		"input{width:100%;box-sizing:border-box;padding:10px;margin:6px 0;font-size:1em}"
		"button{padding:10px 16px;font-size:1em;margin:6px 6px 6px 0}"
		".go{background:#036;color:#fff;border:0;border-radius:4px}"
		".notice{background:#eef;padding:8px}.back{display:block;margin:8px 0;color:#036}"
		".key{display:flex;gap:6px;align-items:center}.key input{flex:1}.key button{margin:0}"
		".edit{font-size:.8em;color:#036;margin-left:8px}.code{font-size:1.3em;font-family:monospace}"
		"nav{display:flex;border-bottom:2px solid #036;margin:-4px 0 4px}"
		"nav a{flex:1;text-align:center;padding:10px 2px;color:#036}"
		"nav a.on{background:#036;color:#fff;border-radius:4px 4px 0 0}"
		"</style></head><body>";
	if (tab != TAB_NONE) {
		page += "<nav>";
		for (i = 0; i < TABS; i++) {
			page += "<a href=";
			page += tabs[i].to;
			page += tabs[i].tab == tab ? " class=on>" : ">";
			page += tabs[i].name;
			page += "</a>";
		}
		page += "</nav>";
	}
	page += "<h1>";
	page += heading;
	page += "</h1>";
	if (notice.length() > 0)
		page += "<p class=notice>" + html(notice.c_str()) + "</p>";
	notice = "";
}

static void end_page(String &page) {
	page += "</body></html>";
	server.send(200, "text/html; charset=utf-8", page);
}

static void again(const String &said, const String &to) {
	notice = said;
	console_say("SETUP: %s", said.c_str());
	server.sendHeader("Location", to, true);
	server.send(303, "text/plain", "");
}

// the signal in four bars, those not earned grey, as on the talkie's
// home page
static void bars(String &page, int rssi) {
	int earned = rssi >= -55 ? 4 : rssi >= -65 ? 3 : rssi >= -75 ? 2 : 1;
	int i;

	page += "<span class=bars>";
	for (i = 1; i <= 4; i++) {
		page += "<i style='height:";
		page += i * 25;
		page += i <= earned ? "%'></i>" : "%;background:#ccc'></i>";
	}
	page += "</span>";
}

// where ssid is among the networks heard, or -1
static int heard_at(const char *ssid, int *rssi, int *is_open_one) {
	char each[SETTINGS_SSID_ROOM];
	int i;

	for (i = 0; i < network_scanned_count(); i++) {
		network_scanned(i, each, sizeof(each), rssi, is_open_one);
		if (!strcmp(each, ssid))
			return i;
	}
	return -1;
}

// a row to tap for a network: its name, a mark if joined now, its signal
// or Offline, a lock if it has a key
static void network_row(String &page, const char *ssid, const char *joined, int is_heard,
		int rssi, int is_open_one) {
	page += "<a class=row href='/network?s=" + in_address(ssid) + "'><span class=name>" + html(ssid);
	if (!strcmp(ssid, joined))
		page += " <small>&#10003; joined</small>";
	page += "</span>";
	if (is_heard) {
		bars(page, rssi);
		page += "<small>";
		page += rssi;
		page += " dBm</small>";
		page += is_open_one ? "<small>open</small>" : "<small>&#128274;</small>";
	} else {
		page += "<small class=off>Offline</small>";
	}
	page += "<span class=chev>&rsaquo;</span></a>";
}

static void show_networks(void) {
	char joined[SETTINGS_SSID_ROOM];
	char address[TEXT_ROOM];
	char ssid[SETTINGS_SSID_ROOM];
	String page;
	int count = known_count(&settings);
	int others = 0;
	int rssi;
	int is_open_one;
	int i;

	console_say("SETUP: THE NETWORKS TO %s", server.client().remoteIP().toString().c_str());
	tab = TAB_WIFI;
	begin_page(page, "WiFi");
	network_ssid(joined, sizeof(joined));
	network_address(address, sizeof(address));
	page += "<p>";
	if (joined[0] != 0)
		page += "Joined to <b>" + html(joined) + "</b>, " + address + ".";
	else
		page += "Not joined to any network.";
	if (is_first_time)
		page += "</p><p><b>Step 2 of 3.</b> Choose the WiFi the talkie will use:</p>";
	else
		page += "</p><p><b>Choose a network to join:</b></p>";
	if (network_is_scanning())
		page += "<p>Listening for networks&hellip; <a href=/ style='color:#036'>refresh</a></p>";
	if (count > 0)
		page += "<h2>Remembered</h2>";
	for (i = 0; i < count; i++) {
		int at = heard_at(settings.known_ssid[i], &rssi, &is_open_one);

		network_row(page, settings.known_ssid[i], joined, at >= 0, rssi, is_open_one);
	}
	for (i = 0; i < network_scanned_count(); i++) {
		network_scanned(i, ssid, sizeof(ssid), &rssi, &is_open_one);
		if (known_find(&settings, ssid) >= 0)
			continue;
		if (others++ == 0)
			page += "<h2>Heard</h2>";
		network_row(page, ssid, joined, 1, rssi, is_open_one);
	}
	page += "<a class=row href='/network?s='><span class=name>Another network&hellip;</span>"
		"<span class=chev>&rsaquo;</span></a>"
		"<p><a href=/scan style='color:#036'>Listen again</a></p>"
		"<p><small>Press MENU on the talkie when done; this access point closes.</small></p>";
	end_page(page);
}

// one network: its key, JOIN, and DELETE if it is remembered
static void show_network(void) {
	String ssid = server.arg("s");
	String page;
	int known = known_find(&settings, ssid.c_str());
	int rssi;
	int is_open_one = 0;
	int at = ssid.length() > 0 ? heard_at(ssid.c_str(), &rssi, &is_open_one) : -1;

	tab = TAB_WIFI;
	begin_page(page, ssid.length() > 0 ? html(ssid.c_str()).c_str() : "Another network");
	page += "<a class=back href=/>&lsaquo; All networks</a>";
	if (at >= 0) {
		page += "<p>";
		bars(page, rssi);
		page += " ";
		page += rssi;
		page += is_open_one ? " dBm, open" : " dBm";
		page += "</p>";
	} else if (ssid.length() > 0) {
		page += "<p class=off>Not heard now.</p>";
	}
	page += "<form method=post action=/wifi/join>";
	if (ssid.length() > 0)
		page += "<input type=hidden name=ssid value=\"" + html(ssid.c_str()) + "\">";
	else
		page += "<input name=ssid placeholder='Network name' maxlength=32 autocapitalize=off autofocus>";
	if (at >= 0 && is_open_one) {
		page += "<p>An open network: no key.</p>";
	} else {
		page += "<div class=key><input id=k name=key type=password maxlength=64 placeholder='";
		page += known >= 0 ? "Key (empty: keep the one saved)" : "Key";
		page += "'";
		if (ssid.length() > 0)
			page += " autofocus";
		page += "><button type=button title='Show the key' "
			"onclick=\"k.type=k.type=='password'?'text':'password'\">&#128065;</button></div>";
	}
	page += "<button class=go>Join</button></form>";
	if (known >= 0) {
		page += "<form method=post action=/wifi/forget><input type=hidden name=ssid value=\"" +
			html(ssid.c_str()) + "\"><button>Delete</button></form>";
	}
	page += "<p><small>Joining closes this access point; the talkie then joins the network "
		"and says on its screen whether it did.</small></p>";
	end_page(page);
}

// the callsign, to set or change; with one set, the code for my.sbitx.net
static void show_callsign(void) {
	char shown[AUTH_COOKIE_SHOWN_ROOM];
	String page;
	int is_set = settings.callsign[0] != 0;

	tab = TAB_USER;
	begin_page(page, is_set ? "User" : "Welcome");
	if (!is_set)
		page += "<p><b>Step 1 of 3.</b> The callsign this talkie goes by.</p>";
	else
		page += "<h2>Callsign</h2>";
	page += "<form method=post action=/callsign>"
		"<input name=call placeholder=Callsign maxlength=15 autocapitalize=characters autofocus value=\"";
	page += html(settings.callsign);
	page += "\"><button class=go>Save</button></form>";
	if (is_set) {
		service_code(shown);
		page += "<h2>Activation code</h2><p class=code>";
		page += html(shown);
		page += "</p><p><small>Add this code at my.sbitx.net as a radio of ";
		page += html(settings.callsign);
		page += ". A new code is needed only if this one is known to others; "
			"the old one then no longer lets this talkie in.</small></p>"
			"<form method=post action=/code/new><button>Make a new code</button></form>";
	}
	end_page(page);
}

static void callsign_set(void) {
	char call[SETTINGS_CALLSIGN_ROOM];

	if (!contacts_callsign(server.arg("call").c_str(), call)) {
		again("A callsign is letters, figures and / only, up to 15.", "/callsign");
		return;
	}
	if (strcmp(call, settings.callsign)) {
		snprintf(settings.callsign, sizeof(settings.callsign), "%s", call);
		settings_save();
		service_restart();
	}
	again(String("The callsign is ") + call + ".", "/");
}

static void code_new(void) {
	char shown[AUTH_COOKIE_SHOWN_ROOM];

	service_new_key();
	service_code(shown);
	again(String("A new code: ") + shown + ". Add it at my.sbitx.net in place of the old one.", "/callsign");
}

// the first page: the callsign, if it is not set yet, or the networks
// the Update page: the code asked first, then the file
static void update_heading(String &page) {
	tab = TAB_FIRMWARE;
	begin_page(page, "Firmware");
}

static int is_code_right(void) {
	return update_is_open() && server.hasArg("code") && server.arg("code").toInt() == update_code();
}

static void show_update(void) {
	String page;

	update_heading(page);
	if (!is_code_right()) {
		if (server.hasArg("code"))
			page += "<p class=off>That is not the code on the talkie.</p>";
		page += "<form method=get action='/update'>"
			"<p>Type the update code shown on the talkie's Setup page, four figures.</p>"
			"<input name=code inputmode=numeric autocomplete=off maxlength=4>"
			"<button class=go>NEXT</button></form>";
		end_page(page);
		return;
	}
	page += "<form method=post enctype='multipart/form-data' action='/update?code=";
	page += update_code();
	page += "'><p>Choose the firmware file, <b>wifitalkie.ino.bin</b>, and SEND. "
		"It takes some seconds; keep this page open until it says it is done.</p>"
		"<input type=file name=firmware accept='.bin' required>"
		"<button class=go>SEND</button></form>";
	end_page(page);
}

// the file, a piece at a time, as the browser sends it
static void update_receive(void) {
	HTTPUpload &upload = server.upload();
	static int is_refused;

	if (upload.status == UPLOAD_FILE_START) {
		is_refused = !is_code_right() || !update_begin((size_t)server.clientContentLength());
	} else if (upload.status == UPLOAD_FILE_WRITE) {
		if (!is_refused && !update_take(upload.buf, upload.currentSize))
			is_refused = 1;
	} else if (upload.status == UPLOAD_FILE_END) {
		if (!is_refused && !update_end())
			is_refused = 1;
	} else if (upload.status == UPLOAD_FILE_ABORTED) {
		update_abort("THE SENDING STOPPED");
	}
}

// the file all sent: whether it was taken
static void update_sent(void) {
	String page;
	char said[UPDATE_TEXT_ROOM];

	update_heading(page);
	update_said(said, sizeof(said));
	if (!strncmp(said, "DONE", 4)) {
		page += "<p>The firmware is in. The talkie is restarting into it; this page "
			"is finished, and the talkie's own WiFi goes with it.</p>"
			"<p>If the new firmware does not start properly, the talkie goes back "
			"to the old one by itself.</p>";
	} else if (!is_code_right()) {
		page += "<p class=off>Refused: the code was not right, or Setup was closed "
			"on the talkie.</p><a class=back href='/update'>&lsaquo; Try again</a>";
	} else {
		page += "<p class=off>Not taken: ";
		page += html(said);
		page += "</p><p>The talkie still runs the firmware it had.</p>"
			"<a class=back href='/update?code=";
		page += update_code();
		page += "'>&lsaquo; Try again</a>";
	}
	end_page(page);
}

static void show_first(void) {
	if (settings.callsign[0] == 0)
		show_callsign();
	else
		show_networks();
}

static void show_contacts(void) {
	String page;
	int count = contacts_count(&settings);
	int i;

	tab = TAB_CONTACTS;
	begin_page(page, "Contacts");
	page += "<a class=back href=/>&lsaquo; Networks</a><p>";
	page += count;
	page += " of ";
	page += SETTINGS_CONTACTS;
	page += "</p>";
	for (i = 0; i < count; i++) {
		page += "<div class=row><span class=name>" + html(settings.contacts[i]) + "</span>"
			"<form method=post action=/contact/forget><input type=hidden name=i value=";
		page += i;
		page += "><button>Delete</button></form></div>";
	}
	page += "<form method=post action=/contact/add>"
		"<input name=call placeholder=Callsign maxlength=15 autocapitalize=characters>"
		"<button class=go>Add</button></form>";
	end_page(page);
}

static void show_activation(void);

// JOIN: the network kept and set; the page says so, and the access point
// closes JOIN_CLOSE_MS later, in portal_poll()
static void wifi_join(void) {
	String ssid = server.arg("ssid");
	String key = server.arg("key");
	String page;
	int known;
	int rssi;
	int is_open_one = 0;

	ssid.trim();
	if (ssid.length() == 0 || ssid.length() >= SETTINGS_SSID_ROOM || key.length() >= SETTINGS_PASSWORD_ROOM) {
		again("A network needs a name of up to 32 characters, and a key of up to 64.", "/");
		return;
	}
	known = known_find(&settings, ssid.c_str());
	if (key.length() == 0 && known >= 0)
		key = settings.known_password[known];
	heard_at(ssid.c_str(), &rssi, &is_open_one);
	if (key.length() > 0 && key.length() < 8) {
		again("A WiFi key is 8 characters at least.", "/network?s=" + in_address(ssid.c_str()));
		return;
	}
	if (key.length() == 0 && !is_open_one && known < 0) {
		again("This network needs its key.", "/network?s=" + in_address(ssid.c_str()));
		return;
	}
	known_remember(&settings, ssid.c_str(), key.c_str());
	snprintf(settings.ssid, sizeof(settings.ssid), "%s", ssid.c_str());
	snprintf(settings.password, sizeof(settings.password), "%s", key.c_str());
	settings_save();
	snprintf(joining, sizeof(joining), "%s", ssid.c_str());
	console_say("SETUP: JOIN %s", joining);
	if (is_first_time)
		show_activation();
	else {
		tab = TAB_WIFI;
		begin_page(page, "Joining");
		page += "<p>The talkie is closing this access point and joining <b>" + html(joining) +
			"</b>. Its screen says whether it joined.</p>"
			"<p>Put this phone back on its usual WiFi.</p>";
		end_page(page);
	}
	close_at_ms = (long)millis() + JOIN_CLOSE_MS;
	is_join_due = 1;
}

// STEP 3, after JOIN on a talkie new from its box: the activation code
// and what to do with it
static void show_activation(void) {
	char shown[AUTH_COOKIE_SHOWN_ROOM];
	String page;

	service_code(shown);
	tab = TAB_USER;
	begin_page(page, "Activate");
	page += "<p><b>Step 3 of 3.</b> The talkie is joining <b>" + html(joining) +
		"</b>; its screen says whether it joined.</p>"
		"<p>Your activation code:</p><p class=code>";
	page += html(shown);
	page += "</p><p>Take a screenshot of this page. Then:</p><ol>"
		"<li>Put this phone back on its usual WiFi.</li>"
		"<li>Go to <a style='color:#036' href=https://my.sbitx.net/><b>my.sbitx.net</b></a> and "
		"make an account for <b>";
	page += html(settings.callsign);
	page += "</b>, or sign in if you have one.</li>"
		"<li>Add a radio, and enter the code above.</li></ol>"
		"<p>The talkie comes online by itself within a minute. The code is also "
		"on the talkie, at Settings, Activate.</p>";
	end_page(page);
}

static void wifi_forget(void) {
	String ssid = server.arg("ssid");
	int i = known_find(&settings, ssid.c_str());

	if (i < 0) {
		again("No such network.", "/");
		return;
	}
	if (!strcmp(settings.ssid, ssid.c_str())) {
		settings.ssid[0] = 0;
		settings.password[0] = 0;
	}
	known_forget(&settings, i);
	settings_save();
	again("Deleted " + ssid + ".", "/");
}

static void contact_add(void) {
	String call = server.arg("call");

	if (contacts_count(&settings) == SETTINGS_CONTACTS) {
		again("The contacts are full; delete one first.", "/contacts");
		return;
	}
	if (!contacts_add(&settings, call.c_str())) {
		again("Not added: a callsign is letters, figures and / only, and is not there already.", "/contacts");
		return;
	}
	settings_save();
	call.toUpperCase();
	call.trim();
	again("Added " + call + ".", "/contacts");
}

static void contact_forget(void) {
	int i = server.arg("i").toInt();
	String call;

	if (!server.hasArg("i") || i < 0 || i >= contacts_count(&settings)) {
		again("No such contact.", "/contacts");
		return;
	}
	call = settings.contacts[i];
	contacts_forget(&settings, i);
	settings_save();
	again("Deleted " + call + ".", "/contacts");
}

static void scan(void) {
	network_scan_begin();
	again("Listening for networks; refresh in a few seconds.", "/");
}

// any other address, as a telephone asks to see if it is behind a portal:
// sent to the first page
static void elsewhere(void) {
	char address[TEXT_ROOM];

	network_ap_address(address, sizeof(address));
	console_say("SETUP: %s%s SENT TO THE PAGE", server.hostHeader().c_str(), server.uri().c_str());
	server.sendHeader("Location", String("http://") + address + "/", true);
	server.send(302, "text/plain", "");
}

static void begin_server(void) {
	server.on("/", HTTP_GET, show_first);
	server.on("/callsign", HTTP_GET, show_callsign);
	server.on("/callsign", HTTP_POST, callsign_set);
	server.on("/code/new", HTTP_POST, code_new);
	server.on("/network", HTTP_GET, show_network);
	server.on("/contacts", HTTP_GET, show_contacts);
	server.on("/wifi/join", HTTP_POST, wifi_join);
	server.on("/wifi/forget", HTTP_POST, wifi_forget);
	server.on("/contact/add", HTTP_POST, contact_add);
	server.on("/contact/forget", HTTP_POST, contact_forget);
	server.on("/scan", HTTP_GET, scan);
	server.on("/wifi", HTTP_GET, show_networks);
	server.on("/update", HTTP_GET, show_update);
	server.on("/update", HTTP_POST, update_sent, update_receive);
	server.onNotFound(elsewhere);
	is_server_begun = 1;
}

void portal_open(void) {
	uint8_t mac[6];

	if (is_open)
		return;
	if (settings.callsign[0] != 0) {
		snprintf(name, sizeof(name), "%s%s", NAME_PREFIX, settings.callsign);
	} else {
		WiFi.macAddress(mac);
		snprintf(name, sizeof(name), "%s%02X%02X", NAME_PREFIX, mac[4], mac[5]);
	}
	if (!network_ap_begin(name))
		return;
	if (!is_server_begun)
		begin_server();
	server.begin();
	dns.setErrorReplyCode(DNSReplyCode::NoError);
	dns.start(DNS_PORT, "*", WiFi.softAPIP());
	network_scan_begin();
	is_join_due = 0;
	is_first_time = settings.callsign[0] == 0 || known_count(&settings) == 0;
	update_open();
	is_open = 1;
}

void portal_close(void) {
	if (!is_open)
		return;
	update_close();
	dns.stop();
	server.stop();
	network_ap_end();
	is_open = 0;
	is_join_due = 0;
	console_say("SETUP: CLOSED");
}

void portal_poll(long now) {
	if (!is_open)
		return;
	dns.processNextRequest();
	server.handleClient();
	if (is_join_due && now - close_at_ms >= 0) {
		portal_close();
		network_join();
		is_joined_said = 1;
	}
}

int portal_is_open(void) {
	return is_open;
}

void portal_name(char *text, int room) {
	snprintf(text, room, "%s", name);
}

// 1, once, after a JOIN at the portal has closed it: the network being
// joined, for the screen to show
int portal_joined(char *ssid, int room) {
	if (!is_joined_said)
		return 0;
	is_joined_said = 0;
	snprintf(ssid, room, "%s", joining);
	return 1;
}
