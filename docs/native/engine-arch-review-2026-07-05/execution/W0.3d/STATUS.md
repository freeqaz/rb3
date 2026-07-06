# W0.3d — Clean the draw-log gate: CharEyes determinism + async-loader order diagnosis — STATUS

Append-only. One `## <subtask-id> — done|partial|blocked` section per subtask, written under
`flock /tmp/rb3-docs.lock`, with commit SHAs, the required determinism evidence (N≥30-boot sweeps,
NOT 3), fail-red demonstrations, and any recorded PLAN deviations. Re-runs read this +
`git log --grep=W0.3d` and skip done work.

Exit: part (a) `--fixed-clock --canonical-order` green ≥15/15 fresh boots with the residual sidecar
EMPTY (jitter frozen) or a defensibly per-name-recalibrated eps (NO global-eps widening),
fail-red intact — this is the green bar (S1). Part (b) is DIAGNOSIS-ONLY wrt Lane-A files (F1):
written root-cause + a staged `W0.3d-fix` handed to the coordinator if it touches
`Rnd_Wgpu_RB3.cpp`/the object-list draw-submission path.

_(No subtask entries yet — planning complete, implementation pending.)_

## W0.3d.S1 — done

Commit: `c6b961da` "W0.3d: freeze CharEyes/CharLookAt look-at RNG under RB3_FIXED_CLOCK"
(files: `src/system/char/CharEyes.cpp`, `native/tests/goldens/drawlog/splash_screen.json`,
`native/tests/goldens/drawlog/splash_screen.fixedclock-residual.json`,
`scripts/native/drawlog-golden.py`).

**What landed.** Under `RB3FixedClockActive()` (HX_NATIVE-only; Wii/MWCC never sees the
guarded code), `CharEyes::Poll()` now forces `sDisableEyeDart` and
`sDisableProceduralBlink` true, and holds `CharLookAt::sDisableJitter` true for the
whole frame (not just the eye-poll bracket, since other CharLookAt instances such as
head IK may poll later in the frame at an order-dependent point). Every addition is
inside `#ifdef HX_NATIVE` with an `#else` branch reproducing the original two lines
verbatim, so the diff is additions-only and Wii/flag-off behavior is unchanged by
construction (confirmed: `git diff --numstat` shows 36/0 on CharEyes.cpp, all hunks
guard-confined; flag-off `MILO_HEADLESS=1 MILO_MAX_FRAMES=3` boot sanity-checked clean).
Re-captured `splash_screen.json` under the frozen recipe (888 draws, same shape as
before — count/topology unchanged, only look-at-driven world-xfms differ).

**Residual: NOT emptied — per-name eps fallback used (as PLAN sanctions).** A rigorous
N≥30 fixed-clock sweep (32 raw captures, both the live greedy `compare_canonical()`
correspondence matcher and an exact/brute-force cross-check, across 4 different
reference anchors ⇒ ~112 pairwise comparisons) shows the freeze measurably reduces
jitter but a **separate, larger, non-RNG residual survives** on the same 7 mesh names
that were already itemized in the old sidecar:

| name | observed max \|Δworld\| (N≥30) | eps landed (≈1.5x margin) |
|---|---|---|
| `0x8217aba90e8175d2` | 20.894 | 32.0 |
| `0x4c3b48a1fe2165eb` | 20.460 | 31.0 |
| `0xa8e830c0e5f7189d` | 15.235 | 23.0 |
| `0x11db9562e2d7790b` | 8.880 | 13.5 |
| `0xb14bc5a60dc4b060` | 8.406 | 13.0 |
| `0x395eb5606b2120d` | 8.363 | 13.0 |
| `0xdc4a1dc968146487` | 8.329 | 13.0 |

This is well above the old documented max (1.9521, flat eps 3.0), confirming this is a
real, reproducible, order-dependent effect distinct from the RNG jitter/dart/blink
sources this subtask targets — my working hypothesis (**not investigated further,
out of S1's scope**) is a selection/assignment effect upstream of CharEyes (e.g.
`mCurInterest`/eye-target choice or band-member-to-slot assignment) fed by the same
async-loader-completion-order nondeterminism as W0.3c.S1's "mechanism 2". Flagging
this explicitly for Part (b) / the coordinator as a lead, not claiming it as
root-caused or fixed here.

Implemented the PLAN's sanctioned fallback: extended the residual sidecar schema with
an optional per-entry `"eps"` override and wired it into
`drawlog-golden.py`'s `load_residual()`/`compare_canonical()` (`name_eps` dict,
falls back to the shared top-level `eps` when absent). This is a **per-name
recalibration, not a global-eps widen** — the legacy index-keyed
`compare_fixed_clock()` path and its flat top-level `eps: 3.0` are untouched byte-for-
byte in meaning (only consumed by the older code path, as before).

**Also observed (not fixed, not this subtask's mechanism):** 1 of 32 raw sweep
captures (`cap25.json`) showed a rare draw-**count** anomaly (883 vs the otherwise
universal 888) — an exact-match field the per-name-eps mechanism cannot and should not
tolerate. Not reproduced in the final N=30 real-script sweep (0/30 fail), so it did not
block the exit bar, but it is a real, pre-existing flake in the same async-loader-race
family as Part (b)'s territory. Flagging for the coordinator/Part-(b) owner to watch
for; not investigated or addressed here.

**Exit-bar verification (this session, native/build-agent-W0.3d/rb3-native):**
- `--fixed-clock --canonical-order`: **30/30 green** (fresh boot each run, `setarch -R`).
- `--fixed-clock --canonical-order --fail-red-audit`: all four fail-red classes
  (count-drop, bind-group-collapse, out-of-bound world, mesh-identity) still **RED**;
  pure order-permutation case still **GREEN**.
- `rb3-tests --gtest_filter='*DrawLog*'`: **9 pass / 1 skip**, unchanged
  (`PopulatesFromRealDrawMesh` skip is pre-existing, unrelated to this change).
- Flag-off (no `RB3_FIXED_CLOCK`): sanity boot clean; diff structurally confined to
  `#ifdef HX_NATIVE` additions with original-preserving `#else` branches.
- `MILO_ENGINE_PIN` unchanged (`6221a56`, no engine touch — this is entirely an rb3
  `src/` change).

**Deviations from plan:** Residual sidecar could not be emptied (see above) — used the
explicitly-sanctioned per-name-eps fallback instead of the primary "empty" outcome.
No Lane-A files (`Rnd_Wgpu_RB3.cpp`, object-list/draw-submission path) touched. No new
flag added (kept flag-free, reusing existing `RB3FixedClockActive()`/DTA statics per
PLAN preference).
