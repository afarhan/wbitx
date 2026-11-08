#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Ashhar Farhan
"""
case.py — the wifitalkie's case, held upright like a talkie.

The 50 mm speaker sits in the top left corner, the top and left walls its
ring on those two sides. The 18650 lies down the right side from the top
wall, on the back, its left flank tucked under the speaker's sloping frame;
two semicircular cradles on the back hold it and a solid wall closes its
lower end. Over the speaker, a grille of large holes as wide as the case
(the Teenage Engineering grille), down to a line across the front; below
the line, centred on the case's width, the display (landscape, held to the
front) and the four 6x6 keys under it, three in a row and one under the
middle; the 10 mm microphone right of the three, on their line. The keys' 3.5 mm plungers
come through the front, each in a countersink for the finger.

The board runs the whole width under the speaker, with a notch out of its
top right corner where the cell's end comes down: the display, keys and mic
in front; behind its left end the headset jack and under it the SuperMini,
both out of the left side. Two M3
screws, either side of the three keys, go through the board into pedestals
hanging from the front's roof, so a key pressed doesn't bend the board.
The TP4056 lies on the back tray under the speaker's back, its USB-C out of
the top.
On the board's back, a right-angle tactile switch at its right edge is the
PTT, out of the right wall under a printed keytop. There is no power
switch: the talkie sleeps.

Three parts to print in black PLA: the front shell (face down), the back
tray (back down) and the PTT's keytop (exports/keytop.stl, face down). Four M3 screws go in from the back: two by the speaker's
left side, two through the board's bottom corners, clamping it to the
front.

    python case.py front    only exports/front.stl
    python case.py test     only exports/front_test.stl, the board's band

    python case.py          exports/*.glb to look at, *.stl to print
    python case.py check    the overlap check
    make serve              http://localhost:8000/viewer.html

Frame: x to the right, y up, z out of the front; the front face is at z = D,
the back at z = 0. "depth" is millimetres in from the front face. Positions on
the board are x from its left edge, y down from its top edge, from the front.
The board is drawn here first; the PCB follows it.
"""
import math
import json
import os
import sys

from build123d import (Align, Axis, Box, Color, Compound, Cone, Cylinder, Pos, Rot,
                       Text, export_gltf, export_step, export_stl, extrude, fillet)

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "exports")
FONT = "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf"

# ------------------------------------------------------------------ sizes
# the board the case is for: "proto", the hand-wired perf board, or "pcb",
# the wificomms board; make BOARD=pcb
BOARD = os.environ.get("BOARD", "proto")
assert BOARD in ("proto", "pcb"), "BOARD is proto or pcb"

WALL = 1.2          # every wall
CLR = 0.3           # slip between a part and what it sits against
CORNER = 3.2

D = 25.0            # the thickness; the width and the height follow

CELL_D, CELL_L = 18.3, 65.2               # 18650, down the right side
CELL_CLR = 0.2      # round the cell
CRADLE_T = 1.6      # the cradles and the wall at the cell's lower end

# the speaker as measured: 50 across, a 2.6 flat rim, the frame narrowing
# to the 30 mm back unit 7.5 in, 15 deep in all
SPK_D, SPK_DEEP, SPK_MAGNET = 50.0, 15.0, 30.0
SPK_RIM, SPK_CONE = 2.6, 7.5
RING_T, RING_H = 1.2, 3.0                 # ring on the front, glue it in
SINK = 0.6          # the speaker's rim into the left and top walls
GRILLE_HOLE, GRILLE_PITCH = 3.2, 4.4
GRILLE_EDGE = 3.6   # at least this from the edges and the line to a hole
DIMPLE = 1.0        # holes off the cone are blind, this deep
SKIN = 0.4          # at least this much wall under a blind hole

# the board, and its height above and below
BT = 1.6
FRONT_H = 4.7       # front wall to the board's front face: the pedestals' height
BACK_H = 6.0        # the SuperMini, the jack, behind the board
SWITCH_TOP = 7.0    # the plungers' tops above the board (6x6x7 switches)

# the OLED module as measured off the prototype: 24.9 x 27.1, the glass
# 8.8 down from its top edge
OLED_W, OLED_H = 24.9, 27.1
OLED_TOP = 5.0      # the display's glass above the board, measured
# the board is 0.1" perfboard, and everything on it sits on its grid
PITCH = 2.54
KEY_PITCH, KEY_ROWS = 4 * PITCH, 3 * PITCH  # 0.4" apart, 0.3" down
# the switches' legs pulled out to span 4 holes wide and 3 tall: pins 3
# pitches apart across, 2 down, so a switch's middle is half a pitch off
# the grid across, between two holes, and on it down
KEY_PINS_X, KEY_PINS_Y = 3 * PITCH, 2 * PITCH
OLED_PIN_DY = 1.5   # the display's pin row up from its module's bottom edge
PIN_GAP = 1         # holes from the display's pins down to the top switches'
PED_HOLES = 2       # holes from the outer switches' pins out to the screws
EDGE = PITCH / 2    # board past the last row of holes, cut between rows
KEY_HOLE = 4.0      # the 3.5 mm plungers, pressed as they are
# The wificomms PCB: Omron B3FS-1010P keys, SMD, a 3 mm plunger 4.3 mm
# tall, and the display soldered down by wires, not on its header, its
# glass 3.0 mm up. So the front comes 1.9 mm nearer the board, and the
# plungers stand 0.3 mm proud; the glass sinks 0.4 mm into the front wall.
SWITCH_BODY, PLUNGER_D = 3.5, 3.5    # the switch's body, its plunger across
if BOARD == "pcb":
    FRONT_H, SWITCH_TOP, OLED_TOP, KEY_HOLE = 2.8, 4.3, 3.0, 3.5
    SWITCH_BODY, PLUNGER_D = 2.6, 3.0
DISH_D, DISH_DEEP = 6.0, 1.0              # round each key, for the finger
MIC_D, MIC_HOLE, MIC_PROUD = 9.7, 10.0, 1.0
GROOVE_W, GROOVE_D = 0.8, 0.5             # the line under the speaker
JACK_HOLE = 6.5                           # behind the board, out the left
JACK_L = 11.0
JACK_W = 7.5                              # its body across, as fitted
JACK_DEPTH = WALL + FRONT_H + BT + 4.0    # its barrel's centre, 1.5 deeper as fit4 showed
USB_W, USB_H = 9.5, 3.8                   # SuperMini's USB-C, out the right
USB_DEPTH = WALL + FRONT_H + BT + 3.5     # 1.5 deeper than before, as fit4 showed
MINI_W, MINI_L = 18.0, 22.5

