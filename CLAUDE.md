# wBitx (wifitalkie): notes for a coding agent

A walkie-talkie on WiFi, on an ESP32-C3 SuperMini, that talks through
my.sbitx.net's VoIP repeater. The firmware is the Arduino sketch in
`wifitalkie/`; the board is `pcb/wificomms/`; the printed cases are in `case/`
(which has its own CLAUDE.md, with rules for every case change).

## Build, flash, test

    make                 # build/wifitalkie.ino.bin, with arduino-cli and the ESP32 core 3.3.0
    make upload          # flash /dev/ttyACM0 (PORT=... for another)
    make monitor         # the talkie's console: type verbs, e.g. STATUS, MEMORY
    make check           # host tests: tests/test_units.c, and tests/test_service.py
    make release         # copy the build to firmware/ for Settings, Update..., From github.com

`how_to_build.md` explains installing the tools and the Arduino IDE route.
The build links against our own ESP-IDF libraries (`sdk/esp32c3.tar.xz`, made
by `sdk/build-libs.sh`), which have power management so the talkie can idle in
light sleep. Don't swap them for the core's own.

After a firmware change, flash it to the plugged-in talkie and check it on the
device. Commit only once the change has been tried on a real talkie.

## How the code is laid out

- `wifitalkie.ino`: `setup()` opens each module, and `loop()` calls each
  module's `*_poll(now)` in turn. Nothing may block the loop. Slow work, such as
  a TLS request, goes on a FreeRTOS task of its own, as in `enrol.c` and
  `update.cpp`. A poll that takes 150 ms or more is reported on the console as
  `SLOW:`.
- One module per file, a `.c`/`.cpp` with its `.h`. Each file opens with a
  comment that says what it does and why. Read it before changing the file,
  and keep it true when you do.
- Plain C (C11) wherever the ESP32 isn't needed. Those files are listed in
  `PLAIN` in the Makefile and tested on the host by `tests/test_units.c`. Add a
  test there for new plain logic.
- The modules, by area:
  - front panel: `keys`, `button`, `screen`, `display`, `font`, `entry`;
  - voice out: `ptt`, `microphone`, `speech`, `telephone`, `vox`;
  - voice in: `voip`, `voip_packet`, `speaker`, `playback`;
  - network: `network`, `known`, `portal`, `service`, `auth`, `sha256`,
    `enrol`;
  - text and Morse: `morse`, `text`;
  - the rest: `settings`, `power`, `sleep`, `battery`, `update`, `local`,
    `console`, `verbs`.
- `pins.h` is the board's pin map. `settings.c` keeps one struct in NVS. Adding
  a field changes the struct's size, so check how `settings.c` handles a stored
  struct of the old size.
- The protocol to my.sbitx.net (authentication, the line, VoIP packets, text)
  is PROTOCOL.md in zbitxre, the server's repository. `voip_packet.c` and
  `auth.c` must match it byte for byte.

## Constraints

- **RAM is tight.** The C3 has no PSRAM, and its IRAM and DRAM share SRAM.
  TLS (activation, updates) needs about 40 KB of free heap at its peak. Check
  `MEMORY` on the console before and after adding a buffer. A large static
  array can stop activation working.
- **Power.** The talkie idles at about 7 mA (`power.c`): the display off, the
  microphones off, the WiFi in modem sleep and the C3 in light sleep. New code
  must not keep it awake. Do work in a poll only when there's something to do.
- **Keys.** Four keys, MENU, UP, DOWN and ENTER, plus the PTT. Every page's
  help line in `screen.c` says what the keys do there; keep it up to date.
- **Style.** Tabs, short functions, `is_` for flags, lower-case names. Comments
  are plain sentences about why, not what.
