#!/usr/bin/env python3
"""Every check this case has needed, with pass/fail limits, so a change can't
quietly undo an earlier fix. Run after each export: `python verify.py`.
Each line says PASS or FAIL and what it measured; the exit code is the
number of failures."""
import math
import os
import re
import struct
import sys

import numpy as np
from build123d import Pos, Rot, Vector

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import wbitx2 as W  # noqa: E402
from printed import whole, box, cyl_z, cyl_x, prism_xy, rrect  # noqa: E402

HERE = os.path.dirname(os.path.abspath(__file__))
fails = []


def check(name, ok, got):
    print(("PASS " if ok else "FAIL ") + name + ": " + got)
    if not ok:
        fails.append(name)


def vol(a, b):
    i = whole(a).intersect(whole(b))
    return sum(s.volume for s in i.solids()) if i else 0.0


def gap(a, b):
    return whole(a).distance_to(whole(b))


front, back, lev, caps = W.front_shell(), W.back_tray(), W.lever(), whole(W.keytops())
P = {n: whole(s) for s, n, _ in W.parts()}
SHELLS = {"front": front, "back": back, "lever": lev, "keytops": caps}

# ---- the user's standing rules
src = open(os.path.join(HERE, "wbitx2.py")).read()
check("screws are M3 only", not re.search(r"\bM[0-9](?!3)\b|M2_|M2\b", src.replace("M3", "")), "no other size named")
check("mic: a hole only, 2 mm", abs(W.MIC_BORE - 2.0) < 1e-6, f"bore {W.MIC_BORE}")
check("keytops 5 x 4", (W.CAP_W, W.CAP_H) == (5.0, 4.0), f"{W.CAP_W} x {W.CAP_H}")
check("keytops 1.4 proud", abs(W.CAP_Z1 - W.D - 1.4) < 1e-6, f"{W.CAP_Z1 - W.D:.2f}")
check("no lip at the seam (the user's)", W.TONGUE_T == 0, f"TONGUE_T {W.TONGUE_T}")
# the BL-5C (the user's, 2026-11-26): 54 x 34, 6.2 deep for the thicker ones
check("cell pocket BL-5C, 54 x 34 x 6.2, 0.4 round",
      (W.CELL_L, W.CELL_W, W.CELL_T, W.CELL_CLR) == (54.0, 34.0, 6.2, 0.4),
      f"{W.CELL_L} x {W.CELL_W} x {W.CELL_T}, {W.CELL_CLR}")
check("the case grew on the left; deep enough for a 20 mm speaker (the user's)",
      abs(W.LEFT_GROW - 4.7) < 1e-6 and W.Z_FACE_IN - W.BACK >= 20.5 - 1e-6, f"left {W.LEFT_GROW}, {W.D:.1f} thick")
check("no magnets, no top-left screw (the user's)", not hasattr(W, "MAGNETS") and len(W.SCREWS) == 1,
      f"{len(W.SCREWS)} screw")

# ---- nothing overlaps (allowed: the bar's preload on the PTT, keytops on their plungers)
allowed = {("lever", "ptt")}
over = []
names = list(SHELLS) + list(P)
objs = {**SHELLS, **P}
for i, a in enumerate(names):
    for b in names[i + 1:]:
        if (a, b) in allowed or (b, a) in allowed:
            continue
        v = vol(objs[a], objs[b])
        if v > 0.01:
            over.append(f"{a}/{b} {v:.2f}")
check("no overlaps", not over, ", ".join(over) or "none")

# ---- screws: heads can't fall through; M3 x SCREW_L fits each
def seat(part, x, y, z):
    worst = 9.0
    for k in range(8):
        a0, a1 = k * math.pi / 4, (k + 1) * math.pi / 4
        sec = (prism_xy([(x, y)] + [(x + 2.75 * math.cos(a0 + (a1 - a0) * i / 6),
                                     y + 2.75 * math.sin(a0 + (a1 - a0) * i / 6)) for i in range(7)],
                        z, z + 0.6) - cyl_z(x, y, 1.8, 0, 30))
        worst = min(worst, vol(part, sec))
    return worst


