# Wave 2 Retrospective (hindsight review)

Run `wf_ffcf5a20-bb5`, 41 agents, 2026-07-06. Engine `9561a19` → `41b9e3a` (pin bumped by
coordinator after clean rb3-native build + lineup-gate PASS against the fully-decomposed engine).

## What Wave 2 was

The Phase-1 **decomposition** wave. Six extractions carved `Rnd_Wgpu_RB3.cpp` from **7,017 →
4,747 lines** as behavior-preserving MOVE commits, plus the test-drift lane and the deferred
determinism/DrawContext pair.

- **W1.2 mesh cache** — `RB3MeshCache.{h,cpp}` extracted, MOVE byte-identical; S4 correctly refused
  to assert equivalence with `gfx/MeshGpuCache` and kept it verbatim.
- **W1.3 material binder** — `RB3MaterialBinder.{h,cpp}`, `RB3BuildMaterialUniforms` extracted.
- **W1.4 postproc/halo/quad** — `RB3HaloPass`/`RB3PostProc`/`RB3Quad` TUs, gem-bloom capture/replay
  preserved.
- **W1.5 ring dedupe** — shared `gfx/UniformRingBuffer` adopted by both backends; one CHANGE
  (overflow wrap→grow) proven inert via a zero-Grow probe.
- **W1.7 GameRenderHook** — all 13 asset-name behavior branches + Bucket-A probes relocated to
  `rb3_render_hook.cpp`, one MOVE per commit (SYS-2 structurally addressed).
- **W2-TESTFIX** — `milo-engine-tests` 169/29-failed → **198/0-failed** across 4 drift buckets; the
  suite became a real wave gate.
- **W0.3b clock seam** — ⚠️ partial (see below).
- **W1.6 DrawContext** — ⏸️ deferred by the workflow barrier (precondition W0.3b-green unmet).

The six decompositions all landed clean and were never refuted by later waves — that half of the
wave was solid, disciplined, and is the load-bearing foundation the later fixes (W1.6/W2.2/W2.1) sat
on. The interesting retrospective lessons are all in the one partial item, **W0.3b**, and in the
handoff it wrote for Wave 3.

## What later waves refuted

### 1. "3/3 boots green" was luck, not determinism (caught in-wave — the safety net working)

W0.3b.S2 and S3 each self-verified the fixed-clock draw-log gate on **exactly 3 runs** and reported
"3/3 green," then S3 built a **residual-sidecar** mechanism (`compare_fixed_clock`, an itemized
26-draw / eps=3.0 CharEyes exception list) on the premise that the residual was a *bounded,
cosmetic, ~26-draw CharEyes look-at jitter*.

The W0.3b **verifier** re-derived instead of trusting: it ran the exact already-built binary **15
times** and got `PASS×5 FAIL×5 PASS×5` — a **~33% flake**, with FAIL runs showing **336–354
unexpected mesh-identity swaps** (skinned/pipe/idx/tris/verts all differing), an order of magnitude
beyond what the sidecar can bound. It then built **two byte-identical binaries** in isolated engine
worktrees and proved they *still disagree with their own reruns* — i.e. **run-to-run process
nondeterminism**, not source drift, not a stale rebuild.

So S3's "the residual is small and bounded" was a **false premise**, and the sidecar was engineered
to close a gate that could not be closed at that scope. The premise was caught *within the wave* by
the verifier's larger sample — the process safety net did its job. But the campaign paid for the
3-run self-check: an entire residual-sidecar layer built against the wrong bug class.

### 2. The draw-order flake was mis-labeled "SYS-3 evidence"

Wave 2's README "Key finding" claimed the draw-order nondeterminism was *"evidence of the SYS-3
order-dependent-state fault the review predicted."* Wave 3 disentangled this:

- **W1.6** (the *actual* SYS-3 fix — the mutable mid-frame `mSceneBindGroup`/`mSceneOffset` global)
  landed cleanly and its verifier proved it **mechanically cannot alter** the flaky eye-jitter
  meshes (A/A controls). So the draw-order flake was **not** SYS-3.
- **W0.3c** root-caused the real flake to **async-loader/worker completion-order feeding object-list
  insertion order** — a distinct bug from SYS-3's mutable scene state.

Conflating the two under one "SYS-3" banner made the handoff read as if fixing W1.6 would help the
determinism gate. It did not; the gate cascade (W0.3b → W0.3c → W0.3d → W0.3d-fix) ran through Waves
3, 4, and 5 as its own thread.

