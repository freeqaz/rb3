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

## W2.1.S2 — done

Landed the coupled placement contract behind ONE engine-registered default-OFF flag
`RB3_PLACEMENT_CONTRACT`. flag-OFF byte-identical; flag-ON turns the S1 oracle GREEN with the
band/hands invariance nets green and the wide gameplay shots visually equivalent to flag-OFF.

**Commit (engine `milo-native-engine`, pin NOT bumped — still `6221a56`):**
- `6852caa` — `src/platform/Rnd_Wgpu_RB3.cpp` (Half A + Half B + shard-guard world-extent fix +
  cached meshWorld) + `NativeCompatFlags.classification.json` (flag registered, class:feature /
  read:presence / default:off) + `NativeCompatFlags.gen.inc` (regenerated). `FxSendNative.cpp` left
  untouched (sibling lane).

**What the contract actually is (empirically determined, per PLAN):** a PROVABLY VERTEX-INVARIANT
reorganization, NOT a vertex-moving fix. `obj.world = mesh->WorldXfm()` (Half A) + bind-relative
palette `skin * inverse(meshWorld)` (Half B) gives, for EVERY bone,
`worldPos = obj.world*(skin*meshWorld^-1)*v = skin*v` — byte-for-byte the flag-OFF result — while
`obj.world` now records the mesh's real placement. Verified numerically with an in-loop self-check
(worst element diff **0.0000** across all meshes; ≤1e-4 for the far crowd props at ±1816).

**Key empirical findings (the PLAN's premises were partly wrong; measured with a one-shot
mesh-world dump + a cancellation self-check, both removed before commit):**
1. **Character meshes have meshWorld == IDENTITY** (band `head/hands/outfit`, `*_crowd_body*`,
   `*_extra_*`): their placement lives in world-space BONES, not the mesh world. So the reorg is an
   exact no-op for them (gated on a non-identity meshWorld) → bit-for-bit identical. Only real
   non-identity mesh worlds take the reorg: bone-attached PROPS (`fist.mesh` v=(-110,-203,3.6),
   `bonesandspikes`) and the per-instance CROWD draws that `Draw3DChars` placed via
   `SetWorldXfm(spXfm)` (`clap`/`lighter` at ±1816, z=-1016).
2. **The palette's identity FALLBACK bones must also cancel** — the null/runaway/V24/skin-clamp
   `continue` paths that leave a bone at identity are initialized to `inverse(meshWorld)` (not I)
   under the flag, else clamped crowd bones fly to `meshWorld*v` while survivors stay at `skin*v`
   → torn shards whose flung emissive geometry feeds the bloom pass into a **full-screen wash**
   (observed, then fixed). This was the coupled-half trap the PLAN warned about.
3. **meshWorld MUST be read ONCE and cached** for both obj.world and its inverse: a later
   same-draw pass (SKEL_WORLDFIX) re-dirties `WorldXfm()`, so two separate reads don't cancel →
   reintroduces the shards+wash. Caching the copy was the decisive fix (clean wide shot after).
4. **The V24 shard guard reads the palette to compute a world bbox** — under the reorg the palette
   is mesh-relative, so the guard must apply `obj.world` to its blended vertex to measure the TRUE
   world extent. Gated on the contract arm; flag-OFF path is literally unchanged.
5. `worldInvTranspose` stays identity for the contract arm (matches the flag-OFF skinned path;
   for the pure-translation meshWorlds that dominate, normals are byte-identical anyway).

**Gate evidence:**
- **flag-OFF byte-identical:** `drawlog-golden.py --fixed-clock --canonical-order` → **888 PASS**
  (canonical multiset match; the ~240-300 divergences are the pre-existing W0.3d CharEyes eye-jitter
  residual, within bound). `lineup-gate.py` → **PASS all layers** (img/segA/ratioB/countC/pin).
- **rb3-tests:** `PlacementOracle.*`/`HandsBindOracle.*`/`DrawLogGolden.*` → 18 passed / 3 skipped
  (live-capture gates).
- **engine invariance suite:** `milo-engine-tests` (context build `build-agent-W2.1-tests`,
  `DC3_DATA`+`MILO_LIB`, `ctest -j1`) → **198 pass / 0 fail / 2 skip** (bar met). SkinGolden +
  ClipPoseFixture green.
- **census:** `native_compat_census.py check` → **exit 0** (flag registered; regen clean).
- **flag-ON oracle:** `RB3_PLACEMENT_CONTRACT=1 placement-gate-capture.py` →
  `PlacementOracle.RealCaptureSpansBowl` **GREEN** — crowd drawn `obj.world` translations == the
  posed `spXfm` (spans the bowl, distinct clusters). Fail-red proven on the pre-change build in S1.
- **flag-ON wides clean:** `coop_all_n00` + `coop_dir_crowd` gameplay shots, flag-OFF vs flag-ON,
  **visually equivalent** — band correctly placed/posed, venue + visible crowd correct, no
  shards / no wash / no holes. (`/tmp/w21-ab/{off,on}_*.png`.)

**Deviations / notes (no scope creep):**
- **flag-ON splash canonical is EXPECTED to diverge (888→792 draws).** The records show the SYS-1
  signature directly: flag-OFF the crowd mesh `0xc57f…` draws 211× ALL at `obj.world=identity`
  (the co-location); flag-ON it draws 115× **spread** across the bowl (`obj.world = spXfm`,
  z=-1016). The vertices are invariant (proven), but obj.world now carries real placement, so the
  far off-screen audience instances flag-OFF wastefully drew at the origin are handled differently
  (impostor/instance bookkeeping keyed on the now-varying obj.world — NOT the shard guard nor the
  venue frustum cull, both ruled out). Zero visible impact (the audience is off-screen behind the
  venue in these pub shots; the wide A/B is identical). This is exactly the exit note's
  "world axis saturates by design — use the other axes." NOT a hard gate for S2.
- **gen.inc / ledger regen also refreshed 86 PRE-EXISTING stale `rb3/src` getenv rows** (char-skin
  probes, loader flags, etc.) from prior lanes that were never regenerated — HEAD's gen.inc/ledger
  were already census-stale before this change. `SCAN_ROOTS` canonically includes `rb3/src/system`,
  so the current generated output is 317 rows; committing it is required for `check` exit 0.
- Name-scoped scrollbar-thumb / hub-bar arms kept structurally intact (flag only rewrites the
  general `else if (skinned)` arm + its palette); their opt-out-no-op proof is S3's job.
- Removed all temp diagnostics (`RB3_MESHWORLD_PROBE`, `RB3_NOSTRIP`, `RB3_CANCEL_PROBE`) before
  commit; no census leakage.

## W2.1.S3 — pending
