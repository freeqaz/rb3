# RESOLUTION — "characters without faces" (2026-07-01)

**Landed:** rb3 `372baf7b` — `fix(native): composite band-character flesh-skin
textures`. HX_NATIVE-only, Wii path byte-identical, opt-out `RB3_SKIN_FIX_OFF=1`.

## What it actually was (supersedes ROOT_CAUSE.md's first premise)
ROOT_CAUSE.md initially guessed "the RTT is a no-op at every layer / outputs stay
black." That was **stale** — the native WebGPU RTT + `DrawRect` + pre-clear
composite already ships and is default-on (it correctly composites clothing +
eyes). The real bug was **flesh-skin-specific** and had two native-only causes on
the runtime `OutfitConfig::SetSkinTextures(dir1,dir2,desc)` path:

1. **Never re-dirtied.** The 3-arg runtime caller (`BandCharacter`, `sym==skin`)
   wired skin.cfg's MatSwap sources but — unlike the no-arg `SetSkinTextures()` —
   never `cfg->Recompose()`d, so `DrawPreClear` saw `unk38==0` and skipped
   `MatSwap::Compose`.
2. **Material-identity split.** The loop binds the output RT onto **dir1's**
   `*_naked` material (the instance the skin MESH samples), but skin.cfg's MatSwaps
   hold a **distinct copy** of the same-named material with a `dummy_*`/null diffuse
   (the native milo merge doesn't unify them as the Wii build does). `Compose`
   renders into `mMat->GetDiffuseTex()` = the dummy, which isn't a `kRenderedNoZ`
   RT → the no-op branch → nothing painted.

**Proof:** `RB3_RENDER_DBG` — before, no `RTT created ... head/torso/legs_skin_
diffuse_output`; after the fix, all three composite. The `MatSwap::Compose` probe
showed the skin mats seeing `dummy_torso.tex` (type 0x1) / null instead of the RT.

## Key measurements that reframed the whole diagnosis
- `BONE_PROBE`: `skinDet=1.0`, orthonormal skinRot → **skinning basis is correct**;
  the long-assumed "C8 pose-basis → dark normals" theory is **refuted** for faces.
- Band faces render geometrically (not missing meshes); the flesh is untextured.
- Web reaches the same state (shared engine) — not web-specific.

## Ground truth (Dolphin/Wii — dolphin-shots/)
Real RB3 gameplay faces are **dark, directionally shaded, silhouette/rim-lit**;
eyes are **recessed and do NOT glow** (`face_singer_rimlit.png`). On the menu shell
(soft light) faces show normal cool-key directional shading. So a dark gameplay
face is partly **intended** — the port's separable defects are:

## Remaining follow-ups (separate from the landed fix)
1. **Over-bright / flat face shading** — the port lights skin flat/too-bright vs the
   Wii's directional key + shadow. A faithful match darkens skin toward the scene
   key. (Lighting, not texture.)
2. **Glowing eyes** — the eye material self-illuminates (emissive/spec) where the
   real eyes stay recessed. Separate eye-material issue (composite agent + Dolphin
   both flagged it).
3. **Normal/wrinkle maps** — `RndTexBlender::DrawShowing()` is an empty stub in RB3
   (`src/system/rndobj/TexBlender.cpp:64`), so `norm_output.tex` /
   `head_wrinkle_output.tex` never paint → head samples the flat-normal fallback (no
   surface detail). DC3 has the full impl (`dc3-decomp/.../TexBlender.cpp:117+`) to
   port, reusing the existing RTT/DrawRect hook. Medium effort.

## Oracle notes
- Dolphin (Wii): easy, reproducible boot-to-gameplay-band; lighting-intent oracle
  (renders Wii assets, native loads Xbox). Recipe in t2-dolphin-oracle.md.
- Xbox: rb3-xenon is not runnable; local Xenia is a DC3 fork that faults on RB3.
  Use Dolphin. (t3-xbox-oracle.md)
