#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Ashhar Farhan
"""
printed.py — a 3D-printed case for the production wificomms board
(pcb/wificomms, the board sent for production), not the moulded case of
mold/mold.py.

Seen from the front (display and keys), the board sits at the bottom
against the right wall; the cell, in a 1 x 18650 holder (76 x 21, 20.35
high with the cell in it), runs up the left side from the board's notch to
the top wall; the 50 mm speaker sits top right beside it.

The holder: a pin on the back's floor goes through the hole in the middle
of its base (a drop of superglue holds it); an outline ridge round its foot;
a stop wall at its lower end so it can't slide down the case; on the front,
two ribs with concave cuts that sit on the cell.

The PTT (Alps SKRTLAE010, at the board's right edge) is pressed by a lever
outside the right wall, as on the microphone case: it turns on an M3 screw
outside the top-right corner. A flat bar, on edge from the back face up to
under the board, runs in a fitted slot cut up the back's wall from its back
face and presses the plunger; being tall it can't rock. A claw at its foot,
inside the case behind the wall (as on the microphone's lever), stops it
swinging out; bar and claw run down to the lever's back face, so it prints
on its back with both on the bed, and the back half is cut through its
floor for them. The pivot
is split at half the depth: the lever's hinge runs from its back face up to
there, so it prints on the bed, and a tube stands from it; above it, an
anchor on the front half, the hinge's diameter, the tube turning in its bore.
An M3 Torx countersunk screw from the front, its head on show flush in a
cone in the face, threads into the tube; the cone tapers to the screw's
clearance just over the tube's end, so the head holds the lever up and
lever and screw turn together in the anchor.

The halves close on four M3 self-tapping screws from the back: two through
the board's holes H1 and H2 (the board is clamped between the back's
standoffs and the front's posts), one in the top-right corner, and one right
of the cell's holder between the board and the speaker. The seam is the board's back face.

    make printed            printed/*.stl and pieces/*.glb; open printed/viewer.html
    python printed.py check the overlap check
"""
import math
import os
import re
import sys

from build123d import (Align, Axis, Box, Color, Compound, Cone, Cylinder, Plane, Location,
                       Polygon, Pos, RectangleRounded, Rot, export_gltf, export_stl,
                       extrude, fillet)

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = HERE
PCB = os.path.join(HERE, "..", "..", "pcb", "wificomms", "wificomms.kicad_pcb")

# ---------------------------------------------------------------- printing
WALL = 1.6          # side walls: four 0.4 mm perimeters
FACE = 1.6          # the front face
BACK = 1.4          # the back: face and back 3 mm together
PORT_WALL, PORT_RECESS = 1.0, 0.6   # the side wall at the jack and USB-C, let in from outside
JACK_RECESS_D = 9.0
USB_RECESS = (13.0, 7.5)            # round a USB-C plug's moulded body
FACE_R = 1.0        # the front face's and the back's outer edges
CORNER = 3.0        # the outside's corners seen from the front
IN_CORNER = 1.0
CLR = 0.4           # board to the walls
INNER_H = 20.7      # floor to the face's inside (the user's, 2026-11-24: thinned, the speaker beside the cell): the cell's top 20.35, 0.35 under the face
assert INNER_H >= 20.35 + 0.3, "the cell reaches the face"
D = BACK + INNER_H + FACE   # the thickness

# ---------------------------------------------------------------- board
# KiCad's view is the board's F side, the talkie's back; from the front
# x runs the other way: u = KX1 - x, v = y - KY0
KX1, KY0 = 168.3, 60.0
BW, BH = 58.3, 42.11
NOTCH_U, NOTCH_V = KX1 - 149.6, 77.2 - KY0      # top-left, for the cell
BOARD_R = 2.5
BT = 1.6
FRONT_H = 5.5       # the face's inside to the board's front: room for the keytops (as wearable2, the user's 5.5)


def footprints():
    t = open(PCB).read()
    out = {}
    for m in re.finditer(r'\(footprint "[^"]+"(.*?)\n\t\)', t, re.S):
        body = m.group(1)
        at = re.search(r'\n\t\t\(at ([-\d.]+) ([-\d.]+)(?: ([-\d.]+))?\)', body)
        ref = re.search(r'\(property "Reference" "([^"]+)"', body)
        if at and ref:
            out[ref.group(1)] = (float(at.group(1)), float(at.group(2)),
                                 float(at.group(3) or 0))
    return out


FP = footprints()


def uv(ref, dx=0.0, dy=0.0):
    x, y, r = FP[ref]
    a = math.radians(r)
    ox = dx * math.cos(a) + dy * math.sin(a)
    oy = -dx * math.sin(a) + dy * math.cos(a)
    return KX1 - (x + ox), (y + oy) - KY0


KEYS = [uv(r) for r in ("SW1", "SW2", "SW3", "SW4")]
KEY_TOP, KEY_BODY = 4.8, 3.0          # B3FS-1010P, the user's measure (2026-11-23): body 3.0, plunger top 4.8
KEY_HOLE, DISH_D, DISH_DEEP = 3.5, 6.0, 1.0
MIC_UV = uv("MK2", 0.0, 0.5)
MIC_HOLE, TUBE_OD, TUBE_ID, GASKET = 1.0, 3.0, 1.2, 0.5
HOLES = [uv("H1"), uv("H2")]

OLED_TOP = 3.0
_du, _dv = uv("DS1")
OLED_BOTTOM = _dv + 12.3 + 1.5
OLED_W, OLED_H = 24.9, 27.1
OLED_U0 = _du - OLED_W / 2
OLED_GLASS = (OLED_U0 + 0.3, OLED_U0 + 25.0, OLED_BOTTOM - 18.3, OLED_BOTTOM - 4.3)
WIN_UP = 1.0        # the display's cutout up (the user's, 2026-11-23: the solder showed)
OLED_WIN = (OLED_U0 + 0.8, OLED_U0 + 24.5, OLED_BOTTOM - 18.0, OLED_BOTTOM - 4.6)
OLED_PCB = (OLED_U0, OLED_U0 + OLED_W, OLED_BOTTOM - OLED_H, OLED_BOTTOM)

# on the back: heights of their centres above the board's back face
PTT_V = uv("PTT1")[1]
PTT_UP = 1.7        # SKRTLAE010: plunger 1.9 x 1.9, its tip at the board edge
PTT_PLUNGER = 1.9
JACK_V = uv("J4")[1]
JACK_UP, JACK_HOLE = 2.5, 6.5
USB_U = uv("U1")[0]
USB_UP, USB_W, USB_H = 2.6, 9.5, 4.2
USB_FIX = 0.5       # the hole and its recess toward the PTT's side (the user's print, 2026-11-25: the socket sat off centre)
KNOB_U = uv("SW_POWER1")[0]
KNOB_UP, KNOB_W, KNOB_H = 0.75, 4.6, 2.2

# ---------------------------------------------------------- speaker, cell
SPK_D, SPK_DEEP, SPK_MAGNET = 50.0, 16.0, 32.0   # the largest magnet in the group build (the user's)
SPK_RIM, SPK_CONE = 2.5, 7.5
SPK_CLR = 0.3
RIB = 1.2
RING_H = 3.0
FOAM = 0.5

# the 1 x 18650 holder, measured with calipers: 76 x 21, 20.35 high with
# the cell in it; an M3 hole in the middle of its base
HOLDER_L, HOLDER_W, HOLDER_H = 76.0, 21.0, 20.35
HOLDER_CLR = 0.3
CELL_D = 18.3
CELL_CLR = 0.3                   # the speaker over the cell
PIN_D, PIN_H = 2.8, 1.0          # through the base's hole; ASSUMED base >= 1 mm
RIDGE_T, RIDGE_H = 1.2, 2.0      # the outline round its foot
STOP_H = 10.0                    # the stop wall at its lower end, wires pass above
CRADLE = True                    # the ridge and stop wall on the back (the user's, 2026-11-24: take the cradle off the top, not the bottom)
SADDLE = False                   # the front's two ribs on the cell left out: they kept the box from closing
SADDLE_DEEP = 3.0                # the front's ribs reach this far down the cell's top

GRILLE_HOLE, GRILLE_PITCH, GRILLE_EDGE = 3.2, 4.4, 3.6
DIMPLE = 1.0
GROOVE_W, GROOVE_D = 0.8, 0.5

# screws: M3 self-tapping into plastic
M3_CLEAR, M3_PILOT, M3_HEAD_D, M3_HEAD_H = 3.4, 2.6, 6.2, 3.0
M3_TAP = 2.9        # this case's pilots (the user's, 2026-11-25: 2.6 printed too small, the M3 wouldn't thread); wearable2 keeps M3_PILOT
POST_D, STANDOFF_D, PILOT_SKIN = 6.0, 6.5, 1.0
BOSS_D, BOSS_SKIN = 10.5, 0.8    # the standoff's foot round the head's pocket