for name, (x, y), part, z in (("H1", W.SCREWS[0], back, W.HEAD_SEAT),
                              ("hinge", W.PIV, lev, W.LEV_Z0 + W.LEV_HEAD)):
    w = seat(part, x, y, z)
    check(f"{name} head held all round", w > 0.05, f"least material under the head {w:.2f} mm3")


def pilot_run(x, y):
    r = W.M3_PILOT / 2 + 0.1
    zs = [z for z in np.arange(0, W.D, 0.05)
          if front.is_inside(Vector(x + r, y, z)) and front.is_inside(Vector(x - r, y, z))
          and not front.is_inside(Vector(x, y, z))]
    return min(zs), max(zs)


for name, (x, y), need in (("H1", W.SCREWS[0], 3.4), ("hinge", W.PIV, 5.0)):
    z0, z1 = pilot_run(x, y)
    L = W.SCREW_L if name == "H1" else W.HINGE_L
    tip = (W.HEAD_SEAT if name == "H1" else W.LEV_Z0 + W.LEV_HEAD) + L
    check(f"{name}: M3 x {L} fits", tip <= z1 - 0.2 and tip - z0 >= need,
          f"tip {tip:.1f}, pilot {z0:.1f}..{z1:.1f}, thread {tip - z0:.1f}")

# ---- the lever: rest, stop, press, room, slot swing
arm = W.PTT_Y - W.PIV[1]


def turned(t):
    return Pos(W.PIV[0], W.PIV[1], 0) * Rot(0, 0, math.degrees(t / arm)) * Pos(-W.PIV[0], -W.PIV[1], 0) * lev


for t, what in ((0.0, "at rest"), (0.35, "pressed 0.35"), (0.8, "pressed 0.8")):
    lp = turned(t)
    hit = {n: round(vol(lp, o), 2) for n, o in (("front", front), ("back", back), ("board", P["board"]),
                                                 ("cell", P["cell"]))}
    check(f"lever {what} touches nothing but the PTT", max(hit.values()) <= 0.01, str(hit))
# the lever not obstructed (the user's): pressed its whole travel, the cell
# where it is and slid 0.4 to the right wall
_slid = Pos(W.CELL_CLR, 0, 0) * P["cell"]
check("lever's T head stops it on the wall", 0.05 <= gap(lev, back) <= 0.15, f"head to wall {gap(lev, back):.2f}")
check("an air gap at rest between the bar and the plunger (the user's)", 0.3 <= gap(lev, P["ptt"]) <= 0.5,
      f"{gap(lev, P['ptt']):.2f}")

# the hinge doesn't rattle (the user's): tight round the column and the screw
check("lever 0.2 round its column, 3.3 hole on its M3 (3.4 rattled, 3.2 tight)", W.LEV_CLR == 0.2 and W.LEV_SCREW == 3.3,
      f"{W.LEV_CLR}, {W.LEV_SCREW}")
# the display's cutout 1 mm toward the glass's top (the user's)
check("display cutout moved up 1 mm", W.WIN_UP == 1.0, f"{W.WIN_UP}")

# ---- keytops
check("keytops 0.3 clear of the front", 0.28 <= gap(caps, front) <= 0.32, f"{gap(caps, front):.2f}")
check("keytops sit on their plungers", gap(caps, P["keys"]) < 0.01, f"{gap(caps, P['keys']):.2f}")
check("keytop flange clears the switch body when pressed",
      W.CAP_Z0 - (W.Z_BF + W.KEY_BODY) >= 0.5, f"{W.CAP_Z0 - (W.Z_BF + W.KEY_BODY):.2f}")
check("plunger socket in the flange, 0.2 a side", W.CAP_SOCKET <= W.CAP_FLANGE_T and abs(W.CAP_SOCKET_D - 3.4) < 1e-6,
      f"{W.CAP_SOCKET_D} across, {W.CAP_SOCKET} deep in a {W.CAP_FLANGE_T} flange")
