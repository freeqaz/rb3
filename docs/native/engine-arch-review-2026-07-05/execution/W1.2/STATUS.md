# W1.2 — Extract RB3MeshEntry mesh-upload cache into its own TU — STATUS

Append-only. Update under `flock /tmp/rb3-docs.lock`. One `## W1.2.S<n> — done|partial|blocked`
section per subtask with commit SHAs, the byte-identical gate result (lineup PASS + WIDE-PNG hash
equality), and any blocker. Re-runs read this + `git log --grep=W1.2` (engine repo) and skip done
work.

Plan: `./PLAN.md`. Repo: engine (`/home/free/code/milohax/milo-native-engine`). Own build dir:
`/home/free/code/milohax/rb3/native/build-agent-W1.2`.

---

## W1.2.S0 — done

**Build:** own dir `native/build-agent-W1.2` (configured with `-DCMAKE_C_COMPILER=clang
-DCMAKE_CXX_COMPILER=clang++` — the plain `cmake -B ... -S native` default picked up GNU/g++ on
this host and failed on Clang-only flags `-ferror-limit=0`/`-fms-compatibility*`/
`-fdelayed-template-parsing`; every sibling `build-agent-*` dir already pins clang++, this just
makes that same requirement explicit for a from-scratch configure). Built `rb3-native` clean.

**Primary — lineup gate:** `python3 scripts/native/lineup-gate.py --bin
native/build-agent-W1.2/rb3-native --out /tmp/w1.2-lineup-baseline` -> **PASS** all four layers
(`img=PASS segA=PASS ratioB=PASS countC=PASS pin=PASS`, `max_band_ratio=2.91`). WIDE PNGs +
`manifest.json`/`verdict.json` copied to `evidence/baseline/lineup/`; sha256:
```
103d49ba48bee48e07272e7da7a2beba3ca0c65d911118a487f84e16261fe964  cand_coop_g_b_0.png
7573e7cce3bf1e5aed478ffcbc3bd3dc963e53bb16025b0d55edc3928d9e7485  cand_coop_g_b_1.png
b6bc789cbfafab387396d394012a028eb84663273b3b00ec8614502dae2e65ce  cand_coop_g_n03_0.png
ef41f2a009116cf073ad56e5448985d466a246c84f5852b10f476eb332fc8ee3  cand_coop_g_n03_1.png
```

**Corroborating — song-select screenshot hash:** `python3 scripts/native/song-select-capture.py
--bin native/build-agent-W1.2/rb3-native --out /tmp/w1.2-songselect-baseline` -> reached
`song_select_screen` (frame=267), captured 5 depths. PNGs + sha256 copied to
`evidence/baseline/song-select/`:
```
daeb4487a8fed031904f981d6da1fff2ff1602fb951d894b837534ea7a23b30d  native_depth_00.png
b258b389414dd99f6d2ad32dac2d0c9e458ec8613816346114938eb522fc83ce  native_depth_08.png
1e498b7a0e24034c911d591263736cd8579c1c03a27dc6346c5be34d0193c09e  native_depth_16.png
59a0cb56d81ceade19655882abadfe38d3e176217be365d5950ceb7fc601e8d9  native_depth_30.png
43f30517544d1df1bb760e3a26a98d2a1303145845f97d9efdb75225c1180ff4  native_depth_50.png
```

**Corroborating — draw-log dump (non-gating):** `RB3_DRAWLOG=1` at the lineup-gate's gameplay
scene (`game_screen`, boot -> main_hub -> song_select -> part_difficulty -> game_screen nav,
guitar/easy, forced `coop_g_n03.shot`, `songMs~20392`), fetched `/api/drawlog` (106023 bytes,
`{frame, count, draws}`) -> `evidence/baseline/drawlog.json`
(sha256 `12f722e521378d0ba094d0e9d45e734d4addb6b7d89b364b137253a0c2e209b0`). Captured via a
one-off script reusing `band-closeup-capture.py`'s proven nav module (`bc`/`bc.k`) rather than
re-deriving boot->gameplay navigation; not committed as a permanent script (PLAN.md marks this
step non-gating/corroborating only — no reusable harness was specified for it). Per PLAN.md this
is diagnostic-only pending W0.3b's frozen-clock seam, not a byte-identical gate.

