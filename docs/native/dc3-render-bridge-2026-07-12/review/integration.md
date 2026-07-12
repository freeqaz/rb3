# Review — Integration / Asset-Risk Skeptic (2026-07-12)

_Lens: integration. Posture: refutation — try to break the plan on two-repo/two-pin
drift, CMake feasibility, Emscripten viability, Wii-asset encodings through an
Xbox-era pipeline, and hidden DAG ordering. Every claim below is checked against
real code (file:line), not against the recon prose. **No builds were run.**_

## Verdict: **GO**

The plan's recommendation — *no bridge; adopt Option C (SceneView shape-neutral
convergence), spike-gated* — survives the attack. Its load-bearing integration
claims **verify against the actual engine + consumer CMake + headers**. My
refutation attempts surfaced only **advisory-level** issues (one factual error in
a "ports as-is" claim, an under-specified/self-contradictory SceneView contract
for the first spike TU, and an internal C1-vs-C2 ordering inconsistency). None
makes the plan unsafe to charter; all three are exactly what the SPIKE-TQ gate and
its kill criteria are designed to catch, and the gate is cheap (one wave, bounded
sunk cost). **No blocking findings.**

---

## What I verified holds (the plan's spine is real)

1. **One engine, two flavors, source-level library, one rndobj per binary.**
   `milo-native-engine/CMakeLists.txt:255-329` defines exactly the flavor lists the
   plan describes: Tier-1 rndobj-free `MILO_ENGINE_GFX_SOURCES` (:261-273, both
   flavors), Tier-2 `MILO_ENGINE_GFX_RNDOBJ_SOURCES` (:281-288, dc3 only),
   `MILO_ENGINE_GPU_PLATFORM_SOURCES` (:304-313, dc3), and
   `MILO_ENGINE_GPU_PLATFORM_SOURCES_RB3` (:317-329, rb3). SPEC §7's "the two
   engines' symbols never coexist" is correct by construction — the engine is
   `add_subdirectory`'d and compiled against one consumer's rndobj at a time.
   **Option A's symbol-collision blocker is genuine**, and the CMake move Option C
   needs (hoist a TU into a new shared "both-flavors" list) is trivial list
   manipulation, no structural obstacle.

2. **Pin reconciliation (C0) is a linear fast-forward, not a merge.** Verified by
   `git merge-base --is-ancestor`: DC3 pin `77eb428b` ⊂ RB3 pin `b36bcfcd` ⊂ HEAD
   `0083bad`. Of the **129 commits** between the DC3 pin and HEAD, **exactly 1**
   touches the shared gfx CORE (`GpuDevice/BloomPass/PipelineManager/
   UniformRingBuffer/Screenshot`), and the handful touching dc3-flavor
   `src/gfx`/`src/platform` are self-labeled *"MOVE, byte-identical for DC3"*
   (shader externalization, UniformRingBuffer extraction) or *"default-OFF"* flags
   (`git log 77eb428b..HEAD`). So bumping DC3 forward is low-risk and DC3
   bit-preservation is correctly gated by C0's own acceptance ("both consumers
   build green at one pin"). **C0 is well-scoped; the three-SHA skew is benign.**

3. **The convergence seam headers are already rndobj-opaque.**
   `src/gfx/ShadowPass.h:1-9` and `src/platform/TransparentQueue.h:1-5` forward-declare
   `RndMesh/RndCam/RndEnviron` only — coupling lives in the `.cpp`
   (`TransparentQueue.cpp:6-9` includes `rndobj/{Cam,Env,Mesh,BaseMaterial}.h`;
   the draw seam is `extern void DrawMeshImmediate(RndMesh*)` at
   `TransparentQueue.cpp:17`). PLAN §Option-C's central feasibility claim holds.

4. **The vertex seam is genuinely already converged.** Both flavors emit the same
   64-byte `GpuVertex` (`src/gfx/VertexFormats.h:9-17`, `static_assert(sizeof==64)`),
   and the rb3 flavor literally `using GpuVertexRB3 = GpuVertex`
   (`Rnd_Wgpu_RB3.h:43`). SPEC §5 "formalize, don't rebuild" is accurate.

