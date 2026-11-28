#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Ashhar Farhan
"""
wearable2.py — the wearable case, the speaker over the board: the board
sawn straight across left of SW2 (seen from the front), H2 and the left
strip gone, through ground pour only; the 20 x 30 oval speaker over the
board's free right-hand front, past its right edge, its face against the
face's inside in a ring round its edge. The face stands over the speaker,
so the keys are under it: a printed keytop on each, a flange under the
face holding it in, 0.8 proud.

The PTT lever hinged near the middle of the right side, above the jack, so
a plug goes straight into the jack's hole; its bar reaches in to the PTT
under the board's edge, ending in a T head across the way it slides as the
lever turns. A BL-5C cell (54 x 34 x 6, the user's, 2026-11-26) flat on
the back's floor behind the whole board, along the top wall, across the
case: the case 4.7 wider on the left for it and 3.4 thicker, the jack and
J6 over it, held down by lips and snap arms; no magnets. The halves close
on snap hooks on the top wall, at both ends, the back pushed straight on,
and an M3 screw through H1 at the bottom right.

    make wearable2    wearable2/*.stl and pieces/*.glb; open wearable2/viewer.html
    python wearable2.py check    the overlap check
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
# the board sawn straight across at u SAW_U (seen from the front, from its
# top-left corner), clear of SW2's pad (13.55; checked below): it stands
# against the left and top walls
SAW_U = 13.3
# the speaker (Taoglas SPKM.2030.8.A, 20 x 30, 5.1 deep with its foam) on
# the board's front right of the display's module, in the middle of the
# height as far as SW3 and H1's post let it
SPK_W_, SPK_L_, SPK_T_, SPK_CLR_ = 20.0, 30.0, 5.1, 0.3
SPK_U = P.OLED_PCB[1] + 0.3 + SPK_CLR_ + SPK_W_ / 2
IW = math.ceil((SPK_U + SPK_W_ / 2 + SPK_CLR_ - (SAW_U - CLR)) * 10) / 10
# the depth from the stack, back to face: the cell on the floor, 0.3, the
# deepest part over it (the headset jack's barrel, JACK_UP + its 2.9 radius,
# 5.8 off the board), the board, the speaker over the board's front and 0.2
# behind it. The cell 6.2 deep: BL-5Cs are sold 5.8 to 6.2 thick
CELL_T_, UNDER_CELL, CELL_GAP = 6.2, 2.9 + 2.9, 0.3
SPK_BACK = 0.4                    # the user's: the face's inside, on the speaker's rim, 5.5 off the board
# the keys (B3FS-1010P, the user's, 2026-11-23): the body 3.0 off the board;
# a keytop's flange rests 3.8 off it, so the plunger's top is 3.8 + the socket
KEY_BODY, KEY_TOP = 3.0, 4.8
FRONT_H_ = SPK_T_ + SPK_BACK
IH = 43.0
# the BL-5C (the user's: 6 x 34 x 54) across the case along the top wall,
# 0.4 round it: the inside widened for it, on the left (the user's), so the
# right side (the jack, the PTT, the lever) stays as it was against the board
CELL_L, CELL_W, CELL_CLR = 54.0, 34.0, 0.4
IW_BOARD = IW
IW = max(IW_BOARD, round(CELL_L + 2 * CELL_CLR, 2))
LEFT_GROW = round(IW - IW_BOARD, 2)
IN_D = round(CELL_T_ + CELL_GAP + UNDER_CELL + P.BT + FRONT_H_, 2)
OW, OH = IW + 2 * WALL, IH + 2 * WALL
D = BACK + IN_D + FACE
# no lip at the seam (the user never asked for one): TONGUE_T 0; TONGUE_H
# still sets where the top-left standoff meets the front's post
TONGUE_T, TONGUE_H, TONGUE_CLR = 0.0, 1.2, 0.3
PORT_WALL, PORT_RECESS = 0.8, 0.4
# USB-C: any compliant cable. The standard's largest plug body is 12.35 x
# 6.5; the socket's face is in from the outside, so that body goes into the
# wall: the opening is its size, 0.4 a side, through the wall
# the SuperMini is soldered by hand, it lands a little off from board to
# board (the user's, 2026-11-27): USB_PLAY more a side across, for the
# socket and the plug
USB_PLAY = 0.6
USB_BODY_W, USB_BODY_H = 12.35 + 0.8 + 2 * USB_PLAY, 6.5 + 0.8
# ...but only USB_MATE short of the socket's face: behind that a skin, 0.8
# thick, with a hole only for the plug's metal end (8.25 x 2.4, 0.4 a side),
# so the case is closed round the socket; no notch to the seam
USB_SHELL_W, USB_SHELL_H = 8.25 + 0.8 + 2 * USB_PLAY, 2.4 + 0.8
USB_MATE = 0.9         # ASSUMED: a plug's metal end shows >= 0.9 past its body when mated
USB_SKIN = 0.8
USB_SKIN_GAP = 0.1     # the skin short of the socket's face (the back goes on straight)
LOCATE_GAP = 0.1
WIN_UP = 1.0           # the display's cutout up toward the glass's top (the user's: it clipped the top row)
LOCATE_X = (6.0 + LEFT_GROW, 40.0 + LEFT_GROW)  # clear of the USB and the power switch's openings
RIGHT_TEST_X = 38.0 + LEFT_GROW     # the right-side test pieces from here out (H1's boss is left of it)
# the headset's plug: its body 6.2 across (the user's); the jack's nose is
# in from the outside, so the body goes into the wall: 0.4 a side, through
JACK_BODY_D = 7.0                # the user's: 7 for badly moulded plugs (bodies 6.0)
# the jack's centre from the board's surface (the user's)
JACK_UP = 2.9

# ---------------------------------------------------------------- board
BW, BH, BT = P.BW, P.BH, P.BT
FRONT_H = FRONT_H_
Z_FACE_IN = D - FACE
Z_BF = Z_FACE_IN - FRONT_H
Z_BB = Z_BF - BT
Z_SPLIT = Z_BB


def bx(u):
    return WALL + LEFT_GROW + CLR + u - SAW_U


def by(v):
    return OH - WALL - CLR - v


def z_back(up):
    return Z_BB - up


# ---------------------------------------------------------------- speaker
# Taoglas SPKM.2030.8.A (datasheet SPE-22-8-004-D, page 8): a 20 x 30
# oval (+-0.2), round ends; 4.6 deep, 5.1 with the black foam liner on its
# face; a small ear at the top, at its face; its wires out of the bottom
# (35 long), round the board's right edge to J3's pads behind. Read off the drawing, not
# dimensioned (ASSUMED): the frame's rim 2.5 deep, the boss behind it 14
# across and 2.1 more; the ear 1.2 round, 0.4 past the oval, 0.5 thick
SPK_W, SPK_L, SPK_NET, SPK_FRAME, SPK_MAG = 20.0, 30.0, 0.5, 2.5, 2.1
SPK_MAG_D = 14.0
HAS_EAR = True
EAR_R, EAR_OUT, EAR_T = 1.2, 0.4, 0.5
SPK_T = SPK_NET + SPK_FRAME + SPK_MAG
SPK_CLR = SPK_CLR_
RIB = 1.0
POST_D_ = 6.0
# the finger's dish round each key, shallow: the keytops are proud
KEY_DISH = 0.4
# the mic's hole through the face, no tube: 1.0 printed shut
MIC_BORE = 2.0
# the keytops: a flange under the face (it keeps them in) on the plunger,
# a stem through the face's hole, 1.4 proud (the user, 2026-11-26: keytops 4.3 tall)
# printed holes come out small and pins large: 0.4 a side between them
# the keytops 5 wide, 4 tall (the user's), through a hole in the face 0.4
# bigger each side; a flange 0.4 past the hole each side under the face
CAP_PROUD, CAP_FLANGE_T, CAP_CLR = 1.4, 1.0, 0.3   # the flange holds the plunger's socket; 0.3 a side (the user's: 0.4 holes too big, 0.2 too tight)
CAP_W, CAP_H, CAP_R = 5.0, 4.0, 0.6
HOLE_W, HOLE_H = CAP_W + 2 * CAP_CLR, CAP_H + 2 * CAP_CLR
FLANGE_W, FLANGE_H = HOLE_W + 0.8, HOLE_H + 0.8
CAP_ROUND = 0.4                   # its top edges
# the display's glass, 16.5 tall in all with the 3 strip at its top (the
# user's measure; printed.py has it 14); the face stands clear over it
_gu0, _gu1, _gv0, _gv1 = P.OLED_GLASS
OLED_GLASS = (_gu0, _gu1, _gv1 - 16.5, _gv1)
RING_H = FRONT_H - 0.3            # down to just over the board
OPEN_IN = 2.0                            # the grille opens this far inside its rim
assert SPK_W == SPK_W_ and SPK_L == SPK_L_ and abs(SPK_T - SPK_T_) < 1e-9
SPK_X = bx(SPK_U)
# the middle of the height, unless SW3's body (its corner, SPK_CLR + 0.2
# off the speaker's round lower end) or H1's front post (its ring 0.1 off)
# holds it up
_r = SPK_W / 2
_ku, _kv = P.uv("SW3")[0] + 3.0, P.uv("SW3")[1] - 3.0
_v_sw3 = _kv - math.sqrt((_r + SPK_CLR + 0.2) ** 2 - (SPK_U - _ku) ** 2) - (SPK_L / 2 - _r)
_hu, _hv = P.uv("H1")
_v_h1 = _hv - math.sqrt((_r + SPK_CLR + RIB + POST_D_ / 2 + 0.1) ** 2 - (SPK_U - _hu) ** 2) - (SPK_L / 2 - _r)
SPK_V = math.floor(min(OH - WALL - CLR - OH / 2, _v_sw3, _v_h1) * 100) / 100
SPK_Y = by(SPK_V)
EAR_Y = SPK_Y + SPK_L / 2 + EAR_OUT        # the ear at the top
SPK_Z1 = Z_FACE_IN
SPK_Z0 = SPK_Z1 - SPK_T

# ---------------------------------------------------------------- cell
# BL-5C (above): its length across the case, against the top wall, over
# everything behind the board; under it at the bottom, H1's foot
CELL_T = CELL_T_
CELL_NOM = 5.5          # the user's BL-5C, measured (2026-11-27); CELL_T leaves room over it
RIDGE_T, RIDGE_H = 1.2, 2.0

# ---------------------------------------------------------------- screws
M3_CLEAR, M3_PILOT, M3_HEAD_D, M3_HEAD_H = P.M3_CLEAR, P.M3_PILOT, P.M3_HEAD_D, P.M3_HEAD_H
POST_D, STANDOFF_D, PILOT_SKIN = 6.0, 6.5, 1.0
# H1's and the hinge's: M3 x 16 (x 12 until 2026-11-26; the case 3.4 thicker
# for the BL-5C left an x 12 0.1 of thread in H1's post)
SCREW_L = 16
BOSS_D, BOSS_SKIN = 10.5, 0.8
SCREWS = [(bx(u), by(v)) for (u, v) in [P.uv("H1")]]     # H2 is sawn off
# the bottom-left corner (the user's, 2026-11-27: loose there, nothing held
# the halves): an M3 x SCREW_L from the back into a post on the front, in
# the strip the case grew by, left of the board's edge, below the cell
CORNER_SCREWS = [(3.7, 4.6)]
CORNER_POST_R = 2.4                      # its post on the front, clear of the board's edge
# the board held off the front by a prop on the face (the user's, 2026-11-27:
# it rose and the keytops held the key down): by R21, which is under the
# OLED's module, so left of the module, on the board's front over R21's row
FRONT_PROPS = [(8.1, 25.2)]
FRONT_PROP_R = 1.4
# and props at the corners it tipped about (the user's, 2026-11-27: the
# bottom-left dipped toward the face, the top-right rose): the bottom-left
# one off the corner's post, the top-right the top rib at LOCATE_X[1]
# carried in over the board's edge, right of the OLED's module
FRONT_PADS = [(5.0, 8.2, 2.2, 6.0), (LOCATE_X[1] - 0.75, LOCATE_X[1] + 0.75, 42.3, OH - WALL + 0.01)]
# the top-left post in the board's notch (empty), on the front, a locating
# pin now: its M3 is gone (the user's, 2026-11-26: the cell is under it),
# and the hooks on the top wall hold the halves there. Tied to the left wall
# across the strip the case grew by
TL_POST_R = 2.0
TL_V = 8.6
TL_Z = Z_SPLIT + TONGUE_H + 0.2      # the front's post down to here
TL = (bx(SAW_U) - CLR - 0.2 + TL_POST_R, by(TL_V))
assert bx(P.OLED_PCB[0]) - (TL[0] + TL_POST_R) >= 0.45, "the top-left post meets the display's module"
assert bx(P.NOTCH_U) >= TL[0] + TL_POST_R + 0.5 and by(P.NOTCH_V) <= TL[1] - TL_POST_R - 0.5
CX0 = WALL + CELL_CLR
CX1 = CX0 + CELL_L
CY1 = OH - WALL - CELL_CLR
CY0 = CY1 - CELL_W
CZ1 = BACK + CELL_T

# the keys exactly where the board's switches are
KEYS = list(P.KEYS)
for _ref, (_u, _v) in zip(("SW1", "SW2", "SW3", "SW4"), KEYS):
    assert (_u, _v) == P.uv(_ref), f"{_ref}'s hole is not on its switch"
# the plunger (3.0 across, the footprint) in a socket under each keytop, so
# it can't slide sideways (the user's); the keytop sits CAP_SOCKET lower on
# it, its top still CAP_PROUD over the face
CAP_SOCKET_D, CAP_SOCKET = 3.0 + 0.4, 1.0
assert CAP_SOCKET <= CAP_FLANGE_T
CAP_Z0 = Z_BF + KEY_TOP - CAP_SOCKET
# a collar round each key hole, rising off the face's inside (the user's
# ridges), down to 0.4 over the flange
COLLAR_T = 0.8
COLLAR_H = Z_FACE_IN - (CAP_Z0 + CAP_FLANGE_T) - 0.4
for _i, (_u, _v) in enumerate(KEYS):
    for (_u2, _v2) in KEYS[_i + 1:]:
        assert abs(_u - _u2) >= FLANGE_W + 0.5 or abs(_v - _v2) >= FLANGE_H + 0.5, "two keytops' flanges meet"
CAP_Z1 = D + CAP_PROUD


# ---------------------------------------------------------------- lever
# as on the microphone's case, but hinged near the middle of the right side
# above the jack (a plug's head, PLUG_HEAD across, clears its foot): on a column
# standing on the back; the lever one outline the full depth up the right
# side, printed on its back (its bar onto the PTT and its claw's tooth stand
# on the bed), the column's pocket open at its back with a 45 degree ceiling;
# an M3 screw from the face through the lever into the column, its head in
# a counterbore. At the
# top, a claw over the case's top: a tooth on it drops into a pocket in the
# back's top wall, the outward stop that keeps the lever on. A bar on the
# lever reaches in through the wall onto the PTT.
# the SuperMini sits USB_FIX further right than its footprint (the user's
# photo, 2026-11-27: the socket against the cutout's right end)
USB_FIX = 0.35          # halfway to the user's 0.7, USB_PLAY either way
USB_FACE_Y = by(BH) - 0.3     # the socket face, 0.3 past the board edge (the SuperMini model; its footprint drawing says 0.15 in)
PTT_X, PTT_Y, PTT_Z = bx(BW), by(P.PTT_V), z_back(P.PTT_UP)
JACK_Y = by(P.JACK_V)
# an air gap at rest between the bar's head and the plunger (the user's): the
# lever moves freely, then presses; PRELOAD is its negative
AIR_GAP = 0.4
PRELOAD = -AIR_GAP
# the PTT (SKRTLAE010) sits back from the board's edge under the board, its
# 1.9 square pedestal 1.7 off the board and not out past the edge: a bar on
# the lever's inside reaches in through a slot in the back's wall onto it,
# just under the board; it runs from the lever's back face (stiff), its top
# a 45 degree roof (printed face down that is its underside)
BAR_W, BAR_CLR, BAR_UNDER = 2.0, 0.3, 0.3     # across; round it in the slot; under the board
# the bar's end: a T head across the way it slides on the PTT's pedestal as
# the lever turns; under the head, 45 degrees down to the bar
HEAD_W = 4.0
# the head is the outward stop too (the user's): at rest its back sits
# HEAD_GAP off the wall's inside, the bar AIR_GAP off the plunger
HEAD_GAP = 0.1
HEAD_T = (OW - WALL - HEAD_GAP) - (PTT_X - PRELOAD)
# the bar passes over the cell (2026-11-26): its web comes off the lever's
# back face and its underside rises at 45 degrees (it prints) to pass the
# cell's end, then runs flat under the T head, a short ledge (BAR_LEDGE);
# the slot is through the back's wall only, no longer into the floor
FLOOR_SLOT = 0.0
# the bar swings round the hinge: pressed in BAR_TRAVEL at the switch it
# also slides toward the hinge, most at the slot's inner end; the slot is
# that much wider on the hinge's side
BAR_TRAVEL = 1.6
BAR_TOP = Z_BB - BAR_UNDER
# under the T head the wedge's foot, BAR_W wide: the bar's flat underside
# there, out to where its 45 degree underside, rising from the lever, passes
# 0.3 over the cell's end with the cell slid to the right wall, the lever
# pressed its whole BAR_TRAVEL and 0.3 more (2026-11-26, the user: the lever
# must not be obstructed)
BAR_UNDER_Z = PTT_Z - P.PTT_PLUNGER / 2 - 0.5 - (HEAD_W - BAR_W) / 2
BAR_LEDGE_X = (OW - WALL) + (BAR_TRAVEL + 0.3) - (BAR_UNDER_Z - (CZ1 + 0.3))
BAR_LEDGE = BAR_LEDGE_X - (PTT_X - PRELOAD)
assert 0 <= BAR_LEDGE, "the bar's 45 degree underside reaches the T head"
LEV_GAP = 2.0                     # the lever off the wall at rest, room to press in (the user: the bar longer than the switch-to-wall distance)
LEV_T, LEV_FLARE = 3.0, 2.0          # the paddle 5.0 thick
LEV_IN = OW + LEV_GAP
# the back's column: round, a 45 degree point on top, its neck to the wall
# a 45 degree roof; the lever's pocket round it the same, open at the
# lever's back (the bed), so its ceiling prints without a bridge
COL_R, COL_H = 3.2, 7.0           # the column's round part stands COL_H, then its point
NECK_W = 3.0
PIV_R = 5.2                       # the lever's boss: 1.6 of wall round the pocket; an M3 pivot screw
PLUG_HEAD = 10.0                  # a 3.5 mm plug's moulded head, across (ASSUMED)
PIV = (OW + PIV_R + 0.4, round(JACK_Y + PLUG_HEAD / 2 + 0.5 + PIV_R, 1))
LEV_TOP = OH - CORNER                # no claw: the T head holds the lever in (the user's)
POCKET_X1 = OW - WALL - 0.6         # the outward stop face
TOOTH_W = 1.5
HOOK_X0 = POCKET_X1 - TOOTH_W
LEV_CLR = 0.2                     # the lever round its column (the user's: it rattled at 0.4)
NECK_CLR = 0.3                    # round the column's neck, over the lever's whole swing
LEV_SCREW = 3.3                   # the lever turns on its M3: rattled at 3.4, tight at 3.2 (2026-11-27)

# ---------------------------------------------------------------- top-wall hooks
# (the user's, 2026-11-26: the top-left screw gone; only hooks on the top
# wall, at both ends; the right wall's are gone). Snap-fits, the back pushed
# straight on (2026-11-26: the slide-on hooks, 0.1 clear, never went in and
# one snapped; the back sat skewed on them): a tongue from the front, thin
# enough to flex, stands HOOK_C inside the top wall below the seam; its nose
# has a 45 degree lead-in below, which the back's wall pushes in as it
# comes, and a flat catch above, which springs out into a window through
# the back's wall. Where the board isn't against the top wall: at the left,
# the strip the case grew by and the board's notch, short of the top-left
# rib; at the right, past the board's edge to the corner
HOOK_T, HOOK_L, HOOK_C = 0.8, 5.5, 0.4     # thick, long below the seam, off the wall
HOOK_E = 0.5                               # the nose's catch into the wall
HOOK_P = HOOK_C + HOOK_E                   # the nose out from the tongue
HOOK_F = 0.4                               # the nose's flat, at full height
HOOK_WIN_Z, HOOK_WIN_X = 0.3, 0.4          # the window round the nose
HOOK_NOSES = [(WALL + IN_CORNER + 0.2, min(bx(P.NOTCH_U) - 0.5, LOCATE_X[0] - 0.75 - 0.3)),
              (bx(BW) + 0.4, OW - WALL - IN_CORNER - 0.2)]
HOOK_TIP_Z = Z_SPLIT - HOOK_L
HOOK_CATCH_Z = HOOK_TIP_Z + HOOK_P + HOOK_F
# the tongue's strain as the nose passes the wall: 1.5 x deflection x
# thickness / length^2, PLA's snaps ~2.5% at most
HOOK_STRAIN = 1.5 * HOOK_E * HOOK_T / HOOK_L ** 2
assert all(b - a >= 2.5 for a, b in HOOK_NOSES), HOOK_NOSES
assert HOOK_STRAIN <= 0.025, HOOK_STRAIN


def hook_noses():
    """The noses alone (for the check of the back going on, the tongues
    flexed): out from the tongue's outer face, 45 degrees below."""
    yt = OH - WALL - HOOK_C
    out = []
    for (a, b) in HOOK_NOSES:
        prof = [(yt - 0.01, HOOK_TIP_Z), (yt + HOOK_P, HOOK_TIP_Z + HOOK_P),
                (yt + HOOK_P, HOOK_CATCH_Z), (yt - 0.01, HOOK_CATCH_Z)]
        out.append(Pos(a, 0, 0) * Rot(90, 90, 0) * prism_xy(prof, 0, b - a))
    return out


