#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Ashhar Farhan
"""
wearable.py — a small wearable case for the production wificomms board,
in the microphone case's look: a rounded box, the face one lattice of
dots (open over the speaker, dimples elsewhere), the PTT pressed by a
lever on the right side.

Seen from the front: the board against the right and top walls; a 20 x 30
oval speaker left of the display, in the middle of the height and as
close to the display as the cut allows, its face against the face's
inside in a ring round its edge (room for its ear at either end),
standing through the cut made by hand in the produced board (CUT_U1,
CUT_V1: the top-left notch carried straight down); a 602030 cell (300 mAh)
flat on the back's floor behind the board, over the low parts between the
display and the jack (J3's pin header comes off: solder the speaker's
wires to its pads); two 10 x 3 disc magnets in pockets in the back, a
0.6 mm skin outside them, so a steel plate behind a shirt holds it.

The halves close on two M3 self-tapping screws from the back through the
board's holes H1 and H2 (the board clamped between the back's standoffs
and the front's posts); a tongue round the back's walls fits inside the
front's at the seam.

The PTT lever is the microphone's: hinged at the bottom right on a column
of the back (an M3 screw from the face through the lever into it), one
outline the full depth up the right side, printed on its back; a claw over
the top whose tooth drops into a pocket in the top wall keeps it on and
stops it outward. A bar on the lever's inside reaches through a slot in
the back's right wall onto the PTT, which sits under the board's edge. The lever has a
hole round the jack's plug.

Inside 62.5 x 43 x 14: the width follows the speaker (the board is
42.11 tall).

    make wearable     wearable/*.stl and pieces/*.glb; open wearable/viewer.html
    python wearable.py check    the overlap check
"""
import json
import math
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "printed"))

from build123d import Align, Axis, Box, Pos, Rot, export_stl, fillet  # noqa: E402
import printed as P  # noqa: E402
from printed import (box, cyl_z, cone_z, cyl_x, prism_xy, rrect, hole_y,  # noqa: E402
                     glb, whole)

OUT = HERE

# ---------------------------------------------------------------- printing
WALL, FACE, BACK = 1.2, 1.2, 1.0          # three 0.4 perimeters; it holds ~25 g
CORNER, IN_CORNER, FACE_R = 3.0, 1.0, 0.8
CLR = 0.4
# the width follows the speaker: against the display's module, the wall
# just past its left edge (20 wide, SPK_CLR each side; 0.3 to the module)
# the cut made in the board by hand (~/Desktop/cut.jpeg, read off the photo
# against H1 and H2, +-0.3): the top-left notch carried straight down, open
# to the board's left edge, to u CUT_U1 and v CUT_V1 (seen from the front,
# from the board's top-left corner)
CUT_U1, CUT_V1 = 13.3, 36.3            # u: 58.3 - 45.0, the user's measure (speaker + board = 65)
CUT_GAP = 0.2                     # the speaker's edge to the cut's
SPK_W_, SPK_L_, SPK_CLR_ = 20.0, 30.0, 0.3
SPK_U = CUT_U1 - CUT_GAP - SPK_W_ / 2
IW = 66.0                         # the user's: the speaker, the cut, the board
assert IW >= P.BW + CLR - (SPK_U - SPK_W_ / 2 - SPK_CLR_) - 1e-9, "the speaker meets the left wall"
# the depth from the stack, back to face: the cell on the floor, 0.3, the
# tallest part over it, the board, the gap to the face. The tallest over
# the cell is the SuperMini's chip antenna at its top end, under the cell's
# edge: ~2.3 off the board on the module (ASSUMED from cut.jpeg; the
# PAM8403 is 1.75)
CELL_T_, UNDER_CELL, CELL_GAP = 6.2, 2.4, 0.3
# the keys (B3FS-1010P, measured): a 2.5 pedestal, the plunger 2.0 more;
# the face's inside rests on the pedestals, the plungers 0.8 proud of it
KEY_BODY, KEY_TOP = 2.5, 4.5
FRONT_H_ = KEY_BODY
IH = 43.0
IN_D = round(CELL_T_ + CELL_GAP + UNDER_CELL + P.BT + FRONT_H_, 2)
OW, OH = IW + 2 * WALL, IH + 2 * WALL
D = BACK + IN_D + FACE
TONGUE_T, TONGUE_H, TONGUE_CLR = 0.5, 1.2, 0.15
PORT_WALL, PORT_RECESS = 0.8, 0.4

# ---------------------------------------------------------------- board
BW, BH, BT = P.BW, P.BH, P.BT
# the face's inside to the board's front (FRONT_H_): the B3FS-1010P keys
# (4.3 tall) 0.5 proud of the face; the display's glass (3.0) sits 0.4 up
# into the face, 0.6 of it left over the glass's edge
FRONT_H = FRONT_H_
Z_FACE_IN = D - FACE
Z_BF = Z_FACE_IN - FRONT_H
Z_BB = Z_BF - BT
Z_SPLIT = Z_BB


def bx(u):
    return OW - WALL - CLR - BW + u


