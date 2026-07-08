# Wave 19 — Pre-dispatch review (Fable)

**Target:** `WAVE19_KICKOFF.md` @ rb3 `d93fa894`, engine pin `beb89e5` (= engine HEAD, verified).
**Scope:** lane scoping/sequencing vs the §6 charters (rb3 `c4395043`) and the CURRENT tree.
Every anchor below re-derived by symbol at HEAD.

**VERDICT: DISPATCH WITH AMENDMENTS** — A1/A2/A3 must be folded into the lane prompts before
dispatch; A4–A6 are one-line scope corrections; nothing requires a redraft.

---

## Amendments (ranked)

### A1 (HIGH, Lane P) — the declared engine region is the WRONG BLOCK; the charter's grounding claim is imprecise but the build survives, cheaper than feared

- **Kickoff claim:** skinned-pose bboxes "computed from the mitten pre-pass's existing
  CPU-side composed palette (`Rnd_Wgpu_RB3.cpp:3752-4009`)"; §6 T2 claims "the mitten
  pre-pass already composes every band bone's skin matrix CPU-side per draw".
- **Tree:** the mitten pre-pass (engine `Rnd_Wgpu_RB3.cpp:3707-3775` at `beb89e5`) is gated
  `sMittenOn && numBones >= 8 && geomHook->IsBandHandMesh(mesh->Name())` (:3752-3753) and
  composes only WRIST transforms (:3764). The reusable palette is NOT the mitten's — it is
  the **general skinned-palette compose loop** inside `BandRnd::DrawMesh` (fn starts :2530):
  `numBones = owner->NumBones()` at **:3377**, per-bone compose/write loop starting
  **:3776** (`bones.bones[b]`, bone `wt = bt->WorldXfm()` :3790, final `skin` past the
  finite/clamp guards ~:3990), continuing through the palette-write loops at :4056-:4113.
  That loop runs for **every** skinned draw through `BandRnd::DrawMesh` — band, crowd,
  extras, world — with the mitten blend as a hand-gated insert.
- **Correction:** R-C resolves FOR the lane: **no new palette tap needed**. Re-scope the
  declared engine region to the `BandRnd::DrawMesh` skinned-palette branch (**~:3377-:4130**,
  by symbol, not the 3752-4009 mitten sub-block). Per-bone posed positions are available two
  ways in that loop: the composed `skin` applied to the bone's bind point, or — cheaper and
  sufficient for bbox/sub-rects — the bone WORLD translation `bt->WorldXfm().v` directly
  (post-`RB3_NO_SKEL_WORLDFIX` forcing at :3630ff, so worlds are fresh). Bbox over bone
  worlds + per-bone sub-rects from the same points; skip `sBonesIdentity`/fallback bones
  (:3786-:3789) explicitly.
- **Lane step 0 (add):** verify every WORLD skinned draw actually reaches this loop —
  specifically that the mesh-GPU-cache / multi-draw path (`MeshGpuCache.cpp`, memory: engine
  `b5309b3`) does not bypass per-draw palette compose for any skinned mesh. If a bypass
  exists, those draws keep `rectKind=1` and the prov row must SAY so (disclosed gap, not
  silent sphere-rect).
- Region disjointness: engine dirty state is `M src/platform/FxSendNative.cpp` only (+1/-2)
  — disjoint from the corrected region. Lane P sole-engine-writer stands.

### A2 (HIGH, Lane F) — "LW-1/ThreadCall completion seam" is half-stale; and gate-1 will FALSE-RED if run on the boot window

- **Kickoff claim (line 45-46):** "Target = the LW-1/ThreadCall completion seam (W0.3b
  already pins queue drain under fixed clock, Loader.cpp:622,729 — bytes-arrival frame is
  what varies)."
- **Tree, two corrections:**
  1. `Loader.cpp:622,729` re-verified — both are the `drainToEmpty = (mPeriod >= 1e29f) ||
     RB3FixedClockActive()` lines (web/native arms). ✓. But **ThreadCall completions cannot
     vary seam-ON**: engine `ThreadCall_Native.cpp:205` (`LoadDetSerialize()`, gated
     `RB3_FIXED_CLOCK && RB3_LOAD_DETERMINISM`, :65-:71) runs pending jobs INLINE under the
     seam — landed as W0.3d-b item 4. The only live frame-assignment mechanism seam-ON is
     the **LW-1 async-file arrival**: `FileLoader`'s `mFile->ReadAsync(...)` at
     `src/system/utl/Loader.cpp:921` + the `ReadDone` poll (engine `AsyncFile_Native.cpp`)
     — a blocked loader becomes drainable on whatever frame bytes arrive. The lane's
     `frameAssign` markers belong at the rb3-side ReadDone-observed flip (keeps Lane P sole
     engine writer). Rename the target: "the LW-1 async-file arrival seam (ThreadCall is
     inline under the seam per LoadDetSerialize)".
  2. **Gate-regime caveat, binding:** `W0.3d-b/STATUS.md:69-77` **REFUTED H-TIMING for the
     headless boot window** — all 511 completions land by frame 2, byte-identical, 10/10.
     The open evidence (songMs 21003 @ frame 3585 vs 5095; `InitParticle`
     [2505,1755,2509]) is the **eng_hot/gameplay window**. Gate-1's "N=6 seam-ON boots must
     show frameAssign/emitTimeline DIVERGING, else instrument-blind → RED" must therefore
     run on the eng_hot capture regime (song load → gameplay window). Run on the boot
     window it is *expected* 6/6-identical and would wrongly condemn a working instrument.
