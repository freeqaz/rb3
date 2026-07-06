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

## W0.3d.S2 — done (DIAGNOSIS complete + minimal fix STAGED as W0.3d-fix, per F1 not landed)

**Deliverable.** Root-cause diagnosis of mechanism-2 (the async-loader/worker
completion-order draw-submission-order flake, W0.3c.S1) + a minimal, verified fix
STAGED as `W0.3d-fix.patch` (single file, `src/system/rndobj/Utl.cpp`) and handed to
the coordinator. **No code landed** (F1: the fix changes exact draw-submission order,
which invalidates S1's just-recaptured `splash_screen.json` golden and interacts with
W2.1's new gameplay golden — a cross-lane sequencing concern the coordinator owns).
Build: `native/build-agent-W0.3d` (clang). `MILO_ENGINE_PIN` unchanged (`6221a56`).

### Root cause — mechanism-2, precisely pinned (file:line + measured)

The draw-order flake is an **address-dependent draw SORT consuming heap-allocation-order
nondeterminism introduced by the native ThreadCall worker pthread.** Chain:

1. **Genuine concurrency source (not tamed by `setarch -R`).** Native (non-Emscripten)
   spawns ONE real `pthread` worker in engine `src/platform/ThreadCall_Native.cpp`
   (`WorkerMain`, sem-driven). `setarch -R` disables ASLR (deterministic *base*) but does
   NOT make pthread scheduling deterministic.
2. **Milo DTA parse is offloaded to it.** `DataLoader::LoadFile`
   (`src/system/obj/DataFile.cpp:786`) does `ThreadCall(unk38)` → the `DataLoaderThreadObj`
   parses the file buffer into a `DataArray` tree on the worker, concurrently with the main
   thread. glibc gives the worker its own malloc arena; the worker↔main allocation
   interleaving is scheduler-timing dependent.
3. **⇒ Object/material heap addresses vary run-to-run under load.** MEASURED (12-boot
   `RB3_DRAWORDER_TRACE` sweep under CPU contention): **282/282 mesh names map to >1 distinct
   `ptr` across runs** — the exact opposite of W0.3c.S1's "282/282 stable addresses". W0.3c's
   stable-address reading was a **low-contention artifact**: in the *quiescent* regime the
   scheduler happens to interleave identically, so both addresses AND order are stable (my
   40 quiescent boots = 1 order variant); the flake only manifests under scheduler pressure,
   where addresses shuffle.
4. **The address-dependent consumer = `SortDraws` pointer tiebreak.**
   `SortDraws` (`src/system/rndobj/Utl.cpp:174`) orders each dir's draw vector by
   `(GetOrder, material, name)`; when two drawables share `GetOrder` but have **different
   materials** it returns **`return mat1 < mat2;` — a raw material-POINTER comparison.**
   Deterministic on Wii (single-threaded, deterministic heap); on native the shuffled
   material addresses flip this compare → each affected dir's `mDraws` re-sorts differently.
   `RndDir::SyncDrawables` (`src/system/rndobj/Dir.cpp:104`) and `RndGroup::SortDraws`
   (`src/system/rndobj/Group.cpp:166`) both feed this comparator.
5. **⇒ Draw traversal walks the permuted `mDraws` → submission-order flake.**

### Evidence (N≥15 RB3_DRAWORDER_TRACE / drawlog sweeps)

- **Flake is a genuine thread race, gated on scheduler contention.** Quiescent: 40 boots
  (`--fixed-clock`, `setarch -R`) → **1 order variant** (0 flake). Under `nproc` busy-loop
  contention: **24 boots → 4 distinct order variants** (12/7/4/1) — reproduces W0.3c.S1's
  "4 variants" (that sweep ran during fleet load). Draw **count invariant** (888 every boot);
  RNG/clock already frozen (W0.3b), so this is purely the thread-scheduling axis.
- **Divergence shape = intra-dir `mDraws` re-sort.** Every variant pair is a **multiset-
  invariant** permutation confined to **contiguous blocks with STABLE boundaries** (idx
  33–41, 65–74, 81–92, 150–158, 523–531, 869–875 — matching W0.3c). Blocks permute
  *internally*; block positions and the rest of the 888-draw stream are byte-identical. Some
  blocks permute as a **cyclic rotation** of a fixed set (e.g. `[c4fe,5247,d0ef,1b84,9a1b]`
  ⇄ `[9a1b,c4fe,5247,d0ef,1b84]`) — a single sort-key shift. This is exactly a per-dir sort
  re-ordering, NOT dir-level or count nondeterminism. The blocks are band-character skinned
  meshes (they contain the S1 eye/face residual names `0x8217aba90e8175d2`,
  `0x4c3b48a1fe2165eb`).
- **Attribution experiment A — serialize the DataFile worker.** Under the harness, running
  `DataLoader`'s parse inline (`unk38->ThreadDone(unk38->ThreadStart())` instead of
  `ThreadCall`) → contention sweep drops **4 variants → 2** (25/5 of 30). Proves the ThreadCall
  worker is a real contributor, but NOT the only address-perturbing allocation (Cache_Wii /
  other allocations remain) — so chasing every async source is whack-a-mole.