### 3. Wave 3's plan chased a DC3-only file (born from the Wave-2 handoff framing)

The Wave-3 W0.3c PLAN designated `TransparentQueue.cpp`'s non-stable `std::sort` as the **PRIMARY**
draw-order lead (Exit A = stabilize the sort). W0.3c's S1 discovered `TransparentQueue.cpp` is in the
**DC3** GPU-source list and is **never compiled into the rb3 flavor** — `BandRnd::DrawMesh` submits
in traversal order, there is no transparent deferral to stabilize. **Exit A had nothing to act on.**
The whole primary-lead lane was built on an untested assumption about which TUs compile into
rb3-native. Note the pre-dispatch review gate (new in Wave 3) caught a *sibling* instance of the same
class — "skin/effector goldens run in the DC3-context suite, not the rb3 band-rebind path" — but did
**not** catch the TransparentQueue one. Same failure mode, one caught, one missed.

## Recurring bug families this wave touched

- **BOOTRNG** — W0.3b root-caused the **draw-count jitter** (885–890 across boots) to the
  **wall-clock boot RNG seed** and pinned it to `0x5EED` under `RB3_FIXED_CLOCK` (System.cpp).
  **State after wave: count-jitter fixed (888× exact); order-jitter is a *separate* mechanism, still
  open** → handed to W0.3c and only settled at W0.3d-fix (Wave 5).
- **Draw-order determinism (new family, opened this wave)** — surfaced by the deterministic harness;
  correctly *not* papered into the residual sidecar (it's mesh-identity swaps, a different class).
  Open at end of wave.
- Hands / wash / UI-text / layout — **not touched** in Wave 2 (those items are Wave 3+).

## Tooling gaps (name the missing capability)

1. **A "flavor-membership" oracle: is source file / symbol X compiled into the rb3-native flavor (vs
   DC3-only)?** One query answers it. The campaign instead spent a full W0.3c **S1 diagnosis stage**
   (grep the CMake source lists, confirm no `TransparentQueue.cpp.o` in the tree, build a probe-only
   binary and check the string is absent) to discover the Wave-3 primary lead was structurally
   absent — *after* a lane plan and fleet were already scoped around it. The same gap produced the
   review-gate's separate catch ("goldens in the DC3 suite"). A `which-flavor <file|symbol>` tool
   would have killed both in one run each, before planning.

2. **A standard N-run determinism-sweep gate (N≥15), reported as a PASS/FAIL distribution.** The
   W0.3b verifier *built this by hand* (15 loop runs of `drawlog-golden.py --fixed-clock`) and only
   because it chose to re-derive rather than trust. The process default was a **3-run self-check**,
   which has a real chance of landing all-green at a 33% flake rate — which is exactly what happened
   to S3. Had a mandated ≥15-run sweep existed as the gate primitive, S3 would have seen the flake
   *before* building the residual sidecar against the wrong bug class. Cost of the gap: one
   subtask's worth of sidecar engineering + a verify stage spent re-discovering the flake.

3. **Process-vs-build nondeterminism attribution.** To prove the flake was *process*
   nondeterminism (not concurrent-W1.2 source drift, not a stale rebuild), the verifier manually
   spun up **two isolated engine `git worktree`s at the exact linked commit**, did from-scratch
   builds, `cmp`-proved byte-identical binaries, and showed they *still* disagree with their own
   reruns. A tool that, given a gate command, reports "same binary, N reruns, K distinct outcomes →
   process-nondeterministic" would have collapsed that multi-worktree bisection into one invocation.

## Wasted-effort estimate: **small–moderate.**

The six decompositions were pure value (no rework). The waste is concentrated in W0.3b:
- The **residual-sidecar layer** (S3) was built against a "bounded 26-draw CharEyes jitter" premise
  that the verifier's larger sample immediately falsified — engineering aimed at a gate that could
  not close at that scope (small: the sidecar still catches the CharEyes class and was reused).
- The **3-run self-check** let a 33% flake read as green, forcing a verify stage to re-derive the
  truth and forcing the whole determinism gate into a 3-wave cascade (W0.3c/d/d-fix).
- The **"SYS-3 evidence" framing** cost Wave 3 a disentanglement it should not have needed, and
  seeded a Wave-3 primary lead (`TransparentQueue`) that a flavor-membership check would have vetoed
  before the plan was written.

None of this bled past the wave boundary as *lost work* — the safety nets (verifier re-derivation,
workflow barrier auto-deferring W1.6, honest "partial" status) contained it. That containment is the
wave's real success story; the flake and the misframing are the lessons.