# the lever
LEV_GAP = 2.0       # right wall's outside to the lever's inner face (the user's: 1.0 left it no room)
LEV_T = 4.5         # thick at the pivot
LEV_FLARE = 4.0     # more at the paddle
PIV_R = COL_R = 5.0           # the hinge and the anchor, the same diameter
TUBE_D, LEV_HOLE = 6.0, 6.4    # the pivot tube, the lever's bore on it
LEV_D = 25.0                    # the lever as printed for the 25 mm case: it is reused
LEV_Z = D - LEV_D               # so it sits this far up off the back face
ANCHOR_Z0 = LEV_Z + LEV_D / 2   # the hinge below (lever), the anchor above (front)
PAN_D, PAN_H = 5.5, 2.4         # M3 Torx pan head (ISO 14583), flush in the face
TUBE_TOP = D - PAN_H            # the lever's tube ends under the head
CSK_D = 6.3                     # M3 Torx countersunk (ISO 14581): the face's 90 deg cone
CSK_Z = D - (CSK_D - M3_CLEAR) / 2   # where it has tapered to the screw's clearance
BAR_W = 3.0         # the bar onto the plunger, across
BAR_BOT = 0.0       # its foot: flush with the lever's back face, on the bed
CLAW_Y, CLAW_X = 4.0, 1.6        # the claw: each way along the wall, in from the wall
CLAW_Z0, CLAW_Z1 = 0.0, 8.0
CLAW_CLR, CLAW_IN = 0.3, 1.5       # round it in the floor's cut; inward travel more (was 0.3: no room)
BAR_TOP = 0.25      # its top this far under the board's back face
BAR_CLR = 0.3       # round it in its slot: a slide's 0.3 (0.15 was too tight)
AIR_GAP = 0.4       # the bar off the plunger at rest (the user's: at 0.1 preload it held the PTT on)
PRELOAD = -AIR_GAP
LEV_CLR = 0.4

# ------------------------------------------------------------ derived
SPK_R = SPK_D / 2 + SPK_CLR
Z_FACE_IN = D - FACE
Z_BF = Z_FACE_IN - FRONT_H
Z_BB = Z_BF - BT
Z_SPLIT = Z_BB


# the keytops, as on wearable2 (the user's): 5 x 4, 0.7 proud, 0.3 a side in
# their holes, a flange under the face holding the plunger's socket, a collar
# round each hole off the face's inside, all joined by a frame
CAP_PROUD, CAP_FLANGE_T, CAP_CLR = 0.7, 1.0, 0.3
CAP_W, CAP_H, CAP_R = 5.0, 4.0, 0.6
HOLE_W, HOLE_H = CAP_W + 2 * CAP_CLR, CAP_H + 2 * CAP_CLR
FLANGE_W, FLANGE_H = HOLE_W + 0.8, HOLE_H + 0.8
CAP_ROUND = 0.4
CAP_SOCKET_D, CAP_SOCKET = 3.0 + 0.4, 1.0
CAP_Z0 = Z_BF + KEY_TOP - CAP_SOCKET
CAP_HEIGHT = 5.25            # the keytop, flange to top (the user's, 2026-11-23: 4.0 was too low)
CAP_Z1 = CAP_Z0 + CAP_HEIGHT
COLLAR_T = 0.8
COLLAR_H = Z_FACE_IN - (CAP_Z0 + CAP_FLANGE_T) - 0.4
FRAME_W, FRAME_T, FRAME_GAP = 0.8, 0.6, 0.45


def z_back(up):
    return Z_BB - up


HX0 = WALL + HOLDER_CLR
HX1 = HX0 + HOLDER_W
HCX = (HX0 + HX1) / 2
CELL_R = CELL_D / 2
# the holder on the front, upside down (the user's, 2026-11-24): its base on
# the face's inside, the cell toward the back; everything goes into the
# front, then the back drops over it
HOLDER_ON_FRONT = True
H_LO, H_HI = ((Z_FACE_IN - HOLDER_H, Z_FACE_IN) if HOLDER_ON_FRONT else (BACK, BACK + HOLDER_H))
CELL_Z = H_LO + CELL_R if HOLDER_ON_FRONT else H_HI - CELL_R


def spk_r(depth):
    """The speaker's radius, its clearance in, this far under the face's
    inside: the rim, the cone narrowing to the magnet, the magnet."""
    if depth < 0 or depth > SPK_DEEP:
        return 0.0
    if depth <= SPK_RIM:
        return SPK_R
    if depth <= SPK_CONE:
        t = (depth - SPK_RIM) / (SPK_CONE - SPK_RIM)
        return SPK_R - t * (SPK_D - SPK_MAGNET) / 2
    return SPK_MAGNET / 2 + SPK_CLR


def cell_gap():
    """The least distance between the speaker's axis and the cell's: the
    speaker's rim passes over the cell's round top, its cone and magnet
    beside the cell; the holder's walls are low."""
    r = CELL_R + CELL_CLR
    need, n = 0.0, 4000
    for i in range(n + 1):
        zz = BACK + (Z_FACE_IN - BACK) * i / n
        h = r * r - (zz - CELL_Z) ** 2
        if h > 0:
            need = max(need, spk_r(Z_FACE_IN - zz) + h ** 0.5)
    return need


# the board's width, and WIDEN more (the user's, 2026-11-23: the speaker's rim
# overlaps the holder); the board, the speaker and the PTT keep to the right
# wall, so the jack and the PTT are where they were; the holder to the left
WIDEN = 2.0
# the speaker beside the cell, not over it (the user's, 2026-11-24: the group
# build's speakers have 29 to 32 mm magnets, and not all fitted the overlap)
SIDE_BY_SIDE = True
OW = (math.ceil((WALL + HOLDER_CLR + HOLDER_W + 0.4 + 2 * SPK_R + WALL) * 10) / 10 if SIDE_BY_SIDE
      else math.ceil(BW + CLR + 2 * WALL) + WIDEN)
IW = OW - 2 * WALL


# the board left (the user's, 2026-11-24, asked 2 mm: wearable2's lever, its T
# head between the plunger and the wall, needs the room); 1.5 is as far as
# its notch's edge stays clear of the cell's round
BOARD_SHIFT = -1.5


def bx(u):
    return OW - WALL - CLR - BW + BOARD_SHIFT + u


def by(v):
    return WALL + CLR + BH - v


HY0 = by(NOTCH_V) + 0.5
HY1 = HY0 + HOLDER_L
OH = math.ceil((HY1 + HOLDER_CLR + WALL) * 10) / 10
IH = OH - 2 * WALL
H_TOP = BACK + HOLDER_H
HCY = (HY0 + HY1) / 2

SPK_X = OW - WALL - SPK_R
SPK_Y = OH - WALL - SPK_R
# beside the holder on the front: the speaker's ring and the holder's ridge
# both stop square across this band either side of the speaker's axis, where
# they would meet; past it the ring's outside is 0.3 clear of the ridge
SPK_BAND = math.sqrt((SPK_R + RIB) ** 2 - (SPK_X - (HX1 + HOLDER_CLR + RIDGE_T + 0.3)) ** 2)

PTT_X, PTT_Y, PTT_Z = bx(BW), by(PTT_V), z_back(PTT_UP)

