# OCP logo geometry changelog

## Summary

Angled the outer-two-ring stem tips to match the client logo: tips follow the
octagon bottom-left diagonal and end on a stem-parallel cut (vertical in the
logo pose) instead of a past-corner vertical extension with a blunt/radial end.
Kept thick bands / tight gaps from 79e8e0e and the Q-notch (diagonal tip + inset
bottom). Collision/door unchanged in behavior.

## Stem tip geometry

| Piece | Before (79e8e0e) | After |
|-------|------------------|-------|
| Local side 7 (left stem) | Extended past corner `0 .. 1+P_STEM_EXT` | Full flat to corner only `0 .. 1` |
| Local side 0 (BL diagonal) | Omitted | Partial tip `makeDiagStemTipQuad` to shared stem-parallel cut at `P_STEM_EXT` on framing `outerR=3.20` |
| Local side 1 (bottom) | Inset `P_GAP_INSET .. 1` | Unchanged |
| Tip end edge | Radial (same-t) on extended vertical | Parallel to stem; middle+outer share one notch wall |

Constants unchanged: `P_STEM_EXT = 0.72`, `P_GAP_INSET = 0.34`. Radii from 79e8e0e unchanged.

## Intentionally unchanged

- Ring radii / band:gap (~10:1) from 79e8e0e
- Inner C single-side opening; logo pose sides 0 / 5 / 5
- Door alignment, collision response, discrete 45° rotation