check("keytops and their frame are one part", len(caps.solids()) == 1, f"{len(caps.solids())} solid(s)")
fr = W.key_frame()
check("the frame clears the front, board, speaker, OLED, switches by >= 0.4",
      min(gap(fr, o) for o in (front, P["board"], P["speaker"], P["oled"], P["keys"])) >= 0.4,
      f"{min(gap(fr, o) for o in (front, P['board'], P['speaker'], P['oled'], P['keys'])):.2f}")
# the measured stack (flange at 3.8, face inside 5.5) leaves 0.3 for it
check("collar round each key hole", W.COLLAR_H >= 0.25, f"{W.COLLAR_H:.1f} tall")

# ---- the display isn't pressed; the board is clamped at H1 only
check("OLED clear of the front", gap(P["oled"], front) >= 0.2, f"{gap(P['oled'], front):.2f}")
check("glass clear of the front", gap(P["glass"], front) >= 0.19, f"{gap(P['glass'], front):.2f}")
far = whole(P["board"]) - cyl_z(W.SCREWS[0][0], W.SCREWS[0][1], 4.0, -5, 30)
for a, b in W.BOARD_RIBS:
    far = far - box(a - 0.5, b + 0.5, -5, W.BOARD_RIB_Y + 0.5, -5, 30)
for x, y in W.FRONT_PROPS:
    far = far - cyl_z(x, y, W.FRONT_PROP_R + 0.5, -5, 30)
for a, b in W.TOP_RIBS:
    far = far - box(a - 0.5, b + 0.5, W.by(0) - W.TOP_RIB_IN - 0.5, 60, -5, 30)
for a, b, c, d in W.FRONT_PADS:
    far = far - box(a - 0.5, b + 0.5, c - 0.5, d + 0.5, -5, 30)
for x, y in W.CORNER_SCREWS:      # its standoff under the board's corner
    far = far - cyl_z(x, y, W.STANDOFF_D / 2 + 0.5, -5, 30)
check("board touches the case only at H1 and, behind it, its ribs (ribs 0.1 off its edges)",
      min(gap(far, front), gap(far, back)) >= 0.08,
      f"elsewhere {min(gap(far, front), gap(far, back)):.2f}")
_rest = vol(Pos(0, 0, -W.BOARD_RIB_DROP - 0.05) * P["board"] & W.box(0, 60, 0, W.BOARD_RIB_Y, -5, 30), back)
check("the board stops on its ribs under the keys, BOARD_RIB_DROP in (the user's)", _rest > 0.05 and W.BOARD_RIB_DROP <= 0.8,
      f"{W.BOARD_RIB_DROP} in: {_rest:.2f} mm3")
_prop = vol(Pos(0, 0, 0.05) * P["board"], front) - vol(Pos(0, 0, 0.05) * P["board"] - cyl_z(*W.FRONT_PROPS[0], 2.0, -5, 30), front)
check("the board held off the front by the prop beside R21 (the user's)", _prop > 0.05, f"0.05 out: {_prop:.2f} mm3")
# held both ways at its corners (the user's, 2026-11-27: it tipped, the
# bottom-left to the face, the top-right off it and the PTT off the lever)
_b = P["board"]
_bk = {n: vol(Pos(0, 0, -0.05) * _b & box(*q, -5, 30), back) for n, q in
       (("bottom", (0, 60, 0, 10)), ("top", (0, 60, 40, 60)))}
_fr = {n: vol(Pos(0, 0, 0.05) * _b & box(*q, -5, 30), front) for n, q in
       (("bottom-left", (0, 12, 0, 10)), ("top-right", (40, 60, 38, 60)), ("left", (0, 12, 20, 30)))}
check("the board can't tip: the back behind it top and bottom, the front at its corners",
      min(_bk.values()) > 0.05 and min(_fr.values()) > 0.02,
      ", ".join(f"{k} {v:.2f}" for k, v in {**_bk, **_fr}.items()))
