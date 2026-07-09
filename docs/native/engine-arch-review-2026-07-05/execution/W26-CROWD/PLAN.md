# W26-CROWD — PLAN

## Lane charter (A8 ownership declaration)

Lane CROWD-MERGE. **Owns ONLY** `src/system/char/FileMerger.cpp` +
`src/system/char/CharDriver.cpp`. Reads `gNativeStartLoadTag` output but does NOT
edit `bandobj/BandCharacter.cpp` (PROP owns it). Does NOT touch the RndMesh loader
or `world/Crowd.cpp:884-1000` (protected WorldCrowd/RndMultiMesh oracle).

## Recon-first discriminator (A1) — checkpoint the verdict BEFORE fix code

STEP 0: log the FilePath discriminator for the beat-2.433 crowd-clip kill.
- Add `FMERGE_PROBE`-gated logging in `FileMerger.cpp` at `AppendLoader` (:176) and
  `NotifyFileLoaded`/kill site (:343) — Merger `mName` + prev `mLoaded` + new
  `loading`/`mSelected` FilePaths + owning-FileMerger path.
- Add a `CHARDRV_BT`-gated backtrace at `CharDriver::Replace` — symbolize the exact
  call chain that deletes `crowdN.clp` at beat 2.433.
- Decide: **DUP** (same-file re-merge → suppress via FilePath compare, A3 game-global
  guard) vs **LEGIT** (different-file deferred selection → re-fire, A2) vs
  **NEITHER** (kill is not a FileMerger event at all).

## Fix path (only after the checkpoint)

Per A2, PREFER composing with the existing `RB3_CROWD_CLIP_KEEP` re-arm: fix the
bank RESOLUTION so `mClips` resolves to a crowd-bearing bank, let the flag re-Play.
All new code flag-first, default-OFF, HX_NATIVE, byte-identical `#else`. Do the
E-C3 `gCrowdKeep` prune cleanup opportunistically.

**If the kill site is outside FileMerger.cpp + CharDriver.cpp (ui/world/vignette),
narrow + hand off honestly** (the W25 model) — a bug that is not in the owned
surface is a valid, reported outcome.

## Gates

crowd census `animating>0`; `RB3_ISOLATE_MESH=crowd_body` 8 lit figures;
near-black-material discriminator; hub walkers E1; MANDATORY WorldCrowd A/B flag-ON
(gameplay draw-counts + SSIM unchanged); drawlog-golden 792 flag-OFF;
`batch_objdiff` == baseline on touched units; rb3-tests 116/0; A3 boot A/B if
`NeedsLoading` touched. Rule on `RB3_CROWD_CLIP_KEEP` removal (E-C2) if it lands.