def by(v):
    return OH - WALL - CLR - v


def z_back(up):
    return Z_BB - up


# ---------------------------------------------------------------- speaker
# Taoglas SPKM.2030.8.A (datasheet SPE-22-8-004-D, page 8): a 20 x 30
# oval (+-0.2), round ends; 4.6 deep, 5.1 with the black foam liner on its
# face; a small ear at the top, at its face; its wires out of the bottom
# (35 long), folded back through the board's cut. Read off the drawing, not
# dimensioned (ASSUMED): the frame's rim 2.5 deep, the boss behind it 14
# across and 2.1 more; the ear 1.2 round, 0.4 past the oval, 0.5 thick
SPK_W, SPK_L, SPK_NET, SPK_FRAME, SPK_MAG = 20.0, 30.0, 0.5, 2.5, 2.1
SPK_MAG_D = 14.0
HAS_EAR = True
EAR_R, EAR_OUT, EAR_T = 1.2, 0.4, 0.5
SPK_T = SPK_NET + SPK_FRAME + SPK_MAG
SPK_CLR = SPK_CLR_
CUT_CLR = SPK_CLR + 0.5            # the board's cut round the speaker
RIB = 1.0
# the finger's dish round each key, shallow: the keys are proud
KEY_DISH = 0.4
# the mic's hole through the face, no tube: 1.0 printed shut
MIC_BORE = 2.0
# the display's glass, 16.5 tall in all with the 3 strip at its top (the
# user's measure; printed.py has it 14): its pocket in the face's inside
# covers all of it, the face left over it as thin as it goes
_gu0, _gu1, _gv0, _gv1 = P.OLED_GLASS
OLED_GLASS = (_gu0, _gu1, _gv1 - 16.5, _gv1)
GLASS_SKIN = 0.5
INSET_TOP = 1.0                   # the pocket 1 more at the glass's top (the test print)
RING_H = FRONT_H - 0.3            # down to just over the board, which it overlaps
OPEN_IN = 2.0                            # the grille opens this far inside its rim
assert SPK_W == SPK_W_
# as close to the display as it goes, in the middle of the height
SPK_X = bx(SPK_U)
# the middle of the height, unless the cut's lower edge holds it up
SPK_Y = max(OH / 2, by(CUT_V1 - CUT_GAP - SPK_L / 2))
EAR_Y = SPK_Y + SPK_L / 2 + EAR_OUT        # the ear may be either end: room at both
SPK_Z1 = Z_FACE_IN
SPK_Z0 = SPK_Z1 - SPK_T

# ---------------------------------------------------------------- cell
# 602030, 300 mAh: 6 x 20 x 30, with room for its tape and protection board
CELL_L, CELL_W, CELL_T = 31.0, 20.5, CELL_T_
CELL_U0 = 11.5                      # board u of its left end
CELL_CLR, RIDGE_T, RIDGE_H = 0.4, 1.2, 2.0
CX0, CX1 = bx(CELL_U0), bx(CELL_U0 + CELL_L)
CY1 = OH - WALL - 0.2
CY0 = CY1 - CELL_W
CZ1 = BACK + CELL_T

# ---------------------------------------------------------------- magnets
MAG_D, MAG_T, MAG_SKIN, MAG_CLR = 10.0, 3.0, 0.6, 0.1
MAG_RIM = 1.2
MAG_V = 28.0                        # board v of their centres
MAG_X = 8.5                         # in from the side walls, the same each side
MAGNETS = [(MAG_X, by(MAG_V)), (OW - MAG_X, by(MAG_V))]
MAG_TOP = MAG_SKIN + MAG_T + 0.2

# ---------------------------------------------------------------- screws
M3_CLEAR, M3_PILOT, M3_HEAD_D, M3_HEAD_H = P.M3_CLEAR, P.M3_PILOT, P.M3_HEAD_D, P.M3_HEAD_H
POST_D, STANDOFF_D, PILOT_SKIN = 6.0, 6.5, 1.0
BOSS_D, BOSS_SKIN = 10.5, 0.8
SCREWS = [(bx(u), by(v)) for (u, v) in P.HOLES]

# the keys exactly where the board's switches are
KEYS = list(P.KEYS)
for _ref, (_u, _v) in zip(("SW1", "SW2", "SW3", "SW4"), KEYS):
    assert (_u, _v) == P.uv(_ref), f"{_ref}'s hole is not on its switch"