_cx, _cy = W.CORNER_SCREWS[0]
_tip = W.HEAD_SEAT + W.SCREW_L
check("bottom-left corner: M3 x SCREW_L, 4+ of thread in the front's post, short of its face (the user's)",
      _tip - W.Z_SPLIT >= 4.0 and _tip <= W.D - W.PILOT_SKIN - 0.3,
      f"thread {_tip - W.Z_SPLIT:.1f}, tip {W.D - W.PILOT_SKIN - _tip:.1f} short of the pilot's end")
check("the corner's post clear of the board's edge", gap(front, P["board"]) >= 0 and
      min(abs(_cx + W.CORNER_POST_R - (W.bx(W.SAW_U))), 9) >= 0.2, f"{W.bx(W.SAW_U) - _cx - W.CORNER_POST_R:.2f}")

# the board can't turn about H1 enough to move the PTT more than 0.15
# worst case at rest: the board turned within the ribs plus H1's slack in the
# board's hole (ASSUMED 3.2 for the 3.0 screw) pushes the plunger into the bar,
# which the T head holds; it must stay short of the switch's 0.25 travel
_d = max(abs(x - W.SCREWS[0][0]) for x in W.LOCATE_X)
_turn = W.LOCATE_GAP / _d * (W.PTT_Y - W.SCREWS[0][1])
_shift = _turn + 0.1
check("PTT not touched at rest, the board shifted", _shift < W.AIR_GAP, f"board shift up to {_shift:.2f} < air gap {W.AIR_GAP}")
# and it clicks, the board shifted away, before the lever meets the case
_need = W.AIR_GAP + 0.25 + _shift
_hit = [t for t in np.arange(0.1, 2.01, 0.05)
        if max(vol(turned(t), o) for o in (front, back, P["board"], P["cell"], _slid)) > 0.01]
_room = _hit[0] if _hit else 2.0
check("PTT clicks before the lever meets the case", _need + 0.15 <= _room,
      f"needs {_need:.2f} at the bar (gap + 0.25 + shift), room {_room:.2f}")
# pressed as far as it goes (its paddle on the case), the cell where it is
# and slid to the right wall
_hit2 = [t for t in np.arange(0.1, 2.01, 0.05)
         if max(vol(turned(t), o) for o in (front, back, P["board"])) > 0.01]
_stop = _hit2[0] if _hit2 else 2.0
_least = min(gap(turned(t), c) for t in list(np.arange(0, _stop, 0.1)) + [_stop] for c in (P["cell"], _slid))
check("the battery clear of the lever over its whole press (the user's)", _least >= 0.25,
      f"least {_least:.2f}, pressed to its stop at {_stop:.2f}")

# ---- ports
zj = W.z_back(W.JACK_UP)
plug = cyl_x(W.bx(W.BW), W.OW + 15, W.JACK_Y, zj, 3.0)
check("jack: 7.0 circle at 2.9 off the board", W.JACK_BODY_D == 7.0 and W.JACK_UP == 2.9, f"{W.JACK_BODY_D}, {W.JACK_UP}")
check("jack: a 6.0 plug body clears everything",
      min(gap(plug, o) for o in (front, back, lev, P["cell"])) >= 0.15,
      f"{min(gap(plug, o) for o in (front, back, lev, P['cell'])):.2f}")
_zt = zj + 3.5 + 0.2   # just over the circle: the front if over the seam
jack_cut = (front if _zt > W.Z_SPLIT else back).is_inside(Vector(W.OW - 0.6, W.JACK_Y, _zt))
check("jack: no slot to the seam", jack_cut, "wall solid just above the circle" if jack_cut else "cut")
ux, zu = W.bx(W.P.USB_U) + W.USB_FIX, W.z_back(W.P.USB_UP)
holes = [(dx, dz) for dx in np.arange(-6.5, 6.6, 0.5) for dz in np.arange(-3.5, 3.6, 0.5)
         if zu + dz < W.Z_SPLIT and not back.is_inside(Vector(ux + dx, W.USB_FACE_Y - 0.5, zu + dz))
         and (abs(dx) > W.USB_SHELL_W / 2 or abs(dz) > W.USB_SHELL_H / 2)]