**milo-engine-tests:** `SkinGolden.*` (3 real + 1 intentionally-skipped `CaptureGolden`) and
`ClipPoseFixture.*` (12/12) all **PASS** — 15 passed, 1 skipped, 0 failed. Ran via
`build-agent-W0.4` (engine repo; reused rather than re-deriving W0.4's documented
`-D_GLIBCXX_NO_ASSERTIONS` workaround for the latent `CharBones::PoseMeshes` OOB-assert-under
`-O0` — confirmed a fresh `build-agent-W0.1` config WITHOUT that flag aborts (SIGABRT, exit 134)
on `ClipPoseFixture.PoseMeshesDoesNotCrash`, matching W0.4/STATUS.md's characterization exactly;
not a new regression). Full transcript -> `evidence/baseline/milo-engine-tests.log`.

**Deviation from PLAN.md, recorded:** PLAN.md's S0 step 5 cites `execution/W0.1/STATUS.md`'s
working configure recipe (`build-agent-W0.1`); in practice that dir needs W0.4's additional
`-D_GLIBCXX_NO_ASSERTIONS` cache flag to get a green `ClipPoseFixture.*` run (W0.1's own scope
was SkinGolden only and never needed it), so `build-agent-W0.4` was used for the combined
`SkinGolden.*:ClipPoseFixture.*` filter run instead of `build-agent-W0.1` alone. No source edits
made or reverted; purely a build-dir choice, both dirs are Wave-1 artifacts reused per the task
brief ("reuse if exists").

**Note for the coordinator:** the engine repo (shared working tree, its own git repo) is
currently at `834954b` ("W0.3b: register RB3_FIXED_CLOCK..."), ahead of the `9561a19` SHA rb3's
`native/CMakeLists.txt` pins via `MILO_ENGINE_PIN` — a concurrent lane's (W0.3b) commits landed
on the shared engine tree between the coordinator's last pin bump and this subtask running.
`native/build-agent-W1.2`'s `rb3-native` (and this baseline) were therefore built against the
engine's current HEAD (`834954b`), not literally the `9561a19` string in the pin file. This is
expected under the "shared working tree, pin bumped once per wave" model (every Wave-2 agent
building from the live engine checkout is in the same position) and does not block W1.2 — the
lineup-gate PASS above is itself evidence the current engine HEAD is behavior-clean for this
baseline's purpose. Not a blocker; flagged for visibility only.

**Remains for later W1.2 subtasks (S1-S4):** none — this baseline is complete and ready to be
diffed against by every subsequent MOVE commit per PLAN.md's byte-identical evidence procedure.

**Blockers:** none.

---

## W1.2.S1 — done

**Commit (engine repo `milo-native-engine`):** `daa0286` — "W1.2: extract RB3MeshEntry
cache data + invalidation into RB3MeshCache TU". 4 files: NEW `src/platform/RB3MeshCache.h`
(159), NEW `src/platform/RB3MeshCache.cpp` (48), `src/platform/Rnd_Wgpu_RB3.cpp`
(−156 net), `CMakeLists.txt` (+1). Concurrent-agent edits `FxSendNative.cpp` +
`tests/test_object_lifetime.cpp` left unstaged/untouched.

**What moved (per PLAN.md S1):**
- `struct RB3MeshEntry` (+ nested `UniformSlot`) → `RB3MeshCache.h` (verbatim).
- `sMeshGpu` / `sGeomSyncGen` map DEFINITIONS → `RB3MeshCache.cpp`.
- `LookupGeomSyncGen` → `RB3MeshCache.h` (kept `static inline`, byte-identical body).
- `CleanupGpuMesh` body + `RndMesh::OnSync` body → `RB3MeshCache.cpp` (with `#include "rndobj/Mesh.h"`).
- Header exports `extern` decls for both maps + the two per-frame counters + `CleanupGpuMesh` decl.
- `Rnd_Wgpu_RB3.cpp`: `#include "platform/RB3MeshCache.h"` added (~line 24); moved regions replaced
  with MOVED-pointer comments; **no call site touched**. The two counter DEFINITIONS stay here
  (frame lifecycle) but lost `static` → external linkage (required so the header extern decls
  resolve in-TU and for S3’s moved `RB3EnsureMeshGpu`). `sFrameSeq` + BeginFrame/EndFrame untouched.
- `CMakeLists.txt`: `RB3MeshCache.cpp` added to `MILO_ENGINE_GPU_PLATFORM_SOURCES_RB3` **only**
  (never the DC3 set; verified only my 1 line changed).

