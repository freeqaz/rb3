# CROWD/EXTRAS CHARACTER "SLIVER" — root-cause diagnosis (read-only)

**Authored:** 2026-05-28 (Opus read-only diagnosis subagent — NO build, NO game
run, NO source edit; this is the only file written).
**Builds on:** `VENUE_RENDER.md` V21 (skinned-mesh bring-up), V24 (engine
extent-ratio guard), V26 (`MakeRotQuat` half-angle — killed the dominant shards),
V32 (crowd-sliver investigation — proved the residual is **NOT** `CharIKHand`);
`STATUS_AND_NEXT_GOALS.md` N5; `DIVERGENCE_AUDIT.md` Part-II V21/V26 + §4 #4
(CharBonesSamples cached 360 layout).
**Scope:** the residual class of slivers that V24's engine guard + V26's math fix
do NOT eliminate — the small, single-frame stretched-triangle artifacts in
crowd-cinematic frames.

---

## 1. Exact visual characterization of the residual slivers

Frames viewed (the crowd-cinematic / pre-gameplay venue shots — NOT the void
cuts, which are an unrelated camera issue):

- `screenshots/v34-status-review/08_f0500.png` — the dense purple-lit small_club
  crowd shot. Residual slivers visible: (a) a **thin teal/dark blade** crossing the
  mid-upper frame roughly at standing chest/arm height over the crowd (a held-prop
  / instrument-string-class shard), and (b) several **short thin red-orange and
  green streaks among the lower-foreground crowd** — at hand/face height of the
  nearest crowd figures, not full-body explosions.
- `screenshots/v20-characters/01_f0505.png`, `06_f0555.png` — thin elongated dark
  spikes interleaved among the standing crowd silhouettes; they emanate from
  individual crowd figures' upper bodies (head/hand region), reading as a 1-2px-wide
  triangle fan rather than a clean mesh.
- `screenshots/v21-band-players/01_f0570.png` — same: small spikes localized to
  crowd figures' head/hand/face area; the bodies themselves are intact and upright.
- (`v34-status-review/09_f0700.png` is a *void/wall cut* — black frame with two pink
  wall slivers — that is the V36/N2 camera issue, NOT a character sliver. Do not
  conflate it with this bug.)

**Characterization that constrains the cause:** the residuals are SMALL,
LOCALIZED to the head / hand / face region of crowd & extras figures (and the
held-prop blade), TRANSIENT (flicker for a frame), and the rest of the body draws
correctly. They are NOT the V21/V26-era screen-crossing teal blades from the
band/extras upper-arm chain (those are gone). This matches V32's measured residual
exactly:
- `male_extras_eyebrows11.mesh` bind 4.94u → world 207u (ratio **42**)
- `goatee_resource.mesh` bind 5.73u → world 97.6u (ratio **17**)
- `clap.mesh` bind 51.28u → world 700-815u (ratio **14-16**); `lighter`/`fist`
- `male_extra_head01.mesh` bind 19.18u → world 178u (ratio **9.3**)
- `male_crowd_body03.mesh` bind 85.83u → world 760u (ratio **8.9**),
  `female_crowd_body02.mesh` 80.13u → 706u (ratio **8.8**)

Two sub-classes by bind extent: **tiny-bind** features (eyebrows/goatee/extra
heads, bind 5-19u) where any small absolute error becomes a huge *ratio*; and
**occasional whole crowd bodies** (bind 80-86u → 760u, ratio ~8.9) — a real,
larger pose error, not just ratio amplification.

---

## 2. Root-cause hypothesis (mechanism + files + lines)

### 2a. The mechanism is in the matched-fork `CharBones` packed-buffer pose pipeline, NOT IK and NOT engine.

V32 is decisive on what it is **not**: `CharIKHand::Poll` is never called in the
phases that render these slivers (0 `IK_TGT` hits over 12000 frames). So the V26
follow-up plan (a `SyncTransProxies`-style hand-IK target rewire, STATUS N5 item
(a)) **cannot** be the cause of the slivers actually visible. The slivers are
produced by the standard animation→skin pipeline:

