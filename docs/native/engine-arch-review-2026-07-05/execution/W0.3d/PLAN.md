# W0.3d — Clean the draw-log gate: CharEyes determinism + async-loader order diagnosis — PLAN

**Lane:** B (rb3 + diagnosis, parallel to Lane A). **Model:** planned by Opus.
**Engine pin:** `6221a56` — **do NOT bump** (coordinator bumps once per wave).
**Authoritative brief:** `../WAVE4_KICKOFF.md` "COORDINATOR ACCEPTANCE" (amendments **F1/F2**
binding) + `../WAVE4_REVIEW.md` R-B/§"Missing items" F1. This PLAN is the summary; on any conflict
the kickoff acceptance wins.

## Objective

Make the W0.3c canonical draw-log gate (`scripts/native/drawlog-golden.py --fixed-clock
--canonical-order`) a **clean, non-probabilistic pass**. The gate is currently ~96% RED in-regime
(verifier: 1 PASS / 23 FAIL, `W0.3c/STATUS.md:304-315`) on a **pre-existing** residual, NOT the
draw-order axis W0.3c already neutralized:

- **The residual (part a, the green bar):** every one of those FAILs is a **CharEyes/CharLookAt
  eye/face-mesh `world`-transform divergence** exceeding the sidecar's single global `eps=3.0`
  (measured 3.65, once 3.989 — `W0.3c/STATUS.md:308-311`, `W0.3b/STATUS.md:58`). Zero
  count / bind-group-collapse / mesh-identity / non-residual-world failures across every sweep —
  the four real defect-class checks never false-positive. The flake is confined to the eye-jitter
  eps axis.
- **The order flake (part b, diagnosis-only):** W0.3c.S1 root-caused the *draw-submission-order*
  nondeterminism to **mechanism 2 (100%)** — async-loader/worker **completion-order feeding
  object-list insertion order** (`W0.3c/STATUS.md:63-78`), multi-site (idx 33–875), timing/
  scheduler-sensitive, over stably-addressed objects (282/282 stable heap addrs). The canonical
  comparator already *tolerates* this by design (order-insensitive multiset); part (b) is the
  **root-cause hunt**, and — per **F1** — is **diagnosis-only with respect to any file Lane A
  (W2.1/W2.3) edits**.

### Root-cause of the residual (verified this planning pass — re-grepped, lines are live)

All order-dependent look-at nondeterminism is **RNG-consumption-order** dependent (a fixed seed
`0x5EED` does not help because the *order* in which look-at instances draw from the global RNG
varies with heap/iteration order — matches "order-dependent, NOT clock- or RNG-seed-driven",
sidecar `_comment`). There are exactly **three** RNG sources in the eye/face look-at path, each
already behind a **static disable lever**:

| Source | Site | RNG | Existing lever |
|---|---|---|---|
| Look-at jitter | `CharLookAt::Poll` `src/system/char/CharLookAt.cpp:180-186` | `RandomFloat(±yaw)`, `RandomFloat(±pitch)` | `CharLookAt::sDisableJitter` (`CharLookAt.h:42`) |
| Eye dart | `CharEyes::DartUpdate` / `GenerateDartOffset` `src/system/char/CharEyes.cpp:459-501` (gate :470) | `RandomFloat`×6, `RandomInt` | `CharEyes::sDisableEyeDart` (`CharEyes.h:110`) |
| Procedural blink | `CharEyes::ProceduralBlinkUpdate` `src/system/char/CharEyes.cpp:652-654` | (blink timing) | `CharEyes::sDisableProceduralBlink` (`CharEyes.cpp:30`) |

Precedent for wiring these statics exists in-file: `CharEyes::Poll` already brackets the eye
solve with `CharLookAt::sDisableJitter = sDisableEyeJitter; … ; CharLookAt::sDisableJitter = false;`
(`CharEyes.cpp:820/825`). **Forcing all three statics true under `RB3FixedClockActive()` removes
every RNG draw from the look-at path**, so the eye/face mesh worlds become a function of the frozen
sim state alone — deterministic regardless of traversal/iteration order. This is exactly the
brief's sanctioned "freeze/zero the look-at state under `RB3_FIXED_CLOCK` (the jitter source)"
option, and it is strictly better than eps-recalibration (empties the sidecar instead of bounding
it). eps-recalibration from an N≥30 sample is the **fallback** only if a residual survives the
freeze (a non-RNG source), and it MUST be **per-name** (NOT the forbidden global-eps widening).