def hook_front():
    zs, y1 = Z_SPLIT, OH - WALL - HOOK_C
    # above the seam the tongue joins the front's wall by a 45 degree gusset
    # (the front prints face down: a flat shelf there would hang)
    yi, yw = y1 - HOOK_T, OH - WALL + 0.4
    out = []
    for (a, b), nose in zip(HOOK_NOSES, hook_noses()):
        # built in (y, z) and swept along x: the profile's x is y, its y is z
        gusset = Pos(a, 0, 0) * Rot(90, 90, 0) * prism_xy(
            [(yi, zs), (yw, zs), (yw, zs + 0.5 + (yw - yi)), (yi, zs + 0.5)], 0, b - a)
        out.append(box(a, b, yi, y1, HOOK_TIP_Z, zs + 0.5).fuse(gusset).fuse(nose))
    return out


def hook_back_cuts():
    zw, xw = HOOK_WIN_Z, HOOK_WIN_X
    z0 = HOOK_CATCH_Z - HOOK_F - HOOK_E - zw      # the nose where it is into the wall
    return [box(a - xw, b + xw, OH - WALL - 0.01, OH + 1, z0, HOOK_CATCH_Z + zw)
            for (a, b) in HOOK_NOSES]


# ---------------------------------------------------------------- the cell held
# (the user's, 2026-11-26: it rattled, room over it): its top edge under two
# fixed lips on the top wall, its bottom edge snapped under two arms standing
# on the floor, their lips ramped 45 degrees on top. In: the top edge under
# the lips, the bottom edge pressed down past the arms. The lips' undersides
# CELL_HELD off the floor: the user's cell is 6.0 (a 6.2 one won't go under)
CELL_HELD = CELL_NOM + 0.1
LIP_T, LIP_OVER = 0.8, 0.8                 # the lips' thickness; over the cell, it in the middle
TOP_LIPS = [(16.0, 24.0), (30.0, 34.5)]     # x, clear of the tongues, J6 above is higher
ARM_T, ARM_OVER = 0.7, 0.5                 # the arms flex; their lips over the cell
ARMS = [(5.0, 13.0), (19.0, 27.0)]         # x, clear of the jack's barrel and H1's foot
ARM_STRAIN = 1.5 * (CELL_CLR + ARM_OVER) * ARM_T / (BACK + CELL_HELD + LIP_T - BACK) ** 2
assert ARM_STRAIN <= 0.025, ARM_STRAIN


