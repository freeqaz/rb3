# W4.4-TEXTCOLOR — PLAN

Wave-16 Lane T. Close out ROWFIX Part B (focused-row DARK text on the Part-A
bright bar) so `RB3_ROWFIX` becomes flippable, and deliver a READY_FOR_FLIP
package for the coordinator to flip Part A + Part B together after E1.

Binding acceptance: A1 (RndText glyph shader is NOT the gap — it multiplies
material.color per draw; the drop is list-path specific), A2 (no global multiply
changes). Grant: rb3 `src/system/ui/`; engine only if truly needed (it was NOT).

## Tasks
1. Observation-first diagnosis — which submesh/material renders the focused-row
   glyphs, and what color is bound.
2. Fix, flag-first default-OFF (folded into `RB3_ROWFIX` as Part B).
3. Enable Part A + Part B together; run the full READY_FOR_FLIP gate suite.

## Result: READY_FOR_FLIP (see STATUS.md)
Fixed rb3-side in `UILabel.cpp` + `UIListSlot.cpp` behind `RB3_ROWFIX`. No engine
edit, no classjson, no new flag. Coordinator flips `RB3_ROWFIX` after E1.