CHG_L, CHG_W, CHG_T = 28.0, 17.5, 1.2     # TP4056 USB-C module
CHG_H = CHG_T + 3.2                       # with its USB-C on top
CHG_USB_W, CHG_USB_H = 9.5, 3.8

BOSS_D, PILOT_D, SCREW_D, HEAD_D = 7.0, 2.5, 3.4, 6.2   # M3, cut its own thread
LIP_H, LIP_T, LIP_GAP = 1.5, 0.8, 0.2
HEAD_DEEP = 1.3     # the screw head's recess, past the inside of the back
COLLAR_D = HEAD_D + 2 * 1.2   # three lines round the recess
PED_D = BOSS_D      # M3 x 6 through the board into a pedestal
SCREW_L = 20.0      # every screw from the back: M3 x 20, under the head
PILOT_SHORT = 0.6   # the screw's tip short of the pilot hole's end
# where the board screws' pilots end: 0.6 short of the front wall's inside;
# on the pcb, with the board 1.9 nearer the front, that left 1.6 of thread,
# so there they run on into the wall, PILOT_SKIN under the front face
PILOT_SKIN = 0.8
BOARD_PILOT_END = D - WALL - 0.6 if BOARD != "pcb" else D - PILOT_SKIN
SEAT_WALL = 1.6     # round a screw head sunk deep, for the column's strength
# the back's columns under the board stop short of it: the last print's
# back stood 0.5 proud of the front with them touching it, and fit4's
# pushed the board 1.5 too far forward
BOARD_GAP = 2.0
BOARD_R = 2.5       # the board's corners, sanded round

# two switches added to the prototype's back, placed off its photos (one
# pitch is 2.54); measure and correct these before trusting a print
# the PTT: a right-angle 6x6 tactile switch at the board's right edge just
# under the notch, its face flush with the edge, its plunger out through the
# right wall wearing a printed keytop
PTT_Y = 20.8                              # board y of the plunger's axis: fit5's hole sat 1.1 above it
PTT_UP = 4.6                              # the plunger's axis behind the board, as soldered
PTT_PLUNGER_D, PTT_PLUNGER_L = 3.5, 2.7   # past the switch's face
PTT_BODY_X, PTT_BODY_Y, PTT_BODY_H = 7.3, 7.0, 7.5   # in from the edge, across, tall
KEYTOP_D, PTT_HOLE = 5.0, 5.6             # the keytop and its hole in the wall
KEYTOP_PROUD = 2.0                        # past the right wall's outside
KEYTOP_BACK = 0.5                         # short of the switch's face, for travel
KEYTOP_BORE = 3.5                         # pressed on the plunger

# ---------------------------------------------------------- derived sizes
CELL_R = CELL_D / 2
SPK_R = SPK_D / 2 + CLR
RING_R = SPK_R + RING_T
CELL_Z = WALL + CELL_CLR + CELL_R        # the cell's axis, on the back


def spk_r(depth):
    """The speaker's radius, with its slip, this far in from the inside of
    the front; the frame taken as a straight cone from the rim to the back
    unit."""
    if depth < 0 or depth > SPK_DEEP:
        return 0.0
    if depth <= SPK_RIM:
        return SPK_R
    if depth <= SPK_CONE:
        t = (depth - SPK_RIM) / (SPK_CONE - SPK_RIM)
        return SPK_R - t * (SPK_D - SPK_MAGNET) / 2
    return SPK_MAGNET / 2 + CLR


def cell_gap():
    """The least room between the speaker's axis and the cell's for the
    two to pass: the speaker's frame against the cell's round."""
    r = CELL_R + CELL_CLR
    need, n = 0.0, 4000
    for i in range(n + 1):
        zz = WALL + (D - 2 * WALL) * i / n
        h = r * r - (zz - CELL_Z) ** 2
        if h > 0:
            need = max(need, spk_r(D - WALL - zz) + h ** 0.5)
    return need


# the speaker against the left wall, its rim sunk into it, and the cell
# against the right, as close as the speaker's frame lets it: the width
SPK_X = WALL - SINK + SPK_R               # frame x of the speaker's axis
OW = math.ceil((SPK_X + cell_gap() + CELL_R + CELL_CLR + WALL) * 10) / 10
IW = OW - 2 * WALL
BW = IW - 2 * CLR

CX = OW / 2 - (WALL + CLR)                # the case's middle, board x
OLED_X0 = CX - OLED_W / 2
OLED_Y0 = 0.0                             # the display at the board's top
OLED_PCB = (OLED_X0, OLED_X0 + OLED_W, OLED_Y0, OLED_Y0 + OLED_H)
OLED_GLASS = (OLED_X0 + 0.3, OLED_X0 + 25.0, OLED_Y0 + 8.8, OLED_Y0 + 22.8)
OLED_WIN = (OLED_X0 + 0.8, OLED_X0 + 24.5, OLED_Y0 + 9.1, OLED_Y0 + 22.5)
# the display's four pins centred over the middle switch, PIN_GAP holes
# above its top pins
KEY_Y = OLED_Y0 + OLED_H - OLED_PIN_DY + PIN_GAP * PITCH + KEY_PINS_Y / 2
KEYS = [(CX - KEY_PITCH, KEY_Y), (CX, KEY_Y),
        (CX + KEY_PITCH, KEY_Y), (CX, KEY_Y + KEY_ROWS)]
# the board ends half a pitch past the bottom switch's lower pins
BH = KEY_Y + KEY_ROWS + KEY_PINS_Y / 2 + EDGE
# the M3 screws either side of the three keys, on the grid with the
# switches' pins and so with each other
PED_DX = KEY_PITCH + KEY_PINS_X / 2 + PED_HOLES * PITCH
# the right one stands aside for the mic: three holes up and one out from
# the right switch's upper outer pin
PEDESTALS = [(CX - PED_DX, KEY_Y),
             (CX + KEY_PITCH + KEY_PINS_X / 2 + PITCH, KEY_Y - 3 * PITCH)]
