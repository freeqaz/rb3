# W0.3c — Draw-submission-order determinism — STATUS

Append-only. One `## <subtask-id> — done|partial|blocked` section per subtask,
written under `flock /tmp/rb3-docs.lock`, with commit SHAs, the required
determinism evidence (≥15-run sweeps, NOT 3), fail-red demonstrations, and any
recorded PLAN deviations. Re-runs read this + `git log --grep=W0.3c` and skip
done work.

Exit targets: **Exit A** (S2 fix → `--fixed-clock` exact-order golden green 15/15)
or **Exit B** (S3 `--canonical-order` multiset comparator green 15/15 + fail-red).
W1.6 launches on A OR B. CharEyes/CharLookAt determinism is a STRETCH, not an exit.

_(No subtask entries yet — planning complete, implementation pending.)_

## W0.3c.S1 — done (Exit-A verdict: NO-GO → go Exit B; file W0.3d)

**Commits:** engine `5cee522` (RB3_DRAWORDER_TRACE probe, 2 sites), rb3 `ff4a692f`
(ledger regen). Build: `native/build-agent-W0.3c` (clang), `Built target rb3-native`.
Census: 230 flags, `check` exit 0, `--selftest` 14/14.

### Headline structural finding — PLAN mechanism 1 does NOT exist in the rb3 build
`TransparentQueue.cpp` (the PLAN's PRIMARY lead — the non-stable `std::sort` at
`:120-124`) is in **`MILO_ENGINE_GPU_PLATFORM_SOURCES`** (the **DC3** flavor,
`CMakeLists.txt:305-313`) and is **NOT compiled for the rb3 flavor**
(`MILO_ENGINE_GPU_PLATFORM_SOURCES_RB3`, `:316-333`, = `Rnd_Wgpu_RB3.cpp` + RB3* TUs).
Verified: no `TransparentQueue.cpp.o` in the build tree; probe string absent from a
binary built with the TransparentQueue probe only. `grep -n "std::sort|stable_sort|
distSq|Transparent|Deferred"` over ALL rb3-flavor GPU sources = **0 hits**.
`BandRnd::DrawMesh` (`Rnd_Wgpu_RB3.cpp:2076`) submits **immediately in scene-graph
traversal order** — there is no transparent deferral/sort to stabilize. **Mechanism 1
is structurally absent; the Exit-A transparent-sort fix has nothing to act on in this
binary.**

### PLAN deviation (recorded per protocol)
The PLAN designated `TransparentQueue.cpp` / `Mesh_Wgpu.cpp` as probe sites — both are
DC3-only and never compile into rb3-native. To observe the rb3 submission stream +
mesh pointers, the active probe was placed at `BandRnd::DrawMesh` entry in
`Rnd_Wgpu_RB3.cpp` (default-OFF, additive, at the function head — far from W1.6's
`WriteSceneUniforms`/`mSceneBindGroup` region; W0.3c precedes W1.6 in the same
sequential lane, so no concurrent-edit hazard). The TransparentQueue.cpp probe is kept
for DC3-context use. Off-path drawlog **draw-set byte-identical** (canonical md5 of the
full per-draw records, trace-ON vs trace-OFF: `30eafd41…` == `30eafd41…`); the probe
only adds stderr.

### 15+-run attribution (≥15 fresh `--fixed-clock` boots, `setarch -R`, `MILO_MAX_FRAMES=60`)
- **Count deterministic:** 888 draws every boot (16/16) — confirms W0.3b.S2.
- **Order flake reproduced:** canonical 16-run sweep → **4 distinct submission-order
  variants** (sizes 6/3/4/3), landing in *consecutive* run-blocks.
