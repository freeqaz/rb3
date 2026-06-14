# verify-c8-land — independent adversarial verification of the C8 land

Independent reviewer (render-polish wave 4 verify), 2026-06-14. Ports 8911-8919.
Verifies `task-c8-land-impl.md` claims against a fresh capture on the composed
master build + a clean pre-C8 BEFORE binary built in a throwaway worktree. I did
NOT reuse the implementer's screenshots, logs, or section hashes — every number
below is re-derived.

**VERDICT: CONFIRM_WITH_RESIDUALS.** The C8 fix is genuinely landed on master,
is byte-identical on the Wii arm (independently reproduced), measurably reduces
band garment guard-drops (my clean A/B shows a *larger* improvement than the
implementer claimed: −34% vs their −15%), the residual is confirmed to be the
left-limb IK class, and there are no new crashes / NaN smears / regressions. The
"residual" qualifier is because band members are *less smeared but still not fully
dressed* (the IK legwear/handwear class persists, as documented) — i.e. the fix
does exactly what it claims, no more.

---

## 1. PRESENT IN SOURCE — CONFIRM (with one minor doc inaccuracy)

- `491288ec` is a real commit: `fix(native char): capture rest pose in CHARACTER
  space, not world space (C8 root cause)`, 1 file
  `src/system/bandobj/BandCharacter.cpp`, +48/−2, all `#ifdef HX_NATIVE`.
- The working tree `src/system/bandobj/BandCharacter.cpp` matches `491288ec`
  byte-for-byte (`git diff 491288ec` empty). `NativeCharSpaceRestXfm` (L785),
  the character-space capture at both sites (L861 in
  `NativeCaptureRestPoseAfterDeform`, L1245 in `RebindHeadHandsAtRest`), and the
  mid-clip skip (`if (mDriver && mDriver->FirstPlaying()) { miss++; … "clipPlaying"; continue; }`
  at L1240) are all present.
- The composed `native/build-native/rb3-native` (built today 09:24) contains the
  symbol `_ZL22NativeCharSpaceRestXfmP16RndTransformable`; `cmake --build … rb3-native`
  reports "no work to do" ⇒ the live binary is current with master source.
- Engine pin `469c5506` in `native/CMakeLists.txt` == engine repo HEAD; engine
  working tree clean. No drift.

**Minor inaccuracy:** the impl doc says "master HEAD == the landed commit". Master
HEAD is actually `d4c42fa8` (a *docs* commit), whose parent is `491288ec`. The
*code* state of master is the C8 fix (verified above), so the claim is materially
true but literally imprecise.

## 4. WII BYTE-IDENTICAL — CONFIRM (independently reproduced, two ways)

Method A — pre-C8 vs C8 source-compiled object, section SHA-256. I built the
pre-C8 `BandCharacter.o` from `491288ec^` in a clean worktree and the C8
`BandCharacter.o` from master, both via `tools/ninja-locked` (real MWCC):

| section | pre-C8 | C8 (master) | result |
|---|---|---|---|
| `.text`   | `c70e161c2ee16199` | `c70e161c2ee16199` | IDENTICAL |
| `.rodata` | `951afe1a20c1b21a` | `951afe1a20c1b21a` | IDENTICAL |
| `.data`   | `6d43d4409b620568` | `6d43d4409b620568` | IDENTICAL |
| `.sdata`  | `3e7077fd2f66d689` | `3e7077fd2f66d689` | IDENTICAL |
| `.sbss`   | `762b023699a0e48a` | `762b023699a0e48a` | IDENTICAL |
| `.rela.text`/`.rela.rodata`/`.rela.data` | match | match | IDENTICAL |

(My C8 `.text c70e161c2ee16199` also matches the hash the impl doc lists, so I
reproduce their exact build.) Adding the HX_NATIVE block changes ZERO Wii machine
code, relocations included.

Method B — objdiff fuzzy. After force-recompiling the C8 source AND regenerating
the split target reference (`dtk dol split`), `report.json` reports
`main/system/bandobj/BandCharacter fuzzy=99.67018` — the documented baseline,
unchanged. Gate PASS.

## 2/3. GARMENTS + NUMBERS — CONFIRM (stronger than claimed)

Clean A/B: AFTER = composed master (`rb3-native`, ports 8911-8918); BEFORE = a
pre-C8 worktree binary (`491288ec^` BandCharacter.cpp reverted, no
`NativeCharSpaceRestXfm` symbol — verified). Band (non-crowd) drops/frame =
`[SHARD_GUARD] dropped` lines, excluding fist/clap/lighter, `*_crowd_body*`, and
`crowd_*`/`*_extras*` dirs, ÷ distinct band-drop frames. `SHARD_DBG=1`,
`--diff hard --game-burst 24`, randomized roster per boot.

