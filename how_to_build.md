# How to build and flash the wifitalkie

The firmware is an Arduino sketch, `wifitalkie/`, for an ESP32-C3 SuperMini.
It builds with the Arduino ESP32 core **3.3.0**, by `arduino-cli` (the
Makefile) or the Arduino IDE.

## 1. Install

### arduino-cli (Linux, macOS)

    curl -fsSL https://raw.githubusercontent.com/arduino/arduino-cli/master/install.sh | sh
    arduino-cli config init
    arduino-cli core update-index
    arduino-cli core install esp32:esp32@3.3.0

Also `make`, `tar` with `xz`, and for the console `python3` with pyserial:

    sudo apt install make xz-utils python3-serial      # Debian, Ubuntu
    pip install pyserial                               # or this

On Linux, to reach the board's USB port without root:

    sudo usermod -aG dialout $USER                     # then log out and in

### Arduino IDE (Windows, macOS, Linux)

1. Install Arduino IDE 2 from arduino.cc.
2. File → Preferences → Additional boards manager URLs:
   `https://espressif.github.io/arduino-esp32/package_esp32_index.json`
3. Tools → Board → Boards Manager: install **esp32 by Espressif Systems,
   version 3.3.0** (exactly that version).

No other libraries are needed: WiFi, WebServer, DNSServer and Update come
with the core.

## 2. Get the source

    git clone https://github.com/afarhan/wbitx.git
    cd wbitx

## 3. Build and flash

Plug the talkie in by USB-C. It shows up as `/dev/ttyACM0` on Linux,
`/dev/cu.usbmodem…` on macOS, or `COMx` on Windows.

### With make (recommended)

    make                            # builds build/wifitalkie.ino.bin
    make upload                     # flashes /dev/ttyACM0
    make upload PORT=/dev/ttyACM1   # another port
    make monitor                    # the console: type verbs, e.g. STATUS

`make` builds against our own ESP32-C3 libraries, `sdk/esp32c3.tar.xz`.
They are Espressif's, rebuilt with power management, so the talkie can idle
in light sleep at about 7 mA with its WiFi held. They are unpacked into
`build/sdk` the first time. Nothing is installed, and the core's own
libraries are left alone. `sdk/build-libs.sh` says how they were made.

The build is stamped with its time, in `wifitalkie/built.h`, which tells
Settings → Update whether GitHub has a newer firmware.

### With the Arduino IDE

1. Open `wifitalkie/wifitalkie.ino`.
2. Tools:
   - Board: **ESP32C3 Dev Module**
   - USB CDC On Boot: **Enabled**
   - Partition Scheme: **Minimal SPIFFS (1.9MB APP with OTA/190KB SPIFFS)**
   - Port: the talkie's
3. Upload.

The IDE builds against the core's stock libraries, which have no power
management. The talkie works fully, but when idle it stays in modem sleep,
which draws more current than light sleep, and the console says `POWER: THE
POWER MANAGEMENT WOULD NOT BE SET`. Builds from the IDE carry no build
time, so Update from github.com always offers the latest. For the
low-power firmware, use `make`.

### If the upload doesn't start

Hold BOOT, tap RESET, release BOOT, and upload again. After it finishes,
tap RESET.

## 4. Without building: the released firmware

`firmware/wifitalkie.ino.bin` is the latest release, built by `make release`.
A talkie that is already running takes it by itself: Settings → Update... →
From github.com. To flash a bare board with it:

    pip install esptool
    esptool.py --chip esp32c3 --port /dev/ttyACM0 write_flash 0x10000 firmware/wifitalkie.ino.bin

That writes only the application. A board that has never had the firmware
also needs the bootloader and partition table, so flash such a board once
with `make upload` or the IDE.

## 5. First time on the talkie

See README.md, "First time": WIFI, CALL, then KEY to add it at my.sbitx.net.

## For developers

    make check      # the plain C's tests on this computer, and the VoIP service's
    make release    # the build copied to firmware/, with firmware/version.txt;
                    # commit and push it, and talkies update from github.com
    make clean