- **One-boot-log gradability (R-B second half): YES, no second probe TU.**
  `native/src/rb3_loaddet_probe.cpp` already has the per-frame tap + attrib flush
  (`RB3LoadDetFrameTap` :117, `FlushAttrib` :89) and completion markers carry `frame=`
  (:145); the attrib tap in `Rand.cpp` fires BEFORE the redirect (:188-:189), so
  isolated-stream draws ARE per-PC-counted → `emitTimeline` is derivable per-frame per-PC
  (addr2line → consumer) from the existing stream. `[WASHPROBE]` lines (engine
  `Rnd_Wgpu_RB3.cpp:1140`, `RB3PostProc.cpp:225`) carry no frame key but interleave on the
  same stderr — join by log-order adjacency to the `[LOADDET] frame=` markers; no engine
  edit. New code: the ReadDone-frame marker (Loader.cpp), the songMs-per-frame sample (see
  A3), and the three `loaddet_gate.py --timeline` axes.

### A3 (MEDIUM, Lane F) — the songClock source collides with the DIRTY hazard file; resolve ownership BEFORE dispatch

The natural songMs source, `CurrentSongMs()`, lives in `native/src/rb3_session_trace.cpp`
— the exact file the hazard note forbids staging. Worse: the current uncommitted +10 diff
in that file IS a linkage fix (anon-namespace → external, comment dated 2026-07-07) made
*specifically so `rb3_loaddet_probe.cpp` can `extern` it* — someone already started
Lane-F-adjacent work. If Lane F's probe declares `extern float CurrentSongMs();` against
the uncommitted fix, Lane F's own commits produce a tree that does not link.
**Correction (pick one, coordinator, pre-dispatch):** (a) have the diff's owner commit the
linkage fix first and note it in the lane prompt; or (b) Lane F avoids
`rb3_session_trace.cpp` entirely and samples songMs from the `NotifyFrame` call site
(`native/src/rb3_http_handlers.cpp:1033`, clean file) into a probe-owned variable.

### A4 (MEDIUM, Lane I) — LightPresetManager anchor points at the PROBE branch; guards must be enclosing-function-scoped

All four anchors re-derived at HEAD; three are exact:
`CharClipDriver.cpp:62` = `RandomFloat(0, mClip->Range())` in the `CharClipDriver` ctor ✓;
`Crowd.cpp:1234` = `RandomInt() % (i + 1)` Fisher-Yates inside `WorldCrowd::OnIterateFrac`
(:1216) ✓; `CharInterest.cpp:172` = `RandomFloat(-0.25f, 0.25f)` in
`CharInterest::ComputeScore` ✓. **But `LightPresetManager.cpp:286` is the
`RB3_BOOTRNG_PROBE` branch copy** — the live shipping draw on graded (probe-OFF) boots is
**:294** (`mPresets[s][RandomInt(0, count)]`); both branches draw once. The R4-M4 evidence
resolved :286 because addr2line landed in the probe-armed inline. **Correction:** the
guard goes at **`PickRandomPreset` function scope**, covering :286 AND :294 — and state
the same rule for all four: guard at the top of the enclosing function (ctor /
`OnIterateFrac` / `ComputeScore` / `PickRandomPreset`), matching the CharEyes precedent
(`CharEyes.cpp:522-528`, `#ifdef HX_NATIVE` + one line). G3 is then trivially safe (MWCC
never sees HX_NATIVE). Do NOT blanket-guard Crowd.cpp — :810/:812 (color-palette draws)
were not attributed; scope to `OnIterateFrac` only. The R4 pattern extends with **zero
`Rand.cpp` edits**: `RB3LoadDetStream` lazily creates tag streams (`Rand.cpp:77-85`) and
`RB3ReseedGRandAtAnchor` resets the whole registry generically (:64-:68).

### A5 (MEDIUM, I∥F) — declare Rand.cpp ownership explicitly: Lane F only

Per A4, Lane I needs no `Rand.cpp` change. But the kickoff's lint-8 self-grade ("I:
per-site tag hit-counts in the ledger") tempts Lane I into adding per-tag draw counters —
which today do not exist (isolated draws bypass `sGRandDrawCount`, `Rand.cpp:120-122`; only
the per-PC attrib tap sees them) and would land in `Rand.cpp`, exactly where Lane F's
`emitTimeline` work goes. **Rule:** `src/system/math/Rand.{h,cpp}` is **Lane F's file this
wave**; Lane I's writable code set is exactly the four consumer TUs. Lane I satisfies
lint-8 with the existing attribution mode's per-PC counts (addr2line → the four symbols).