```
CharDriverMidi/clip  → CharClip::ScaleAdd/RotateBy            (char/CharClip.cpp:534,548)
  → CharBonesSamples::{RotateBy,ScaleAddSample} reads packed sample blob
                                                  (char/CharBonesSamples.cpp:91,105)
    → CharBones::{ScaleAdd,RotateBy,Blend} packed-buffer math (char/CharBones.cpp)
  → CharBonesMeshes::PoseMeshes() writes each channel back to a bone's mLocalXfm
                                                  (char/CharBonesMeshes.cpp:92)
→ bone WorldXfm composes up the parent chain
→ engine: skin = BoneOffsetAt(b) * boneTrans->WorldXfm()  (Rnd_Wgpu_RB3.cpp:1310)
→ vs_skinned blends the vertex; a mis-posed bone flings its weighted verts
```

The engine side (`Rnd_Wgpu_RB3.cpp` DrawMesh, bone palette, vs_skinned, the 88-byte
unpack) is correct: V24 proved its per-bone and composed-matrix finiteness guards
**never fire** on these — the bad bone WorldXfm is finite and within ±1e5, just
**wrong**. So the corruption is upstream, in the matched-fork pose math, and it is
the same LP64/MWCC-divergence CLASS as V21 (`Mtx.h:639` empty-body `Multiply`) and
V26 (`Rot.cpp:484` `MakeRotQuat` dropped half-angle) — a math/format primitive that
silently produces a finite-but-wrong result under clang-LP64 where MWCC/PPC was
correct.

### 2b. PRIMARY suspect — `CharBonesSamples::LoadData` cached-360 padding desync for the SCALE channel (and the per-sample 16-byte realign), `char/CharBonesSamples.cpp:554-640`.

This is the highest-probability site because it is the one place where a per-bone
*stride/offset* assumption differs between the 360 cached asset layout and the
in-memory layout, and it is the documented (DIVERGENCE §4 #4) format divergence.

The HX_NATIVE `cached` branch (CharBonesSamples.cpp:569-639) consumes the Xbox
padded on-disk layout. Two concerns, in priority order:

1. **The POS-padding loop conflates POS and SCALE.** When `mCompression <
   kCompressVects` (uncompressed), the loop at lines 583-588 reads
   `Vector3` + a pad float for every channel from `Start()` up to `QuatOffset()` —
   i.e. it treats BOTH the POS block AND the SCALE block as padded Vector4s. If the
   360 Save path padded ONLY the POS Vector3s (or padded SCALE differently), the
   SCALE channel reads desync by 4 bytes per scale-bone, corrupting every scale
   channel from the first SCALE bone onward. A corrupted per-bone SCALE is *exactly*
   the signature that produces both sub-classes of residual: a tiny-bind face
   feature with a wrong scale explodes to a huge ratio, and a crowd body with a
   wrong scale on one bone flings a limb (ratio 8.9). **Verify against the DC3
   sister `Save`'s `cached` branch** (`dc3-decomp/src/system/char/CharBonesSamples.cpp`
   — its `Save` is the authority on what the 360 padded layout actually is for POS
   vs SCALE vs QUAT) and against the actual byte stream of a `small_club` extras/face
   sample.
2. **The per-sample 16-byte realign (lines 622-631) is computed from `bs.Tell()`
   deltas.** If `BinStream::Tell()` is not byte-exact for the native stream wrapper
   (e.g. a buffered/`Cached()` stream where Tell reports a block position rather
   than a byte cursor), `delta` is wrong and every sample after the first is read at
   the wrong base — corrupting later bones (and later bones tend to be the face /
   extremity bones, matching "head/hand/face" localization). Verify Tell() is
   byte-exact for the cached path.

