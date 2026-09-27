# OCP logo geometry changelog

## Summary

Fixed the backwards-Q BL notch per client feedback: square/axis-aligned end caps
and a tighter gap. Replaced f9ec56a's diagonal tip with a vertical (stem-parallel)
cut with a tip cut parallel to the bottom flat (horizontal in the logo pose) and
gave the bottom bar an orthogonal (vertical) start. Kept thick bands / tight ring
spacing from 79e8e0e. Door/collision stay consistent with drawn quads.

## Stem / notch geometry

| Piece | Before (f9ec56a) | After |
|-------|------------------|-------|
| Local side 0 (BL diagonal tip) | Partial tip; cut parallel to stem (vertical in logo) | Partial tip; cut parallel to **bottom** (horizontal in logo) |
| Local side 1 (bottom) start | Radial / same-t (slanted in logo) | **Orthogonal** (vertical left edge in logo) |
| Local side 7 (left stem) | Full to corner `0..1` | Unchanged (meets diagonal tip) |
| Notch gap | Too wide | Tightened via retuned constants |

Constants: `P_STEM_EXT = 0.58`, `P_GAP_INSET = 0.14` (were 0.72 / 0.34).
Radii / band:gap from 79e8e0e unchanged.

## Helpers

- `makeDiagStemTipQuad` — diagonal tip ending on a shared bottom-parallel cut
  (framing `outerR = 3.20`) so middle+outer tips share one horizontal notch wall.
- `makeFlatSideQuadEnds` — flat-side trapezoid with optional orthogonal ends;
  used for the bottom-bar start.

## Intentionally unchanged

- Ring radii / band:gap (~10:1) from 79e8e0e
- Inner C single-side opening; logo pose sides 0 / 5 / 5
- Door alignment, collision response (`localSolidQuads`), discrete 45° rotation
