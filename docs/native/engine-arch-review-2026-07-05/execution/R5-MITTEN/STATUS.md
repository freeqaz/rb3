# R5-MITTEN — STATUS (Wave-18 Lane M)

**Verdict: READY_FOR_E1.** The GT-D closure package's item 1 — the optional
"mitten fallback" render-layer mitigation for the closed R5-hands family
(`R5-HANDS-ENDGAME/CLOSURE.md` item 1, `PLAN-R5 §3.4` item 1) — is implemented
flag-first, **`RB3_HANDS_MITTEN` DEFAULT-OFF**. The default (ship ON or OFF) is
**E1-decided by the coordinator**, not this lane, and does not reopen the closure
either way.

## What it is (honest classification: WORKAROUND)

Render-side, hands-scoped, **BAND-only** palette blend. Explicitly **NOT an offset
bake**: no anchor capture, no authored-data mutation — a per-frame palette lerp in
the skinned-mesh palette upload path.

Mechanism (engine `Rnd_Wgpu_RB3.cpp`, `DrawMesh` skinned palette compose): for each
band hand-mesh finger bone, compare its composed skin `A_i·O_i(t)` to its side's
**WRIST** bone's rigid composed skin `A_w·O_w(t)` in the wrist frame; when the
rotation-to-wrist exceeds the ramp `TH_LO=45 → TH_HI=90°`, blend that finger's
palette entry toward wrist-rigid. Because the wrist offset `A_w` preserves the
finger's authored bind geometry while the wrist (D4: Wii-faithful ≤0.06°) drives its
motion rigidly, a fully-blended finger degrades to a rigid extension of the wrist —
fingers stop articulating **at exactly the poses that tear/displace** (the 87.2°
seed-R "ceiling hand" + spike-webbing, `HANDS-ADJUDICATION/VERDICT.md §2`); the hand
stays attached and moving.

## Scope (engine-side seam, crowd untouched)

Hook name-classifiers added (engine keeps ALL math; rb3 answers only names):
`IsBandHandMesh` (hands_naked / gloves / fingernails; **crowd/extras excluded by
name** — the 24× crowd rebind is load-bearing), `HandBoneRole` (wrist=`bone_?-hand`,
finger=index/middle/ring/pinky/thumb), `HandBoneSide` (`_L-`/`_R-`). Runtime
confirmed (`evidence/scope_confirmation.txt`): wrist bones `bone_L/R-hand.mesh` on
`char/char/main/skeleton_unshared.milo`, `rebound=1` — band members only.

## Gates

1. **E1 evidence — PASS (visual, coordinator-adjudicated).** Matched-frame ON vs OFF
   pairs at default config, `evidence/{OFF,ON}_burst_*.png`, **both genders**:
   - `burst_08/12` male guitarist — OFF spike-fan / floating ceiling-hand → ON
     coherent attached hand.
   - `burst_45` **female** singer close-up — OFF right hand torn into thin spikes →
     ON compact rigid hand (pose-matched, Δ61ms).
   The spike-fans/ceiling-hands collapse to attached rigid hands. Default is
   **E1-decided** from these.
2. **No-regression on coherent frames — PASS by construction + trigger rate.** The
   blend is a **mathematical no-op when `relAng < TH_LO` (α=0)** → coherent frames
   (fingers near rest) are byte-identical, trigger rate 0 there. Aggregate trigger
   rate **16.4%** of finger-draws (`evidence/trigger_rate.txt`); relAng distribution
   p50=21.8° p90=62° tail→134° (`evidence/relang_distribution.txt`) — the ramp fires
   only on the extreme-pose tail.
   - **Determinism caveat** (`evidence/frame_pairing_note.txt`): the band render is
     non-deterministic across processes (director camera cuts + clip phase); a
     control of two independent OFF runs diffs 48–93% of pixels (median 85%) at
     nearest-songMs pairs — higher than any OFF-vs-ON signal. So a cross-run pixel
     diff **cannot** gate this; per `R5-HANDS-ENDGAME/VERDICT §8` bone-world/pixel
     gates are blind to this class. Gates are SKINNED-OUTPUT (trigger rate) + VISUAL.
3. **Flag-OFF byte-identical — PASS.** `RB3_HANDS_MITTEN` unset → `sMittenOn=0` →
   whole block inert. `drawlog-golden.py --scene splash_screen`: **"PASS: matches
   golden (792 draws)"**. classjson row appended (default-OFF, class `workaround`)
   under `/tmp/milo-engine-classjson.lock` — left uncommitted for the coordinator's
   close-out regen (append-only per process rules).
4. **Tests — rb3-tests PASS** (116 passed / 0 failed / 7 fixture-skipped; the R2
   HandsBind/FarVert/Oracle skinning fixtures all green; the 10 apparent ctest
   failures are GPU-context contention under parallel ctest — all pass standalone).
   **milo-engine-tests NOT buildable in this rb3 sandbox** (pre-existing at HEAD:
   `test_skin_golden.cpp:631` uses undeclared `RotateAboutY`; needs DC3_RUNTIME_ROOT
   / dc3 math context, not present) — independent of this change, which does not
   compile into that suite (`Rnd_Wgpu_RB3.cpp` is not in it; the `GameRenderHook.h`
   change is ABI-additive virtuals with base defaults).

## Calibration

Default `TH_LO=45 / TH_HI=90°` (rotation-to-wrist ramp), overridable via
`RB3_HANDS_MITTEN_TH_LO` / `_TH_HI`. Derived from the live relAng-to-wrist
distribution against the Wave-16 `burst_08/12` displacement/tear evidence: below 45°
the finger sits near its rest basis (coherent, never blended); 45→90° catches the
extreme-pose tail where the seed-R displacement becomes visible. `RB3_HANDS_MITTEN_PROBE`
dumps per-bone relAng/relTr/α + a running trigger rate.

## Commits

- engine (`milo-native-engine`): `Rnd_Wgpu_RB3.cpp` (mitten pre-pass + blend + probe
  + `MittenMatAngleDeg`/`MittenBlendXfm` helpers) + `GameRenderHook.h` (3 additive
  virtuals). classjson appended, left unstaged.
- rb3: `native/src/rb3_render_hook.cpp` (3 classifier impls) + this hub + evidence.

NO default flips, NO pin bumps (coordinator, at close-out). ELEVEN defaults untouched.
`FxSendNative.cpp` / `rb3_session_trace.cpp` never staged.

## Followups (coordinator)

- **E1 default decision** (ship ON/OFF) from the `evidence/{OFF,ON}_burst_*` pairs.
- If ON: the accepted-residual statement flips to "fingers degrade toward rigid-hand
  at extreme poses; hand stays attached and moving" (already drafted in
  `CLOSURE.md §Accepted-residual`).
- Threshold is tunable per venue/cam if E1 wants a more/less aggressive collapse
  (`RB3_HANDS_MITTEN_TH_LO/HI`).