**Single-definition (exit criterion 3) — verified via `grep -rn src/`:**
`struct RB3MeshEntry {` → 1 (header). `sMeshGpu`/`sGeomSyncGen` non-extern defs → 1 each
(`RB3MeshCache.cpp`). Counter defs → 1 each (`Rnd_Wgpu_RB3.cpp`, non-static/non-extern).
`CleanupGpuMesh`/`RndMesh::OnSync` bodies appear in both `RB3MeshCache.cpp` (RB3 set) and
`MeshGpuCache.cpp` (DC3 set) — **no ODR clash**: the two source sets are mutually exclusive at
configure time (`MILO_ENGINE_GPU_BACKEND={rb3,dc3}`), exactly as PLAN.md §Risks predicts.

**Build (own dir `native/build-agent-W1.2`, clang pinned per S0):** `rb3-native` AND `rb3-tests`
both link CLEAN — RB3MeshCache.cpp compiled, no duplicate-symbol / no orphan-reference errors,
zero new warnings from the moved code.

**Test gate — `rb3-tests` (the suite that ACTUALLY links RB3MeshCache.cpp):** `70 passed, 1
skipped (DrawLogGolden.PopulatesFromRealDrawMesh — intentional), 0 failed`. Includes SkinGolden,
ClipPose, DrawLogGolden, stub-census suites. GREEN.

**MOVE proof — source-textual byte-identity (see DEVIATION below for why this replaces the
PNG-hash gate).** Diffed each moved region against `git show HEAD~1:…Rnd_Wgpu_RB3.cpp`:
`awk`-extracted `struct RB3MeshEntry` body, `LookupGeomSyncGen`, `CleanupGpuMesh`, and
`RndMesh::OnSync` are ALL **IDENTICAL** (empty diff). The only byte change anywhere is the
intended `static` keyword removal on the 2 map defs + 2 counter defs (internal→external linkage;
unique names, RB3-only TU ⇒ single instance, identical runtime behavior). A pure MOVE.

**Lineup gate — SEMANTIC PASS (stable, but NOT hash-stable — see DEVIATION):**
`python3 scripts/native/lineup-gate.py --bin native/build-agent-W1.2/rb3-native` →
`verdict=PASS img=PASS segA=PASS ratioB=PASS countC=PASS pin=PASS` on all four frames
(`coop_g_n03`×2, `coop_g_b`×2). Passes identically across every run.

**DEVIATION from PLAN.md §Byte-identical evidence procedure (recorded per HARD-RULE / brief):**
The specified byte-identical **PNG-SHA256** gate is **NOT achievable on this host** for ANY W1.2
subtask in isolation, because the frozen-clock seam (W0.3b) is a PARALLEL, still-unlanded lane.
PROVEN with a determinism negative-control: two lineup runs of the SAME S1 binary produced
completely different WIDE-PNG hashes (`f8892f94…` vs `8274e238…`) with `songMs` 16877 vs 21207
(S0 baseline captured `songMs~20392`, hash `103d49ba…`); likewise song-select differs (frame
`275` vs baseline `267`, all 5 depth hashes differ). Both capture scenes advance a wall-clock-
driven song/animation clock during boot, so pixels — and thus SHA256 — vary boot-to-boot
independent of any code change. The PLAN’s "fixed-pose capture is deterministic" assumption does
not hold pre-W0.3b. → **Substituted the strictly stronger proof for a pure MOVE: source-textual
byte-identity** (above) — if the moved statements are byte-identical and only linkage changed,
runtime output is provably unchanged, which the SHA256 gate could only *sample*. Corroborated by
the stable four-layer lineup SEMANTIC PASS. Recommend later W1.2 subtasks (S2/S3) use this same
source-identity + semantic-PASS gate until W0.3b lands a frozen clock, then a hash re-baseline can
be added retroactively.

**`milo-engine-tests` note:** that suite’s build (`build-agent-W0.4`) is configured
`MILO_ENGINE_GPU_BACKEND=dc3` (CMakeCache) — it compiles `MeshGpuCache.cpp`, NOT `RB3MeshCache.cpp`,
so it is invariant to this RB3-only change by construction; S0’s green `SkinGolden.*`/
`ClipPoseFixture.*` result stands. The rb3-side equivalent (`rb3-tests`, which DOES link the new
TU) is green above.

