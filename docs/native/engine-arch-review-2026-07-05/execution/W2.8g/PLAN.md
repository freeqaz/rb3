# W2.8g — Lane B, STAGE B-S1: Instrument B + axis discrimination

KEY=W2.8g. Engine pin `146fd19`. Build dir `native/build-agent-W2.8g` (own).
Checkpoint `/tmp/wave12-checkpoints/B-S1.json`. **No fix this stage** — diagnosis + instrument only.

## Line-range re-declaration (task 1) — verified on engine HEAD 146fd19, TU = 5,775 lines
`src/platform/Rnd_Wgpu_RB3.cpp`:
- wext CPU 4-bone blend (as-drawn mirror `s(v)`): **:4351-4394** (loop 4358-4392, `wext` at :4394).
- RB3_DUALSKIN_PROBE block: **:4453-4735**.
- RB3_HANDS_ATTACH_PROBE (Tier-1/Tier-2, freshness-validated rest capture): **:4736-4864**.
- Palette build (`bones.bones[b] = colMajor(off_b · liveW_b)`): :3638-3744 (read-only reference).

## Edits I will make (declared before editing)
1. **Rnd_Wgpu_RB3.cpp: INSERT Instrument B** inside the `if (haMatch)` scope, immediately after the
   Tier-2 `[HANDS_ATTACH]` log block closes (after current **:4862**, before the `}` at :4863).
   New block ~90 lines, opt-in `RB3_HANDS_INSTR_B` (getenv, render-inert). Reuses in-scope
   `skinnedView,n,step,wext,bones,owner,nb,haRest[]` (Tier-1's freshness-validated rest). No other
   line touched; flag-OFF byte-identical.
2. **engine tests/test_skin_golden.cpp: APPEND** a composition-oracle test (`ShellInvariant_*`)
   validating the SPACE-vs-DECODE truth table on real hand-region golden verts (task 5). Append-only
   at EOF region; no existing test modified.
3. **engine src/platform/NativeCompatFlags.classification.json**: append-only row `RB3_HANDS_INSTR_B`
   (under flock; NO gen.inc regen — coordinator).

## Instrument B (task 2) — per-vertex shell invariant
- `s(v)`   = as-drawn CPU 4-bone blend (`bones.bones[bi] = off·liveW`) — identical to the wext loop.
- `ŝ(v)`   = authored shell transported by ONLY coherent bone motion:
  `Σ_k w_k · v · inverse(restW_bk) · liveW_bk`, restW from Tier-1's pointer-identity freshness capture.
- Report per-mesh worst/mean `‖s−ŝ‖`, co-sampled with `wext` every 20 frames (A7 co-variation bar).
- **A7 gate**: `‖s−ŝ‖` trajectory must rise/fall WITH wext (95-106u sighting frames) — else the
  metric is not reading the symptom.

## Axis discrimination (tasks 3,4) — pre-registered branches
- **Instrument B RED co-varying with wext** → composition / SPACE axis (Tier-1 uniform 87.3° prior).
- **Instrument B GREEN while wext RED** → axis moves to WEIGHTS / INDICES / DECODE (the CPU mirror
  shares the decode, so B being blind ⇒ error is in the shared decode).
- **Cheap discriminator (task 4)**: within the most-populated single-dominant-bone group, correlate
  bind radius `‖v−v_cent‖` vs residual `‖s−ŝ‖`. SPACE → one rigid rotation ⇒ residual LINEAR in
  radius (Pearson→1, positive slope). DECODE → verts scatter independently (low/no correlation).
- **Clean-body control** (`greaserjacket_resource`): captured identically; if it reads ~0 while
  `hands_naked` reads large, rest-capture is NOT confounding the RED (parallels the Tier-2 control).

## Instrument A (confirmatory only, predicted GREEN)
GPU-vs-CPU-same-palette readback is predicted GREEN by design (wext already smears 106u on the CPU
with zero GPU). Deferred as a separate ~200-LOC build; logged if run. Not the gate.

## DONE (B-S1 landed)
- Instrument B inserted at `Rnd_Wgpu_RB3.cpp:4872-4966` (verified). Oracle appended to
  `test_skin_golden.cpp` (ShellInvariantAxisOracle). classjson row `RB3_HANDS_INSTR_B` appended.
- Verdict: **SPACE/composition axis; DECODE refuted** (orthoResid≈0.0002, isoDistort≈0.0000 on hands
  vs 1-4u clean-body control; oracle truth-table PASS). See STATUS.md. Engine `218494a`, rb3 `dde9aacc`.

## Gates / process
- Own build dir `native/build-agent-W2.8g`; `build-native` only under flock if needed.
- drawlog-golden flag-OFF must stay PASS (792) on the committed binary (probe render-inert).
- Fail-red: Instrument B must read RED on an induced space error (oracle test) and GREEN on decode.
- Commits per review cycle under flock. classification.json append-only. Leave FxSendNative.cpp alone.