### Faithful-reference notes

- This changes **native determinism-harness behavior only**: the freeze is gated on
  `RB3FixedClockActive()` (an existing default-OFF flag, `native/src/rb3_replay.cpp:516`) **and**
  `HX_NATIVE`. Wii/MWCC build is provably untouched (`HX_NATIVE` undefined there; the statics keep
  their existing `false` default and existing DTA/cheat semantics). A normal boot (no
  `RB3_FIXED_CLOCK`) is byte-identical.
- The `RB3_FIXED_CLOCK` seam (`W0.3b`), the loader deterministic drain (`Loader.cpp:622/729`), the
  pinned boot RNG seed (`System.cpp:397`), and the frozen dt (`Task.cpp:403`) are all already in
  place; part (a) only adds the look-at RNG freeze on the same gate.

## Subtasks

### W0.3d.S1 — Part (a): freeze CharEyes/CharLookAt look-at RNG under RB3_FIXED_CLOCK; re-golden; N≥30 sweep
- **model:** sonnet
- **goal:** Under `RB3FixedClockActive()` (native-only), force `CharLookAt::sDisableJitter`,
  `CharEyes::sDisableEyeDart`, and `CharEyes::sDisableProceduralBlink` true so the eye/face look-at
  path draws zero RNG → deterministic eye-mesh worlds. Re-capture the committed `splash_screen`
  golden under the frozen recipe and shrink the residual sidecar to **empty** (delete its `draws`).
  Exit: `--fixed-clock --canonical-order` **green ≥15/15 fresh boots** in an **N≥30** sweep with an
  empty sidecar (or a defensibly per-name-recalibrated eps if a non-RNG residual survives), all four
  fail-red classes still RED. **DEFAULT-OFF by construction:** behavior changes only when
  `RB3_FIXED_CLOCK` is set — no new default flag, flag-OFF byte-identical.
- **exact files:**
  - `src/system/char/CharEyes.cpp` — native-guarded (`#ifdef HX_NATIVE` + `#include "rb3_replay.h"`)
    block that, when `RB3FixedClockActive()`, sets the three statics. Preferred site: early in
    `CharEyes::Poll` (runs every frame, cheap, always current even for late-created instances) — set
    the two `CharEyes` statics there and OR the fixed-clock condition into the existing
    `CharLookAt::sDisableJitter = sDisableEyeJitter` line at `:820` (so the bracket at `:825`
    resetting it to `false` still restores the non-harness default). Confirm with the implementer
    that the `:825` reset does not re-enable jitter mid-frame under the flag — if it does, gate the
    reset too (`= RB3FixedClockActive() ? true : false`).
  - `native/tests/goldens/drawlog/splash_screen.json` — **re-captured** under the frozen recipe
    (jitter/dart/blink off), so the golden reflects the deterministic eye worlds.
  - `native/tests/goldens/drawlog/splash_screen.fixedclock-residual.json` — set `draws: []`
    (empty) once the freeze proves clean; keep the file (with an updated `_comment` explaining the
    freeze) so the loader path and fail-red machinery are unchanged. **Do NOT** widen `eps`.
  - *(optional, only if an A/B opt-out is wanted)* a new getenv `RB3_FIXED_CLOCK_KEEP_LOOKAT_JITTER`
    → then it MUST be registered in `milo-native-engine/src/platform/NativeCompatFlags.classification.json`
    at introduction + `NativeCompatFlags.gen.inc` + `NATIVE_COMPAT_LEDGER.md` regenerated via
    `scripts/analysis/native_compat_census.py gen` (F2). Keep the primary path flag-free if possible.
