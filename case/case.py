#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Ashhar Farhan
"""
case.py — the wifitalkie's case, the size of a credit card, held landscape.

The front, as talkie_front.jpeg is: the 50 mm speaker on the left behind a
full rectangle of large holes (the Teenage Engineering grille, turned on its
side), the perfboard on the right with the 10 mm microphone poking out at its
top, the display, and four 6x6 keys under it. The headset jack comes out of
the top, the ESP32's USB-C out of the right side. The keys' 3.5 mm plungers
come through the front, each in a countersink for the finger. Behind it all,
in a shallow tray of its own, a KP523450 pouch cell lies flat on the back wall
behind the board, and its charger module lies behind the speaker's magnet with
its USB-C out of the bottom.

Two parts to print in black PLA: the front shell (face down) and the back
tray (back down). Four M3 screws go in from the back.

    python case.py front    only exports/front.stl, to try on the prototype

    python case.py          exports/*.glb to look at, *.stl to print
    python case.py check    the overlap check
    make serve              http://localhost:8000/viewer.html

Frame: x to the right, y up, z out of the front; the front face is at z = D,
the back at z = 0. "depth" is millimetres in from the front face. Positions on
the board are x from its left edge, y down from its top edge, from the front,
scaled from talkie_front.jpeg at 11.45 px/mm. Measure, and change them here.
"""
import os
import sys

from build123d import (Align, Axis, Box, Color, Compound, Cone, Cylinder, Pos, Rot,
                       Text, export_gltf, export_stl, extrude, fillet)

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "exports")
FONT = "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf"

# ------------------------------------------------------------------ sizes
WALL = 2.0          # every wall
CLR = 0.3           # slip between a part and what it sits against
CORNER = 3.2        # a credit card's corner

# the perfboard (0.1" pitch), and its height above and below
BW, BH, BT = 35.1, 52.6, 1.6   # 1.5 wider than first measured: it did not fit
FRONT_H = 4.0       # front wall to the board's front face: the pads' height
BACK_H = 6.0        # the SuperMini, the jack, behind the board
SWITCH_TOP = 7.0    # the plungers' tops above the board (6x6x7 switches)

MIC_XY, MIC_D, MIC_HOLE = (7.5, 4.9), 9.7, 10.0     # it pokes out
MIC_PROUD = 1.0
OLED_PCB = (4.2, 29.1, 10.2, 37.3)        # x0, x1, y0, y1: the blue module
OLED_GLASS = (4.5, 29.2, 19.0, 33.0)      # the black glass
OLED_WIN = (5.0, 28.7, 19.3, 32.7)        # the window in the front
OLED_TOP = 5.0      # the display's glass above the board, measured
KEYS = [(6.5, 42.1), (16.66, 42.1), (26.82, 42.1),   # 4 holes apart,
        (16.66, 49.72)]                              # and 3 holes down
KEY_HOLE = 4.0      # the 3.5 mm plungers, pressed as they are
GROOVE_W, GROOVE_D = 0.8, 0.5             # the line between the two plates
DISH_D, DISH_DEEP = 6.0, 1.0              # round each key, for the finger
JACK_X, JACK_HOLE = 26.5, 6.5             # on the back, out of the top
JACK_DEPTH = WALL + FRONT_H + BT + 2.5    # its barrel's centre
USB_Y, USB_W, USB_H = 24.8, 9.5, 3.8      # SuperMini's USB-C, out the right
USB_DEPTH = WALL + FRONT_H + BT + 3.0

SPK_D, SPK_DEEP, SPK_MAGNET = 50.0, 14.0, 22.6
RING_T, RING_H = 1.2, 3.0                 # ring on the front, glue it in
GRILLE_HOLE, GRILLE_PITCH, GRILLE_N = 3.2, 4.4, 5   # 11 x 11 holes
DIMPLE = 1.0        # holes off the cone are blind, this deep

BAT_W, BAT_L, BAT_T = 34.0, 50.0, 5.2     # KP523450 pouch, standing upright
CHG_L, CHG_W, CHG_T = 28.0, 17.5, 1.2     # TP4056 USB-C module
CHG_H = CHG_T + 3.2                       # with its USB-C on top
CHG_X = 13.0        # its left edge, inside x; it lies with its port down
CHG_USB_W, CHG_USB_H = 9.5, 3.8

BOSS_D, PILOT_D, SCREW_D, HEAD_D = 7.0, 2.5, 3.4, 6.2   # M3, cut its own thread
LIP_H, LIP_T, LIP_GAP = 1.5, 0.8, 0.2