# the board held up under the keys (the user's, 2026-11-27: the DOWN key
# pushed the board in, nothing under it since the top-left screw went):
# ribs off the bottom wall up to the board's back, either side of the
# SuperMini, between it and the 0805s at the left and the power switch at
# the right; the cell fills the rest
BOARD_RIBS = [(12.75, 14.55), (34.75, 36.0)]  # x (the right one off the SuperMini, moved USB_FIX)
BOARD_RIB_Y = 7.2                             # out from the wall, short of the cell's arms
BOARD_RIB_DROP = 0.8     # their tops under the board's back (the user's, 2026-11-27: at it, they held the seam 0.75 open)


# and along the top edge (the user's, 2026-11-27: the screws at the bottom
# drew it in there and its top rose off the front, the PTT missed the
# lever): ribs off the top wall over its back's edge, between the hooks'
# tongues, the 0805s and J6, hung from the wall on 45 degree gussets
TOP_RIBS = [(10.6, 15.0), (24.0, 27.0), (45.5, 50.8)]   # x
TOP_RIB_IN = 0.8                                        # over the board's back
TOP_RIB_Z = 9.0                                         # their foot, over the cell


def board_ribs():
    s = [box(a, b, WALL - 0.01, BOARD_RIB_Y, BACK - 0.5, Z_BB - BOARD_RIB_DROP) for a, b in BOARD_RIBS]
    y0, yw = by(0) - TOP_RIB_IN, OH - WALL + 0.01
    for a, b in TOP_RIBS:
        rib = box(a, b, y0, yw, TOP_RIB_Z, Z_BB)
        gus = Pos(a, 0, 0) * Rot(90, 90, 0) * prism_xy(
            [(y0, TOP_RIB_Z + 0.01), (yw, TOP_RIB_Z + 0.01), (yw, TOP_RIB_Z - (yw - y0))], 0, b - a)
        s.append(rib.fuse(gus))
    return s


