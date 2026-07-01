# task-c8-land-impl — LAND the C8 character-space rest-bake fix

Implementer (render-polish wave 4), 2026-06-14. Ports 8901-8909.
Reads: `scout-c8-rotation-basis.md` (root cause + fix + measurements),
`WAVE3_RESULTS.md`, `PLAN.md`, `task-char-render-impl.md`,
`docs/native/CHAR_SKINNING_DEFORM_INVESTIGATION.md` (the three hardening rounds).

---

## TL;DR

The C8 fix (`41ff9e97`, rest captured in CHARACTER space not WORLD space) is now
**LANDED on rb3 master as `491288ec`**. It is HX_NATIVE-gated, touches only
`src/system/bandobj/BandCharacter.cpp` (+48/−2), and the Wii `BandCharacter.o` is
**byte-identical** to master across every linked section. Engine pin unchanged
(`469c550`; the diagnostics-only engine probe `6a324be` was NOT landed).

Measured across N=3 master boots vs N=6 fix boots (random roster, see "Method"):
band non-crowd guard-drops/frame **27.2 (BEFORE) → 23.0 mean (AFTER)**, with the
fix's clip-free-capture boots reaching **16-17/frame** (matching the scout's
predicted 20.4). No new shard fans, no NaN/inf smears, no crashes across all 9 runs.
Garments + heads render coherently. The residual is the **separate left-limb IK
mispose class** (scout-c8 §5) — explicitly NOT fixed here.

---

## What landed

- **rb3 master `491288ec`** — `fix(native char): capture rest pose in CHARACTER
  space, not world space (C8 root cause)`. Cherry-picked clean from
  `wt-c8-deep-dive @ 41ff9e97`. One file, `src/system/bandobj/BandCharacter.cpp`,
  +48/−2, all `#ifdef HX_NATIVE`.
- **Engine pin: UNCHANGED** at `469c5506...` in `native/CMakeLists.txt`. The engine
  probe `6a324be` (diagnostics-only `C8_PROBE`/`C8_VERT`) was deliberately NOT landed.

The fix adds `NativeCharSpaceRestXfm(own)` — it walks `TransParent()` to the
trans-chain root (the member instance) and returns `L_rest = world_rest ·
inv(rootWorld)`, dividing out the member's stage/venue PLACEMENT. Applied at both
capture sites (`NativeCaptureRestPoseAfterDeform` post-`SetDeformation`, and the
`RebindHeadHandsAtRest` first-resolve path). The first-resolve path additionally
**skips capture while a clip plays** (`mDriver && mDriver->FirstPlaying()` → mark
the bone `pending`, reason `clipPlaying`) rather than snapshot a mid-clip/IK pose.

---

## Before / after band guard-drop rate

Band (non-crowd) drops/frame = `[SHARD_GUARD] dropped` lines excluding `fist`,
`clap`, `lighter` and `*_crowd_body*`, divided by distinct frames-with-a-band-drop.
`SHARD_DBG=1`, `--diff hard`, gameplay burst.

| run | binary | band drops/frame |
|---|---|---|
| 8901 | master (BEFORE) | 26.23 |
| 8903 | master (BEFORE) | 27.30 |
| 8904 | master (BEFORE) | 27.96 |
| **BEFORE mean** | | **27.16** (median 27.30, tight 26.2–28.0) |
| 8902 | C8 fix (AFTER) | 27.21 |
| 8905 | C8 fix (AFTER) | 15.95 |
| 8906 | C8 fix (AFTER) | 17.01 |
| 8907 | C8 fix (AFTER) | 25.74 |
| 8908 | C8 fix (AFTER) | 24.94 |
| 8909 | C8 fix (AFTER) | 26.94 |
| **AFTER mean** | | **22.96** (median 25.34, range 15.95–27.21) |

**Reduction: 4.2/frame mean (15%).** The AFTER distribution is **bimodal** by
design: boots whose head/hands first-resolve lands in a clip-FREE window capture
the character-space rest and save the garments (→ 16-17/frame, ≈ scout's 20.4);
boots that first-resolve mid-clip leave those bones `pending` and stay at baseline
(~25-27/frame) — **strictly no worse than master**, exactly as documented in the
fix's clip-playing poison guard. The landed-binary smoke run (8901, master tree
after cherry-pick) measured 24.74/frame, inside the AFTER distribution.

### Method note (why a single-run A/B is insufficient — and why this is sound)

