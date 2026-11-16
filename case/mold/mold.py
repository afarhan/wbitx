#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Ashhar Farhan
"""
mold.py — the wBitx case for injection molding, around the production
wificomms board (pcb/wificomms), not the perf-board prototype of case.py.

Seen from the front (the display and keys, the board's B side), the
production board is the prototype's mirror: the cell's notch is top-left,
so the 18650 runs down the left side and the 50 mm speaker sits top-right.
The board's back (F side) carries the PTT (Alps SKRTLAE010) and the 3.5 mm
jack, both out of the right wall, the SuperMini's USB-C and the power
slide switch (K3-1296S-E1) out of the bottom wall. The mic is a bottom-port
MEMS (LMA2718): a tube from the front wall meets the board round its port,
for a 0.5 mm foam gasket. Positions come from wificomms.kicad_pcb.

Moulding: PC/ABS, 1.8 mm walls; draft 1.5 deg on the outside (for a VDI 24
texture), 1 deg inside, 0.5 deg on ribs, standoffs and tubes; ribs on the
face 1.0 mm (0.55 of the wall) so they don't sink. The parting line is the
seam; the front's skirt runs into a rebate in the back.

The halves close on five snap hooks on the back, each with a slot through
the back plate under its lip: the slot lets the tool form the lip without a
lifter, and a pick pushed in through it releases the hook. Their catches
are pockets in the front's inner walls: five small lifters in the front's
core. The board is held to the front by two thread-forming screws through
H1 and H2 into standoffs. The ports are notches in the front's side walls
open to the seam (shut-offs, no side actions), filled by tongues on the
back. The PTT's keytop is a third small part, held in a counterbore.

    make mold               mold/*.step and the viewer's .glb, in this folder
    python mold.py check    the overlap check
"""
import math
import os
import re
import sys

from build123d import (Align, Axis, Box, Color, Compound, Cone, Cylinder, Plane,
                       Polygon, Pos, Rectangle, RectangleRounded, Rot, export_gltf, export_step,
                       extrude, fillet)

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = HERE
PCB = os.path.join(HERE, "..", "..", "pcb", "wificomms", "wificomms.kicad_pcb")

# --------------------------------------------------------------- molding
WALL = 1.8          # nominal wall, PC/ABS
RIB = 1.0           # ribs and plates standing on the face: 0.55 of the wall
DRAFT_OUT = 1.5     # degrees, the outside, textured
DRAFT_IN = 1.0      # the inside walls
DRAFT_RIB = 0.5     # ribs, standoffs, tubes, notches' sides get DRAFT_IN
FACE_R = 1.0        # the front face's and the back's outer edges
INNER_R = 0.5       # inside, where the walls meet the face and the back
CORNER = 2.6        # the outside's corners seen from the front
IN_CORNER = 0.8     # the inside's: CORNER - WALL, so the corners keep the wall
CLR = 0.4           # board to the walls (the inside draft takes 0.2 of it)

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
    """Reference -> (x, y, rotation) from the board file."""
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
    """A footprint's point, offset in its own frame, as board u, v."""
    x, y, r = FP[ref]
    a = math.radians(r)
    ox = dx * math.cos(a) + dy * math.sin(a)
    oy = -dx * math.sin(a) + dy * math.cos(a)
    return KX1 - (x + ox), (y + oy) - KY0


KEYS = [uv(r) for r in ("SW1", "SW2", "SW3", "SW4")]
KEY_TOP, KEY_BODY = 4.3, 2.6          # B3FS-1010P
KEY_HOLE, DISH_D, DISH_DEEP = 3.5, 6.0, 1.0
MIC_UV = uv("MK2", 0.0, 0.5)          # the bottom port's hole
MIC_HOLE, TUBE_OD, TUBE_ID, GASKET = 1.0, 3.0, 1.2, 0.5
STANDOFFS = [uv("H1"), uv("H2")]
STANDOFF_D, PILOT_D, PILOT_SKIN = 5.0, 2.5, 1.0