# ---------------------------------------------------------- derived sizes
SPK_R = SPK_D / 2 + CLR
RING_R = SPK_R + RING_T
SPK_CX = SPK_R + 0.2                      # inside x; the ring meets the wall
BOARD_X0 = SPK_CX + RING_R + CLR          # board's left edge, inside x
IW = BOARD_X0 + BW + CLR
IH = BH + 2 * CLR
SPK_CY = IH / 2
OW, OH = IW + 2 * WALL, IH + 2 * WALL

SPLIT = WALL + max(SPK_DEEP, FRONT_H + BT + BACK_H) + 0.5   # the seam
# four screws at the corners of the speaker's square
BOSSES = [(SPK_CX - 22, SPK_CY - 22), (SPK_CX + 22, SPK_CY - 22),
          (SPK_CX - 22, SPK_CY + 22), (SPK_CX + 22, SPK_CY + 22)]

# the cell lies on the back wall behind the board, the charger behind the
# speaker's magnet; whichever stack is deeper sets the thickness
POST_H = 2.0        # posts under the board's bottom edge, below the cell
BAT_X0 = BOARD_X0 + BW / 2 - BAT_W / 2    # inside x, centred on the board
BAT_Y0 = CLR + POST_H + CLR               # inside y, above the posts
D = max(WALL + FRONT_H + BT + BACK_H + CLR + BAT_T,
        WALL + SPK_DEEP + CLR + CHG_H) + WALL
Z_SPLIT = D - SPLIT

PROUD = SWITCH_TOP - FRONT_H - WALL       # plungers past the front face
assert PROUD >= -DISH_DEEP, "the plungers don't reach the front; lower FRONT_H"
assert BAT_Y0 + BAT_L <= IH, "the cell is taller than the inside"
assert OLED_TOP - FRONT_H + 0.2 <= WALL - 0.6, "the display pocket leaves too thin a wall"


# --------------------------------------------------------------- helpers
def z(depth):
    return D - depth


def ix(x):
    return WALL + x


def iy(y):
    return WALL + y


def bx(x):            # board x -> frame x
    return WALL + BOARD_X0 + x


def by(y):            # board y (down from its top) -> frame y
    return WALL + IH - CLR - y


def box(x0, x1, y0, y1, z0, z1):
    return Pos(min(x0, x1), min(y0, y1), min(z0, z1)) * Box(
        abs(x1 - x0), abs(y1 - y0), abs(z1 - z0), align=(Align.MIN,) * 3)


def cyl_z(cx, cy, r, z0, z1):
    return Pos(cx, cy, min(z0, z1)) * Cylinder(
        r, abs(z1 - z0), align=(Align.CENTER, Align.CENTER, Align.MIN))


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


def chg_mouth():
    """The charger's USB-C through the bottom wall; it may cross the seam."""
    return mouth_y(-1, iy(0) + 1, ix(CHG_X + CHG_W / 2),
                   WALL + CHG_T + CHG_USB_H / 2, CHG_USB_W, CHG_USB_H)


def mouth_y(y0, y1, cx, cz, w, h):
    """An obround through the top or bottom wall: w along x, h along z."""
    r = h / 2
    return (cyl_y(cx - (w / 2 - r), y0, y1, cz, r)
            + cyl_y(cx + (w / 2 - r), y0, y1, cz, r)
            + box(cx - (w / 2 - r), cx + (w / 2 - r), y0, y1, cz - r, cz + r))


# the two ports that leave through the walls of the front shell; each is a
# notch open to the seam, so the board drops in from behind, and the tray
# fills the notch back in with a tongue
def jack_hole(y_lo, y_hi):
    return cyl_y(bx(JACK_X), y_lo, y_hi, z(JACK_DEPTH), JACK_HOLE / 2)


def jack_notch(extra=0.0):
    c, r = bx(JACK_X), JACK_HOLE / 2 + extra
    return box(c - r, c + r, iy(IH) - 1, OH + 1, z(JACK_DEPTH),
               Z_SPLIT - LIP_H - 1)


def usb_hole():
    return mouth_x(ix(IW) - 1, OW + 1, by(USB_Y), z(USB_DEPTH), USB_W, USB_H)


def usb_notch(extra=0.0):
    c, w = by(USB_Y), USB_W / 2 + extra
    return box(ix(IW) - 1, OW + 1, c - w, c + w, z(USB_DEPTH),
               Z_SPLIT - LIP_H - 1)


