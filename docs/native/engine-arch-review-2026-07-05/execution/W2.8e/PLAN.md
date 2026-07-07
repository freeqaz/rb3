# W2.8e — LIKE-SPACE INSTRUMENT + ASSET GROUND TRUTH (Wave 10 Lane A, Opus)

Charter: WAVE10_KICKOFF A.S1 + WAVE10_REVIEW A2/A3/A4. Single writer of
`Rnd_Wgpu_RB3.cpp` for the wave (STEP0 landed WHITE-fix at engine `2998e78`).
Build dir: `native/build-agent-W2.8e`. Probe: `RB3_DUALSKIN_PROBE`.

## Subtasks

- **S-A2 (probe capture gate, `model: opus`)** — parameterize the `wext>60` gate via
  `RB3_DUALSKIN_MINWEXT` so a flag-ON capture (a fix that DROPS wext) is mechanically
  possible. Re-run drawlog-792 (the probe is a *modification*).
- **S-A3 (like-space fixture + re-prove RED)** — re-derive the dualskin coherent
  reference in like-space (remove the placement-yaw confound), re-commit
  `goldens/w2.8-farvert/live_pose.txt`, and RE-PROVE it RED. "No longer RED" ⇒
  premise-death NO_MATCH stop.
- **S-A4 (asset ground truth + verdict)** — define the extraction target operationally,
  compute the asset/provenance-level per-bone ΔR vs what the palette composes, test it
  against the operative error under PRE-REGISTERED tolerances, and write
  `verdict: MATCH|NO_MATCH`.

## PRE-REGISTERED tolerances (written BEFORE the A4 measurement)

The W2.8e FIX (S2, next stage) = a **static, char-space, asset-derived rebake** of the
appendage-mesh bone offset against the per-member bone's OWN authored rest ROTATION basis
(the only unrefuted path; world-space rebake / rigid anchor / per-frame conjugation are all
dead with numbers). The verdict tests whether that fix premise is alive.

**T1 — the shard is real AND placement-free (A3).**
- T1a: committed like-space fixture worst-sep ≥ `kShardThreshold` = **20u** (RED).
- T1b: the operative rest-basis error is placement-free — demonstrated by ≥1 band member
  drawn with **identity `obj.world`** (zero placement) still showing worst-sep ≥ 20u; and the
  char-space (unplaced-coherent) sep must stay **< 2R** (geometric bound) while the
  placed-coherent control **exceeds 2R** (proving the placement, when applied to the
  coherent, is spurious double-counting — i.e. asDrawn-vs-char-space-coherent was already
  placement-free).
- PASS ⇒ candidate (b) survives. FAIL (fixture < 20u after like-space, OR the error tracks
  placement) ⇒ **NO_MATCH premise death**.

**T2 — the error is a rest-BASIS (rotation) conjugation error, not translation/pose.**
- T2a: char-space ΔR(off, bone-own-rest) ≥ **20°** and **pose-INDEPENDENT** (constant to
  within ±10° across ≥3 sampled poses at ≥120° finger curl) — the candidate-(b) signature
  (a pose-*evolution* error would vary with the pose).
- T2b: each factor (off, liveWorld, liveLocal) is a clean rigid transform (det ∈ [0.99,1.01],
  rows unit-length, ortho ≈ 0) — the pose pipeline is NOT the fault (refutes candidate (a)).

**T3 — the fix is viable: the shard COLLAPSES under a rebake against the palette bone's own
captured (asset-derived) rest.**
- T3a: by construction, the like-space coherent reference = the vertex drawn with
  `off := inv(bone-own-rest)`; the committed fixture's `ref` column IS that rebake result.
  The rebake is viable iff the fixture's `ref`-side extent is a **coherent (non-sharded)
  hand** — worst `ref`-vs-ref-centroid ≤ **25u** (a hand is ~15-20u; a coherent ref must not
  itself be exploded). If the `ref` side is ALSO exploded, the captured rest is poisoned ⇒
  the rebake target is not faithful ⇒ **NO_MATCH**.
- T3b (asset provenance): the offset's bake-basis `inv(off)` must be attributable to a
  DIFFERENT rest than the palette bone's own settled clip-free rest `bw`, with the mismatch
  being a **rest-capture/frame or magnet-vs-per-member provenance** issue (an asset-derivable,
  static, per-member-consistent basis) — NOT a per-frame runtime pose. Operationally: log the
  rebind's rest SOURCE for the finger bones (own==bound magnet-seed vs distinct per-member vs
  clip-poisoned) and confirm the baked-against rest differs from `bw` by the operative ΔR
  (±20°). If the baked-against rest already EQUALS `bw` (ΔR≈0 expected but measured ≠0 only
  from a runtime/non-static source) ⇒ NO_MATCH.

**VERDICT rule (mechanical):**
- **MATCH** iff T1 PASS ∧ T2 PASS ∧ T3a PASS ∧ T3b PASS (real placement-free rotation-basis
  error, coherent rebake target, asset/provenance-derivable static basis). S2 may implement
  the asset-derived char-space rebake at the `RB3_APPENDAGE_REST_ROT` site.
- **NO_MATCH** otherwise. Honest-negative writeup, no fix code (5th-class stop).

Verdict written machine-readable to `/tmp/wave10-checkpoints/A-S1.json` and STATUS.md.

## Files touched
- engine `src/platform/Rnd_Wgpu_RB3.cpp` (probe: capture gate + like-space reference +
  placement-frame resolution dump — a modification, drawlog-792 re-run required).
- engine `src/platform/NativeCompatFlags.classification.json` (append-only `RB3_DUALSKIN_MINWEXT`
  probe row; NO gen.inc regen — coordinator).
- rb3 `native/tests/goldens/w2.8-farvert/live_pose.txt` (re-committed like-space fixture).
- rb3 `src/system/bandobj/BandCharacter.cpp` (finger-bone rest-SOURCE provenance log, gated,
  HX_NATIVE, additive — T3b).

## Build/verify
- `cmake --build native/build-agent-W2.8e --target rb3-native`
- probe: `python3 scripts/native/_w28d_probe.py --bin native/build-agent-W2.8e/rb3-native`
- drawlog-792: `scripts/native/drawlog-golden.py` (`--fixed-clock --canonical-order`), flag-OFF.
