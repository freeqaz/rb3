# T2-WORLDROI — STATUS (implementer, Opus)

**Lane:** Wave-19 Lane P — world-cam ROI provenance (skinned-aware). SOLE ENGINE WRITER this wave.
**Engine pin:** `beb89e5` (unchanged — no pin bump; edits picked up via `add_subdirectory` working tree).
**rb3 base:** `2b2fcb46`.
**Naming box:** T2 = SPATIAL/ROI axis (which mesh/bone/owner drew a pixel). Touches NOTHING on the
T1 frame-assignment TIMING axis or R4's ledger `order` axis. No doc/code here conflates them.

## Amendment adoption (PLAN_REVIEW.md B1–B8, all BINDING) — how each was applied
- **B1 (HIGH)** — bone worlds are ALWAYS projected through an IDENTITY model matrix (the SYS-1
  placement contract keeps `worldPos = skin*v` invariant, Rnd_Wgpu_RB3.cpp:3683-3687), never
  `ctx.world` (would double-transform crowd/prop draws). The plan's "apply ctx.world" contingency
  is DELETED. The name-scoped UI placement arms (`scrollbarThumb`, `hubBarPlacement`) are excluded
  via `skinnedPoseValid = skinned && !scrollbarThumb && !hubBarPlacement`, computed in DrawMesh and
  passed as a param; excluded draws keep the disclosed sphere fallback.
- **B2 (HIGH)** — clamped/null/nonfinite bones render at BIND, not at the bone world. `boneFallback:N`
  (DrawMesh's per-draw `sFallbackBones`) is emitted per prov row; when N>0 the rectKind=3 bbox UNIONS
  the legacy bind-pose sphere extent (through `world`, as the kind=1 path does) so it over-approximates.
  M5 gains a fallback triage path (re-rank by boneFallback / re-run RED sphere arm).
- **B3 (MED)** — G1 redefined as a known-answer localization contrast (RED fails to localize; GREEN
  names mesh+bone), cardinalities as evidence only. `RB3_PROV_SKIN_SPHERE` is env-cached (getenv at
  first call) → two boots of the SAME binary A/B it; no rebuild.
- **B4 (MED)** — M0 mesh-cache bypass check done: `RecordDrawProv`/`RecordDrawLog` are the sole sinks
  at DrawMesh's tail; `SubmitDraw` is the single indexed-draw path; MeshGpuCache/RB3MeshCache are
  vertex caches (no DrawIndexed). Runtime-confirmed at M1: 306/306 skinned draws → rectKind:3, 0 sphere.
- **B5 (MED-LOW)** — G3 queries a coherent (mitten no-op) hand frame or accepts wrist-level naming;
  noted in the gate evidence so a mitten-triggered frame isn't misread as instrument failure.
- **B6 (LOW)** — per-bone parent endpoint is included only if the parent is itself one of this mesh's
  palette bones (`members[]` pointer set); otherwise the sub-rect degenerates to the bone point.
- **B7 (LOW)** — only the sphere `else if (mesh)` is guarded with `kind != 3` (the vv exact-vert path
  never runs for skinned). `boneRects` injected BEFORE the prov object's closing brace. Sketch typos
  fixed; behind-camera bones are skipped (not forced to kind=2). `RB3ProvBoneRect` is top-level.
- **B8 (LOW)** — M2 adds a batch_objdiff on `system/char/Character` pre/post as committed evidence.

No conflict with WAVE19_REVIEW A1 (its region re-scope + step-0 cache check are folded in via B1/B4).

## Milestones
- **M0** — DONE. `evidence/M0-audit.md`. Static: single sink, no skinned bypass, contract makes
  projection unconditionally identity, sFallbackBones = the bind-render set.
- **M1** — DONE (GREEN). `evidence/M1-coverage.md` + `evidence/green-skinned-prov-frame1873.json`.
  306/306 skinned → rectKind:3 positioned bboxes naming real bones; boneFallback path exercised (6 draws).
  G4 flag-OFF golden 792 canonical PASS + fail-red reads RED.
- **M2** — DONE (GREEN). `evidence/M2-owner-scope.md`. owner=player0-3 + named crowd (band+3D
  crowd via Crowd.cpp:574 dispatch), distinct per member. B8: `Character.o` byte-identical
  (HX_NATIVE native-only; DrawShowing 98.2% pre-existing register-swap residual, unchanged).
- **M3** — DONE (GREEN). `evidence/M3-coverage.md` + `M3-uidump_query-roi.txt`. uidump_query.py
  --roi names mesh/owner/bones/mat/pass; coverage N=304 rectKind:3, M=0 sphere-fallback.
- **M4** — DONE (ALL GREEN). `evidence/M4-gates.md` + `evidence/gates/`. G1 known-answer contrast
  (GREEN names 106 mesh+bone in band ROI; RED sphere mislocates → 0 — vindicates B3), G2 disjoint
  corner (0 leaked owners), G3 hand ROI names hands/gloves + finger bones, G4 golden 792 canonical
  PASS. classjson row `RB3_PROV_SKIN_SPHERE` appended (E6, no regen).
- **M5** — DONE (triage only, no fix). `evidence/M5-forearm-float-triage.md` + query JSON + frame
  crop. NAMED: FOREARM-FLOAT = player3's right forearm/hand (`gloves_resource.mesh` +
  `clearcoat_resource.mesh` sleeve; bones `bone_R-foreArm`/`bone_R-foreTwist1/2`/`bone_R-hand`),
  floating above the band heads. boneFallback=0 → a POSE-placement float, distinct from the
  finger-level mitten/clamp class (actionable lead for a future fix charter).

## Flags (both default-OFF; no default flips)
| Flag | Default | Role |
|---|---|---|
| `RB3_DRAWLOG_PROV` | OFF (existing) | master gate; T2 rides it (rectKind=3 + boneRects + boneFallback + owner scope) |
| `RB3_PROV_SKIN_SPHERE` | OFF (NEW) | A/B control: force legacy sphere for skinned (G1 RED baseline) |

## Hazard discipline
Never staged: engine `src/platform/FxSendNative.cpp`; rb3 `native/src/rb3_session_trace.cpp`.
