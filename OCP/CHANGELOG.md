# OCP logo geometry changelog

## Summary

Retuned concentric ring radii so band thickness and inter-ring gaps match the client OCP logo: thick black octagon bands with narrow white channels (band:gap ~10:1). Kept the backwards-Q outer notch and C inner opening from commit b4f0eb2 (flat-edge quads, stem/inset, door/collision unchanged).

## Radii change

| Ring | Before (innerR-outerR) | After | Band | Gap to next |
|------|------------------------|-------|------|-------------|
| Inner (C) | 1.15-1.65 | 0.80-1.51 | 0.71 | 0.07 |
| Middle (Q) | 2.00-2.45 | 1.58-2.32 | 0.74 | 0.07 |
| Outer (Q) | 2.75-3.20 | 2.39-3.20 | 0.81 | - |

Outer `outerR` stays 3.20 (framing / hallway limit). Proportions measured from the attached logo (flat-axis band ~160-184 px, gap ~16 px on the 1560 px reference).

## Intentionally unchanged

- Backwards-Q: `P_STEM_EXT = 0.72`, `P_GAP_INSET = 0.34`, flat-edge `makeFlatSideQuad` / `localSolidQuads`
- Inner C single-side opening; logo pose sides 0 / 5 / 5
- Door alignment, collision, discrete 45 degree rotation, no free-spin / bottom-lock