def cell_holders():
    z = BACK + CELL_HELD
    g = CELL_CLR
    s = []
    for (a, b) in TOP_LIPS:
        y0 = CY1 + g - g - LIP_OVER                 # from the wall over the cell
        lip = box(a, b, y0, OH - WALL + 0.01, z, z + LIP_T)
        # its top a 45 degree gusset to the wall (it prints, and it is stiff)
        gus = Pos(a, 0, 0) * Rot(90, 90, 0) * prism_xy(
            [(y0, z + LIP_T - 0.01), (OH - WALL + 0.01, z + LIP_T - 0.01),
             (OH - WALL + 0.01, z + LIP_T + (OH - WALL - y0))], 0, b - a)
        s.append(lip.fuse(gus))
    for (a, b) in ARMS:
        yi = CY0 - g                                  # the arm's inner face at the pocket
        arm = box(a, b, yi - ARM_T, yi, BACK - 0.5, z + LIP_T)
        over = g + ARM_OVER
        # its lip: flat under, over the cell; 45 degrees on top, the ramp
        lip = Pos(a, 0, 0) * Rot(90, 90, 0) * prism_xy(
            [(yi - 0.01, z), (yi + over, z), (yi + over, z + 0.3), (yi - 0.01, z + 0.3 + over + 0.01)],
            0, b - a)
        s.append(arm.fuse(lip))
    return s


# ---------------------------------------------------------------- grille
G_PITCH, G_HOLE, G_EDGE, DIMPLE = 3.6, 2.4, 2.6, 0.6

