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
An M3 Torx pan-head screw from the front, its head on show in a counterbore
in the face, threads into the tube and bears on its end only, so lever and
screw turn together in the anchor.

The halves close on five M3 self-tapping screws from the back: two through
the board's holes H1 and H2 (the board is clamped between the back's
standoffs and the front's posts), one in the top-right corner, one in the
strip left of the board, and one at the top right of the cell's holder. The seam is the board's back face.

    make printed            printed/*.stl and pieces/*.glb; open printed/viewer.html
    python printed.py check the overlap check
"""
import math
import os
import re
import sys

from build123d import (Align, Axis, Box, Color, Compound, Cone, Cylinder, Plane,
                       Polygon, Pos, RectangleRounded, Rot, export_gltf, export_stl,
                       extrude, fillet)

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = HERE
PCB = os.path.join(HERE, "..", "..", "pcb", "wificomms", "wificomms.kicad_pcb")

# ---------------------------------------------------------------- printing
WALL = 1.6          # side walls: four 0.4 mm perimeters
FACE = 1.6          # the front face
BACK = 2.0          # the back: screw heads' pockets, the claw's cut
PORT_WALL, PORT_RECESS = 1.0, 0.6   # the side wall at the jack and USB-C, let in from outside
JACK_RECESS_D = 9.0
USB_RECESS = (13.0, 7.5)            # round a USB-C plug's moulded body
FACE_R = 1.0        # the front face's and the back's outer edges
CORNER = 3.0        # the outside's corners seen from the front
IN_CORNER = 1.0
CLR = 0.4           # board to the walls
D = 25.0            # the thickness

# ---------------------------------------------------------------- board
# KiCad's view is the board's F side, the talkie's back; from the front
# x runs the other way: u = KX1 - x, v = y - KY0
KX1, KY0 = 168.3, 60.0
BW, BH = 58.3, 42.11
NOTCH_U, NOTCH_V = KX1 - 149.6, 77.2 - KY0      # top-left, for the cell
BOARD_R = 2.5
BT = 1.6
FRONT_H = 2.8       # the face's inside to the board's front


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
KEY_TOP, KEY_BODY = 4.3, 2.6          # B3FS-1010P
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
KNOB_U = uv("SW_POWER1")[0]
KNOB_UP, KNOB_W, KNOB_H = 0.75, 4.6, 2.2

# ---------------------------------------------------------- speaker, cell
SPK_D, SPK_DEEP, SPK_MAGNET = 50.0, 15.0, 30.0
SPK_RIM, SPK_CONE = 2.6, 7.5
SPK_CLR = 0.3
RIB = 1.2
RING_H = 3.0
FOAM = 0.5
PRESS_D = 24.0

# the 1 x 18650 holder, measured with calipers: 76 x 21, 20.35 high with
# the cell in it; an M3 hole in the middle of its base
HOLDER_L, HOLDER_W, HOLDER_H = 76.0, 21.0, 20.35
HOLDER_CLR = 0.3
CELL_D = 18.3
PIN_D, PIN_H = 2.8, 1.0          # through the base's hole; ASSUMED base >= 1 mm
RIDGE_T, RIDGE_H = 1.2, 2.0      # the outline round its foot
STOP_H = 10.0                    # the stop wall at its lower end, wires pass above
SADDLE_DEEP = 3.0                # the front's ribs reach this far down the cell's top

GRILLE_HOLE, GRILLE_PITCH, GRILLE_EDGE = 3.2, 4.4, 3.6
DIMPLE = 1.0
GROOVE_W, GROOVE_D = 0.8, 0.5

# screws: M3 self-tapping into plastic
M3_CLEAR, M3_PILOT, M3_HEAD_D, M3_HEAD_H = 3.4, 2.6, 6.2, 3.0
POST_D, STANDOFF_D, PILOT_SKIN = 6.0, 6.5, 1.0

# the lever
LEV_GAP = 1.0       # right wall's outside to the lever's inner face
LEV_T = 4.5         # thick at the pivot
LEV_FLARE = 4.0     # more at the paddle
PIV_R = COL_R = 5.0           # the hinge and the anchor, the same diameter
TUBE_D, LEV_HOLE = 6.0, 6.4    # the pivot tube, the lever's bore on it
ANCHOR_Z0 = D / 2               # the hinge below (lever), the anchor above (front)
PAN_D, PAN_H = 5.5, 2.4         # M3 Torx pan head (ISO 14583), flush in the face
TUBE_TOP = D - PAN_H            # the lever's tube ends under the head
CB_D, CB_DEEP = 6.2, PAN_H + 0.1   # the head's counterbore, its floor clear of the head
BAR_W = 3.0         # the bar onto the plunger, across
BAR_BOT = 0.0       # its foot: flush with the lever's back face, on the bed
CLAW_Y, CLAW_X = 4.0, 1.6        # the claw: each way along the wall, in from the wall
CLAW_Z0, CLAW_Z1 = 0.0, 8.0
CLAW_CLR, CLAW_IN = 0.3, 0.3       # round it in the floor's cut; inward travel more
BAR_TOP = 0.25      # its top this far under the board's back face
BAR_CLR = 0.15      # round it in its slot
PRELOAD = 0.1       # the bar presses the plunger this much at rest
LEV_CLR = 0.4

# ------------------------------------------------------------ derived
SPK_R = SPK_D / 2 + SPK_CLR
Z_FACE_IN = D - FACE
Z_BF = Z_FACE_IN - FRONT_H
Z_BB = Z_BF - BT
Z_SPLIT = Z_BB


def z_back(up):
    return Z_BB - up


HX0 = WALL + HOLDER_CLR
HX1 = HX0 + HOLDER_W
OW = math.ceil((HX1 + 0.4 + 2 * SPK_R + WALL) * 10) / 10
IW = OW - 2 * WALL


def bx(u):
    return OW - WALL - CLR - BW + u


def by(v):
    return WALL + CLR + BH - v


HY0 = by(NOTCH_V) + 0.5
HY1 = HY0 + HOLDER_L
OH = math.ceil((HY1 + HOLDER_CLR + WALL) * 10) / 10
IH = OH - 2 * WALL
H_TOP = BACK + HOLDER_H
HCX, HCY = (HX0 + HX1) / 2, (HY0 + HY1) / 2
CELL_R = CELL_D / 2
CELL_Z = H_TOP - CELL_R

SPK_X = OW - WALL - SPK_R
SPK_Y = OH - WALL - SPK_R

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
_BY_CELL = between_holes(HX1, OH - WALL - 3.2,
                         lambda x, y: _in_cavity(x, y) and _clear_of_speaker(x, y)
                         and x - STANDOFF_D / 2 >= HX1 + HOLDER_CLR + 0.2)

# the screws: (x, y, through the board)
SCREWS = [(bx(u), by(v), True) for (u, v) in HOLES] + [
    _TOP_RIGHT + (False,),
    ((WALL + bx(0) - CLR) / 2, (WALL + HY0) / 2, False),
    # top, right of the holder: the cell pushes the halves apart there
    _BY_CELL + (False,)]

LEV_IN = OW + LEV_GAP
PIV = (OW + 5.5, OH - 5.0)
LEV_END = PTT_Y - 12.0

# ------------------------------------------------------------ checks
assert bx(0) >= WALL + CLR - 1e-9
assert bx(NOTCH_U) >= HX1 + 0.3, "the holder meets the board"
assert HY0 >= by(NOTCH_V) + 0.3, "the holder meets the board's notch"
assert SPK_X - SPK_R >= HX1 + 0.3, "the holder meets the speaker"
assert H_TOP <= Z_FACE_IN - 0.3, "the holder reaches the face"
assert SPK_Y - SPK_R >= by(0) + 1, "the speaker meets the board"
assert KEY_TOP - FRONT_H <= FACE
assert abs(WALL - PORT_RECESS - PORT_WALL) < 1e-9
assert PTT_Z + PTT_PLUNGER / 2 <= Z_BB - BAR_TOP, "the bar misses the plunger's top"
assert PAN_D <= TUBE_D, "the head overhangs the tube onto the anchor"
assert LEV_END > by(JACK_V) + JACK_HOLE / 2 + 5, "the lever covers the jack"
assert math.hypot(SCREWS[2][0] - SPK_X, SCREWS[2][1] - SPK_Y) >= SPK_R + RIB + POST_D / 2
assert math.hypot(SCREWS[4][0] - SPK_X, SCREWS[4][1] - SPK_Y) >= SPK_R + RIB + POST_D / 2, \
    "the top screw meets the speaker's ring"
assert SCREWS[3][0] - STANDOFF_D / 2 >= WALL and SCREWS[3][0] + STANDOFF_D / 2 <= bx(0) - CLR


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
    jy, ux, kx = by(JACK_V), bx(USB_U), bx(KNOB_U)
    bw = BAR_W / 2 + BAR_CLR
    top = Z_SPLIT + 0.01
    return [
        (box(xr0, xr1, PTT_Y - bw, PTT_Y + bw, -1, Z_SPLIT),
         box(xr0, xr1, PTT_Y - bw, PTT_Y + bw, -1, top)),
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
    ux, zu = bx(USB_U), z_back(USB_UP)
    return (cyl_x(OW - PORT_RECESS, OW + 1, jy, zj, JACK_RECESS_D / 2)
            + hole_y(-1, PORT_RECESS, ux, zu, *USB_RECESS))


def cell_round(extra=0.0):
    return cyl_y(HCX, HY0 - 5, HY1 + 5, CELL_Z, CELL_R + extra)


def holder_box(grow=0.0):
    return box(HX0 - grow, HX1 + grow, HY0 - grow, HY1 + grow, BACK - 0.01, H_TOP + grow)




def grille_xy():
    x0 = G_M + GRILLE_HOLE / 2
    return [(x0 + i * GRILLE_PITCH, G_Y0 + j * G_PY) for i in range(G_NX) for j in range(G_NY)]


# ---------------------------------------------------------- front shell
def front_shell():
    ff = Z_FACE_IN
    out = outside(Z_SPLIT, D)
    out = fillet(out.faces().sort_by(Axis.Z)[-1].edges(), FACE_R)
    s = out - inside(Z_SPLIT - 1, ff)

    # the speaker's ring, clear of the holder
    ring = (cyl_z(SPK_X, SPK_Y, SPK_R + RIB, ff - RING_H, ff + 0.5)
            - box(-1, HX1 + 0.3, -1, OH + 1, 0, D))
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
    for y in (HY0 + 14.0, HY1 - 14.0):
        rib = box(HX0 + 0.5, HX1 - 0.5, y - RIB / 2, y + RIB / 2, H_TOP - SADDLE_DEEP, ff + 0.5)
        s += (rib - cell_round(0.1)) & inside(Z_SPLIT - 10, D, 0.5)

    # ---- cuts
    for (x, y, board) in SCREWS:
        s -= cyl_z(x, y, M3_PILOT / 2, Z_SPLIT - 6, D - PILOT_SKIN)
    s -= cyl_z(mx, my, TUBE_ID / 2, Z_BF, ff + 0.01)
    s -= cyl_z(mx, my, MIC_HOLE / 2, ff - 0.1, D + 1)

    open_r = SPK_D / 2 - 3.0
    hr = GRILLE_HOLE / 2
    for (x, y) in grille_xy():
        if (x - SPK_X) ** 2 + (y - SPK_Y) ** 2 <= open_r ** 2:
            s -= cyl_z(x, y, hr, ff - 1, D + 1)
        else:
            s -= cyl_z(x, y, hr, D - DIMPLE, D + 1)
    s -= box(-1, OW + 1, LINE_Y - GROOVE_W / 2, LINE_Y + GROOVE_W / 2, D - GROOVE_D, D + 1)

    x0, x1, v0, v1 = OLED_WIN
    s -= box(bx(x0), bx(x1), by(v1), by(v0), ff - 1, D + 1)
    pocket = Z_BF + OLED_TOP - ff + 0.2
    x0, x1, v0, v1 = OLED_GLASS
    s -= box(bx(x0) - CLR, bx(x1) + CLR, by(v1) - CLR, by(v0) + CLR, ff - 1, ff + pocket)
    for (u, v) in KEYS:
        x, y = bx(u), by(v)
        s -= cyl_z(x, y, KEY_HOLE / 2, ff - 1, D + 1)
        s -= cone_z(x, y, KEY_HOLE / 2, DISH_D / 2, D - DISH_DEEP, D)
        s -= cyl_z(x, y, DISH_D / 2, D, D + 1)

    for hole, _ in ports():
        s -= hole
    s -= recesses()
    # the hinge's anchor: joined to the wall above the seam, clear of the
    # back's wall below it; its tube down through the lever's bore
    s += column(0, Z_SPLIT, D) + column(0, ANCHOR_Z0, Z_SPLIT + 0.01, OW + 0.3)
    s -= cyl_z(PIV[0], PIV[1], LEV_HOLE / 2, ANCHOR_Z0 - 1, D + 1)
    s -= cyl_z(PIV[0], PIV[1], CB_D / 2, D - CB_DEEP, D + 1)
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

    # the holder: a pin through its base, an outline ridge round its foot,
    # a stop wall at its lower end
    s += cyl_z(HCX, HCY, PIN_D / 2, floor, BACK + PIN_H)
    g = HOLDER_CLR
    ridge = (box(HX0 - g - RIDGE_T, HX1 + g + RIDGE_T, HY0 - g - RIDGE_T, HY1 + g + RIDGE_T,
                 floor, BACK + RIDGE_H)
             - box(HX0 - g, HX1 + g, HY0 - g, HY1 + g, 0, D))
    stop = box(HX0 - g - RIDGE_T, HX1 + g + RIDGE_T, HY0 - g - RIDGE_T, HY0 - g, floor, BACK + STOP_H)
    s += (ridge + stop) & inside(0, D, 0.5)

    # the ring that presses the speaker's magnet, through a foam pad
    z_mag = Z_FACE_IN - SPK_DEEP - FOAM
    s += (cyl_z(SPK_X, SPK_Y, PRESS_D / 2, floor, z_mag)
          - cyl_z(SPK_X, SPK_Y, PRESS_D / 2 - RIB, floor + 0.6, z_mag + 1))


    # ---- cuts
    for (x, y, board) in SCREWS:
        s -= cyl_z(x, y, M3_CLEAR / 2, -1, D)
        s -= head_pocket(x, y)
    for hole, notch in ports():
        s -= hole + notch
    s -= recesses()
    # the floor cut for the claw, open to the back face and the slot
    s -= box(OW - WALL - CLAW_X - CLAW_CLR - CLAW_IN, OW - WALL + 0.01,
             PTT_Y - CLAW_Y - CLAW_CLR, PTT_Y + CLAW_Y + CLAW_CLR, -1, CLAW_Z1 + 0.5)
    return s


# --------------------------------------------------------------- lever
def lever():
    L, T, F = LEV_IN, LEV_T, LEV_FLARE
    y_f0, y_f1 = PTT_Y + 8.0, PTT_Y - 2.0
    body = prism_xy([(L, PIV[1]), (L + T, PIV[1]), (L + T, y_f0), (L + T + F, y_f1),
                     (L + T + F, LEV_END), (L, LEV_END)], 0, D)
    boss = cyl_z(PIV[0], PIV[1], PIV_R, 0, D)
    s = body + boss
    try:
        s = fillet(s.edges().filter_by(Axis.Z), 0.8)
    except Exception:
        pass
    # the bar through its slot onto the plunger
    x0 = PTT_X - PRELOAD
    bar = box(x0, L + 0.5, PTT_Y - BAR_W / 2, PTT_Y + BAR_W / 2,
              BAR_BOT, Z_BB - BAR_TOP)
    # the claw behind the wall at its foot: it rests on the wall's inside
    claw = box(OW - WALL - CLAW_X, OW - WALL, PTT_Y - CLAW_Y, PTT_Y + CLAW_Y, CLAW_Z0, CLAW_Z1)
    s += bar + claw
    # the hinge stops under the anchor and turns on its tube
    s -= column(LEV_CLR, ANCHOR_Z0 - LEV_CLR, D + 1)
    s -= cyl_z(PIV[0], PIV[1], PIV_R + 1, ANCHOR_Z0 - LEV_CLR, D + 1)
    # the tube up through the anchor; the screw's pilot down it
    s += cyl_z(PIV[0], PIV[1], TUBE_D / 2, ANCHOR_Z0 - LEV_CLR - 0.1, TUBE_TOP)
    s -= cyl_z(PIV[0], PIV[1], M3_PILOT / 2, 1.0, D)
    return s


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


def check(front, back, lev):
    for name, p in (("front", front), ("back", back), ("lever", lev)):
        print(f"{name}: {len(p.solids())} solid(s)")
    shells = [(front, "front"), (back, "back"), (lev, "lever")]
    things = [(p, n) for p, n, _ in parts()]
    bad = 0
    seen = set()
    for a in shells + things:
        for b in shells + things:
            k = tuple(sorted((a[1], b[1])))
            if a[1] == b[1] or k in seen or (a in things and b in things):
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
          f"({OW + LEV_GAP + LEV_T + LEV_FLARE:.1f} wide with the lever); "
          f"seam {D - Z_SPLIT:.1f} from the face")
    front = front_shell()
    back = back_tray()
    lev = lever()
    if "check" in sys.argv:
        check(front, back, lev)
        return
    # to print: the front face down, the back and the lever on their backs
    export_stl(Pos(0, 0, D) * Rot(180, 0, 0) * front, os.path.join(OUT, "front.stl"))
    export_stl(back, os.path.join(OUT, "back.stl"))
    export_stl(lev, os.path.join(OUT, "lever.stl"))
    print(f"wrote {OUT}/front.stl back.stl lever.stl")
    # one .glb per piece for viewer.html, which colours, outlines and
    # explodes them
    import json
    pieces = [(front, "front", SHELL), (back, "back", SHELL), (lev, "lever", LEVER)] + parts()
    os.makedirs(os.path.join(OUT, "pieces"), exist_ok=True)
    for shape, name, rgb in pieces:
        glb([(shape, name, rgb)], os.path.join(OUT, "pieces", f"{name}.glb"))
    with open(os.path.join(OUT, "pieces", "list.json"), "w") as f:
        json.dump([n for _, n, _ in pieces], f)

if __name__ == "__main__":
    main()
