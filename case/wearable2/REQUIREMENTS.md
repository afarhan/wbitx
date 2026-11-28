# wearable2: what the user asked for

Read this before any change. Every revision must keep every item here. Add an item only when the user states a new one; never add one from guesswork.

## Board (produced, 300 at JLCPCB)
- No board revision, ever. The only change is the user's cut.
- Sawn straight off left of SW2, "sparing the leftmost switch" (u 13.3, full height). H2 is gone with it.
- Key holes exactly on the real switches.

## Speaker: Taoglas SPKM.2030.8.A, 20 x 30 oval
- On the right side, on top of the PCB.
- Its rim sealed by a ring round its edge, not a bead on its face.
- Grille holes over the speaker only; no faux grille (dimples) anywhere else.

## Keys (B3FS-1010P)
- Measured (2026-11-23): the switch body 3.0 mm off the board; a keytop's flange rests 3.8 mm off it (plunger top 4.8). The face's inside rests on the speaker's rim, 5.5 mm off the board.
- Printed keytops bring them out over the front surface.
- Keytops 5 mm x 4 mm each.
- They must be easy to press. 1.4 mm proud of the face, the keytops 4.3 mm tall (the user, 2026-11-26: 0.7 proud gave 3.6 tall, too low; 2026-11-23: 2.0 was too high).
- Fit: keytops are smaller than their holes, 0.3 a side (0.4 left the holes too big and the keytops turned; 0.2 was too tight to insert).

## PTT lever
- No claw: the T-shaped plunger itself holds the lever in.
- A projection from the lever reaches inside to press the recessed SKRTLAE010.
- The hinge is up the right side, near the middle, so the jack is reachable directly, not through a hole in the lever.
- Enough movement to press the button. The plunger end has a T head to allow for slippage.
- An air gap at rest between the T head and the switch's plunger (0.4 mm), so the lever moves freely before it presses.
- Thinner than the first version.
- Prints without supports.

## Display
- The glass is 16.5 mm tall in all, including a 3 mm strip at its top.
- (Committed case) the inset's top is 1 mm higher.
- (wearable2) the display cutout 1 mm up toward the glass's top: it clipped the top row of pixels.
- (wearable2) the hinge must not rattle round its pin.

## Shell
- Walls thin (it holds about 25 g).
- As thin as the stack allows; no needless gaps (for example between the cell and the PCB).
- Mic: a hole only, no tube. (A 1.0 mm hole printed shut.)

## Power and mounting
- (2026-11-26) Cell: Nokia BL-5C, 6 x 34 x 54 mm (the user's; listings vary 5.8 to 6.2 thick, so the pocket is 6.2), behind the PCB, over the headset jack, clear of the lever. It runs across the case along the top wall, clear of H1 at the bottom.
- (2026-11-26) The enclosure grows about 5 mm in width for it, on the **left**, so the right side (jack, PTT, lever) stays as it is against the board; and 3.4 mm thicker, so the cell sits behind the jack.
- (2026-11-26) **No magnets.** (They were two 10 x 3 discs in the back, the right one top right under the PTT.)
- (2026-11-26) **No top-left screw**: the cell is under it. The halves close with **hooks on the top wall only, at both ends** (the right wall's hooks are gone), and the M3 through H1 at the bottom right.
- **All screws M3**, length to fit the case.

## 2026-11-25 (hook and magnet replaced on 2026-11-26, above)
- The top right (the PTT's side) didn't close: a rigid hook as wbitx's on the right wall: tongues from the front inside the wall, three noses (above the PTT's T head; two between the hinge's neck and the bar's slot) out through windows in the back's wall. The back goes on tilted about its right edge, slides in onto the noses, swings shut. The hinge neck's point cut flat at the seam beside the wall for that.
- Both magnets were low and it tipped forward off a shirt: the right magnet moved up to the top right, under the PTT (as far right as the lever's bar lets it). Top left has no room (the top-left screw's foot); centring the cell would block the top right.

## Working rules
- Everything local; nothing published.
- Commit only after the user has tried it.
- After every change: the viewer link (http://localhost:8000/wearable2/viewer.html, with outline and exploded views) and exactly what changed.
- Test pieces when asked; make them quick to print.
- Don't assume. If something isn't specified, ask, or state the assumption before the user prints.

## 2026-11-26 (after the fit print)
- Hooks: snap-fits, the back pushed straight on; 0.4 clear off the wall, tongues 0.8 x 5.5, noses 0.5 into the wall with a 45 degree lead-in (the slide-on hooks at 0.1 never went in, one snapped, the back sat skewed).
- The BL-5C held, no rattle: its top edge under two lips on the top wall, its bottom edge under two snap arms; lips 6.1 off the floor for the user's 6.0 cell.
- The board held behind the keys: two ribs off the bottom wall up to its back, either side of the SuperMini (x 12.75-14.55, 34.0-35.8) (2026-11-27, the DOWN key pushed it in).
- 2026-11-27: the cell is 5.5 (measured); lips and arms 0.1 over it.
- 2026-11-27: an M3 x 16 at the bottom-left corner (as seen from the front; the user's "right bottom" from the back), the halves were loose there; post in the strip left of the board.
- 2026-11-27: a prop from the front's face onto the board beside R21 (R21 is under the OLED module; the prop is just left of it), so the board can't rise and hold a key down.
- 2026-11-27: the lever's hole on its M3 3.3 (3.4 rattled, 3.2 was tight).
- 2026-11-27: the board clamped at its top edge: ribs off the back's top wall over its back (x 10.6-15, 24-27, 45.5-50.8), the board's top 0.4 off the wall leaves no room to slide it under lips on the front. Props on the front at the bottom-left corner and the top-right (right of the OLED module), so it can't tip and the PTT stays on the lever.
- 2026-11-27: the SuperMini is hand-soldered and lands off from board to board: the USB cutout 0.6 wider a side across (USB_PLAY), centred 0.35 right (the user's board was 0.7 right); the right board rib off the SuperMini (34.75-36.0). The two bottom ribs 0.8 under the board's back (at it, they held the seam 0.75 open).