# ---------------------------------------------------------------- checks
# every screw is M3 (the user's rule, case/CLAUDE.md): no other size's
# constants in this file
import re as _re
assert not _re.search(r"\bM[0-24-9]_[A-Z]", open(__file__).read()), "a screw that is not M3"
def copper_left_of(u_saw, margin=0.25):
    """Every pad, via and track of the produced board within margin of
    the saw line or left of it, H2's pad aside."""
    import re
    t = open(P.PCB).read()

    def uvxy(x, y):
        return P.KX1 - x, y - P.KY0
    bad = []
    for m in re.finditer(r'\(footprint "[^"]+"(.*?)\n\t\)', t, re.S):
        body = m.group(1)
        ref = re.search(r'\(property "Reference" "([^"]+)"', body).group(1)
        if ref == "H2":
            continue
        at = re.search(r'\n\t\t\(at ([-\d.]+) ([-\d.]+)(?: ([-\d.]+))?\)', body)
        x, y, rot = float(at.group(1)), float(at.group(2)), float(at.group(3) or 0)
        a = math.radians(rot)
        for pd in re.finditer(r'\(pad "[^"]*" \w+ \w+\s*\(at ([-\d.]+) ([-\d.]+)[^)]*\)\s*\(size ([\d.]+) ([\d.]+)\)', body):
            px, py, sw, sh = map(float, pd.groups())
            u, _ = uvxy(x + px * math.cos(a) + py * math.sin(a), y - px * math.sin(a) + py * math.cos(a))
            hw = sh if rot % 180 else sw
            if u - hw / 2 < u_saw + margin:
                bad.append((ref, round(u - hw / 2, 2)))
    for m in re.finditer(r'\(segment\s*\(start ([-\d.]+) [-\d.]+\)\s*\(end ([-\d.]+) [-\d.]+\)\s*\(width ([\d.]+)\)', t):
        x0, x1, w = map(float, m.groups())
        if min(P.KX1 - x0, P.KX1 - x1) - w / 2 < u_saw + margin:
            bad.append(("track", round(min(P.KX1 - x0, P.KX1 - x1), 2)))
    for m in re.finditer(r'\(via\s*\(at ([-\d.]+) [-\d.]+\)\s*\(size ([\d.]+)', t):
        if P.KX1 - float(m.group(1)) - float(m.group(2)) / 2 < u_saw + margin:
            bad.append(("via", round(P.KX1 - float(m.group(1)), 2)))
    return bad


_copper = copper_left_of(SAW_U)
assert not _copper, f"the saw line meets copper: {_copper}"
assert by(BH) >= WALL + 0.3, "the board meets the bottom wall"
assert EAR_Y + EAR_R + SPK_CLR <= OH - WALL + 1e-9, "the ear meets the top wall"
assert SPK_Y - SPK_L / 2 - SPK_CLR >= WALL, "the speaker meets the bottom wall"
assert SPK_X + SPK_W / 2 + SPK_CLR <= OW - WALL + 1e-9, "the speaker meets the right wall"
assert SPK_X - SPK_W / 2 - SPK_CLR >= bx(P.OLED_PCB[1]) + 0.3 - 1e-9, "the speaker meets the OLED"
assert Z_FACE_IN - RING_H >= Z_BF + 0.3 - 1e-9, "the ring meets the board"
assert SPK_Z0 >= Z_BF + SPK_BACK - 1e-9, "the speaker meets the board"
assert CAP_Z0 + CAP_FLANGE_T <= Z_FACE_IN - 0.05, "the keytop's flange reaches the face"
assert CZ1 + CELL_GAP <= Z_BB - UNDER_CELL + 1e-9, "the cell meets the parts over it"
assert CZ1 + CELL_GAP <= Z_BB - (JACK_UP + 2.9) + 1e-9, "the cell meets the jack"
assert CZ1 + CELL_GAP <= Z_BB - 4.6 + 1e-9, "the cell meets J6"
assert SCREWS[0][1] + STANDOFF_D / 2 + 0.3 <= CY0 - CELL_CLR + 1e-9, "H1's standoff meets the cell"
assert CX1 + CELL_CLR <= OW - WALL + 1e-9 and CY1 + CELL_CLR <= OH - WALL + 1e-9, "the cell meets a wall"
assert BACK + IN_D == Z_FACE_IN
assert BAR_TOP >= PTT_Z + P.PTT_PLUNGER / 2 + 0.2, "the bar's roof leaves the pedestal's top"
assert PIV[1] - PIV_R >= JACK_Y + PLUG_HEAD / 2 + 0.5 - 1e-9, "the lever meets the plug's head"
assert PIV_R - (COL_R + LEV_CLR) >= 1.2, "the lever's wall round its pocket"
assert BAR_LEDGE_X <= LEV_IN - 0.5, "the bar's ledge reaches the lever's face"


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
    zj, zu, zk = z_back(JACK_UP), z_back(P.USB_UP), z_back(P.KNOB_UP)
    xr0, xr1 = OW - WALL - 1, OW + 1
    yb0, yb1 = -1, WALL + 1
    ux, kx = bx(P.USB_U) + USB_FIX, bx(P.KNOB_U)
    w = BAR_W / 2 + BAR_CLR
    sw = BAR_TRAVEL * (PIV[0] - (OW - WALL - FLOOR_SLOT)) / (PTT_Y - PIV[1])
    slot = box(OW - WALL - FLOOR_SLOT, xr1, PTT_Y - w - sw, PTT_Y + w, -1, Z_SPLIT)
    top = Z_SPLIT + TONGUE_H + 0.5     # through the tongue too
    jr = JACK_BODY_D / 2
    return [
        (slot, box(xr0, xr1, PTT_Y - w - sw, PTT_Y + w, -1, top)),
        # a plain circle, the same one through both halves; nothing passes
        # the wall here, so no slot to the seam
        (cyl_x(xr0, xr1, JACK_Y, zj, jr), cyl_x(xr0, xr1, JACK_Y, zj, jr)),
        (hole_y(yb0, USB_FACE_Y - USB_MATE, ux, zu, USB_BODY_W, USB_BODY_H)
         + hole_y(yb0, USB_FACE_Y + 0.5, ux, zu, USB_SHELL_W, USB_SHELL_H),
         hole_y(yb0, USB_FACE_Y + 0.5, ux, zu, USB_SHELL_W, USB_SHELL_H)),
        (hole_y(yb0, yb1, kx, zk, P.KNOB_W, P.KNOB_H),
         box(kx - P.KNOB_W / 2, kx + P.KNOB_W / 2, yb0, yb1, zk, top)),
    ]



COL_Z = D - COL_H                    # the column's round part, from the face down to here


