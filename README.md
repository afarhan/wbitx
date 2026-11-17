# wifitalkie

A walkie-talkie on WiFi, on an ESP32-C3, that speaks through my.sbitx.net's
VoIP repeater. To my.sbitx.net it is one more radio of its owner's account,
always in the mode VOIP: the dial, FREQ, is the room, and whoever speaks on
that kilohertz, from a zBitx, an sBitx or another talkie, is heard here.

This first one is set from the laptop over USB, a verb a line.

## Build, upload, talk to it

    make                          # arduino-cli, esp32:esp32 3.3.0, ESP32-C3, USB CDC on boot
    make upload PORT=/dev/ttyACM0
    make monitor                  # tools/talkie.py: type verbs
    make check                    # host tests, against zbitxre's accounts/voip.py

`tools/talkie.py STATUS` sends one verb and prints what comes back.

## First time

    WIFI "my ssid" mypassword
    CALL VU2ESE
    FREQ 7074000
    KEY                           # add this at my.sbitx.net as one of your radios
    STATUS                        # SERVICE ONLINE vu2ese-talkie, VOIP 7074 KHZ, 2 HERE

The first connection stretches the key, some seconds; it is kept after that.

## Verbs

    WIFI <ssid> <password>    CALL <callsign>     GRID <locator>
    FREQ <hz>                 SERVER <host> [port]
    KEY                       NEW_KEY (twice within 5 s)
    VOIP ONLINE|OFFLINE       PTT ON|OFF          TONE ON|OFF
    VOL <0-100>               MIC <0-100>         LEVELS ON|OFF
    STATUS                    RESTART

What the talkie says by itself: `WIFI UP`, `SERVICE ...`, `ROOM 7074 KHZ, 3 HERE`,
`HEARD VU3XYZ`, `OVER VU3XYZ`, `BUSY VU3XYZ`, `PTT ON`, `PTT OFF: ONE MINUTE`.

## Wiring (ESP32-C3 SuperMini; pins.h)

| GPIO | |
|---|---|
| 3 (ADC1) | microphone: MAX9814 module output |
| 4 | earbuds: 10 Ω, then an LC low-pass of about 5 kHz (1 mH in series and 1 µF to ground for 32 Ω earbuds; 470 µH and 2.2 µF for 16 Ω), then 220 µF (+ toward the C3) to tip and ring; sleeve to ground |
| 9 | PTT: the BOOT button, to ground |
| 8 | lamp: the board's LED, lit while speaking or hearing |

## Where the protocol is written

zbitxre's `PROTOCOL.md` ("Authentication", "VoIP: the repeater"),
`accounts/voip.py` and `clients/remote/zbitx_remote.c`.
`wifitalkie/sha256.c` is zbitxre's `clients/remote/sha256.c`, as it is.

## Updating over WiFi

`firmware/wifitalkie.ino.bin` is the firmware as `make release` puts it
there, with `firmware/version.txt`, the time it was built.

From GitHub: on the talkie, joined to a network, Settings > Update... >
From github.com, ENTER. It takes the file if it is newer than its own and
restarts into it; otherwise it says Up to date. The serial command
`UPDATE GITHUB` does the same.

Locally, from a phone or laptop: Settings > Update... > Locally on the
talkie, join its access point TALKIE-<callsign>, open 192.168.4.1, type
the four-figure code the talkie shows, choose the file and SEND. The
talkie and the page both show how far it has got; when it says done,
close the page.

Either way, a firmware that does not start goes back to the old one by
itself.

## License

GPL-3.0-or-later; see LICENSE.