# the screws: (x, y, through the board)
# grille: the line sits over the window as far below as the bottom key's
# dish sits over the bottom edge
G_BELOW = by(max(v for _, v in KEYS)) - DISH_D / 2
LINE_Y = by(OLED_WIN[2]) + G_BELOW + GROOVE_W / 2
G_NX = int((OW - 2 * GRILLE_EDGE - GRILLE_HOLE) // GRILLE_PITCH) + 1
G_M = (OW - (G_NX - 1) * GRILLE_PITCH - GRILLE_HOLE) / 2
G_Y0 = LINE_Y + GROOVE_W / 2 + G_M + GRILLE_HOLE / 2
G_Y1 = OH - G_M - GRILLE_HOLE / 2
G_NY = int(round((G_Y1 - G_Y0) / GRILLE_PITCH)) + 1
G_PY = (G_Y1 - G_Y0) / (G_NY - 1)


def between_holes(x, y, ok):
    """The centre of the grille cell (midway between four holes) nearest
    x, y for which ok(x, y) holds: a post there keeps the dimples off its
    pilot hole and leaves the face its full strength round it."""
    gx0 = G_M + GRILLE_HOLE / 2
    # the lattice runs one half-cell past the outer holes, toward the walls
    cells = [(gx0 + (i + 0.5) * GRILLE_PITCH, G_Y0 + (j + 0.5) * G_PY)
             for i in range(-1, G_NX) for j in range(-1, G_NY)]
    cells = [c for c in cells if ok(*c)]
    return min(cells, key=lambda c: math.hypot(c[0] - x, c[1] - y))


def _clear_of_speaker(x, y):
    return math.hypot(x - SPK_X, y - SPK_Y) >= SPK_R + RIB + POST_D / 2


def _in_cavity(x, y):
    """The post may run into a wall; its screw and head must stay inside."""
    r = M3_HEAD_D / 2 + 0.5
    return WALL + r <= x <= OW - WALL - r + 2 and y <= OH - WALL - 2


_TOP_RIGHT = between_holes(OW - WALL - 3.2, OH - WALL - 3.2,
                           lambda x, y: _in_cavity(x, y) and _clear_of_speaker(x, y))
# right of the holder, in the corner between the board and the speaker's
# ring: too tight to snap to the grille's lattice
_BY_CELL = (HX1 + HOLDER_CLR + 0.2 + STANDOFF_D / 2, by(0) + CLR + STANDOFF_D / 2 + 0.2)
assert _clear_of_speaker(*_BY_CELL), "the screw by the cell meets the speaker's ring"

# the screws: (x, y, through the board)
MID_SCREW = False   # the one by the cell out (the user's, 2026-11-25): the holder on the front, the cell no longer pushes the halves apart
SCREWS = [(bx(u), by(v), True) for (u, v) in HOLES] + [
    _TOP_RIGHT + (False,)] + ([
    # right of the holder, under the speaker: the cell pushes the halves apart
    _BY_CELL + (False,)] if MID_SCREW else [])

LEV_IN = OW + LEV_GAP

# the board's bottom supports (x0, x1, y0, y1), from the footprints' courtyards:
# left of the display, nothing on the back from x 4 to 18.5, y 8 to 26.5;
# right of it, between J6, PTT1 and the board's top edge, and 0.4 off the
# claw's cut in the floor
# the bar's slot and the claw's cut stop SLOT_SKIN off the back face, 0.2
# under the lever's back face: the lever rests on them and can't slide off
# the PTT toward the back (the user's)
OLD_LEVER = False
SLOT_SKIN = LEV_Z - 0.2
assert not OLD_LEVER or SLOT_SKIN >= 0.6, "no skin under the lever"
# the back's: the left one only; wearable2 has none by the PTT (the user's, 2026-11-24)
BACK_SUPPORTS = None
SUPPORTS = [(bx(0) + 6.3, bx(0) + 10.3, by(BH) + 19.0, by(BH) + 23.0),
            (bx(BW) - 6.4, bx(BW) - 3.4, by(0) - 3.2, by(0) - 0.8)]
BACK_SUPPORTS = SUPPORTS[:1]
assert SUPPORTS[1][1] <= OW - WALL - CLAW_X - CLAW_CLR - CLAW_IN - 0.4, "the post meets the claw's cut"
PIV = (OW + 5.5, OH - 5.0)
LEV_END = PTT_Y - 12.0

# ------------------------------------------------------------ checks
assert bx(0) >= WALL + 0.1 - 1e-9
_dx = bx(NOTCH_U) - CLR - HCX
assert HOLDER_ON_FRONT or _dx >= CELL_R or CELL_Z + math.sqrt(CELL_R ** 2 - _dx ** 2) <= Z_BB - 0.3, \
    "the cell meets the board's edge"
assert HY0 >= by(NOTCH_V) + 0.3, "the holder meets the board's notch"
assert H_HI <= Z_FACE_IN - 0.3 or HOLDER_ON_FRONT, "the holder reaches the face"
assert H_LO >= BACK + 0.3, "the cell reaches the floor"
assert SPK_Y - SPK_R >= by(0) + 1, "the speaker meets the board"
assert CAP_Z0 + CAP_FLANGE_T <= Z_FACE_IN - 0.05, "the keytop's flange reaches the face"
assert COLLAR_H >= 0.25, "no room for the collars"
for _i, (_u, _v) in enumerate(KEYS):
    for (_u2, _v2) in KEYS[_i + 1:]:
        assert abs(_u - _u2) >= FLANGE_W + 0.5 or abs(_v - _v2) >= FLANGE_H + 0.5, "two keytops' flanges meet"
assert abs(WALL - PORT_RECESS - PORT_WALL) < 1e-9
assert PTT_Z + PTT_PLUNGER / 2 <= Z_BB - BAR_TOP, "the bar misses the plunger's top"
assert CSK_Z >= TUBE_TOP + 0.6, "the head's cone reaches the tube"
assert LEV_END > by(JACK_V) + JACK_HOLE / 2 + 5, "the lever covers the jack"
assert math.hypot(SCREWS[2][0] - SPK_X, SCREWS[2][1] - SPK_Y) >= SPK_R + RIB + POST_D / 2
assert not MID_SCREW or math.hypot(SCREWS[3][0] - SPK_X, SCREWS[3][1] - SPK_Y) >= SPK_R + RIB + POST_D / 2, \
    "the top screw meets the speaker's ring"


# ------------------------------------------------------------ helpers
def box(x0, x1, y0, y1, z0, z1):
    return Pos(min(x0, x1), min(y0, y1), min(z0, z1)) * Box(
        abs(x1 - x0), abs(y1 - y0), abs(z1 - z0), align=(Align.MIN,) * 3)


def cyl_z(cx, cy, r, z0, z1):
    return Pos(cx, cy, min(z0, z1)) * Cylinder(
        r, abs(z1 - z0), align=(Align.CENTER, Align.CENTER, Align.MIN))


def cone_z(cx, cy, r0, r1, z0, z1):
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


def rrect(x0, y0, w, h, r, z0, z1):
    pl = Plane(origin=(x0 + w / 2, y0 + h / 2, min(z0, z1)))
    return extrude(pl * RectangleRounded(w, h, r), amount=abs(z1 - z0))


def outside(z0, z1):
    return rrect(0, 0, OW, OH, CORNER, z0, z1)


def inside(z0, z1, grow=0.0):
    return rrect(WALL - grow, WALL - grow, IW + 2 * grow, IH + 2 * grow,
                 max(IN_CORNER + grow, 0.2), z0, z1)


def prism_xy(pts, z0, z1):
    """A polygon in x, y run from z0 to z1."""
    pl = Plane(origin=(0, 0, z0))
    return extrude(pl * Polygon(*pts, align=None), amount=z1 - z0, dir=(0, 0, 1))


def head_pocket(x, y):
    """The screw head's counterbore from the back, closed with a 45 degree
    cone so it prints without a bridge."""
    h = (M3_HEAD_D - M3_CLEAR + 0.2) / 2
    return (cyl_z(x, y, M3_HEAD_D / 2, -1, M3_HEAD_H)
            + cone_z(x, y, M3_HEAD_D / 2, (M3_CLEAR - 0.2) / 2, M3_HEAD_H - 0.01, M3_HEAD_H + h))


def hole_x(x0, x1, cy, cz, w, h):
    r = min(w, h) / 2
    return (cyl_x(x0, x1, cy - (w / 2 - r), cz, r) + cyl_x(x0, x1, cy + (w / 2 - r), cz, r)
            + box(x0, x1, cy - (w / 2 - r), cy + (w / 2 - r), cz - r, cz + r))


def hole_y(y0, y1, cx, cz, w, h):
    r = min(w, h) / 2
    return (cyl_y(cx - (w / 2 - r), y0, y1, cz, r) + cyl_y(cx + (w / 2 - r), y0, y1, cz, r)
            + box(cx - (w / 2 - r), cx + (w / 2 - r), y0, y1, cz - r, cz + r))


def ports():
    """(the hole, its notch up to the seam in the back's wall)."""
    zj, zu, zk = z_back(JACK_UP), z_back(USB_UP), z_back(KNOB_UP)
    xr0, xr1 = OW - WALL - 1, OW + 1
    yb0, yb1 = -1, WALL + 1
    jy, ux, kx = by(JACK_V), bx(USB_U) + USB_FIX, bx(KNOB_U)
    bw = BAR_W / 2 + BAR_CLR
    top = Z_SPLIT + 0.01
    return ([(box(xr0, xr1, PTT_Y - bw, PTT_Y + bw, SLOT_SKIN, Z_SPLIT),
              box(xr0, xr1, PTT_Y - bw, PTT_Y + bw, -1, top))] if OLD_LEVER else []) + [
        (cyl_x(xr0, xr1, jy, zj, JACK_HOLE / 2),
         box(xr0, xr1, jy - JACK_HOLE / 2, jy + JACK_HOLE / 2, zj, top)),
        (hole_y(yb0, yb1, ux, zu, USB_W, USB_H),
         box(ux - USB_W / 2, ux + USB_W / 2, yb0, yb1, zu, top)),
        (hole_y(yb0, yb1, kx, zk, KNOB_W, KNOB_H),
         box(kx - KNOB_W / 2, kx + KNOB_W / 2, yb0, yb1, zk, top)),
    ]


def recesses():
    """Let into the side walls' outside round the jack and the USB-C, so a
    plug reaches its socket through PORT_WALL, not the full wall."""
    jy, zj = by(JACK_V), z_back(JACK_UP)
    ux, zu = bx(USB_U) + USB_FIX, z_back(USB_UP)
    return (cyl_x(OW - PORT_RECESS, OW + 1, jy, zj, JACK_RECESS_D / 2)
            + hole_y(-1, PORT_RECESS, ux, zu, *USB_RECESS))


def cell_round(extra=0.0):
    return cyl_y(HCX, HY0 - 5, HY1 + 5, CELL_Z, CELL_R + extra)


def speaker_room():
    ff = Z_FACE_IN
    return (cyl_z(SPK_X, SPK_Y, SPK_R + 0.3, ff - SPK_RIM, ff + 0.01)
            + cone_z(SPK_X, SPK_Y, SPK_R + 0.3, spk_r(SPK_CONE) + 0.3, ff - SPK_RIM, ff - SPK_CONE))


def holder_box(grow=0.0):
    return box(HX0 - grow, HX1 + grow, HY0 - grow, HY1 + grow, H_LO - grow, H_HI + grow)




def grille_xy():
    x0 = G_M + GRILLE_HOLE / 2
    return [(x0 + i * GRILLE_PITCH, G_Y0 + j * G_PY) for i in range(G_NX) for j in range(G_NY)]


# ---------------------------------------------------------- front shell
def front_shell():
    ff = Z_FACE_IN
    out = outside(Z_SPLIT, D)
    out = fillet(out.faces().sort_by(Axis.Z)[-1].edges(), FACE_R)
    s = out - inside(Z_SPLIT - 1, ff)

    if HOLDER_ON_FRONT:
        # the holder's cradle, as the back's was, hanging from the face: a pin
        # through its base's hole, a ridge round its foot, the stop wall at its
        # lower end (wires pass under it), cut short over the board
        g = HOLDER_CLR
        s += cyl_z(HCX, HCY, PIN_D / 2, ff - PIN_H, ff + 0.5)
        ridge = (box(HX0 - g - RIDGE_T, HX1 + g + RIDGE_T, HY0 - g - RIDGE_T, HY1 + g + RIDGE_T,
                     ff - RIDGE_H, ff + 0.5)
                 - box(HX0 - g, HX1 + g, HY0 - g, HY1 + g, 0, D))
        stop = box(HX0 - g - RIDGE_T, min(HX1 + g + RIDGE_T, bx(0) - 0.3), HY0 - g - RIDGE_T, HY0 - g,
                   ff - STOP_H, ff + 0.5)
        # the ridge clear of the speaker's pocket (no sliver between); below
        # the seam 0.2 off the back's wall
        ridge -= box(HX1 + g - 0.01, HX1 + g + RIDGE_T + 1, SPK_Y - SPK_BAND, SPK_Y + SPK_BAND, 0, D + 1)
        s += (ridge + stop) & (inside(Z_SPLIT, D, 0.5) + inside(0, Z_SPLIT + 0.01, -0.2))
    # the speaker's ring, clear of the holder
    ring = (cyl_z(SPK_X, SPK_Y, SPK_R + RIB, ff - RING_H, ff + 0.5)
            - (box(-1, SPK_X - 10, SPK_Y - SPK_BAND, SPK_Y + SPK_BAND, 0, D) if HOLDER_ON_FRONT
               else box(-1, HX1 + 0.3, -1, OH + 1, 0, D) if not SIDE_BY_SIDE else box(-1, 0, -1, 0, 0, 1)))
    s += ring & inside(Z_SPLIT, D, 0.5)
    s -= (cyl_z(SPK_X, SPK_Y, SPK_R, ff - SPK_RIM, ff)
          + cone_z(SPK_X, SPK_Y, SPK_R, SPK_R - (SPK_D - SPK_MAGNET) / 2, ff - SPK_RIM, ff - SPK_CONE)
          + cyl_z(SPK_X, SPK_Y, SPK_R, ff - RING_H - 1, ff - SPK_RIM))

    # posts: on the board at H1, H2; to the seam at the other two
    for (x, y, board) in SCREWS:
        s += cyl_z(x, y, POST_D / 2, Z_BF if board else Z_SPLIT, ff + 0.5)
    mx, my = bx(MIC_UV[0]), by(MIC_UV[1])
    s += cyl_z(mx, my, TUBE_OD / 2, Z_BF + GASKET, ff + 0.5)

    # two ribs that sit on the cell, cut concave to it
    # the lower one above the board, the holder's foot being under its edge
    for y in ((max(HY0 + 14.0, by(0) + CLR + RIB), HY1 - 14.0) if SADDLE else ()):
        rib = box(HX0 + 0.5, HX1 - 0.5, y - RIB / 2, y + RIB / 2, H_TOP - SADDLE_DEEP, ff + 0.5)
        rib = (rib - cell_round(0.1) - speaker_room()) & inside(Z_SPLIT - 10, D, 0.5)
        for piece in (rib.solids() if rib else []):
            if piece.volume > 1.0:
                s += piece

    # ---- cuts
    for (x, y, board) in SCREWS:
        s -= cyl_z(x, y, M3_TAP / 2, Z_SPLIT - 6, D - PILOT_SKIN)
    s -= cyl_z(mx, my, TUBE_ID / 2, Z_BF, ff + 0.01)
    s -= cyl_z(mx, my, MIC_HOLE / 2, ff - 0.1, D + 1)

    open_r = SPK_D / 2 - 3.0
    hr = GRILLE_HOLE / 2
    for (x, y) in grille_xy():
        if any(math.hypot(x - sx, y - sy) < M3_TAP / 2 + hr + 0.5 for sx, sy, _ in SCREWS):
            continue
        if (x - SPK_X) ** 2 + (y - SPK_Y) ** 2 <= open_r ** 2:
            s -= cyl_z(x, y, hr, ff - 1, D + 1)
        else:
            s -= cyl_z(x, y, hr, D - DIMPLE, D + 1)
    s -= box(-1, OW + 1, LINE_Y - GROOVE_W / 2, LINE_Y + GROOVE_W / 2, D - GROOVE_D, D + 1)

    x0, x1, v0, v1 = OLED_WIN
    s -= box(bx(x0), bx(x1), by(v1) + WIN_UP, by(v0) + WIN_UP, ff - 1, D + 1)
    pocket = Z_BF + OLED_TOP - ff + 0.2
    x0, x1, v0, v1 = OLED_GLASS
    s -= box(bx(x0) - CLR, bx(x1) + CLR, by(v1) - CLR, by(v0) + CLR, ff - 1, ff + pocket)
    for (u, v) in KEYS:
        x, y = bx(u), by(v)
        s -= rrect(x - HOLE_W / 2, y - HOLE_H / 2, HOLE_W, HOLE_H, CAP_R + CAP_CLR, ff - 1, D + 1)
        hw, hh = HOLE_W / 2 + COLLAR_T, HOLE_H / 2 + COLLAR_T
        s += (rrect(x - hw, y - hh, 2 * hw, 2 * hh, CAP_R + CAP_CLR + COLLAR_T, Z_FACE_IN - COLLAR_H, Z_FACE_IN + 0.01)
              - rrect(x - HOLE_W / 2, y - HOLE_H / 2, HOLE_W, HOLE_H, CAP_R + CAP_CLR, ff - 2, D + 1))

    for hole, _ in ports():
        s -= hole
    s -= recesses()
    # the hinge's anchor: joined to the wall above the seam, clear of the
    # back's wall below it; its tube down through the lever's bore
    if OLD_LEVER:
        s += column(0, Z_SPLIT, D) + column(0, ANCHOR_Z0, Z_SPLIT + 0.01, OW + 0.3)
        s -= cyl_z(PIV[0], PIV[1], LEV_HOLE / 2, ANCHOR_Z0 - 1, TUBE_TOP + 0.3)
        s -= cyl_z(PIV[0], PIV[1], M3_CLEAR / 2, TUBE_TOP, D + 1)
        s -= cone_z(PIV[0], PIV[1], M3_CLEAR / 2, CSK_D / 2 + 0.01, CSK_Z, D + 0.01)
    elif PADDLE:
        # the paddle's pin, down from the face's inside into a socket on the back
        s += cyl_z(PX, PY, PIN_R, PIN_Z0, Z_FACE_IN + 0.01)
    else:
        # wearable2's hinge: the column outside the right wall, hanging from
        # the face (below the seam only outside the wall), the M3's pilot up it
        s += ((w_column() & box(0, OW + 30, -20, OH + 20, 0, D))
              - box(-1, OW, -1, OH + 1, -1, Z_SPLIT + 0.3))
        s -= cyl_z(W_PIV[0], W_PIV[1], M3_TAP / 2, W_COL_Z - W_COL_R - 1, D - PILOT_SKIN)
    # posts down to the board's front face, opposite the back's supports:
    # the board rests on them while the case is put together (the user's)
    for (x0, x1, y0, y1) in SUPPORTS:
        s += box(x0, x1, y0, y1, Z_BF, Z_FACE_IN + 0.01)
    for pad in snap_pads():
        s = s.fuse(pad)
    for cut in snap_cuts():
        s -= cut
    s = s.fuse(hook_front())
    return s


# ------------------------------------------------------------ back tray
def column(grow=0.0, z0=0.0, z1=D, x_join=OW - 2.0):
    """The hinge's anchor footprint outside the corner, reaching to x_join."""
    pts = [(x_join - grow, PIV[1] - COL_R - grow), (PIV[0], PIV[1] - COL_R - grow),
           (PIV[0], PIV[1] + COL_R + grow), (x_join - grow, PIV[1] + COL_R + grow)]
    return prism_xy(pts, z0, z1) + cyl_z(PIV[0], PIV[1], COL_R + grow, z0, z1)


def back_tray():
    out = outside(Z_SPLIT, 0)
    out = fillet(out.faces().sort_by(Axis.Z)[0].edges(), FACE_R)
    s = out - inside(BACK, Z_SPLIT + 1)
    floor = BACK - 0.5

    for (x, y, board) in SCREWS:
        s += cyl_z(x, y, STANDOFF_D / 2, floor, Z_BB if board else Z_SPLIT)
        # a wide foot round the head's pocket, then 45 degrees in to the standoff
        top = M3_HEAD_H + BOSS_SKIN
        foot = (cyl_z(x, y, BOSS_D / 2, floor, top)
                + cone_z(x, y, BOSS_D / 2, STANDOFF_D / 2, top - 0.01, top + (BOSS_D - STANDOFF_D) / 2))
        s += (foot - holder_box(HOLDER_CLR)) & inside(0, D, 0.5)

    # the holder: a pin through its base, an outline ridge round its foot,
    # a stop wall at its lower end
    g = HOLDER_CLR
    if not HOLDER_ON_FRONT:
        s += cyl_z(HCX, HCY, PIN_D / 2, floor, BACK + PIN_H)
    ridge = (box(HX0 - g - RIDGE_T, HX1 + g + RIDGE_T, HY0 - g - RIDGE_T, HY1 + g + RIDGE_T,
                 floor, BACK + RIDGE_H)
             - box(HX0 - g, HX1 + g, HY0 - g, HY1 + g, 0, D))
    stop = box(HX0 - g - RIDGE_T, HX1 + g + RIDGE_T, HY0 - g - RIDGE_T, HY0 - g, floor, BACK + STOP_H)
    if CRADLE and not HOLDER_ON_FRONT:
        s += (ridge + stop) & inside(0, D, 0.5)

    # posts under the board either side of the display, up to its back face
    # as the standoffs are, so hard typing can't bend it (the user's); on
    # bare board, clear of every part's courtyard
    for (x0, x1, y0, y1) in BACK_SUPPORTS:
        s += box(x0, x1, y0, y1, floor, Z_BB)



    # ---- cuts
    for (x, y, board) in SCREWS:
        s -= cyl_z(x, y, M3_CLEAR / 2, -1, D)
        s -= head_pocket(x, y)
    for hole, notch in ports():
        s -= hole + notch
    s -= recesses()
    if OLD_LEVER:
        # the floor cut for the claw, open to the back face and the slot
        s -= box(OW - WALL - CLAW_X - CLAW_CLR - CLAW_IN, OW - WALL + 0.01,
                 PTT_Y - CLAW_Y - CLAW_CLR, PTT_Y + CLAW_Y + CLAW_CLR, SLOT_SKIN, LEV_Z + CLAW_Z1 + 0.5)
    elif PADDLE:
        # the paddle's window in the right wall, open up to the seam
        s -= box(OW - WALL - 0.5, OW + 1, PAD_Y0 - WIN_END, PAD_Y1 + WIN_END, PAD_Z0 - PAD_CLR, Z_SPLIT + 1)
        # the pin's socket on the floor
        s += cyl_z(PX, PY, SOCKET_R, BACK - 0.5, SOCKET_TOP)
        s -= cyl_z(PX, PY, PIN_R + PIN_CLR, PIN_Z0 - 1.0, SOCKET_TOP + 1)
    else:
        # wearable2's ports() slot: through the wall, open to the back face
        # and the seam, FLOOR_SLOT into the floor under the bar's rising
        # underside; wider on the hinge's side for the swing
        w = W_BAR_W / 2 + W_BAR_CLR
        sw = W_BAR_TRAVEL * (W_PIV[0] - (OW - WALL - W_FLOOR_SLOT)) / (PTT_Y - W_PIV[1])
        s -= box(OW - WALL - W_FLOOR_SLOT, OW + 1, PTT_Y - w - sw, PTT_Y + w, -1, Z_SPLIT)
    if PTT_SUPPORT and not OLD_LEVER:
        # under the PTT's body, so pressing can't push it down off the board
        s += box(PTT_SUP_X0, PTT_SUP_X1, PTT_Y - 2.3, PTT_Y + 2.3, BACK - 0.5, PTT_BODY_Z0 - 0.1)
    for slit in snap_slits():
        s -= slit
    for tab in snap_tabs():
        s = s.fuse(tab)
    for cut in hook_back_cuts():
        s -= cut
    return s


# ------------------------------------------- the paddle lever (2026-11-24)
# The user's sketch: the lever inside, hinged on a pin from the front
# panel's inside some 14 mm over the PTT; a flat paddle hangs from it in a
# window in the back's right wall, 1.0 proud as a large button, 0.4 off the
# plunger at rest; a tail at its foot behind the wall under the window is
# the outward stop. Only the PTT's pad of it reaches up to the plunger: by
# the diodes (D1, D3 to the board's back face) it stays under them.
PTT_SUPPORT = False     # none under the PTT, as on wearable2 (the user's, 2026-11-24)
PADDLE = False          # the inside paddle: the board stopped it closing on the switch (the user's, 2026-11-24)
AIR = 0.4                                   # the paddle off the plunger at rest
PAD_PROUD, PAD_CLR, WIN_END = 1.0, 0.3, 0.4  # out of the wall at the hinge's end; round it in the window, its ends
PAD_PROUD_FOOT = 2.5                         # at its foot: the face tilts out, to be seen as the PTT (the user's)
PAD_X0 = OW - WALL                          # its inner face, on the wall's inside
PAD_X1 = OW + PAD_PROUD_FOOT
assert not PADDLE or abs(PAD_X0 - (PTT_X + AIR)) < 1e-6, "the paddle's air gap"
PAD_Y0, PAD_Y1 = 26.4, 47.0                 # along the wall: from under the diodes to over the board's top
PAD_Z0 = 5.0                                # its foot (on the bed as printed)
PAD_Z1 = Z_SPLIT - PAD_CLR                  # its top, under the front's wall
PAD_LOW = 14.5                              # the paddle's inner part under the diodes' bodies
PAD_RECESS = 0.6                            # its inner face set back over PAD_LOW, but at the PTT
PIN_R, PIN_CLR = 2.0, 0.2                   # the pin; the eye round it (0.4 rattled on wearable2)
EYE_R = PIN_R + PIN_CLR + 1.6
PX, PY = OW - WALL - 0.4 - EYE_R, PTT_Y + 14.0
PIN_Z0 = 2.4                                # its tip, in the socket
SOCKET_R, SOCKET_TOP = PIN_R + PIN_CLR + 1.2, 4.0
TAIL_W, TAIL_Y = 2.0, 3.0                   # the stop: in from the wall, down past the window
PTT_BODY_Z0 = Z_BB - 3.3                    # the PTT's body under the board (ASSUMED 3.3, as parts())
PTT_SUP_X0, PTT_SUP_X1 = bx(BW) - 0.78 - 3.4, bx(BW) - 2.8   # clear of the T head's swing


def paddle_lever():
    z0, z1 = PAD_Z0, PAD_Z1
    # the paddle: low inside under the diodes, full height in the wall, and
    # the PTT's pad full height to its inner face
    s = (box(PAD_X0, OW, PAD_Y0, PAD_Y1, z0, PAD_LOW)
         + box(PAD_X0 + PAD_RECESS, OW, PAD_Y0, PAD_Y1, z0, z1)
         + box(PAD_X0, OW, PTT_Y - 2.5, PTT_Y + 2.5, z0, z1))
    # the button outside the wall: a wedge, PAD_PROUD at the hinge's end
    # out to PAD_PROUD_FOOT at the foot
    s += prism_xy([(OW - 0.01, PAD_Y0), (OW + PAD_PROUD_FOOT, PAD_Y0),
                   (OW + PAD_PROUD, PAD_Y1), (OW - 0.01, PAD_Y1)], z0, z1)
    # the foot's outer corner cut back, so it clears the window's end as the
    # paddle swings in (the window is printed; only the lever is changed)
    s -= box(PAD_X0 - 0.01, OW + PAD_PROUD_FOOT + 1, PAD_Y0 - 1, PAD_Y0 + 0.4, z0 - 1, z1 + 1)
    # the tail behind the wall under the window: the stop
    # (up behind the paddle 3 mm, so their joint is long enough to take the stop)
    s += (box(PAD_X0 - TAIL_W, PAD_X0, PAD_Y0 - TAIL_Y, PAD_Y0 + 3.0, z0, PAD_LOW)
          + box(PAD_X0 - TAIL_W, PAD_X0 + 0.01, PAD_Y0 + 0.4, PAD_Y0 + 3.0, z0, PAD_LOW))
    # the arm up to the eye, inside, 0.3 off the wall above the window
    s += box(PX, PAD_X0 + 0.5, PAD_Y1 - 2.4, PAD_Y1, z0, PAD_LOW)
    s += box(PX - EYE_R + 0.5, PAD_X0 - 0.3, PAD_Y1 - 2.4, PY, z0, PAD_LOW)
    s += cyl_z(PX, PY, EYE_R, z0, PAD_LOW)
    s -= cyl_z(PX, PY, PIN_R + PIN_CLR, z0 - 1, PAD_LOW + 1)
    return s


# ------------------------------------- wearable2's PTT lever (2026-11-24)
# The user's: "use what was on wearable2 for the PTT". Its hinge on a column
# hanging from the front outside the right wall, over the jack; an M3 from
# the lever's back through it into the column clamps it; a bar through a
# slot in the back's wall onto the plunger, AIR_GAP off it at rest. Here the
# board's edge is 0.4 off the wall (wearable2's 4.7), so the T head that is
# the outward stop can't stand between the plunger and the wall: it is at
# the bar's foot instead, under the switch, its wings W_HEAD_GAP off the
# wall's inside. The bar's top W_BAR_UNDER under the board, so the board
# can't stop it (the user's: it stopped the paddle).
W_BAR_W, W_BAR_CLR, W_BAR_UNDER = 2.0, 0.3, 0.3     # wearable2's
W_BAR_TOP = Z_BB - W_BAR_UNDER
W_PRELOAD = -AIR
W_HEAD_W, W_HEAD_GAP = 4.0, 0.1
W_HEAD_T = (OW - WALL - W_HEAD_GAP) - (PTT_X - W_PRELOAD)
W_FLOOR_SLOT, W_BAR_TRAVEL = 2.3, 1.6
W_LEV_GAP, W_LEV_T, W_LEV_FLARE = 2.0, 3.0, 2.0
W_LEV_IN = OW + W_LEV_GAP
W_COL_R, W_COL_H, W_NECK_W, W_PIV_R = 3.2, 7.0, 3.0, 5.2
W_COL_Z = D - W_COL_H
W_COL_TIP = W_COL_Z - W_COL_R + M3_TAP / 2 + 0.8
W_PIV = (OW + W_PIV_R + 0.4, round(by(JACK_V) + 10.0 / 2 + 0.5 + W_PIV_R, 1))
W_LEV_TOP = PTT_Y + 4.85                    # as far over the PTT as wearable2's
W_LEV_CLR, W_NECK_CLR, W_LEV_SCREW = 0.2, 0.3, 3.2
assert W_HEAD_T >= 1.35, ("the T head", W_HEAD_T)   # wearable2's 1.9; 1.4 here
assert W_BAR_TOP >= PTT_Z + PTT_PLUNGER / 2, "the bar misses the plunger's top"


def w_column(grow=0.0):
    """wearable2's column(): round from the face down to W_COL_Z, a 45
    degree point under; its neck to the wall, a 45 degree point too."""
    r, z = W_COL_R + grow, W_COL_Z - grow
    c = (cyl_z(W_PIV[0], W_PIV[1], r, z, D + 1)
         + cone_z(W_PIV[0], W_PIV[1], 0.01, r, z - r, z + 0.01))
    w = W_NECK_W + 2 * grow
    x0 = OW - 1.0 - grow
    neck = (box(x0, W_PIV[0], W_PIV[1] - w / 2, W_PIV[1] + w / 2, z, D + 1)
            + Pos(x0, W_PIV[1], z) * Rot(45, 0, 0)
            * Box(W_PIV[0] - x0, w / 2 ** 0.5, w / 2 ** 0.5, align=(Align.MIN, Align.CENTER, Align.CENTER)))
    if grow == 0.0:
        # its point cut off flat where the wall round the M3's pilot is 0.8
        c -= box(W_PIV[0] - 10, W_PIV[0] + 10, W_PIV[1] - 10, W_PIV[1] + 10, -1, W_COL_TIP)
    return c + neck


def w_lever_pocket():
    """wearable2's: W_LEV_CLR round the column, W_NECK_CLR round its neck
    swept over the swing."""
    r, z = W_COL_R + W_LEV_CLR, W_COL_Z - W_LEV_CLR
    p = (cyl_z(W_PIV[0], W_PIV[1], r, z, D + 1)
         + cone_z(W_PIV[0], W_PIV[1], 0.01, r, z - r, z + 0.01))
    g = W_NECK_CLR
    zn, w, x0 = W_COL_Z - g, W_NECK_W + 2 * g, OW - 1.0 - g
    neck = (box(x0, W_PIV[0], W_PIV[1] - w / 2, W_PIV[1] + w / 2, zn, D + 1)
            + Pos(x0, W_PIV[1], zn) * Rot(45, 0, 0)
            * Box(W_PIV[0] - x0, w / 2 ** 0.5, w / 2 ** 0.5, align=(Align.MIN, Align.CENTER, Align.CENTER)))
    for a in (-8.0, -6.0, -4.0, -2.0, 0.0, 1.0):
        p += Pos(W_PIV[0], W_PIV[1], 0) * Rot(0, 0, a) * Pos(-W_PIV[0], -W_PIV[1], 0) * neck
    return p


def w_lever():
    """wearable2's lever(), printed on its back."""
    L, T, F = W_LEV_IN, W_LEV_T, W_LEV_FLARE
    py = W_PIV[1]
    s = prism_xy([(L, py), (L + T, py), (L + T, py + 3.0), (L + T + F, py + 11.0),
                  (L + T + F, W_LEV_TOP), (L, W_LEV_TOP)], 0, D)
    s += cyl_z(W_PIV[0], py, W_PIV_R, 0, D)
    try:
        s = fillet(s.edges().filter_by(Axis.Z), 1.5)
    except Exception:
        pass
    s -= w_lever_pocket()
    s -= cyl_z(W_PIV[0], py, W_LEV_SCREW / 2, -1, D + 1)
    s -= head_pocket(W_PIV[0], py)
    # wearable2's bar: from the lever's back face through the wall, inside
    # it its underside rising at 45 degrees; a 45 degree roof
    x0 = PTT_X - W_PRELOAD
    xi = OW - WALL
    roof = W_BAR_W / 2
    zt = W_BAR_TOP - roof
    side = prism_xy([(x0, xi - x0), (xi, 0), (L + 0.5, 0), (L + 0.5, zt), (x0, zt)],
                    -W_BAR_W / 2, W_BAR_W / 2)
    bar = Pos(0, PTT_Y, 0) * Rot(90, 0, 0) * side
    bar = bar.fuse(Pos(x0, PTT_Y, zt) * Rot(45, 0, 0)
                   * Box(L + 0.5 - x0, W_BAR_W / 2 ** 0.5, W_BAR_W / 2 ** 0.5,
                         align=(Align.MIN, Align.CENTER, Align.CENTER)))
    # its T head on the plunger, flat on top; under it 45 degrees in to the bar
    hz0 = PTT_Z - PTT_PLUNGER / 2 - 0.5
    hz1 = W_BAR_TOP
    taper = (W_HEAD_W - W_BAR_W) / 2
    bar = bar.fuse(box(x0, x0 + W_HEAD_T, PTT_Y - W_HEAD_W / 2, PTT_Y + W_HEAD_W / 2, hz0, hz1))
    wedge = prism_xy([(-W_HEAD_W / 2, hz0), (W_HEAD_W / 2, hz0), (W_BAR_W / 2, hz0 - taper),
                      (-W_BAR_W / 2, hz0 - taper)], 0, W_HEAD_T)
    bar = bar.fuse(Pos(x0, PTT_Y, 0) * Rot(0, 0, 90) * Rot(90, 0, 0) * wedge)
    whole_lever = s.solids()[0]
    for piece in s.solids()[1:]:
        whole_lever = whole_lever.fuse(piece)
    return whole_lever.fuse(bar).clean()


# ---------------------------------------------------------- the snaps
# No screw reaches the top-left corner, over the cell (the user's): a snap
# on the left wall and one on the top wall near it. A tab rises off the
# back's wall past the seam, in the wall's outer half, flush outside; a
# barb on its inner face springs into a window in the front's wall, its
# flat underside the catch; a 45 degree lead-in over it. Pried open with
# a nail from outside.
SNAP_W, SNAP_T, SNAP_L = 8.0, 0.8, 6.0      # wide, thick, up past the seam
SNAP_BARB, SNAP_CLR = 0.9, 0.1               # the barb's reach (0.6 didn't catch); the tab's inner face
SNAP_ROOT = 5.0                              # the tab springs from this far under the seam (11 mm to flex)
SNAP_SIDE = 0.1                              # round the tab in the front's slot, sides and top (the user's: close the seams)
SNAP_SLIT = 0.35                             # the slits down its sides in the back's wall, so it can flex
SNAP_PAD = 0.15                              # inside the front's wall: its strip by the window 0.85, two lines
SNAP_PLAY = 0.1                              # the catch's play
SNAP_AT = []     # the tabs out (the user's, 2026-11-25): the holder, 0.3 inside the wall, left them no room to flex; the hook below instead
# the top-left corner's hook (the user's, 2026-11-25), rigid, in the top wall
# over the holder: a tongue from the front's wall, its inner HOOK_T, reaches
# HOOK_L past the seam, a nose out at its tip; the back's wall takes the
# tongue in a rebate, the nose in a window through its outer part, under a
# bar. The back goes on tilted ~5 degrees (its bottom end up), HOOK_SLIDE
# toward the top; its top edge slides down onto the nose, then it swings
# shut about the seam's outer edge; the screws hold it
HOOK_X0, HOOK_X1 = 4.0, 20.0
HOOK_T, HOOK_L, HOOK_NH, HOOK_C = 0.7, 3.0, 1.2, 0.1
HOOK_N = 0.85                # the nose's reach under the bar, to 0.05 short of the outside (the user's, 2026-11-25: 0.5 didn't lock)
HOOK_SLIDE = 0.95
# three short noses, full-wall pillars between their windows, so the bar
# over each window is a short bridge (the user's, 2026-11-25: 16 mm was long)
HOOK_SEGS, HOOK_PILLAR = 3, 1.4
_HOOK_GAP = HOOK_PILLAR + 2 * HOOK_C
_HOOK_SEG = (HOOK_X1 - HOOK_X0 - (HOOK_SEGS - 1) * _HOOK_GAP) / HOOK_SEGS
HOOK_NOSES = [(HOOK_X0 + i * (_HOOK_SEG + _HOOK_GAP), HOOK_X0 + i * (_HOOK_SEG + _HOOK_GAP) + _HOOK_SEG)
              for i in range(HOOK_SEGS)]
HOOK_IN = 0.15               # the tongue stands this far in past the wall's inside (the holder 0.39 off it): 0.85 thick
_HY_I = OH - WALL
_HOOK_BAR = WALL - HOOK_T - HOOK_C
assert _HOOK_BAR >= 0.8 - 1e-9, "the bar over the nose under two lines"
assert HOOK_SLIDE >= HOOK_N + 0.1 - 1e-9, "the slide doesn't clear the nose"
assert HOOK_T + HOOK_IN >= 0.8 and OH - WALL - HOOK_IN - HY1 >= 0.2, "the tongue"


def hook_front():
    zs = Z_SPLIT
    s = box(HOOK_X0, HOOK_X1, _HY_I - HOOK_IN, _HY_I + HOOK_T, zs - HOOK_L, zs + 0.5)
    for (a, b) in HOOK_NOSES:
        s = s.fuse(box(a, b, _HY_I + HOOK_T - 0.01, _HY_I + HOOK_T + HOOK_N, zs - HOOK_L, zs - HOOK_L + HOOK_NH))
    return s


def hook_back_cuts():
    zs, c = Z_SPLIT, HOOK_C
    return [box(HOOK_X0 - c, HOOK_X1 + c, _HY_I - 0.5, _HY_I + HOOK_T + c, zs - HOOK_L - c, zs + 0.01),
            ] + [box(a - c, b + c, _HY_I + HOOK_T - 0.01, OH + 1, zs - HOOK_L - c, zs - HOOK_L + HOOK_NH + c)
                 for (a, b) in HOOK_NOSES]
assert SNAP_T + SNAP_BARB < WALL + SNAP_PAD, "the barb would reach into the case"


def _snap_frame(wall, shape):
    """Built on the left wall (x inward from its outside, y along it);
    turned onto the top wall (inward is -y from OH)."""
    return shape if wall == "left" else Pos(0, OH, 0) * Rot(0, 0, -90) * shape


def _barb(c):
    zc = Z_SPLIT + SNAP_L - 1.4
    pts = [(SNAP_T - 0.01, zc), (SNAP_T + SNAP_BARB, zc), (SNAP_T + SNAP_BARB, zc + 0.4),
           (SNAP_T - 0.01, zc + 0.4 + SNAP_BARB + 0.01)]
    prof = extrude(Polygon(*pts, align=None), SNAP_W)        # in x-y, out along z
    return Pos(0, c - SNAP_W / 2, 0) * Rot(90, 0, 0) * Pos(0, 0, -SNAP_W) * prof, zc


def snap_tabs():
    out = []
    for wall, c in SNAP_AT:
        barb, _ = _barb(c)
        tab = box(0, SNAP_T, c - SNAP_W / 2, c + SNAP_W / 2, Z_SPLIT - SNAP_ROOT - 0.01, Z_SPLIT + SNAP_L).fuse(barb)
        out.append(_snap_frame(wall, tab))
    return out


def snap_slits():
    """Cut from the back's wall round each tab: a slit down each side and
    the wall behind it, so it springs from SNAP_ROOT under the seam."""
    w = SNAP_W / 2 + SNAP_SLIT
    return [_snap_frame(wall, box(-1, WALL + 0.01, c - w, c + w, Z_SPLIT - SNAP_ROOT, Z_SPLIT + 1))
            for wall, c in SNAP_AT]


def snap_cuts():
    out = []
    for wall, c in SNAP_AT:
        _, zc = _barb(c)
        w = SNAP_W / 2 + SNAP_SIDE
        cut = (box(-1, SNAP_T + SNAP_CLR, c - w, c + w, Z_SPLIT - 1, Z_SPLIT + SNAP_L + SNAP_SIDE)
               + box(SNAP_T, WALL + SNAP_PAD + 0.01, c - w, c + w, zc - SNAP_PLAY, zc + 0.4 + SNAP_BARB + 0.3))
        out.append(_snap_frame(wall, cut))
    return out


def snap_pads():
    out = []
    for wall, c in SNAP_AT:
        _, zc = _barb(c)
        w = SNAP_W / 2 + SNAP_SIDE + 1.0
        out.append(_snap_frame(wall, box(WALL - 0.01, WALL + SNAP_PAD, c - w, c + w,
                                         Z_SPLIT + 0.01, zc + 0.4 + SNAP_BARB + 1.0)))
    return out


# --------------------------------------------------------------- lever
def lever():
    """Built in its own frame (z 0 its back face, LEV_D its top), as printed
    for the 25 mm case, then set LEV_Z up: the face, the board and the right
    wall are where they were to it."""
    L, T, F = LEV_IN, LEV_T, LEV_FLARE
    z_bb, z_anc, z_tube = Z_BB - LEV_Z, ANCHOR_Z0 - LEV_Z, TUBE_TOP - LEV_Z
    y_f0, y_f1 = PTT_Y + 8.0, PTT_Y - 2.0
    body = prism_xy([(L, PIV[1]), (L + T, PIV[1]), (L + T, y_f0), (L + T + F, y_f1),
                     (L + T + F, LEV_END), (L, LEV_END)], 0, LEV_D)
    boss = cyl_z(PIV[0], PIV[1], PIV_R, 0, LEV_D)
    s = body + boss
    try:
        s = fillet(s.edges().filter_by(Axis.Z), 0.8)
    except Exception:
        pass
    # the bar through its slot onto the plunger
    x0 = PTT_X - PRELOAD
    bar = box(x0, L + 0.5, PTT_Y - BAR_W / 2, PTT_Y + BAR_W / 2,
              BAR_BOT, z_bb - BAR_TOP)
    # the claw behind the wall at its foot: it rests on the wall's inside
    claw = box(OW - WALL - CLAW_X, OW - WALL, PTT_Y - CLAW_Y, PTT_Y + CLAW_Y, CLAW_Z0, CLAW_Z1)
    s += bar + claw
    # the hinge stops under the anchor and turns on its tube
    s -= column(LEV_CLR, z_anc - LEV_CLR, LEV_D + 1)
    s -= cyl_z(PIV[0], PIV[1], PIV_R + 1, z_anc - LEV_CLR, LEV_D + 1)
    # the tube up through the anchor; the screw's pilot down it
    s += cyl_z(PIV[0], PIV[1], TUBE_D / 2, z_anc - LEV_CLR - 0.1, z_tube)
    s -= cyl_z(PIV[0], PIV[1], M3_PILOT / 2, 1.0, LEV_D)
    return s.moved(Location((0, 0, LEV_Z)))


# ---------------------------------------------------------------- keytops
def keytops():
    """As wearable2's: one on each key, printed on its flange; the plunger in
    a socket in the flange; the frame fused to them all."""
    s = None
    for (u, v) in KEYS:
        x, y = bx(u), by(v)
        top = rrect(x - CAP_W / 2, y - CAP_H / 2, CAP_W, CAP_H, CAP_R, CAP_Z0 + CAP_FLANGE_T - 0.01, CAP_Z1)
        top = fillet(top.faces().sort_by(Axis.Z)[-1].edges(), CAP_ROUND)
        k = rrect(x - FLANGE_W / 2, y - FLANGE_H / 2, FLANGE_W, FLANGE_H, CAP_R + 0.8,
                  CAP_Z0, CAP_Z0 + CAP_FLANGE_T) + top
        k -= cyl_z(x, y, CAP_SOCKET_D / 2, CAP_Z0 - 1, CAP_Z0 + CAP_SOCKET)
        s = k if s is None else s + k
    u = key_frame()
    for k in (s if isinstance(s, (list, tuple)) else s.solids()):
        u = u.fuse(k)
    return u.clean()


def key_frame():
    """wearable2's (the user's sketch): a U round the row of three, each tied
    to the top bar; the fourth key hangs from the bottom bar."""
    ys = sorted({round(by(v), 1) for (_, v) in KEYS})
    row = [(bx(u), by(v)) for (u, v) in KEYS if abs(by(v) - ys[-1]) < 1]
    lone = [(bx(u), by(v)) for (u, v) in KEYS if abs(by(v) - ys[-1]) >= 1]
    assert len(row) == 3 and len(lone) == 1, "the keys aren't a row of three and one"
    (lx, ly), ry = lone[0], max(y for _, y in row)
    W_, G = FRAME_W, FRAME_GAP
    x0 = min(x for x, _ in row) - FLANGE_W / 2 - G - W_
    yb0 = ly + FLANGE_H / 2 + G
    wb = min(W_, min(y for _, y in row) - FLANGE_H / 2 - G - yb0)
    assert wb >= 0.6, f"the bottom bar would be {wb:.2f} wide"
    yt1 = ry + FLANGE_H / 2 + G + W_
    z0, z1 = CAP_Z0, CAP_Z0 + FRAME_T
    xr = max(x for x, _ in row) + W_ / 2
    # the mic's tube stands where wearable2's bottom bar ran: the bar goes
    # under it, the left bar down to meet it, a riser back up past it
    mx, my = bx(MIC_UV[0]), by(MIC_UV[1])
    xm = mx + TUBE_OD / 2 + G                    # the riser, right of the tube
    x0 = min(x0, mx - TUBE_OD / 2 - G - W_)      # the left bar, left of the tube
    yd1 = my - TUBE_OD / 2 - G
    yd0 = yd1 - W_
    assert x0 < xm < lx - FLANGE_W / 2 - G - W_ and yd1 < yb0
    f = (box(x0, x0 + W_, yd0, yt1, z0, z1)
         + box(x0, xr, yt1 - W_, yt1, z0, z1)
         + box(x0, xm + W_, yd0, yd1, z0, z1)
         + box(xm, xm + W_, yd0, yb0 + wb, z0, z1)
         + box(xm, lx + W_ / 2, yb0, yb0 + wb, z0, z1))
    for (x, y) in row:
        f += box(x - W_ / 2, x + W_ / 2, y + FLANGE_H / 2 - 0.3, yt1 - W_ + 0.01, z0, z1)
    f += box(lx - W_ / 2, lx + W_ / 2, ly + FLANGE_H / 2 - 0.3, yb0 + 0.01, z0, z1)
    return f


# --------------------------------------------- the parts, for looking only
def parts():
    board = (box(bx(0), bx(BW), by(BH), by(0), Z_BB, Z_BF)
             - box(bx(-1), bx(NOTCH_U), by(NOTCH_V), by(-1), Z_BB - 1, Z_BF + 1))
    for (u, v) in HOLES:
        board -= cyl_z(bx(u), by(v), 1.6, Z_BB - 1, Z_BF + 1)
    x0, x1, v0, v1 = OLED_PCB
    glass_z = Z_BF + OLED_TOP
    oled = box(bx(x0), bx(x1), by(v1), by(v0), glass_z - 2.3, glass_z - 1.3)
    gx0, gx1, gv0, gv1 = OLED_GLASS
    glass = box(bx(gx0), bx(gx1), by(gv1), by(gv0), glass_z - 1.3, glass_z)
    keys = None
    for (u, v) in KEYS:
        x, y = bx(u), by(v)
        k = (box(x - 3, x + 3, y - 3, y + 3, Z_BF, Z_BF + KEY_BODY)
             + cyl_z(x, y, 1.5, Z_BF + KEY_BODY - 0.1, Z_BF + KEY_TOP))
        keys = k if keys is None else keys + k
    c = PTT_Y
    ptt = (box(bx(BW) - 0.78 - 3.4, bx(BW) - 0.78, c - 2.3, c + 2.3, Z_BB - 3.3, Z_BB)
           + box(bx(BW) - 0.8, bx(BW), c - PTT_PLUNGER / 2, c + PTT_PLUNGER / 2,
                 PTT_Z - PTT_PLUNGER / 2, PTT_Z + PTT_PLUNGER / 2))
    jy, jz = by(JACK_V), z_back(JACK_UP)
    jack = (box(bx(BW) - 0.1 - 2.0 - 11.8, bx(BW) - 0.1 - 2.0, jy - 2.9, jy + 2.9, Z_BB - 5.0, Z_BB)
            + cyl_x(bx(BW) - 2.2, bx(BW) - 0.1, jy, jz, 2.3))
    ux, uz = bx(USB_U), z_back(USB_UP)
    mini = (box(ux - 9.175, ux + 9.175, by(BH) + 0.15, by(BH) + 22.65, Z_BB - 1.0, Z_BB)
            + box(ux - 4.47, ux + 4.47, by(BH) - 0.3, by(BH) + 7.0, uz - 1.6, uz + 1.6))
    kx = bx(KNOB_U)
    slide = (box(kx - 3.3, kx + 3.3, by(BH) + 1.0, by(BH) + 3.7, Z_BB - 1.4, Z_BB)
             + box(kx - 0.6, kx + 0.6, by(BH) - 1.0, by(BH) + 1.0, Z_BB - 1.5, Z_BB))
    ff = Z_FACE_IN
    speaker = (cyl_z(SPK_X, SPK_Y, SPK_D / 2, ff - SPK_RIM, ff)
               + cone_z(SPK_X, SPK_Y, SPK_D / 2, SPK_MAGNET / 2, ff - SPK_RIM, ff - SPK_CONE)
               + cyl_z(SPK_X, SPK_Y, SPK_MAGNET / 2, ff - SPK_DEEP, ff - SPK_CONE))
    # the holder as a tray under the cell, its hole round the pin
    base = 1.2
    if HOLDER_ON_FRONT:
        holder = (box(HX0, HX1, HY0, HY1, CELL_Z, H_HI)
                  - box(HX0 + 1.2, HX1 - 1.2, HY0 + 1.5, HY1 - 1.5, -1, H_HI - base)
                  - cyl_z(HCX, HCY, 1.6, 0, D))
    else:
        holder = (box(HX0, HX1, HY0, HY1, BACK, CELL_Z)
                  - box(HX0 + 1.2, HX1 - 1.2, HY0 + 1.5, HY1 - 1.5, BACK + base, D)
                  - cyl_z(HCX, HCY, 1.6, 0, D))
    cell = cyl_y(HCX, HY0 + 5.0, HY1 - 5.0, CELL_Z, CELL_R)
    return [(board, "board", (0.10, 0.40, 0.25)),
            (oled, "oled", (0.10, 0.25, 0.65)),
            (glass, "glass", (0.01, 0.01, 0.01)),
            (keys, "keys", (0.85, 0.85, 0.85)),
            (ptt, "ptt", (0.75, 0.75, 0.75)),
            (jack, "jack", (0.25, 0.25, 0.25)),
            (mini, "supermini", (0.20, 0.20, 0.55)),
            (slide, "switch", (0.9, 0.9, 0.9)),
            (speaker, "speaker", (0.55, 0.55, 0.55)),
            (holder, "holder", (0.15, 0.15, 0.15)),
            (cell, "cell", (0.70, 0.72, 0.75))]


# ---------------------------------------------------------------- output
SHELL = (0.30, 0.32, 0.35)
LEVER = (0.8, 0.15, 0.1)


def whole(shape):
    return shape if hasattr(shape, "wrapped") else Compound(list(shape))


def check(front, back, lev, caps):
    for name, p in (("front", front), ("back", back), ("lever", lev), ("keytops", caps)):
        print(f"{name}: {len(p.solids())} solid(s)")
    shells = [(front, "front"), (back, "back"), (lev, "lever"), (caps, "keytops")]
    things = [(p, n) for p, n, _ in parts()]
    bad = 0
    seen = set()
    for a in shells + things:
        for b in shells + things:
            k = tuple(sorted((a[1], b[1])))
            # the speaker over the holder was tried on the bench and fits
            # (2026-11-18); the speaker's frame here is only an outline
            pair = (bool({a[1], b[1]} & {"cell", "holder"}) and {a[1], b[1]} != {"cell", "holder"}
                    and "speaker" not in (a[1], b[1]))
            if a[1] == b[1] or k in seen or (a in things and b in things and not pair):
                continue
            seen.add(k)
            i = whole(a[0]).intersect(whole(b[0]))
            v = sum(s.volume for s in i.solids()) if i else 0
            if v > 0.05:
                print(f"  {a[1]} meets {b[1]}: {v:.2f} mm3")
                bad += 1
    print("clear" if not bad else f"{bad} overlaps")


def glb(items, path):
    shapes = []
    for shape, label, rgb in items:
        loose = shape.solids() if len(shape.solids()) > 1 else [shape]
        for n, piece in enumerate(loose):
            piece.color = Color(*rgb)
            piece.label = label if len(loose) == 1 else f"{label}{n}"
            shapes.append(piece)
    export_gltf(Compound(children=shapes), path, binary=True,
                linear_deflection=0.03, angular_deflection=0.2)
    print(f"wrote {path}")


def main():
    print(f"box {OW:.1f} wide, {OH:.1f} tall, {D:.1f} thick "
          f"({(OW + LEV_GAP + LEV_T + LEV_FLARE) if OLD_LEVER else PAD_X1 if PADDLE else W_PIV[0] + W_PIV_R:.1f} wide with the lever); "
          f"seam {D - Z_SPLIT:.1f} from the face")
    front = front_shell()
    back = back_tray()
    lev = lever() if OLD_LEVER else paddle_lever() if PADDLE else w_lever()
    caps = keytops()
    if "check" in sys.argv:
        check(front, back, lev, caps)
        return
    # to print: the front face down, the back and the lever on their backs
    export_stl(Pos(0, 0, D) * Rot(180, 0, 0) * front, os.path.join(OUT, "front.stl"))
    export_stl(back, os.path.join(OUT, "back.stl"))
    export_stl(lev.moved(Location((0, 0, -(LEV_Z if OLD_LEVER else PAD_Z0 if PADDLE else 0)))), os.path.join(OUT, "lever.stl"))
    export_stl(whole(caps).translate((0, 0, -CAP_Z0)), os.path.join(OUT, "keytops.stl"))
    print(f"wrote {OUT}/front.stl back.stl lever.stl keytops.stl")
    # one .glb per piece for viewer.html, which colours, outlines and
    # explodes them
    import json
    pieces = [(front, "front", SHELL), (back, "back", SHELL), (lev, "lever", LEVER),
              (caps, "keytops", (0.9, 0.9, 0.9))] + parts()
    os.makedirs(os.path.join(OUT, "pieces"), exist_ok=True)
    for shape, name, rgb in pieces:
        glb([(shape, name, rgb)], os.path.join(OUT, "pieces", f"{name}.glb"))
    with open(os.path.join(OUT, "pieces", "list.json"), "w") as f:
        json.dump([n for _, n, _ in pieces], f)

if __name__ == "__main__":
    main()
