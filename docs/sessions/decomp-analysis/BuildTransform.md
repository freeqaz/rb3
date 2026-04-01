# BuildTransform Analysis: CamShotFrame::BuildTransform(RndCam*, Transform&, bool) const

**Symbol:** `BuildTransform__12CamShotFrameCFP6RndCamR9Transformb`
**Unit:** `world/CameraShot`
**Current Match:** 3.6% (2008 bytes)
**Status:** Stub only (single MILO_ASSERT)

---

## Function Purpose

`CamShotFrame::BuildTransform()` constructs the camera transformation matrix for a given keyframe. It is the core function that:

1. **Computes target position** — Gets the world position of the camera's target(s)
2. **Applies screen-space offset** — Adjusts camera aim based on normalized screen offset coordinates
3. **Handles smooth filtering** — Interpolates between current and last target position using a configurable filter distance
4. **Incorporates parent transforms** — If a parent object is specified, applies its transform/rotation
5. **Applies camera path animation** — If `mCamShot->mPath` is set, blends the path-based transform with the keyframe offset
6. **Processes dynamic offsets** — Applies per-shot configuration for offset adjustments (pre/post look-at)
7. **Updates persistent state** — Caches `mLastTargetPos` for frame-to-frame smoothing

The function is called twice per interpolation step (once for each keyframe being blended) and its output is used to compute the final interpolated camera position and rotation.

---

## DC3 Reference Implementation

**Location:** `/home/free/code/milohax/dc3-decomp/src/system/world/CameraShot.cpp:354–473`

```cpp
void CamShotFrame::BuildTransform(RndCam *cam, Transform &tf, bool b3) const {
    CamShotFrame *me = const_cast<CamShotFrame *>(this);

    Vector3 targetPos;
    GetCurrentTargetPosition(targetPos);

#ifdef HX_NATIVE
    // Guard: if targetPos is zero (no valid targets) or camera projection
    // hasn't been initialized, WorldToScreen + subsequent math produces NaN/inf.
    // Skip target-dependent filtering and use raw offset.
    if (targetPos.x == 0.0f && targetPos.y == 0.0f && targetPos.z == 0.0f
        && mTargets.empty()) {
        me->mLastTargetPos = targetPos;
        tf = mWorldOffset;
        Multiply(tf, mCamShot->WorldXfm(), tf);
        return;
    }
#endif

    Vector2 screenPos;
    cam->WorldToScreen(targetPos, screenPos);

#ifdef HX_NATIVE
    // Guard: WorldToScreen may produce NaN when projection is degenerate
    if (screenPos.x != screenPos.x || screenPos.y != screenPos.y) {
        screenPos.Set(0.0f, 0.0f);
    }
#endif

    screenPos.x = -((mScreenOffset.x + 1.0f) * 0.5f - screenPos.x);
    screenPos.y = -((1.0f - mScreenOffset.y) * 0.5f - screenPos.y);

    float dist = std::sqrt(screenPos.y * screenPos.y + screenPos.x * screenPos.x);
    dist = (1.0f - dist) < 0.0f ? 1.0f : dist;
    float filterDist = dist * mCamShot->mFilter;

    if (mLastTargetPos.x == kHugeFloat) {
        filterDist = 0.0f;
    } else {
        float dt = TheTaskMgr.DeltaSeconds();
        if (dt == 0.0f) {
            filterDist = 1e-11f;
        }
        if (filterDist != 0.0f) {
            ::Interp(mLastTargetPos, targetPos, filterDist, targetPos);
        }
    }
    me->mLastTargetPos = targetPos;

    MILO_ASSERT(mLastTargetPos.x != kHugeFloat, 0x7ce);

    if (mCamShot->mPath) {
        float pathFrame = mCamShot->mPathFrame;
        if (pathFrame < 0.0f) {
            if (0.0f < mCamShot->mDuration) {
                pathFrame = mCamShot->GetFrame() / mCamShot->mDuration;
            } else {
                pathFrame = 0.0f;
            }
        }
        RndTransAnim *path = mCamShot->mPath;
        float endFrame = path->EndFrame();
        path->MakeTransform(endFrame * pathFrame, tf, true, 1.0f);
        Multiply(mCamShot->mKeyframes[0].mWorldOffset, tf, tf);
    } else {
        tf = mWorldOffset;
    }

    RndTransformable *parent = mParent;
    if (parent) {
        bool useLiveParent;
        if (!mParentFirstFrame || mCamShot->mShotStarted) {
            useLiveParent = true;
        } else {
            useLiveParent = false;
        }

        const Transform *parentXfm;
        if (useLiveParent) {
            parentXfm = &parent->WorldXfm();
        } else {
            parentXfm = &mTargetXfm;
        }

        Transform localParent;
        localParent = *parentXfm;

        if (useLiveParent) {
            if (mCamShot->mFilter != 0.0f) {
                ::Interp(me->mTargetXfm.m, localParent.m, filterDist, localParent.m);
                ::Interp(me->mTargetXfm.v, localParent.v, filterDist, localParent.v);
            }
            me->mTargetXfm = localParent;
        }

        if (mUseParentRotation) {
            Multiply(tf, localParent, tf);
        } else {
            Add(tf.v, localParent.v, tf.v);
        }

        if (0.0f < mCamShot->mClampHeight && mTargets.size() == 1) {
            RndTransformable *target = mTargets.front();
            if (target) {
                float clampZ = mCamShot->mClampHeight + target->WorldXfm().v.z;
                if (clampZ > tf.v.z) {
                    tf.v.z = clampZ;
                }
            }
        }
    }

    Multiply(tf, mCamShot->WorldXfm(), tf);

    mCamShot->ApplyDynamicOffsetPreLookAt(tf, HasTargets());
    if (b3) {
        ApplyScreenOffset(tf, cam);
    }
    mCamShot->ApplyDynamicOffsetPostLookAt(tf);
}
```