# ------------------------------------------------------------ front shell
def front_shell():
    s = outer(Z_SPLIT, D) - inner(Z_SPLIT - 1, D - WALL)
    s += inner(Z_SPLIT - LIP_H, Z_SPLIT, LIP_T) - inner(Z_SPLIT - 2, D, 0)

    sx, sy = ix(SPK_CX), iy(SPK_CY)
    s += (cyl_z(sx, sy, RING_R, D - WALL - RING_H, D - WALL + 0.5)
          - cyl_z(sx, sy, SPK_R, D - WALL - RING_H - 1, D - WALL))

    # pads the board's front face rests on: left and right columns, top edge
    zf = z(WALL + FRONT_H)
    for (x0, x1, y0, y1) in [(0, 1.3, 12, 38), (BW - 1.3, BW, 10, 38),
                             (14, 24, 0, 1.3)]:
        s += box(bx(x0), bx(x1), by(y0), by(y1), zf, D - WALL + 0.5)

    for (x, y) in BOSSES:
        s += cyl_z(ix(x), iy(y), BOSS_D / 2, Z_SPLIT, D - WALL + 0.5)

    # ---- cuts
    for (x, y) in BOSSES:
        s -= cyl_z(ix(x), iy(y), PILOT_D / 2, Z_SPLIT - 1, D - WALL - 0.6)

    # the grille: through over the cone, blind dimples elsewhere
    open_r = SPK_D / 2 - 3.0
    for i in range(-GRILLE_N, GRILLE_N + 1):
        for j in range(-GRILLE_N, GRILLE_N + 1):
            dx, dy = i * GRILLE_PITCH, j * GRILLE_PITCH
            through = dx * dx + dy * dy <= open_r ** 2
            s -= cyl_z(sx + dx, sy + dy, GRILLE_HOLE / 2,
                       D - WALL - 1 if through else D - DIMPLE, D + 1)

    gx = bx(0) - CLR - RING_T / 2
    s -= box(gx - GROOVE_W / 2, gx + GROOVE_W / 2, -1, OH + 1,
             D - GROOVE_D, D + 1)

    x0, x1, y0, y1 = OLED_WIN
    s -= box(bx(x0), bx(x1), by(y0), by(y1), D - WALL - 1, D + 1)
    # a pocket on the inside for the display standing taller than the pads
    pocket = OLED_TOP - FRONT_H + 0.2
    if pocket > 0.2:
        x0, x1, y0, y1 = OLED_PCB
        s -= box(bx(x0) - CLR, bx(x1) + CLR, by(y0) - CLR, by(y1) + CLR,
                 D - WALL - 1, D - WALL + pocket)
    for (kx, ky) in KEYS:
        s -= cyl_z(bx(kx), by(ky), KEY_HOLE / 2, D - WALL - 1, D + 1)
        # a 45 degree countersink, not a flat-bottomed dish: printed face
        # down, a flat floor would bridge over air and come out stringy
        s -= Pos(bx(kx), by(ky), D - DISH_DEEP) * Cone(
            KEY_HOLE / 2, DISH_D / 2, DISH_DEEP, align=(Align.CENTER, Align.CENTER, Align.MIN))
        s -= cyl_z(bx(kx), by(ky), DISH_D / 2, D, D + 1)
    s -= cyl_z(bx(MIC_XY[0]), by(MIC_XY[1]), MIC_HOLE / 2, D - WALL - 1, D + 1)

    s -= jack_hole(iy(IH) - 1, OH + 1) + jack_notch(0.1)
    s -= usb_hole() + usb_notch(0.1)
    if WALL + CHG_T + CHG_USB_H > Z_SPLIT - 1:
        s -= chg_mouth()
    return s