**Remains:** S2 (move `RB3UnpackMeshVerts` + Xbox-cvert decode helpers), S3 (move
`RB3EnsureMeshGpu`), S4 (convergence delta note). The header intentionally does NOT yet declare
`RB3UnpackMeshVerts`/`RB3EnsureMeshGpu` — S2/S3 add their own exports (PLAN keeps the surface
minimal per subtask).

**Blockers:** none.

---

## W1.2.S2 — done

**Commit (engine repo `milo-native-engine`):** `daf0ed1` — "W1.2: move RB3UnpackMeshVerts +
Xbox-cvert decode helpers into RB3MeshCache". 3 files: `src/platform/RB3MeshCache.h` (+18),
`src/platform/RB3MeshCache.cpp` (+141), `src/platform/Rnd_Wgpu_RB3.cpp` (-137 net incl. a
5-line MOVED pointer comment). Concurrent-agent edit `FxSendNative.cpp` left unstaged/untouched
(only my 3 files staged, under `flock /tmp/milo-engine-git.lock`).

**Base note:** engine HEAD had advanced 2 commits past S1's `daa0286` base by the time this ran
(`834954b` W0.3b NativeCompat registry entry for `RB3_FIXED_CLOCK\*` — registers the flag
names only, no actual fixed-clock implementation landed in `Rnd_Wgpu_RB3.cpp` yet; `0dab386`
W2-TESTFIX, test-file only, no overlap). Confirmed via `grep -rn RB3_FIXED_CLOCK src/` — only
`NativeCompatFlags.gen.inc`/`.classification.json` hits, no seam in the render TU. So the
determinism gap S1 documented (wall-clock-driven boot -> non-hash-stable PNGs) still applies
unchanged; this subtask reuses S1's substituted gate (see below) rather than re-deriving it.

**What moved (per PLAN.md S2):**
- `struct XboxCVert` + `BeFloat`/`Half2Float`/`BeUV`/`BeColor`/`BeDec4n`/`BeUDec4n`/
  `BeUByte4` → `RB3MeshCache.cpp`, kept **file-static** (no external consumer, per PLAN.md).
- `RB3UnpackMeshVerts` → `RB3MeshCache.cpp`, **exported** via `RB3MeshCache.h` (loses
  `static`; needed by `DrawMesh` now and `RB3EnsureMeshGpu` after S3).
- `Rnd_Wgpu_RB3.cpp`: the whole 141-line block (lines 1817-1957 pre-change) replaced with a
  5-line MOVED pointer comment in the same style as S1's. Both call sites (`DrawMesh` ~3701,
  `RB3EnsureMeshGpu` ~6179 — still resident in `Rnd_Wgpu_RB3.cpp` pending S3) resolve through
  the header **unchanged** — no call-site code touched, confirmed by `git diff daa0286 --
  Rnd_Wgpu_RB3.cpp` showing only the one deletion hunk.

**Deviation from PLAN.md, recorded (mechanical, not a scope change):** the exported signature's
vector element type changed from `GpuVertexRB3` to `GpuVertex` (PLAN.md explicitly calls this
out: "prefer `GpuVertex` in the shared header to avoid pulling the RB3-only alias").
`GpuVertexRB3` is `using GpuVertexRB3 = GpuVertex;` in `Rnd_Wgpu_RB3.h` — same underlying
type, zero behavior change; keeps `RB3MeshCache.h` from needing to include
`platform/Rnd_Wgpu_RB3.h`. Verified via textual diff (below) that this + the `static` removal
are the ONLY two classes of byte change in the moved block.

**MOVE proof — source-textual byte-identity:** extracted the pre-move block
(`git show daa0286:src/platform/Rnd_Wgpu_RB3.cpp` lines 1817-1957) and diffed against the
corresponding region of the new `RB3MeshCache.cpp`. Diff shows **exactly 3 hunks**, all expected:
the `RB3UnpackMeshVerts` declaration losing `static` + `GpuVertexRB3`->`GpuVertex` on the
signature, and the same alias substitution on the function's 2 internal `GpuVertexRB3&` locals.
All 7 Be*/XboxCVert bodies and every other line of `RB3UnpackMeshVerts` (both branches,
uncompressed + compressed-vert paths) are **byte-identical**, still file-static. Single-definition
check: `grep -rn "^struct XboxCVert"` and `grep -rn "RB3UnpackMeshVerts(RndMesh"` each hit
exactly 1 (both in `RB3MeshCache.cpp`) — no ODR duplicate.