# the display, soldered by wires, its glass 3.0 above the board; sizes as
# measured on the prototype's module, placed by its four pins
OLED_TOP = 3.0
_du, _dv = uv("DS1")
_pin_v = _dv + 12.3
OLED_BOTTOM = _pin_v + 1.5
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
JACK_UP, JACK_HOLE = 2.5, 6.5        # PJ320D, its nose at the board edge
USB_U = uv("U1")[0]
USB_UP, USB_W, USB_H = 2.6, 9.5, 4.2  # SuperMini flat: 1.0 board + half the USB-C
KNOB_U = uv("SW_POWER1")[0]
KNOB_UP, KNOB_W, KNOB_H = 0.75, 4.6, 2.2   # the slot the knob slides in
BACK_H = 6.0        # the tallest on the back (the jack), for the seam

# the PTT's keytop, the third part
STEM_D, STEM_HOLE, STEM_PROUD = 3.0, 3.4, 1.2
FLANGE_D, FLANGE_T, CB_D, CB_DEEP = 5.0, 0.6, 5.4, 0.8
NUB_D = 1.6

# ---------------------------------------------------------- speaker, cell
SPK_D, SPK_DEEP, SPK_MAGNET = 50.0, 15.0, 30.0
SPK_RIM, SPK_CONE = 2.6, 7.5
SPK_CLR = 0.3
RING_H = 3.0
SINK = 0.6          # the speaker's rim into the right and top walls
FOAM = 0.5          # a foam pad between the magnet and the back's ring
PRESS_D = 24.0      # the back's ring that presses the magnet

CELL_D, CELL_L = 18.3, 65.2
CELL_CLR, CELL_TOP_CLR = 0.2, 0.4

GRILLE_HOLE, GRILLE_PITCH, GRILLE_EDGE = 3.2, 4.4, 3.6
DIMPLE = 1.0        # blind grille holes off the cone; 0.8 left under them
GROOVE_W, GROOVE_D = 0.8, 0.5

# the seam: the front's skirt runs into the back's rebate
LIP_H, LIP_T, LIP_GAP = 1.5, 0.9, 0.15

# snap hooks on the back, catches in the front
HOOK_W, HOOK_T, HOOK_GAP = 5.0, 1.0, 0.3   # beam across, thick, off the wall
HOOK_Y = 0.5        # the lip past the wall's inside: the deflection
HOOK_LAND = 0.3     # the lip's flat before its 45 degree lead-in
CATCH_UP = 0.85     # the lip's catching face, past the seam into the front
POCKET_DEEP, POCKET_CLR = 0.7, 0.2

# ------------------------------------------------------------ derived
SPK_R = SPK_D / 2 + SPK_CLR
RING_R = SPK_R + RIB
CELL_R = CELL_D / 2
CELL_Z = WALL + CELL_CLR + CELL_R


def spk_r(depth):
    """The speaker's radius with its slip, this far in from the face's inside."""
    if depth < 0 or depth > SPK_DEEP:
        return 0.0
    if depth <= SPK_RIM:
        return SPK_R
    if depth <= SPK_CONE:
        t = (depth - SPK_RIM) / (SPK_CONE - SPK_RIM)
        return SPK_R - t * (SPK_D - SPK_MAGNET) / 2
    return SPK_MAGNET / 2 + SPK_CLR


def cell_gap():
    """The least distance between the speaker's axis and the cell's."""
    r = CELL_R + CELL_CLR
    need, n = 0.0, 4000
    for i in range(n + 1):
        zz = WALL + (D - 2 * WALL) * i / n
        h = r * r - (zz - CELL_Z) ** 2
        if h > 0:
            need = max(need, spk_r(D - WALL - zz) + h ** 0.5)
    return need


OW = math.ceil(max(BW + 2 * CLR + 2 * WALL,
                   WALL + CELL_CLR + CELL_R + cell_gap() + SPK_R - SINK + WALL) * 10) / 10
IW = OW - 2 * WALL
IH = BH + 2 * CLR + 2 * SPK_R - SINK
OH = round(IH + 2 * WALL, 2)

# the board against the right wall (the PTT and jack are there), the cell
# against the left
def bx(u):
    return OW - WALL - CLR - BW + u


def by(v):
    return WALL + CLR + BH - v


SPK_X = OW - WALL + SINK - SPK_R
SPK_Y = OH - WALL + SINK - SPK_R
CELL_X = WALL + CELL_CLR + CELL_R
CELL_Y1 = OH - WALL - CELL_TOP_CLR
CELL_Y0 = CELL_Y1 - CELL_L
END_Y1 = CELL_Y0 - CELL_CLR
END_Y0 = END_Y1 - RIB
BAY_X1 = CELL_X + CELL_R + CELL_CLR

