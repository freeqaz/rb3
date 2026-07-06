# W2.1 — Skinned-placement contract (SYS-1 placement half) — STATUS

Append-only. Update under `flock /tmp/rb3-docs.lock`. One `## <subtask-id> — done|partial|blocked`
section per subtask with commit SHAs + blockers. Re-runs read this + `git log --grep=W2.1` and skip
done work.

Plan: `PLAN.md` (this dir). Engine pin `6221a56` (do NOT bump — coordinator only).
Lane A item 1 (engine), sequential BEFORE W2.3. Flag: `RB3_PLACEMENT_CONTRACT` (default-OFF).

## Planning — done
- PLAN.md written (Opus planner). Verified against engine HEAD `6221a56`: bug site
  `Rnd_Wgpu_RB3.cpp:2847-2848` (`else if (skinned) { identity }`), palette `:3159-3160`/`:3315`,
  double-transform trap comment `:2797-2800`, crowd ground truth `Crowd.cpp:403` (`SetWorldXfm(spXfm)`),
  flag template `RB3_FIXED_CLOCK` in `NativeCompatFlags.classification.json`. Subtasks S1 (gate-first,
  fail-red RED on current build) → S2 (coupled contract, default-OFF, atomic) → S3 (flag-ON exit
  evidence, A/A discipline). All opus.

## W2.1.S1 — done

Gate-first: stood up the placement correctness gate the splash golden cannot provide, and proved
it goes RED on the UNCHANGED `6221a56` build. No behavior change committed.

**Commits (rb3):**
- `4c0d268d` — probe + oracle + test + capture harness (gate infra).
- `519add4c` — frame-scope the probe log to the capture window (script-only refinement).

**Files:**
- `src/system/world/Crowd.cpp` — `HX_NATIVE` + `RB3_PLACEMENT_PROBE`-gated diagnostic `fprintf` at
  the `curChar->SetWorldXfm(spXfm)` site (the actual line is `:403`, not `:404`), dumping
  `inst=<i> x/y/z = spXfm.v`. Cached-static getenv (default-OFF), Wii-compile-inert. **No behavior
  change** (does not touch spXfm or the draw).
- `native/tests/placement_oracle.h` (new) — the "right, not just different" oracle. Correlates the
  probe (faithful per-instance spXfm) with the drawlog (`drawlog_compare.h` DrawLogFrame). Three
  defect kinds: `kPosedNotDrawn` (coverage — far-posed positions with no matching drawn skinned
  obj.world), `kDrawnCollapsed` (span — drawn skinned bbox extent vs posed extent), `kDrawnColocated`
  (distinctness — < 2 clusters). Reference-sanity guard returns INCONCLUSIVE (not a false pass) when
  the capture never reached a spread crowd frame.
- `native/tests/test_placement_oracle.cpp` (new, wired into `rb3-tests` in `native/CMakeLists.txt`)
  — 6 synthetic always-run tests (probe parse; catches current-build co-location naming all 3 kinds;
  passes on correct spread; tolerates sub-eps jitter; inconclusive-not-pass on empty crowd; catches
  shared-non-origin collapse) + the env-gated live gate `RealCaptureSpansBowl` (SKIPs unless
  `RB3_PLACEMENT_DRAWLOG` + `RB3_PLACEMENT_PROBE_LOG` are set).
- `scripts/native/placement-gate-capture.py` (new) — boots rb3-native headless (`RB3_HTTP`,
  `RB3_DRAWLOG`, `RB3_PLACEMENT_PROBE`, `RB3_FIXED_CLOCK`), navigates to gameplay (reuses
  `keyboard-to-gameplay` nav), pins a wide venue shot (`rb3_director_disable` FIRST then
  `rb3_force_shot`), GETs `/api/drawlog`, extracts the probe lines, runs the live oracle.

**Build:** `cmake -B native/build-agent-W2.1 -S native -DCMAKE_C_COMPILER=/usr/bin/clang
-DCMAKE_CXX_COMPILER=/usr/bin/clang++` → `rb3-native` + `rb3-tests` both build clean.

**Gate evidence — synthetic (always-run) GREEN:**
`rb3-tests --gtest_filter='PlacementOracle.*'` → 6 passed, 1 skipped (live gate). Combined with
`DrawLogGolden.*` → 15 passed / 2 skipped, **no regression** in the existing draw-log net.

**Gate evidence — FAIL-RED on the unchanged `6221a56` build (the free proof):**
`python3 scripts/native/placement-gate-capture.py` navigated to `game_screen` (songMs≈21441), pinned
`coop_all_n00.shot`, captured 387 draws (227 skinned) + the probe. `RealCaptureSpansBowl` went RED:
```
posed=103064 far=103064 matched=0 skinnedDraws=227 clusters=1 posedExtent=2277.623 drawnExtent=0.000 [FAIL]
  - posed-not-drawn: only 0/103064 far crowd positions have a drawn skinned obj.world within 1.000
  - drawn-collapsed: drawn skinned bbox extent 0.000 < 0.50 * posed extent 2277.623 (1138.811)
  - drawn-colocated: drawn skinned translations form only 1 cluster(s) at eps 1.000
```
The crowd is posed across a 2277-unit bowl (probe positions e.g. x∈[-1816,…], z≈-1016, y up to 1980)
but every one of the 227 skinned draws is drawn at the origin — the exact SYS-1 co-location the gate
was built to catch. **Gate proven RED on the current build.**

**Deviations / notes (no scope creep):**
- The plan cited the SetWorldXfm site as `Crowd.cpp:404`; the actual line is `:403` (verified). The
  bug site `Rnd_Wgpu_RB3.cpp:2847-2848` matches the plan.
- **Did NOT commit `native/tests/goldens/drawlog/gameplay_crowd.json`.** The plan lists it as an
  optional RED reference; committing the raw 387-draw drawlog would bake in the W0.3c draw-submission
  ORDER nondeterminism (unfixed) as a flaky golden. The oracle is order-insensitive (set/bbox-based)
  and derives everything from a fresh capture + probe, so it is the real gate; no exact golden needed.
  The capture script supports `--update-red-golden` if a reference file is later wanted.
- **Probe accumulates per-frame** (100k+ lines/session); `519add4c` frame-scopes the extracted probe
  to the settle+capture window. For S2's flag-ON GREEN threshold this scoped probe is a small superset
  of the exact captured frame; if a wide shot only frames part of the bowl, `spanFrac` (default 0.5,
  env-overridable in `OracleOptions`) may need lowering against the actual fixed-build capture — an S2
  tuning decision, not an S1 gap.
- **Drum prop check:** S1 emits only the crowd probe (the clean, free RED). The oracle format carries
  a `kind` field ("crowd" today, "drum" reserved) so S2 can add a drummer-prop-bone probe without
  touching the oracle. On the current build the drum prop also collapses to origin (all skinned →
  identity), so it is covered by the same span/collocation RED; a drum-specific bone/waypoint
  correlation is deferred to S2's contract work.

## W2.1.S2 — pending
## W2.1.S3 — pending