# ---------------------------------------------------------------- lever
# as on the microphone's case: the hinge at the bottom right, on a column
# standing on the back; the lever one outline the full depth up the right
# side, printed on its back (its bar onto the PTT and its claw's tooth stand
# on the bed), the column's pocket open at its back with a 45 degree ceiling;
# an M3 screw from the face through the lever into the column, its head in
# a counterbore. At the
# top, a claw over the case's top: a tooth on it drops into a pocket in the
# back's top wall, the outward stop that keeps the lever on. A bar on the
# lever reaches in through the wall onto the PTT.
PTT_X, PTT_Y, PTT_Z = bx(BW), by(P.PTT_V), z_back(P.PTT_UP)
JACK_Y = by(P.JACK_V)
PRELOAD = 0.1
# the PTT (SKRTLAE010) sits back from the board's edge under the board, its
# 1.9 square pedestal 1.7 off the board and not out past the edge: a bar on
# the lever's inside reaches in through a slot in the back's wall onto it,
# just under the board; it runs from the lever's back face (stiff), its top
# a 45 degree roof (printed face down that is its underside)
BAR_W, BAR_CLR, BAR_UNDER = 2.0, 0.2, 0.3     # across; round it in the slot; under the board
BAR_TOP = Z_BB - BAR_UNDER
LEV_GAP = 1.0
LEV_T, LEV_FLARE = 4.5, 4.0
LEV_IN = OW + LEV_GAP
# the back's column: round, a 45 degree point on top, its neck to the wall
# a 45 degree roof; the lever's pocket round it the same, open at the
# lever's back (the bed), so its ceiling prints without a bridge
COL_R, COL_H = 3.2, 7.0           # the column's round part stands COL_H, then its point
NECK_W = 3.0
PIV_R = 5.2                       # the lever's boss: 1.3 of wall round the pocket
PIV = (OW + PIV_R + 0.4, 4.3)
LEV_TOP = OH + 0.3 + 2.0            # the top of the claw
POCKET_X1 = OW - WALL - 0.6         # the outward stop face
TOOTH_W = 1.5
HOOK_X0 = POCKET_X1 - TOOTH_W
PLUG_D = 8.5                        # through the lever, round the jack's plug
LEV_CLR = 0.4

# ---------------------------------------------------------------- grille
G_PITCH, G_HOLE, G_EDGE, DIMPLE = 3.6, 2.4, 2.6, 0.6

# ---------------------------------------------------------------- checks
# the board's hand cut, in its own u, v (seen from the front, from its
# top-left corner): the speaker's oval grown by CUT_CLR
_cut_u = SPK_X + SPK_W / 2 + CUT_CLR - bx(0)
_cut_v = (by(SPK_Y + SPK_L / 2 + CUT_CLR), by(SPK_Y - SPK_L / 2 - CUT_CLR))
_cut_c = (SPK_U, by(SPK_Y))


def copper_in_cut(margin=0.3):
    """Every pad, via and track of the produced board within margin of
    the hand cut, as (what, how far inside); empty when the cut is clear."""
    import re
    t = open(P.PCB).read()
    r, hl = SPK_W / 2 + CUT_CLR, SPK_L / 2 + CUT_CLR
    uc, vc = _cut_c

    def outside(u, v):
        dy = max(abs(v - vc) - (hl - r), 0.0)
        return math.hypot(u - uc, dy) - r

    def uvxy(x, y):
        return P.KX1 - x, y - P.KY0
    bad = []
    for m in re.finditer(r'\(footprint "[^"]+"(.*?)\n\t\)', t, re.S):
        body = m.group(1)
        ref = re.search(r'\(property "Reference" "([^"]+)"', body).group(1)
        at = re.search(r'\n\t\t\(at ([-\d.]+) ([-\d.]+)(?: ([-\d.]+))?\)', body)
        x, y, rot = float(at.group(1)), float(at.group(2)), float(at.group(3) or 0)
        a = math.radians(rot)
        for pd in re.finditer(r'\(pad "[^"]*" \w+ \w+\s*\(at ([-\d.]+) ([-\d.]+)[^)]*\)\s*\(size ([\d.]+) ([\d.]+)\)', body):
            px, py, sw, sh = map(float, pd.groups())
            u, v = uvxy(x + px * math.cos(a) + py * math.sin(a), y - px * math.sin(a) + py * math.cos(a))
            hw, hh = (sh, sw) if rot % 180 else (sw, sh)
            for i in range(11):
                for j in range(11):
                    d = outside(u - hw / 2 + hw * i / 10, v - hh / 2 + hh * j / 10)
                    if d < margin:
                        bad.append((ref, round(d, 2)))
                        break
                else:
                    continue
                break
    for m in re.finditer(r'\(segment\s*\(start ([-\d.]+) ([-\d.]+)\)\s*\(end ([-\d.]+) ([-\d.]+)\)\s*\(width ([\d.]+)\)', t):
        x0, y0, x1, y1, w = map(float, m.groups())
        (u0, v0), (u1, v1) = uvxy(x0, y0), uvxy(x1, y1)
        for i in range(41):
            d = outside(u0 + (u1 - u0) * i / 40, v0 + (v1 - v0) * i / 40) - w / 2
            if d < margin:
                bad.append((f"track at {u0:.1f},{v0:.1f}", round(d, 2)))
                break
    for m in re.finditer(r'\(via\s*\(at ([-\d.]+) ([-\d.]+)\)\s*\(size ([\d.]+)', t):
        u, v = uvxy(float(m.group(1)), float(m.group(2)))
        if outside(u, v) - float(m.group(3)) / 2 < margin:
            bad.append(("via", round(outside(u, v), 2)))
    return bad