Why this affects the RESIDUAL specifically and not the band players V21 fixed: the
band PLAYERS' explosion was a translation runaway (V21) and the upper-arm chain was
a quat-scale (V26) — both math, fixed at the source. The crowd/extras face-servo
and held-prop clips are driven by DIFFERENT (smaller, uncompressed or differently-
compressed) sample blobs whose padded-layout read is exercised by this branch; a
stride error there is invisible to the V21/V26 math fixes.

### 2c. SECONDARY suspect — `Hmx::Quat::Set(const Hmx::Matrix3&)`, `Rot.cpp:250`, in the `AcquirePose` BIND path.

`CharBonesMeshes::AcquirePose` (char/CharBonesMeshes.cpp:72-75) computes each
QUAT-channel bone's BIND value with `p->Set((*curMesh)->mLocalXfm.m)` — i.e.
`Hmx::Quat::Set(Matrix3)`. The implementation (Rot.cpp:250-280) is the
trace>0 / largest-diagonal branch form. It looks correct AND ends with
`Normalize`, but it is a hand-decompiled non-trivial branch: if the off-diagonal
sign convention is transposed for one branch, the BIND quat is the conjugate, and
EVERY subsequent `PoseMeshes` (which does `Normalize(*p); MakeRotMatrix(*p, …)`)
produces a mirrored/rotated bind for that bone — a finite-but-wrong pose. This is
lower-probability than 2b because it would affect band players too (they share the
path) and V21/V26 verified the players are clean — but it is worth a 5-minute diff
against `dc3-decomp/src/system/math/Rot.cpp`'s `Quat::Set(Matrix3)` to rule out a
sign transposition that only matters for the specific bind rotations of face/prop
bones.

### 2d. Why a wrong bone is amplified into a *sliver* rather than a wrong-but-plausible pose

The engine guard (Rnd_Wgpu_RB3.cpp:1304-1337) only rejects bones whose composed
skin matrix is non-finite or |element|>1e5. A scale-corrupted or conjugated bind
bone produces a skin matrix with, say, a 5-15x scale or a 90-180° rotation error —
finite, <1e5, so it passes the guard, and the verts weighted to it stretch into a
thin triangle. Tiny-bind meshes (one small mesh weighted ~entirely to one face
bone) therefore become a single long spike (ratio 42); a crowd body with one
corrupted bone flings the limb weighted to it (ratio 8.9).

---

## 3. Why V26 fixed the dominant case but NOT this residual