def column(grow=0.0):
    """The hinge's column outside the right wall, on the FRONT (the user's:
    the lever goes onto the front, with the PTT): it hangs from the face
    plane, round down to COL_Z, then a 45 degree point; its neck to the wall
    a 45 degree point too. The front prints face down, so the column stands
    on the bed and its point is up. Grown, it is the lever's pocket round
    it, open at the lever's face side."""
    r = COL_R + grow
    z = COL_Z - grow
    c = (cyl_z(PIV[0], PIV[1], r, z, D + 1)
         + cone_z(PIV[0], PIV[1], 0.01, r, z - r, z + 0.01))
    w = NECK_W + 2 * grow
    x0 = OW - 1.0 - grow
    neck = (box(x0, PIV[0], PIV[1] - w / 2, PIV[1] + w / 2, z, D + 1)
            + Pos(x0, PIV[1], z) * Rot(45, 0, 0)
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
    return box(POCKET_X1 - 3.5, POCKET_X1, OH - WALL + 0.3, OH + 1, -1, Z_SPLIT + TONGUE_H + 0.5)


# ---------------------------------------------------------------- grille
def lattice():
    # laid out on the case as it was before it grew LEFT_GROW on the left,
    # moved with the board: the holes over the speaker as they were (the
    # user's, 2026-11-26: centred on the wider case, the lowest row was one
    # orphan hole)
    ow = OW - LEFT_GROW
    nx = int((ow - 2 * G_EDGE) // G_PITCH) + 1
    ny = int((OH - 2 * G_EDGE) // G_PITCH) + 1
    x0 = LEFT_GROW + (ow - (nx - 1) * G_PITCH) / 2
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
        if any(math.hypot(x - bx(u), y - by(v)) < P.DISH_D / 2 + hr + 0.8 for u, v in KEYS):
            continue
        if math.hypot(x - mx, y - my) < hr + 1.5:
            continue
        if x + hr > POCKET_X1 - 3.5 - 0.6 and y + hr > OH - WALL + 0.3 - 0.6:
            continue
        if any(math.hypot(x - sx, y - sy) < M3_PILOT / 2 + hr + 0.6 for sx, sy in SCREWS + CORNER_SCREWS + [TL]):
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
        ring -= ear(SPK_CLR, ff - RING_H - 1, ff + 1)
    # no ring between the speaker and the display's module, nor over the keys
    x0, x1, v0, v1 = P.OLED_PCB
    ring -= box(bx(x0) - 0.3, bx(max(x1, OLED_GLASS[1])) + 0.2, by(v1) - 1, by(v0) + 1, 0, D)
    for (u, v) in KEYS:
        ring -= box(bx(u) - FLANGE_W / 2 - 0.4, bx(u) + FLANGE_W / 2 + 0.4,
                    by(v) - FLANGE_H / 2 - 0.4, by(v) + FLANGE_H / 2 + 0.4, 0, D)   # the flange 0.4 clear
    s += ring & inside(Z_SPLIT, D, 0.5)

    for (x, y) in SCREWS:
        s += cyl_z(x, y, POST_D / 2, Z_BF, ff + 0.5)
    for (x, y) in CORNER_SCREWS:
        s += cyl_z(x, y, CORNER_POST_R, Z_SPLIT, ff + 0.5) & inside(0, D, 0.5)
    for (x, y) in FRONT_PROPS:
        s += cyl_z(x, y, FRONT_PROP_R, Z_BF, ff + 0.5)
    for (a, b, c, d) in FRONT_PADS:
        s += box(a, b, c, d, Z_BF, ff + 0.5) & inside(0, D, 0.5)
    s += cyl_z(TL[0], TL[1], TL_POST_R, TL_Z, ff + 0.5) + box(0, TL[0], TL[1] - TL_POST_R, TL[1] + TL_POST_R, TL_Z, ff + 0.5)
    mx, my = bx(P.MIC_UV[0]), by(P.MIC_UV[1])


    # ---- cuts
    # the speaker's room, its clearance round it
    s -= obround(SPK_X, SPK_Y, SPK_W + 2 * SPK_CLR, SPK_L + 2 * SPK_CLR, SPK_Z0 - 1, ff)
    # room for the back's tongue
    if TONGUE_T > 0:
        s -= inside(Z_SPLIT - 1, Z_SPLIT + TONGUE_H + 0.2, TONGUE_T + TONGUE_CLR)
    for (x, y) in SCREWS + CORNER_SCREWS:
        s -= cyl_z(x, y, M3_PILOT / 2, Z_SPLIT - 6, D - PILOT_SKIN)
    s -= cyl_z(mx, my, MIC_BORE / 2, ff - 1, D + 1)

    holes, dimples = grille()
    hr = G_HOLE / 2
    for (x, y) in holes:
        s -= cyl_z(x, y, hr, ff - 1, D + 1)
    # no dimples: the grille is over the speaker only

    x0, x1, v0, v1 = P.OLED_WIN
    s -= box(bx(x0), bx(x1), by(v1) + WIN_UP, by(v0) + WIN_UP, ff - 1, D + 1)
    for (u, v) in KEYS:
        x, y = bx(u), by(v)
        s -= rrect(x - HOLE_W / 2, y - HOLE_H / 2, HOLE_W, HOLE_H, CAP_R + CAP_CLR, ff - 1, D + 1)
        hw, hh = HOLE_W / 2 + COLLAR_T, HOLE_H / 2 + COLLAR_T
        s += (rrect(x - hw, y - hh, 2 * hw, 2 * hh, CAP_R + CAP_CLR + COLLAR_T, Z_FACE_IN - COLLAR_H, Z_FACE_IN + 0.01)
              - rrect(x - HOLE_W / 2, y - HOLE_H / 2, HOLE_W, HOLE_H, CAP_R + CAP_CLR, ff - 2, D + 1))

    for hole, _ in ports():
        s -= hole
    # ribs on the top and bottom walls to LOCATE_GAP off the board's edges, near
    # each end: the board, on one screw, can't slide or turn (the user's photo:
    # 0.8 free at the bottom)
    for x in LOCATE_X:
        # up to the face's inside, so no free end overhangs as printed face down
        s += box(x - 0.75, x + 0.75, WALL - 0.01, by(BH) - LOCATE_GAP, Z_SPLIT, Z_FACE_IN + 0.01)
        s += box(x - 0.75, x + 0.75, by(0) + LOCATE_GAP, OH - WALL + 0.01, Z_SPLIT, Z_FACE_IN + 0.01)
    # the hinge's column (the back's wall and tongue below the seam are left
    # alone), the screw's pilot up it from its point, closed at the face
    s += ((column() & box(0, OW + 20, -20, OH + 20, 0, D)) - box(-1, OW, -1, OH + 1, -1, Z_SPLIT + TONGUE_H + 0.3))
    s -= cyl_z(PIV[0], PIV[1], M3_PILOT / 2, COL_Z - COL_R - 1, D - PILOT_SKIN)
    for tongue in hook_front():
        s = s.fuse(tongue)
    return s


# ---------------------------------------------------------------- back
def back_tray():
    out = outside(Z_SPLIT, 0)
    out = fillet(out.faces().sort_by(Axis.Z)[0].edges(), FACE_R)
    s = out - inside(BACK, Z_SPLIT + 1)
    if TONGUE_T > 0:
        s += inside(Z_SPLIT - 0.01, Z_SPLIT + TONGUE_H, TONGUE_T) - inside(Z_SPLIT - 1, D)
    floor = BACK - 0.5

    for (x, y) in SCREWS:
        s += cyl_z(x, y, STANDOFF_D / 2, floor, Z_BB)
        top = M3_HEAD_H + BOSS_SKIN
        foot = (cyl_z(x, y, BOSS_D / 2, floor, top)
                + cone_z(x, y, BOSS_D / 2, STANDOFF_D / 2, top - 0.01, top + (BOSS_D - STANDOFF_D) / 2))
        # kept off the cell, which lies over its top side
        foot -= box(CX0 - CELL_CLR, CX1 + CELL_CLR, CY0 - CELL_CLR, CY1 + CELL_CLR, 0, D)
        s += foot & inside(0, D, 0.5)

    for (x, y) in CORNER_SCREWS:
        s += cyl_z(x, y, STANDOFF_D / 2, floor, Z_SPLIT) & inside(0, D, 0.5)
        top = M3_HEAD_H + BOSS_SKIN
        foot = (cyl_z(x, y, BOSS_D / 2, floor, top)
                + cone_z(x, y, BOSS_D / 2, STANDOFF_D / 2, top - 0.01, top + (BOSS_D - STANDOFF_D) / 2))
        # off the cell, and off the snap arms' feet, which flex
        foot -= box(CX0 - CELL_CLR, CX1 + CELL_CLR, CY0 - CELL_CLR - ARM_T - 0.5, OH, 0, D)
        s += foot & inside(0, D, 0.5)

    # the cell: an outline ridge round its foot
    g = CELL_CLR
    ridge = (box(CX0 - g - RIDGE_T, CX1 + g + RIDGE_T, CY0 - g - RIDGE_T, CY1 + g + RIDGE_T,
                 floor, BACK + RIDGE_H)
             - box(CX0 - g, CX1 + g, CY0 - g, CY1 + g, 0, D))
    for (a, b) in ARMS:          # the arms flex from the floor, the ridge kept off them
        ridge -= box(a - 0.5, b + 0.5, -1, CY0, -1, D)
    s += ridge & inside(0, D, 0.5)
    for h in cell_holders():
        s = s.fuse(h & inside(0, D, 0.5))
    for rb in board_ribs():
        s = s.fuse(rb & inside(0, D, 0.5))



    # ---- cuts
    for (x, y) in SCREWS + CORNER_SCREWS:
        s -= cyl_z(x, y, M3_CLEAR / 2, -1, D)
        s -= head_pocket(x, y)
    # the USB's skin: the wall thickened inward to just short of the socket's
    # face, below the seam (the board's edge is above it)
    ux, zu = bx(P.USB_U) + USB_FIX, z_back(P.USB_UP)
    s += box(ux - USB_BODY_W / 2 - 1.0, ux + USB_BODY_W / 2 + 1.0, WALL - 0.01, USB_FACE_Y - USB_SKIN_GAP,
             BACK - 0.01, Z_SPLIT)
    # clear of the jack's body (5.8 deep, its barrel JACK_UP off the board): H1's foot
    s -= box(bx(BW) - 0.1 - 2.0 - 11.8 - 0.3, bx(BW) + 0.3, JACK_Y - 2.9 - 0.3, JACK_Y + 2.9 + 0.3,
             Z_BB - JACK_UP - 2.9 - 0.3, Z_BB)
    for hole, notch in ports():
        s -= hole + notch
    for cut in hook_back_cuts():
        s -= cut
    return s


# ---------------------------------------------------------------- lever
def lever_pocket():
    """Round the column LEV_CLR (no rattle); round its neck NECK_CLR, swept
    over the lever's swing (pressed in to ~8 degrees, out 1), as the neck
    swings across the pocket when the lever turns."""
    r, z = COL_R + LEV_CLR, COL_Z - LEV_CLR
    p = (cyl_z(PIV[0], PIV[1], r, z, D + 1)
         + cone_z(PIV[0], PIV[1], 0.01, r, z - r, z + 0.01))
    g = NECK_CLR
    zn, w, x0 = COL_Z - g, NECK_W + 2 * g, OW - 1.0 - g      # column()'s neck, grown g
    neck = (box(x0, PIV[0], PIV[1] - w / 2, PIV[1] + w / 2, zn, D + 1)
            + Pos(x0, PIV[1], zn) * Rot(45, 0, 0)
            * Box(PIV[0] - x0, w / 2 ** 0.5, w / 2 ** 0.5, align=(Align.MIN, Align.CENTER, Align.CENTER)))
    for a in (-8.0, -6.0, -4.0, -2.0, 0.0, 1.0):
        p += Pos(PIV[0], PIV[1], 0) * Rot(0, 0, a) * Pos(-PIV[0], -PIV[1], 0) * neck
    return p


def lever():
    """As the microphone's, printed on its back: the bar onto the PTT and
    the claw's tooth stand on the bed; the hinge's pocket opens on the bed,
    its ceiling 45 degrees over the column's point."""
    L, T, F = LEV_IN, LEV_T, LEV_FLARE
    s = prism_xy([(L, PIV[1]), (L + T, PIV[1]), (L + T, PIV[1] + 3.0), (L + T + F, PIV[1] + 11.0),
                  (L + T + F, LEV_TOP), (L, LEV_TOP)], 0, D)
    s += cyl_z(PIV[0], PIV[1], PIV_R, 0, D)
    try:
        s = fillet(s.edges().filter_by(Axis.Z), 1.5)
    except Exception:
        pass
    # the front's column's pocket, open at the face side (the top as
    # printed, its floor the column's point); the screw from the back, its
    # head in a pocket on the bed closed by a 45 degree cone onto the
    # clearance, so the head bears on a full ring
    s -= lever_pocket()
    s -= cyl_z(PIV[0], PIV[1], LEV_SCREW / 2, -1, D + 1)
    s -= head_pocket(PIV[0], PIV[1])
    # the bar onto the PTT: from the lever's back face through the wall, a
    # 45 degree roof; its underside rises at 45 degrees from the lever to
    # pass the cell's end, then runs flat under the T head (BAR_LEDGE)
    x0 = PTT_X - PRELOAD
    roof = BAR_W / 2
    zt = BAR_TOP - roof
    zp = BAR_UNDER_Z - (L + 0.5 - BAR_LEDGE_X)      # the underside at the lever's face
    side = prism_xy([(x0, BAR_UNDER_Z), (BAR_LEDGE_X, BAR_UNDER_Z), (L + 0.5, max(zp, 0.0)),
                     (L + 0.5, zt), (x0, zt)], -BAR_W / 2, BAR_W / 2)
    s += Pos(0, PTT_Y, 0) * Rot(90, 0, 0) * side
    s += (Pos(x0, PTT_Y, zt) * Rot(45, 0, 0)
          * Box(L + 0.5 - x0, BAR_W / 2 ** 0.5, BAR_W / 2 ** 0.5,
                align=(Align.MIN, Align.CENTER, Align.CENTER)))
    # its T head on the pedestal, flat on top; under it 45 degrees in to the bar
    hz0 = PTT_Z - P.PTT_PLUNGER / 2 - 0.5
    hz1 = BAR_TOP
    taper = (HEAD_W - BAR_W) / 2
    s += box(x0, x0 + HEAD_T, PTT_Y - HEAD_W / 2, PTT_Y + HEAD_W / 2, hz0, hz1)
    wedge = prism_xy([(-HEAD_W / 2, hz0), (HEAD_W / 2, hz0), (BAR_W / 2, hz0 - taper),
                      (-BAR_W / 2, hz0 - taper)], 0, HEAD_T)
    s += Pos(x0, PTT_Y, 0) * Rot(0, 0, 90) * Rot(90, 0, 0) * wedge
    return s


# ---------------------------------------------------------------- keytops
def keytops():
    """One on each key, printed on its flange: the flange on the plunger
    under the face, the stem through the face's hole, 1.4 proud."""
    s = None
    for (u, v) in KEYS:
        x, y = bx(u), by(v)
        top = rrect(x - CAP_W / 2, y - CAP_H / 2, CAP_W, CAP_H, CAP_R, CAP_Z0 + CAP_FLANGE_T - 0.01, CAP_Z1)
        top = fillet(top.faces().sort_by(Axis.Z)[-1].edges(), CAP_ROUND)
        k = rrect(x - FLANGE_W / 2, y - FLANGE_H / 2, FLANGE_W, FLANGE_H, CAP_R + 0.8,
                  CAP_Z0, CAP_Z0 + CAP_FLANGE_T) + top
        # the plunger's socket, all in the flange (the stem's walls round it
        # would be too thin); its flat roof, a 3.4 bridge, sits on the plunger
        k -= cyl_z(x, y, CAP_SOCKET_D / 2, CAP_Z0 - 1, CAP_Z0 + CAP_SOCKET)
        s = k if s is None else s + k
    # one part: the frame fused to every keytop (a list + a solid only lists them)
    u = key_frame()
    for k in (s if isinstance(s, (list, tuple)) else s.solids()):
        u = u.fuse(k)
    return u.clean()


# the user's sketch: a frame round the row of three keys, each tied to its
# top bar by one link; the fourth key hangs from its bottom bar. The bars,
# long and thin, bend so a key presses alone; the frame stops them turning
FRAME_W, FRAME_T, FRAME_GAP = 0.8, 0.6, 0.45


def key_frame():
    ys = sorted({round(by(v), 1) for (_, v) in KEYS})
    row = [(bx(u), by(v)) for (u, v) in KEYS if abs(by(v) - ys[-1]) < 1]
    lone = [(bx(u), by(v)) for (u, v) in KEYS if abs(by(v) - ys[-1]) >= 1]
    assert len(row) == 3 and len(lone) == 1, "the keys aren't a row of three and one"
    (lx, ly), ry = lone[0], max(y for _, y in row)
    W_, G = FRAME_W, FRAME_GAP
    x0 = min(x for x, _ in row) - FLANGE_W / 2 - G - W_
    yb0 = ly + FLANGE_H / 2 + G                    # the bottom bar, between the lone key and the row
    wb = min(W_, min(y for _, y in row) - FLANGE_H / 2 - G - yb0)
    assert wb >= 0.6, f"the bottom bar would be {wb:.2f} wide"
    yt1 = ry + FLANGE_H / 2 + G + W_
    z0, z1 = CAP_Z0, CAP_Z0 + FRAME_T
    # open on the right (the speaker is there): a U of the left, top and bottom bars
    xr = max(x for x, _ in row) + W_ / 2           # the top bar to the right key's link
    f = (box(x0, x0 + W_, yb0, yt1, z0, z1)
         + box(x0, xr, yt1 - W_, yt1, z0, z1)
         + box(x0, lx + W_ / 2, yb0, yb0 + wb, z0, z1))
    for (x, y) in row:
        f += box(x - W_ / 2, x + W_ / 2, y + FLANGE_H / 2 - 0.3, yt1 - W_ + 0.01, z0, z1)
    f += box(lx - W_ / 2, lx + W_ / 2, ly + FLANGE_H / 2 - 0.3, yb0 + 0.01, z0, z1)
    return f


# ---------------------------------------------------------------- parts
def parts():
    board = (box(bx(SAW_U), bx(BW), by(BH), by(0), Z_BB, Z_BF)
             - box(bx(-1), bx(P.NOTCH_U), by(P.NOTCH_V), by(-1), Z_BB - 1, Z_BF + 1))
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
    jz = z_back(JACK_UP)
    jack = (box(bx(BW) - 0.1 - 2.0 - 11.8, bx(BW) - 0.1 - 2.0, JACK_Y - 2.9, JACK_Y + 2.9, Z_BB - JACK_UP - 2.9, Z_BB)   # to its barrel's far side
            + cyl_x(bx(BW) - 2.2, bx(BW) - 0.1, JACK_Y, jz, 2.3))
    ux, uz = bx(P.USB_U) + USB_FIX, z_back(P.USB_UP)
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
    cell = box(CX0, CX1, CY0, CY1, BACK, BACK + CELL_NOM)
    j6 = box(bx(42.3), bx(51.5), by(10.2), by(0.0), Z_BB - 4.6, Z_BB)    # JST PH, ~4.6 (ASSUMED)
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
            (j6, "j6", (0.95, 0.95, 0.9))]


SHELL = (0.30, 0.32, 0.35)
LEVER = (0.8, 0.15, 0.1)


def check(front, back, lev, caps):
    for name, p in (("front", front), ("back", back), ("lever", lev)):
        print(f"{name}: {len(p.solids())} solid(s)")
    shells = [(front, "front"), (back, "back"), (lev, "lever"), (caps, "keytops")]
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
            if a in things and b in things and not ({a[1], b[1]} & {"cell", "speaker", "j6"}):
                continue
            i = whole(a[0]).intersect(whole(b[0]))
            v = sum(s.volume for s in i.solids()) if i else 0
            if v > 0.05:
                print(f"  {a[1]} meets {b[1]}: {v:.2f} mm3")
                bad += 1
    print("clear" if not bad else f"{bad} overlaps")


def test_piece(front):
    """The face alone, inside the walls: the display's window, the keys'
    holes, the speaker's ring and grille, H1's post and the mic's tube (it
    stands on the board on those two). Printed face down, held on the
    board with the keytops in."""
    return front & box(WALL + 0.3, OW - WALL - 0.3, WALL + 0.3, OH - WALL - 0.3, Z_BF, D + 1)


def report_changes():
    """Every dimension (the module's upper-case numbers) against the last
    export's, so anything changed without being asked shows."""
    now = {}
    for k, v in sorted(globals().items()):
        if k.isupper() and not k.startswith("_"):
            if isinstance(v, (int, float)) and not isinstance(v, bool):
                now[k] = round(float(v), 3)
            elif isinstance(v, (tuple, list)) and v and all(isinstance(x, (int, float)) for x in v):
                now[k] = [round(float(x), 3) for x in v]
            elif isinstance(v, (tuple, list)) and v and all(isinstance(x, (tuple, list)) for x in v):
                now[k] = [[round(float(y), 3) for y in x] for x in v]
    path = os.path.join(OUT, "params.json")
    try:
        before = json.load(open(path))
    except (OSError, ValueError):
        before = None
    if before is not None:
        changed = [k for k in sorted(set(now) | set(before)) if now.get(k) != before.get(k)]
        print("changed since the last export:" if changed else "no dimension changed since the last export")
        for k in changed:
            print(f"  {k}: {before.get(k)} -> {now.get(k)}")
    json.dump(now, open(path, "w"), indent=1)


def main():
    print(f"box {OW:.1f} wide, {OH:.1f} tall, {D:.1f} thick "
          f"({PIV[0] + PIV_R:.1f} wide with the lever); "
          f"inside {IW} x {IH} x {IN_D}; speaker centre u {SPK_U:.2f}, v {SPK_V:.2f}; "
          f"hinge {PIV[1]:.1f} up (the jack {JACK_Y:.1f})")
    front, back, lev, caps = front_shell(), back_tray(), lever(), keytops()
    if "check" in sys.argv:
        check(front, back, lev, caps)
        return
    # to print: the face down, the back and the lever on their backs
    export_stl(Pos(0, 0, D) * Rot(180, 0, 0) * front, os.path.join(OUT, "front.stl"))
    export_stl(back, os.path.join(OUT, "back.stl"))
    export_stl(lev, os.path.join(OUT, "lever.stl"))
    export_stl(whole(caps).translate((0, 0, -CAP_Z0)), os.path.join(OUT, "keytops.stl"))
    # no test pieces (the user's, 2026-11-27: only the parts to print)
    print(f"wrote {OUT}/front.stl back.stl lever.stl keytops.stl")
    report_changes()
    pieces = ([(front, "front", SHELL), (back, "back", SHELL), (lev, "lever", LEVER),
               (caps, "keytops", (0.9, 0.9, 0.9))] + parts())
    os.makedirs(os.path.join(OUT, "pieces"), exist_ok=True)
    for shape, name, rgb in pieces:
        glb([(shape, name, rgb)], os.path.join(OUT, "pieces", f"{name}.glb"))
    with open(os.path.join(OUT, "pieces", "list.json"), "w") as f:
        json.dump([n for _, n, _ in pieces], f)


if __name__ == "__main__":
    main()