SPLIT = WALL + max(SPK_DEEP + FOAM, FRONT_H + BT + BACK_H) + 0.5
Z_SPLIT = D - SPLIT
Z_FACE_IN = D - WALL                  # the face's inside
Z_BF = Z_FACE_IN - FRONT_H            # the board's front face
Z_BB = Z_BF - BT                      # its back face


def z_back(up):
    """Height above the board's back face, as frame z."""
    return Z_BB - up


# grille, as case.py: the line sits over the window as far below as the
# bottom key's dish sits over the bottom edge
G_BELOW = by(max(v for _, v in KEYS)) - DISH_D / 2
LINE_Y = by(OLED_WIN[2]) + G_BELOW + GROOVE_W / 2
G_NX = int((OW - 2 * GRILLE_EDGE - GRILLE_HOLE) // GRILLE_PITCH) + 1
G_M = (OW - (G_NX - 1) * GRILLE_PITCH - GRILLE_HOLE) / 2
G_Y0 = LINE_Y + GROOVE_W / 2 + G_M + GRILLE_HOLE / 2
G_Y1 = OH - G_M - GRILLE_HOLE / 2
G_NY = int(round((G_Y1 - G_Y0) / GRILLE_PITCH)) + 1
G_PY = (G_Y1 - G_Y0) / (G_NY - 1)

# the snap hooks: (wall, frame coordinate along it)
SNAPS = [("top", OW / 2 + 4.0),
         ("left", (by(STANDOFFS[1][1]) + END_Y0) / 2),
         ("right", SPK_Y),
         ("right", (by(PTT_V) + by(JACK_V)) / 2),
         ("bottom", (bx(STANDOFFS[1][0]) + bx(USB_U) - USB_W / 2) / 2)]

# ------------------------------------------------------------ checks
assert bx(0) >= WALL + CLR - 1e-9, "the board meets the left wall"
assert bx(NOTCH_U) >= BAY_X1 + 0.2, "the cell meets the board's notch"
assert END_Y0 >= by(NOTCH_V) + 0.3, "the cell's end wall meets the board"
assert SPK_X - CELL_X >= cell_gap() - 1e-6, "the cell meets the speaker"
assert WALL + SPK_DEEP + FOAM <= SPLIT, "the speaker crosses the seam"
assert KEY_TOP - FRONT_H > 0, "the plungers don't reach the face"
assert KEY_TOP - FRONT_H <= WALL, "the plungers stand past the face"
assert KEY_BODY <= FRONT_H - 0.2 + 1e-9, "the keys' bodies meet the face"
assert OLED_TOP - FRONT_H + 0.2 <= WALL - 1.0, "the glass pocket leaves a thin wall"
assert by(OLED_PCB[2]) <= SPK_Y - SPK_R, "the display meets the speaker's rim"
for (u, v) in STANDOFFS:
    for (ku, kv) in KEYS:
        assert math.hypot(u - ku, v - kv) >= STANDOFF_D / 2 + 3.2, "a standoff crowds a key"
for w, c in SNAPS:
    if w == "left":
        assert c + HOOK_W / 2 + 1 < END_Y0, "the left hook meets the cell"
    if w == "top":
        assert c - HOOK_W / 2 - 1 > BAY_X1, "the top hook meets the cell"


# ------------------------------------------------------------ helpers
def t(deg):
    return math.tan(math.radians(deg))


def box(x0, x1, y0, y1, z0, z1):
    return Pos(min(x0, x1), min(y0, y1), min(z0, z1)) * Box(
        abs(x1 - x0), abs(y1 - y0), abs(z1 - z0), align=(Align.MIN,) * 3)


def cyl_z(cx, cy, r, z0, z1):
    return Pos(cx, cy, min(z0, z1)) * Cylinder(
        r, abs(z1 - z0), align=(Align.CENTER, Align.CENTER, Align.MIN))


def cone_z(cx, cy, r0, r1, z0, z1):
    """Radius r0 at z0 to r1 at z1."""
    if z0 > z1:
        z0, z1, r0, r1 = z1, z0, r1, r0
    return Pos(cx, cy, z0) * Cone(r0, r1, z1 - z0,
                                  align=(Align.CENTER, Align.CENTER, Align.MIN))


def dcyl(cx, cy, r, z_root, z_tip, deg=DRAFT_RIB):
    """A drafted post: r at its root, narrower toward its tip."""
    return cone_z(cx, cy, r, r - abs(z_tip - z_root) * t(deg), z_root, z_tip)


def cyl_x(x0, x1, cy, cz, r):
    return Pos(min(x0, x1), cy, cz) * Rot(0, 90, 0) * Cylinder(
        r, abs(x1 - x0), align=(Align.CENTER, Align.CENTER, Align.MIN))


def cyl_y(cx, y0, y1, cz, r):
    return Pos(cx, min(y0, y1), cz) * Rot(-90, 0, 0) * Cylinder(
        r, abs(y1 - y0), align=(Align.CENTER, Align.CENTER, Align.MIN))


def prism(x0, y0, w, h, r, z_ref, z_end, deg):
    """A rounded rectangle, w x h at z_ref, run to z_end, narrowing by deg
    away from z_ref (negative deg widens)."""
    up = z_end > z_ref
    pl = Plane(origin=(x0 + w / 2, y0 + h / 2, z_ref), x_dir=(1, 0, 0),
               z_dir=(0, 0, 1 if up else -1))
    return extrude(pl * RectangleRounded(w, h, r), amount=abs(z_end - z_ref), taper=deg)


def slab(x0, x1, y0, y1, z_root, z_tip, deg=DRAFT_RIB):
    """A rib from z_root to z_tip, its faces drafted, narrower at the tip."""
    up = z_tip > z_root
    w, h = x1 - x0, y1 - y0
    pl = Plane(origin=((x0 + x1) / 2, (y0 + y1) / 2, z_root), x_dir=(1, 0, 0),
               z_dir=(0, 0, 1 if up else -1))
    return extrude(pl * Rectangle(w, h), amount=abs(z_tip - z_root), taper=deg)


def outside(z0, z1):
    """The case's outside from the seam toward the face or the back."""
    return prism(0, 0, OW, OH, CORNER, z0, z1, DRAFT_OUT)


def inside(z0, z1, grow=0.0, deg=DRAFT_IN):
    return prism(WALL - grow, WALL - grow, IW + 2 * grow, IH + 2 * grow,
                 max(IN_CORNER + grow, 0.2), z0, z1, deg)


NORMAL = {"left": (-1, 0, 0), "right": (1, 0, 0), "bottom": (0, -1, 0), "top": (0, 1, 0)}


def wall_plane(wall, offset=0.0):
    """A plane on a wall's inside (offset outward), z along the outward
    normal, y along frame z, x along the wall. Returns it and the sign that
    turns a frame coordinate along the wall into the plane's x."""
    from build123d import Vector
    n = Vector(*NORMAL[wall])
    x = Vector(0, 0, 1).cross(n)
    if wall in ("left", "right"):
        o = Vector(WALL if wall == "left" else OW - WALL, 0, 0)
        sign = x.Y
    else:
        o = Vector(0, WALL if wall == "bottom" else OH - WALL, 0)
        sign = x.X
    return Plane(origin=o + n * offset, x_dir=x, z_dir=n), sign


def wall_prism(wall, pts, n0, n1):
    """A profile in (frame coordinate along the wall, frame z), run along the
    outward normal from n0 to n1 past the wall's inside."""
    pl, sign = wall_plane(wall, n0)
    return extrude(pl * Polygon(*[(sign * a, zz) for a, zz in pts], align=None),
                   amount=n1 - n0, dir=pl.z_dir)


def notch(wall, c, zc, half, extra=0.0):
    """A port's notch through a side wall, from its centre down past the
    seam, its sides drafted for the shut-off."""
    zb = Z_SPLIT - LIP_H - 1.0
    h0 = half + extra
    h1 = h0 + (zc - zb) * t(DRAFT_IN)
    pts = [(c - h0, zc), (c + h0, zc), (c + h1, zb), (c - h1, zb)]
    return wall_prism(wall, pts, -1.0, WALL + 1.0)


def hole_x(x0, x1, cy, cz, w, h):
    """An obround along x: w along y, h along z."""
    r = min(w, h) / 2
    return (cyl_x(x0, x1, cy - (w / 2 - r), cz, r) + cyl_x(x0, x1, cy + (w / 2 - r), cz, r)
            + box(x0, x1, cy - (w / 2 - r), cy + (w / 2 - r), cz - r, cz + r))


def hole_y(y0, y1, cx, cz, w, h):
    r = min(w, h) / 2
    return (cyl_y(cx - (w / 2 - r), y0, y1, cz, r) + cyl_y(cx + (w / 2 - r), y0, y1, cz, r)
            + box(cx - (w / 2 - r), cx + (w / 2 - r), y0, y1, cz - r, cz + r))


def cell_round(extra=0.0):
    return cyl_y(CELL_X, CELL_Y0 - 5, CELL_Y1 + 5, CELL_Z, CELL_R + CELL_CLR + extra)


# the ports: (wall, centre along it, z, the hole, half the notch's width)
def ports():
    zp, zj = z_back(PTT_UP), z_back(JACK_UP)
    zu, zk = z_back(USB_UP), z_back(KNOB_UP)
    xr0, xr1 = OW - WALL - 1, OW + 1
    yb0, yb1 = -1, WALL + 1
    return [
        ("right", by(PTT_V), zp, cyl_x(xr0, xr1, by(PTT_V), zp, STEM_HOLE / 2), STEM_HOLE / 2),
        ("right", by(JACK_V), zj, cyl_x(xr0, xr1, by(JACK_V), zj, JACK_HOLE / 2), JACK_HOLE / 2),
        ("bottom", bx(USB_U), zu, hole_y(yb0, yb1, bx(USB_U), zu, USB_W, USB_H), USB_W / 2),
        ("bottom", bx(KNOB_U), zk, hole_y(yb0, yb1, bx(KNOB_U), zk, KNOB_W, KNOB_H), KNOB_W / 2),
    ]


def counterbore(extra=0.0):
    """The PTT keytop's flange: a counterbore in the right wall's inside,
    open to the seam like the notch."""
    zp, c = z_back(PTT_UP), by(PTT_V)
    r = CB_D / 2 + extra
    x0 = OW - WALL - 1.0
    x1 = OW - WALL + CB_DEEP
    zb = Z_SPLIT - LIP_H - 1.0
    return cyl_x(x0, x1, c, zp, r) + box(x0, x1, c - r, c + r, zb, zp)


def keytop():
    """Along +x, in place: the nub on the plunger, the flange in the
    counterbore, the stem STEM_PROUD past the wall."""
    zp, c = z_back(PTT_UP), by(PTT_V)
    tip = bx(BW)                                   # the plunger's tip
    x_flange = OW - WALL + CB_DEEP - FLANGE_T      # flange's inner face
    return (cyl_x(tip + 0.05, x_flange, c, zp, NUB_D / 2)
            + cyl_x(x_flange, x_flange + FLANGE_T, c, zp, FLANGE_D / 2)
            + cyl_x(x_flange + FLANGE_T, OW + STEM_PROUD, c, zp, STEM_D / 2))


def grille_xy():
    x0 = G_M + GRILLE_HOLE / 2
    return [(x0 + i * GRILLE_PITCH, G_Y0 + j * G_PY) for i in range(G_NX) for j in range(G_NY)]


def snap_parts(wall, c):
    """The hook (on the back), the slot under its lip through the back plate
    and up the wall (cut from the back), and its catch (cut from the front)."""
    from build123d import Vector
    z_catch = Z_SPLIT + CATCH_UP
    z_top = z_catch + HOOK_LAND + HOOK_Y + HOOK_GAP     # the lead-in meets the beam
    z_tip = z_top + 0.2
    a0, a1 = c - HOOK_W / 2, c + HOOK_W / 2
    hook = wall_prism(wall, [(a0, WALL - 0.3), (a1, WALL - 0.3), (a1, z_tip), (a0, z_tip)],
                      -(HOOK_GAP + HOOK_T), -HOOK_GAP)
    # the lip: a profile in (outward, frame z), run along the wall
    n = Vector(*NORMAL[wall])
    d = n.cross(Vector(0, 0, 1))                  # x = n, y = z, so z = n x Z
    pl0, _ = wall_plane(wall)
    e = Vector(0, 1, 0) if wall in ("left", "right") else Vector(1, 0, 0)
    start = a0 if d.dot(e) > 0 else a1
    if wall in ("left", "right"):
        origin = Vector(pl0.origin.X, start, 0)
    else:
        origin = Vector(start, pl0.origin.Y, 0)
    lp = Plane(origin=origin, x_dir=n, z_dir=d)
    lip_pts = [(-HOOK_GAP - 0.2, z_catch), (HOOK_Y, z_catch), (HOOK_Y, z_catch + HOOK_LAND),
               (-HOOK_GAP, z_top), (-HOOK_GAP - 0.2, z_top)]
    lip = extrude(lp * Polygon(*lip_pts, align=None), amount=HOOK_W, dir=lp.z_dir)
    slot = (wall_prism(wall, [(a0 - 0.2, -1), (a1 + 0.2, -1), (a1 + 0.2, WALL + 0.01),
                              (a0 - 0.2, WALL + 0.01)], -HOOK_GAP, HOOK_Y + 0.1)
            + wall_prism(wall, [(a0 - 0.2, WALL - 0.01), (a1 + 0.2, WALL - 0.01),
                                (a1 + 0.2, Z_SPLIT + 0.01), (a0 - 0.2, Z_SPLIT + 0.01)],
                         -0.01, HOOK_Y + 0.1))
    pocket = wall_prism(wall, [(a0 - POCKET_CLR, z_catch - 0.05), (a1 + POCKET_CLR, z_catch - 0.05),
                               (a1 + POCKET_CLR, z_tip + POCKET_CLR), (a0 - POCKET_CLR, z_tip + POCKET_CLR)],
                        -0.3, POCKET_DEEP)
    return hook + lip, slot, pocket


# ---------------------------------------------------------- front shell
def front_shell():
    out = outside(Z_SPLIT, D)
    out = fillet(out.faces().sort_by(Axis.Z)[-1].edges(), FACE_R)
    cav = inside(Z_SPLIT, Z_FACE_IN)
    cav = fillet(cav.faces().sort_by(Axis.Z)[-1].edges(), INNER_R)
    s = out - cav - inside(Z_SPLIT - 5, Z_SPLIT + 0.01, 0, 0)

    # the skirt into the back's rebate: its outside narrows toward its edge,
    # its inside carries the walls' draft on
    skirt = (prism(WALL - LIP_T, WALL - LIP_T, IW + 2 * LIP_T, IH + 2 * LIP_T,
                   IN_CORNER + LIP_T, Z_SPLIT, Z_SPLIT - LIP_H, DRAFT_IN)
             - inside(Z_SPLIT + 0.01, Z_SPLIT - LIP_H - 0.5, 0, -DRAFT_IN))
    s += skirt
    # the skirt stands aside for the cell
    s -= cyl_y(CELL_X, CELL_Y0 - CELL_CLR, CELL_Y1 + CELL_CLR, CELL_Z, CELL_R + CELL_CLR) \
        & box(-1, OW + 1, -1, OH + 1, Z_SPLIT - LIP_H - 1, Z_SPLIT + 0.01)

    # the speaker's ring, short of the display under it
    ff = Z_FACE_IN
    ring = (cone_z(SPK_X, SPK_Y, RING_R, RING_R - RING_H * t(DRAFT_RIB), ff + 0.5, ff - RING_H)
            - box(bx(OLED_PCB[0]) - CLR, bx(OLED_PCB[1]) + CLR, 0, by(OLED_PCB[2]) + CLR,
                  ff - RING_H - 1, D))
    s += (ring - cell_round(0.3)) & inside(Z_SPLIT, D, 0.5, 0)
    s -= (cyl_z(SPK_X, SPK_Y, SPK_R, ff - SPK_RIM, ff)
          + cone_z(SPK_X, SPK_Y, SPK_R, spk_r(SPK_CONE), ff - SPK_RIM, ff - SPK_CONE)
          + cyl_z(SPK_X, SPK_Y, SPK_R, ff - RING_H - 1, ff - SPK_RIM))

    # the board's standoffs and the mic's tube
    for (u, v) in STANDOFFS:
        s += dcyl(bx(u), by(v), STANDOFF_D / 2, ff + 0.5, Z_BF)
    mx, my = bx(MIC_UV[0]), by(MIC_UV[1])
    s += dcyl(mx, my, TUBE_OD / 2, ff + 0.5, Z_BF + GASKET)

    # over the cell: the end wall and two saddles
    xr = OW - WALL - 0.5
    x_end0 = max(WALL - 0.5, 0)
    s += slab(x_end0, BAY_X1, END_Y0, END_Y1, ff + 0.5, Z_SPLIT) \
        & inside(Z_SPLIT, D, 0.4, 0)
    for y in (END_Y1 + 12.0, CELL_Y1 - 4.0):
        sad = slab(WALL - 0.5, BAY_X1 - 4.0, y - RIB / 2, y + RIB / 2, ff + 0.5, CELL_Z) - cell_round()
        # the top saddle keeps off the speaker's ring
        sad -= cyl_z(SPK_X, SPK_Y, RING_R + 0.5, 0, D)
        s += sad & inside(Z_SPLIT, D, 0.4, 0)

    # ---- cuts
    for (u, v) in STANDOFFS:
        s -= cyl_z(bx(u), by(v), PILOT_D / 2, Z_BF - 1, D - PILOT_SKIN)
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
        s -= Pos(x, y, D - DISH_DEEP) * Cone(KEY_HOLE / 2, DISH_D / 2, DISH_DEEP,
                                             align=(Align.CENTER, Align.CENTER, Align.MIN))
        s -= cyl_z(x, y, DISH_D / 2, D, D + 1)

    for wall, c, zc, hole, half in ports():
        s -= hole + notch(wall, c, zc, half, 0.1)
    s -= counterbore(0.1)
    for wall, c in SNAPS:
        s -= snap_parts(wall, c)[2]
    return s


# ------------------------------------------------------------ back tray
def back_tray():
    out = outside(Z_SPLIT, 0)
    out = fillet(out.faces().sort_by(Axis.Z)[0].edges(), FACE_R)
    cav = inside(Z_SPLIT, WALL)
    cav = fillet(cav.faces().sort_by(Axis.Z)[0].edges(), INNER_R)
    s = out - cav - inside(Z_SPLIT - 0.01, Z_SPLIT + 5, 0, 0)
    # the rebate the front's skirt runs into
    s -= prism(WALL - LIP_T - LIP_GAP, WALL - LIP_T - LIP_GAP, IW + 2 * (LIP_T + LIP_GAP),
               IH + 2 * (LIP_T + LIP_GAP), IN_CORNER + LIP_T + LIP_GAP,
               Z_SPLIT + 0.01, Z_SPLIT - LIP_H - LIP_GAP, DRAFT_IN)

    floor = WALL - 0.5
    # what stands inside the back: into the walls low down, inside the
    # front's skirt up by the seam
    zr = Z_SPLIT - LIP_H - LIP_GAP - 0.1
    within = inside(0, zr, 0.4, 0) + inside(zr, D, -0.1, 0)
    # tongues: each notch filled back in, flush with the walls
    walls = (outside(Z_SPLIT, D) - inside(Z_SPLIT, D)) + (outside(Z_SPLIT, 0) - inside(Z_SPLIT, 0))
    zt = Z_SPLIT - LIP_H - LIP_GAP - 0.5
    for wall, c, zc, hole, half in ports():
        tg = notch(wall, c, zc, half, -0.1) & walls & box(-1, OW + 1, -1, OH + 1, zt, zc)
        s += tg - hole
    s += counterbore(-0.1) & box(-1, OW + 1, -1, OH + 1, zt, z_back(PTT_UP)) & walls
    # the keytop's flange turns in the counterbore, clear of every tongue
    s -= cyl_x(OW - WALL - 2, OW - WALL + CB_DEEP + 0.1, by(PTT_V), z_back(PTT_UP), CB_D / 2 + 0.1)

    # the cell: two cradles and the end wall
    for y in (CELL_Y0 + 10.0, CELL_Y1 - 12.0):
        s += (slab(WALL - 0.5, BAY_X1 + 1.5, y - 0.6, y + 0.6, floor, CELL_Z - 0.3)
              - cell_round()) & within
    s += slab(WALL - 0.5, BAY_X1 + 0.5, END_Y0, END_Y1, floor, Z_SPLIT - LIP_GAP) & within

    # the ring that presses the speaker's magnet, through a foam pad
    z_mag = Z_FACE_IN - SPK_DEEP - FOAM
    s += (dcyl(SPK_X, SPK_Y, PRESS_D / 2, floor, z_mag)
          - cone_z(SPK_X, SPK_Y, PRESS_D / 2 - RIB, PRESS_D / 2 - RIB + 0.1, floor + 0.6, z_mag + 0.1))

    for wall, c in SNAPS:
        hook, slot, _ = snap_parts(wall, c)
        s = s - slot + hook
    return s


# --------------------------------------------- the parts, for looking only
def parts():
    board = (box(bx(0), bx(BW), by(BH), by(0), Z_BB, Z_BF)
             - box(bx(-1), bx(NOTCH_U), by(NOTCH_V), by(-1), Z_BB - 1, Z_BF + 1))
    r = BOARD_R
    for (u, v, su, sv) in ((BW, 0, 1, 1), (0, BH, -1, -1), (BW, BH, 1, -1)):
        cx, cy = bx(u) - su * r, by(v) - sv * r
        corner = box(cx, cx + su * (r + 1), cy, cy + sv * (r + 1), Z_BB - 1, Z_BF + 1)
        board -= corner - cyl_z(cx, cy, r, Z_BB - 1, Z_BF + 1)
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
    zp, c = z_back(PTT_UP), by(PTT_V)
    ptt = (box(bx(BW) - 0.78 - 3.4, bx(BW) - 0.78, c - 2.3, c + 2.3, Z_BB - 3.3, Z_BB)
           + box(bx(BW) - 0.8, bx(BW), c - PTT_PLUNGER / 2, c + PTT_PLUNGER / 2,
                 zp - PTT_PLUNGER / 2, zp + PTT_PLUNGER / 2))
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
    cell = cyl_y(CELL_X, CELL_Y0, CELL_Y1, CELL_Z, CELL_R)
    return [(board, "board", (0.10, 0.40, 0.25)),
            (oled, "oled", (0.10, 0.25, 0.65)),
            (glass, "glass", (0.01, 0.01, 0.01)),
            (keys, "keys", (0.85, 0.85, 0.85)),
            (ptt, "ptt", (0.75, 0.75, 0.75)),
            (jack, "jack", (0.25, 0.25, 0.25)),
            (mini, "supermini", (0.20, 0.20, 0.55)),
            (slide, "switch", (0.9, 0.9, 0.9)),
            (speaker, "speaker", (0.55, 0.55, 0.55)),
            (cell, "cell", (0.70, 0.72, 0.75))]


# ---------------------------------------------------------------- output
PCABS = (0.07, 0.07, 0.08)


def whole(shape):
    return shape if hasattr(shape, "wrapped") else Compound(list(shape))


def check(front, back, key):
    for name, p in (("front", front), ("back", back), ("keytop", key)):
        print(f"{name}: {len(p.solids())} solid(s)")
    shells = [(front, "front"), (back, "back"), (key, "keytop")]
    things = [(p, n) for p, n, _ in parts()]
    bad = 0
    pairs = [(a, b) for a in shells + things for b in things if a[1] != b[1]]
    pairs += [((front, "front"), (back, "back")), ((front, "front"), (key, "keytop")),
              ((back, "back"), (key, "keytop"))]
    seen = set()
    for (a, an), (b, bn) in pairs:
        k = tuple(sorted((an, bn)))
        if k in seen:
            continue
        seen.add(k)
        i = whole(a).intersect(whole(b))
        v = sum(s.volume for s in i.solids()) if i else 0
        if v > 0.05:
            print(f"  {an} meets {bn}: {v:.2f} mm3")
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
    os.makedirs(OUT, exist_ok=True)
    print(f"box {OW:.1f} wide, {OH:.1f} tall, {D:.1f} thick; seam {SPLIT:.1f} from the face; "
          f"plungers {KEY_TOP - FRONT_H - WALL:+.1f} past the face")
    front = front_shell()
    back = back_tray()
    key = keytop()
    if "check" in sys.argv:
        check(front, back, key)
        return
    export_step(front, os.path.join(OUT, "front.step"))
    export_step(back, os.path.join(OUT, "back.step"))
    export_step(key, os.path.join(OUT, "keytop.step"))
    asm = [(front, "front", PCABS), (back, "back", PCABS), (key, "keytop", (0.8, 0.15, 0.1))]
    export_step(Compound(children=[s for s, _, _ in asm]), os.path.join(OUT, "assembly.step"))
    print(f"wrote {OUT}/front.step back.step keytop.step assembly.step")
    ex = OUT
    glb(asm, os.path.join(ex, "closed.glb"))
    gap = 35
    glb([(Pos(0, 0, -gap) * back, "back", PCABS)] + parts()
        + [(Pos(0, 0, gap) * front, "front", PCABS), (key, "keytop", (0.8, 0.15, 0.1))],
        os.path.join(ex, "exploded.glb"))
    glb([(Rot(0, 180, 0) * back, "back", PCABS)], os.path.join(ex, "back.glb"))


if __name__ == "__main__":
    main()