---

## RB3-Specific Adaptations Needed

### Member Name / Struct Differences

The RB3 `CamShotFrame` class has these key members (from header analysis):

| DC3 | RB3 | Notes |
|-----|-----|-------|
| `mWorldOffset` | `mWorldOffset` | ✓ Same (TransformNoScale) |
| `mScreenOffset` | `mScreenOffset` | ✓ Same (Vector2) |
| `mParent` | `mParent` | ✓ Same (ObjPtr<RndTransformable>) |
| `mParentFirstFrame` | `mParentFirstFrame` | ✓ Same (bitfield, 0x8b & 1) |
| `mUseParentRotation` | `mUseParentNotation` | ⚠️ **RENAMED** (bitfield, 0x8b >> 1 & 1) |
| `mTargetXfm` | `unk44` | ✓ Both TransformNoScale; RB3 name is "unk44" |
| `mLastTargetPos` | `mLastTargetPos` | ✓ Same (Vector3) |
| `mFocalTarget` | `mFocusTarget` | ✓ Renamed but semantically same |
| `mCamShot->mPath` | `mCamShot->mPath` | ✓ Same |
| `mCamShot->mPathFrame` | `mCamShot->mPathFrame` | ✓ Same |
| `mCamShot->mDuration` | `mCamShot->mDuration` | ✓ Same |
| `mCamShot->mFilter` | `mCamShot->mFilter` | ✓ Same |
| `mCamShot->WorldXfm()` | `mCamShot->WorldXfm()` | ✓ Same method |

### Critical Issues

1. **`mUseParentRotation` → `mUseParentNotation`**: The RB3 member is named `mUseParentNotation` but is in the same bitfield location (0x8b, bit 1). This is likely a copy-paste error in the header. Use `mUseParentNotation` in the RB3 implementation.

2. **`mTargetXfm` → `unk44`**: RB3 has an anonymous member at offset 0x44 that should be `mTargetXfm`. The m2c decompilation shows this as a `TransformNoScale` at the correct offset. RB3 calls it `unk44`.

3. **No `ApplyScreenOffset()` or `ApplyDynamicOffset*()`**: These member functions exist in DC3 but are not yet decompiled in RB3. For now, skip these calls or check if RB3's version has simpler/different logic.

---

## Proposed Implementation Strategy

### Phase 1: Core Transform Building