| build | port | band drops/frame |
|---|---|---|
| BEFORE (pre-C8) | 8912 | 38.77 |
| BEFORE (pre-C8) | 8915 | 26.70 |
| BEFORE (pre-C8) | 8917 | 28.15 |
| **BEFORE mean** | | **31.21** (median 28.15) |
| AFTER (C8) | 8911 | 18.89 |
| AFTER (C8) | 8913 | 29.27 |
| AFTER (C8) | 8914 | 19.43 |
| AFTER (C8) | 8916 | 16.27 |
| AFTER (C8) | 8918 | 18.87 |
| **AFTER mean** | | **20.55** (median 18.89, range 16.27–29.27) |

**Reduction: 10.7/frame mean (−34%), median −9.3.** This is a LARGER effect than
the implementer's reported 27.2→23.0 (−15%) — because my BEFORE binary is a true
pre-C8 build, not a bimodally-confounded "stayed-at-baseline" AFTER boot. The
AFTER bimodality the impl doc describes reproduces exactly: 4/5 AFTER boots land at
16-19/frame (clip-free first-resolve → garments saved, ≈ scout's 20.4), one (8913,
29.27) first-resolved mid-clip and stayed at baseline — still below the BEFORE mean
and strictly no worse than pre-C8. No AFTER boot exceeded the worst BEFORE boot.

**Residual = left-limb IK class — CONFIRM.** AFTER build + `RB3_NO_IK=1` (port
8919) drops the band rate to **7.64/frame** (from ~18-20 with IK on, −60%). The
remaining top meshes are footwear/legwear (`maleslipons2`, `nailboots`,
`hotpants_socks`, `suitpants`) on `bone_L-*` chains, plus `fingernails` (hand-IK:
~18-30k drops → 2166 under NO_IK). This matches scout-c8 §5 (their 20.4→4.9; mine
~18→7.6 — same order). The residual is the documented IK mispose, NOT a new failure
mode.

**Visual:** the gameplay bursts mostly frame the highway (a known harness gap), but
band members appear at frame edges. AFTER low-drop boots (`/tmp/c8rev/4/burst_18.png`,
`1/burst_02.png`): band members render with heads/hair + visible clothed torsos,
crowd full-bodied and distinct (sibling fix intact), no shard fans / pale slabs /
NaN smears. BEFORE boots (`/tmp/c8rev/before1/burst_02.png`,`/burst_08.png`): more
partial/floating-limb band geometry + a visible crumpled garment smear. Contact
sheets: `/tmp/c8rev/contact_before.png`, `/tmp/c8rev/contact_after.png`. The heavy
PINK in some AFTER frames is the separate, pre-existing venue-lighting BLOWOUT
(WAVE3 `verify-venue-wash` FAIL), not a C8 regression.

## 5. NO REGRESSION — CONFIRM

- **Crashes:** 0 SIGSEGV/SIGABRT/abort/terminate across all 8 AFTER runs; every run
  reached `game_screen` and PASSed. An extended 60-shot run (port 8911) played to
  songMs=85355 (~85s) with 0 crashes. (The endgame score-screen abort is
  pre-existing + instrument-agnostic per WAVE3 — not reached here, not a C8 issue.)
- **NaN/inf smears:** 0 `worldExt=(nan|inf)` lines in any AFTER run.
- **Worst-case smear unchanged:** max `worldExt = 912.85` in BOTH before and after
  — the fix adds no new/larger smear (corroborates the impl doc exactly).
- **Crowd intact:** crowd_body drops ~7-10k both before and after (C8 doesn't touch
  crowd); crowd renders full-bodied + spread in framed shots.
- **Heads/hands coherent:** heads/hair render cleanly; the residual is garment/limb,
  not head.

## 6. New observation (in passing)

- `fingernails_resource.mesh` is the single largest band-drop contributor in nearly
  every AFTER boot (often the #1 mesh) and is hand-IK-driven (collapses to 2166
  under `RB3_NO_IK`). So the IK residual is meaningfully a HAND/finger-IK problem,
  not only footwear — worth scoping into the left-limb IK follow-up, not just
  `bone_L-*` legwear.
- The venue pink-wash blowout reproduces on the composed build (it's a real,
  separate, pre-existing FAIL); it does NOT interact with C8 but makes band-member
  visual judgment harder in lit venues — the drop-rate metric remains the
  authoritative garment gate.

## Evidence paths (my own captures)

- AFTER screenshots: `/tmp/c8rev/{1,2,3,4,5}/`, extended `/tmp/c8rev/long/`.
- BEFORE screenshots: `/tmp/c8rev/before{1,2,3}/` (pre-C8 worktree binary).
- Contact sheets: `/tmp/c8rev/contact_{before,after}.png`.
- Engine drop logs: `/tmp/rb3-kbd2game-89{11..19}.log` (reaped over time).
- Parser: `/tmp/banddrops.py`; section-hash: `/tmp/sechash.py`.
- BEFORE binary: `.claude/worktrees/c8rev-before/native/build-native/rb3-native`
  (pre-C8, no `NativeCharSpaceRestXfm`). Tear down after review.