**Build (own dir `native/build-agent-W1.2`):** `rb3-native` AND `rb3-tests` both link CLEAN,
zero new warnings from the moved code.

**Test gate — `rb3-tests`:** `70 passed, 1 skipped (DrawLogGolden.PopulatesFromRealDrawMesh,
intentional), 0 failed` — identical pass/skip/fail counts to S1's baseline.

**Lineup gate — SEMANTIC PASS (stable, hash gate still N/A — see S1's DEVIATION, unchanged):**
`python3 scripts/native/lineup-gate.py --bin native/build-agent-W1.2/rb3-native` ->
`verdict=PASS img=PASS segA=PASS ratioB=PASS countC=PASS pin=PASS` on both forced shots
(`coop_g_n03`x2, `coop_g_b`x2). Per-frame img scores (65.47/49.27/64.75/56.09) are in the same
range as S1's run; PNG SHA256 not compared (still boot-clock nondeterministic pre-W0.3b real
implementation, per the base note above) — source-textual identity is the controlling proof for
this pure MOVE, per S1's established precedent.

**Remains:** S3 (move `RB3EnsureMeshGpu`), S4 (convergence delta note). Header now additionally
exports `RB3UnpackMeshVerts`; `RB3EnsureMeshGpu` still resident in `Rnd_Wgpu_RB3.cpp` and its
own export is S3's job (kept minimal per subtask, per PLAN.md's S1 status note).

**Blockers:** none.

## W1.2.S3 — done

**Commit (engine repo `milo-native-engine`):** `6f9d340` — "W1.2: move RB3EnsureMeshGpu
upload helper into RB3MeshCache". 3 files: `src/platform/RB3MeshCache.h` (+forward-decl
`class BandRnd;` + export `bool RB3EnsureMeshGpu(BandRnd&, RndMesh*)` + its comment block),
`src/platform/RB3MeshCache.cpp` (+`#include "platform/Rnd_Wgpu_RB3.h"` + the ~103-line
function body verbatim after `RndMesh::OnSync`), `src/platform/Rnd_Wgpu_RB3.cpp` (−114 net:
the comment block + `static bool RB3EnsureMeshGpu{...}` deleted, replaced with a 5-line MOVED
pointer comment). Concurrent-agent edit `FxSendNative.cpp` left unstaged/untouched (only my 3
files staged, under `flock /tmp/milo-engine-git.lock`).

**Base note:** engine HEAD at S3 start was `daf0ed1` (S2). No further concurrent commits landed
between S2 and this commit; `FxSendNative.cpp` remains the sole unrelated uncommitted edit.

**What moved (per PLAN.md S3):**
- `RB3EnsureMeshGpu(BandRnd&, RndMesh*)` (the VB/IB `CreateBuffer`+`WriteBuffer`+fingerprint-stamp
  helper driving the L2 warm sweep) → `RB3MeshCache.cpp`, verbatim except losing `static`
  (file-static → external, exported via the header). Its comment block moved with it.
- `RB3MeshCache.h`: forward-declares `class BandRnd;` (helper takes it by ref for `mGpu`
  Device/Queue) and exports `bool RB3EnsureMeshGpu(BandRnd&, RndMesh*);`. Header stays light —
  `BandRnd`'s full type is NOT pulled in (fwd-decl only), per PLAN.md.
- `RB3MeshCache.cpp`: added `#include "platform/Rnd_Wgpu_RB3.h"` — supplies `class BandRnd`
  (`:67`) for `rnd.mGpu.Device()/Queue()`/`mesh->SetSphere` and the `GpuVertexRB3` alias
  (`using GpuVertexRB3 = GpuVertex;` `:42`) the body references. `rndobj/Mesh.h` (from S1) supplies
  `RndMesh::Face`/`mFaces`/`mVerts`/`IsSkinned`/`mNumCompressedVerts`. No circular include:
  `Rnd_Wgpu_RB3.h` does NOT include `RB3MeshCache.h`.
- `sMeshBufCreatesThisFrame` bumped inside the moved body resolves via the S1 `extern` decl
  (definition stays in `Rnd_Wgpu_RB3.cpp`, frame lifecycle) — unchanged.
- `Rnd_Wgpu_RB3.cpp`: `WarmGpuForDir`'s call `RB3EnsureMeshGpu(*this, it)` untouched — resolves
  through the header. `WarmGpuForDir` body itself unchanged.

