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
import wearable2 as W  # noqa: E402
from printed import whole, cyl_z, cyl_x, prism_xy, rrect  # noqa: E402

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
src = open(os.path.join(HERE, "wearable2.py")).read()
check("screws are M3 only", not re.search(r"\bM[0-9](?!3)\b|M2_|M2\b", src.replace("M3", "")), "no other size named")
check("mic: a hole only, 2 mm", abs(W.MIC_BORE - 2.0) < 1e-6, f"bore {W.MIC_BORE}")
check("keytops 5 x 4", (W.CAP_W, W.CAP_H) == (5.0, 4.0), f"{W.CAP_W} x {W.CAP_H}")
check("keytops 0.7 proud", abs(W.CAP_Z1 - W.D - 0.7) < 1e-6, f"{W.CAP_Z1 - W.D:.2f}")
check("no lip at the seam (the user's)", W.TONGUE_T == 0, f"TONGUE_T {W.TONGUE_T}")

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

# ---- screws: heads can't fall through; M3 x 12 fits each
def seat(part, x, y):
    worst = 9.0
    for k in range(8):
        a0, a1 = k * math.pi / 4, (k + 1) * math.pi / 4
        sec = (prism_xy([(x, y)] + [(x + 2.75 * math.cos(a0 + (a1 - a0) * i / 6),
                                     y + 2.75 * math.sin(a0 + (a1 - a0) * i / 6)) for i in range(7)],
                        W.M3_HEAD_H, W.M3_HEAD_H + 0.6) - cyl_z(x, y, 1.8, 0, 10))
        worst = min(worst, vol(part, sec))
    return worst


for name, (x, y), part in (("H1", W.SCREWS[0], back), ("top left", W.TL, back), ("hinge", W.PIV, lev)):
    w = seat(part, x, y)
    check(f"{name} head held all round", w > 0.05, f"least material under the head {w:.2f} mm3")


def pilot_run(x, y):
    r = W.M3_PILOT / 2 + 0.1
    zs = [z for z in np.arange(0, W.D, 0.05)
          if front.is_inside(Vector(x + r, y, z)) and front.is_inside(Vector(x - r, y, z))
          and not front.is_inside(Vector(x, y, z))]
    return min(zs), max(zs)


for name, (x, y), need in (("H1", W.SCREWS[0], 3.4), ("top left", W.TL, 3.4), ("hinge", W.PIV, 5.0)):
    z0, z1 = pilot_run(x, y)
    tip = W.M3_HEAD_H + 12
    check(f"{name}: M3 x 12 fits", tip <= z1 - 0.2 and tip - z0 >= need,
          f"tip {tip:.1f}, pilot {z0:.1f}..{z1:.1f}, thread {tip - z0:.1f}")

# ---- the lever: rest, stop, press, room, slot swing
arm = W.PTT_Y - W.PIV[1]


def turned(t):
    return Pos(W.PIV[0], W.PIV[1], 0) * Rot(0, 0, math.degrees(t / arm)) * Pos(-W.PIV[0], -W.PIV[1], 0) * lev


for t, what in ((0.0, "at rest"), (0.35, "pressed 0.35"), (0.8, "pressed 0.8")):
    lp = turned(t)
    hit = {n: round(vol(lp, o), 2) for n, o in (("front", front), ("back", back), ("board", P["board"]),
                                                 ("magnets", P["magnets"]))}
    check(f"lever {what} touches nothing but the PTT", max(hit.values()) <= 0.01, str(hit))
check("lever's T head stops it on the wall", 0.05 <= gap(lev, back) <= 0.15, f"head to wall {gap(lev, back):.2f}")
check("an air gap at rest between the bar and the plunger (the user's)", 0.3 <= gap(lev, P["ptt"]) <= 0.5,
      f"{gap(lev, P['ptt']):.2f}")

# the hinge doesn't rattle (the user's): tight round the column and the screw
check("lever 0.2 round its column, 3.2 hole on its M3", W.LEV_CLR == 0.2 and W.LEV_SCREW == 3.2,
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
check("board touches the case only at H1 (ribs 0.1 off its edges)", min(gap(far, front), gap(far, back)) >= 0.08,
      f"elsewhere {min(gap(far, front), gap(far, back)):.2f}")

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
        if max(vol(turned(t), o) for o in (front, back, P["board"])) > 0.01]
_room = _hit[0] if _hit else 2.0
check("PTT clicks before the lever meets the case", _need + 0.15 <= _room,
      f"needs {_need:.2f} at the bar (gap + 0.25 + shift), room {_room:.2f}")

# ---- ports
zj = W.z_back(W.JACK_UP)
plug = cyl_x(W.bx(W.BW), W.OW + 15, W.JACK_Y, zj, 3.0)
check("jack: 7.0 circle at 2.9 off the board", W.JACK_BODY_D == 7.0 and W.JACK_UP == 2.9, f"{W.JACK_BODY_D}, {W.JACK_UP}")
check("jack: a 6.0 plug body clears everything",
      min(gap(plug, o) for o in (front, back, lev, P["magnets"])) >= 0.15,
      f"{min(gap(plug, o) for o in (front, back, lev, P['magnets'])):.2f}")