```cpp
void CamShotFrame::BuildTransform(RndCam *cam, Transform &tf, bool b3) const {
    CamShotFrame *me = const_cast<CamShotFrame *>(this);

    Vector3 targetPos;
    GetCurrentTargetPosition(targetPos);

    Vector2 screenPos;
    cam->WorldToScreen(targetPos, screenPos);

    // Normalize screen offset: convert from [-1, 1] to screen space
    screenPos.x = -((mScreenOffset.x + 1.0f) * 0.5f - screenPos.x);
    screenPos.y = -((1.0f - mScreenOffset.y) * 0.5f - screenPos.y);

    // Compute euclidean distance in screen space
    float dist = std::sqrt(screenPos.y * screenPos.y + screenPos.x * screenPos.x);
    dist = (1.0f - dist) < 0.0f ? 1.0f : dist;

    // Smoothing filter based on camera's mFilter setting
    float filterDist = dist * mCamShot->mFilter;

    // On first frame (mLastTargetPos uninitialized), don't filter
    if (mLastTargetPos.x == kHugeFloat) {
        filterDist = 0.0f;
    } else {
        float dt = TheTaskMgr.DeltaSeconds();
        if (dt == 0.0f) {
            filterDist = 1e-11f;
        }
        if (filterDist != 0.0f) {
            ::Interp(mLastTargetPos, targetPos, filterDist, targetPos);
        }
    }
    me->mLastTargetPos = targetPos;

    // Start with world offset from keyframe
    if (mCamShot->mPath) {
        // Use camera path animation if configured
        float pathFrame = mCamShot->mPathFrame;
        if (pathFrame < 0.0f) {
            if (0.0f < mCamShot->mDuration) {
                pathFrame = mCamShot->GetFrame() / mCamShot->mDuration;
            } else {
                pathFrame = 0.0f;
            }
        }
        RndTransAnim *path = mCamShot->mPath;
        float endFrame = path->EndFrame();
        path->MakeTransform(endFrame * pathFrame, tf, true, 1.0f);
        Multiply(mCamShot->mKeyframes[0].mWorldOffset, tf, tf);
    } else {
        tf = mWorldOffset;
    }

    // Apply parent transform if specified
    RndTransformable *parent = mParent;
    if (parent) {
        bool useLiveParent;
        if (!mParentFirstFrame || mCamShot->mShotStarted) {
            useLiveParent = true;
        } else {
            useLiveParent = false;
        }

        const Transform *parentXfm;
        if (useLiveParent) {
            parentXfm = &parent->WorldXfm();
        } else {
            parentXfm = &unk44;  // unk44 stores the cached parent transform
        }

        Transform localParent;
        localParent = *parentXfm;

        if (useLiveParent) {
            if (mCamShot->mFilter != 0.0f) {
                ::Interp(me->unk44.m, localParent.m, filterDist, localParent.m);
                ::Interp(me->unk44.v, localParent.v, filterDist, localParent.v);
            }
            me->unk44 = localParent;
        }

        if (mUseParentNotation) {  // RB3 uses mUseParentNotation not mUseParentRotation
            Multiply(tf, localParent, tf);
        } else {
            Add(tf.v, localParent.v, tf.v);
        }

        if (0.0f < mCamShot->mClampHeight && mTargets.size() == 1) {
            RndTransformable *target = mTargets.front();
            if (target) {
                float clampZ = mCamShot->mClampHeight + target->WorldXfm().v.z;
                if (clampZ > tf.v.z) {
                    tf.v.z = clampZ;
                }
            }
        }
    }

    Multiply(tf, mCamShot->WorldXfm(), tf);

    // TODO: RB3 may have different dynamic offset functions or none at all
    // mCamShot->ApplyDynamicOffsetPreLookAt(tf, HasTargets());

    // if (b3) {
    //     ApplyScreenOffset(tf, cam);  // TODO: locate or port this function
    // }
    //
    // mCamShot->ApplyDynamicOffsetPostLookAt(tf);

    MILO_ASSERT(mLastTargetPos.x != kHugeFloat, 0x855);
}
```

### Phase 2: TODO Items

