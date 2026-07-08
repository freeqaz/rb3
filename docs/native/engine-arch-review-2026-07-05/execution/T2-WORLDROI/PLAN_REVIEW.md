# T2-WORLDROI — PLAN review (Fable, adversarial)

**Target:** `execution/T2-WORLDROI/PLAN.md` @ rb3 `0df0e0c8`. Engine pin `beb89e5` (verified =
engine HEAD; dirty = `M src/platform/FxSendNative.cpp` only). Reviewed against: OPTIONS.md §6.3
charter + WAVE19_KICKOFF Lane P + WAVE19_REVIEW A1 (BINDING) + the ten §4 lints + the CURRENT
tree — every load-bearing anchor re-derived below by symbol.

**VERDICT: APPROVE-WITH-AMENDMENTS** — amendments B1–B8 below are BINDING on the implementer.
The core design decision (re-derive bone worlds inside `RecordDrawProv`, zero intrusion into
the compose loop, everything behind default-OFF `RB3_DRAWLOG_PROV`) is verified sound. But two
of the plan's labeled assumptions resolve the WRONG WAY when attacked against the tree — the
plan's own prescribed fallback for A-1 would double-transform crowd/prop draws, and its clamp
claim is inverted in exactly the failure class M5 exists to triage. Neither invalidates the
architecture; both change what the implementer must build.

---

## Anchor verification (all re-derived at HEAD)