# the case's screws through the board's bottom corners, level with the
# bottom switch: the right one five holes out from the right switch's outer
# pin, its column run into the wall; the left one where the prototype's
# drilled hole came, four and a half out. The prototype's holes wander off
# its grid: the right one a quarter hole lower and nearer the wall. A proper
# board puts both on the grid.
#
# The wificomms PCB, BOARD=pcb, keeps the left one and mirrors it across
# for the right, at the prototype's right one's height, to stay clear of
# the mic: the prototype's right hole is 1.8 mm from the edge, too near
# for a made board. Its holes are H1 and H2.
_bdx = KEY_PITCH + KEY_PINS_X / 2 + 5 * PITCH
_left = (CX - _bdx + PITCH / 2, KEY_Y + KEY_ROWS)
if BOARD == "pcb":
    BOARD_BOSSES = [_left, (BW - _left[0], KEY_Y + KEY_ROWS + PITCH / 4)]
else:
    BOARD_BOSSES = [_left, (CX + _bdx + PITCH / 4, KEY_Y + KEY_ROWS + PITCH / 4)]
# the mic right of the three keys, on their line, centred across between
# the right switch and the case's right edge
_gx = (CX + KEY_PITCH + 3.0 + OW - (WALL + CLR)) / 2   # board x
_gy = KEY_Y
MIC_XY = (_gx, _gy)

# the speaker in the top left corner, the board under it: that sets the
# height
IH = BH + CLR + CLR + 2 * SPK_R - SINK
OH = IH + 2 * WALL
SPK_Y = OH - WALL + SINK - SPK_R          # frame y of the speaker's axis

# the cell on the back against the right wall, hanging from the top wall,
# a wall under its lower end
CELL_X = OW - WALL - CELL_CLR - CELL_R    # frame x, z of the cell's axis
CELL_Y1 = OH - WALL - CELL_CLR
CELL_Y0 = CELL_Y1 - CELL_L
END_Y1 = CELL_Y0 - CELL_CLR               # the end wall, frame y
END_Y0 = END_Y1 - CRADLE_T
BAY_X0 = CELL_X - CELL_R - CELL_CLR       # the cell's leftmost, frame x
CRADLES = [CELL_Y0 + 10.0, CELL_Y1 - 12.0]   # the back's cradles, frame y

_by = lambda y: WALL + CLR + BH - y      # by(), before it is defined
_bx = lambda x: WALL + CLR + x
# the front's own walls by the cell keep clear of the display's module
HOLD_X0 = max(BAY_X0, _bx(OLED_PCB[1]) + CLR)
# two saddles on the cell from the front: one between its end wall and the
# board's top, one at its top end, where the speaker's round has turned
# away from it
def _clear_of_ring(y):
    """The first x right of the speaker's ring, across a saddle at y."""
    dy = max(abs(y - SPK_Y) - CRADLE_T / 2, 0)
    rr = RING_R + CLR
    return SPK_X + (rr * rr - dy * dy) ** 0.5 if dy < rr else 0.0


_ty = CELL_Y1 - 3.0
HOLDS = [(HOLD_X0, (END_Y1 + _by(0)) / 2),
         (max(BAY_X0, _clear_of_ring(_ty)), _ty)]
# the board's notch under the cell's end: board x, y
NOTCH_X = BAY_X0 - CLR - (WALL + CLR)
NOTCH_H = _by(0) - (END_Y0 - CLR)

# behind the board's left end, both out of the left wall: the jack at the
# top, the SuperMini under it, clear of the left pedestal's screw head
HEAD_CLR = 3.0      # an M3 pan head's radius, and some
JACK_Y = 0.5 + 3.0
USB_Y = JACK_Y + 3.0 + 0.3 + MINI_W / 2 + PITCH   # a hole lower, clear of the jack's tabs

# the charger under the speaker's back unit; its USB-C is not used, the
# cell is charged through the SuperMini's
CHG_Y1 = WALL + IH - CLR
CHG_Y0 = CHG_Y1 - CHG_L
CHG_X0 = WALL + CLR + BOSS_D + 1.6
CHG_X1 = CHG_X0 + CHG_W

# the case's screws by the speaker: in the top left corner, and on the left
# wall above the jack, far enough up to clear its body with room to spare;
# there it eats into the speaker's ring, and the front's column is cut back
# where the speaker's rim comes
_ex = WALL + CLR + BOSS_D / 2
BOSS_UP = 2.5
BOSSES = [(_ex, OH - _ex), (_ex, _by(JACK_Y) + JACK_W / 2 + BOSS_UP + BOSS_D / 2)]   # frame x, y

