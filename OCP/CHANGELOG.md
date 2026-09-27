# OCP logo geometry changelog

## Summary

Client rejected d70dacd: square end caps were correct, but retuning the BL notch
closed the gap (hairline leftover) instead of narrowing it to the logo corridor.
Restored a visible open vertical channel matching the logo gap (~20% of band
thickness). Kept horizontal stem tip + vertical bottom start, thick bands, and
ring spacing.

## Stem / notch geometry

| Piece | d70dacd (rejected) | After |
|-------|--------------------|-------|
| Local side 0 (BL diagonal tip) | Horizontal cut (square tip) | Unchanged |
| Local side 1 (bottom) start | Orthogonal vertical edge | Unchanged |
| Notch gap | Sealed (~1.6% of band) | Open corridor (~22% of band, logo-matched) |

Constants: `P_STEM_EXT = 0.55`, `P_GAP_INSET = 0.18` (were 0.58 / 0.14).
Measured logo BL corridor ~8px vs ~40px stem in logo-bl-zoom (~20% of band);
0.55/0.18 yields outer hGap ~0.162 (21.6% of outer band 0.748).
Radii / band:gap from 79e8e0e unchanged. Tip/cap helpers unchanged.

## Intentionally unchanged

- Square/straight end caps (`makeDiagStemTipQuad` horizontal cut, `makeFlatSideQuadEnds` ortho bottom start)
- Ring radii / band:gap (~10:1) from 79e8e0e
- Inner C single-side opening; logo pose sides 0 / 5 / 5
- Door alignment, collision response (`localSolidQuads`), discrete 45-degree rotation