# the speaker (its clearance in) inside the cut made
assert SPK_U + SPK_W / 2 <= CUT_U1 - 0.15, "the speaker meets the cut's right edge"
assert by(SPK_Y - SPK_L / 2) <= CUT_V1 - 0.15, "the speaker meets the cut's lower edge"
assert by(BH) >= WALL + 0.3, "the board meets the bottom wall"
assert (EAR_Y + EAR_R if HAS_EAR else SPK_Y + SPK_L / 2) + SPK_CLR <= OH - WALL + 1e-9
assert SPK_X + SPK_W / 2 + SPK_CLR <= bx(P.OLED_PCB[0]) - 0.3 + 1e-9, "the speaker meets the OLED"
assert Z_FACE_IN - RING_H >= Z_BF + 0.3 - 1e-9, "the ring meets the board"
assert Z_BF + P.OLED_TOP <= D - GLASS_SKIN + 1e-9, "the display's glass meets the face"
assert Z_BF + KEY_BODY <= Z_FACE_IN + 1e-9, "the keys' pedestals reach into the face"
assert CZ1 + CELL_GAP <= Z_BB - UNDER_CELL + 1e-9, "the cell meets the parts over it"
assert CZ1 <= SPK_Z0 - 0.3, "the cell meets the speaker"
assert Z_FACE_IN - RING_H <= SPK_Z1 - SPK_NET - 0.3, "the ring misses the frame"
assert CX0 - CELL_CLR - RIDGE_T >= MAGNETS[0][0] + MAG_D / 2 + 0.3
assert MAG_TOP <= z_back(5.0) - 0.5, "a magnet meets the jack"
assert BACK + IN_D == Z_FACE_IN
assert BAR_TOP >= PTT_Z + P.PTT_PLUNGER / 2 + 0.2, "the bar's roof leaves the pedestal's top"
assert PIV[1] + COL_R + LEV_CLR + 0.8 <= JACK_Y - PLUG_D / 2, "the plug's hole meets the hinge's pocket"
assert PIV_R - (COL_R + LEV_CLR) >= 1.2, "the lever's wall round its pocket"


def obround(cx, cy, w, l, z0, z1):
    return rrect(cx - w / 2, cy - l / 2, w, l, w / 2 - 0.01, z0, z1)


def ear(grow, z0, z1, end=1):
    y = SPK_Y + end * (SPK_L / 2 + EAR_OUT)
    return (cyl_z(SPK_X, y, EAR_R + grow, z0, z1)
            + box(SPK_X - EAR_R - grow, SPK_X + EAR_R + grow, min(SPK_Y, y), max(SPK_Y, y), z0, z1))


def outside(z0, z1):
    return rrect(0, 0, OW, OH, CORNER, z0, z1)


def inside(z0, z1, grow=0.0):
    return rrect(WALL - grow, WALL - grow, IW + 2 * grow, IH + 2 * grow,
                 max(IN_CORNER + grow, 0.2), z0, z1)


def head_pocket(x, y):
    h = (M3_HEAD_D - M3_CLEAR + 0.2) / 2
    return (cyl_z(x, y, M3_HEAD_D / 2, -1, M3_HEAD_H)
            + cone_z(x, y, M3_HEAD_D / 2, (M3_CLEAR - 0.2) / 2, M3_HEAD_H - 0.01, M3_HEAD_H + h))


def ports():
    """(the hole, its notch up to the seam), as printed.py's."""
    zj, zu, zk = z_back(P.JACK_UP), z_back(P.USB_UP), z_back(P.KNOB_UP)
    xr0, xr1 = OW - WALL - 1, OW + 1
    yb0, yb1 = -1, WALL + 1
    ux, kx = bx(P.USB_U), bx(P.KNOB_U)
    w = BAR_W / 2 + BAR_CLR
    slot = box(xr0, xr1, PTT_Y - w, PTT_Y + w, -1, Z_SPLIT)
    top = Z_SPLIT + TONGUE_H + 0.5     # through the tongue too
    jr = P.JACK_HOLE / 2
    return [
        (slot, box(xr0, xr1, PTT_Y - w, PTT_Y + w, -1, top)),
        (cyl_x(xr0, xr1, JACK_Y, zj, jr),
         box(xr0, xr1, JACK_Y - jr, JACK_Y + jr, zj, top)),
        (hole_y(yb0, yb1, ux, zu, P.USB_W, P.USB_H),
         box(ux - P.USB_W / 2, ux + P.USB_W / 2, yb0, yb1, zu, top)),
        (hole_y(yb0, yb1, kx, zk, P.KNOB_W, P.KNOB_H),
         box(kx - P.KNOB_W / 2, kx + P.KNOB_W / 2, yb0, yb1, zk, top)),
    ]


def recesses():
    ux, zu = bx(P.USB_U), z_back(P.USB_UP)
    return (cyl_x(OW - PORT_RECESS, OW + 1, JACK_Y, z_back(P.JACK_UP), P.JACK_RECESS_D / 2)
            + hole_y(-1, PORT_RECESS, ux, zu, *P.USB_RECESS))