# -------------------------------------------------------------- back tray
def back_tray():
    s = outer(0, Z_SPLIT) - inner(WALL, Z_SPLIT + 1)
    s -= inner(Z_SPLIT - LIP_H - LIP_GAP, Z_SPLIT + 1, LIP_T + LIP_GAP)

    # tongues that close the notches over the jack and the USB-C
    s += jack_notch(-0.1) & box(-1, OW + 1, iy(IH), OH, Z_SPLIT - 0.5, D)
    s += usb_notch(-0.1) & box(ix(IW), OW, -1, OH + 1, Z_SPLIT - 0.5, D)

    for (x, y) in BOSSES:
        s += cyl_z(ix(x), iy(y), BOSS_D / 2, WALL - 0.5, Z_SPLIT - LIP_GAP)

    # a rail along the cell's left edge; the right wall and the posts do
    # the rest
    bx0 = ix(BAT_X0) - CLR
    s += box(bx0 - 1.2, bx0, iy(BAT_Y0), iy(IH) + 0.5, WALL - 0.5, WALL + 2.5)

    # the charger, port down: a low fence on its three inside sides
    cx0, cx1, cy1 = ix(CHG_X), ix(CHG_X + CHG_W), iy(CLR + CHG_L)
    s += box(cx0 - 1.2, cx0, iy(0) - 0.5, cy1 + 1.2, WALL - 0.5, WALL + 2.5)
    s += box(cx1, cx1 + 1.2, iy(0) - 0.5, cy1 + 1.2, WALL - 0.5, WALL + 2.5)
    s += box(cx0 - 1.2, cx1 + 1.2, cy1, cy1 + 1.2, WALL - 0.5, WALL + 2.5)

    # two posts under the board's bottom edge, holding it to the front pads
    zb = z(WALL + FRONT_H + BT)
    for xb in (6.0, BW - 6.0):
        s += box(bx(xb) - 2.5, bx(xb) + 2.5, by(BH - 0.5), by(BH - POST_H),
                 WALL - 0.5, zb)

    # ---- cuts
    for (x, y) in BOSSES:
        s -= cyl_z(ix(x), iy(y), SCREW_D / 2, -1, Z_SPLIT + 1)
        s -= cyl_z(ix(x), iy(y), HEAD_D / 2, -1, 2.5)
    s -= jack_hole(iy(IH) - 1, OH + 1)
    s -= usb_hole()
    s -= chg_mouth()
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
    board = box(bx(0), bx(BW), by(0), by(BH), zbb, zbf)
    x0, x1, y0, y1 = OLED_PCB
    glass_z = z(WALL + FRONT_H - OLED_TOP)
    oled = box(bx(x0), bx(x1), by(y0), by(y1), glass_z - 2.3, glass_z - 1.3)
    x0, x1, y0, y1 = OLED_GLASS
    glass = box(bx(x0), bx(x1), by(y0), by(y1), glass_z - 1.3, glass_z)
    switches = None
    for (kx, ky) in KEYS:
        x, y = bx(kx), by(ky)
        sw = (box(x - 3, x + 3, y - 3, y + 3, zbf, zbf + 3.5)
              + cyl_z(x, y, 1.75, zbf + 3.4, zbf + SWITCH_TOP))
        switches = sw if switches is None else switches + sw
    mic = cyl_z(bx(MIC_XY[0]), by(MIC_XY[1]), MIC_D / 2, zbf, D + MIC_PROUD)
    jc = bx(JACK_X)
    jack = (box(jc - 3, jc + 3, by(11), by(0), zbb - 5, zbb)
            + cyl_y(jc, by(0), OH + 0.5, z(JACK_DEPTH), 2.5))
    mini = (box(bx(13), bx(BW), by(USB_Y - 11), by(USB_Y + 11), zbb - 1.0, zbb)
            + box(bx(BW) - 7, ix(IW), by(USB_Y) - 4.45, by(USB_Y) + 4.45,
                  z(USB_DEPTH) - 1.6, z(USB_DEPTH) + 1.6))
    sx, sy = ix(SPK_CX), iy(SPK_CY)
    speaker = (cyl_z(sx, sy, SPK_D / 2, z(WALL + 2.5), z(WALL))
               + cyl_z(sx, sy, SPK_D / 2 - 6, z(WALL + 8), z(WALL + 2.5))
               + cyl_z(sx, sy, SPK_MAGNET / 2, z(WALL + SPK_DEEP), z(WALL + 8)))
    cell = box(ix(BAT_X0), ix(BAT_X0 + BAT_W), iy(BAT_Y0), iy(BAT_Y0 + BAT_L),
               WALL, WALL + BAT_T)
    cx0, cx1, cm = ix(CHG_X), ix(CHG_X + CHG_W), ix(CHG_X + CHG_W / 2)
    charger = (box(cx0, cx1, iy(CLR), iy(CLR + CHG_L), WALL, WALL + CHG_T)
               + box(cm - 4.45, cm + 4.45, iy(0), iy(7.3),
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
            (charger, "charger", (0.10, 0.30, 0.70))]


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
    front_print = Pos(0, OH, D) * Rot(180, 0, 0) * front
    # a fit test: only the board's end of the front, as deep as the board
    test = front & box(bx(0) - CLR, OW + 1, -1, OH + 1,
                       z(WALL + FRONT_H + BT), D + 1)
    test_print = Pos(0, OH, D) * Rot(180, 0, 0) * test
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
    export_stl(back, os.path.join(OUT, "back.stl"))
    export_stl(test_print, os.path.join(OUT, "front_test.stl"))

    # what shows through the front: the display, the plungers, the mic
    seen = [p for p in parts() if p[1] in ("glass", "text", "oled",
                                           "switches", "mic")]
    glb([(front, "front", PLA)] + seen, "front.glb")
    glb([(back, "back", PLA)], "back.glb")
    glb([(test, "test", PLA)] + seen, "test.glb")
    glb([(back, "back", PLA), (front, "front", PLA)] + seen, "closed.glb")
    gap = 35
    glb([(Pos(0, 0, -gap) * back, "back", PLA)] + parts()
        + [(Pos(0, 0, gap) * front, "front", PLA)], "exploded.glb")


if __name__ == "__main__":
    main()