check("USB: closed round the socket", not holes, f"{len(holes)} open spots")

# ---- assembly: the back (with the cell) pushed straight on over the
# front, board, keytops (the lever goes on after), the hooks' tongues flexed
# in (their noses left out); at home the noses sit in their windows clear.
# Allowed: 0.05 mm3 a pose (2026-11-26, snap hooks)
_flexed = front
for a, b in W.HOOK_NOSES:
    _flexed = _flexed - W.box(a - 0.01, b + 0.01, W.OH - W.WALL - W.HOOK_C, W.OH,
                            W.HOOK_TIP_Z - 0.01, W.HOOK_CATCH_Z + 0.01)
fixed = {"keytops": caps, **{n: s for n, s in P.items() if n != "cell"}}
moving = [whole(back), P["cell"]]
bad = []
for dz in (9, 6, 3, 1.5, 0.5, 0):
    T = Pos(0, 0, -dz)
    v = sum(vol(T * m, o) for m in moving for o in list(fixed.values()) + [_flexed])
    if dz == 0:
        v += sum(vol(m, front) for m in moving)
    if v > 0.05:
        bad.append(f"down {dz}: {v:.2f}")
check("the back goes on straight, the tongues flexed, and sits clear", not bad, ", ".join(bad) or "clear all the way")
check("the cell's arms flex within PLA's (strain <= 2.5%)", W.ARM_STRAIN <= 0.025, f"{W.ARM_STRAIN:.3f}")
_cl = vol(Pos(0, 0, W.CELL_HELD - W.CELL_NOM + 0.05) * P["cell"], back)
check("the cell held: lips 0.1 over a 6.0 cell (the user's rattle)", _cl > 0.1 and vol(P["cell"], back) < 0.01,
      f"lifted 0.15 it meets the lips: {_cl:.2f} mm3")
check("no snap hooks; screws in both top corners (the user's)",
      not W.HOOK_NOSES and sum(1 for x, y in W.CORNER_SCREWS if y > W.OH - 10) == 2, f"{W.CORNER_SCREWS}")

# ---- printing: overhangs and walls too thin to print
def load(fn):
    d = open(os.path.join(HERE, fn), "rb").read()
    n = struct.unpack("<I", d[80:84])[0]
    return np.frombuffer(d[84:84 + n * 50], dtype=np.dtype([("n", "<3f4"), ("v", "<9f4"), ("a", "<u2")]))["v"] \
        .reshape(-1, 3, 3).astype(float)


# allowed: back 9 (the USB socket's 5.9 bridge, and the jack circle's top arch
# by the seam), keytops 40 (the plungers' sockets, 3.4 bridges); with the
# hooks: the front its noses' flat catches, ledges HOOK_C + HOOK_N wide (the
# user's wbitx printed its 0.85 ones), the back the bars over the windows;
# the lever (2026-11-26) the bar's flat ledge under the T head, BAR_LEDGE
# long, BAR_W wide, where it passes over the cell
_ledges = sum((b - a) * (W.HOOK_P + 0.1) for a, b in W.HOOK_NOSES) * 1.05
# the back: the bars over the windows, and the cell's lips (1.2 wide) and
# the arms' (1.0), short ledges
_bridges = (sum((b - a + 2 * W.HOOK_WIN_X) * W.WALL for a, b in W.HOOK_NOSES)
            + sum((b - a) * (W.CELL_CLR + W.LIP_OVER) for a, b in W.TOP_LIPS)
            + sum((b - a) * (W.CELL_CLR + W.ARM_OVER) for a, b in W.ARMS)
            + sum((b - a) * 0.3 for a, b in W.TOP_RIBS)) * 1.05