V26 fixed `MakeRotQuat` (the `sqrt(2)` quat fed un-normalized into `MakeRotMatrix`),
which is consumed by the **IK / twist** paths (`CharIKHand::IKElbow`,
`CharForeTwist`, `CharUpperTwist`, `CharLookAt`, `CharIKFingers`). Those drive the
band/extras **upper-arm → hand → finger** chain — the dominant screen-crossing teal
blades. With V26, the upper-arm local determinant is back to 1.000 and those blades
are gone (verified in V26's down-highway frames).

The residual slivers come from a DIFFERENT subsystem that V26's `MakeRotQuat` fix
does not touch:
- The crowd/extras **face servo** (`CharFaceServo`, eyebrows/goatee) and the **base
  body / held-prop clips** are posed via `CharClip → CharBonesSamples →
  CharBones::{ScaleAdd,RotateBy,Blend} → PoseMeshes`, which read the packed sample
  blob and write bone locals directly. They do NOT route through `MakeRotQuat` or
  the IK chain.
- V32 confirmed this empirically: `CharIKHand::Poll` is never called in the phases
  that render these slivers, yet the slivers (ratio 8.9-42) are present from frame 1.
  V32 also measured that V21+V26 reduced the DROP count ~50% (744→364 on a 500-frame
  menu run) but did not eliminate it — i.e. they fixed the math-primitive consumers
  but a separate packed-buffer/format issue remains.

So V26 was a correct fix for the dominant IK/twist explosions; the residual is the
non-IK `CharBones`/`CharBonesSamples` pose-buffer path, which is untouched by both
V21 and V26.

---

## 4. Proposed fix (what to change, where, which layer)

The fix is **matched-fork (layer a), additive `#ifdef HX_NATIVE`** — the same shape
as V21/V26 — at the `CharBonesSamples::LoadData` cached padding branch. Do NOT touch
the engine guard (it is a correct safety net) and do NOT pursue the IK-target rewire
(V32 disproved it for this symptom).

**Step 1 — confirm the exact 360 padded layout (do this FIRST, it determines the
fix).** Read `dc3-decomp/src/system/char/CharBonesSamples.cpp`'s `Save` method,
specifically its `cached` branch, to learn authoritatively how the Xbox/PS3 Save
path pads POS, SCALE, QUAT, and ROT, and how it aligns each sample. The native
`LoadData` consume-loop (rb3 `CharBonesSamples.cpp:571-638`) must mirror that
Save byte-for-byte. The likely defect: the uncompressed POS+SCALE loop
(lines 583-588) pads BOTH POS and SCALE as Vector4 when the Save only padded one of
them, or padded the SCALE block with a different stride.

**Step 2 — fix the consume-loop to match.** Split the POS and SCALE handling if
they pad differently (POS runs `Start()`→`ScaleOffset()`, SCALE runs
`ScaleOffset()`→`QuatOffset()`), consuming the correct number of pad bytes for each.
Keep the in-memory `mRawData` UNPADDED (the comment at line 567-568 already commits
to this — `RecomputeSizes` computed the unpadded offsets, and all the readers
`EvaluateChannel`/`ScaleAdd`/`RotateBy`/`PoseMeshes` assume unpadded). The `#else`
(non-HX_NATIVE) branch stays byte-identical to the permuter's matched fork.

**Step 3 — harden the per-sample realign.** If `bs.Tell()` is not byte-exact for the
cached native stream, compute the per-sample padded stride explicitly from the
known padded element sizes (as the DC3 Save does) instead of from `Tell()` deltas.

If Step 1-3 prove the layout is already correct (i.e. 2b is wrong), fall to the
SECONDARY: diff `Hmx::Quat::Set(Matrix3)` (rb3 `Rot.cpp:250`) against
`dc3-decomp/.../Rot.cpp` for an off-diagonal sign transposition and add an
HX_NATIVE corrected body if they differ — same shape as V26.

---

## 5. How the follow-up agent should VERIFY

**Critical prerequisite (V32's blocker):** V32 could NOT reach in-song gameplay
(`game_screen` reached but venue never engaged, mesh count stuck ~50). The follow-up
MUST first confirm the venue/crowd actually renders (`EnterVenue` fires, mesh count
200+) — the crowd preview that renders in the menu/pre-game window is sufficient for
measuring these specific meshes (V32 measured them there at frame 1+), so a full
gameplay reach is NOT strictly required to verify the fix.

Instrumentation (all already in-tree):
- `SHARD_GUARD_OFF=1 SHARD_RATIO_DBG=1` — the decisive metric. Before the fix, the
  V32 baseline shows `male_extras_eyebrows11` ratio 42, `goatee_resource` 17, `clap`
  14-16, `male_crowd_body03` 8.9, `female_crowd_body02` 8.8. After the fix, those
  ratios should collapse toward ~1.0-1.9 (the legitimate-pose band V24 measured) and
  the per-frame `DROP` count should fall well below V32's 364/500-frame baseline.
- `SHARD_BONE_DBG=1` (Rnd_Wgpu_RB3.cpp:1351) — names the OUTLIER bone per mesh.
  Run it on `clap.mesh` / `male_extras_eyebrows11.mesh` / `male_crowd_body03.mesh`
  BEFORE the fix to confirm WHICH bone is mis-posed and whether it is a SCALE-type
  bone (consistent with 2b) — that pins POS-vs-SCALE padding as the culprit.
- Add (HX_NATIVE, env-gated) a one-shot dump in `CharBonesSamples::LoadData` printing
  per-sample `bs.Tell()` before/after + computed `delta` + the first SCALE channel's
  decoded value, for a face/extras clip, to confirm the stream stays byte-aligned.

Reproducer (V32's, which reaches the crowd-preview render where these meshes
explode):
```
SHARD_GUARD_OFF=1 SHARD_RATIO_DBG=1 \
  RB3_GAME=1 MILO_HEADLESS=1 MILO_AUDIO=1 \
  RB3_DATA=$PWD/orig-assets/extracted MILO_MAX_FRAMES=500 \
  RB3_GAME_INPUT="@10:start,@30:confirm,@140:select:pn_quickplay.btn,@220:select:qp_quickplay.btn,@320:down,@350:msg:music_library:select_highlighted_node,@380:track:guitar,@450:msg:overshell:end_override_flow:1:0,@500:nofail" \
  ./native/build-native/rb3-native
```
Capture frames in the crowd-cinematic window (`08_f0500`-class) with
`SHARD_GUARD_OFF=1` (so the guard is not masking the fix) and confirm the head/hand/
face spikes are gone. THEN re-run WITHOUT `SHARD_GUARD_OFF` and confirm the V24 guard
now fires far less (ideally never) — its becoming-redundant is the success signal.

Pre-flight: `grep` that V21 (`Mtx.h:639`) and V26 (`Rot.cpp:484`) HX_NATIVE blocks
are still present (V32 found the permuter had wiped both) — re-apply before testing
or the residual measurement will be contaminated by the dominant explosions
returning.

---

## 6. Confidence + most-likely alternative

**Confidence: MEDIUM (≈60%) that the primary cause is the `CharBonesSamples::LoadData`
cached-360 padding stride (2b)**, specifically a POS-vs-SCALE padding mismatch or a
non-byte-exact per-sample realign. Reasoning for confidence: it is the only place in
the residual's code path that carries a per-bone *stride/offset* assumption tied to
the 360 asset format (the exact class V32 named — "CharServoBone/CharBones LP64
math... wrong stride/offset"); a scale/offset desync explains BOTH residual
sub-classes (tiny-bind amplification AND whole-body ratio-8.9 limb fling); and it is
untouched by the V21/V26 math fixes (explaining why those reduced but didn't
eliminate). The confidence is not higher because I could not run the build to read
the actual byte stream or `SHARD_BONE_DBG` output, and the existing HX_NATIVE branch
already *attempts* to handle the padding — so the defect, if present, is a subtle
remaining stride bug rather than a missing handler.

**Single most-likely ALTERNATIVE if 2b is wrong:** a sign/branch transposition in
`Hmx::Quat::Set(const Hmx::Matrix3&)` (`Rot.cpp:250`, suspect 2c) corrupting the
BIND quat in `CharBonesMeshes::AcquirePose`, OR a wrong element stride in one of the
`CharBones::ScaleAdd`/`RotateBy`/`Blend` compressed-quat/short-quat branches
(CharBones.cpp:438-622, 813-922) where the `sdata += 4` / `sdata += 3` byte strides
and the `(short*)`/`(char*)` casts could read a face/prop bone's compressed channel
at the wrong offset. Both are the same V21/V26 footgun class (a hand-decompiled math
primitive that is finite-but-wrong under clang) and both would be ruled in/out by the
`SHARD_BONE_DBG` outlier-bone identification + a diff against the DC3 sister files.

A lower-probability third possibility worth keeping in mind: the held-prop meshes
(`clap`/`lighter`/`fist`) may attach via a separate prop-bone offset chain (not the
CharBones sample path) — if `SHARD_BONE_DBG` shows the prop's outlier bone is a
prop-attach bone rather than a sampled body/face bone, the prop slivers are a
distinct (smaller) sub-issue from the eyebrows/crowd-body slivers and should be
triaged separately.