def column(grow=0.0):
    """The hinge's column outside the bottom-right corner, standing on the
    back: round to COL_H, then a 45 degree point; its neck to the wall a
    45 degree roof. Grown, it is the lever's pocket round it."""
    r = COL_R + grow
    c = (cyl_z(PIV[0], PIV[1], r, -1, COL_H)
         + cone_z(PIV[0], PIV[1], r, 0.01, COL_H - 0.01, COL_H + r))
    w = NECK_W + 2 * grow
    x0 = OW - 2.0 - grow
    neck = (box(x0, PIV[0], PIV[1] - w / 2, PIV[1] + w / 2, -1, COL_H)
            + Pos(x0, PIV[1], COL_H) * Rot(45, 0, 0)
            * Box(PIV[0] - x0, w / 2 ** 0.5, w / 2 ** 0.5, align=(Align.MIN, Align.CENTER, Align.CENTER)))
    return c + neck


def teardrop_x(x0, x1, cy, cz, r, up=1):
    """A round hole or pin along x with a 45 degree point toward +z (up=1)
    or -z, so it prints without support: a square r on a side, turned 45
    degrees, its lower corner at the centre and its sides on the circle."""
    pt = Pos(x0, cy, cz + up * r / 2 ** 0.5) * Rot(45, 0, 0) * Box(
        x1 - x0, r, r, align=(Align.MIN, Align.CENTER, Align.CENTER))
    return cyl_x(x0, x1, cy, cz, r) + pt


def tooth_pocket():
    """The claw's tooth's pocket in the top wall, back half, open to the back
    face and the top; 0.3 left of the wall inside it."""
    return box(POCKET_X1 - 3.5, POCKET_X1, OH - WALL + 0.3, OH + 1, -1, Z_SPLIT + 0.01)


