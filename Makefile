# wifitalkie: the ESP32-C3 firmware, built and uploaded with arduino-cli,
# and the tests of its plain C on this computer.
#
#   make                     build the firmware into build/
#   make upload PORT=...     upload it (default /dev/ttyACM0)
#   make monitor             type verbs at the talkie (tools/talkie.py)
#   make check               the host tests
#   make release             the firmware built put in firmware/, with
#                            firmware/version.txt, for Update... From
#                            github.com once committed and pushed
#
# The ESP32-C3 libraries the core builds against are our own, sdk/
# esp32c3.tar.xz: Espressif's, rebuilt with power management so that the
# talkie can sleep with its WiFi held (sdk/build-libs.sh says how). They are
# unpacked into build/sdk the first time and arduino-cli is pointed at them;
# nothing is installed, and the core's own are left as they are. The core
# compiled against them is kept apart, in build/cache, and the sketch's
# objects in build/sketch, so that nothing made against the core's own
# libraries is taken for them. They are
# for the core ESP32 3.3.0, which the build checks for:
#
#   arduino-cli core install esp32:esp32@3.3.0
#
# Each build is stamped with the time it was built, FIRMWARE_BUILT in
# wifitalkie/built.h, made here and not kept in git: the firmware's own
# description carries the core library's date, not the sketch's, so the
# talkie tells a newer firmware on GitHub by this (update.cpp).

FQBN = esp32:esp32:esp32c3:CDCOnBoot=cdc,PartitionScheme=min_spiffs
PORT ?= /dev/ttyACM0
ZBITXRE ?= /home/farhan/Documents/hfsigs/claude_sbitx
CFLAGS = -O2 -Wall -Wextra -std=c11 -D_DEFAULT_SOURCE
CORE = 3.3.0
SDK = build/sdk/esp32c3

PLAIN = wifitalkie/auth.c wifitalkie/battery.c wifitalkie/button.c wifitalkie/contacts.c wifitalkie/dial.c wifitalkie/entry.c wifitalkie/morse.c wifitalkie/text.c wifitalkie/known.c wifitalkie/playback.c wifitalkie/sha256.c wifitalkie/speech.c wifitalkie/telephone.c wifitalkie/voip_packet.c wifitalkie/vox.c

all: build/wifitalkie.ino.bin

$(SDK)/sdkconfig: sdk/esp32c3.tar.xz
	@arduino-cli core list | grep -q "^esp32:esp32 *$(CORE) " || { \
		echo "the ESP32 core $(CORE) is needed: arduino-cli core install esp32:esp32@$(CORE)"; exit 1; }
	rm -rf build/sdk
	mkdir -p build/sdk
	tar -xJf sdk/esp32c3.tar.xz -C build/sdk
	touch $@

build/wifitalkie.ino.bin: $(SDK)/sdkconfig wifitalkie/build_opt.h wifitalkie/*.c wifitalkie/*.cpp wifitalkie/*.h wifitalkie/*.ino
	echo "#define FIRMWARE_BUILT $$(date +%s)LL" > wifitalkie/built.h
	ARDUINO_BUILD_CACHE_PATH=$(CURDIR)/build/cache arduino-cli compile --fqbn $(FQBN) \
		--warnings default --output-dir build --build-path $(CURDIR)/build/sketch \
		--build-property "compiler.sdk.path=$(CURDIR)/$(SDK)" wifitalkie

upload: build/wifitalkie.ino.bin
	arduino-cli upload --fqbn $(FQBN) -p $(PORT) --input-dir build wifitalkie

monitor:
	python3 tools/talkie.py --port $(PORT)

tests/test_units: tests/test_units.c $(PLAIN) wifitalkie/*.h
	$(CC) $(CFLAGS) -o $@ tests/test_units.c $(PLAIN) -lm

tests/voip_tool: tests/voip_tool.c $(PLAIN) wifitalkie/*.h
	$(CC) $(CFLAGS) -o $@ tests/voip_tool.c $(PLAIN) -lm

check: tests/test_units tests/voip_tool
	tests/test_units
	ZBITXRE=$(ZBITXRE) python3 tests/test_service.py

release: build/wifitalkie.ino.bin
	cp build/wifitalkie.ino.bin firmware/wifitalkie.ino.bin
	sed -n 's/^#define FIRMWARE_BUILT \([0-9]*\)LL$$/\1/p' wifitalkie/built.h > firmware/version.txt
	@echo "firmware/ is built $$(cat firmware/version.txt); commit and push it for From github.com"

clean:
	rm -rf build tests/test_units tests/voip_tool

.PHONY: all upload monitor check release clean
