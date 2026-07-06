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