# ---------------------------------------------------------------- grille
def lattice():
    nx = int((OW - 2 * G_EDGE) // G_PITCH) + 1
    ny = int((OH - 2 * G_EDGE) // G_PITCH) + 1
    x0 = (OW - (nx - 1) * G_PITCH) / 2
    y0 = (OH - (ny - 1) * G_PITCH) / 2
    return [(x0 + i * G_PITCH, y0 + j * G_PITCH) for i in range(nx) for j in range(ny)]


def in_obround(x, y, cx, cy, w, l):
    r = w / 2
    dy = max(abs(y - cy) - (l / 2 - r), 0.0)
    return math.hypot(x - cx, dy) <= r


def grille():
    """(through holes, dimples): open over the speaker's cone, dimples
    everywhere else on the face that is clear of the window, the keys,
    the mic and the posts' pilots."""
    hr = G_HOLE / 2
    holes, dimples = [], []
    x0, x1, v0, v1 = P.OLED_WIN
    wx0, wx1, wy0, wy1 = bx(x0), bx(x1), by(v1), by(v0)
    mx, my = bx(P.MIC_UV[0]), by(P.MIC_UV[1])
    for (x, y) in lattice():
        if in_obround(x, y, SPK_X, SPK_Y, SPK_W - 2 * OPEN_IN - G_HOLE,
                      SPK_L - 2 * OPEN_IN - G_HOLE):
            holes.append((x, y))
            continue
        m = hr + 1.2
        if wx0 - m < x < wx1 + m and wy0 - m < y < wy1 + m:
            continue
        g0, g1, gv0, gv1 = OLED_GLASS
        m = hr + CLR + 0.6
        if bx(g0) - m < x < bx(g1) + m and by(gv1) - m < y < by(gv0 - INSET_TOP) + m:
            continue
        if any(math.hypot(x - bx(u), y - by(v)) < P.DISH_D / 2 + hr + 0.8 for u, v in KEYS):
            continue
        if math.hypot(x - mx, y - my) < hr + 1.5:
            continue
        if x + hr > POCKET_X1 - 3.5 - 0.6 and y + hr > OH - WALL + 0.3 - 0.6:
            continue
        if any(math.hypot(x - sx, y - sy) < M3_PILOT / 2 + hr + 0.6 for sx, sy in SCREWS):
            continue
        # not over the speaker's rim, where the ring stands under the face
        if in_obround(x, y, SPK_X, SPK_Y, SPK_W + 2 * (SPK_CLR + RIB) + G_HOLE,
                      SPK_L + 2 * (SPK_CLR + RIB) + G_HOLE):
            continue
        dimples.append((x, y))
    return holes, dimples


# ---------------------------------------------------------------- front
def front_shell():
    ff = Z_FACE_IN
    out = outside(Z_SPLIT, D)
    out = fillet(out.faces().sort_by(Axis.Z)[-1].edges(), FACE_R)
    s = out - inside(Z_SPLIT - 1, ff)

    # the speaker's ring, into the left and top walls
    ring = (obround(SPK_X, SPK_Y, SPK_W + 2 * (SPK_CLR + RIB), SPK_L + 2 * (SPK_CLR + RIB),
                    ff - RING_H, ff + 0.5)
            - obround(SPK_X, SPK_Y, SPK_W + 2 * SPK_CLR, SPK_L + 2 * SPK_CLR, ff - RING_H - 1, ff + 1))
    if HAS_EAR:
        ring -= ear(SPK_CLR, ff - RING_H - 1, ff + 1) + ear(SPK_CLR, ff - RING_H - 1, ff + 1, -1)
    # no ring between the speaker and the display's module
    x0, x1, v0, v1 = P.OLED_PCB
    ring -= box(bx(x0) - 0.3, bx(x1), by(v1) - 1, by(v0) + 1, 0, D)
    s += ring & inside(Z_SPLIT, D, 0.5)

    for (x, y) in SCREWS:
        s += cyl_z(x, y, POST_D / 2, Z_BF, ff + 0.5)
    mx, my = bx(P.MIC_UV[0]), by(P.MIC_UV[1])


    # ---- cuts
    # the speaker's room, its clearance round it: off the posts too (H2's
    # post comes within its round end)
    s -= obround(SPK_X, SPK_Y, SPK_W + 2 * SPK_CLR, SPK_L + 2 * SPK_CLR, SPK_Z0 - 1, ff)
    # room for the back's tongue
    s -= inside(Z_SPLIT - 1, Z_SPLIT + TONGUE_H + 0.2, TONGUE_T + TONGUE_CLR)
    for (x, y) in SCREWS:
        s -= cyl_z(x, y, M3_PILOT / 2, Z_SPLIT - 6, D - PILOT_SKIN)
    s -= cyl_z(mx, my, MIC_BORE / 2, ff - 1, D + 1)

    holes, dimples = grille()
    hr = G_HOLE / 2
    for (x, y) in holes:
        s -= cyl_z(x, y, hr, ff - 1, D + 1)
    # no dimples: the grille is over the speaker only

    x0, x1, v0, v1 = P.OLED_WIN
    s -= box(bx(x0), bx(x1), by(v1), by(v0), ff - 1, D + 1)
    pocket = FACE - GLASS_SKIN
    x0, x1, v0, v1 = OLED_GLASS
    s -= box(bx(x0) - CLR, bx(x1) + CLR, by(v1) - CLR, by(v0 - INSET_TOP) + CLR, ff - 1, ff + pocket)
    for (u, v) in KEYS:
        x, y = bx(u), by(v)
        s -= cyl_z(x, y, P.KEY_HOLE / 2, ff - 1, D + 1)
        s -= cone_z(x, y, P.KEY_HOLE / 2, P.DISH_D / 2, D - KEY_DISH, D)
        s -= cyl_z(x, y, P.DISH_D / 2, D, D + 1)

    for hole, _ in ports():
        s -= hole
    s -= recesses()
    return s


# ---------------------------------------------------------------- back
def back_tray():
    out = outside(Z_SPLIT, 0)
    out = fillet(out.faces().sort_by(Axis.Z)[0].edges(), FACE_R)
    s = out - inside(BACK, Z_SPLIT + 1)
    s += inside(Z_SPLIT - 0.01, Z_SPLIT + TONGUE_H, TONGUE_T) - inside(Z_SPLIT - 1, D)
    floor = BACK - 0.5

    for (x, y) in SCREWS:
        s += cyl_z(x, y, STANDOFF_D / 2, floor, Z_BB)
        top = M3_HEAD_H + BOSS_SKIN
        foot = (cyl_z(x, y, BOSS_D / 2, floor, top)
                + cone_z(x, y, BOSS_D / 2, STANDOFF_D / 2, top - 0.01, top + (BOSS_D - STANDOFF_D) / 2))
        s += foot & inside(0, D, 0.5)

    for (x, y) in MAGNETS:
        s += cyl_z(x, y, MAG_D / 2 + MAG_CLR + MAG_RIM, floor, MAG_TOP)

    # the cell: an outline ridge round its foot
    g = CELL_CLR
    ridge = (box(CX0 - g - RIDGE_T, CX1 + g + RIDGE_T, CY0 - g - RIDGE_T, CY1 + g + RIDGE_T,
                 floor, BACK + RIDGE_H)
             - box(CX0 - g, CX1 + g, CY0 - g, CY1 + g, 0, D))
    s += ridge & inside(0, D, 0.5)

    # a cross of ribs up to the speaker's back: it is held to the face's foam
    # (on its magnet)
    s += box(SPK_X - RIB / 2, SPK_X + RIB / 2, SPK_Y - 4.5, SPK_Y + 4.5, floor, SPK_Z0)
    s += box(SPK_X - 4.5, SPK_X + 4.5, SPK_Y - RIB / 2, SPK_Y + RIB / 2, floor, SPK_Z0)


    # ---- cuts
    for (x, y) in SCREWS:
        s -= cyl_z(x, y, M3_CLEAR / 2, -1, D)
        s -= head_pocket(x, y)
    for (x, y) in MAGNETS:
        s -= cyl_z(x, y, MAG_D / 2 + MAG_CLR, MAG_SKIN, MAG_TOP + 1)
    for hole, notch in ports():
        s -= hole + notch
    s -= recesses()
    s -= tooth_pocket()
    # the hinge's column, the screw's pilot down it
    s += column() & box(0, OW + 20, -20, OH, 0, D)
    s -= cyl_z(PIV[0], PIV[1], M3_PILOT / 2, 0.5, D)
    return s


# ---------------------------------------------------------------- lever
def lever():
    """As the microphone's, printed on its back: the bar onto the PTT and
    the claw's tooth stand on the bed; the hinge's pocket opens on the bed,
    its ceiling 45 degrees over the column's point."""
    L, T, F = LEV_IN, LEV_T, LEV_FLARE
    s = prism_xy([(L, PIV[1]), (L + T, PIV[1]), (L + T, 12.0), (L + T + F, OH - 8.0),
                  (L + T + F, LEV_TOP), (L, LEV_TOP)], 0, D)
    s += cyl_z(PIV[0], PIV[1], PIV_R, 0, D)
    try:
        s = fillet(s.edges().filter_by(Axis.Z), 1.5)
    except Exception:
        pass
    # the claw over the case's top, and its tooth down into the pocket
    s += box(HOOK_X0, L + 1.0, OH + 0.3, OH + 2.3, 0, D)
    s += box(HOOK_X0, HOOK_X0 + TOOTH_W, OH - WALL + 0.7, LEV_TOP, 0, Z_SPLIT - LEV_CLR)
    # the column's pocket, open at the back (the bed); the screw from the
    # face, its head in a counterbore (open at the top as printed)
    s -= column(LEV_CLR)
    s -= cyl_z(PIV[0], PIV[1], M3_CLEAR / 2, -1, D + 1)
    s -= cyl_z(PIV[0], PIV[1], M3_HEAD_D / 2, D - (M3_HEAD_H + 0.3), D + 1)
    # the bar onto the PTT, from the lever's back face up to its roof
    x0 = PTT_X - PRELOAD
    roof = BAR_W / 2
    s += box(x0, L + 0.5, PTT_Y - BAR_W / 2, PTT_Y + BAR_W / 2, 0, BAR_TOP - roof)
    s += (Pos(x0, PTT_Y, BAR_TOP - roof) * Rot(45, 0, 0)
          * Box(L + 0.5 - x0, BAR_W / 2 ** 0.5, BAR_W / 2 ** 0.5,
                align=(Align.MIN, Align.CENTER, Align.CENTER)))
    # round the jack's plug: its point toward the face, up as printed
    s -= teardrop_x(L - 1, L + T + F + 1, JACK_Y, z_back(P.JACK_UP), PLUG_D / 2, up=1)
    return s


# ---------------------------------------------------------------- parts
def parts():
    board = (box(bx(0), bx(BW), by(BH), by(0), Z_BB, Z_BF)
             - box(bx(-1), bx(P.NOTCH_U), by(P.NOTCH_V), by(-1), Z_BB - 1, Z_BF + 1)
             - box(bx(-1), bx(CUT_U1), by(CUT_V1), by(-1), Z_BB - 1, Z_BF + 1))
    for (x, y) in SCREWS:
        board -= cyl_z(x, y, 1.6, Z_BB - 1, Z_BF + 1)
    x0, x1, v0, v1 = P.OLED_PCB
    glass_z = Z_BF + P.OLED_TOP
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
           + box(bx(BW) - 0.8, bx(BW), c - P.PTT_PLUNGER / 2, c + P.PTT_PLUNGER / 2,
                 PTT_Z - P.PTT_PLUNGER / 2, PTT_Z + P.PTT_PLUNGER / 2))
    jz = z_back(P.JACK_UP)
    jack = (box(bx(BW) - 0.1 - 2.0 - 11.8, bx(BW) - 0.1 - 2.0, JACK_Y - 2.9, JACK_Y + 2.9, Z_BB - 5.0, Z_BB)
            + cyl_x(bx(BW) - 2.2, bx(BW) - 0.1, JACK_Y, jz, 2.3))
    ux, uz = bx(P.USB_U), z_back(P.USB_UP)
    mini = (box(ux - 9.175, ux + 9.175, by(BH) + 0.15, by(BH) + 22.65, Z_BB - 1.0, Z_BB)
            + box(ux - 4.47, ux + 4.47, by(BH) - 0.3, by(BH) + 7.0, uz - 1.6, uz + 1.6))
    kx = bx(P.KNOB_U)
    slide = (box(kx - 3.3, kx + 3.3, by(BH) + 1.0, by(BH) + 3.7, Z_BB - 1.4, Z_BB)
             + box(kx - 0.6, kx + 0.6, by(BH) - 1.0, by(BH) + 1.0, Z_BB - 1.5, Z_BB))
    pam = box(bx(P.uv("U3")[0]) - 2.0, bx(P.uv("U3")[0]) + 2.0,
              by(P.uv("U3")[1]) - 5.0, by(P.uv("U3")[1]) + 5.0, Z_BB - 1.75, Z_BB)
    zf = SPK_Z1 - SPK_NET
    speaker = (obround(SPK_X, SPK_Y, SPK_W, SPK_L, zf, SPK_Z1)
               + obround(SPK_X, SPK_Y, SPK_W, SPK_L, zf - SPK_FRAME, zf)
               + cyl_z(SPK_X, SPK_Y, SPK_MAG_D / 2, SPK_Z0, zf - SPK_FRAME + 0.01)
               )
    if HAS_EAR:
        speaker += ear(0, zf - EAR_T, zf)
    cell = box(CX0, CX1, CY0, CY1, BACK, CZ1)
    mags = None
    for (x, y) in MAGNETS:
        m = cyl_z(x, y, MAG_D / 2, MAG_SKIN, MAG_SKIN + MAG_T)
        mags = m if mags is None else mags + m
    return [(board, "board", (0.10, 0.40, 0.25)),
            (oled, "oled", (0.10, 0.25, 0.65)),
            (glass, "glass", (0.01, 0.01, 0.01)),
            (keys, "keys", (0.85, 0.85, 0.85)),
            (ptt, "ptt", (0.75, 0.75, 0.75)),
            (jack, "jack", (0.25, 0.25, 0.25)),
            (mini, "supermini", (0.20, 0.20, 0.55)),
            (slide, "switch", (0.9, 0.9, 0.9)),
            (pam, "pam8403", (0.1, 0.1, 0.1)),
            (speaker, "speaker", (0.55, 0.55, 0.55)),
            (cell, "cell", (0.70, 0.72, 0.75)),
            (mags, "magnets", (0.6, 0.6, 0.65))]


SHELL = (0.30, 0.32, 0.35)
LEVER = (0.8, 0.15, 0.1)


def check(front, back, lev):
    for name, p in (("front", front), ("back", back), ("lever", lev)):
        print(f"{name}: {len(p.solids())} solid(s)")
    shells = [(front, "front"), (back, "back"), (lev, "lever")]
    things = [(p, n) for p, n, _ in parts()]
    # the stub presses the plunger by PRELOAD on purpose
    allowed = {("lever", "ptt")}
    bad, seen = 0, set()
    for a in shells + things:
        for b in shells + things:
            k = tuple(sorted((a[1], b[1])))
            if a[1] == b[1] or k in seen or k in allowed:
                continue
            seen.add(k)
            if a in things and b in things and not ({a[1], b[1]} & {"cell", "speaker", "magnets"}):
                continue
            i = whole(a[0]).intersect(whole(b[0]))
            v = sum(s.volume for s in i.solids()) if i else 0
            if v > 0.05:
                print(f"  {a[1]} meets {b[1]}: {v:.2f} mm3")
                bad += 1
    print("clear" if not bad else f"{bad} overlaps")


def test_piece(front):
    """The front over the display and the keys only, to try their fit on
    the board: printed face down, held on the board by hand."""
    us = [u for u, _ in KEYS] + [OLED_GLASS[0], OLED_GLASS[1]]
    vs = [v for _, v in KEYS] + [OLED_GLASS[2], OLED_GLASS[3]]
    u0, u1 = min(us) - 4.0, max(us) + 4.0
    v0, v1 = min(vs) - 2.0, min(max(vs) + 4.0, BH)
    return front & box(bx(u0), bx(u1), by(v1), by(v0), Z_BF, D + 1)


def main():
    print(f"box {OW:.1f} wide, {OH:.1f} tall, {D:.1f} thick "
          f"({PIV[0] + PIV_R:.1f} wide with the lever); "
          f"inside {IW} x {IH} x {IN_D}; speaker centre u {SPK_U:.2f}, v {by(SPK_Y):.2f} on the cut to u {CUT_U1}, v {CUT_V1}")
    front, back, lev = front_shell(), back_tray(), lever()
    if "check" in sys.argv:
        check(front, back, lev)
        return
    # to print: the face down, the back and the lever on their backs
    export_stl(Pos(0, 0, D) * Rot(180, 0, 0) * front, os.path.join(OUT, "front.stl"))
    export_stl(back, os.path.join(OUT, "back.stl"))
    export_stl(lev, os.path.join(OUT, "lever.stl"))
    test = test_piece(front)
    export_stl(Pos(0, 0, D) * Rot(180, 0, 0) * test, os.path.join(OUT, "front_test.stl"))
    bb = test.bounding_box()
    print(f"wrote {OUT}/front.stl back.stl lever.stl front_test.stl "
          f"(the test piece {bb.size.X:.1f} x {bb.size.Y:.1f} x {bb.size.Z:.1f})")
    pieces = [(front, "front", SHELL), (back, "back", SHELL), (lev, "lever", LEVER)] + parts()
    os.makedirs(os.path.join(OUT, "pieces"), exist_ok=True)
    for shape, name, rgb in pieces:
        glb([(shape, name, rgb)], os.path.join(OUT, "pieces", f"{name}.glb"))
    with open(os.path.join(OUT, "pieces", "list.json"), "w") as f:
        json.dump([n for _, n, _ in pieces], f)


if __name__ == "__main__":
    main()
