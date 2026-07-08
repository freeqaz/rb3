# VISCAP (Wave-18 Lane V) — STATUS

**Verdict: BOX EXHAUSTED (priced). NO branch letter assignable → GT-D CLOSURE**
(pre-authorized VERDICT §4 + kickoff A1). Coordinator executes GT-D; no further
discriminator lanes.

## Summary

Built the visually-guided Wii nav rig the D5 exhaustion report priced FEASIBLE
(`Xvfb + Dolphin -p x11 -v Vulkan + guitar-extension Wiimote + F9 screenshots`) on
the D2 Bank-8 patched-disc boot. **Nav wall D5 hit = SOLVED**: by sight, cleared the
guest-profile / "Use Guest? YES" / guitar-navhelp / calibrate-skip prompt flow D5
could not clear blind, and reached **main_hub** and **live gameplay** (2 songs).

**Real wall (now proven one level deeper):** the Wii Bank-8 debug patched-disc boot
does **not** drive band CharBone skeleton animation in ANY reachable state including
confirmed-live gameplay. `gpmotion`: scene provably live (visual Δ 25.97, game loop
live) yet **0/992 CharBone world transforms move**; finger relRot span 0° everywhere
(hub 0.00°/10.5 s == D2/D4 bind; gameplay 0.0°). `active CharClipDrivers = 0` at every
state. Native's own reference (`D3_delta_table_gameplay.json`) reads the SAME
CharBone-world representation and IS animated (54–99°) — so the join's Wii half is
unobtainable in the required representation, not an instrument bug.

**G-D5-1 FAILS everywhere** (swing 0°, need ≥15°) → no articulated capture → no branch
letter. A1 exhaustion.

## Deliverables

- `milo-trace tools/wii_visgame_capture.py` — the visual nav rig (`session` / `gpcap`
  / `gpmotion` / `run` / `capture` / `noise`).
- `scripts/analysis/v18_hand_classify.py` — VERDICT §4 branch-table classifier, ready
  if an articulated capture is ever obtained.
- `execution/R5-HANDS-ENDGAME/evidence/V_exhaustion.json` — machine contract.
- `execution/R5-HANDS-ENDGAME/evidence/V_findings.md` — full findings.
- `execution/R5-HANDS-ENDGAME/evidence/V_*.png` + `V_gpmotion.json` +
  `V_hub_articulation.json` + `V_gameplay_capture.json` — evidence.

## Followups (coordinator, out of V's box)

1. GT-D closure §3.4 with the §8 clamp-corrected residual statement.
2. (NEW scoped item, optional) root-cause WHY the Wii band CharBones stay at bind
   (`active CharClipDrivers = 0`): debug-build stub vs Guest/patched-disc director
   init vs headless subsystem gating. Could unlock the capture; distinct from V.