- **step-by-step:**
  1. Read `CharEyes::Poll` (`src/system/char/CharEyes.cpp`, the region around `:815-825`) and
     confirm the three static names + the `:820/:825` bracket. Grep `sDisableEyeJitter` to see the
     member driving `:820`.
  2. Add the `HX_NATIVE` include of `native/src/rb3_replay.h` (mirror the pattern in
     `src/system/utl/Loader.cpp:36` / `src/system/os/System.cpp:392`) and the guarded set of the
     three statics under `RB3FixedClockActive()`.
  3. Build own dir: `cmake -B native/build-agent-W0.3d -S native -DCMAKE_C_COMPILER=/usr/bin/clang
     -DCMAKE_CXX_COMPILER=/usr/bin/clang++ && cmake --build native/build-agent-W0.3d --target
     rb3-native rb3-tests -j8`.
  4. **Fail-red FIRST (prove the freeze is load-bearing):** before re-goldening, run the current
     `--fixed-clock --canonical-order` N≥30 sweep on the NEW binary against the OLD (jitter-captured)
     golden+sidecar → still ~flaky? Then empty the sidecar and re-capture the golden.
  5. Re-capture: `python3 scripts/native/drawlog-golden.py --fixed-clock --update` (frozen recipe,
     `setarch -R`, `MILO_MAX_FRAMES=60`, `RB3_FIXED_CLOCK=1`, stabilizer env
     `RB3_GAMEWARM_OFF/RB3_TEX_PREWARM_OFF/RB3_ASYNC_OPEN_OFF`). Empty the sidecar `draws`.
  6. **N≥30 sweep** (NOT 3 — the W0.3b/W0.3c trap): loop `--fixed-clock --canonical-order` ≥30
     fresh boots (`setarch -R`), record the full PASS/FAIL sequence; require ≥15/15 consecutive-clean
     AND a low overall FAIL rate (target 0/30; any FAIL must be inspected — if it's a surviving
     non-RNG eye residual, switch to the per-name-eps fallback below, NOT a global widen).
  7. **Fallback (only if a residual survives the freeze):** extend the sidecar schema to per-name
     eps (`draws:[{name, eps}]`) fitted from the N≥30 sample's per-name max |delta| ×~1.5 margin,
     and teach `compare_canonical`'s residual lookup to read per-name eps. This is the amendment-F1
     "per-name eps from an N≥30-boot sample" path; do NOT touch the global `eps` field's meaning for
     the legacy `compare_fixed_clock` path.
- **verification commands:**
  - `python3 scripts/native/drawlog-golden.py --fixed-clock --canonical-order --fail-red-audit`
    → all 4 defect classes RED + permutation GREEN (unchanged from W0.3c).
  - N≥30 sweep script (record sequence): `for i in $(seq 30); do setarch -R python3
    scripts/native/drawlog-golden.py --fixed-clock --canonical-order; echo $?; done` → ≥15/15
    clean, 0 unexpected.
  - `native/build-agent-W0.3d/rb3-tests --gtest_filter='*DrawLog*'` → 9 pass / 1 skip (unchanged).
  - Flag-OFF byte-identical: `git show --numstat` on the `CharEyes.cpp` commit = additions only,
    all inside the `HX_NATIVE`+`RB3FixedClockActive()` guard; a plain `MILO_HEADLESS=1
    MILO_MAX_FRAMES=3 rb3-native` (no `RB3_FIXED_CLOCK`) output identical to pre-change.
  - If a flag was added: `python3 scripts/analysis/native_compat_census.py check` → exit 0 +
    `--selftest` pass.
- **fail-red demo:** `--fail-red-audit --canonical-order` must stay RED on all four defect classes
  (count-drop, bind-group-collapse, out-of-bound world on a non-residual draw, mesh-identity) and
  GREEN on the pure permutation — proving the empty sidecar did not blunt the gate.
- **gate staging:** no default flip — the change is inert unless `RB3_FIXED_CLOCK` is set (already
  default-OFF). This is the item's **green bar** (must land).

### W0.3d.S2 — Part (b): DIAGNOSIS-ONLY root-cause of async-loader completion-order draw-flake
- **model:** opus
- **goal:** Produce a **written diagnosis** (in `STATUS.md`) of mechanism-2 — how async-loader/
  worker **completion order** feeds **object-list insertion order** and thereby the draw-submission
  order flake (W0.3c.S1) — plus a **minimal proposed fix as a staged patch**. **Land nothing that
  touches a Lane-A file** (`milo-native-engine/src/platform/Rnd_Wgpu_RB3.cpp` or the object-list/
  draw-submission path W2.1/W2.3 edit). If the minimal fix wants those files → **STOP**, attach the
  patch to `STATUS.md` as **`W0.3d-fix`**, and hand it to the coordinator for post-Lane-A
  sequencing (the exact discipline W0.3c.S2 honored). If (and only if) the fix is fully contained in
  a load-path file NO lane touches (e.g. loader completion drain ordering in `src/system/utl/`),
  it may be staged default-OFF behind a registered flag — but confirm the file-disjointness with
  the coordinator's Lane-A exact-file list before landing.