# the line sits over the display's window as far as the keys' last dish
# sits over the bottom edge, so the two are centred between the line and
# the bottom. The grille fills the rest: as many columns as fit the width
# at the pitch, centred, and the margin that leaves kept from the top edge
# and from the line, its rows' pitch stretched a little to make it so.
G_BELOW = _by(KEY_Y + KEY_ROWS) - DISH_D / 2      # bottom edge to the keys
LINE_Y = _by(OLED_WIN[2]) + G_BELOW + GROOVE_W / 2
G_NX = int((OW - 2 * GRILLE_EDGE - GRILLE_HOLE) // GRILLE_PITCH) + 1
G_M = (OW - (G_NX - 1) * GRILLE_PITCH - GRILLE_HOLE) / 2
G_Y0 = LINE_Y + GROOVE_W / 2 + G_M + GRILLE_HOLE / 2   # lowest row's centre
G_Y1 = OH - G_M - GRILLE_HOLE / 2                      # highest
G_NY = int(round((G_Y1 - G_Y0) / GRILLE_PITCH)) + 1
G_PY = (G_Y1 - G_Y0) / (G_NY - 1)

SPLIT = WALL + max(SPK_DEEP, FRONT_H + BT + BACK_H) + 0.5   # the seam
Z_SPLIT = D - SPLIT


PROUD = SWITCH_TOP - FRONT_H - WALL       # plungers past the front face
POCKET = OLED_TOP - FRONT_H + 0.2         # the glass into the front wall
assert PROUD >= -DISH_DEEP, "the plungers don't reach the front; lower FRONT_H"
THREAD = BOARD_PILOT_END - PILOT_SHORT - (D - WALL - FRONT_H)   # into the pedestal
assert THREAD >= 2.5, "a board screw has too little thread in its pedestal"
print(f"board screws: {THREAD:.1f} mm of thread", file=sys.stderr)
for (x, y) in BOARD_BOSSES:
    for (kx, ky) in KEYS:
        assert math.hypot(_bx(x) - _bx(kx), _by(y) - _by(ky)) >= DISH_D / 2 + PILOT_D / 2 + 0.5, \
            "a board screw's pilot runs under a key's dish"
assert SWITCH_BODY <= FRONT_H - 0.2 + 1e-9, "the switches' bodies meet the front wall"
assert CELL_X - SPK_X >= cell_gap(), "the cell meets the speaker's frame"
assert WALL + SPK_DEEP + 0.5 <= D - WALL, "the speaker is too deep"
assert WALL + CHG_H + CLR <= D - WALL - SPK_DEEP, "the charger meets the speaker"
assert NOTCH_H < OLED_Y0 + OLED_H - OLED_PIN_DY - 1.5, "the cell comes down too far"
assert MIC_D / 2 + 0.5 < MIC_XY[0] < BW - MIC_D / 2 - 0.5, "the mic is off the board"
for (x, y) in PEDESTALS + BOARD_BOSSES:
    assert ((MIC_XY[0] - x) ** 2 + (MIC_XY[1] - y) ** 2) ** 0.5 \
        >= PED_D / 2 + MIC_HOLE / 2 + 0.4, "the mic crowds a screw"
assert MIC_XY[0] - MIC_HOLE / 2 >= CX + KEY_PITCH + 3.0 + 0.5, "the mic meets a switch"
for (x, y) in PEDESTALS:
    for (kx, ky) in KEYS:
        for sx in (-1, 1):
            for sy in (-1, 1):
                d = ((x - kx - sx * KEY_PINS_X / 2) ** 2
                     + (y - ky - sy * KEY_PINS_Y / 2) ** 2) ** 0.5
                assert d >= PED_D / 2 + 0.8, "a pedestal crowds a switch's pin"
    assert not (OLED_PCB[0] - PED_D / 2 - CLR < x < OLED_PCB[1] + PED_D / 2 + CLR
                and y < OLED_PCB[3] + PED_D / 2 + CLR), "a pedestal meets the display"
    assert x < NOTCH_X - PED_D / 2 or y >= NOTCH_H + PED_D / 2 + CLR, \
        "a pedestal is off the board, under the cell"
assert PED_DX - KEY_PITCH - 3.0 - PED_D / 2 >= 2.0, "a pedestal crowds a switch"
assert _bx(PEDESTALS[0][0]) - PED_D / 2 >= WALL + 0.5, "a pedestal meets the wall"
assert KEY_Y - 3.0 - (OLED_Y0 + OLED_H) >= 0.5, "the display meets the switches"
assert POCKET <= WALL - 0.6, "the display pocket leaves too thin a wall"
assert USB_Y + MINI_W / 2 <= BH, "the SuperMini is off the board"
assert JACK_Y + JACK_W / 2 + 0.5 <= BH, "the jack is off the board"
assert PTT_Y - PTT_BODY_Y / 2 >= NOTCH_H - 1.5, "the PTT hangs too far into the notch"
assert USB_Y + MINI_W / 2 <= KEY_Y - HEAD_CLR or PEDESTALS[0][0] - HEAD_CLR >= MINI_L, \
    "the SuperMini meets the left pedestal's screw"
for (x, y) in BOARD_BOSSES:
    for (kx, ky) in KEYS:
        for sx in (-1, 1):
            for sy in (-1, 1):
                d = ((x - kx - sx * KEY_PINS_X / 2) ** 2
                     + (y - ky - sy * KEY_PINS_Y / 2) ** 2) ** 0.5
                assert d >= BOSS_D / 2 + 0.8, "a boss crowds a switch's pin"
    assert (x - BOSS_D / 2 - 0.2 >= MINI_L
            or y - BOSS_D / 2 - 0.2 >= USB_Y + MINI_W / 2), \
        "a boss meets the SuperMini"
    assert SCREW_D / 2 <= x <= BW - SCREW_D / 2, \
        "a screw misses the board"
    assert WALL + 0.4 <= _bx(x) - HEAD_D / 2 and _bx(x) + HEAD_D / 2 <= OW - 0.6 + 0.4, \
        "a board screw's head comes too near the side"
    assert abs(x - JACK_L / 2) > JACK_L / 2 + BOSS_D / 2 or abs(y - JACK_Y) > JACK_W / 2 + BOSS_D / 2, \
        "a boss meets the jack"
for (x0, y) in HOLDS:
    assert x0 >= _clear_of_ring(y) and x0 <= CELL_X - 4.0, \
        "a saddle meets the speaker's ring, or misses the cell's crest"
for (x, y) in BOSSES:
    assert ((x - SPK_X) ** 2 + (y - SPK_Y) ** 2) ** 0.5 \
        >= SPK_R + PILOT_D / 2 + 0.8, "a boss's screw meets the speaker"
    assert x + BOSS_D / 2 + 0.3 <= CHG_X0 or y + BOSS_D / 2 < CHG_Y0, \
        "a boss meets the charger"


# --------------------------------------------------------------- helpers
def z(depth):
    return D - depth


def bx(x):            # board x -> frame x
    return WALL + CLR + x


def by(y):            # board y (down from its top) -> frame y
    return WALL + CLR + BH - y


def box(x0, x1, y0, y1, z0, z1):
    return Pos(min(x0, x1), min(y0, y1), min(z0, z1)) * Box(
        abs(x1 - x0), abs(y1 - y0), abs(z1 - z0), align=(Align.MIN,) * 3)


def cyl_z(cx, cy, r, z0, z1):
    return Pos(cx, cy, min(z0, z1)) * Cylinder(
        r, abs(z1 - z0), align=(Align.CENTER, Align.CENTER, Align.MIN))


def cone_z(cx, cy, r0, r1, z0, z1):
    """A cone from radius r0 at z0 to r1 at z1."""
    if z0 > z1:
        z0, z1, r0, r1 = z1, z0, r1, r0
    return Pos(cx, cy, z0) * Cone(r0, r1, z1 - z0,
                                  align=(Align.CENTER, Align.CENTER, Align.MIN))


def cyl_x(x0, x1, cy, cz, r):
    return Pos(min(x0, x1), cy, cz) * Rot(0, 90, 0) * Cylinder(
        r, abs(x1 - x0), align=(Align.CENTER, Align.CENTER, Align.MIN))


def cyl_y(cx, y0, y1, cz, r):
    return Pos(cx, min(y0, y1), cz) * Rot(-90, 0, 0) * Cylinder(
        r, abs(y1 - y0), align=(Align.CENTER, Align.CENTER, Align.MIN))


def rounded(x0, y0, w, h, z0, z1, r):
    return fillet(box(x0, x0 + w, y0, y0 + h, z0, z1).edges().filter_by(Axis.Z), r)


def outer(z0, z1):
    return rounded(0, 0, OW, OH, z0, z1, CORNER)


def inner(z0, z1, grow=0.0):
    return rounded(WALL - grow, WALL - grow, IW + 2 * grow, IH + 2 * grow,
                   z0, z1, max(CORNER - WALL + grow, 0.5))


def mouth_x(x0, x1, cy, cz, w, h):
    """An obround through a side wall: w along y, h along z, round ends."""
    r = h / 2
    return (cyl_x(x0, x1, cy - (w / 2 - r), cz, r)
            + cyl_x(x0, x1, cy + (w / 2 - r), cz, r)
            + box(x0, x1, cy - (w / 2 - r), cy + (w / 2 - r), cz - r, cz + r))


def mouth_y(y0, y1, cx, cz, w, h):
    """An obround through the top or bottom wall: w along x, h along z."""
    r = h / 2
    return (cyl_y(cx - (w / 2 - r), y0, y1, cz, r)
            + cyl_y(cx + (w / 2 - r), y0, y1, cz, r)
            + box(cx - (w / 2 - r), cx + (w / 2 - r), y0, y1, cz - r, cz + r))


def cell_round(extra=0.0):
    return cyl_y(CELL_X, CELL_Y0 - 5, CELL_Y1 + 5, CELL_Z,
                 CELL_R + CELL_CLR + extra)


# the two ports that leave through the walls of the front shell; each is a
# notch open to the seam, so the board drops in from behind, and the tray
# fills the notch back in with a tongue
def jack_hole():
    return cyl_x(-1, WALL + 1, by(JACK_Y), z(JACK_DEPTH), JACK_HOLE / 2)


def jack_notch(extra=0.0):
    c, r = by(JACK_Y), JACK_HOLE / 2 + extra
    return box(-1, WALL + 1, c - r, c + r, z(JACK_DEPTH),
               Z_SPLIT - LIP_H - 1)


def usb_hole():
    return mouth_x(-1, WALL + 1, by(USB_Y), z(USB_DEPTH), USB_W, USB_H)


def usb_notch(extra=0.0):
    c, w = by(USB_Y), USB_W / 2 + extra
    return box(-1, WALL + 1, c - w, c + w, z(USB_DEPTH),
               Z_SPLIT - LIP_H - 1)


# the PTT through the right wall, notched open to the seam like the ports
def ptt_hole(r=PTT_HOLE / 2):
    return cyl_x(OW - WALL - 1, OW + 1, by(PTT_Y), z(WALL + FRONT_H + BT + PTT_UP), r)


def ptt_notch(extra=0.0):
    c, r = by(PTT_Y), PTT_HOLE / 2 + extra
    return box(OW - WALL - 1, OW + 1, c - r, c + r,
               z(WALL + FRONT_H + BT + PTT_UP), Z_SPLIT - LIP_H - 1)


def ptt_body(grow=0.0):
    """The PTT switch's body behind the board, for the walls to clear."""
    bb, c = z(WALL + FRONT_H + BT), by(PTT_Y)
    return box(bx(BW - PTT_BODY_X) - grow, bx(BW) + grow,
               c - PTT_BODY_Y / 2 - grow, c + PTT_BODY_Y / 2 + grow,
               bb - PTT_BODY_H - grow, bb)


def keytop():
    """The PTT's keytop along +x, in place: pressed on the plunger, through
    the right wall's hole, KEYTOP_PROUD past it."""
    c, zc = by(PTT_Y), z(WALL + FRONT_H + BT + PTT_UP)
    x0 = bx(BW) + KEYTOP_BACK
    tip = bx(BW) + PTT_PLUNGER_L
    return (cyl_x(x0, OW + KEYTOP_PROUD, c, zc, KEYTOP_D / 2)
            - cyl_x(x0 - 1, tip, c, zc, KEYTOP_BORE / 2))


def board_seat():
    """How far in from the back a board screw's head sits: so deep that
    SCREW_L reaches PILOT_SHORT short of the end of the pedestal's pilot."""
    return BOARD_PILOT_END - PILOT_SHORT - SCREW_L


def board_boss_xy():
    return [(bx(x), by(y)) for (x, y) in BOARD_BOSSES]


def grille_xy():
    """The grille's holes, frame x, y."""
    x0 = G_M + GRILLE_HOLE / 2
    return [(x0 + i * GRILLE_PITCH, G_Y0 + j * G_PY)
            for i in range(G_NX) for j in range(G_NY)]


# ------------------------------------------------------------ front shell
def front_shell():
    s = outer(Z_SPLIT, D) - inner(Z_SPLIT - 1, D - WALL)
    s += inner(Z_SPLIT - LIP_H, Z_SPLIT, LIP_T) - inner(Z_SPLIT - 2, D, 0)

    # the speaker's ring; on the top and the left the walls are its ring,
    # and it stops short of the display's module under it
    x0, x1, y0, y1 = OLED_PCB
    ring = (cyl_z(SPK_X, SPK_Y, RING_R, D - WALL - RING_H, D - WALL + 0.5)
            - box(bx(x0) - CLR, bx(x1) + CLR, by(y1), by(y0) + CLR,
                  D - WALL - RING_H - 1, D))
    s += ring & inner(0, D, 0.5)
    s -= cyl_z(SPK_X, SPK_Y, SPK_R, D - WALL - RING_H - 1, D - WALL)

    # pedestals from the roof the board is screwed up to, either side of
    # the keys
    zf = z(WALL + FRONT_H)
    for (x, y) in PEDESTALS:
        s += cyl_z(bx(x), by(y), PED_D / 2, zf, D - WALL + 0.5)

    for (x, y) in BOSSES:
        s += cyl_z(x, y, BOSS_D / 2, Z_SPLIT, D - WALL + 0.5)
    ff = D - WALL
    s -= (cyl_z(SPK_X, SPK_Y, SPK_R, ff - SPK_RIM, ff)
          + cone_z(SPK_X, SPK_Y, SPK_R, spk_r(SPK_CONE), ff - SPK_RIM, ff - SPK_CONE))
    for (x, y) in board_boss_xy():
        s += cyl_z(x, y, BOSS_D / 2, zf, D - WALL + 0.5) & inner(0, D, 0.6)

    # over the cell: the wall at its lower end, and two saddles pressing it
    # down, below the speaker and at the cell's top end
    xr = WALL + IW + 0.5
    s += box(HOLD_X0, xr, END_Y0, END_Y1, Z_SPLIT, D - WALL + 0.5)
    for (x0, y) in HOLDS:
        s += (box(x0, xr, y - CRADLE_T / 2, y + CRADLE_T / 2,
                  CELL_Z, D - WALL + 0.5) - cell_round())

    # ---- cuts
    for (x, y) in BOSSES:
        s -= cyl_z(x, y, PILOT_D / 2, Z_SPLIT - 1, D - WALL - 0.6)
    for (x, y) in board_boss_xy():
        s -= cyl_z(x, y, PILOT_D / 2, zf - 1, BOARD_PILOT_END)
    for (x, y) in PEDESTALS:
        s -= cyl_z(bx(x), by(y), PILOT_D / 2, zf - 1, D - WALL - 0.6)

    # the grille, the whole width over the line, the same margin on all
    # four sides (the line is the fourth): through over the cone, blind
    # elsewhere
    open_r = SPK_D / 2 - 3.0
    hr = GRILLE_HOLE / 2
    blind = min(DIMPLE, WALL - SKIN)
    for (x, y) in grille_xy():
        dx, dy = x - SPK_X, y - SPK_Y
        if dx * dx + dy * dy <= open_r ** 2:
            s -= cyl_z(x, y, hr, D - WALL - 1, D + 1)
        else:
            s -= cyl_z(x, y, hr, D - blind, D + 1)

    # the line between the speaker and the rest
    s -= box(-1, OW + 1, LINE_Y - GROOVE_W / 2, LINE_Y + GROOVE_W / 2,
             D - GROOVE_D, D + 1)

    x0, x1, y0, y1 = OLED_WIN
    s -= box(bx(x0), bx(x1), by(y0), by(y1), D - WALL - 1, D + 1)
    # a pocket on the inside for the glass, standing taller than the
    # pedestals
    if POCKET > 0.2:
        x0, x1, y0, y1 = OLED_GLASS
        s -= box(bx(x0) - CLR, bx(x1) + CLR, by(y0) + CLR, by(y1) - CLR,
                 D - WALL - 1, D - WALL + POCKET)
    for (kx, ky) in KEYS:
        s -= cyl_z(bx(kx), by(ky), KEY_HOLE / 2, D - WALL - 1, D + 1)
        # a 45 degree countersink, not a flat-bottomed dish: printed face
        # down, a flat floor would bridge over air and come out stringy
        s -= Pos(bx(kx), by(ky), D - DISH_DEEP) * Cone(
            KEY_HOLE / 2, DISH_D / 2, DISH_DEEP, align=(Align.CENTER, Align.CENTER, Align.MIN))
        s -= cyl_z(bx(kx), by(ky), DISH_D / 2, D, D + 1)
    s -= cyl_z(bx(MIC_XY[0]), by(MIC_XY[1]), MIC_HOLE / 2, D - WALL - 1, D + 1)

    s -= jack_hole() + jack_notch(0.1)
    s -= usb_hole() + usb_notch(0.1)
    s -= ptt_hole() + ptt_notch(0.1) + ptt_body(CLR)
    # the lip inside the right wall stands aside for the cell
    s -= cyl_y(CELL_X, CELL_Y0 - CELL_CLR, CELL_Y1 + CELL_CLR, CELL_Z,
               CELL_R + CELL_CLR)
    return s


# -------------------------------------------------------------- back tray
def back_tray():
    s = outer(0, Z_SPLIT) - inner(WALL, Z_SPLIT + 1)
    s -= inner(Z_SPLIT - LIP_H - LIP_GAP, Z_SPLIT + 1, LIP_T + LIP_GAP)

    # tongues that close the notches over the jack and the USB-C
    # down to the full wall below the lip's cut, or they stand on a 0.2 skin
    zt = Z_SPLIT - LIP_H - LIP_GAP - 0.5
    s += jack_notch(-0.1) & box(0, WALL, -1, OH + 1, zt, D)
    s += usb_notch(-0.1) & box(0, WALL, -1, OH + 1, zt, D)
    s += ptt_notch(-0.1) & box(OW - WALL, OW, -1, OH + 1, zt, D)

    zb = z(WALL + FRONT_H + BT) - BOARD_GAP
    # the columns under the board run up past the seam, into the front: up
    # there they keep off its walls and its lip, which comes down inside
    # the back's walls, or the two halves wedge on them (fit5)
    zl = Z_SPLIT - LIP_H - LIP_GAP - 0.2
    keep = inner(0, zl, 0.6) + inner(zl, D, -(LIP_T + LIP_GAP))
    for (x, y) in BOSSES:
        s += cyl_z(x, y, BOSS_D / 2, WALL - 0.5, Z_SPLIT - LIP_GAP)
    for (x, y) in board_boss_xy():
        s += cyl_z(x, y, BOSS_D / 2, WALL - 0.5, zb) & keep
    # a collar round each column's foot, so that the wall round the screw
    # head's recess is a few lines thick and the column stands on the plate;
    # under the board, where the head is sunk deep, it runs up past the
    # recess's roof, SEAT_WALL round it
    for (x, y) in BOSSES:
        s += cyl_z(x, y, COLLAR_D / 2, WALL - 0.5, WALL + HEAD_DEEP + 0.8) & inner(0, D, 0.6)
    zs = board_seat()
    for (x, y) in board_boss_xy():
        s += cyl_z(x, y, HEAD_D / 2 + SEAT_WALL, WALL - 0.5,
                   zs + (HEAD_D - SCREW_D) / 2 + 1.0) & keep

    # the cell: two semicircular cradles up to its axis and the wall at its
    # lower end, kept off the front's lip inside the walls
    xr = WALL + IW - LIP_T - LIP_GAP
    for y in CRADLES:
        s += (box(BAY_X0 - 1.0, xr, y - CRADLE_T / 2, y + CRADLE_T / 2,
                  WALL - 0.5, CELL_Z) - cell_round())
    s += box(HOLD_X0, xr, END_Y0, END_Y1, WALL - 0.5, Z_SPLIT - LIP_GAP)

    # the charger, port up: a low fence on its left, right and bottom; the
    # top wall does the rest. It sits under the speaker's back unit.
    cx0, cx1, cy0 = CHG_X0 - CLR, CHG_X1 + CLR, CHG_Y0 - CLR
    top = WALL + 2.0
    s += box(cx0 - 1.2, cx0, cy0 - 1.2, WALL + IH + 0.5, WALL - 0.5, top)
    s += box(cx1, cx1 + 1.2, cy0 - 1.2, WALL + IH + 0.5, WALL - 0.5, top)
    s += box(cx0 - 1.2, cx1 + 1.2, cy0 - 1.2, cy0, WALL - 0.5, top)

    # ---- cuts
    for (x, y) in BOSSES + board_boss_xy():
        s -= cyl_z(x, y, SCREW_D / 2, -1, D)
        # the recess's roof is a 45 degree cone, not a flat ring over air,
        # so the column's core prints on a slope and stands on the plate
        zr = board_seat() if (x, y) in board_boss_xy() else WALL + HEAD_DEEP
        s -= (cyl_z(x, y, HEAD_D / 2, -1, zr)
              + cone_z(x, y, HEAD_D / 2, SCREW_D / 2, zr, zr + (HEAD_D - SCREW_D) / 2)
              ) & inner(-2, D, WALL - 0.6)
    s -= jack_hole()
    s -= usb_hole()
    s -= ptt_hole() + ptt_body(CLR)
    # no mouth for the charger's USB-C: the cell is charged through the
    # SuperMini's USB-C
    return s


# ------------------------------------------- the parts, for looking only
def display_text(zf):
    """The home page (screen.c), white on the black glass."""
    gx0, gx1, gy0, gy1 = OLED_GLASS
    left = bx((gx0 + gx1) / 2) - 21.7 / 2       # 128 x 64 over 21.7 x 10.9
    top = by((gy0 + gy1) / 2) + 10.9 / 2
    px = 21.7 / 128

    def text(s, col, row, size=1):
        t = Text(s, font_size=7 * size * px, font_path=FONT,
                 align=(Align.MIN, Align.MAX))
        return Pos(left + col * 6 * px, top - row * 8 * px - 0.5 * px, zf) * \
            extrude(t, amount=0.05)

    parts = [text("VU2ESE", 0, 0), text("RX", 13, 0),
             text("21080", 3.5, 1.6, 3),
             text("2 HERE", 0, 5), text("VOL [#######    ] 50", 0, 6),
             text("HEARD VU3XYZ", 0, 7)]
    # the online dot and four bars
    parts.append(cyl_z(left + 17 * 6 * px, top - 3.5 * px, 2.5 * px, zf, zf + 0.05))
    for n in range(4):
        h = (2 + 2 * n) * px
        x = left + (110 + 4 * n) * px
        parts.append(box(x, x + 2.5 * px, top - 7 * px, top - 7 * px + h,
                         zf, zf + 0.05))
    out = parts[0]
    for p in parts[1:]:
        out += p
    return out


def parts():
    zbf = z(WALL + FRONT_H)                  # board's front face
    zbb = zbf - BT
    # its corners rounded to clear the inside's
    board = (rounded(bx(0), by(BH), BW, BH, zbb, zbf, BOARD_R)
             - box(bx(NOTCH_X), bx(BW + 1), by(-1), by(NOTCH_H), zbb - 1, zbf + 1))
    x0, x1, y0, y1 = OLED_PCB
    glass_z = z(WALL + FRONT_H - OLED_TOP)
    oled = box(bx(x0), bx(x1), by(y0), by(y1), glass_z - 2.3, glass_z - 1.3)
    x0, x1, y0, y1 = OLED_GLASS
    glass = box(bx(x0), bx(x1), by(y0), by(y1), glass_z - 1.3, glass_z)
    switches = None
    for (kx, ky) in KEYS:
        x, y = bx(kx), by(ky)
        sw = (box(x - 3, x + 3, y - 3, y + 3, zbf, zbf + SWITCH_BODY)
              + cyl_z(x, y, PLUNGER_D / 2, zbf + SWITCH_BODY - 0.1, zbf + SWITCH_TOP))
        switches = sw if switches is None else switches + sw
    mic = cyl_z(bx(MIC_XY[0]), by(MIC_XY[1]), MIC_D / 2, zbf, D + MIC_PROUD)
    jc = by(JACK_Y)
    jack = (box(bx(0), bx(JACK_L), jc - JACK_W / 2, jc + JACK_W / 2, zbb - 5, zbb)
            + cyl_x(-0.5, bx(0), jc, z(JACK_DEPTH), 2.5))
    mini = (box(bx(0), bx(MINI_L), by(USB_Y - MINI_W / 2),
                by(USB_Y + MINI_W / 2), zbb - 1.0, zbb)
            + box(WALL, bx(0) + 7, by(USB_Y) - 4.45, by(USB_Y) + 4.45,
                  z(USB_DEPTH) - 1.6, z(USB_DEPTH) + 1.6))
    ff = D - WALL                            # the inside of the front
    speaker = (cyl_z(SPK_X, SPK_Y, SPK_D / 2, ff - SPK_RIM, ff)
               + cone_z(SPK_X, SPK_Y, SPK_D / 2, SPK_MAGNET / 2,
                        ff - SPK_RIM, ff - SPK_CONE)
               + cyl_z(SPK_X, SPK_Y, SPK_MAGNET / 2, ff - SPK_DEEP, ff - SPK_CONE))
    cell = cyl_y(CELL_X, CELL_Y0, CELL_Y1, CELL_Z, CELL_R)
    ptt = ptt_body() + cyl_x(bx(BW), bx(BW) + PTT_PLUNGER_L, by(PTT_Y),
                             z(WALL + FRONT_H + BT + PTT_UP), PTT_PLUNGER_D / 2 - 0.1)
    cm = (CHG_X0 + CHG_X1) / 2
    charger = (box(CHG_X0, CHG_X1, CHG_Y0, CHG_Y1, WALL, WALL + CHG_T)
               + box(cm - 4.45, cm + 4.45, CHG_Y1 - 7.3, CHG_Y1 + 0.2,
                     WALL + CHG_T, WALL + CHG_H))
    return [(board, "perfboard", (0.10, 0.40, 0.25)),
            (oled, "oled", (0.10, 0.25, 0.65)),
            (glass, "glass", (0.01, 0.01, 0.01)),
            (display_text(glass_z), "text", (1.0, 1.0, 1.0)),
            (switches, "switches", (0.75, 0.75, 0.75)),
            (mic, "mic", (0.15, 0.15, 0.15)),
            (jack, "jack", (0.25, 0.25, 0.25)),
            (mini, "supermini", (0.20, 0.20, 0.55)),
            (speaker, "speaker", (0.55, 0.55, 0.55)),
            (cell, "cell", (0.70, 0.72, 0.75)),
            (charger, "charger", (0.10, 0.30, 0.70)),
            (ptt, "ptt", (0.75, 0.75, 0.75)),
            (keytop(), "keytop", (0.80, 0.15, 0.10))]


# ---------------------------------------------------------------- output
PLA = (0.07, 0.07, 0.08)


def glb(items, name):
    shapes = []
    for shape, label, rgb in items:
        loose = shape.solids() if len(shape.solids()) > 1 else [shape]
        for n, piece in enumerate(loose):
            piece.color = Color(*rgb)
            piece.label = label if len(loose) == 1 else f"{label}{n}"
            shapes.append(piece)
    path = os.path.join(OUT, name)
    export_gltf(Compound(children=shapes), path, binary=True,
                linear_deflection=0.03, angular_deflection=0.2)
    print(f"wrote {path}")


def to_origin(shape):
    """On the bed: its lowest corner at the origin, for the slicer."""
    lo = shape.bounding_box().min
    return Pos(-lo.X, -lo.Y, -lo.Z) * shape


def whole(shape):
    return shape if hasattr(shape, "wrapped") else Compound(list(shape))


def check(front, back):
    """Every printed part one solid; nothing in another's way."""
    for name, p in (("front", front), ("back", back)):
        print(f"{name}: {len(p.solids())} solid(s)")
    shells = [(front, "front"), (back, "back")]
    things = [(p, n) for p, n, _ in parts() if n != "text"]
    bad = 0
    for a, an in shells + things:
        for b, bn in things:
            if an == bn:
                continue
            v = sum(s.volume for s in whole(a).intersect(whole(b)).solids()) \
                if whole(a).intersect(whole(b)) else 0
            if v > 0.05:
                print(f"  {an} meets {bn}: {v:.2f} mm3")
                bad += 1
    print("clear" if not bad else f"{bad} overlaps")


def main():
    os.makedirs(OUT, exist_ok=True)
    print(f"box {OW:.1f} wide, {OH:.1f} tall, {D:.1f} thick "
          f"(seam {SPLIT:.1f} from the front); plungers {PROUD:+.1f} mm "
          f"past the front")
    front = front_shell()
    # to print, the front shell lies on its face: turned over, face at z = 0
    front_print = to_origin(Rot(180, 0, 0) * front)
    # a fit test: only the board's band of the front, past the ports
    # deep enough to hold the USB-C and the jack in their cut-outs
    test_deep = max(USB_DEPTH + USB_H / 2, JACK_DEPTH + JACK_HOLE / 2) + 1.0
    test = front & box(-1, OW + 1, -1, by(0) + CLR + 1.0, z(test_deep), D + 1)
    test_print = to_origin(Rot(180, 0, 0) * test)
    if "test" in sys.argv:
        export_stl(test_print, os.path.join(OUT, "front_test.stl"))
        print("wrote exports/front_test.stl")
        return
    if "front" in sys.argv:
        export_stl(front_print, os.path.join(OUT, "front.stl"))
        print("wrote exports/front.stl")
        return
    back = back_tray()
    if "check" in sys.argv:
        check(front, back)
        return
    export_stl(front_print, os.path.join(OUT, "front.stl"))
    export_stl(to_origin(back), os.path.join(OUT, "back.stl"))
    export_stl(test_print, os.path.join(OUT, "front_test.stl"))
    export_stl(to_origin(Rot(0, 90, 0) * keytop()), os.path.join(OUT, "keytop.stl"))
    export_step(front, os.path.join(OUT, "front.step"))
    export_step(back, os.path.join(OUT, "back.step"))

    # what shows through the front: the display, the plungers, the mic
    seen = [p for p in parts() if p[1] in ("glass", "text", "oled",
                                           "switches", "mic", "keytop")]
    glb([(front, "front", PLA)] + seen, "front.glb")
    glb([(back, "back", PLA)], "back.glb")
    board = [p for p in parts() if p[1] == "perfboard"]
    glb([(test, "test", PLA)] + seen + board, "test.glb")
    glb([(back, "back", PLA), (front, "front", PLA)] + seen, "closed.glb")
    gap = 35
    glb([(Pos(0, 0, -gap) * back, "back", PLA)] + parts()
        + [(Pos(0, 0, gap) * front, "front", PLA)], "exploded.glb")

    # the exploded view again, a file a piece, so the viewer can hide any
    groups = {"front": [(Pos(0, 0, gap) * front, "front", PLA)],
              "back": [(Pos(0, 0, -gap) * back, "back", PLA)]}
    for p in parts():
        key = "display" if p[1] in ("oled", "glass", "text") else p[1]
        groups.setdefault(key, []).append(p)
    os.makedirs(os.path.join(OUT, "pieces"), exist_ok=True)
    for key, items in groups.items():
        glb(items, os.path.join("pieces", key + ".glb"))
    with open(os.path.join(OUT, "pieces", "list.json"), "w") as fh:
        json.dump(list(groups), fh)


if __name__ == "__main__":
    main()