**In-DrawMesh inline upload block — DELIBERATELY UNTOUCHED (deferred convergence candidate).**
Per PLAN.md S3 + the subtask brief, the per-draw inline upload in `DrawMesh` (`sMeshGpu[mesh]` …
fingerprint stamp, now ~`Rnd_Wgpu_RB3.cpp:3630`s region) is NOT extracted or deduped against the
moved `RB3EnsureMeshGpu`. It is interleaved with the per-instance uniform-slot claim, the
local-sphere recompute, and debug probes; routing it through `RB3EnsureMeshGpu` is a behavior
CHANGE, out of MOVE scope. Follow-up: a later CHANGE commit could converge DrawMesh onto
`RB3EnsureMeshGpu`, but only under the draw-log golden once W0.3b lands its frozen clock.

**Single-definition (exit criterion 3) — verified `grep -rn src/`:** exactly one definition of
`RB3EnsureMeshGpu` (`RB3MeshCache.cpp:206`), one declaration (`RB3MeshCache.h:190`), one call
site (`Rnd_Wgpu_RB3.cpp:6033`, `WarmGpuForDir`). No ODR duplicate.

**MOVE proof — source-textual byte-identity:** `git show HEAD:…Rnd_Wgpu_RB3.cpp` lines 6000-6113
(pre-move block) diffed against the moved region of `RB3MeshCache.cpp` → **exactly ONE hunk**:
`static bool RB3EnsureMeshGpu(...)` → `bool RB3EnsureMeshGpu(...)` (the linkage change). Every
other line of the body is byte-identical. A pure MOVE.

**Build (own dir `native/build-agent-W1.2`, clang pinned per S0):** `rb3-native` AND `rb3-tests`
both link CLEAN — `RB3MeshCache.cpp` recompiled with the moved body, no duplicate-symbol / no
orphan-reference errors, zero new warnings.

**Test gate — `rb3-tests` (the suite that links RB3MeshCache.cpp):** `70 passed, 1 skipped
(DrawLogGolden.PopulatesFromRealDrawMesh — intentional), 0 failed` — identical to S1/S2. SkinGolden,
ClipPose, DrawLogGolden, stub-census suites GREEN.

**Lineup gate — SEMANTIC PASS (exercises the WARM path — the S3-specific requirement):**
`python3 scripts/native/lineup-gate.py --bin native/build-agent-W1.2/rb3-native --out
/tmp/w1.2s3-lineup` → `verdict=PASS img=PASS segA=PASS ratioB=PASS countC=PASS pin=PASS` on all
four frames (`coop_g_n03`×2 img 80.7/72.47, `coop_g_b`×2 img 81.52/81.81; max_band_ratio=3.30).
The lineup boots through the loading dwell that drives `WarmGpuForDir` → `RB3EnsureMeshGpu`, so a
broken warm path would regress the skinned-mesh lineup — it does not.

**DEVIATION from PLAN.md §Byte-identical evidence procedure (same as S1/S2, recorded):** the
PNG-SHA256 gate is NOT achievable pre-W0.3b — boot advances a wall-clock song/animation clock, so
capture pixels vary boot-to-boot (this run songMs=21221 vs S0 baseline ~20392). Per S1's
determinism negative-control and established precedent, the controlling proof for a pure MOVE is
**source-textual byte-identity** (above: only `static` removed) corroborated by the stable
four-layer lineup SEMANTIC PASS. A hash re-baseline can be added retroactively once W0.3b lands
the frozen clock.

**`milo-engine-tests` note (unchanged from S1):** that suite builds `MILO_ENGINE_GPU_BACKEND=dc3`
(compiles `MeshGpuCache.cpp`, NOT `RB3MeshCache.cpp`), so it is invariant to this RB3-only change
by construction; S0's green `SkinGolden.*`/`ClipPoseFixture.*` stands. `rb3-tests` (which DOES
link the new TU) is green above.

**Remains:** S4 (convergence analysis + delta note, doc-only). The header public surface now
carries all three moved exports (`RB3UnpackMeshVerts`, `RB3EnsureMeshGpu`) + the data/invalidation
from S1 — the full W1.2 extract is source-complete pending S4's written verdict.

**Blockers:** none.

---

## W1.2.S4 — done

**Type:** doc-only (no source change), per PLAN.md S4. Satisfies the brief's "diff to prove
identity, else record the delta" requirement.

