# wBitx case: notes for the molder

Files (from `make mold` in `case/`, in `case/mold/`): `front.step`, `back.step`,
`keytop.step`, and `assembly.step` (all three in place). Units mm. Frame: z out
of the front face; the parting line is the seam between the halves.

## Material and finish
- PC/ABS, black. Nominal wall 1.8 mm.
- Outside: VDI 3400 Ref 24 texture. The front face's grille dimples and the
  line under the speaker stay as modelled.
- Inside: polished or as machined.

## Draft (modelled)
- Outside walls 1.5°, inside walls 1°, measured from the parting line.
- Ribs, standoffs, the mic tube and the speaker ring 0.5°.
- Port notches' sides 1° (they are shut-offs).
- Not modelled: the small holes (grille, key, mic, screw pilots). Please put
  0.5° on those pins.

## Tooling
- **Front:** five catch pockets on the inner walls, just past the seam. They
  are undercuts, so they need five small lifters. Everything else pulls
  straight: the ports are notches open to the seam and need no side actions.
- **Back:** five snap hooks. Each has a slot through the back plate under its
  lip, so no lifters are needed. The slot is also how the hook is released.
- **Keytop:** a small part; could go in the same tool as a family cavity.
- Gates and ejector pins are your choice. Keep gate marks and pins off the
  front face.

## Assembly
- **Board to front:** two thread-forming screws for plastics, M3 × 5 (PT or
  similar), through the board's holes H1 and H2 into the standoffs. The pilot
  is 2.5 mm.
- **Mic:** a 0.5 mm foam gasket ring between the mic tube and the board.
- **Speaker:** sits in the front's ring. A 0.5 mm foam pad goes between its
  magnet and the back's ring.
- **Closing:** the PTT keytop goes into its counterbore, then the back snaps on.

## To check on a real production board before cutting steel
- **USB-C:** the mouth's centre is taken as 2.6 mm off the board's back.
- **Jack:** the PJ320D's axis is taken as 2.5 mm off the board's back.
- **Slide switch:** the slot for the K3-1296S-E1 knob is 4.6 × 2.2 mm.
- **Display:** the module's size and glass position were measured on the
  prototype's module, not the production one.
- **Speaker plug:** room behind the board for the speaker's plug on J3.