1. **Locate `ApplyScreenOffset()`** in RB3's `CamShot` class. If it doesn't exist, determine if the screen offset logic is embedded differently or if it's not needed.
2. **Locate `ApplyDynamicOffsetPreLookAt()` and `ApplyDynamicOffsetPostLookAt()`** in RB3's `CamShot`. These are methods called in DC3 to apply per-shot tweaks. RB3 may have different or no equivalent.
3. **Verify `mCamShot->mShotStarted`** field exists in RB3's `CamShot`. Used to decide whether to use cached or live parent transform on the first frame.
4. **Handle NaN guards**: DC3 has `#ifdef HX_NATIVE` guards around NaN checks for `WorldToScreen()` results. RB3 may not need these or may need different guards.

---

## Uncertainties & Questions

1. **Member Name: `mUseParentNotation` vs `mUseParentRotation`**
   - The RB3 header calls it `mUseParentNotation` which seems like a typo.
   - Semantic meaning: "use parent's rotation when transforming the camera frame."
   - **Decision:** Use `mUseParentNotation` to match RB3 header, but document that this may be a rename.

2. **Missing `unk44` member interpretation:**
   - In RB3's m2c output, offset 0x44 is shown as part of `TransformNoScale unk44`.
   - In the header (line 75), it's listed as `TransformNoScale unk44;`.
   - In DC3, this is `mTargetXfm` — the cached parent transform for smooth interpolation.
   - **Decision:** Use `unk44` to access this field; it serves the same purpose as DC3's `mTargetXfm`.

3. **`ApplyScreenOffset()` presence in RB3:**
   - Not found in grepped results from RB3.
   - May be inlined, removed, or renamed.
   - **Action:** Search `CamShot` class methods or grep for "ScreenOffset" across all RB3 files.

4. **Assertion line number (0x855 vs 0x7ce):**
   - DC3 uses `0x7ce` for its assertion.
   - RB3 stub uses `0x855`.
   - These are different source line numbers (DC3 and RB3 have slightly different file layouts).
   - **Decision:** Keep RB3's `0x855` to match the stub line.

5. **NaN/Inf Handling:**
   - DC3 has conditional guards (`#ifdef HX_NATIVE`) around `WorldToScreen()` result checks.
   - RB3 may or may not need these.
   - **Decision:** Omit guards initially; add if testing reveals NaN-related crashes.

---

## Register & Stack Pressure

The function signature is:
```cpp
void BuildTransform(RndCam *cam, Transform &tf, bool b3) const
```

- **Callee-save regs needed:** r26–r31 (for non-trivial local state tracking)
- **Spill slots:** ~0xf0 bytes (from objdiff: stack frame is `-0xf0` vs base `-0x10`)
- **Key locals:** `targetPos` (Vector3), `screenPos` (Vector2), `filterDist`, `pathFrame`, various interpolation temps

The high register pressure and large stack frame suggest:
- Heavy Vector3/Matrix3 operations → f28–f31 FPU regs
- Multiple Transform temp copies → r26–r29 for pointers
- Deep nesting of Multiply/Interp calls

---

## Next Steps

1. **Collect the full implementation above** into a working C++ function.
2. **Search for `ApplyScreenOffset`, `ApplyDynamicOffsetPreLookAt`, `ApplyDynamicOffsetPostLookAt`** in RB3's codebase; if missing, comment them out and track as a TODO.
3. **Verify `mCamShot->mShotStarted`** field in RB3's `CamShot` class.
4. **Test with `/permute`** to find optimal instruction ordering, especially around register allocation in the parent transform block.
5. **Use `/compare-asm`** to diagnose specific instruction mismatches once implementation is complete.

---

## References

- **DC3 Source:** `/home/free/code/milohax/dc3-decomp/src/system/world/CameraShot.cpp:354–473`
- **RB3 Header:** `/home/free/code/milohax/rb3/src/system/world/CameraShot.h`
- **RB3 Source (stub):** `/home/free/code/milohax/rb3/src/system/world/CameraShot.cpp:1185–1187`
- **Calling context:** `/home/free/code/milohax/rb3/src/system/world/CameraShot.cpp:1046` (in `Interp()`)