**Evidence file:** `evidence/convergence-notes.md` (full per-axis diff verdict, grounded in a
fresh read of `RB3MeshCache.{h,cpp}`, `gfx/VertexFormats.{h,cpp}`, `platform/MeshGpuCache.{h,cpp}`
at engine HEAD `6f9d340`).

**Axis 1 — `RB3UnpackMeshVerts` vs `VertexFormats::Unpack{Static,Skinned,Compressed,CompressedSkinned}Vertices`:
NOT provably identical** (FIVE divergence points, one behavioral):
1. Signature/alloc: RB3 = 1 fn, `vector&` self-`resize`, returns count/−1; VertexFormats = 4 fns,
   `out[]+maxVerts`, caller-allocated.
2. Compressed decode: RB3 reinterpret-casts to `struct XboxCVert{int...}` (36B) + `Be*` bswap
   helpers; VertexFormats reads by byte-offset (`LoadBE32(rec+kCV_*)`), deliberately no type-pun.
3. **Compressed COLOUR channel order — REAL behavioral divergence.** RB3 `BeColor` = `0xAARRGGBB`
   (R=v>>16); VertexFormats `UnpackColor_BE` = `0xAABBGGRR` (R=v>>0). **R↔B swapped.** RB3's comment
   records this was a deliberate change (prelit-gated in-shader); swapping in the shared unpacker
   re-introduces the swap. Not interchangeable.
4. Uncompressed Vert access: RB3 reads `mVerts` + `color.fr()`/`uv`/`boneWeights.GetX()` (Color32
   packed); VertexFormats reads `Verts(i)` + `color.red`/`tex`/`boneWeights.x` (float channels) →
   different effective `RndMesh::Vert` shapes.
5. Tangent: VertexFormats runs MikkTSpace (uncompressed) + extracts DEC4N bitangent sign
   (compressed `tangent[3]=±1`); RB3 hardcodes `tangent=(1,0,0,1)` everywhere. Handedness diverges.
   (UV half-float + UDEC4N/UBYTE4 bone bit-math ARE equivalent, but can't be shared in isolation.)

**Axis 2 — `RB3MeshEntry`/`sMeshGpu` vs `GpuMeshData`/`sMeshGpuData` (MeshGpuCache): NOT identical.**
RB3's entry is a strict superset for the browser-WebGPU leak/backpressure problem; DC3's is a plain
VB/IB record + viewer/text metadata. RB3-only: fingerprint (`ownerKey`/`fpVerts`/`fpFaces`/
`fpSkinned`) + `sGeomSyncGen` owner-generation invalidation (auto-detects position-only owner
rewrites — the sustain-tail case DC3's explicit `InvalidateGpuMesh` can't) + L1 `cachedSkinnedVerts`
+ the per-instance `UniformSlot` free-list (obj/bone/mat UB+BG + material-view invalidation +
`frameSeen`/`nextSlot`). DC3-only: `depthBias`/`debugLabel` + the `SetMeshDebugLabel`/`DepthBias`/
`MeshLabel`/`GetMeshGpuData`/frame-stats API. `RndMesh::OnSync` differs too (DC3 only clears
`uploaded`; RB3 also `++sGeomSyncGen[this]`).

**Convergence path (deferred, OUT of W1.2 scope — a later gated CHANGE wave):** `01-renderer-core.md`
§5a's `MeshUploadCache` interface both backends implement — but it must first reconcile the hard
divergences (single unpack alloc-contract + Vert accessor + the compressed-colour R↔B order +
tangent policy; shared VB/IB+fingerprint core with the RB3-only UniformSlot/L1/owner-gen as an RB3
specialization and DC3-only depthBias/debugLabel as a DC3 specialization) and be gated under the
draw-log golden (post-W0.3b) + lineup gate, since it changes generated output. Related in-item
follow-up (from S3): routing DrawMesh's inline upload through `RB3EnsureMeshGpu` is likewise a
deferred CHANGE.

**Exit criterion 8 (S4 verdict recorded — unpack + entry both NOT provably identical, kept verbatim,
convergence deferred): MET.**

**Commits:** doc-only. Evidence `evidence/convergence-notes.md` + this STATUS.md section (rb3 repo,
staged under `flock /tmp/rb3-git.lock`).

**Remains:** none — W1.2 is source-complete (S1–S3 MOVE commits landed in the engine repo) and this
S4 verdict closes the item.

**Blockers:** none.