for fn, allow in (("front.stl", round(0.5 + _ledges, 1)), ("back.stl", round(9.0 + _bridges, 1)),
                  ("lever.stl", round(0.5 + W.BAR_LEDGE * W.BAR_W * 1.05, 1)), ("keytops.stl", 40.0)):
    v = load(fn)
    nr = np.cross(v[:, 1] - v[:, 0], v[:, 2] - v[:, 0])
    L = np.linalg.norm(nr, axis=1)
    bad = (nr[:, 2] / np.maximum(L, 1e-12) < -0.75) & (v[:, :, 2].min(axis=1) > 0.9)
    a = (L / 2)[bad].sum()
    check(f"{fn} needs no supports", a <= allow, f"overhang {a:.2f} mm2 (bridges allowed {allow})")

# ---- wbitx2 (the user's, 2026-11-27): the 50 mm speaker above the board
_h, _d = W.grille()
check("the whole grille: no hole lost to a screw (the user's)", len(_h) + len(_d) == len(W.lattice()),
      f"{len(_h) + len(_d)} of {len(W.lattice())}")
check("the wbitx grille: through over the cone, dimples on the square, a groove under it",
      len(_h) > 40 and len(_d) > 20 and W.LINE_Y < min(y for x, y in _h + _d), f"{len(_h)} holes, {len(_d)} dimples, groove y {W.LINE_Y:.1f}")
_spk = P["speaker"]
check("the speaker clear of the board, above it (no overlap)",
      _spk.bounding_box().min.Y >= W.by(0) + 0.5, f"speaker from y {_spk.bounding_box().min.Y:.2f}, board to {W.by(0):.2f}")
check("the case as wide as wearable2 (57.2)", abs(W.OW - 57.2) < 0.05, f"{W.OW:.1f}")
check("the speaker 50 across, up to 20 deep (the user's)", W.SPK_D == 50.0 and W.SPK_DEEP == 20.0, f"{W.SPK_D}, {W.SPK_DEEP}")
_up = vol(Pos(0, 0, -W.SPK_HOLD - 0.1) * _spk, back)
check("the speaker held up to the face: the back's ribs 0.2 under it", vol(_spk, back) < 0.01 and _up > 0.1,
      f"0.3 back it meets them: {_up:.2f} mm3")
check("four M3 screws: H1, the bottom-left, the two top corners",
      len(W.SCREWS) + len(W.CORNER_SCREWS) == 4, f"{W.SCREWS + W.CORNER_SCREWS}")
for _x, _y in W.CORNER_SCREWS:
    _tip = W.HEAD_SEAT + W.SCREW_L
    check(f"M3 x {W.SCREW_L} at ({_x:.1f}, {_y:.1f}): 4+ of thread, short of the face",
          _tip - W.Z_SPLIT >= 4.0 and _tip <= W.D - W.PILOT_SKIN - 0.3, f"thread {_tip - W.Z_SPLIT:.1f}")
check("the cell's lips and the board's top ribs on the cross-wall",
      W.CY1 + W.CELL_CLR <= W.XWALL_Y + 1e-9 and W.XWALL_Y == W.OH_B - W.WALL, f"wall at y {W.XWALL_Y:.1f}")

from build123d import import_stl as _imp
_w2 = _imp(os.path.join(HERE, "..", "wearable2", "lever.stl"))
_l2 = _imp(os.path.join(HERE, "lever.stl"))
_dv = abs(_l2.volume - _w2.volume) if hasattr(_l2, "volume") else 99
_b1, _b2 = _w2.bounding_box(), _l2.bounding_box()
_off = max(abs(a - b) for a, b in zip((_b1.min.X, _b1.min.Y, _b1.min.Z, _b1.max.X, _b1.max.Y, _b1.max.Z),
                                      (_b2.min.X, _b2.min.Y, _b2.min.Z, _b2.max.X, _b2.max.Y, _b2.max.Z)))
check("the lever is wearable2's, unchanged (the user's)", _off < 0.01, f"bounds off by {_off:.3f}")

print(f"\n{len(fails)} failed" + (": " + "; ".join(fails) if fails else ""))
sys.exit(len(fails))
