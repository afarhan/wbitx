#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Ashhar Farhan
"""
talkie.py — the laptop's end of the talkie's console, over USB.

    tools/talkie.py                         type verbs; what the talkie says
                                            is printed as it comes
    tools/talkie.py STATUS                  send one verb, print what comes
                                            back for a second, and stop
    tools/talkie.py "FREQ 7074000" STATUS   several, one after another
    tools/talkie.py --port /dev/ttyACM1 --wait 5 STATUS

The ESP32-C3's own USB is a serial port, /dev/ttyACM0 unless another is
named. The port is opened with DTR and RTS left low: on the C3's USB the two
together are how esptool resets the chip, and a console that reset the
talkie at each opening would lose what it was saying. The talkie does not
echo what is typed; the line is typed here, with the shell's editing, and
sent when Enter is pressed.

Needs pyserial.
"""

import argparse
import sys
import threading
import time

import serial

DEFAULT_PORT = "/dev/ttyACM0"
BAUD = 115200
READ_WAIT_S = 0.1


def open_port(name):
    port = serial.Serial()
    port.port = name
    port.baudrate = BAUD
    port.timeout = READ_WAIT_S
    port.dtr = False
    port.rts = False
    port.open()
    return port


def print_what_comes(port, stop):
    while not stop.is_set():
        try:
            data = port.read(256)
        except serial.SerialException as error:
            print("talkie.py: the port went away: %s" % error, file=sys.stderr)
            stop.set()
            return
        if data:
            sys.stdout.write(data.decode(errors="replace").replace("\r\n", "\n"))
            sys.stdout.flush()


def main():
    parser = argparse.ArgumentParser(description="the talkie's console")
    parser.add_argument("--port", default=DEFAULT_PORT)
    parser.add_argument("--wait", type=float, default=1.0,
                        help="seconds to listen after each verb given on the command line")
    parser.add_argument("verbs", nargs="*")
    args = parser.parse_args()
    port = open_port(args.port)
    stop = threading.Event()
    reader = threading.Thread(target=print_what_comes, args=(port, stop), daemon=True)
    reader.start()
    try:
        if args.verbs:
            for verb in args.verbs:
                port.write((verb + "\n").encode())
                time.sleep(args.wait)
            return
        while not stop.is_set():
            line = sys.stdin.readline()
            if not line:
                return
            port.write(line.encode())
    except KeyboardInterrupt:
        pass
    finally:
        stop.set()
        reader.join(1)
        port.close()


if __name__ == "__main__":
    main()