5. **`SceneView_RB3.cpp` is compile-viable — the LP64 "22/64 clang-clean" worry is
   correctly scoped out.** `Rnd_Wgpu_RB3.cpp:6-15` already `#include`s
   `rndobj/{Cam,Mesh,Mat,Env,Lit}.h` and reads their fields in both the native and
   web builds today. The R4 "22/64 rndobj TUs not clang-clean" caveat is about
   compiling the rndobj `.cpp` TUs (Mesh.cpp/Tex.cpp) themselves — **not** about
   *including their headers*, which BandRnd already does successfully. So a per-flavor
   `SceneView_RB3.cpp` reading the same fields is proven-feasible. PLAN's risk-table
   mitigation ("accessors read only fields BandRnd already reads") is right.

6. **Web/Emscripten is neutral for converged passes.** The web exclusion dance
   (`rb3/native/CMakeLists.txt:206-223`) targets only platform-I/O TUs
   (`File_Web/CDReader_Web/GpuDevice_Web/AudioDevice_Web/ImGuiBackend_Web/WebAssets`)
   — **not** the gfx pass TUs. Converged passes are ordinary engine gfx sources and
   inherit the seam. The plan's "web neutral" claim holds. (One honest caveat, below.)

7. **No third-repo linkage is proposed.** The plan turns on the *engine's existing*
   dc3-flavor TUs behind a seam; it does **not** pull `dc3-decomp/native/src` into
   rb3-native's link line. R4 finding 2 and the CMake confirm. The charter's
   "linking dc3-decomp sources into rb3-native" hazard therefore does not apply to
   Option C (it would apply to a naive reading of the ask, which the plan rejects).

8. **Wii-asset-through-Xbox-decoder risk is correctly firewalled.** SPEC §5 rule 3 /
   §6 keep all platform decode consumer-side; shared code sees only `GpuVertex` +
   linear RGBA. The one residual unknown (is any Wii-encoded `.milo` actually loaded
   natively?) is honestly carried as SPEC §9 OQ2. Not blocking.

---

## Advisory findings (should improve before/inside the spike — none blocking)

### A1 — SPEC's "`IsTransparentBlend(int)` ports as-is" is factually wrong
`TransparentQueue.cpp:58-63` implements `IsTransparentBlend` by comparing against
**`BaseMaterial::kBlendSrcAlpha` / `kBlendSrcAlphaAdd` / `kBlendAdd` /
`kBlendSubtract` / `kPreMultAlpha`** — enum constants *qualified on `BaseMaterial`*.
RB3's rndobj has **no `BaseMaterial` class** (R3 finding 3: RB3 `RndMat : Hmx::Object`;
DC3 `RndMat : BaseMaterial`). The values are numerically identical, but the *source*
`BaseMaterial::kBlend*` will not compile against RB3's headers. PLAN §Option-C cites
this function as evidence the seam is "cheaper than recon assumed"
(PLAN.md line ~160: *"`IsTransparentBlend(int)` ports as-is"*) — that specific
evidence is false; the enum qualifier must be abstracted (a fork-neutral constant, a
`SceneView`-provided blend enum, or plain ints). Small edit, but it sits on the
spike's critical path and slightly undercuts the "cheaper than assumed" framing.

### A2 — SceneView §3 surface is incomplete for its own first spike TU, and its "read-only" invariant collides with what TransparentQueue actually does
TransparentQueue's flush issues **mutating** virtual calls: `td.cam->Select()`,
`td.env->Select(nullptr)`, `savedCam->Select()`, `savedEnv->Select(nullptr)`
(`TransparentQueue.cpp:137-145, 205-217`). SPEC §3 declares SceneView *"Read-only …
never mutates scene objects"* and lists **no** camera/env (re)selection primitive in
its initial surface. This forces a design choice the SPEC leaves unresolved:
- **(a)** If the pass `.cpp` must include *no* rndobj header (SPEC §1's stated goal —
  *"stop including rndobj/*.h directly"*), then `Select()` needs a **mutating**
  SceneView seam → contradicts the §3 read-only rule.
- **(b)** If the `.cpp` keeps `#include "rndobj/Cam.h"`/`Env.h` and calls `Select()`
  directly (viable — both forks expose `virtual void Select()`: `rb3 Cam.h:31`,
  `dc3 Cam.h:27`, resolved per-flavor at compile), then **PLAN C1's acceptance
  "`SceneView.h` includes no rndobj header (grep-enforced) … pass `.cpp` includes no
  rndobj header" is unachievable for TransparentQueue.**