- **Attribution experiment B — fix the CONSUMER (the staged fix).** Make `SortDraws`'
  material tiebreak address-INDEPENDENT under the harness (compare material NAME, then the
  unique per-dir draw NAME) → **30/30 identical order under heavy contention** AND 8/8
  quiescent, **same order across both regimes** (68/68). **Fully eliminates the flake in ONE
  file**, regardless of how many async allocations shuffle addresses — because a total-order
  comparator makes `std::sort` output independent of both input order and pointer values.

### Minimal fix (STAGED — `W0.3d-fix.patch`, NOT landed)

`src/system/rndobj/Utl.cpp` `SortDraws`, additions-only (25/0), all inside
`#ifdef HX_NATIVE` + `if (RB3FixedClockActive())`: when `mat1 != mat2`, tiebreak by
`strcmp(mat1->Name(), mat2->Name())`, falling through to the existing unique per-dir
`strcmp(draw1->Name(), draw2->Name())` when material names tie — a total order independent
of heap address. Verified with the fix applied to `build-agent-W0.3d`:
- **Determinism:** 30/30 identical draw order under `nproc` contention + 8/8 quiescent, one
  global order across both regimes.
- **Flag-OFF byte-identical:** `git diff --numstat` = 25/0, 100% guard-confined; a normal
  boot (no `RB3_FIXED_CLOCK`) keeps `mat1 < mat2`; Wii/MWCC never sees the guard (`HX_NATIVE`
  undefined).
- **Gate intact:** `--fixed-clock --canonical-order` PASS 3/3 against the committed golden
  (multiset-preserving); `rb3-tests --gtest_filter='*DrawLog*'` 9 pass / 1 skip (unchanged).

### Handoff to coordinator (why staged, not landed — F1)

- **File-disjointness to confirm.** `Utl.cpp`'s `SortDraws` is the rb3 scene-graph draw-SORT
  that FEEDS the engine traversal; it does NOT touch Lane-A's engine `Rnd_Wgpu_RB3.cpp`
  DrawMesh/`WriteSceneUniforms` (W2.1/W2.3). It is disjoint from the Lane-A exact-file list —
  but the PLAN's landing example is `src/system/utl/` load-path files, so I did not
  self-authorize landing a render-side sort change; coordinator to confirm against the Lane-A
  list.
- **Golden re-capture (the real sequencing reason).** The fix changes the *exact*
  draw-submission ORDER (name-sort ≠ pointer-sort), so **any exact-order golden must be
  re-captured under the deterministic order** — including S1's just-recaptured
  `splash_screen.json` and W2.1's new gameplay golden. The order-insensitive canonical/multiset
  comparator is unaffected (verified PASS), but the coordinator must land this BEFORE (or
  jointly with) re-goldening so all exact-order baselines are captured against the stable order.
- **Flag design for fail-red.** The fix reuses `RB3FixedClockActive()` (no new flag), so
  "flag-ON stable / flag-OFF flaky" cannot be A/B'd *within* the harness. Recommend the
  coordinator add a registered opt-out (e.g. `RB3_DRAWSORT_DETERMINISTIC_OFF`, registered in
  `NativeCompatFlags.classification.json` per F2) so the flake remains demonstrable (flag-OFF
  under contention → 4 variants; ON → 1) as a landed fail-red. The A/B is already proven here
  by the unpatched-vs-patched sweep.

### Also relevant to Part (a) / S1's open lead

S1 flagged a "separate, larger, non-RNG residual" (max |Δworld| ~20u on the same 7 eye/face
names) and hypothesized upstream selection/assignment fed by the same async-loader-completion
nondeterminism. That is consistent with THIS mechanism: those eye/face meshes live in the very
blocks that permute here. The 20u world deltas are NOT explained by SortDraws re-order alone
(re-ordering doesn't change a mesh's own world-xfm), so a residual look-at/target-selection
effect likely remains — but both share the root async-worker allocation-order nondeterminism.
If the coordinator lands the SortDraws fix (deterministic order) the correspondence-matching in
`compare_canonical` becomes trivial (no ambiguous same-key buckets), which may itself shrink the
per-name eps residual. Flagging for the pin-bump cross-diff.

### PLAN deviations

- Diagnosis-only wrt Lane-A honored (F1): the fix is in rb3 `src/system/rndobj/Utl.cpp`, not
  `Rnd_Wgpu_RB3.cpp`/the object-list draw-submission path; staged, not landed.
- **Corrects a W0.3c.S1 datum for the record:** "282/282 stable heap addresses ⇒ NOT
  allocation nondeterminism" holds only in the quiescent regime; under contention (where the
  flake lives) 282/282 addresses VARY. The flake IS allocation-order nondeterminism (surfaced
  through the address-dependent `SortDraws` pointer compare), not pure iteration-order over
  stable addresses.
- `PLAN.md` in this dir is untracked (S1's commit landed only `STATUS.md`); committing it here
  alongside my artifacts so the item's plan is not lost. Not a sibling lane's file.