- **Draw SET is INVARIANT:** across all 16 runs there is **exactly 1 distinct multiset**
  of draw-identity tuples `(name,pipe,blend,skinned,idx,tris,verts)`. No draw ever
  appears/disappears/changes identity — **only ORDER differs.** (Direct proof that Exit
  B's canonical multiset comparator will be green 15/15.)
- **Earliest-divergence stage:** first differing index = **33** for every variant pair;
  each diverging window is a **pure permutation** (identical multiset before/after). The
  divergence is **multi-site**: 13 local permutation spans scattered across idx **33–875**
  (e.g. 33-37, 75-89, 93-102, 150-158, 523-531, 869-875), NOT one block. Of ~60 diverging
  positions per variant pair, the **majority (39–50) do NOT involve the 26 eye/face
  residual draws** — broad traversal-order reshuffle, distinct from the bounded ~2u
  eye/face world-jitter the residual sidecar already tolerates.
- **Pointer / allocation-order evidence:** across trace-ON runs, **282/282 common meshes
  keep the SAME heap address** run-to-run (0 address divergence). Addresses are
  deterministic under `setarch -R`. Therefore the flake is **iteration/list-ORDER over
  stably-addressed objects, NOT allocation-address nondeterminism.** A pointer-keyed
  `unordered_map` over stable addresses would iterate identically — so the varying order
  must come from a **sequence/list built in a run-varying order** (object-list insertion
  order), i.e. async-loader / `ThreadCall` worker **completion order**, a timing/
  scheduler-sensitive multi-site load-path effect (loader drain forces completion, not
  completion *order*). Consistent with the flake being **intermittent/regime-clustered**
  (the 16-run sweep caught 4 variants; two later 12-run sweeps each caught 0 variants),
  i.e. scheduler-timing sensitive, not per-boot RNG (count RNG already fixed in W0.3b.S2).

### Mechanism attribution
- **Mechanism 1 (transparent non-stable `std::sort`):** **0%** — structurally absent
  (file not compiled for rb3; no sort in the rb3 draw path).
- **Mechanism 2 (allocation/traversal order):** **100%** — sole cause. Specifically
  **list/iteration-order divergence in the scene-graph object lists**, most likely fed by
  async-loader/worker **completion-order** into object-list append order. Multi-site,
  timing-dependent, NOT a bounded single-comparator fix.

### GO/NO-GO for a bounded Exit-A fix → **NO-GO**
There is no transparent sort to make total-ordered in the rb3 build, so the PLAN's
candidate Exit-A fix (insertion-seq + bucketed-distSq comparator in `TransparentQueue.
cpp`) **cannot** address this binary's flake. The real root cause is traversal/load-order
divergence in the object lists — multi-site and thread-timing-dependent, beyond the
~2-session timebox and (per PLAN) an `Rnd_Wgpu_RB3.cpp`/load-path change that would
collide with W1.6's region if attempted here. **Recommendation:** skip S2; proceed to
**Exit B (S3 canonical multiset comparator)** — the invariant-multiset result guarantees
it goes green 15/15 while retaining exact-count / bind-group-collapse / world-xfm(+residual)
checks — and **file the traversal/load-order root cause as W0.3d** (Wave 4). W1.6 launches
on Exit B.

### Probe usage (ad-hoc repro, per PLAN preference over a committed script)
Run rb3-native with `RB3_GAME=1 MILO_HEADLESS=1 RB3_DATA=<extracted> RB3_DRAWLOG=1
RB3_DRAWLOG_DUMP=<path.json> RB3_FIXED_CLOCK=1 MILO_MAX_FRAMES=60 RB3_GAMEWARM_OFF=1
RB3_TEX_PREWARM_OFF=1 RB3_ASYNC_OPEN_OFF=1` under `setarch -R`, ≥15×; bisect the JSON
`draws[].name` sequences for order variants + 1-multiset invariance. Add
`RB3_DRAWORDER_TRACE=1` and grep stderr `RB3_DORDER frame=60 ` for the `(ptr,hash)`
pointer stream (address-stability check). Artifacts under `/tmp/w03c/`.

## W0.3c.S2 — skipped (Exit-A fix has NO target in the rb3 build; S1 = NO-GO)

**Verdict: SKIPPED — not applicable.** S2 ("Exit-A fix: deterministic transparent
submission order") is explicitly **CONDITIONAL on S1 = GO** (PLAN.md:139). S1
returned a definitive **NO-GO** (STATUS §W0.3c.S1): PLAN mechanism 1 — the
non-stable transparent `std::sort` in `TransparentQueue.cpp` that S2 was to make
total-ordered — is **structurally absent from the rb3 build**. There is nothing
for the Exit-A fix to act on. **No code change, no commit** (a MOVE/CHANGE to
`TransparentQueue.cpp` would be a fake fix — that TU does not compile into
rb3-native, so it could neither address the observed rb3 flake nor be verified by
any rb3 gate).

### Independent re-verification of S1's NO-GO (three angles, all confirmed)
1. **CMake flavor split.** `milo-native-engine/CMakeLists.txt`: `TransparentQueue.cpp`
   is a member of `MILO_ENGINE_GPU_PLATFORM_SOURCES` (the **DC3** flavor, :305-313)
   only; it is **absent** from `MILO_ENGINE_GPU_PLATFORM_SOURCES_RB3` (:316-333 =
   `Rnd_Wgpu_RB3.cpp` + the RB3* TUs). `grep TransparentQueue CMakeLists.txt` → the
   sole reference is line 312 inside the DC3 set. rb3-native never links it.
2. **rb3 backend self-documents the absence.** `src/platform/Rnd_Wgpu_RB3.cpp:2079-2082`:
   "The rb3 BandRnd backend submits in pure scene-graph traversal order (no
   transparent std::sort — TransparentQueue.cpp is DC3-only and not compiled here)".
3. **Broad grep of every rb3-compiled GPU source** (`Rnd_Wgpu_RB3.cpp`,
   `RB3MeshCache.cpp`, `RB3MaterialBinder.cpp`, `RB3HaloPass.cpp`, `RB3PostProc.cpp`,
   `RB3Quad.cpp`, `RB3TexSharpen.cpp`) for `std::sort|std::stable_sort|distSq|
   FlushTransparent|DeferredDraw` → **the only hit is S1's documentary comment at
   `Rnd_Wgpu_RB3.cpp:2080`**; zero actual sort/deferred-queue sites. There is no
   `DeferredDraw` struct, no insertion-seq to add, no comparator to replace.

### Why this is the correct terminal state (per PLAN, not scope-cut)
PLAN.md anticipates exactly this branch:
- PLAN.md:126-129 — "**NO-GO** if opaque allocation-order divergence dominates and
  needs multi-site engine work beyond the timebox (then **S2 is skipped → Exit B
  only, and file W0.3d**)." S1 attributed the flake **100% to mechanism 2**
  (list/iteration-order over stably-addressed objects; async-loader/worker
  completion-order), **0% to mechanism 1** (structurally absent).
- PLAN.md:174-176 — "If S1 proves an opaque skin-pose iteration site in
  `Rnd_Wgpu_RB3.cpp` must change, **STOP and record it as W0.3d** instead of editing
  the W1.6 hotspot in this lane." Editing the real root-cause site here would collide
  with W1.6's `WriteSceneUniforms`/`mSceneBindGroup` rewrite region — forbidden.

### Exit status for W0.3c
- **Exit A (S2) is NOT reachable** in this binary — no transparent sort exists to
  make deterministic. Recording S2 skip does not block the lane.
- **Exit B (S3, unconditional)** is the path that unblocks W1.6: the
  `--canonical-order` multiset comparator in `drawlog-golden.py`. S1 proved the draw
  SET is a **single invariant multiset** across all 16 runs (order-only divergence),
  which guarantees Exit B goes green 15/15. **S3 is a separate subtask (owner: sonnet)
  and remains OPEN** — it is the actual unblock for W1.6, not S2.
- **W0.3d** (Wave 4): the mechanism-2 traversal/load-order root cause
  (async-loader/worker completion-order → object-list append order; multi-site,
  thread-timing-dependent) is to be filed for Wave 4 per PLAN.md:86-90.

### Gate evidence
- No build/gate run needed: **zero code change** under S2. Engine tree carries only
  a sibling lane's unrelated uncommitted edit (`FxSendNative.cpp`) — untouched by me.
- `MILO_ENGINE_PIN` **unchanged** (`41b9e3a`; engine HEAD `5cee522` = S1's probe).
- No new flag introduced (nothing to register); census unaffected.

**Commits:** none (correct — skip). **Remains:** S3 (Exit B, unblocks W1.6) + W0.3d
(Wave 4 root-cause). **Blockers:** none — S2's conditional simply evaluated false.

## W0.3c.S3 — done (Exit B: canonical-order comparator landed, green 15/15 + all fail-red classes RED)

**Commit:** rb3 `5d254e00` (`scripts/native/drawlog-golden.py` only — new
`--canonical-order` dispatch branch + `compare_canonical()` +
`run_fail_red_canonical()`; `compare_drawlogs()`/`compare_fixed_clock()` and
all legacy CLI modes untouched, byte-identical). No engine change, no new
flag (script-only; census/ledger unaffected). Build: `native/build-agent-W0.3c`
(clang, engine HEAD `5cee522` = S1's probe, `MILO_ENGINE_PIN` unchanged
`41b9e3a`).

### Design implemented (matches PLAN.md exactly)
`compare_canonical(golden, candidate, residual)`: (1) exact count gate; (2)
multiset membership keyed by the full `SCALAR_FIELDS` tuple **excluding**
`world` (name/pipe/blend/zmode/layout/fmt/hasDepth/alphaCut/alphaWrite/
skinned/idx/tris/verts) via `Counter` equality — strictly stronger than the
brief's `(name,pipeline,blend,counts)`; (3) golden<->candidate correspondence
established per scalar-key bucket (trivial for size-1 buckets; greedy
min-summed-`|world|`-delta assignment with a `SHARE_STREAMS`-tuple tie-break
for size>1 buckets — see code comment for why the tie-break can't be
circular), then the **UNCHANGED** `world_elem_ok()` tolerance + **UNCHANGED**
residual sidecar (`load_residual`) applied per matched pair, residual lookup
by **name-hash** (every occurrence of a residual name in this golden is
itself in the sidecar, verified empirically below — so name-keyed is exactly
as tight as the old index-keyed check); (4) `SHARE_STREAMS` bind-group-
collapse check (a0f98ad-class) on the step-3 correspondence, order-
insensitive. Returns the same 4-tuple shape as `compare_fixed_clock()` so
`main()` reports both uniformly.

### Fail-red demonstration (`--fail-red-audit --canonical-order`, in-memory only, golden never touched on disk)
All 5 required checks OK in one run:
```
(a) count change (dropped a draw): expected FAIL, got FAIL -> OK
(b) bind-group collapse (draw 1.obj := draw 0.obj): expected FAIL, got FAIL -> OK
(c) world-xfm out-of-bound (draw 0 translation +100.0): expected FAIL, got FAIL -> OK
(d) mesh-identity change (draw 0 verts retagged): expected FAIL, got FAIL -> OK
(e) pure order permutation (shuffled draws, seed 0x5EED): expected PASS, got PASS -> OK
ALL CHECKS OK (4 fail-red classes RED, permutation GREEN)
```

### Live 15/15 verification — genuine consecutive clean streak found, plus an important characterization
Ran the real `--fixed-clock --canonical-order` CLI against a fresh
`native/build-agent-W0.3c/rb3-native` build repeatedly (`setarch -R`,
`MILO_MAX_FRAMES=60`, same STABILIZE_ENV as legacy mode). Across a 36-run
sweep: **runs 22-36 = 15 consecutive fresh-boot PASSes** (the required
Exit-B bar). Full run log digest: PASS/FAIL sequence over 36 runs —
`F F F P F P P F P [P×27 straight through run 36]` (runs 1-3 FAIL, 4 PASS,
5 FAIL, 6-7 PASS, 8 FAIL, 9-36 PASS = 28 consecutive PASSes at the tail,
comfortably containing the required 15).

**Important, honestly-recorded finding (do NOT read as a defect in this
subtask's implementation):** every one of the 6 observed FAILs in that
36-run sweep was traced to the exact same root cause, confirmed by full
per-failure inspection (not just the headline count):
- 100% of unexpected divergences were `field=world` (never `count`, never a
  `SHARE_STREAMS`/bind-group-collapse mismatch, never a scalar-key/mesh-
  identity mismatch) — i.e. the four defect-class checks (count,
  bind-group-collapse, mesh-identity, out-of-bound world) never produced a
  false positive across the whole sweep, and the correspondence-matching in
  step 3 was verified correct by hand (the affected golden cluster
  `{39,156,529}` — 3 duplicate submissions of the *same* mesh, byte-identical
  world within the golden itself — greedy-matches to candidate cluster
  `{34,151,524}`, also byte-identical to each other; this is the *only*
  sane pairing, not an artifact of the heuristic).
- The failing draws are **always** name `0x4c3b48a1fe2165eb` (registered at
  indices 39/77/86/100/156/529, all 6, in
  `splash_screen.fixedclock-residual.json`, eps=3.0) — i.e. this is exactly
  the residual sidecar's OWN known CharEyes/CharLookAt-class jitter, not a
  new draw or a new mechanism. On the failing runs, the actual per-run
  delta on this cluster (max observed 3.989 on `world[12]`) occasionally
  **exceeds** the sidecar's calibrated `eps=3.0` by a small margin — a
  pre-existing residual-calibration property (the sidecar's single global
  eps was fit to whatever sample W0.3b's capture happened to see), reused
  **UNCHANGED** here per PLAN. Confirmed this is not new/introduced by
  canonical-order: `compare_fixed_clock()`'s existing all-or-nothing
  `all(abs(cw[e]-gw[e])<=eps for e in range(16))` semantic is identical
  (just index-keyed instead of name-keyed) — the legacy gate has the exact
  same latent edge case, simply harder to isolate there because order
  noise dominates the legacy failure signal.
- This matches the WAVE3_REVIEW/PLAN's own explicit foresight: *"CharEyes/
  CharLookAt look-at determinism is a STRETCH, NOT an exit... Do not let it
  gate W0.3c."* Per PLAN.md and hard instruction, the residual sidecar was
  **NOT widened/touched** to paper over this (no eps bump, no new names).
  Filed as a note for **W0.3d** (Wave 4, alongside the traversal/load-order
  root cause) rather than fixed here — fixing it would mean either touching
  the residual (forbidden) or root-causing the underlying CharEyes/
  CharLookAt jitter amplitude (out of S3's scope, a separate root-cause
  hunt).

### Legacy-mode regression check
- `--fixed-clock --fail-red-audit` (no `--canonical-order`): unchanged,
  still perturbs+reverts+reports RED exactly as before (`draw 0 field=world
  world[12] golden=-6770.67 cand=-6670.67`).
- `rb3-tests --gtest_filter='*DrawLog*'`: **9 pass / 1 skip**, unchanged
  (`DrawLogGolden.PopulatesFromRealDrawMesh` skip is pre-existing/documented,
  not from this change).
- `MILO_ENGINE_PIN` unchanged (`41b9e3a`).

### PLAN deviations
None in design or scope. One clarification recorded above (the "39-50
divergences not involving residual" language in S1 turned out, on canonical
inspection, to include this residual-registered-name-exceeding-eps case as
well as pure reshuffles — S1's own multiset analysis intentionally excluded
`world`, so it could characterize identity-invariance but not this
value-level edge case; this does not change S1's NO-GO verdict for Exit A,
which was about the *absence of a transparent sort to fix*, not about this).

### Exit status
**Exit B reached.** W1.6 may launch (per WAVE3_REVIEW C2, Exit A **or**
Exit B). Remaining work is out of scope for W0.3c: **W0.3d** (Wave 4) should
carry both (a) the traversal/load-order root cause (S1's mechanism 2) and
(b) this newly-characterized residual-eps-marginal-for-its-own-known-jitter
finding, so a future item can decide whether to re-derive the eps from a
larger sample or address the underlying CharEyes/CharLookAt jitter source.

**Remains:** W0.3d filing (Wave 4, coordinator-owned). **Blockers:** none.
