# T2-WORLDROI — M0 flavor + coverage audit (no edits)

Engine HEAD `beb89e5` (verified = pin), dirty = `M src/platform/FxSendNative.cpp` only (disjoint, never staged).
rb3 HEAD `2b2fcb46`, dirty = `M native/src/rb3_session_trace.cpp` only (hazard, never staged).

## B4 (BINDING) — mesh-cache / multi-draw bypass check
- `RecordDrawProv(` call sites: **exactly 1** — `Rnd_Wgpu_RB3.cpp:5630`, at the tail of `BandRnd::DrawMesh`.
- `RecordDrawLog(` call sites: **exactly 1** — `:5621`, same tail.
- `BandRnd::SubmitDraw` (`:2519`) is documented "the single indexed-draw submission path"; `DrawMesh` calls it then records. The only other `DrawIndexed` calls (`:6389`) are `DrawRect`/`DrawParticles` (UI/particle, non-skinned-character).
- `MeshGpuCache.cpp` / `RB3MeshCache.cpp` are vertex-unpack/upload caches (no `DrawIndexed`), not draw paths.
- **Conclusion:** every skinned CHARACTER mesh that produces pixels routes through `DrawMesh`, so the compose loop (:3776) always runs before the sidecar. No skinned bypass. The runtime coverage number (skinned rectKind:3 count vs drawlog `skinned` flag count) is committed at M3 as the empirical confirmation.

## B1 (BINDING) — ctx.world contract; projection is unconditionally identity
- Placement contract is DEFAULT-ON (`kPlacementContractDefaultOn = 1`, :3299). For the general skinned arm, `obj.world = meshWorld` and the palette is rewritten to `skin * inverse(meshWorld)` so `worldPos = skin*v` is invariant (contract doc :3683-3687). Bone `WorldXfm().v` is ALWAYS a world-space point for the general arm regardless of contract flag.
- Therefore the skinned branch ALWAYS projects bone worlds through an IDENTITY world. The plan's "apply ctx.world" contingency is DELETED (B1). The M0 "print one band ctx.world" check is SUPERSEDED — the math no longer depends on ctx.world's value.
- Name-scoped UI arms `scrollbarThumb` (:3257) and `hubBarPlacement` (:3224) render as `placement ∘ skin(v)` while their bone worlds sit near origin (HUB_BAR_PROBE doc :3834-3839). These are EXCLUDED from the bone-world bbox: `skinnedPoseValid = skinned && !scrollbarThumb && !hubBarPlacement`. Excluded draws keep the sphere fallback (rectKind=1 + skinned:true, disclosed).
- Locals `scrollbarThumb`, `hubBarPlacement`, `sFallbackBones` are all at 4-space DrawMesh-body scope; the `RecordDrawProv` call at :5630 is DrawMesh's last statement (function closes :5631) — all in scope, passable as params.

## B2 (BINDING) — clamp renders at bind pose
- `sFallbackBones` (:3375) counts null (:3787) + non-finite (:3829) + bad (:3881) + clamped (:3959) bones — exactly the "renders at bind, not at bone world" set. Passed to `RecordDrawProv` as `boneFallback`.
- When `boneFallback > 0`, the sphere-corner (bind-extent) projection is UNIONED into the rectKind=3 bbox so the rect over-approximates bind-pose-rendered geometry (the FOREARM-FLOAT M5 class).

## Bone APIs confirmed at HEAD
`owner->NumBones()`, `owner->BoneTransAt(b)->{WorldXfm(),Name(),TransParent()}`, `Transform::v.{x,y,z}` (compose loop :3776-3833).