_zt = zj + 3.5 + 0.2   # just over the circle: the front if over the seam
jack_cut = (front if _zt > W.Z_SPLIT else back).is_inside(Vector(W.OW - 0.6, W.JACK_Y, _zt))
check("jack: no slot to the seam", jack_cut, "wall solid just above the circle" if jack_cut else "cut")
ux, zu = W.bx(W.P.USB_U), W.z_back(W.P.USB_UP)
holes = [(dx, dz) for dx in np.arange(-6.5, 6.6, 0.5) for dz in np.arange(-3.5, 3.6, 0.5)
         if zu + dz < W.Z_SPLIT and not back.is_inside(Vector(ux + dx, W.USB_FACE_Y - 0.5, zu + dz))
         and (abs(dx) > W.USB_SHELL_W / 2 or abs(dz) > W.USB_SHELL_H / 2)]
check("USB: closed round the socket", not holes, f"{len(holes)} open spots")

# ---- assembly: the back (with the cell and magnets) goes on over the front,
# board, keytops (the lever goes on after): tilted 5 degrees about its right
# edge, HOOK_SLIDE out to the right, lowered, slid in onto the hook's noses,
# swung shut (2026-11-25). Allowed: 0.05 mm3 a pose, the top-left standoff
# brushing the front's left wall by ~0.02 mm mid-swing
fixed = {"front": front, "keytops": caps, **{n: s for n, s in P.items() if n not in ("cell", "magnets")}}
moving = [whole(back), P["cell"], P["magnets"]]
_xp, _zp = W.OW, W.Z_SPLIT


def _rot(a):
    return Pos(_xp, 0, _zp) * Rot(0, a, 0) * Pos(-_xp, 0, -_zp)


_sg = 1 if (_rot(3) * Pos(0, 0, _zp)).position.Z < _zp else -1


def _hits(T):
    return sum(vol(T * m, o) for m in moving for o in fixed.values())


bad = []
poses = ([(f"down {dz}", Pos(W.HOOK_SLIDE, 0, -dz) * _rot(5 * _sg)) for dz in (9, 6, 3, 1.5, 0.5, 0)]
         + [(f"slide {s}", Pos(s, 0, 0) * _rot(5 * _sg)) for s in (0.9, 0.6, 0.3, 0)]
         + [(f"swing {a}", _rot(a * _sg)) for a in (4, 3, 2, 1, 0.5, 0)])
for what, T in poses:
    v = _hits(T)
    if v > 0.05:
        bad.append(f"{what}: {v:.2f}")
check("the back goes on (tilt, slide onto the hook, swing shut)", not bad, ", ".join(bad[:5]) or "clear all the way")
_pull = vol(whole(back).translate((0, 0, -0.2)), front)
check("the hook holds: pulled 0.2 apart, the noses bear on the bars", _pull >= 0.5, f"{_pull:.2f} mm3")
_straight = vol(whole(back).translate((0, 0, -1.0)), front)
check("the hook is real: straight on, the noses block", _straight >= 1.0, f"{_straight:.2f} mm3")
check("a magnet at the top right, under the PTT (the user's)",
      W.MAGNETS[1][1] >= W.OH - W.WALL - W.MAG_D / 2 - W.MAG_CLR - W.MAG_RIM - 0.01,
      f"at {W.MAGNETS[1][0]:.1f}, {W.MAGNETS[1][1]:.1f}")

# ---- printing: overhangs and walls too thin to print
def load(fn):
    d = open(os.path.join(HERE, fn), "rb").read()
    n = struct.unpack("<I", d[80:84])[0]
    return np.frombuffer(d[84:84 + n * 50], dtype=np.dtype([("n", "<3f4"), ("v", "<9f4"), ("a", "<u2")]))["v"] \
        .reshape(-1, 3, 3).astype(float)


# allowed: back 9 (the USB socket's 5.9 bridge, and the jack circle's top arch
# by the seam), keytops 40 (the plungers' sockets, 3.4 bridges); with the hook
# (2026-11-25): the front its noses' flat catches, ledges HOOK_C + HOOK_N wide
# (the user's wbitx printed its 0.85 ones), the back the bars over the
# windows, ~3.5 bridges
_ledges = sum((b - a) * (W.HOOK_C + W.HOOK_N) for a, b in W.HOOK_NOSES) * 1.05
_bridges = sum((b - a + 2 * W.HOOK_C) * W.WALL for a, b in W.HOOK_NOSES) * 1.05
for fn, allow in (("front.stl", round(0.5 + _ledges, 1)), ("back.stl", round(9.0 + _bridges, 1)),
                  ("lever.stl", 0.5), ("keytops.stl", 40.0)):
    v = load(fn)
    nr = np.cross(v[:, 1] - v[:, 0], v[:, 2] - v[:, 0])
    L = np.linalg.norm(nr, axis=1)
    bad = (nr[:, 2] / np.maximum(L, 1e-12) < -0.75) & (v[:, :, 2].min(axis=1) > 0.9)
    a = (L / 2)[bad].sum()
    check(f"{fn} needs no supports", a <= allow, f"overhang {a:.2f} mm2 (bridges allowed {allow})")

print(f"\n{len(fails)} failed" + (": " + "; ".join(fails) if fails else ""))
sys.exit(len(fails))
