#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Ashhar Farhan
"""
test_service.py — the talkie's answers and packets, held against
my.sbitx.net's own code.

    make check

The talkie's C, built for this computer as tests/voip_tool, makes the
stretch, the handshake's proof, the stream's keys and the packets. They are
checked here with Python's hashlib and hmac, as accounts/accounts.py and
accounts/remote.py check them, and the packets are given to the repeater
itself, accounts/voip.py from zbitxre, with a clock of this test's own: the
talkie and a radio join a channel, the talkie speaks and the radio is sent
its speech, the radio speaks and what the repeater sends the talkie is
opened by the talkie's C. No socket is opened.

ZBITXRE names the folder of zbitxre; by default the author's copy.
"""

import hashlib
import hmac
import os
import subprocess
import sys
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
ZBITXRE = os.environ.get("ZBITXRE", "/home/farhan/Documents/hfsigs/claude_sbitx")
sys.path.insert(0, os.path.join(ZBITXRE, "accounts"))
import voip

TOOL = os.path.join(HERE, "voip_tool")
COOKIE = "abcdefghjkmn"
CALLSIGN = "VU2ESE"
OTHER = "VU3XYZ"
CHALLENGE = "5f1c0a9e" * 4
NONCE = "8d2e" * 8
TALKIE_ID = bytes.fromhex("3f9a1c2b4d5e6f70")
RADIO_ID = bytes.fromhex("0102030405060708")
TALKIE_ADDRESS = ("198.51.100.7", 40001)
RADIO_ADDRESS = ("198.51.100.9", 40002)
HZ = 7074000
STEP = 0.02


def tool(*words):
    return subprocess.run([TOOL] + [str(w) for w in words], check=True,
                          capture_output=True, text=True).stdout.strip()


class ServiceTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.stretched = tool("stretch", COOKIE, CALLSIGN)

    def test_stretch_is_the_service_s(self):
        theirs = hashlib.pbkdf2_hmac("sha256", COOKIE.encode(),
                                     ("zbitx-1 " + CALLSIGN.lower()).encode(), 50000).hex()
        self.assertEqual(self.stretched, theirs)

    def test_proofs_are_the_service_s(self):
        for side in ("radio", "server"):
            text = "zbitx-auth-1 %s %s %s %s" % (side, CALLSIGN, CHALLENGE, NONCE)
            theirs = hmac.new(self.stretched.encode(), text.encode(), hashlib.sha256).hexdigest()
            self.assertEqual(tool("proof", self.stretched, side, CALLSIGN, CHALLENGE, NONCE), theirs)

    def test_keys_are_the_service_s(self):
        for words in ("zbitx-voip-up-1", "zbitx-voip-down-1"):
            theirs = voip.key_for(words, self.stretched, NONCE).decode()
            self.assertEqual(tool("key", self.stretched, words, NONCE), theirs)

    def test_through_the_repeater(self):
        sent = []
        told = []
        repeater = voip.Voip(lambda packet, address: sent.append((packet, address)),
                             lambda radio, line: told.append((radio, line)), 2500)
        repeater.open("talkie", TALKIE_ID, CALLSIGN, self.stretched, NONCE)
        radio = repeater.open("radio", RADIO_ID, OTHER, "radio's stretch", NONCE)
        up = tool("key", self.stretched, "zbitx-voip-up-1", NONCE)
        down = tool("key", self.stretched, "zbitx-voip-down-1", NONCE)
        now = 1000.0

        # both say where they are tuned, a few hundred Hz apart: one channel
        repeater.take(bytes.fromhex(tool("make", TALKIE_ID.hex(), up, 0, HZ + 300, 1)),
                      TALKIE_ADDRESS, now)
        repeater.take(voip.packet(RADIO_ID, radio.up_key, voip.KIND_TUNED, "", HZ - 200, 1),
                      RADIO_ADDRESS, now)
        self.assertIn(("talkie", "VOIP ROOM 7074 2"), told)

        # the talkie speaks; the radio is sent it, under the account's callsign
        sound = bytes(range(160))
        for number in range(2, 12):
            now += STEP
            repeater.take(bytes.fromhex(tool("make", TALKIE_ID.hex(), up, 3, HZ + 300,
                                             number, sound.hex())), TALKIE_ADDRESS, now)
        to_radio = [packet for packet, address in sent if address == RADIO_ADDRESS]
        self.assertEqual(len(to_radio), 10)
        body = to_radio[0][:-voip.TAG_BYTES]
        self.assertEqual(voip.tag_of(radio.down_key, body), to_radio[0][-voip.TAG_BYTES:])
        self.assertEqual(body[10:20].rstrip(b"\0").decode(), CALLSIGN)
        self.assertEqual(body[28:188], sound)

        # the radio answers after its turn; the talkie opens what it is sent
        now += 5
        sent.clear()
        for number in range(2, 7):
            now += STEP
            repeater.take(voip.packet(RADIO_ID, radio.up_key, voip.KIND_SPEECH, "", HZ, number,
                                      bytes([0x55]) * 160), RADIO_ADDRESS, now)
        to_talkie = [packet for packet, address in sent if address == TALKIE_ADDRESS]
        self.assertEqual(len(to_talkie), 5)
        opened = tool("open", TALKIE_ID.hex(), down, to_talkie[0].hex()).split()
        self.assertEqual(opened[:4], ["OPENED", OTHER, str(HZ), "1"])
        self.assertEqual(opened[4], "55" * 160)

        # a packet changed on its way, or tagged with the up key, is refused
        changed = bytearray(to_talkie[1])
        changed[30] ^= 1
        self.assertEqual(tool("open", TALKIE_ID.hex(), down, changed.hex()), "WRONG_TAG")
        self.assertEqual(tool("open", TALKIE_ID.hex(), up, to_talkie[1].hex()), "WRONG_TAG")
        self.assertEqual(tool("open", RADIO_ID.hex(), down, to_talkie[1].hex()), "NOT_OURS")


if __name__ == "__main__":
    unittest.main()