- **exact files (READ-ONLY for diagnosis; any patch is STAGED, not landed if Lane-A):**
  - investigate: `src/system/utl/Loader.cpp` (async loader / `LoadMgr::Poll` drain, worker
    completion), the `ThreadCall`/worker-completion path feeding it, and the scene-graph object-list
    append/insertion site that draw traversal iterates (grep `ObjectDir`/`mObjects`/append/`push`
    on the render-traversed list). Cross-reference the W0.3c divergence spans (idx 33-37, 75-89,
    93-102, 150-158, 523-531, 869-875).
  - engine `Rnd_Wgpu_RB3.cpp` and the draw-submission path — **READ-ONLY** (Lane A owns it).
  - write: `STATUS.md` (diagnosis) + `W0.3d-fix.patch` (or a `W0.3d-fix/` note) staged for the
    coordinator.
- **step-by-step:**
  1. Reproduce with the W0.3c harness: `RB3_DRAWORDER_TRACE=1` (engine probe at `Rnd_Wgpu_RB3.cpp`
     DrawMesh entry, landed W0.3c `5cee522`) + `--fixed-clock` boots under `setarch -R`; bisect the
     `draws[].name` sequences for the order variants (W0.3c/STATUS.md "Probe usage").
  2. Trace the earliest-divergence index (33) back from the draw stream to the object-list the
     traversal walks, then to where that list is **appended** during load, then to the async
     completion that triggers the append. Confirm the ordering key is a completion-order artifact
     (not allocation-order — W0.3c proved 282/282 stable addresses).
  3. Identify the **minimal** stabilizer: a deterministic insertion key at the append site (e.g.
     load-request index / path / name) OR a deterministic completion-drain order in the loader.
     Pick the one that lands in the FEWEST files and, ideally, in a **non-Lane-A** file.
  4. Determine the landing file. **If Lane-A → STOP, stage as `W0.3d-fix`, record in STATUS, hand to
     coordinator.** If non-Lane-A and coordinator-confirmed disjoint → may land default-OFF behind a
     registered flag with its own fail-red (the flake reproduces under the flag-OFF path; the flag-ON
     path is order-stable across the N≥30 sweep).
- **verification commands (diagnosis evidence):**
  - N≥15 (prefer ≥30, regime-clustered per W0.3c) `RB3_DRAWORDER_TRACE` sweep showing the divergence
    spans + the object-list append site that produces them (cite file:line).
  - If a non-Lane-A patch is staged and landed: N≥30 `--fixed-clock` (exact-order, NOT just
    canonical) sweep green ≥15/15 with the flag ON, flag-OFF still flaky (fail-red).
- **gate staging:** diagnosis is the deliverable; any patch is default-OFF + coordinator-sequenced.
  This subtask does NOT gate the item's green bar (S1 does).

## Exit criteria

**Flag-OFF (no `RB3_FIXED_CLOCK`) — byte-identical (hard):**
- The `CharEyes.cpp` change is additions-only inside `#ifdef HX_NATIVE` + `RB3FixedClockActive()`;
  `git show --numstat` confirms no existing-body edit outside the guard.
- A normal boot (`MILO_HEADLESS=1`, no `RB3_FIXED_CLOCK`) is output-identical to pre-change; Wii/MWCC
  build unaffected (`HX_NATIVE` undefined → statics keep default `false` + existing DTA/cheat paths).
- `MILO_ENGINE_PIN` unchanged (`6221a56`).

