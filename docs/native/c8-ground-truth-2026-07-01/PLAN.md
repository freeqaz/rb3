# C8 pose-basis / dark-face — ground-truth campaign (2026-07-01)

**Symptom (user report):** band characters render **without faces** on web (and
identically in native). Face SKIN (`head.mesh`) renders **dark/black**; the rigid
eyes + teeth (`NumBones()==0`) glow bright on top; body/outfit render lit & dressed.
Thin geometry (hair/gloves/nails) shards. This is the long-deferred **C8** issue.

## What is already established (do not re-derive)
- **Not web-specific.** Reproduces in `rb3-native` (shared `src/` + engine). Debug in native.
- rb3-native compiles **rb3's own matched** `src/system/char/*.cpp` (pose decode = Wii-correct algorithm). Confirmed via build `.o` paths.
- Assets loaded natively are **Xbox `.milo_xbox`** (big-endian, padded) — read byte-correctly per the audited `CharBonesSamples::LoadData` HX_NATIVE branch. ⇒ **Xenia (Xbox 360) is the faithful oracle**, Dolphin (Wii) is a secondary oracle (shared engine ⇒ same intended pose, but different asset bytes/skeleton).
- The render-polish-2026-06-11 campaign fixed the worst (bodies no longer vanish; "band stands dressed"), but deferred the deep fix.
- Prior audit clue (CharBonesSamples.cpp:574-583, "V38"): residual char artifacts traced to a **bind-offset-vs-skeleton SCALE mismatch — skin det 0.53, no SCALE channel in the clip** + face-servo posing on tiny-bind meshes.

## The mechanism (this session's diagnosis)
Skinned vertex shader (`milo-native-engine/src/gfx/standard_wgsl.inc`, `fn vs_skinned`):
```wgsl
blendedNormal += influenceWeight * (boneMatrix * vec4f(vertex.normal, 0.0));
...
let worldNormal = normalize((object.worldInvTranspose * vec4f(blendedNormal.xyz,0.0)).xyz);
```
The normal is transformed by the **raw** skin matrix `boneMatrix = BoneOffsetAt(b)*boneWorld`. That is correct **only if the skin matrix is rigid (det≈1)**. With the audited **det 0.53** (non-uniform scale/shear), the normal direction is wrong ⇒ face fails to catch light ⇒ **dark**. Positions survive on compact geometry (head keeps shape); thin geometry shards (R·sinθ).

### Two competing root causes (ground truth disambiguates)
- **H-A — scale is a NATIVE composition bug.** Skin matrix *should* be ~rigid; something in `BoneOffsetAt` baking or `boneWorld` composition (LP64 `Transform`/`Multiply`, or the bind-pose capture) leaks a 0.53 scale. Fix upstream; shader is fine.
- **H-B — scale is REAL.** Character legitimately has a non-uniform bind scale (det 0.53 correct). Then the shader MUST transform normals with the skin matrix's **inverse-transpose** (per-bone normal matrix), not the raw matrix.

### Free oracle (no emulator): the bind-pose invariant
At bind/rest pose `skin = BoneOffsetAt(b)*boneWorld = inverse(bindWorld)*bindWorld = identity` ⇒ **det must be 1.0**. If native reports **det 0.53 at rest**, H-A is proven with zero emulator work.

## Measurement tools (already in-tree)
- Engine `BONE_PROBE` (Rnd_Wgpu_RB3.cpp:4266+): fires once for the first skinned mesh matching `BONE_PROBE_NAME` (substr) with `NumBones()>=8`, at/after `BONE_PROBE_MINFRAME`. Dumps per bone: `worldRot`, `offDet`, `offPos`, `skinPos`, `skinDet`, `skinRot`, `offRot`. Comment: "At bind pose this should be ~identity (det 1)."
  - Drive: `BONE_PROBE=1 BONE_PROBE_NAME=head BONE_PROBE_MINFRAME=<f>`
- Harness: `scripts/native/band-closeup-capture.py` (pins a band closeup so the head mesh is drawn; inherits `os.environ`).
- A/B lever: `RB3_NO_HEAD_REBIND=1` (whole member vanishes ⇒ head-rebind is load-bearing).

## Tracks
- **T1 (native measurement, orchestrator):** run BONE_PROBE on a member's head at rest vs animating; record skinDet + skinRot + offRot. Decide H-A vs H-B. → `t1-native-skindet.md`
- **T2 (Dolphin oracle feasibility):** can `dolphin/build/Binaries/dolphin-emu-nogui` boot the Wii RB3 wbfs headless, and what mechanism reads a bone WorldXfm/skin matrix from game memory at a frame? → `t2-dolphin-oracle.md`
- **T3 (Xenia / rb3-xenon oracle feasibility):** native loads Xbox assets ⇒ Xbox is the faithful oracle. Is `../xenia` buildable quickly? Does `../rb3-xenon` (XenonRecomp) boot far enough to instrument the same skin composition with the SAME Xbox assets? → `t3-xbox-oracle.md`
- **T4 (converge & fix):** pick fix per H-A/H-B, land engine-side (DC3-safe gating), bump pin, verify faces lit. Requires T1 (+ oracle confirm if ambiguous).

## Constraints
- Engine fixes → `../milo-native-engine` commit first, then bump `MILO_ENGINE_PIN` in a matching rb3 commit. DC3-safe: gate RB3-only behavior behind a flag reusing a pad slot / `MILO_ENGINE_GPU_BACKEND STREQUAL rb3`, default byte-identical.
- HX_NATIVE `#else` must stay byte-identical (Wii decomp match). Stage only own files. No git revert/checkout/stash in main repo.
