# OCP logo geometry changelog

## Summary

Client rejected f602603: offline hGap claimed ~22% open but the LIVE OpenGL
render stayed sealed because the shared tipRefR=3.20 horizontal cut lets the
middle tip overshoot past the bottom-bar start (visual corridor negative).
Retuned P_STEM_EXT / P_GAP_INSET against real DISPLAY=:4 captures until the
BL notch shows a clear vertical corridor matching the logo (~50px, ~30% of
band). Square caps unchanged (horizontal stem tip, vertical bottom start).
Thick bands / radii unchanged.

## Stem / notch geometry

| Piece | f602603 (rejected) | After |
|-------|--------------------|-------|
| P_STEM_EXT | 0.55 | 0.30 |
| P_GAP_INSET | 0.18 | 0.45 |
| Live notch | Sealed (hairline) | Open corridor ~50px / ~0.30 of band |
| End caps | Horizontal tip + vertical bottom | Unchanged |

Live verify (1280x800 ffmpeg on :4): gap_px median 50, ratio 0.299 at BL tip rows.
Logo compare-notch reference measured ~50px / ~0.29 at the same junction.

## Intentionally unchanged

- Tip/cap helpers (`makeDiagStemTipQuad` horizontal cut, `makeFlatSideQuadEnds` ortho bottom)
- Ring radii / band:gap (~10:1) from 79e8e0e
- Inner C opening; logo pose sides 0 / 5 / 5
- Door alignment, collision (`localSolidQuads`), discrete 45-degree rotation