The clean resolution is (b): enforce rndobj-free only on `SceneView.h`, and let pass
`.cpp`s keep including rndobj headers for the fork-common method surface (`Name()`,
`Select()`), routing through SceneView **only the divergent** accesses
(`RndMesh::Vert`, `BaseMaterial` base, `GetViewProjectXfms`). The architect should
(i) relax PLAN C1's grep acceptance to the header only, and (ii) either add a
mutating `SceneView::SelectCam/SelectEnv` pair or explicitly bless direct `Select()`
calls in the pass `.cpp`. As written, the §3 surface + read-only rule under-specify
the very TU the spike ports, and the "≤250 LOC SceneView" estimate is optimistic.

### A3 — internal inconsistency: does the spike (C2) create SceneView, or does C1?
SPEC §3's de-risking spike says the spike itself *"add[s] `SceneView.h` +
`SceneView_RB3.cpp` … and port[s] `TransparentQueue.cpp`"* — i.e. **C2 creates the
initial SceneView**. But PLAN §4's DAG makes **C1** a separate **M-sized** node that
builds the *full* SceneView surface (all mesh/cam/env/light accessors, SPEC §3) and
puts **C2 downstream of C1**. If C1 is dispatched at full surface *before* the C2
gate proves the approach, it violates SPEC §3's own *"grow-on-demand … no speculative
surface"* rule and front-loads throwaway work should the spike fail. Fix: scope C1 to
the **minimal TransparentQueue slice** (or fold C1 into C2), and grow the accessor
surface in C3+ as each pass lands. Both nodes are cheap and pre-gate, so this is not
safety-critical — but it is a real contradiction between SPEC §3 and PLAN §4/C1 that
will confuse whoever dispatches the wave.

### A4 — C4→C5 is over-serialized (safe, but throughput-limiting)
The DAG chains `C4 (RB3Quad/RB3PostProc) → C5 (material-bind)`. C5 converges
`RB3MaterialBinder` vs `MaterialSetup`; `MaterialSetup.cpp` lives in the dc3
**platform** list (`CMakeLists.txt:304-313`), which C3's gfx seam-port does not
touch, so C5 carries its own independent seam work and does not depend on C4's
Quad/PostProc merge. Extra serialization is *safe* (the charter asked about **missing**
edges, not extra ones), so advisory only: C5 may parallelize with C3/C4 once C1+C2
land, if wave throughput matters.

---

## Hidden-ordering audit (charter's specific ask: edges the plan MISSED)

I found **no missing dependency edge** that would make an increment unsafe:
- C3 (seam-port `ShadowPass/DofPass/PostProcPass/DrawRect2D`) correctly precedes
  C4 (merge `RB3Quad/RB3PostProc` **into** the now-seamed `DrawRect2D/PostProcPass`)
  and C6 (enable a converged pass) — both need the seamed TU first. Real edges.
- C0 before C1 is a *soft* edge (landing C1 means bumping both pins; easier if C0
  unified them) — the plan draws them as parallel feeders to C2, which is fine.
- TransparentQueue's `extern DrawMeshImmediate` (rb3) is satisfied at link by
  BandRnd, not by dragging in `Mesh_Wgpu.cpp` — so no hidden C2→Mesh_Wgpu edge. OK.

The only ordering defects are the *internal* C1-vs-C2 inconsistency (A3) and the
*spurious* C4→C5 edge (A4) — neither is a missing-edge safety hole.

---

## One honest caveat carried forward (not a finding, a test-gap the plan already gates)
RB3's web build has **never** compiled the Tier-2 gfx pass TUs (RB3 is the `rb3`
flavor, which drops `MILO_ENGINE_GFX_RNDOBJ_SOURCES`). So the first time a converged
`ShadowPass`/`TransparentQueue` compiles **against RB3 rndobj under Emscripten** is
genuinely new territory — "web neutral" is a structural claim, not a demonstrated
build. The plan already gates this: C2's risk-register row requires "one web smoke,"
and the 28 MB brotli budget check is homed in C7. Adequate; just flag it so the C2
agent actually runs the web smoke and does not treat native-green as web-green.

## Confidence
- **HIGH** on items 1-7 (CMake, pin ancestry, header opacity, GpuVertex identity,
  SceneView_RB3 include-viability, web exclusion scope) — all read directly from
  engine/consumer source + git ancestry, cited above.
- **HIGH** on A1/A2 (direct reads of `TransparentQueue.cpp` and both forks' `Cam.h`).
- **MEDIUM** on the *magnitude* of A2's LOC impact (no build run; the mutating-seam
  vs direct-call resolution changes the estimate but is bounded by the spike).