### A6 (LOW, R-D) — resolve the declared I∥F script overlap by re-scoping, not landing order

- `scripts/native/loaddet_gate.py`: no real overlap — the kickoff already says "I adds
  nothing there". Assign F-only. ✓ as written.
- `execution/R4-M4/wash_cosample.py` (note: it lives under R4-M4/, not scripts/native/ —
  name the path in the lane prompts): **move wholly into Lane F**, including its
  capture_lints wiring (the kickoff itself offers this option; take it). Lane I owns the
  NEW `scripts/native/capture_lints.py` + wiring `execution/R4-M4/white_regrade.py` only.
- Residual ordering: Lane I lands `capture_lints.py` as its FIRST checkpoint commit (new
  file, conflict-free); Lane F imports it for wash v2. If F reaches v2 first, F stubs the
  import and re-wires when I's file lands — one line either way.

### A7 (LOW) — verified-clean items (no change)

- Pin `beb89e5` = engine HEAD ✓ (engine log). TWELVE defaults ✓ (README:636; closeout:133
  "arithmetically right"; registry regenerated in `d138888`/`beb89e5`).
- Hazard notes accurate ✓: engine `M FxSendNative.cpp` (+1/-2, audio); rb3
  `M native/src/rb3_session_trace.cpp` (+10, the A3 linkage fix). Neither staged by lanes.
- `Loader.cpp:622,729` anchors exact ✓. Evidence file
  `R4-M4/evidence/venue-path-divergent-consumers.md` matches the four consumers, sum
  8+7+1=16 = observed spread (F3 "within ±1" honored) ✓.
- Lint 9 pre-verified: all four Lane-I TUs resolve via addr2line in the attribution boots →
  compiled into rb3-native ✓ (keep lane step 0 anyway).
- Wash v2's refusal target `R4-M4/evidence/wash_natural.json` exists ✓.
- T3 correctly excluded; WHITE re-grade correctly deferred to Wave-20 ✓.

---

## Answers to R-A..R-D

**R-A (bounded re-attribution loop): YES, and 2 is the right bound — with one binding
rule that kills the whack-a-mole.** The exit gate (eng_hot OFF-arm ledger stream 10/10) is
itself the completeness oracle: a missed fifth consumer FAILS it. What makes iteration
convergent rather than whack-a-mole: **re-attribution must grade the failing gate boots'
OWN logs** (the A2/F6 VOID discipline — never discard-and-rerun), because the attrib tap
records every gRand-reaching draw by PC, so one pass over the 10 failing boots names the
complete residual set *for those boots*; N=3 attribution (how R4-M4 found these four)
under-samples rare-divergence consumers like LightPreset (spread [1,0,1]). Iteration 1 =
the four named sites; iteration 2 = isolate whatever the 10-boot gate logs name. If it
still fails after 2, the residual is presumptively NOT a per-consumer stream-position axis
— it is draw-count-within-consumer variation driven by frame assignment, which is Lane
F/T1's domain — so the stop rule is "hand the failing boots' logs to T1's attribution,
do not add a third guard round."

**R-B:** Right seam in substance, stale in name — see A2. Under the seam, ThreadCall is
inline (`LoadDetSerialize`), so the attribution point is the LW-1 async-file
arrival→drainable-frame transition (Loader.cpp:921 ReadAsync / ReadDone poll), plus the
already-frame-keyed DirLoader/DataLoader complete markers. All three axes ARE gradeable
from one boot log via the existing probe TU (per-frame tap + attrib flush + log-order join
for `[WASHPROBE]`); no second instrumentation pass, no second probe TU — but the songMs
sample must dodge the dirty `rb3_session_trace.cpp` (A3), and gate-1 must run on the
eng_hot window, not the H-TIMING-refuted boot window (A2.2).

**R-C:** The mitten pre-pass palette does NOT cover all world skinned draws — but the
question dissolves: the mitten is a hand-gated insert inside the GENERAL skinned-palette
compose loop, which runs for every skinned `BandRnd::DrawMesh` draw. T2 needs no own
palette tap; bbox from bone worlds (`bt->WorldXfm().v`, post-worldfix) is the cheap
sufficient form. The declared region moves to the general branch ~:3377-:4130 (A1), with
lane step 0 checking the mesh-cache path for skinned bypasses. Region is disjoint from the
only live engine dirt (FxSendNative.cpp).

**R-D:** Re-scope: `loaddet_gate.py` F-only (as drafted); `wash_cosample.py` wholly F
(including its capture_lints wiring); I owns `capture_lints.py` (new) + `white_regrade.py`
wiring, lands `capture_lints.py` first (A6).

---

## Dispatch verdict

**DISPATCH WITH AMENDMENTS.** Fold A1 (Lane P region + step-0 cache check), A2 (Lane F
seam rename + eng_hot gate regime), A3 (songMs source decision), A4 (function-scope guards,
:294), A5 (Rand.cpp = F-only), A6 (wash_cosample → F) into the lane prompts. No lane
charter is invalidated; no redraft needed.