Engine `Rnd_Wgpu_RB3.cpp` @ `beb89e5`: DrawMesh :2530 ✓, GeomOwner :2546 ✓, `skinned` :2675 ✓,
`BoneUniforms bones{}` :3374 ✓, NumBones/kMaxBones :3377-3378 ✓ (kMaxBones=40,
`gfx/UniformStructs.h:102`), worldfix block :3656-3681 ✓, bone loop :3776 ✓, BoneTransAt :3777
✓, identity fill :3778-3789 ✓, `wt = bt->WorldXfm()` :3790 ✓, finite guard :3827-3831 ✓,
`Multiply(off, wt, skin)` :3833 ✓, viewProj CPU copy :1545 ✓ (ProvOn-gated),
`RecordDrawProv(mesh, mat, mu.color, skinned, ctx.world)` :5629-5630 ✓, ProvOn/RB3DrawProvEnabled
:5672-5690 ✓, scope stacks + push/pop :5700-5714 ✓, `RB3ProvProjectToScreen` :5727-5745 ✓,
`RecordDrawProv` body :5747-5826 ✓ (kind=0 verts :5776, kind=1 sphere :5789, kind=2 :5822).
`RB3DrawLogDebug.h` struct :61-74 ✓; `RB3DrawScopeGuard` :92-101 ✓. rb3: `HandleDrawLog` :218 ✓,
prov emit :281-299 ✓ (plan's :280-298 exact), ROI filter :302-322 ✓,
`Character::DrawShowing` :291 ✓ (Name() use :340 ✓), Text.cpp RAII :26-50 ✓,
uidump_query.py :127/:146/:170 ✓, classjson `RB3_DRAWLOG_PROV` row :518 ✓, golden = 792 draws ✓,
`drawlog-golden.py` has `--fixed-clock/--scene/--fail-red-audit` ✓. Oracles exist:
`IsBandHandMesh`/`HandBoneRole`/`HandBoneSide` (`GameRenderHook.h:244+`, used :3753/:3757/:3994)
✓. Crowd path: `curChar->DrawShowing()` at `Crowd.cpp:574` dispatches to
`Character::DrawShowing` (no override between Character and band/crowd classes — grepped) so E3
covers band + 3D crowd ✓. Collision matrix holds: `Character.cpp` is not a W-ISO TU; engine
edits disjoint from `FxSendNative.cpp` ✓. §6.3 charter fidelity: rectKind=3 + per-bone sub-rects
+ owner scope + all four validation items present ✓.

---

## B1 (HIGH) — ASSUMPTION A-1 resolves the OPPOSITE way: never apply `ctx.world`; the plan's prescribed fallback would double-transform crowd/prop skinned draws

- **Plan claim (§E2 projection note + A-1):** "skinned `ctx.world`==identity — verify once …
  if some skinned world draws carry a non-identity `ctx.world`, apply it (the bone point is
  then mesh-local and needs `world`)."
- **Tree:** the SYS-1 placement contract is **DEFAULT-ON** (Wave 6 flip,
  `Rnd_Wgpu_RB3.cpp:3299` `kPlacementContractDefaultOn = 1`). Under it, the general skinned
  arm sets `obj.world = meshWorld` — "== I for character meshes … **== spXfm/prop world
  otherwise**" (:3323-3326) — so skinned crowd/prop draws legitimately reach `RecordDrawProv`
  with a NON-identity `ctx.world`. But the palette is simultaneously rewritten to
  `skin * inverse(meshWorld)` so that "**worldPos = skin*v is unchanged**" (contract doc
  :3684-3687). Bone `WorldXfm().v` is therefore ALWAYS a world-space point for the general
  arm, whatever `ctx.world` holds. The plan's fallback ("apply it") would multiply the world
  point by `meshWorld` a second time and mis-project exactly the crowd/prop class — and M0's
  prescribed check (print ONE **band** draw's `ctx.world`, which IS identity) would never
  catch it.
- **Correction (BINDING):** the skinned branch ALWAYS projects bone worlds through an
  identity `world`. Delete the "apply it" contingency. Rewrite the M0 check: confirm the draw
  takes the general arm (not the assumption that ctx.world==I); the invariant to cite in the
  code comment is the contract's own ":3684-3687 worldPos = skin*v".
- **Same amendment, second half — exclude the name-scoped UI placement arms:** for
  `scrollbarThumb` (:3315-3316, `obj.world = sScrollbarPlacement`, palette NOT stripped) and
  `hubBarPlacement` (:3317-3322, obj.world = mesh translation), rendered geometry =
  `obj.world ∘ skin(v)` while the bone worlds sit near the ORIGIN (the HUB_BAR_PROBE comment
  :3835-3841 documents precisely this: "boneWorld + composed skin land near origin, not at
  the focused label"). A bone-world bbox for these meshes is a confidently-wrong rect — a
  REGRESSION vs today's positioned sphere. Correction: compute
  `bool skinnedPoseValid = skinned && !scrollbarThumb && !hubBarPlacement` where those locals
  are in scope in `DrawMesh` and pass it into `RecordDrawProv` (one added bool param), OR
  re-derive by name in the sink (`GetGameRenderHook()->IsHubBarMesh(...)` + the two scrollbar
  mesh names). Excluded draws keep the sphere fallback (disclosed via rectKind=1 +
  skinned:true, exactly the plan's existing bypass-disclosure rule).

## B2 (HIGH) — the clamp claim is INVERTED, and it matters for exactly the M5 triage class

- **Plan claim (§1 skip rule):** "A clamped bone (SKIN_CLAMP, :3948) still has a real
  skeleton world → it IS included (the clamp only substitutes the bone's *vertices* to bind;
  its world position is still where the structure is — correct for localization)."
- **Tree:** the clamp does the opposite of what that parenthesis implies for localization:
  `:3969 continue; // keep identity for this bone (vertices stay at bind)` — a clamped
  bone's vertices RENDER AT BIND POSE (and under the contract the identity fallback is
  `inverse(meshWorld)` :3778-3783, same bind result). The rendered pixels for
  clamped/null/runaway bones are NOT at the bone world. The FOREARM-FLOAT structure —
  "persistent top-center floating flesh-colored structure, unchanged OFF→ON"
  (`R5-HANDS-ENDGAME/CLOSURE.md:98-101`) — is a prime candidate for exactly this mechanism
  (geometry rendering at bind while the skeleton is elsewhere). A bbox built ONLY from bone
  worlds can therefore FAIL TO COVER the very pixels the M5 killer query targets: the ROI
  query returns nothing and the production smoke fails while the instrument reports GREEN
  gates.
- **Correction (BINDING):**
  1. Per-draw fallback disclosure: emit a `boneFallback:N` count in the prov row (N = null +
     non-finite + clamped bones). Cheapest faithful source: a ProvOn()-gated per-draw counter
     stashed by `DrawMesh` (the loop already counts `sFallbackBones` :3375/:3960) and passed
     to / read by `RecordDrawProv`; re-deriving the clamp test in the sink is NOT equivalent
     (it needs `reboundSkip` :3912 + `sSkinClamp` state). One gated stash line in DrawMesh is
     an acceptable, inert-flag-off deviation from "zero intrusion" — G4 remains the proof.
  2. When `boneFallback > 0`, UNION the existing sphere-corner projection (:5799-5811, the
     bind-pose extent) into the rectKind=3 bbox — the rect must over-approximate the rendered
     geometry, and part of it renders at bind. Clean draws (N==0, the overwhelming majority)
     keep the tight bone-world bbox, preserving G1/G2.
  3. M5 protocol addition: if the primary ROI query names nothing plausible, re-rank draws by
     `boneFallback>0` and/or re-run the RED (sphere) arm as the fallback triage path — commit
     whichever answer the evidence supports. (Lint 8 spirit: the negative result must carry
     its hit-counts.)

## B3 (MEDIUM) — G1's cardinality criterion assumes the wrong v1 failure mode; it can false-RED

- **Plan claim (§5 G1):** GREEN iff `count(rectKind3 ∩ ROI) < count(sphere ∩ ROI)`; "the
  sphere arm returns ~all skinned draws regardless of frame."
- **Tree:** the shipped sphere fallback is NOT near-full-viewport: it projects the 8
  sphere-box corners and is documented "POSITIONED (an under-bound, **never
  full-viewport**)" (:5792-5797, landed with R3 in `753ed20`). The R3 STATUS blindness line
  (:58) says near-full-viewport **for static UI quads**, not for skinned world draws. For a
  skinned character mesh the sphere is bind-pose mesh-local projected through `world` (≈I) —
  i.e. potentially small and MISLOCATED (and `r=1` when the authored radius is 0, :5800).
  The RED arm may thus return FEWER ROI matches than the GREEN arm → the inequality reads
  RED while the instrument is working.
- **Correction (BINDING):** define G1 as a known-answer contrast on the same band ROI: RED
  arm FAILS to localize — either undiscriminated (≥K matches) OR the known draw's rect does
  not cover the ROI / mislocates it; GREEN arm returns a bounded set that includes and names
  the mesh + bone(s). Report both cardinalities as committed evidence, not as the pass
  criterion. Also fix the plan text: `RB3_PROV_SKIN_SPHERE` as specified is a process-cached
  **env** read (getenv at first call), not compile-time — "flip the knob is a rebuild" is
  wrong; two boots of the SAME binary suffice (cheaper than the plan claims; the two-boot
  cardinality-robustness argument still applies).

## B4 (MEDIUM) — M0 omits the BINDING review-A1 mesh-cache bypass check

WAVE19_REVIEW A1 lane step 0 (BINDING): "verify … that the mesh-GPU-cache / multi-draw path
(`MeshGpuCache.cpp`) does not bypass per-draw palette compose for any skinned mesh." The
plan's M0 checks TU flavor + compose freshness probes but never names this check. My own read
says it will pass trivially — `RB3MeshCache.cpp`/`MeshGpuCache.cpp` are vertex-unpack/upload
caches, not draw paths, and `RecordDrawProv` sits at the tail of `DrawMesh` so any draw in the
sidecar has composed — but the check is BINDING and cheap: add to M0 "confirm prov-ring skinned
row count ≈ skinned submissions for one frame (e.g. drawlog `skinned` flag count), and grep
that no other call site issues skinned draws." Record the result in STATUS.

## B5 (MEDIUM-LOW) — G3 caveat: the default-ON mitten makes rendered fingers diverge from raw finger-bone worlds on TRIGGERED frames

The mitten blend rewrites a triggered finger's palette entry toward wrist-rigid
(:3988-4000+) while `boneRects` (per this plan) reports the RAW finger bone world. On a
mitten-triggered frame G3's "boneRects intersecting the ROI name finger/wrist bones" can fail
against a perfectly correct instrument (rendered hand at wrist-rigid, reported finger bones at
the displaced pose). Amend G3: query a coherent hand frame (mitten no-op — the common case) or
accept wrist-level naming as GREEN; note this in the gate evidence so a triggered frame is not
misread as instrument failure.

## B6 (LOW) — parent-segment sub-rects: gate the parent endpoint on palette membership

`bt->TransParent()` is not guaranteed to be another palette bone — a root bone's parent can be
a dir/proxy transform at the origin or stage placement, producing a giant spurious sub-rect
that intersects EVERY ROI and adds a phantom bone name to every query. Correction: include the
parent endpoint only if the parent is itself one of this mesh's `BoneTransAt(0..nb)` set
(cheap pointer-set built in the same loop); otherwise the sub-rect degenerates to the bone
point (still a valid, small rect).

## B7 (LOW) — sketch-level implementation corrections (fold in silently)

1. **E2.3 is aimed at the wrong block:** the exact-vert `vv` path CANNOT run for skinned
   draws — `vv = (mesh && !skinned) ? &mesh->Verts() : nullptr` (:5776). Only the sphere
   `else if (mesh)` (:5789) needs the `kind != 3` guard (or restructure as
   `else if (mesh && kind != 3)`).
2. **E4 placement:** the prov snprintf (:298-311) already CLOSES the prov object with `}` —
   `boneRects` must be injected before that closing brace (split the format string), not
   appended after, or it lands outside `prov` and the plan's "inside the prov object" contract
   + `uidump_query.py` reading breaks.
3. E2 sketch typos: `sminbx` vs `minbx`; `anyBehind` in the skinned branch should not force
   kind=2 when other bones projected (mirror the existing skip semantics). If E1 nests
   `RB3ProvBoneRect` inside `RB3DrawProv`, E4 must qualify it
   (`RB3DrawProv::RB3ProvBoneRect`) or use `const auto&`.

## B8 (LOW) — make the Wii byte-identical claim evidence, not assertion

The `#ifdef HX_NATIVE` hook in `Character.cpp` is preprocessor-inert on MWCC (shipped
Text.cpp precedent) — I agree it cannot move the match. But per lint 7, add to M2's exit one
cheap committed check: `mcp batch_objdiff` (or the unit's report.json match%) on
`system/char/Character` unchanged pre/post — one line of evidence closing the "cannot silently
regress the Wii match" obligation where this lane touches `src/system`.

---

## What survives attack unchanged (for the implementer's confidence)

- The central design decision (re-derive in `RecordDrawProv`, not in the 1500-line compose
  body) is CORRECT and verified: `RecordDrawProv` runs at DrawMesh's tail (:5630), the palette
  is a per-draw local (:3374) with no cache-skip between :2675 and :3776, and the worldfix
  block (:3656-3681) forces fresh worlds — the freshness argument holds.
- Flag topology is right: everything rides default-OFF `RB3_DRAWLOG_PROV`; `RB3DrawProv` is
  the parallel non-golden struct (header doc :55-60), so the 792 golden (verified: 792 draws,
  dict) is untouchable by construction; G4 + `--fail-red-audit` is the right regression net.
- E3 owner scope: single site covers band + 3D crowd (Crowd.cpp:574 dispatch verified); RAII
  pattern is the shipped Text.cpp one verbatim; `scopeOwner` already serialized (:5757, :300).
- Collision matrix, classjson row shape (mirrors :518-524), naming box (no timing-axis claims
  anywhere in the plan), M-structure and evidence discipline: all clean.
- A-2 (`Character::Name()` as owner label) is correctly framed with an M0 check; the
  kickoff's `rb3_char_probe` machinery indeed does not exist (grep confirmed).

**Verdict: APPROVE-WITH-AMENDMENTS.** B1–B8 are binding; B1 and B2 change code the implementer
would otherwise write wrong; B3 changes a gate criterion that would otherwise false-RED;
B4/B5/B8 are gate/evidence additions; B6/B7 are correctness details. No redraft — the plan's
skeleton, milestones, and flag/collision discipline are sound as written.
