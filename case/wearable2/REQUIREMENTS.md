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
- They must be easy to press. 0.7 mm proud of the face (the user, 2026-11-23: 2.0 was too high).
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
- Cell: 602030, 300 mAh, behind the PCB.
- Two 10 x 3 mm disc magnets in the back, so a steel plate holds it through a shirt.
- **All screws M3**, length to fit the case. Top-left screw: option (b), the M3 head let into the left side wall.

## Working rules
- Everything local; nothing published.
- Commit only after the user has tried it.
- After every change: the viewer link (http://localhost:8000/wearable2/viewer.html, with outline and exploded views) and exactly what changed.
- Test pieces when asked; make them quick to print.
- Don't assume. If something isn't specified, ask, or state the assumption before the user prints.
