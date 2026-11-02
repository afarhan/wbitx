# wifitalkie: the ESP32-C3 firmware, built and uploaded with arduino-cli,
# and the tests of its plain C on this computer.
#
#   make                     build the firmware into build/
#   make upload PORT=...     upload it (default /dev/ttyACM0)
#   make monitor             type verbs at the talkie (tools/talkie.py)
#   make check               the host tests

FQBN = esp32:esp32:esp32c3:CDCOnBoot=cdc
PORT ?= /dev/ttyACM0
ZBITXRE ?= /home/farhan/Documents/hfsigs/claude_sbitx
CFLAGS = -O2 -Wall -Wextra -std=c11 -D_DEFAULT_SOURCE

PLAIN = wifitalkie/auth.c wifitalkie/button.c wifitalkie/dial.c wifitalkie/entry.c wifitalkie/known.c wifitalkie/playback.c wifitalkie/sha256.c wifitalkie/speech.c wifitalkie/telephone.c wifitalkie/voip_packet.c

all: build/wifitalkie.ino.bin

build/wifitalkie.ino.bin: wifitalkie/*.c wifitalkie/*.cpp wifitalkie/*.h wifitalkie/*.ino
	arduino-cli compile --fqbn $(FQBN) --warnings default --output-dir build wifitalkie

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

clean:
	rm -rf build tests/test_units tests/voip_tool

.PHONY: all upload monitor check clean