**Flag-ON (`RB3_FIXED_CLOCK`) — clean gate (hard, the green bar = S1):**
- `--fixed-clock --canonical-order` **green ≥15/15 fresh boots** across an **N≥30** sweep, with the
  residual sidecar **empty** (jitter frozen) OR a **per-name** eps recalibrated from that N≥30 sample
  (NO global-eps widening — the W0.3b verifier's explicit prohibition).
- `--fail-red-audit --canonical-order`: all four defect classes RED + pure-permutation GREEN.
- `rb3-tests --gtest_filter='*DrawLog*'` → 9 pass / 1 skip (unchanged).
- If a new opt-out flag was introduced: `native_compat_census.py check` exit 0 + `--selftest` pass;
  flag registered in `NativeCompatFlags.classification.json` at introduction (F2).

**Part (b) — diagnosis + staged patch (deliverable, not a green-bar gate):**
- Written mechanism-2 diagnosis in `STATUS.md` tracing loader-completion-order → object-list
  insertion → draw-order, with file:line and an N≥15 trace-sweep evidence.
- A minimal proposed fix, staged as `W0.3d-fix` for coordinator sequencing if it touches any Lane-A
  file; landed default-OFF only if fully contained in a coordinator-confirmed non-Lane-A file.

## Files touched

- `src/system/char/CharEyes.cpp` (part a; native-guarded RNG-freeze) — **rb3**
- `native/tests/goldens/drawlog/splash_screen.json` (re-captured, frozen recipe) — **rb3**
- `native/tests/goldens/drawlog/splash_screen.fixedclock-residual.json` (emptied `draws`, or
  per-name eps in the fallback) — **rb3**
- `scripts/native/drawlog-golden.py` (**only** in the fallback: per-name-eps residual lookup) — **rb3**
- *(optional, only if opt-out flag added)*
  `milo-native-engine/src/platform/NativeCompatFlags.classification.json` +
  `NativeCompatFlags.gen.inc` (engine, own repo) + `docs/native/NATIVE_COMPAT_LEDGER.md` (rb3)
- `docs/native/engine-arch-review-2026-07-05/execution/W0.3d/{PLAN.md,STATUS.md}` — **rb3 docs**
- Part (b): `docs/.../W0.3d/W0.3d-fix.patch` (staged, coordinator-sequenced) + `STATUS.md`
  diagnosis. Any actual code fix for (b) is coordinator-gated — NOT listed here as a landed file.

## Risks / conflicts

- **F1 (binding) — part (b) is diagnosis-only wrt Lane-A files.** Lane A runs W2.1→W2.3 sequentially
  on engine `Rnd_Wgpu_RB3.cpp` (`DrawMesh` :2121-4193, incl. the object-list/draw-submission path).
  Part (b)'s fix must NOT land in that file or that path this wave — STOP and hand to coordinator as
  `W0.3d-fix`. Part (a) (`src/system/char/` + sidecar) is disjoint from Lane A, Lane C (W2.6 =
  `src/system/bandobj/BandCharacter.cpp`), and every other lane.
- **Golden re-capture cross-lane note.** S1 re-captures `splash_screen.json` under the frozen recipe.
  W2.1 demotes the splash canonical gate to a regression net (`WAVE4_REVIEW.md` B1). Both changes
  only manifest under `RB3_FIXED_CLOCK` and their own flags, and the golden is regenerated to match
  the frozen deterministic state — so once both land the golden is self-consistent. **The coordinator
  must cross-diff:** W2.1's flag-OFF regression-net baseline should be taken against the **frozen-
  jitter** golden (post-S1), not the pre-W0.3d one. Flagged for the pin-bump cross-diff.
- **No global-eps widening** (W0.3b verifier warning, F1). Fallback eps is strictly **per-name** from
  an N≥30 sample.
- **Sample-size discipline.** W0.3b (3-run) and W0.3c (cherry-picked tail) both mis-called the gate
  green; the verifier's N≥24 sweep exposed ~96% FAIL. S1's exit REQUIRES an N≥30 sweep, not 3.
- **Hard rules:** `<KEY>=W0.3d` commit prefix; stage only S1's own files (never `-A`/`-a`); flock
  `/tmp/rb3-git.lock` (+ `/tmp/milo-engine-git.lock` if the optional engine flag lands); STATUS under
  `/tmp/rb3-docs.lock`; NO `src/App.cpp` edits; leave the sibling `FxSendNative.cpp` edit untouched;
  own build dir `native/build-agent-W0.3d`; never `git reset/rebase/checkout--/restore` shared trees;
  MOVE-xor-CHANGE commits; do NOT bump `MILO_ENGINE_PIN`.
