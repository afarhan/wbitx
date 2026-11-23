# Rules for the printed cases (case/)

These are the user's standing rules. Follow them in every change, every time.

Before changing a case, read its `REQUIREMENTS.md` (the user's specifications, in their words) and keep every item. After exporting, report the build's parameter changes; any change the user didn't ask for is a bug, so undo it.

- **Screws are M3, all of them.** Never use M2 or any other size, even for a thinner part or a tight spot. If an M3 can't fit somewhere, say so and ask; don't swap the size yourself. Each case script asserts this, so never weaken that assert.
- **Change only what was asked.** A request about one part leaves every other part as it was. Don't add features, resize parts or move things that weren't mentioned. If something else looks wrong, say so; don't fix it silently.
- **The board is produced** (300 at JLCPCB). Holes and cutouts follow the real footprints and the user's own cuts. Never design for a board revision.
- **Fits:** at least 0.4 mm a side for a part through a hole, 0.3 mm for a slide. Keytops in their face holes: 0.3 mm a side (0.4 let them turn, 0.2 was too tight). Get the clearance by shrinking the part that's quickest to reprint, never by enlarging a hole in a part the user has already printed.
- **Use the user's measurements** over datasheet guesses. List every assumption before the user prints.
- **Before giving out STLs:**
  - the overlap check is clear
  - moving parts are tested at rest and pressed
  - an overhang scan shows nothing needs supports
  - each screw's length fits the case's thickness
  - the parts can be assembled in order
  - plugs and knobs can be reached
- Then give the viewer link (http://localhost:8000/<case>/viewer.html) and say exactly what changed.
- **Optimise for the user's time.** Print time is the cost.
  - Before offering test pieces, compare their plastic (or slicer time) with the full print's, and say the ratio.
  - Offer them only when they are a small fraction (about 20% or less) and test the risky change. Otherwise recommend the full print.
  - With every set of STLs, give a short list of what to look for at assembly, so one print yields every finding.
  - Every print job costs about 7 minutes of printer prep on top of its print time. Count it, and put everything for one round on one plate: no separate jobs for small pieces.
  - A test piece must test the feature as it really works. It needs whatever locates the parts in the real case (for example, the walls that stop the board turning about its one screw). No feature should appear in two test pieces.
- **Run the case's `verify.py` before every export** (wearable2 has one). It holds every earlier fix as a pass/fail check. A failure blocks the STLs. Add a check for each new fix, so nothing regresses.