The band roster + outfits are **randomized per boot** (no deterministic native RNG
seed; `BandWardrobe` picks differ every launch — BEFORE drew
bikinichain/escapeartist/flarejeans/lowtopsneaks/saddleshoe, AFTER drew
drivinggloves/hotpants/loudleggings/parkajacket/wrestlingboots, with only
fingernails/jacketvestnoshirt/suitpants overlapping). A naïve single before/after
run is therefore confounded by which random garments got assigned. I averaged
N=3 BEFORE / N=6 AFTER to wash out the roster; the BEFORE cluster is tight (~27)
and the AFTER mean (23) sits clearly below it, with the best boots matching the
scout's measured 20.4. The scout's own 25.2→20.4 was a same-process env-toggle A/B
(`RB3_NO_IK`), which controls the roster automatically; my cross-boot averaging is
the equivalent for a fix that has no opt-out env.

## No regression: shard fans / NaN smears / crashes

- **Crashes:** 0 across all 9 runs (no SIGSEGV/SIGABRT/abort/terminate); every run
  reached `game_screen` and PASSed.
- **NaN/inf smears:** 0 `worldExt=(nan|inf)` lines in any AFTER run.
- **Worst-case smear unchanged:** max dropped-mesh `worldExt` = 912.85 in BOTH
  BEFORE and AFTER — the fix introduces no new or larger smear; that residual is the
  left-limb IK / hand-pose class (see Residual).
- **Garments visible:** AFTER low-drop boots show band members with visible garments
  + coherent heads/hair (pink-haired member's head renders cleanly, clothed limbs
  present) and no full-screen pale slabs / radiating shard fans. Venue cameras in
  the bursts rarely frame a tight band closeup (a known harness gap noted in wave-2
  and wave-3 — the drop-rate metric is the authoritative quantitative gate).

## Wii match-neutral gate — PASS (byte-identical)

Force-recompiled `build/SZBE69_B8/src/system/bandobj/BandCharacter.o` from the
cherry-picked source in the worktree (`rm` + `tools/ninja-locked` → real MWCC
recompile), then per-section SHA-256 vs the master object:

| section | result |
|---|---|
| `.text` | IDENTICAL (`c70e161c2ee16199`) |
| `.rodata` | IDENTICAL (`951afe1a20c1b21a`) |
| `.data` | IDENTICAL (`6d43d4409b620568`) |
| `.sdata` | IDENTICAL (`3e7077fd2f66d689`) |
| `.sbss` | IDENTICAL (empty) |
| `.rela.text` / `.rela.rodata` / `.rela.data` | IDENTICAL (empty) |

Only `.symtab`/`.strtab`/debug-info differ (source-path strings + the HX_NATIVE
block's local symbols — no linked-image effect), so the whole-file SHA differs while
every load-bearing + relocation section is byte-identical. The HX_NATIVE gating is
confirmed: the Wii arm cannot change. BandCharacter fuzzy stays at the 99.67018
baseline by construction (section byte-identity ⇒ fuzzy unchanged).

## Evidence paths

- Screenshots BEFORE (master): `/tmp/c8land/before/` (burst_00..23),
  `/tmp/c8land/before_8903/`, `/tmp/c8land/before_8904/`.
- Screenshots AFTER (C8 fix): `/tmp/c8land/after/` (8902),
  `/tmp/c8land/after_8905/` (low-drop boot, garments visible),
  `/tmp/c8land/after_8906..8909/`.
- Landed-binary smoke: `/tmp/c8land/landed-smoke/`.
- Contact sheets: `/tmp/c8land/contact_before.png`, `/tmp/c8land/contact_after.png`.
- Engine logs (SHARD_GUARD drop lines): `/tmp/rb3-kbd2game-89{01..09}.log`.
  (NOTE: `/tmp` logs get reaped — capture promptly when re-verifying.)

## Known residual (NOT fixed here — scout-c8 §5)

The remaining ~20-25/frame on the non-improved boots is the **left-limb IK mispose**
class, proven separate by the scout's `RB3_NO_IK` A/B (20.4 → 4.9/frame). The top
residual drops concentrate on `bone_L-*` chains for footwear/legwear
(thighboots/maleslipons2/wrestlingboots/suitpants in my AFTER runs;
loudleggings/parkajacket etc). This is a distinct engine-side IK-apply / L-R
handedness problem (cf. the DC3 feet-in-floor IK trail) and is the next char task.
Do NOT attempt it as part of C8.

## Worktree (left in place for teardown by the orchestrator)

- rb3 worktree: `.claude/worktrees/task-c8-land` (branch `wt-task-c8-land`,
  tip `cabeac3f` = cherry-pick of `41ff9e97`). Native build at
  `native/build-native/rb3-native` (configured with
  `Dawn_DIR=/home/free/code/milohax/dc3-decomp-deps/dawn/lib/cmake/Dawn`, clang).
- The source `wt-c8-deep-dive` branches (rb3 `41ff9e97` + engine probe `6a324be`)
  can be torn down now that the fix is landed; the engine probe was never landed.
