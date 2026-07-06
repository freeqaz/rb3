#include "bandobj/BandCharacter.h"
#include <cstdlib>
#ifdef HX_NATIVE
#include <cstdio>
#include <cmath>
#include <ctime>
#include <vector>
#include <list>
#include <set>
#include <algorithm>
#include "rndobj/Mesh.h"
#include "rndobj/Dir.h"
#endif
#include "bandobj/BandHeadShaper.h"
#include "bandobj/BandWardrobe.h"
#include "char/CharCollide.h"
#include "char/CharServoBone.h"
#include "char/CharClipDriver.h"
#include "char/CharClipGroup.h"
#include "char/CharFaceServo.h"
#include "char/CharInterest.h"
#include "char/CharMeshCacheMgr.h"
#include "char/CharUtl.h"
#include "math/Rand.h"
#include "math/Rot.h"
#include "obj/Task.h"
#include "obj/Utl.h"
#include "utl/Loader.h"
#include "rndobj/Cam.h"
#include "rndobj/Env.h"
#include "rndobj/Utl.h"
#include "utl/Symbols.h"
#include "utl/Messages.h"

INIT_REVS(BandCharacter)

#ifdef HX_NATIVE
// render-polish 2026-06-11 (char-render): reload-churn probe plumbing.
// gNativeStartLoadTag attributes each BandCharacter::StartLoad to its trigger
// (set by every call site: RecomposePatches / MiloReload / start_load DTA /
// in_closet propsync / CharCache::Request / BandWardrobe paths) so RELOAD_PROBE
// can answer "what reloads the band mid-song?". gNativeBandCharPollSerial is a
// monotonic Poll counter shared by all members, printed by the probes for log
// correlation. Diagnostics only — all output gated on RELOAD_PROBE=1.
const char *gNativeStartLoadTag = "?";
int gNativeBandCharPollSerial = 0;
static bool NativeReloadProbe() {
    static int sOn = -1;
    if (sOn < 0)
        sOn = getenv("RELOAD_PROBE") ? 1 : 0;
    return sOn != 0;
}
// walk-on snap (docs/native/walkon-2026-07-02/SCOUT.md fix #1). Default-ON;
// set RB3_WALKON_SNAP_OFF=1 to leave the on-stage band on whatever the intro
// shot delivers (and frozen on the stale vignette pose if it is late/misses).
static bool WalkonSnapEnabled() {
    static int sOn = -1;
    if (sOn < 0)
        sOn = getenv("RB3_WALKON_SNAP_OFF") ? 0 : 1;
    return sOn != 0;
}
#endif

ObjectDir *sBoneMergeDir;
ObjectDir *sOutfitDir;
ObjectDir *sResourceDir;
ObjectDir *sCharSharedDir;
ObjectDir *sInstrumentDir;
ObjectDir *sInstResourceDir;
ObjectDir *sToDir;

const char *BandIntensityString(int num) {
    if (num != 0) {
        int intensity = num & 0x7F000;
        switch (intensity) {
        case 0x1000:
            return "realtime_idle";
        case 0x2000:
            return "idle";
        case 0x4000:
            return "idle_intense";
        case 0x8000:
            return "play_mellow";
        case 0x10000:
            return "play_normal";
        case 0x20000:
            return "play_intense";
        case 0x40000:
            return "play_solo";
        default:
            MILO_FAIL("Bad intensity %d!", intensity);
            break;
        }
    }
    return "";
}

void BandCharacter::Init() { Register(); }
void BandCharacter::Terminate() {}

BandCharacter::BandCharacter()
    : mPlayFlags(0), unk454(this, 0), mAddDriver(0), mFaceDriver(0), mForceNextGroup(0),
      mForceVertical(1), mOutfitDir(this, 0), mInstDir(this, 0), mTempo("medium"),
      mFileMerger(0), mHeadLookAt(this, 0), mNeckLookAt(this, 0), mEyes(this, 0),
      unk574(0), mTestPrefab(this, 0), mGenre("rocker"), mDrumVenue("small_club"),
      mTestTourEndingVenue(0), mInstrumentType("none"), unk594(this, 0), mInCloset(0),
      unk5a1(0), unk5a2(0), unk5a3(0), mSingalongWeight(this, 0),
      unk5b0(this, kObjListNoNull), unk5c0(this, kObjListNoNull),
      unk5d0(this, kObjListNoNull), unk5e0(this, kObjListNoNull),
      unk5f0(this, kObjListNoNull), unk600(this, kObjListNoNull),
      unk610(this, kObjListNoNull), unk620(this, kObjListNoNull),
      unk630(this, kObjListNoNull), unk640(this, kObjListNoNull),
      unk650(this, kObjListNoNull), unk660(this, kObjListNoNull),
      unk670(this, kObjListNoNull), unk680(this, 0), unk68c(this, 0), unk698(this, 0),
      unk6a4(this, 0), unk6b0(this, 0), mUseMicStandClips(0), unk6bd(1), unk6c0(this, 0),
      mInTourEnding(0), unk6d8(0), unk6ec(0), unk738(0), unk73c(this, kObjListNoNull),
      unk74c(this, kObjListNoNull) {
    mGroupName[0] = 0;
    mOverrideGroup[0] = 0;
    mFaceGroupName[0] = 0;
    mOverlay = RndOverlay::Find("char_status", true);
    unk734 = Hmx::Object::New<Waypoint>();
    unk734->SetRadius(2.0f);
    unk734->SetStrictRadiusDelta(5.0f);
    unk734->SetAngRadius(0.17453292f);
    unk734->SetStrictAngDelta(0.2617994f);
#ifdef HX_NATIVE
    mNativeReboundOnce = 0;
    mNativeReboundQuiet = 0;
    mNativeReboundBody = 0;
    mNativeHeadReboundOnce = 0;
    mNativeHeadReboundQuiet = 0;
    mNativeRestCaptured = false;
    mNativeInstReboundOnce = 0;
    mNativeInstReboundQuiet = 0;
    mNativeHandsRigidOnce = 0;
    mNativeHandsRigidQuiet = 0;
    mNativeHandsConjOnce = 0; // W2.8c per-frame conjugation
    mNativeHandsConjQuiet = 0;
    mNativeSkinnedCacheValid = false; // frame-stall 2026-06-20 (TRACK A)
    mNativeWalkonSnapPending = false; // walk-on snap (SCOUT.md fix #1)
#endif
}

#pragma push
#pragma dont_inline on
BandCharacter::~BandCharacter() {
    TheRnd->CompressTextureCancel(this);
    delete unk734;
}
#pragma pop

void BandCharacter::AddedObject(Hmx::Object *o) {
    Character::AddedObject(o);
    if (streq(o->Name(), "main_add.drv"))
        mAddDriver = dynamic_cast<CharDriver *>(o);
    if (streq(o->Name(), "expression.drv"))
        mFaceDriver = dynamic_cast<CharDriver *>(o);
    else if (streq(o->Name(), "head.lookat"))
        mHeadLookAt = dynamic_cast<CharLookAt *>(o);
    else if (streq(o->Name(), "neck.lookat"))
        mNeckLookAt = dynamic_cast<CharLookAt *>(o);
    else if (streq(o->Name(), "FileMerger.fm"))
        mFileMerger = dynamic_cast<FileMerger *>(o);
    else if (streq(o->Name(), "outfit"))
        mOutfitDir = dynamic_cast<Character *>(o);
    else if (streq(o->Name(), "instrument"))
        mInstDir = dynamic_cast<Character *>(o);
    else if (streq(o->Name(), "CharEyes.eyes"))
        mEyes = dynamic_cast<CharEyes *>(o);
    else if (streq(o->Name(), "singalong.weight"))
        mSingalongWeight = dynamic_cast<CharWeightSetter *>(o);
    else
        AddObject(o);
}

void BandCharacter::RemovingObject(Hmx::Object *o) {
    Character::RemovingObject(o);
    if (o == mAddDriver)
        mAddDriver = 0;
    else if (o == mFileMerger)
        mFileMerger = 0;
}

void BandCharacter::Replace(Hmx::Object *from, Hmx::Object *to) {
    BandCharDesc::Replace(from, to);
    Character::Replace(from, to);
    if (from == mTestPrefab) {
        mTestPrefab = dynamic_cast<BandCharDesc *>(to);
        if (mTestPrefab)
            CopyCharDesc(mTestPrefab);
    }
}

void BandCharacter::Enter() {
    OnRestoreCategories(0);
    mForceVertical = true;
    mForceNextGroup = false;
    unk574 = false;
    unk5a2 = false;
    unk5a3 = false;
    unk594 = 0;
    mGroupName[0] = 0;
    mPlayFlags &= 0x300000;
    mOverrideGroup[0] = 0;
    mFaceGroupName[0] = 0;
    mFrozen = false;
    Character::Enter();
    SetState("", mPlayFlags, 2, false, false);
    SetHeadLookatWeight(0);
    unk6c0 = 0;
    if (mDriver) {
        Message msg("get_matching_dude");
        DataNode handled = HandleType(msg);
        if (handled.Type() == kDataObject) {
            unk6c0 = handled.Obj<BandCharacter>();
            if (unk6c0) {
                unk6c0->unk6c0 = this;
                CharClip *clip = unk6c0->GetDriver()->FirstPlayingClip();
                if (clip)
                    MakeMRU(this, clip);
            }
        }
    }
}

void BandCharacter::Exit() { Character::Exit(); }

bool BandCharacter::InVignetteOrCloset() const {
    Symbol cliptype = mDriver->ClipType();
    bool ret = false;
    if (cliptype == shell || cliptype == vignette)
        ret = true;
    return ret;
}

DECOMP_FORCEACTIVE(BandCharacter, "BandCharacter.no_anim")

CharClipDriver *BandCharacter::PlayMainClip(int i, bool b) {
    static DataNode &noAnim = DataVariable("BandCharacter.no_anim" + 14);
    if (noAnim.Int())
        return 0;
    if (mGroupName[0] == 0 || !unk454)
        return 0;
    else {
        ObjectDir *clipdir = unk454->ClipDir();
        if (!clipdir)
            return 0;
        else {
            CharClipGroup *grp = clipdir->Find<CharClipGroup>(mGroupName, false);
            if (!grp) {
                MILO_NOTIFY_ONCE(
                    "%s could not find group %s in %s\n",
                    PathName(this),
                    mGroupName,
                    PathName(clipdir)
                );
                return 0;
            } else {
                bool invorc = InVignetteOrCloset();
                int mask = mPlayFlags;
                if (invorc) {
                    mask = mGender == "male" ? 0x20 : 0x40;
                } else if (streq(mGroupName, "realtime_idle")) {
                    mask = mask & 0xFFF80FFF | 0x1000;
                }
                CharClip *clp = 0;
                if (mUseMicStandClips
                    || mInstrumentType == keyboard && ((i & 0xF) != 2) && !b) {
                    CharClip *firstclip = unk454->FirstClip();
                    if (firstclip) {
                        if (firstclip->InGroup(grp)) {
                            i = i & 0xfffffff0U | 4;
                            clp = firstclip;
                        }
                    }
                }
                if (!clp)
                    clp = grp->GetClip(mask);
                if (!clp && invorc && mask == 0x40) {
                    MILO_NOTIFY_ONCE(
                        "%s no female vignette clip in %s, using male",
                        PathName(this),
                        PathName(grp)
                    );
                    mask = 0x20;
                    clp = grp->GetClip(0x20);
                }
                if (!clp) {
                    MILO_NOTIFY_ONCE(
                        "%s no clip w. flags %s in %s",
                        PathName(this),
                        FlagString(mask),
                        PathName(grp)
                    );
                    return 0;
                } else {
                    if (invorc)
                        clp->SetFlags(clp->Flags() | 0xF);
                    else {
                        bool hasDriver = AddDriverClipDir();
                        if (hasDriver) {
                            int imask = 1;
                            if ((i & 0xF) == 2)
                                imask = 2;
                            CharDriver *drv;
                            if ((CharDriver *)unk454 == mDriver)
                                drv = mAddDriver;
                            else
                                drv = mDriver;
                            CharClip *stillclip =
                                drv->ClipDir()->Find<CharClip>("still", false);
                            if (stillclip)
                                drv->Play(stillclip, imask, -1.0f, 1e+30f, 0.0f);
                            else
                                MILO_NOTIFY_ONCE(
                                    "%s could not find still clip", PathName(drv)
                                );
                        }
                    }
                    CharClipDriver *played = unk454->Play(clp, i, -1.0f, 1e+30f, 0.0f);
                    if ((i & 0xF) == 2)
                        mTeleported = true;
                    if (played) {
                        MakeMRU(unk6c0, clp);
                    }
                    return played;
                }
            }
        }
    }
}

void BandCharacter::MakeMRU(BandCharacter *bchar, CharClip *clip) {
    MILO_ASSERT(clip, 0x1A1);
    if (bchar && bchar->GetDriver()->ClipDir()) {
        CharClip *clip2 =
            bchar->GetDriver()->ClipDir()->Find<CharClip>(clip->Name(), false);
        if (clip2)
            clip2->MakeMRU();
    }
}

void BandCharacter::PlayFaceClip() {
    if (mFaceDriver) {
        CharClipGroup *grp =
            mFaceDriver->ClipDir()->Find<CharClipGroup>(mFaceGroupName, false);
        if (!grp) {
            MILO_WARN(
                "Could not find CharClipGroup %s in %s\n",
                mFaceGroupName,
                PathName(mDriver->ClipDir())
            );
        } else
            mFaceDriver->Play(grp->GetClip(), 4, -1.0f, 1e+30f, 0.0f);
    }
}

bool BandCharacter::AllowOverride(const char *cc) {
    if (mInstrumentType == "mic") {
        if (!streq(cc, "stand") && !streq(cc, "closeup") && !streq(cc, "extreme_closeup")
            && !streq(cc, "")) {
            return false;
        }
    }
    return true;
}

void BandCharacter::Poll() {
    START_AUTO_TIMER("cc_poll");
#ifdef HX_NATIVE
    gNativeBandCharPollSerial++; // probe log correlation (RELOAD_PROBE)
#endif
    if (unk5a2) {
        Teleport(unk594);
        unk5a2 = false;
    }
    if (unk5a3) {
        const char *name;
        if (mOverrideGroup[0] != 0)
            name = mOverrideGroup;
        else
            name = mInstrumentType == drum ? "sit" : "stand";
        SetState(name, mPlayFlags, 2, false, true);
        unk5a3 = false;
    }
#ifdef HX_NATIVE
    // WALK-ON SNAP retry (docs/native/walkon-2026-07-02/SCOUT.md fix #1).
    // Armed by SetContext("venue"). By venue entry the loading-screen vignette
    // clip has already been cleared, so until a NEW clip plays the driver is
    // empty and Character::Poll stops re-driving the bones — the skeleton
    // freezes on the last (seated/lying) vignette pose. Normally the intro
    // shot's play_group delivers a gameplay idle within a frame or two, but if
    // that delivery is late (body_clips still streaming) or misses this member
    // (per-member intro-shot target miss, SCOUT.md H3), the stale vignette pose
    // is held on the lit stage until the NEXT shot ~7 s later. This retries the
    // member's default stage idle each Poll until a clip really plays — the
    // earliest the clips allow — guaranteeing no on-stage member is left frozen
    // on a vignette pose. Self-clears the instant any clip is live (mine, or the
    // intro shot's own group). HX_NATIVE-only, so the Wii build is byte-identical.
    if (mNativeWalkonSnapPending && WalkonSnapEnabled()) {
        if (!mDriver || mDriver->FirstPlayingClip()) {
            // a clip is live (mine or the intro shot's) -> window closed.
            mNativeWalkonSnapPending = false;
        } else if (mDriver->ClipDir()) {
            mDriver->Clear();
            if (mAddDriver)
                mAddDriver->Clear();
            CharClipDriver *played = SetState(
                mInstrumentType == drum ? "sit" : "stand", mPlayFlags, 2, false, true
            );
            SetTeleported(true);
            if (played)
                mNativeWalkonSnapPending = false;
        }
    }
#endif

    // Eye interest polling - update head/neck lookat targets
    if (mEyes) {
        RndTransformable *interest = mEyes->GetCurrentInterest();
        if (interest) {
            Transform xfm = interest->WorldXfm();
            if (mHeadLookAt) {
                mHeadLookAt->GetDest()->SetWorldXfm(xfm);
            }
            if (mNeckLookAt) {
                mNeckLookAt->GetDest()->SetWorldXfm(xfm);
            }
        }
    }

    // Edit mode starvation handling - clear driver if clip near end
    if (LOADMGR_EDITMODE && unk6d8 < 0.0f && TheTaskMgr.DeltaSeconds() > 0.0f
        && Dir() != this) {
        if (mDriver && mDriver->FirstPlaying()) {
            float startBeat = mDriver->FirstPlaying()->GetClip()->StartBeat();
            float lengthBeats = mDriver->FirstPlaying()->GetClip()->LengthBeats();
            if (mDriver->FirstPlaying()->mBeat < -(0.1f * lengthBeats - startBeat)) {
                mDriver->Clear();
                if (mAddDriver) {
                    mAddDriver->Clear();
                }
            }
        }
    }

    unk6d8 = TheTaskMgr.DeltaSeconds();

    if (!mFrozen) {
        // Force vertical orientation
        if (mForceVertical) {
            if (!(mCache->mFlags & 1)) {
                mCache->SetDirty_Force();
            }
            MakeVertical(mLocalXfm.m);
        }

        // Expression driver handling
        if (unk454) {
            CharClip *clip = unk454->FirstPlayingClip();
            if (clip && (clip->PlayFlags() & 0xF0) == 0x10) {
                mForceNextGroup = true;
            }
            if (unk454->Starved()) {
                PlayMainClip(4, false);
            }
        }

        // Save and force showing state
        bool wasShowing = Showing();
        SetShowing(true);
        if (Showing()) {
            // Update singalong weight
            if (unk6b0) {
                unsigned int showWeight = 0.0f;
                if (wasShowing && mMinLod < 1) {
                    showWeight = 1;
                }
                unk6b0->SetWeight((float)showWeight);
            }

            // Sync outfit character state
            if (mOutfitDir) {
                mOutfitDir->SetTeleported(mTeleported);
                mOutfitDir->mMinLod = mMinLod;
            }

            // Sync instrument character state
            if (mInstDir) {
                mInstDir->SetTeleported(mTeleported);
                mInstDir->mMinLod = mMinLod;
            }

#ifdef HX_NATIVE
            // wave-07 BAND_ANIM_PROBE: trace the per-frame band animation chain to
            // find WHERE the on-stage band skeleton fails to move. Env-gated, default
            // OFF. BAND_ANIM_PROBE=<substr> matches a member by its dir name (or "*").
            // Captures: driver presence, the playing clip, and a named bone's worldPos
            // BEFORE vs AFTER Character::Poll() (the actual skeleton-drive sweep).
            const char *banimEnv = getenv("BAND_ANIM_PROBE");
            bool banim = false;
            RndTransformable *probeBone = nullptr;
            Vector3 bonePre(0, 0, 0);
            if (banimEnv) {
                const char *myName = Name() ? Name() : "?";
                if (banimEnv[0] == '*' || (myName && strstr(myName, banimEnv)))
                    banim = true;
            }
            if (banim) {
                const char *bn = getenv("BAND_ANIM_BONE");
                if (!bn || !bn[0]) bn = "bone_R-upperArm.mesh";
                probeBone = Find<RndTransformable>(bn, false);
                if (!probeBone) probeBone = Find<RndTransformable>("bone_pelvis.mesh", false);
                if (probeBone) bonePre = probeBone->WorldXfm().v;
            }
#endif

#ifdef HX_NATIVE
            // C7/C8: rebind the head/hair/hands/face skin meshes onto the per-member
            // skeleton, baking the EXACT inverse-bind against each bone's REST pose.
            // MUST run BEFORE Character::Poll() so that on the first Poll the
            // per-member skeleton still holds the SetDeformation gender-bind REST pose
            // (Character::Poll below applies the first clip frame). Find here already
            // resolves the LIVE per-member instance (BAND_ANIM_PROBE reads it pre-Poll).
            RebindHeadHandsAtRest();
#endif
            // Poll base character
            Character::Poll();

#ifdef HX_NATIVE
            if (banim) {
                static int frameCt = 0;
                // throttle: print at most every ~30 frames to keep logs readable
                bool emit = (frameCt++ % 30) == 0;
                if (emit) {
                    const char *myName = Name() ? Name() : "?";
                    CharDriver *drv = mDriver;
                    CharClipDriver *fp = drv ? drv->FirstPlaying() : nullptr;
                    CharClip *clip = drv ? drv->FirstPlayingClip() : nullptr;
                    CharDriver *u454 = unk454;
                    CharClip *u454clip = u454 ? u454->FirstPlayingClip() : nullptr;
                    Vector3 bonePost(0, 0, 0);
                    if (probeBone) bonePost = probeBone->WorldXfm().v;
                    float moved = 0.0f;
                    {
                        Vector3 d;
                        Subtract(bonePost, bonePre, d);
                        moved = Length(d);
                    }
                    fprintf(stderr,
                        "[BAND_ANIM] member='%s' grp='%s' mDriver=%p clipType='%s' "
                        "FirstPlaying=%p clip='%s' | unk454=%p u454clip='%s' bones=%p | "
                        "bonePtr=%p "
                        "bone='%s' pre=(%.4f,%.4f,%.4f) post=(%.4f,%.4f,%.4f) moved=%.6f\n",
                        myName, mGroupName[0] ? mGroupName : "(none)", (void *)drv,
                        drv ? drv->ClipType().Str() : "?", (void *)fp,
                        clip ? (clip->Name() ? clip->Name() : "?") : "(none)",
                        (void *)u454,
                        u454clip ? (u454clip->Name() ? u454clip->Name() : "?") : "(none)",
                        drv ? (void *)drv->GetBones() : nullptr,
                        (void *)probeBone,
                        probeBone ? (probeBone->Name() ? probeBone->Name() : "?") : "(null)",
                        bonePre.x, bonePre.y, bonePre.z,
                        bonePost.x, bonePost.y, bonePost.z, moved);
                }
            }

            // wave-08: now that Character::Poll() has posed the per-member skeleton
            // for THIS frame (the animated bones are live), repoint the outfit skin
            // meshes onto them. Runs once per member (mNativeReboundOnce); retries
            // each Poll until the moving instance is reachable. Must come AFTER
            // Character::Poll() (skeleton posed) and BEFORE the outfit meshes draw.
            RebindOutfitBonesToOwnSkeleton();

            // W2.8 BL-A1 (opt-in RB3_HANDS_POSEAWARE=1, default OFF): rigid-anchor the
            // hand/finger/glove skin meshes to their per-side wrist bone so each hand
            // rides its wrist as one rigid body, collapsing the pose-varying
            // multi-bone basis divergence that flings the distal finger verts into
            // sheets (the "missing hands" shard). Runs AFTER the head/hands rest-rebind
            // above so it OVERWRITES that mesh's sharding bind; latched per member.
            // Flag-OFF this is a getenv-cached early return (byte-identical).
            //
            // W2.8c: RB3_HANDS_PERFRAME_CONJ selects the per-frame pose-aware
            // conjugation pass INSTEAD of the rigid-anchor collapse (mutually
            // exclusive — only one touches the hand meshes). Both default-OFF; when
            // both flags are unset this is exactly the pre-W2.8c NativeRepinHandsRigid
            // early-return (byte-identical). See NativeConjHandsPerFrame.
            {
                static int sPerframeConj = -1;
                if (sPerframeConj < 0)
                    sPerframeConj = getenv("RB3_HANDS_PERFRAME_CONJ") ? 1 : 0;
                if (sPerframeConj)
                    NativeConjHandsPerFrame();
                else
                    NativeRepinHandsRigid();
            }
#endif

            // Poll child characters
            if (mOutfitDir) {
                mOutfitDir->Poll();
            }
            if (mInstDir) {
                mInstDir->Poll();
            }
#ifdef HX_NATIVE
            // wave-inststrings: rebind the band instrument's *_strings skin meshes so
            // their world skin re-composes to ~bind (ratio ~1.0). Must run AFTER
            // mInstDir->Poll() so the instrument/neck bones are posed THIS frame, and
            // before the instrument draws. Scope = mInstDir *_strings.mesh whose bones
            // resolve to skeleton_unshared.milo only; FINE instruments (own-resource
            // neck) are never touched. See the method header.
            RebindInstStringsToRestBasis();
#endif
        } else {
            mTeleported = true;
        }
        SetShowing(wasShowing);
    }

    UpdateOverlay();
    CalcBoundingSphere();

    // Check current clip for vignette/mic_body status
    unk574 = false;
    if (mDriver) {
        CharClip *clip = mDriver->FirstPlayingClip();
        if (clip) {
            if (clip->Type() == vignette) {
                unk574 = true;
            }
            if (clip->Type() == mic_body) {
                if (unk680) {
                    unk680->SetShowing(clip->Flags() & 0x8000000);
                }
            }
        }
    }

    // Update mesh visibility based on vignette state
    if (unk68c) {
        unk68c->SetShowing(!unk574);
    }
    if (unk698) {
        unk698->SetShowing(!unk574);
    }
    if (unk6a4) {
        unk6a4->SetShowing(!unk574);
    }
}

void BandCharacter::CalcBoundingSphere() {
    mBounding.Zero();
    Sphere s48(Vector3(0, 0, 5.0f), 45.0f);
    Multiply(s48, mSphereBase->WorldXfm(), s48);
    mBounding.GrowToContain(s48);
    if (mInstDir) {
        Sphere s58;
        mInstDir->MakeWorldSphere(s58, false);
        mBounding.GrowToContain(s58);
    }
    Transform tf38;
    FastInvert(mSphereBase->WorldXfm(), tf38);
    Multiply(mBounding, tf38, s48);
    SetSphere(s48);
}

bool BandCharacter::ValidateInterest(CharInterest *ci, ObjectDir *dir) {
    if (!ci)
        return false;
    if (dir) {
        if (dir == this || ci->Dir() == this) {
            if (ci->CategoryFlags() & 0x200)
                return false;
        }
        const DataNode *prop = dir->Property("lookat_cameras", false);
        if (prop && (ci->CategoryFlags() & 1) && !prop->Int())
            return false;
    }
    return true;
}

bool BandCharacter::SetFocusInterest(CharInterest *ci, int i) {
    if (mEyes)
        return mEyes->SetFocusInterest(ci, i);
    else
        return Character::SetFocusInterest(ci, i);
}

void BandCharacter::SetInterestFilterFlags(int i) {
    if (mEyes)
        mEyes->SetInterestFilterFlags(i);
    else
        Character::SetInterestFilterFlags(i);
}

void BandCharacter::ClearInterestFilterFlags() {
    if (mEyes)
        mEyes->ClearInterestFilterFlags();
    else
        Character::ClearInterestFilterFlags();
}

DataNode BandCharacter::OnToggleInterestDebugOverlay(DataArray *da) {
    if (mEyes)
        mEyes->ToggleInterestsDebugOverlay();
    return DataNode(0);
}

struct FlagPair {
    int flag;
    const char *str;
};

const char *BandCharacter::FlagString(int flags) {
    static FlagPair pairs[7] = {
        { 0x1000, "IR|" }, { 0x2000, "I|" },   { 0x4000, "II|" },  { 0x8000, "PM|" },
        { 0x10000, "P|" }, { 0x20000, "PI|" }, { 0x40000, "PS|" },
    };
    char buf[256];
    buf[0] = 0;
    for (unsigned int i = 0; i < 7; i++) {
        if (flags & pairs[i].flag) {
            strcat(buf, pairs[i].str);
            flags &= ~(pairs[i].flag);
        }
    }
    if (flags != 0 || buf[0] == 0)
        strcat(buf, MakeString("0x%x", flags));
    else
        buf[strlen(buf) - 1] = 0;
    return MakeString(buf);
}

void BandCharacter::UpdateOverlay() {
    if (mOverlay->Showing()) {
        *mOverlay << Name() << "- " << mInstrumentType << ": " << mGroupName << " "
                  << FlagString(mPlayFlags & 0x7F000);
        CharClipDriver *firstplaying = mDriver->FirstPlaying();
        if (firstplaying) {
            if (AddDriverClipDir()) {
                *mOverlay << " " << SafeName(firstplaying->GetClip());
                CharClipDriver *firstaddplaying = mAddDriver->FirstPlaying();
                if (firstaddplaying) {
                    *mOverlay << "/" << SafeName(firstaddplaying->GetClip()) << " "
                              << FlagString(firstaddplaying->GetClip()->Flags() & 0x7F000)
                              << " ";
                    *mOverlay << " "
                              << CharClip::BeatAlignString(firstaddplaying->mPlayFlags);
                    *mOverlay << MakeString(
                        " %.2f %.2f",
                        std::fmod(TheTaskMgr.Beat(), 1.0f),
                        std::fmod(firstaddplaying->mBeat, 1.0f)
                    );
                } else {
                    *mOverlay << " "
                              << FlagString(firstplaying->GetClip()->Flags() & 0x7F000);
                    *mOverlay << " "
                              << CharClip::BeatAlignString(firstplaying->mPlayFlags);
                    *mOverlay << MakeString(
                        " %.2f %.2f",
                        std::fmod(TheTaskMgr.Beat(), 1.0f),
                        std::fmod(firstplaying->mBeat, 1.0f)
                    );
                }
            } else {
                *mOverlay << " " << SafeName(firstplaying->GetClip()) << " "
                          << FlagString(firstplaying->GetClip()->Flags() & 0x7F000);
                *mOverlay << " " << CharClip::BeatAlignString(firstplaying->mPlayFlags);
                *mOverlay << MakeString(
                    " %.2f %.2f",
                    std::fmod(TheTaskMgr.Beat(), 1.0f),
                    std::fmod(firstplaying->mBeat, 1.0f)
                );
            }
        }
        *mOverlay << "\n";
    }
}

void BandCharacter::RemoveDrawAndPoll(Character *c) {
    if (c) {
        c->SyncObjects();
        VectorRemove(mDraws, c);
        VectorRemove(mPolls, c);
    }
}

#ifdef HX_NATIVE
// render-polish 2026-06-11 (char-render): shared skinned-mesh collector (factored
// out of the two Poll-time rebinds, which had identical copies; also used by the
// SyncObjects rest-pose seeding). Collects every skinned mesh the band member
// draws. The face/hand/tongue/teeth skin meshes live in mOutfitDir's hashtable
// (reached by ObjDirItr), but the BODY clothing meshes (trackjacket / vestdenim /
// plaidshirt / shred + _skin.N) are merged resources with an EMPTY dir name — NOT
// in any hashtable — and are only reachable by walking the dir's DRAW tree
// (mDraws -> RndGroup patch.grp -> nested meshes, plus every LOD's
// Group()/TransGroup(), which is where Character::DrawLodOrShadow actually draws
// from), via the engine-native RndDrawable::ListDrawChildren recursion.
// Scope = {this, mOutfitDir}; mInstDir (guitar / mic / drums) is DELIBERATELY
// excluded — instruments attach to specific hand/prop bones, not the gender
// skeleton; rebinding their bones to animated character bones distorts the prop.
//
// frame-stall 2026-06-20 (TRACK A): the full walk is EXPENSIVE — an ObjDirItr
// RTTI dynamic_cast per hashtable entry + a recursive draw-tree walk that
// dynamic_cast<RndMesh*>'s every drawable. Pre-2026-06-20 it ran every Poll for
// every band member (the two Poll-time rebinds each called it) for the whole
// ~10s rebind-latch window — measured at ~650ms of song-start (the #1
// __dynamic_cast caller chain). The mesh SET only changes when the dir tree is
// re-stuffed, i.e. exactly at StartLoad / SyncObjects, which already re-arm the
// rebind latches. So we cache the collected list once per (re)load and hand back
// a copy each Poll; the heavy RTTI walk runs once instead of ~4x/frame. The
// internal O(N^2) std::find dedup is also replaced with O(1) std::set membership.
// Invalidated by NativeInvalidateSkinnedMeshCache() at every StartLoad/SyncObjects
// (same points that reset mNative*ReboundOnce), so a re-stuffed dir re-walks.
void BandCharacter::NativeRebuildSkinnedMeshCache() {
    std::vector<RndMesh *> &targets = mNativeSkinnedMeshCache;
    targets.clear();
    std::set<RndMesh *> seenMesh;     // O(1) target dedup (was O(N^2) std::find)
    std::set<RndDrawable *> visited;  // O(1) draw-tree cycle guard

    Character *drawChars[2] = { this, (Character *)mOutfitDir };
    // worklist of drawables to expand (start with each dir's top draw list)
    std::vector<RndDrawable *> work;
    for (int d = 0; d < 2; d++) {
        Character *dc = drawChars[d];
        if (!dc) continue;
        RndDir *dd = dc;
        // (1) hashtable objects (face/hands/etc.)
        for (ObjDirItr<RndMesh> mit(dd, true); mit != 0; ++mit) {
            RndMesh *m = mit;
            if (m && m->NumBones() != 0 && seenMesh.insert(m).second)
                targets.push_back(m);
        }
        // (2) seed the draw-tree walk from the dir's own draw list
        for (std::vector<RndDrawable *>::iterator it = dd->mDraws.begin();
             it != dd->mDraws.end(); ++it)
            work.push_back(*it);
        // (3) seed from every LOD's draw group + trans group — this is where the
        // BODY CLOTHING actually lives. It is NOT in mDraws, so without this the
        // female torso mesh is never reached.
        for (int li = 0; li < dc->mLods.size(); li++) {
            if (dc->mLods[li].Group()) work.push_back(dc->mLods[li].Group());
            if (dc->mLods[li].TransGroup()) work.push_back(dc->mLods[li].TransGroup());
        }
    }
    // (4) recurse the draw tree (groups -> nested clothing meshes). Bounded by a
    // visited set so a cyclic/shared group reference cannot loop forever.
    while (!work.empty()) {
        RndDrawable *dr = work.back();
        work.pop_back();
        if (!dr) continue;
        if (!visited.insert(dr).second) continue;
        RndMesh *m = dynamic_cast<RndMesh *>(dr);
        if (m && m->NumBones() != 0 && seenMesh.insert(m).second)
            targets.push_back(m);
        std::list<RndDrawable *> kids;
        dr->ListDrawChildren(kids);
        for (std::list<RndDrawable *>::iterator k = kids.begin(); k != kids.end(); ++k)
            if (*k && visited.find(*k) == visited.end())
                work.push_back(*k);
    }
    mNativeSkinnedCacheValid = true;
}

void BandCharacter::NativeCollectSkinnedMeshes(std::vector<RndMesh *> &targets) {
    // frame-stall 2026-06-20 (TRACK A): serve from the per-member cache; only
    // (re)walk the dir tree + draw tree (the RTTI-heavy part) when the cache was
    // invalidated by a StartLoad/SyncObjects. Optional timing probe (RB3_SKIN_TIMING)
    // sums rebuild vs cache-hit cost so the song-start saving is measurable.
    static int sTiming = -1;
    if (sTiming < 0) sTiming = getenv("RB3_SKIN_TIMING") ? 1 : 0;
    // A/B escape hatch (measurement only): RB3_SKIN_NOCACHE=1 reproduces the
    // pre-2026-06-20 behavior — re-walk the full RTTI tree EVERY call — so the
    // cached vs uncached per-Poll cost can be compared in one binary.
    static int sNoCache = -1;
    if (sNoCache < 0) sNoCache = getenv("RB3_SKIN_NOCACHE") ? 1 : 0;
    if (sNoCache) mNativeSkinnedCacheValid = false;
    if (!mNativeSkinnedCacheValid) {
        if (sTiming) {
            static double sRebuildMs = 0.0;
            static long sRebuilds = 0;
            timespec t0, t1;
            clock_gettime(CLOCK_MONOTONIC, &t0);
            NativeRebuildSkinnedMeshCache();
            clock_gettime(CLOCK_MONOTONIC, &t1);
            double ms = (t1.tv_sec - t0.tv_sec) * 1e3 +
                        (t1.tv_nsec - t0.tv_nsec) * 1e-6;
            sRebuildMs += ms;
            sRebuilds++;
            if ((sRebuilds % 50) == 0 || ms > 2.0)
                fprintf(stderr,
                        "[SKIN_TIMING] REBUILD char='%s' meshes=%d this=%.3fms "
                        "rebuilds=%ld totalRebuildMs=%.1f\n",
                        Name() ? Name() : "?", (int)mNativeSkinnedMeshCache.size(),
                        ms, sRebuilds, sRebuildMs);
        } else {
            NativeRebuildSkinnedMeshCache();
        }
    } else if (sTiming) {
        static long sHits = 0;
        if ((++sHits % 2000) == 0)
            fprintf(stderr, "[SKIN_TIMING] cacheHits=%ld\n", sHits);
    }
    // hand back a copy: callers append to / dedup against `targets` and some
    // (RebindHeadHandsAtRest) mutate per-mesh state; keep the cache itself read-only.
    targets.insert(targets.end(), mNativeSkinnedMeshCache.begin(),
                   mNativeSkinnedMeshCache.end());
}

void BandCharacter::NativeInvalidateSkinnedMeshCache() {
    mNativeSkinnedCacheValid = false;
    mNativeSkinnedMeshCache.clear();
}

// scout-c8 (render-polish 2026-06-11): CHARACTER-SPACE rest capture.
// The C8 deep-dive proved the "rotation-basis divergence" was actually a rest-
// bake SPACE error: the rest snapshot stored own->WorldXfm() — which includes
// the member's stage/venue PLACEMENT (and is captured after the member root has
// been positioned, e.g. x=-18.5/y=24 measured) — while the skinned meshes' verts
// are authored in MODEL space at the origin (raw locality audit: verts sit
// 5-9u from their authored bind bones, but 27-60u == |placement| from the
// world-space-baked rests). Baking offset = inv(worldRest) then makes every
// vert swing on a |placement|-length lever arm as the bone rotates -> the
// R*sin(theta) smear (200-460u extents) the V24 guard hides. Fix: store the
// rest RELATIVE to the bone's trans-chain ROOT (the member instance), i.e.
// L_rest = world_rest * inv(rootWorld). Then offset = meshWorld(=I) * inv(L_rest)
// composes to inv(authoredBindLocal) * L(t) * M(t) — placement-independent and
// correct through animation. Bones rooted at the static magnet (root world ==
// identity) are unaffected (rel == world there).
static Transform NativeCharSpaceRestXfm(RndTransformable *own) {
    Transform rest = own->WorldXfm();
    RndTransformable *root = own;
    int guard = 0;
    while (root->TransParent() && guard++ < 64)
        root = root->TransParent();
    if (root != own) {
        Transform invRoot;
        Invert(root->WorldXfm(), invRoot);
        Transform rel;
        Multiply(rest, invRoot, rel);
        return rel;
    }
    return rest;
}

// render-polish 2026-06-11 (char-render step 2): deterministic rest-pose seeding.
// Called from SyncObjects() IMMEDIATELY after SetDeformation(), where the gender
// deform clip's PoseMeshes() has just left every deform-driven bone at the
// character's weighted REST pose — the exact basis the head/hands rest rebind
// bakes its inverse-bind against. Seeds mNativeRestPose for any skin-mesh bone
// that already resolves to a DISTINCT live per-member instance (own != bound)
// and isn't snapshotted yet (first capture wins, never overwritten), so a mesh
// (re)merged MID-SONG gets a true-rest bake basis instead of relying on the
// Poll-time fallback (which would capture a mid-clip pose for a bone that is
// already animating). Bones still resolving to the shared magnet (own == bound)
// are skipped — the SyncObjects-time Find caveat in
// CHAR_SKINNING_DEFORM_INVESTIGATION.md — and remain covered by the Poll-time
// first-DISTINCT-resolve capture in RebindHeadHandsAtRest.
void BandCharacter::NativeCaptureRestPoseAfterDeform() {
    static int sDisabled = -1;
    if (sDisabled < 0) sDisabled = getenv("RB3_NO_HEAD_REBIND") ? 1 : 0;
    if (sDisabled) return;
    // Poison guard: SetDeformation() poses only the DEFORM-CLIP bones to rest. If a
    // venue clip is already playing (the mid-song StartClipLoads churn), any bone
    // OUTSIDE the deform clip (props / hair / mic-stand) is at a MID-CLIP pose right
    // now — seeding it as "rest" would bake a wrong basis. Seed only while no clip
    // plays (initial-load SyncObjects — where ALL bones hold load/deform rest).
    // Empirically all mid-song seeds were added=0 anyway (bones already snapshotted
    // at load); genuinely-new late bones stay covered by the Poll-time
    // first-distinct-resolve capture.
    if (mDriver && mDriver->FirstPlaying()) {
        if (NativeReloadProbe())
            fprintf(stderr,
                    "[REST_SEED] poll=%d char='%s' SKIPPED (clip playing) restPose=%d\n",
                    gNativeBandCharPollSerial, Name() ? Name() : "?",
                    (int)mNativeRestPose.size());
        return;
    }
    std::vector<RndMesh *> targets;
    NativeCollectSkinnedMeshes(targets);
    int added = 0, distinct = 0;
    for (std::vector<RndMesh *>::iterator mi = targets.begin();
         mi != targets.end(); ++mi) {
        RndMesh *mesh = *mi;
        for (int b = 0; b < mesh->NumBones(); b++) {
            RndTransformable *bound = mesh->BoneTransAt(b);
            if (!bound || !bound->Name()) continue;
            RndTransformable *own = Find<RndTransformable>(bound->Name(), false);
            if (!own) continue;
            std::string bname(bound->Name());
            bool isDistinct = (own != bound);
            if (isDistinct) distinct++;
            // capture policy: a DISTINCT resolve (own != bound — the live
            // per-member instance) is the authoritative rest basis; it is captured
            // once and never overwritten. An own==bound resolve (mesh already bound
            // to whatever Find returns — live bone OR the shared magnet) is seeded
            // only if nothing better exists, and stays overwritable by the first
            // later distinct resolve (mNativeRestDistinct provenance).
            bool haveDistinct =
                mNativeRestDistinct.find(bname) != mNativeRestDistinct.end();
            if (haveDistinct) continue;
            if (!isDistinct && mNativeRestPose.find(bname) != mNativeRestPose.end())
                continue;
            // scout-c8: capture in CHARACTER space (placement divided out), not
            // world space — see NativeCharSpaceRestXfm.
            Transform rest = NativeCharSpaceRestXfm(own);
            // same finite/sane guard as the Poll-time capture (the engine clamp is
            // disabled for rebound meshes, so a NaN inverse-bind has no backstop)
            if (!(std::fabs(rest.v.x) < 1e5f && std::fabs(rest.v.y) < 1e5f &&
                  std::fabs(rest.v.z) < 1e5f))
                continue;
            mNativeRestPose[bname] = rest;
            if (isDistinct) mNativeRestDistinct.insert(bname);
            added++;
        }
    }
    if (NativeReloadProbe()) {
        fprintf(stderr,
                "[REST_SEED] poll=%d char='%s' meshes=%d distinctSlots=%d added=%d "
                "restPose=%d\n",
                gNativeBandCharPollSerial, Name() ? Name() : "?", (int)targets.size(),
                distinct, added, (int)mNativeRestPose.size());
        // one-shot full target inventory per member: attributes guard-dropped mesh
        // names to band members vs venue extras/crowd (whose meshes are NOT in any
        // BandCharacter draw tree).
        for (std::vector<RndMesh *>::iterator mi = targets.begin();
             mi != targets.end(); ++mi) {
            RndMesh *m = *mi;
            RndMesh *go = m->GeomOwner();
            fprintf(stderr,
                    "[CHAR_MESH] char='%s' mesh='%s' bones=%d rebound=%d shared=%d\n",
                    Name() ? Name() : "?", m->Name() ? m->Name() : "?", m->NumBones(),
                    (int)m->mNativeBonesRebound, (int)(go && go != m));
        }
    }
}

// wave-08: rebind this band member's outfit skin meshes from the static shared
// char/main/skeleton magnet onto the member's OWN animated per-member skeleton.
//
// GROUND TRUTH (wave-07 BAND_ANIM_PROBE, built+measured): at Poll time
// Find<RndTransformable>("bone_R-upperArm.mesh") from THIS BandCharacter resolves
// to the LIVE per-member skeleton bone (e.g. player0 0x..429c00) which MOVES
// 100-187u/frame (a real venue clip is playing). The outfit skin meshes, however,
// are bound at parse/merge time to a DIFFERENT, STATIC shared magnet
// (char/main/skeleton.milo, 0x..924ec0, worldPos (7.4,-0.8,57.5)). So the band
// renders static AND the female (trackjacket) flings (her female-authored
// inverse-bind offset lands on the male-bind static magnet -> skinPos 19.8u).
//
// FIX: for each outfit skin bone, look up its animated counterpart BY NAME and
// SetBone(b, own, /*calcOffset=*/false) — keeping the authored gender-correct
// offset while repointing to the moving instance. This fixes BOTH the static band
// (the bone now animates) AND the female fling (her female offset now composes
// against her female-posed per-member bone -> skinPos ~0). Runs once per member,
// after the per-member skeleton is live (guarded by mNativeReboundOnce). Only
// touches bones whose Find resolves to a DIFFERENT instance than the bound one
// (own != bound) so already-correct binds are left alone.
//
// SUPERSEDES the wave-06 renderer SKEL_REBAKE (which rebakes against the static
// magnet and would CONFLICT): each rebound mesh sets RndMesh::mNativeBonesRebound,
// which the renderer's rebake AND fling-clamp both skip (the clamp would freeze a
// now-correctly-animating arm). The clamp stays live for crowd/extras + any
// dynamic hair/face bones we don't rebind.
//
// DEFAULT-ON, TORSO-SCOPED (opt-out RB3_NO_SKEL_REBIND=1) — see the wave-08 finding.
// The rebind repoints the outfit skin meshes from the static shared char/main/
// skeleton magnet onto the member's OWN animated per-member skeleton bone (resolved
// by name via Find, which at Poll time returns the LIVE moving instance), so the
// band ANIMATES: the OUTFIT-bound bone_R-upperArm worldPos goes from byte-identical-
// static to 744+ distinct values, up to ~200u/frame (MEASURED), and the female
// trackjacket stops flinging (skin-to-bone delta 50-65u limb extent, clean — was a
// ~20u static bind mismatch before).
//
// SCOPE = TORSO CLOTHING ONLY (trackjacket / vestdenim / plaidshirt / shred + _skin).
// The high-bone head/hands/face meshes are DELIBERATELY NOT rebound: their long-thin
// geometry (hair strands, fingers) shards when skinned to the animated bone, because
// the animated per-member bone's rotation BASIS differs from the static magnet the
// authored offsets were baked against (bone ORIGINS map correctly — translation
// delta <65u — but a basis mismatch flings vertices far from the bone origin into
// thin radiating shards; calcOffset=true shards too, since the skeleton is already
// animating when first reachable so there is no rest frame to re-bake). The compact
// torso/arm clothing has no such long-thin geometry, so it rebinds CLEANLY and
// animates. Head/hands stay coherent-static via the wave-06 rebake (which still runs
// on the non-rebound meshes). Full-scope rebind (incl. head/hands) is available for
// study via RB3_SKEL_REBIND_FULL=1 (it animates the whole body but shards thin geo).
// Opt-out the whole rebind with RB3_NO_SKEL_REBIND=1 (-> wave-06 coherent static).
void BandCharacter::RebindOutfitBonesToOwnSkeleton() {
    static int sDisabled = -1;
    if (sDisabled < 0) sDisabled = getenv("RB3_NO_SKEL_REBIND") ? 1 : 0;
    if (sDisabled) return;
    if (mNativeReboundOnce) return; // fully rebound: never scan again

    bool probe = getenv("SKEL_REBIND_PROBE") != 0;
    int meshes = 0, slots = 0, reboundBones = 0, reboundMeshes = 0;
    int sawAnimated = 0;  // bones whose Find result differs from the bound magnet
    int gotBodyMesh = 0;  // rebound at least one high-bone (>=20) body/face mesh

    // Collect every skinned mesh the band member draws (shared collector — see
    // NativeCollectSkinnedMeshes for the hashtable + draw-tree + LOD-group walk).
    std::vector<RndMesh *> targets;
    NativeCollectSkinnedMeshes(targets);

    // TORSO-CLOTHING-ONLY by default (the clean scope — see header comment). Rebind
    // only the body clothing meshes (which have compact geometry and rebind without
    // shards), skipping the high-bone head/hands/face whose long-thin geometry shards
    // under the rotation-basis mismatch. RB3_SKEL_REBIND_FULL=1 rebinds everything
    // (animates the whole body but shards thin geo — for study only).
    static int sTorsoOnly = -1;
    if (sTorsoOnly < 0) sTorsoOnly = getenv("RB3_SKEL_REBIND_FULL") ? 0 : 1;
    for (std::vector<RndMesh *>::iterator mi = targets.begin();
         mi != targets.end(); ++mi) {
        RndMesh *mesh = *mi;
        if (mesh->mNativeBonesRebound) continue; // already rebound: don't re-touch
        if (sTorsoOnly) {
            const char *mn = mesh->Name();
            bool torso = mn && (strstr(mn, "trackjacket") || strstr(mn, "vestdenim") ||
                                strstr(mn, "plaidshirt") || strstr(mn, "shred"));
            if (!torso) continue;
        }
        meshes++;
        if (probe && meshes <= 16) {
            fprintf(stderr, "[SKEL_REBIND]   mesh='%s' numBones=%d\n",
                    mesh->Name() ? mesh->Name() : "?", mesh->NumBones());
        }
        int meshRebound = 0;
        for (int b = 0; b < mesh->NumBones(); b++) {
            RndTransformable *bound = mesh->BoneTransAt(b);
            if (!bound || !bound->Name()) continue;
            slots++;
            RndTransformable *own = Find<RndTransformable>(bound->Name(), false);
            if (!own) continue;
            if (own == bound) continue; // already bound to resolvable instance
            sawAnimated++;
            static int sCalc = -1;
            if (sCalc < 0) sCalc = getenv("RB3_SKEL_REBIND_CALCOFF") ? 1 : 0;
            mesh->SetBone(b, own, sCalc != 0);
            reboundBones++;
            meshRebound++;
            if (probe && reboundBones <= 8) {
                fprintf(stderr,
                    "[SKEL_REBIND] member='%s' mesh='%s' bone='%s' magnet=%p -> own=%p\n",
                    Name() ? Name() : "?", mesh->Name() ? mesh->Name() : "?",
                    bound->Name(), (void *)bound, (void *)own);
            }
        }
        if (meshRebound > 0) {
            mesh->mNativeBonesRebound = true; // renderer: skip rebake + clamp
            reboundMeshes++;
            // Latch gate: any torso clothing mesh (>=11 bones) counts as "body
            // caught". Some torso meshes (vestdenim_resource=18, shred_resource=19,
            // trackjacket_skin.2=11) are <20 bones, so a >=20 gate could miss a
            // member; >=11 covers every torso outfit mesh while still excluding
            // low-bone props/instruments.
            if (mesh->NumBones() >= 11) gotBodyMesh = 1;
            // Optional post-rebind verification (gated SKEL_REBIND_SKINPOS=1). The
            // TRUE skinning-correctness metric is |skinWorld - boneWorld| — how far a
            // bone's composed skin places its vertices from the bone itself. For clean
            // skinning this is bounded by limb/joint extent (~40-65u, MEASURED); a
            // broken bind would fling it to hundreds/thousands. NOTE: a skinned mesh's
            // own WorldXfm is identity (the renderer convention — the palette already
            // carries world space), so a "mesh-local" (skin * inv(meshWorld)) measure
            // is NOT a bind-mismatch — it just reads back the character's world
            // position (~hundreds of u from origin) and is misleading; use the
            // bone-relative delta below.
            if (getenv("SKEL_REBIND_SKINPOS")) {
                float worst = 0.f;
                const char *worstBone = "?";
                for (int b2 = 0; b2 < mesh->NumBones(); b2++) {
                    RndTransformable *bt = mesh->BoneTransAt(b2);
                    if (!bt) continue;
                    Transform skin;
                    Multiply(mesh->BoneOffsetAt(b2), bt->WorldXfm(), skin);
                    Vector3 d;
                    Subtract(skin.v, bt->WorldXfm().v, d);
                    float dd = d.x * d.x + d.y * d.y + d.z * d.z;
                    if (dd > worst) {
                        worst = dd;
                        worstBone = bt->Name() ? bt->Name() : "?";
                    }
                }
                fprintf(stderr,
                    "[SKEL_REBIND_SKINPOS] member='%s' mesh='%s' worstBone='%s' "
                    "skinToBoneDelta=%.3fu (clean<~65u limb extent; fling=hundreds)\n",
                    Name() ? Name() : "?", mesh->Name() ? mesh->Name() : "?",
                    worstBone, std::sqrt(worst));
            }
        }
    }

    // LATCH only once the rebind is COMPLETE. The body clothing + face/hands skin
    // meshes (>=20 bones) can become reachable a few frames AFTER the hair props,
    // so an early latch on the FIRST rebound bone would freeze before the body is
    // caught (regression: female torso/arm left flung). Strategy: keep scanning each
    // Poll; whenever a scan rebinds something new, reset the quiet counter; once we
    // have rebound a high-bone body/face mesh AND a later scan finds nothing new for
    // several consecutive Polls, latch and stop scanning. Meshes already rebound are
    // skipped above, so re-scans only cost the dir/draw-tree walk (bounded).
    if (gotBodyMesh) mNativeReboundBody = 1;
    if (reboundBones > 0) {
        mNativeReboundQuiet = 0;
    } else {
        mNativeReboundQuiet++;
        // Only latch after the body was caught AND a sustained quiet period (late
        // LOD pieces — shoes / pants / accessories — stream in a second or more
        // after the torso, so a short quiet window would latch before they bind and
        // leave them to the fling-clamp). ~90 quiet Polls (>1s) covers the streaming
        // tail. Fallback long grace for members with no >=20-bone outfit (low-LOD /
        // instrument-only dirs) so scanning still stops. The per-mesh
        // mNativeBonesRebound skip keeps the re-scan cost bounded meanwhile.
        if ((mNativeReboundBody && mNativeReboundQuiet >= 90) ||
            mNativeReboundQuiet >= 600)
            mNativeReboundOnce = 1;
    }

    if (probe && (meshes > 0 || reboundBones > 0)) {
        fprintf(stderr,
            "[SKEL_REBIND] member='%s' meshes=%d slots=%d reboundBones=%d "
            "reboundMeshes=%d body=%d quiet=%d latched=%d\n",
            Name() ? Name() : "?", meshes, slots, reboundBones, reboundMeshes,
            mNativeReboundBody, mNativeReboundQuiet, mNativeReboundOnce);
    }
}

// C7/C8 — rebind the head/hair/hands/face (and any remaining NON-torso) skin meshes
// onto the member's OWN per-member skeleton with an EXACT inverse-bind baked against
// the bone's REST WorldXfm. The head/hair/finger geometry is long-thin, so the small
// rotation-basis error between the authored magnet bind and the live skeleton (which
// the compact torso tolerates) explodes it into radiating shards (R*sin(theta)). The
// torso rebind (RebindOutfitBonesToOwnSkeleton, calcOffset=false) can't fix this
// because the authored offset was baked against the magnet basis; calcOffset=true at
// the Poll site shards too (the bone is already mid-animation there). The fix: capture
// each per-member bone's REST WorldXfm at the FIRST Poll (this method runs BEFORE
// Character::Poll(), so the skeleton still holds the SetDeformation gender-bind rest
// pose), then bake mOffset = meshWorld * inverse(restWorld) and bind to the LIVE bone.
// At rest the composed skin is meshWorld (coherent); as the bone animates the verts
// follow it correctly. The rest snapshot makes late-streamed head/accessory meshes
// rebake against the true rest even on later (animating) frames. Opt-out
// RB3_NO_HEAD_REBIND=1. Crowd/extras are never in a BandCharacter dir (untouched).
void BandCharacter::RebindHeadHandsAtRest() {
    static int sDisabled = -1;
    if (sDisabled < 0) sDisabled = getenv("RB3_NO_HEAD_REBIND") ? 1 : 0;
    if (sDisabled) return;
    if (mNativeHeadReboundOnce) return;
    bool probe = getenv("HEAD_REBIND_PROBE") != 0;

    // Collect every skinned mesh the member draws (shared collector — same walk as
    // the torso rebind; see NativeCollectSkinnedMeshes).
    std::vector<RndMesh *> targets;
    NativeCollectSkinnedMeshes(targets);

    int reboundMeshes = 0, pending = 0, slots = 0, reboundBones = 0;
    for (std::vector<RndMesh *>::iterator mi = targets.begin();
         mi != targets.end(); ++mi) {
        RndMesh *mesh = *mi;
        if (mesh->mNativeBonesRebound) continue; // owned by torso rebind or already done
        // W2.8c mutual exclusion (plan Section 2a-i): when the per-frame conjugation pass
        // owns the hand/finger/glove meshes (RB3_HANDS_PERFRAME_CONJ), leave their
        // offsets PRISTINE here so it can capture the authored invBind (A_b) unmutated.
        // Flag-OFF (default) this is inert — no in-scope mesh is skipped, byte-identical.
        {
            static int sConjOwnsHands = -1;
            if (sConjOwnsHands < 0)
                sConjOwnsHands = getenv("RB3_HANDS_PERFRAME_CONJ") ? 1 : 0;
            if (sConjOwnsHands) {
                const char *hn = mesh->Name();
                if (hn) {
                    std::string ln(hn);
                    for (size_t i = 0; i < ln.size(); i++) {
                        char c = ln[i];
                        if (c >= 'A' && c <= 'Z') ln[i] = (char)(c + 32);
                    }
                    if (ln.find("hand") != std::string::npos ||
                        ln.find("finger") != std::string::npos ||
                        ln.find("glove") != std::string::npos)
                        continue; // property of NativeConjHandsPerFrame
                }
            }
        }
        // render-polish 2026-06-11 (char-render): own==bound rest-rebake is
        // OPT-IN (RB3_BOUND_REBAKE=1), default OFF. EXPERIMENT OUTCOME (measured,
        // this wave): rebaking the never-rebound own==bound garments anchors their
        // translation (draw-time |skinWorld-boneWorld| <= 92u, zero >120u flings,
        // no mixed anchors) but does NOT repair the native rotation-basis
        // divergence — verts far from bone origins smear by R*sin(theta) to
        // PERSISTENT 200-460u world extents (gloves/fingernails/jackets; a
        // character is ~70u tall), i.e. the V24 guard was correctly hiding
        // genuinely broken poses, and exempting them drew full-screen slabs.
        // The faithful fix for those meshes is the CharBones/pose-pipeline basis
        // root-cause (C8), not a bind-side bake. Default OFF = those meshes stay
        // on the engine clamp/V24 guard exactly as pre-2026-06-11.
        static int sNoBoundRebake = -1;
        if (sNoBoundRebake < 0)
            sNoBoundRebake = getenv("RB3_BOUND_REBAKE") ? 0 : 1;
        const char *mn = mesh->Name();
        bool torsoName = mn && (strstr(mn, "trackjacket") || strstr(mn, "vestdenim") ||
                                strstr(mn, "plaidshirt") || strstr(mn, "shred"));
        // COMPLEMENT of the torso scope: a torso mesh with any DISTINCT-resolving
        // bone (own != bound) is owned by the Poll torso rebind
        // (RebindOutfitBonesToOwnSkeleton, authored offsets) — skip it here. But a
        // torso mesh whose bones ALL resolve own==bound can never be repointed by
        // the torso rebind (it needs a distinct instance); pre-2026-06-11 such
        // meshes fell to the engine clamp/guard -> invisible torso. They now fall
        // through to the own==bound rest-rebake below.
        if (torsoName && !sNoBoundRebake) {
            bool anyDistinct = false;
            for (int b = 0; b < mesh->NumBones(); b++) {
                RndTransformable *bound = mesh->BoneTransAt(b);
                if (!bound || !bound->Name()) continue;
                RndTransformable *own = Find<RndTransformable>(bound->Name(), false);
                if (own && own != bound) { anyDistinct = true; break; }
            }
            if (anyDistinct) continue; // torso rebind's lane
        } else if (torsoName) {
            continue;
        }
        // GeomOwner-shared meshes: the engine builds the GPU bone palette from
        // owner=mesh->GeomOwner() (owner->BoneOffsetAt/BoneTransAt), so writing this
        // mesh's bone array has no GPU effect. If the OWNER has been rebound its
        // palette is already correct — propagate the flag so the drawn mesh is also
        // exempted from the fling-clamp (which would otherwise freeze its
        // legitimately-animating rest-baked palette to bind). If the owner is NOT
        // (yet) rebound, leave the mesh to the clamp (flagging it would disable the
        // only backstop -> a HARDER shard).
        RndMesh *go = mesh->GeomOwner();
        if (go && go != mesh) {
            if (go->mNativeBonesRebound) {
                mesh->mNativeBonesRebound = true;
                reboundMeshes++;
                reboundBones++; // counts as progress: keeps the quiet counter armed
            } else {
                pending++;
            }
            continue;
        }
        // TWO-PASS apply (render-polish 2026-06-11): NEVER mutate a mesh that
        // cannot COMPLETE. A partial rebake (some bones rest-baked, some left
        // authored, mesh unflagged) hands the engine clamp a MIXED palette that
        // the V24 ratio guard then drops every frame — worse than untouched.
        // Pass A resolves every bone slot and validates a rest basis; only if ALL
        // slots resolve does pass B repoint + rebake + flag the mesh.
        int nb = mesh->NumBones();
        std::vector<RndTransformable *> owns((size_t)nb, (RndTransformable *)0);
        std::vector<Transform> rests((size_t)nb);
        std::vector<unsigned char> apply((size_t)nb, 0);
        int resolvable = 0, miss = 0;
        const char *missBone = 0;
        const char *missWhy = "";
        for (int b = 0; b < nb; b++) {
            RndTransformable *bound = mesh->BoneTransAt(b);
            if (!bound || !bound->Name()) continue; // empty slot (engine: identity)
            slots++;
            RndTransformable *own = Find<RndTransformable>(bound->Name(), false);
            if (!own) {
                miss++;
                if (!missBone) { missBone = bound->Name(); missWhy = "unresolvable"; }
                continue;
            }
            std::string bname(bound->Name());
            std::map<std::string, Transform>::iterator rp = mNativeRestPose.find(bname);
            bool haveDistinct =
                mNativeRestDistinct.find(bname) != mNativeRestDistinct.end();
            Transform rest;
            if (own == bound) {
                // render-polish 2026-06-11 (char-render): the mesh is ALREADY bound
                // to the instance Find resolves (a per-member live bone — or the
                // shared magnet when no live instance exists). Pre-2026-06-11 these
                // were skipped -> never rebound -> the engine clamp froze some bones
                // (>12u) while others passed -> mixed palette -> V24 ratio guard
                // dropped the whole mesh EVERY frame (invisible legs/feet/hands —
                // the dominant "only teeth/eyes render" symptom; ~46 never-rebound
                // meshes/run measured, bone0 at STAGE coords). Fix: same rest-pose
                // rebake as the distinct case, against the post-deform rest
                // snapshot (seeded at load-time SyncObjects, where SetDeformation
                // just posed THIS bone): at rest the composed skin is identity ->
                // coherent; as the bone animates the verts follow. No repoint
                // needed (already bound to it).
                if (sNoBoundRebake || rp == mNativeRestPose.end()) {
                    miss++;
                    if (!missBone) {
                        missBone = bound->Name();
                        missWhy = sNoBoundRebake ? "boundRebakeOff" : "noRest(own==bound)";
                    }
                    continue;
                }
                rest = rp->second;
            } else if (rp != mNativeRestPose.end() && haveDistinct) {
                rest = rp->second;
            } else {
                // FIRST distinct resolve of this bone: this method runs
                // pre-Character::Poll(), so a freshly-resolvable per-member bone
                // still holds its load/rest pose — capturing on first-resolve (not
                // just the literal first Poll) covers per-member skeletons that
                // stream in late. It also OVERWRITES an own==bound-seeded entry
                // (which may have captured the shared magnet): the distinct live
                // instance is the authoritative basis (mNativeRestDistinct). Reject
                // a non-finite / huge rest xfm (a broken bone) so Invert can't
                // produce NaN — the engine clamp is disabled for rebound meshes,
                // so there is no backstop. The capture is KEPT even if this mesh
                // fails to complete (it is the authoritative basis for every mesh).
                //
                // scout-c8: (a) capture in CHARACTER space (placement divided
                // out); (b) NEVER capture while a clip is playing — a mid-song
                // first-resolve (after the reload-churn re-arm) used to snapshot
                // a mid-clip/IK pose (measured: a guitar-FRET-hand pose baked as
                // "rest" for the fingernail bones) which is a POSE poison the
                // space fix cannot repair. Such bones stay pending (mesh stays
                // on the guard — status quo) until a clip-free capture happens.
                if (mDriver && mDriver->FirstPlaying()) {
                    // W2.2.S2 — count-in/walkon clip-free capture gap
                    // (RB3_HANDS_BIND_FIX, class:feature, default OFF). A bone whose
                    // FIRST distinct resolve lands mid-clip (the count-in/walk-on
                    // window always plays a clip, so a per-member skeleton that
                    // streams in there first-resolves poisoned) is normally rejected
                    // -> the whole mesh stays pending -> the V24 ratio guard drops it
                    // (S1a measured: saddleshoe_skin.2 4.73x DROP + head.mesh 69.5u /
                    // hand 2.3x grazes, all in the count-in window). But the
                    // load-time seed (NativeCaptureRestPoseAfterDeform, poison-guarded
                    // to the clip-free deform rest) has usually ALREADY snapshotted
                    // this bone's own==bound magnet rest into mNativeRestPose (rp !=
                    // end here, and it is NON-distinct = clip-free by construction:
                    // the seed and the RB3_BOUND_REBAKE Poll capture are the only
                    // writers and both are clip-free). The shared magnet and the
                    // per-member bone hold the SAME weighted gender-bind rest at
                    // load, and char-space divides out placement, so that seed is a
                    // valid clip-free basis for the now-distinct bone. Reuse it
                    // instead of poisoning on the mid-clip pose, and promote it to
                    // distinct (authoritative for later frames). Default OFF; the S1b
                    // oracle (offset'=meshWorld*inv(rest), HANDS_BIND_ORACLE_PERTURB)
                    // fail-reds a wrong basis, and S3 measures whether this actually
                    // clears the drop without re-introducing the 200-460u tripwire.
                    static int sHandsBindFix = -1;
                    if (sHandsBindFix < 0)
                        sHandsBindFix = getenv("RB3_HANDS_BIND_FIX") ? 1 : 0;
                    if (sHandsBindFix && rp != mNativeRestPose.end()) {
                        rest = rp->second;              // clip-free load-time seed
                        mNativeRestDistinct.insert(bname); // promote: authoritative
                        owns[b] = own;
                        rests[b] = rest;
                        apply[b] = 1;
                        resolvable++;
                        continue;
                    }
                    miss++;
                    if (!missBone) { missBone = bound->Name(); missWhy = "clipPlaying"; }
                    continue;
                }
                rest = NativeCharSpaceRestXfm(own);
                if (!(std::fabs(rest.v.x) < 1e5f && std::fabs(rest.v.y) < 1e5f &&
                      std::fabs(rest.v.z) < 1e5f)) {
                    miss++;
                    if (!missBone) { missBone = bound->Name(); missWhy = "nonFiniteRest"; }
                    continue;
                }
                mNativeRestPose[bname] = rest;
                mNativeRestDistinct.insert(bname);
            }
            owns[b] = own;
            rests[b] = rest;
            apply[b] = 1;
            resolvable++;
        }
        if (miss == 0 && resolvable > 0) {
            // pass B: all bones validated — repoint + bake
            // mOffset = meshWorld * inverse(restWorld), bind to the LIVE bone.
            int anchoredMine = 0, anchoredForeign = 0;
            for (int b = 0; b < nb; b++) {
                if (!apply[b]) continue;
                if (owns[b] != mesh->BoneTransAt(b))
                    mesh->SetBone(b, owns[b], false);
                Transform invRest;
                Invert(rests[b], invRest);
                Multiply(mesh->WorldXfm(), invRest, mesh->BoneOffsetAt(b));
                reboundBones++;
                if (probe) {
                    // anchor diagnosis: is this bone a trans-descendant of THIS
                    // member (live per-member skeleton) or foreign (the shared
                    // magnet / another root)? A mesh mixing both anchors smears
                    // between the stage and the authored location.
                    bool mine = false;
                    int guard = 0;
                    for (RndTransformable *p = owns[b]; p && guard < 64;
                         p = p->TransParent(), guard++)
                        if (p == (RndTransformable *)this) { mine = true; break; }
                    if (mine) anchoredMine++;
                    else anchoredForeign++;
                }
            }
            if (probe && anchoredForeign > 0) {
                static std::map<std::string, int> sMixSeen;
                std::string key = std::string(Name() ? Name() : "?") + "/" +
                                  (mesh->Name() ? mesh->Name() : "?");
                if (sMixSeen[key]++ % 120 == 0)
                    fprintf(stderr,
                            "[HEAD_REBIND_ANCHOR] member='%s' mesh='%s' mine=%d "
                            "foreign=%d (mixed anchors -> smear candidate)\n",
                            Name() ? Name() : "?", mesh->Name() ? mesh->Name() : "?",
                            anchoredMine, anchoredForeign);
            }
            mesh->mNativeBonesRebound = true; // engine skips rebake + fling-clamp
            reboundMeshes++;
        } else if (miss > 0) {
            pending++; // not yet completable — mesh left UNTOUCHED; retry next frame
            if (probe) {
                static std::map<std::string, int> sPendSeen; // throttle per mesh
                std::string key = std::string(Name() ? Name() : "?") + "/" +
                                  (mesh->Name() ? mesh->Name() : "?");
                if (sPendSeen[key]++ % 120 == 0)
                    fprintf(stderr,
                            "[HEAD_REBIND_PENDING] member='%s' mesh='%s' miss=%d "
                            "resolvable=%d/%d firstMiss='%s' why=%s\n",
                            Name() ? Name() : "?", mesh->Name() ? mesh->Name() : "?",
                            miss, resolvable, nb, missBone ? missBone : "-", missWhy);
            }
        }
    }
    mNativeRestCaptured = true;

    // Latch policy (render-polish 2026-06-11, char-render step 3). The old fixed
    // 120-quiet "give-up" latched while reload churn was still streaming meshes, so
    // anything arriving later never got a correct bind and lived on the clamp/guard
    // for the whole song. New policy: latch as COMPLETE only when no UNREBOUND
    // in-scope mesh remains (pending==0) and the scan made no progress for a short
    // sustained window (>=30 Polls — covers intra-merge streaming gaps where a new
    // mesh appears a few frames later without any StartLoad/SyncObjects event).
    // Members with a permanently-unresolvable mesh (own==bound forever — the known
    // 1-residual-mesh-per-member case, which correctly stays on the engine clamp)
    // never satisfy pending==0, so keep a LONG give-up fallback (600 Polls ≈ 10s)
    // purely to bound the per-Poll draw-tree rescan cost. StartLoad AND SyncObjects
    // both re-arm this latch, so reloads/merges always trigger a fresh scan pass.
    if (reboundBones == 0) mNativeHeadReboundQuiet++;
    else mNativeHeadReboundQuiet = 0;
    if ((pending == 0 && mNativeHeadReboundQuiet >= 30) ||
        mNativeHeadReboundQuiet >= 600)
        mNativeHeadReboundOnce = 1;

    if (probe && (reboundBones > 0 || pending > 0)) {
        fprintf(stderr,
            "[HEAD_REBIND] member='%s' targets=%d slots=%d reboundBones=%d "
            "reboundMeshes=%d pending=%d restPose=%d quiet=%d latched=%d\n",
            Name() ? Name() : "?", (int)targets.size(), slots, reboundBones,
            reboundMeshes, pending, (int)mNativeRestPose.size(),
            mNativeHeadReboundQuiet, mNativeHeadReboundOnce);
    }
}

// wave-inststrings (native-only): fix the band lead-guitar *_strings skin explosion.
//
// GROUND TRUTH (INST_STRINGS_PROBE, built+measured): the "brain"-class special
// guitars (chainsaw / guitar_brain / etc.) author their string-bend rig on the
// CHARACTER skeleton (char/char/main/skeleton_unshared.milo): all 10
// chainsaw_strings.mesh bones (bone_nut / bone_bridge / bone_bend_string01..06 /
// bone_vibrate_hi / bone_vibrate_low) resolve to skeleton_unshared.milo, and
// mInstDir->Find returns NIL for those names (the instrument's own
// <inst>_resource.milo has NO neck bones — unlike the standard guitars/basses
// whose *_strings bind to their own rigid resource neck at ratio 1.0). The neck
// bones ANIMATE during play (the guitarist flexes the neck: bone_nut swings
// 3-5u/window while bone_bridge stays ~0.6-1.2u) — so binding to skeleton_unshared
// is BY DESIGN (a basis-divergence skin bug), NOT a name mis-resolution.
//
// On Wii the authored inverse-bind composes correctly against the same-name
// character-skeleton bones; on native the per-member skeleton's basis/spacing
// diverges from the authored bind, so the rigid-authored strings mesh (bind 27.6u)
// skins to a ~136u world AABB (ratio ~5.0) -> the engine V24 [SHARD_GUARD] (which
// detects skeleton_unshared-bound meshes as `band` and applies the relaxed 4.0x/110u
// caps) STILL drops it (5.0 > 4.0 AND 136 > 110) -> the visible left-edge smear.
//
// FIX (rigid-anchor, default): repoint EVERY strings bone to ONE rigid anchor — the
// body-end bone (bone_bridge, the least-moving neck bone, which rides the instrument
// body rigidly) — and rebake each bone's offset = meshWorld * inv(anchorWorld) at the
// CURRENT pose. The whole strings mesh then rides that single rigid bone: its world
// AABB == its bind AABB through the entire bend animation (ratio ~1.0), exactly like
// the FINE instruments whose strings ride their rigid resource neck. This drops the
// in-mesh string-bend wobble, but the FINE instruments already render rigid strings,
// so this IS the correct visual target state. (RB3_INST_STRINGS_MODE=rebake instead
// rest-rebakes each bone in place, preserving the bend — kept for A/B; it does not
// hold ratio ~1.0 through the bend because the native animated basis itself diverges.)
//
// SCOPE: mInstDir's *_strings.mesh ONLY, and ONLY when its bones resolve to
// skeleton_unshared.milo. The FINE instruments (own-resource neck, ratio 1.0) never
// match the skeleton_unshared gate, so they are never touched (touching them would
// regress ratio 1.0). Each rebound mesh sets RndMesh::mNativeBonesRebound so the
// engine rebake/clamp skip it. Idempotent (mNativeInstReboundOnce latch, re-armed by
// SyncObjects/StartLoad like the other rebinds). DEFAULT-ON, opt-out
// RB3_NO_INST_REBIND=1 (mirrors RB3_NO_SKEL_REBIND). No-op on Wii (HX_NATIVE only),
// DC3-inert.
void BandCharacter::RebindInstStringsToRestBasis() {
    static int sDisabled = -1;
    if (sDisabled < 0) sDisabled = getenv("RB3_NO_INST_REBIND") ? 1 : 0;
    if (sDisabled) return;
    if (!mInstDir) return;
    if (mNativeInstReboundOnce) return;
    // mode: "rigid" (default — anchor all bones to one rigid neck bone) or "rebake"
    // (per-bone rest-rebake in place, preserves bend; A/B only).
    static int sRigid = -1;
    if (sRigid < 0) {
        const char *m = getenv("RB3_INST_STRINGS_MODE");
        sRigid = (m && std::strcmp(m, "rebake") == 0) ? 0 : 1;
    }
    bool probe = getenv("INST_REBIND_PROBE") != 0;

    // Collect mInstDir's skinned strings meshes (its own draw tree only — never
    // this/mOutfitDir; the rest of the instrument, _resource/_teeth, is ratio 1.0
    // and must stay untouched). Same walk shape as NativeCollectSkinnedMeshes.
    std::vector<RndMesh *> targets;
    {
        std::vector<RndDrawable *> work;
        for (ObjDirItr<RndMesh> mit(mInstDir, true); mit != 0; ++mit) {
            RndMesh *m = mit;
            if (m && m->NumBones() != 0 &&
                std::find(targets.begin(), targets.end(), m) == targets.end())
                targets.push_back(m);
        }
        for (std::vector<RndDrawable *>::iterator it = mInstDir->mDraws.begin();
             it != mInstDir->mDraws.end(); ++it)
            work.push_back(*it);
        for (int li = 0; li < mInstDir->mLods.size(); li++) {
            if (mInstDir->mLods[li].Group()) work.push_back(mInstDir->mLods[li].Group());
            if (mInstDir->mLods[li].TransGroup()) work.push_back(mInstDir->mLods[li].TransGroup());
        }
        std::vector<RndDrawable *> visited;
        while (!work.empty()) {
            RndDrawable *dr = work.back();
            work.pop_back();
            if (!dr) continue;
            if (std::find(visited.begin(), visited.end(), dr) != visited.end()) continue;
            visited.push_back(dr);
            RndMesh *m = dynamic_cast<RndMesh *>(dr);
            if (m && m->NumBones() != 0 &&
                std::find(targets.begin(), targets.end(), m) == targets.end())
                targets.push_back(m);
            std::list<RndDrawable *> kids;
            dr->ListDrawChildren(kids);
            for (std::list<RndDrawable *>::iterator k = kids.begin(); k != kids.end(); ++k)
                if (*k && std::find(visited.begin(), visited.end(), *k) == visited.end())
                    work.push_back(*k);
        }
    }

    int meshes = 0, reboundMeshes = 0, pending = 0;
    for (std::vector<RndMesh *>::iterator mi = targets.begin();
         mi != targets.end(); ++mi) {
        RndMesh *mesh = *mi;
        if (mesh->mNativeBonesRebound) continue; // already rebound
        const char *mn = mesh->Name();
        // GATE 1: name must end "_strings.mesh".
        if (!mn) continue;
        size_t len = std::strlen(mn);
        const char *suffix = "_strings.mesh";
        size_t slen = std::strlen(suffix);
        if (len < slen || std::strcmp(mn + len - slen, suffix) != 0) continue;
        // GATE 2: at least one bone must resolve to the character skeleton
        // (skeleton_unshared.milo) — the explosion signature. FINE instruments
        // (own-resource neck) never match, so they are never touched. Also pick the
        // rigid anchor (bone_bridge if present, else the bone with the SMALLEST
        // world-translation magnitude relative to the mesh centroid is not knowable
        // here, so prefer bone_bridge by name, then bone[0]).
        int nb = mesh->NumBones();
        bool onCharSkel = false;
        int anchorIdx = -1;
        for (int b = 0; b < nb; b++) {
            RndTransformable *bound = mesh->BoneTransAt(b);
            if (!bound) continue;
            ObjectDir *bd = bound->Dir();
            if (bd && !bd->mStoredFile.empty() &&
                std::strstr(bd->mStoredFile.c_str(), "skeleton_unshared.milo") != 0)
                onCharSkel = true;
            if (bound->Name() && std::strstr(bound->Name(), "bone_bridge") != 0)
                anchorIdx = b;
        }
        if (!onCharSkel) continue; // not a band-bound exploding strings mesh
        meshes++;
        if (anchorIdx < 0) {
            // no bone_bridge — fall back to bone[0] as the rigid anchor.
            for (int b = 0; b < nb && anchorIdx < 0; b++)
                if (mesh->BoneTransAt(b)) anchorIdx = b;
        }
        if (anchorIdx < 0) { pending++; continue; }
        RndTransformable *anchor = mesh->BoneTransAt(anchorIdx);
        // Sanity: the anchor (and, for rebake mode, every bone) must have a finite
        // world pose so Invert can't produce NaN (the engine clamp is disabled for
        // rebound meshes — no backstop).
        Vector3 av = anchor->WorldXfm().v;
        if (!(std::fabs(av.x) < 1e5f && std::fabs(av.y) < 1e5f && std::fabs(av.z) < 1e5f)) {
            pending++; continue;
        }
        if (sRigid) {
            // RIGID-ANCHOR: bind every bone to the single rigid anchor and rebake its
            // offset = meshWorld * inv(anchorWorld). All verts then ride `anchor`
            // rigidly -> world AABB == bind AABB (ratio ~1.0) through the bend.
            Transform invAnchor;
            Invert(anchor->WorldXfm(), invAnchor);
            for (int b = 0; b < nb; b++) {
                if (!mesh->BoneTransAt(b)) continue;
                if (mesh->BoneTransAt(b) != anchor)
                    mesh->SetBone(b, anchor, false);
                Multiply(mesh->WorldXfm(), invAnchor, mesh->BoneOffsetAt(b));
            }
        } else {
            // REBAKE-IN-PLACE: rest-rebake each bone against its own current pose
            // (mOffset = meshWorld * inv(boneWorld)). Coherent at this pose; follows
            // the bend (but the native animated basis divergence can re-grow it).
            bool ok = true;
            for (int b = 0; b < nb; b++) {
                RndTransformable *bt = mesh->BoneTransAt(b);
                if (!bt) continue;
                Vector3 bv = bt->WorldXfm().v;
                if (!(std::fabs(bv.x) < 1e5f && std::fabs(bv.y) < 1e5f &&
                      std::fabs(bv.z) < 1e5f)) { ok = false; break; }
            }
            if (!ok) { pending++; continue; }
            for (int b = 0; b < nb; b++) {
                RndTransformable *bt = mesh->BoneTransAt(b);
                if (!bt) continue;
                mesh->SetBone(b, bt, true);
            }
        }
        mesh->mNativeBonesRebound = true; // engine: skip rebake + fling-clamp
        reboundMeshes++;
        if (probe)
            fprintf(stderr,
                "[INST_REBIND] member='%s' mesh='%s' bones=%d mode=%s anchor[%d]='%s' "
                "anchorWp=(%.1f,%.1f,%.1f)\n",
                Name() ? Name() : "?", mn, nb, sRigid ? "rigid" : "rebake", anchorIdx,
                (anchor->Name() ? anchor->Name() : "?"), av.x, av.y, av.z);
    }

    // Latch when no in-scope strings mesh remains to rebind for a short window. Like
    // the other rebinds, keep a long give-up fallback to bound the rescan cost.
    if (reboundMeshes == 0) mNativeInstReboundQuiet++;
    else mNativeInstReboundQuiet = 0;
    if ((pending == 0 && mNativeInstReboundQuiet >= 30) ||
        mNativeInstReboundQuiet >= 600)
        mNativeInstReboundOnce = 1;
}

// W2.8 BL-A1 — rigid-anchor the hand/finger/glove skin meshes (see the header comment
// for the full mechanism + tradeoff). Mirrors the proven RebindInstStringsToRestBasis
// rigid path: repoint every bone (per L/R side) to that side's wrist bone and rebake
// offset = meshWorld * inv(wristWorld) so the hand rides its wrist as one rigid body,
// world AABB == bind AABB — no relative multi-bone basis error left to shard.
// DEFAULT-OFF (opt-in RB3_HANDS_POSEAWARE=1); flag-OFF is a getenv-cached early return.
void BandCharacter::NativeRepinHandsRigid() {
    static int sEnabled = -1;
    if (sEnabled < 0) sEnabled = getenv("RB3_HANDS_POSEAWARE") ? 1 : 0;
    if (!sEnabled) return; // flag-OFF: byte-identical no-op
    if (mNativeHandsRigidOnce) return;
    bool probe = getenv("HANDS_RIGID_PROBE") != 0;

    std::vector<RndMesh *> targets;
    NativeCollectSkinnedMeshes(targets);

    int meshes = 0, reboundMeshes = 0, pending = 0;
    std::vector<RndMesh *> ownersDone; // dedupe GeomOwner-shared meshes this pass
    for (std::vector<RndMesh *>::iterator mi = targets.begin();
         mi != targets.end(); ++mi) {
        RndMesh *drawn = *mi;
        const char *dn = drawn->Name();
        if (!dn) continue;
        // SCOPE: hand/finger/glove ONLY. "finger" also catches fingernails_*; bare
        // "nail" is deliberately excluded so footwear (nailboots_*) is never touched.
        // Head/hair/face are a separate problem (W2.7) — not in scope.
        std::string lname(dn);
        for (size_t i = 0; i < lname.size(); i++) {
            char c = lname[i];
            if (c >= 'A' && c <= 'Z') lname[i] = (char)(c + 32);
        }
        bool inScope = lname.find("hand") != std::string::npos ||
                       lname.find("finger") != std::string::npos ||
                       lname.find("glove") != std::string::npos;
        if (!inScope) continue;
        // Operate on the GPU palette source (GeomOwner) — writing a shared mesh's own
        // bone array has no GPU effect; flag the drawn mesh too so the engine skips its
        // fling-clamp. (Deliberately DO NOT skip mNativeBonesRebound: the head-rebind
        // above may have already bound this mesh with the sharding per-bone rebake, and
        // this pass exists to OVERWRITE that with the rigid anchor.)
        RndMesh *mesh = drawn->GeomOwner() ? drawn->GeomOwner() : drawn;
        if (std::find(ownersDone.begin(), ownersDone.end(), mesh) != ownersDone.end()) {
            drawn->mNativeBonesRebound = true; // propagate to the shared draw
            continue;
        }
        int nb = mesh->NumBones();
        if (nb == 0) continue;
        meshes++;
        // Per-side wrist anchor. Names: bone_L-hand / bone_R-hand (wrist), fingers
        // bone_L-index01 etc. Side = the char after "bone_" ('L'/'R'). Anchor per side =
        // the "*-hand" wrist bone if present, else the side bone that is TransParent-
        // ancestor of the most other side bones, else the first side bone.
        int anchorL = -1, anchorR = -1;      // preferred wrist by name
        int fallbackL = -1, fallbackR = -1;  // first resolvable bone per side
        int ancBestL = -1, ancBestR = -1, ancCntL = -1, ancCntR = -1;
        for (int b = 0; b < nb; b++) {
            RndTransformable *bt = mesh->BoneTransAt(b);
            if (!bt || !bt->Name()) continue;
            const char *bn = bt->Name();
            int side = 0; // -1 left, +1 right, 0 none
            const char *u = std::strstr(bn, "bone_");
            const char *tag = u ? u + 5 : bn;
            if (tag[0] == 'L' && tag[1] == '-') side = -1;
            else if (tag[0] == 'R' && tag[1] == '-') side = 1;
            else if (std::strstr(bn, "_L-") || std::strstr(bn, "-L-")) side = -1;
            else if (std::strstr(bn, "_R-") || std::strstr(bn, "-R-")) side = 1;
            if (side == 0) continue; // no clear side — leave untouched
            bool isWrist = std::strstr(bn, "-hand") != 0;
            if (side < 0) {
                if (fallbackL < 0) fallbackL = b;
                if (isWrist) anchorL = b;
            } else {
                if (fallbackR < 0) fallbackR = b;
                if (isWrist) anchorR = b;
            }
            // ancestor count within same side
            int cnt = 0;
            for (int c = 0; c < nb; c++) {
                if (c == b) continue;
                RndTransformable *ct = mesh->BoneTransAt(c);
                if (!ct || !ct->Name()) continue;
                const char *cn = ct->Name();
                int cside = 0;
                const char *cu = std::strstr(cn, "bone_");
                const char *ctag = cu ? cu + 5 : cn;
                if (ctag[0] == 'L' && ctag[1] == '-') cside = -1;
                else if (ctag[0] == 'R' && ctag[1] == '-') cside = 1;
                if (cside != side) continue;
                int guard = 0;
                for (RndTransformable *p = ct->TransParent(); p && guard < 64;
                     p = p->TransParent(), guard++)
                    if (p == bt) { cnt++; break; }
            }
            if (side < 0) { if (cnt > ancCntL) { ancCntL = cnt; ancBestL = b; } }
            else { if (cnt > ancCntR) { ancCntR = cnt; ancBestR = b; } }
        }
        if (anchorL < 0) anchorL = (ancBestL >= 0 ? ancBestL : fallbackL);
        if (anchorR < 0) anchorR = (ancBestR >= 0 ? ancBestR : fallbackR);
        if (anchorL < 0 && anchorR < 0) { pending++; continue; }
        // Resolve, per side, the anchor's CHAR-SPACE REST xfm (mNativeRestPose, seeded
        // clip-free by the head rebind / post-deform capture) and its LIVE per-member
        // bone (Find by name — the same instance the head rebind binds to). This is the
        // EXACT proven RebindHeadHandsAtRest formula (offset = meshWorld * inv(restChar),
        // bind to live bone), just COLLAPSED so every side bone shares its wrist's
        // mapping -> the whole hand rides the wrist rigidly with correct placement. Do
        // NOT use the anchor's CURRENT world (skinned meshWorld is identity, so
        // meshWorld*inv(worldNow) detaches the verts to bind-local -> a worse shard;
        // measured 593u). All-or-nothing: if a present side lacks a rest snapshot or a
        // live bone, defer the whole mesh (leave head-rebind's bind untouched).
        RndTransformable *ownL = 0, *ownR = 0;
        Transform invRestL, invRestR;
        bool needL = anchorL >= 0, needR = anchorR >= 0;
        bool okL = !needL, okR = !needR;
        if (needL) {
            RndTransformable *aL = mesh->BoneTransAt(anchorL);
            const char *anL = aL ? aL->Name() : 0;
            if (anL) {
                std::map<std::string, Transform>::iterator rp =
                    mNativeRestPose.find(std::string(anL));
                RndTransformable *own = Find<RndTransformable>(anL, false);
                if (rp != mNativeRestPose.end() && own) {
                    Vector3 v = rp->second.v;
                    if (std::fabs(v.x) < 1e5f && std::fabs(v.y) < 1e5f &&
                        std::fabs(v.z) < 1e5f) {
                        Invert(rp->second, invRestL); ownL = own; okL = true;
                    }
                }
            }
        }
        if (needR) {
            RndTransformable *aR = mesh->BoneTransAt(anchorR);
            const char *anR = aR ? aR->Name() : 0;
            if (anR) {
                std::map<std::string, Transform>::iterator rp =
                    mNativeRestPose.find(std::string(anR));
                RndTransformable *own = Find<RndTransformable>(anR, false);
                if (rp != mNativeRestPose.end() && own) {
                    Vector3 v = rp->second.v;
                    if (std::fabs(v.x) < 1e5f && std::fabs(v.y) < 1e5f &&
                        std::fabs(v.z) < 1e5f) {
                        Invert(rp->second, invRestR); ownR = own; okR = true;
                    }
                }
            }
        }
        if (!okL || !okR) { pending++; continue; }
        // Apply: every side bone -> that side's LIVE wrist bone, offset = meshWorld *
        // inv(wristRestChar). Bones with no clear side stay as bound (their finger
        // residual is left to head-rebind/clamp — rare in hand meshes).
        for (int b = 0; b < nb; b++) {
            RndTransformable *bt = mesh->BoneTransAt(b);
            if (!bt || !bt->Name()) continue;
            const char *bn = bt->Name();
            int side = 0;
            const char *u = std::strstr(bn, "bone_");
            const char *tag = u ? u + 5 : bn;
            if (tag[0] == 'L' && tag[1] == '-') side = -1;
            else if (tag[0] == 'R' && tag[1] == '-') side = 1;
            else if (std::strstr(bn, "_L-") || std::strstr(bn, "-L-")) side = -1;
            else if (std::strstr(bn, "_R-") || std::strstr(bn, "-R-")) side = 1;
            RndTransformable *anchorOwn = (side < 0) ? ownL : (side > 0) ? ownR : 0;
            const Transform *invRest = (side < 0) ? &invRestL : (side > 0) ? &invRestR : 0;
            if (!anchorOwn || !invRest) continue;
            if (bt != anchorOwn) mesh->SetBone(b, anchorOwn, false);
            Multiply(mesh->WorldXfm(), *invRest, mesh->BoneOffsetAt(b));
        }
        mesh->mNativeBonesRebound = true;   // engine: skip rebake + fling-clamp
        drawn->mNativeBonesRebound = true;
        ownersDone.push_back(mesh);
        reboundMeshes++;
        if (probe)
            fprintf(stderr,
                "[HANDS_RIGID] member='%s' mesh='%s' bones=%d anchorL[%d]='%s' "
                "anchorR[%d]='%s'\n",
                Name() ? Name() : "?", mesh->Name() ? mesh->Name() : "?", nb,
                anchorL, (ownL && ownL->Name()) ? ownL->Name() : "-",
                anchorR, (ownR && ownR->Name()) ? ownR->Name() : "-");
    }

    // Latch like the sibling rebinds: quiet window with nothing left, or a long
    // give-up fallback; re-armed on StartLoad/SyncObjects.
    if (reboundMeshes == 0) mNativeHandsRigidQuiet++;
    else mNativeHandsRigidQuiet = 0;
    if ((pending == 0 && mNativeHandsRigidQuiet >= 30) ||
        mNativeHandsRigidQuiet >= 600)
        mNativeHandsRigidOnce = 1;

    if (probe && (reboundMeshes > 0 || pending > 0))
        fprintf(stderr,
            "[HANDS_RIGID] member='%s' meshes=%d reboundMeshes=%d pending=%d "
            "quiet=%d latched=%d\n",
            Name() ? Name() : "?", meshes, reboundMeshes, pending,
            mNativeHandsRigidQuiet, mNativeHandsRigidOnce);
}

// W2.8c helper (plan Section 2b): force a bone's whole TransParent chain root->leaf so
// the world we sample in Poll matches the palette's post-force world (the default-ON
// RB3_NO_SKEL_WORLDFIX draw pass recomputes bone worlds AFTER Poll; a stale Poll-time
// read would make the conjugation cancel against the wrong world and re-introduce a
// residual). WorldXfm_Force recomputes mWorldXfm from mParent->WorldXfm() unconditionally,
// so forcing root->leaf leaves every link finite-and-current. Dedupe via `forced` (as the
// engine pass does with sForced) to keep cost bounded across the mesh's bones.
static void NativeForceBoneChain(RndTransformable *bone,
                                 std::set<RndTransformable *> &forced) {
    if (!bone) return;
    RndTransformable *chain[64];
    int n = 0;
    for (RndTransformable *p = bone; p && n < 64; p = p->TransParent()) chain[n++] = p;
    // chain[0]=leaf .. chain[n-1]=root; force root->leaf
    for (int i = n - 1; i >= 0; i--)
        if (forced.insert(chain[i]).second) chain[i]->WorldXfm_Force();
}

// W2.8c — per-frame pose-aware appendage (hands/fingers) basis correction. See the
// header comment for the full mechanism + the math derivation. Unlike the sibling
// rebinds this pass is UNLATCHED: it runs every Poll, re-sampling each claimed bone's
// live world L_b(t) and rewriting its palette offset so the live motion is conjugated
// into the authored magnet frame — cancelling the growing R*sin(theta) twist while each
// finger still rides its OWN bone (articulation preserved). Anchors A_b and L_b(t0) are
// captured ONCE per mesh at claim (mNativeHandsConj), so there is no per-frame drift.
// DEFAULT-OFF (opt-in RB3_HANDS_PERFRAME_CONJ=1); flag-OFF is a getenv-cached early
// return (byte-identical no-op). Mutually exclusive with NativeRepinHandsRigid.
void BandCharacter::NativeConjHandsPerFrame() {
    static int sEnabled = -1;
    if (sEnabled < 0) sEnabled = getenv("RB3_HANDS_PERFRAME_CONJ") ? 1 : 0;
    if (!sEnabled) return; // flag-OFF: byte-identical no-op
    bool probe = getenv("HANDS_CONJ_PROBE") != 0;

    // ---- Phase A (pre-latch): claim newly-reachable hand/finger/glove meshes. ----
    // Capture the pristine authored invBind (A_b) + live rest world (L_b(t0)) once,
    // bind each bone to its OWN live per-member bone (SetBone, per bone — NOT collapsed
    // to a wrist), then hand the mesh to the per-frame apply below. All-or-nothing per
    // mesh (like RebindHeadHandsAtRest): if any in-scope slot can't resolve a finite
    // live world, defer the whole mesh and retry next Poll.
    int claimed = 0, pending = 0;
    if (!mNativeHandsConjOnce) {
        std::vector<RndMesh *> targets;
        NativeCollectSkinnedMeshes(targets);
        std::vector<RndMesh *> ownersDone; // dedupe GeomOwner-shared draws this pass
        for (std::vector<RndMesh *>::iterator mi = targets.begin();
             mi != targets.end(); ++mi) {
            RndMesh *drawn = *mi;
            const char *dn = drawn->Name();
            if (!dn) continue;
            // SCOPE: hand/finger/glove ONLY (same scope as NativeRepinHandsRigid;
            // "finger" catches fingernails_*, "nail" is deliberately NOT matched so
            // footwear is never touched). Head/hair/face are W2.7's problem.
            std::string lname(dn);
            for (size_t i = 0; i < lname.size(); i++) {
                char c = lname[i];
                if (c >= 'A' && c <= 'Z') lname[i] = (char)(c + 32);
            }
            bool inScope = lname.find("hand") != std::string::npos ||
                           lname.find("finger") != std::string::npos ||
                           lname.find("glove") != std::string::npos;
            if (!inScope) continue;
            // Hazard 2a: the draw reads owner->BoneOffsetAt(b); write the palette source.
            RndMesh *owner = drawn->GeomOwner() ? drawn->GeomOwner() : drawn;
            if (mNativeHandsConj.find(owner) != mNativeHandsConj.end()) {
                drawn->mNativeBonesRebound = true; // shared draw of an already-claimed owner
                continue;
            }
            if (std::find(ownersDone.begin(), ownersDone.end(), owner) !=
                ownersDone.end()) {
                drawn->mNativeBonesRebound = true;
                continue;
            }
            ownersDone.push_back(owner);
            int nb = owner->NumBones();
            if (nb == 0) continue;
            // Hazard 2b: force every referenced bone's chain BEFORE sampling worlds.
            std::set<RndTransformable *> forced;
            for (int b = 0; b < nb; b++) {
                RndTransformable *bound = owner->BoneTransAt(b);
                if (!bound || !bound->Name()) continue;
                RndTransformable *own = Find<RndTransformable>(bound->Name(), false);
                if (own) NativeForceBoneChain(own, forced);
            }
            // Pass 1: resolve + validate every slot; anchors captured but NOT applied.
            NativeHandsConjEntry e;
            e.ownBone.assign((size_t)nb, (RndTransformable *)0);
            e.A.resize((size_t)nb);
            e.pre.resize((size_t)nb);
            e.valid.assign((size_t)nb, 0);
            bool ok = true;
            int slots = 0;
            for (int b = 0; b < nb; b++) {
                RndTransformable *bound = owner->BoneTransAt(b);
                if (!bound || !bound->Name()) continue; // empty slot: engine uses identity
                slots++;
                RndTransformable *own = Find<RndTransformable>(bound->Name(), false);
                if (!own) { ok = false; break; } // can't resolve live bone: defer mesh
                Transform L0 = own->WorldXfm(); // forced above
                if (!(std::fabs(L0.v.x) < 1e5f && std::fabs(L0.v.y) < 1e5f &&
                      std::fabs(L0.v.z) < 1e5f)) {
                    ok = false; break; // non-finite live rest world: defer
                }
                // offA = pristine authored invBind (RebindHeadHandsAtRest skipped this
                // mesh under the flag, so BoneOffsetAt is unmutated). A_b = inv(offA).
                Transform offA = owner->BoneOffsetAt(b);
                Transform A;
                Invert(offA, A);
                Transform invL0;
                Invert(L0, invL0);
                Transform pre;
                Multiply(offA, invL0, pre); // pre = offA * inv(L0)   (offA == inv(A_b))
                e.ownBone[b] = own;
                e.A[b] = A;
                e.pre[b] = pre;
                e.valid[b] = 1;
            }
            if (!ok || slots == 0) { pending++; continue; } // untouched; retry next Poll
            // Pass 2 commit: bind each valid bone to its own live bone (per bone). The
            // per-frame offset is written by Phase B below (this same Poll).
            for (int b = 0; b < nb; b++) {
                if (!e.valid[b]) continue;
                if (owner->BoneTransAt(b) != e.ownBone[b])
                    owner->SetBone(b, e.ownBone[b], false);
            }
            mNativeHandsConj[owner] = e;
            owner->mNativeBonesRebound = true; // engine: skip rebake + fling-clamp
            drawn->mNativeBonesRebound = true;
            claimed++;
            if (probe) {
                int nvalid = 0;
                for (int b = 0; b < nb; b++) nvalid += e.valid[b];
                fprintf(stderr,
                        "[HANDS_CONJ] CLAIM member='%s' mesh='%s' owner='%s' bones=%d "
                        "perBoneSlots=%d (per-bone, NOT wrist-collapsed)\n",
                        Name() ? Name() : "?", dn,
                        owner->Name() ? owner->Name() : "?", nb, nvalid);
            }
        }
        // Latch the CLAIM scan (not the per-frame apply): once nothing new is claimable
        // for a quiet window, stop re-walking. Re-armed at SyncObjects.
        if (claimed == 0) mNativeHandsConjQuiet++;
        else mNativeHandsConjQuiet = 0;
        if ((pending == 0 && mNativeHandsConjQuiet >= 30) ||
            mNativeHandsConjQuiet >= 600)
            mNativeHandsConjOnce = 1;
    }

    // ---- Phase B (EVERY Poll): re-apply the per-frame conjugated offset. ----
    // For each claimed owner, re-sample L_b(t) (forced) and recompute
    //   offset_b(t) = pre * L_b(t) * A * inv(L_b(t))   ( = inv(A_b)*inv(L0)*L(t)*A_b*inv(L(t)) )
    // so skin_b = offset*L(t) = inv(A_b)*inv(L0)*L(t)*A_b : the live motion since t0
    // rebased onto the authored joint axis (no R*sin(theta) twist), fingers articulating.
    for (std::map<RndMesh *, NativeHandsConjEntry>::iterator it =
             mNativeHandsConj.begin();
         it != mNativeHandsConj.end(); ++it) {
        RndMesh *owner = it->first;
        NativeHandsConjEntry &e = it->second;
        int nb = owner->NumBones();
        if (nb > (int)e.valid.size()) nb = (int)e.valid.size();
        std::set<RndTransformable *> forced;
        for (int b = 0; b < nb; b++)
            if (e.valid[b] && e.ownBone[b]) NativeForceBoneChain(e.ownBone[b], forced);
        float worst = 0.0f;
        const char *worstBone = "?";
        for (int b = 0; b < nb; b++) {
            if (!e.valid[b] || !e.ownBone[b]) continue;
            Transform Lt = e.ownBone[b]->WorldXfm(); // forced above
            if (!(std::fabs(Lt.v.x) < 1e5f && std::fabs(Lt.v.y) < 1e5f &&
                  std::fabs(Lt.v.z) < 1e5f))
                continue; // non-finite this frame: keep last good offset
            Transform invLt;
            Invert(Lt, invLt);
            Transform tmp1, tmp2;
            Multiply(e.pre[b], Lt, tmp1);   // pre * L(t)
            Multiply(tmp1, e.A[b], tmp2);   // * A_b
            Multiply(tmp2, invLt, owner->BoneOffsetAt(b)); // * inv(L(t)) -> offset_b(t)
            if (probe) {
                // wext-style read: composed skin translation vs the bone itself. For a
                // correct conjugation this stays bounded by the appendage extent; the
                // rigid-anchor dead end blew it to 200-460u.
                Transform skin;
                Multiply(owner->BoneOffsetAt(b), Lt, skin);
                Vector3 d;
                Subtract(skin.v, Lt.v, d);
                float dd = Length(d);
                if (dd > worst) {
                    worst = dd;
                    RndTransformable *bt = owner->BoneTransAt(b);
                    worstBone = (bt && bt->Name()) ? bt->Name() : "?";
                }
            }
        }
        if (probe) {
            static std::map<std::string, int> sSeen; // throttle per member/mesh
            std::string key = std::string(Name() ? Name() : "?") + "/" +
                              (owner->Name() ? owner->Name() : "?");
            if (sSeen[key]++ % 120 == 0)
                fprintf(stderr,
                        "[HANDS_CONJ] APPLY member='%s' mesh='%s' worstBone='%s' "
                        "skinToBone=%.2fu (rigid-anchor dead end was 200-460u)\n",
                        Name() ? Name() : "?", owner->Name() ? owner->Name() : "?",
                        worstBone, worst);
        }
    }

    if (probe && (claimed > 0 || pending > 0))
        fprintf(stderr,
                "[HANDS_CONJ] member='%s' claimed=%d pending=%d tracked=%d quiet=%d "
                "latched=%d\n",
                Name() ? Name() : "?", claimed, pending, (int)mNativeHandsConj.size(),
                mNativeHandsConjQuiet, mNativeHandsConjOnce);
}
#endif

#pragma push
#pragma pool_data off
void BandCharacter::SyncObjects() {
    unk6b0 = Find<CharWeightable>("lod0.weight", false);
    static const char *bones[8] = { "bone_pelvis.mesh", "bone_prop0.mesh",
                                    "bone_prop1.mesh",  "bone_prop2.mesh",
                                    "bone_prop3.mesh",  "spot_neck.mesh",
                                    "spot_navel.mesh",  "bone_mic_stand_bottom.mesh" };
#ifdef HX_NATIVE
    // `bones` has no null sentinel: the matched loop walks until `*ptr == 0`,
    // relying on the static datum *after* the 8-element array being zero on the
    // Wii image. Under clang LP64 that adjacent storage is arbitrary, so the
    // loop reads bones[8] (OOB) as a garbage non-null pointer and crashes in
    // Find(). Bound the walk to the array's 8 known elements (same iteration set).
    for (const char **ptr = bones; ptr != bones + 8; ptr++) {
#else
    for (const char **ptr = bones; *ptr != 0; ptr++) {
#endif
        RndTransformable *t = Find<RndTransformable>(*ptr, false);
        if (t)
            t->SetTransParent(this, false);
    }
    SetDeformation();
#ifdef HX_NATIVE
    // render-polish 2026-06-11 (char-render steps 2+3): SetDeformation() has just
    // posed the skeleton at the weighted gender-bind REST pose — the one
    // deterministic rest point in the flow. (a) Seed the rest-pose snapshot for any
    // newly-resolvable per-member bones (a mid-song merge's meshes then bake
    // against true rest, not a mid-clip Poll pose). (b) Re-arm both Poll-time
    // rebind latches: a SyncObjects pass means merge/deform state changed (meshes
    // re-stuffed / re-skinned), so the next Polls must re-scan for new or
    // re-merged meshes (idempotent for meshes already rebound, which keep
    // RndMesh::mNativeBonesRebound).
    // frame-stall 2026-06-20 (TRACK A): a SyncObjects pass means the dir tree /
    // mesh set was re-stuffed — invalidate the skinned-mesh cache BEFORE the
    // rest-pose seeding (NativeCaptureRestPoseAfterDeform calls
    // NativeCollectSkinnedMeshes, which then rewalks once into the fresh cache).
    NativeInvalidateSkinnedMeshCache();
    NativeCaptureRestPoseAfterDeform();
    mNativeReboundOnce = 0;
    mNativeReboundQuiet = 0;
    mNativeHeadReboundOnce = 0;
    mNativeHeadReboundQuiet = 0;
    // wave-inststrings: re-arm the instrument-strings rebind too (a merge/deform
    // change may have re-stuffed/re-skinned mInstDir's strings mesh).
    mNativeInstReboundOnce = 0;
    mNativeInstReboundQuiet = 0;
    // W2.8 BL-A1: re-arm the rigid-anchor hands rebind (a merge/deform may have
    // re-skinned the hand meshes back onto the sharding per-member bind).
    mNativeHandsRigidOnce = 0;
    mNativeHandsRigidQuiet = 0;
    // W2.8c: drop the per-frame conjugation state (its RndMesh*/RndTransformable*
    // anchors become stale when the mesh set is re-stuffed) and re-arm the claim scan.
    // SetDeformation() just posed the per-member skeleton at the weighted gender-bind
    // REST pose, so the next claim captures L_b(t0) at true rest (the ideal anchor).
    mNativeHandsConj.clear();
    mNativeHandsConjOnce = 0;
    mNativeHandsConjQuiet = 0;
#endif
    RndMat *feetmat = Find<RndMat>("feet_socks_skin.mat", false);
    if (feetmat) {
        RndMat *legmat = mOutfitDir->Find<RndMat>("legs_socks_swap.mat", false);
        if (legmat)
            feetmat->Copy(legmat, kCopyDeep);
        else {
            RndMat *skinmat = Find<RndMat>("feet_skin.mat", false);
            if (skinmat)
                feetmat->Copy(skinmat, kCopyDeep);
        }
    }

    unk5e0.sort(ByRadius());
    //   iVar4 = *(int *)(this + 0x5e4);
    //   if ((iVar4 != 0) && (*(int *)(iVar4 + 4) != 0)) {
    //     piVar12 = *(int **)(iVar4 + 8);
    //     for (piVar3 = (int *)piVar12[2]; piVar11 = piVar3, piVar3 != piVar12; piVar3 =
    //     (int *)piVar3[2 ])
    //     {
    //       for (; piVar11 != piVar12; piVar11 = (int *)piVar11[1]) {
    //         iVar4 = *piVar11;
    //         iVar5 = *(int *)piVar11[1];
    //         if (*(float *)(iVar5 + 0x178) <= *(float *)(iVar4 + 0x178)) break;
    //         *piVar11 = iVar5;
    //         *(int *)piVar11[1] = iVar4;
    //       }
    //     }
    //   }

    for (ObjPtrList<CharHair, ObjectDir>::iterator it = unk5f0.begin();
         it != unk5f0.end();
         ++it) {
        (*it)->Hookup(unk5e0);
    }
    Character::SyncObjects();
#ifdef HX_NATIVE
    // SKEL_REBIND (wave-06): DIAGNOSTIC ONLY, default OFF. A per-member skin-bone
    // rebind onto whatever Find<RndTransformable>(boneName) resolves to in THIS
    // member's dir tree. PROVEN A NO-OP this wave: Find from the BandCharacter dir
    // returns the SAME shared char/main/skeleton.milo magnet the outfit meshes are
    // already bound to (SKEL_REBIND_PROBE: reboundDiff=0 same=4; only ONE
    // bone_R-upperArm instance reachable in the member subtree). There is no live,
    // per-member, female-posed skeleton to rebind to — the char pose pipeline
    // (CharUtlFindBoneTrans -> dir->Find) also resolves to that one magnet, which is
    // STATIC (never animated). So the faithful "rebind to own live skeleton" path is
    // not reachable without a deep, crowd-affecting loader un-share. The shipped fix
    // is the renderer-side static-pose offset rebake (Rnd_Wgpu_RB3.cpp SKEL_REBAKE).
    // This block stays purely as a probe (enable with SET_SKEL_REBIND=1). The Wii
    // path is byte-identical (HX_NATIVE).
    {
        static int sRebind = -1;
        if (sRebind < 0) sRebind = getenv("SET_SKEL_REBIND") ? 1 : 0;
        bool probe = getenv("SKEL_REBIND_PROBE") != 0;
        if (sRebind || probe) {
        int meshes = 0, rebound = 0, same = 0, nullown = 0, slots = 0, logged = 0;
        for (ObjDirItr<RndMesh> mit(this, true); mit != 0; ++mit) {
            RndMesh *mesh = mit;
            if (!mesh || mesh->NumBones() == 0) continue;
            meshes++;
            for (int b = 0; b < mesh->NumBones(); b++) {
                RndTransformable *bound = mesh->BoneTransAt(b);
                if (!bound || !bound->Name()) continue;
                slots++;
                RndTransformable *own = Find<RndTransformable>(bound->Name(), false);
                if (!own) { nullown++; continue; }
                if (own != bound) {
                    if (probe && logged < 6) {
                        fprintf(stderr,
                            "[SKEL_REBIND] member='%s' mesh='%s' bone='%s' bound=%p own=%p REBIND\n",
                            Name() ? Name() : "?", mesh->Name() ? mesh->Name() : "?",
                            bound->Name(), (void *)bound, (void *)own);
                        logged++;
                    }
                    if (sRebind) mesh->SetBone(b, own, false);
                    rebound++;
                } else {
                    same++;
                }
            }
        }
        // Probe whether any per-member skeleton instance distinct from the bound
        // magnet exists in this member's subtree (for the key arm bone).
        if (probe && meshes > 0) {
            int distinct = 0; void *seen[16]; int ns = 0;
            for (ObjDirItr<RndTransformable> tit(this, true); tit != 0; ++tit) {
                RndTransformable *t = tit;
                if (!t || !t->Name() || strstr(t->Name(), "bone_R-upperArm") == 0)
                    continue;
                bool dup = false;
                for (int k = 0; k < ns; k++) if (seen[k] == (void *)t) dup = true;
                if (!dup && ns < 16) { seen[ns++] = (void *)t; distinct++;
                    ObjectDir *d = t->Dir();
                    fprintf(stderr,
                        "[SKEL_REBIND]   upperArm instance=%p dirFile='%s'\n", (void *)t,
                        (d && !d->mStoredFile.empty()) ? d->mStoredFile.c_str() : "-");
                }
            }
            fprintf(stderr,
                "[SKEL_REBIND]   distinct upperArm instances in member subtree=%d\n",
                distinct);
        }
        if (probe) {
            fprintf(stderr,
                "[SKEL_REBIND] member='%s' path='%s' skinMeshes=%d slots=%d reboundDiff=%d same=%d nullOwn=%d\n",
                Name() ? Name() : "?", PathName(this), meshes, slots, rebound, same, nullown);
        }
        } // if (sRebind || probe)
    }
#endif
    for (ObjPtrList<CharBoneOffset, ObjectDir>::iterator it = unk640.begin();
         it != unk640.end();
         ++it) {
        (*it)->ApplyToLocal();
        mOutfitDir->RemoveFromPoll(*it);
    }
    RemoveDrawAndPoll(mOutfitDir);
    RemoveDrawAndPoll(mInstDir);
    if (!mInCloset) {
        for (ObjPtrList<OutfitConfig, ObjectDir>::iterator it = unk620.begin();
             it != unk620.end();
             ++it) {
            (*it)->CompressTextures();
        }
        while (!unk610.empty()) {
            RndMeshDeform *df = unk610.front();
            if (!df->Mesh())
                MILO_FAIL("BandCharacter::SyncObjects() - character missing mesh data.");
            df->Mesh()->SetKeepMeshData(false);
            delete df;
        }
        while (!unk600.empty()) {
            delete unk600.front();
        }
        for (ObjPtrList<CharCollide, ObjectDir>::iterator it = unk5e0.begin();
             it != unk5e0.end();
             ++it) {
            (*it)->ClearMesh();
        }
    }
    CharMeshHide::HideAll(unk5b0, -(mDriver->ClipType() == "vignette") & 0x2000);
    if (InVignetteOrCloset()) {
        CharClipDriver *first = mDriver->FirstPlaying();
        if (first && mGroupName[0] != 0) {
            int mask = mGender == "male" ? 0x20 : 0x40;
            CharClipDriver *fp = mDriver->FirstPlaying();
            if (!(fp->GetClip()->mFlags & mask)) {
                float frame = fp->GetClip()->BeatToFrame(fp->mBeat);
                CharClipDriver *result = PlayMainClip(2, false);
                if (result) {
                    result->mBeat = result->GetClip()->FrameToBeat(frame);
                }
            }
        }
    }
    const char *eyedfname =
        mGender == "male" ? "eyesdeform_male.anim" : "eyesdeform_female.anim";
    RndPropAnim *panim = Find<RndPropAnim>(eyedfname, false);
    if (panim) {
        int numeyeshapes = BandHeadShaper::sEyeNum;
        if ((int)panim->EndFrame() != numeyeshapes)
            MILO_NOTIFY_ONCE(
                "%s must have a frame for each eye shape.  It currently has %d frames, but there are %d eye shapes",
                eyedfname,
                (int)panim->EndFrame(),
                BandHeadShaper::sEyeNum
            );
        if (!DataVariable("eyetweaker.loadedsettings").Int()) {
            panim->SetFrame(mHead.mEye, 1.0f);
        }
    } else
        MILO_NOTIFY_ONCE(
            "Can't find eye settings prop anim %s. This is required to set range of motion and lid tracking for each eye shape.",
            eyedfname
        );
}
#pragma pop

float sDrawOrder = -1.0f;

void BandCharacter::SetClipTypes(Symbol s1, Symbol s2) {
    if (mDriver) {
        mDriver->SetClipType(s2);
        if (BoneServo()) {
            BoneServo()->SetClipType(s1);
        }
    }
}

SAVE_OBJ(BandCharacter, 0x3EB)

BEGIN_LOADS(BandCharacter)
    PreLoad(bs);
    PostLoad(bs);
END_LOADS

void BandCharacter::PreLoad(BinStream &bs) {
    LOAD_REVS(bs);
    ASSERT_REVS(8, 0);
    Character::PreLoad(bs);
    int hashsize = (mHashTable.mNumEntries + 20) * 2;
    int strsize = mStringTable.UsedSize();
    Reserve(hashsize, strsize + 440);
}

void BandCharacter::PostLoad(BinStream &bs) {
    Character::PostLoad(bs);
    if (gLoadingProxyFromDisk) {
        BandCharDescTest test;
        test.Load(bs);
    } else
        BandCharDesc::Load(bs);
    bs >> mPlayFlags;
    bs >> mTempo;
    if (gRev < 6) {
        if (gRev < 4) {
            int i;
            bs >> i;
            if (gRev < 3) {
                Symbol s;
                bs >> s;
            }
        }
        Symbol s;
        bs >> s;
    }
    if (gRev > 6)
        bs >> mDrumVenue;
    if (gRev != 0)
        mTestPrefab.Load(bs, true, BandCharDesc::GetPrefabs());
    if (gRev == 2 || gRev == 3 || gRev == 4) {
        bool b;
        bs >> b;
    }
    if (gRev > 7) {
        if (gLoadingProxyFromDisk) {
            Symbol s;
            bs >> s;
        } else
            bs >> mInstrumentType;
    }
}

BEGIN_COPYS(BandCharacter)
    COPY_SUPERCLASS(Character)
    COPY_SUPERCLASS(BandCharDesc)
    CREATE_COPY(BandCharacter)
    BEGIN_COPYING_MEMBERS
        COPY_MEMBER(mPlayFlags)
        COPY_MEMBER(mTempo)
        COPY_MEMBER(mDrumVenue)
        COPY_MEMBER(mTestPrefab)
        COPY_MEMBER(mInstrumentType)
    END_COPYING_MEMBERS
END_COPYS

void BandCharacter::CollideList(const Segment &seg, std::list<Collision> &colls) {
    if (CollideSphere(seg)) {
        if (IsProxy())
            RndDrawable::CollideList(seg, colls);
        else {
            if (mOutfitDir)
                mOutfitDir->CollideListSubParts(seg, colls);
            if (mInstDir)
                mInstDir->CollideListSubParts(seg, colls);
            RndDir::CollideList(seg, colls);
        }
    }
}

RndDrawable *BandCharacter::CollideShowing(const Segment &s, float &f, Plane &pl) {
    if (mOutfitDir->CollideShowing(s, f, pl))
        return this;
    else
        return RndDir::CollideShowing(s, f, pl);
}

void BandCharacter::DrawShowing() {
    if (!unk6bd || !IsLoading()) {
        auto _tmp0 = DataVariable("bandcharacter.show_spheres").Int();
        if (_tmp0) {
            Sphere debugSphere(Vector3(0.0f, 0.0f, 5.0f), 45.0f);
            Multiply(debugSphere, mSphereBase->WorldXfm(), debugSphere);
            Hmx::Color red(1.0f, 0.0f, 0.0f, 1.0f);
            UtilDrawSphere(debugSphere.center, debugSphere.radius, red);
            if (mInstDir) {
                Sphere instSphere;
                mInstDir->MakeWorldSphere(instSphere, false);
                Hmx::Color green(0.0f, 1.0f, 0.0f, 1.0f);
                UtilDrawSphere(instSphere.center, instSphere.GetRadius(), green);
            }
            Hmx::Color blue(0.0f, 0.0f, 1.0f, 1.0f);
            UtilDrawSphere(mBounding.center, mBounding.GetRadius(), blue);
        }
        Character::DrawShowing();
        static const DataNode &n = DataVariable("bandcharacter.show_slot");
        if (n.Int()) {
            const Transform &headxfm = CharUtlFindBoneTrans("bone_head", this)->WorldXfm();
            Vector3 headPos;
            headPos.x = headxfm.v.x;
            headPos.y = headxfm.v.y;
            headPos.z = headxfm.v.z + 6.0f;
            Vector2 screenPos;
            float depth = RndCam::sCurrent->WorldToScreen(headPos, screenPos);
            if (depth > 0.0f) {
                const char *dirName = Name();
                BandWardrobe::TargetNames *targetNames;
                int _tmp7 = strlen(dirName);
                int charPos = dirName[_tmp7 - 1] - '0';
                if (InVignetteOrCloset()) {
                    targetNames = &TheBandWardrobe->mVignetteNames;
                } else {
                    targetNames = &TheBandWardrobe->mVenueNames;
                }
                Symbol nameSym(Name());
                int slot = 0;
                if (targetNames->names[0] != nameSym) {
                    slot = 1;
                    if (targetNames->names[1] != nameSym) {
                        slot = 2;
                        if (targetNames->names[2] != nameSym) {
                            slot = 3;
                            if (targetNames->names[3] != nameSym) {
                                slot = 4;
                            }
                        }
                    }
                }
                const char *text = MakeString("slot%d pos%d", slot, charPos);
                screenPos.x *= (float)TheRnd->mWidth;
                screenPos.y *= (float)TheRnd->mHeight;
                Hmx::Color white(1.0f, 1.0f, 1.0f, 1.0f);
                Vector2 &end = TheRnd->DrawString(text, screenPos, white, false);
                screenPos.x = -(0.5f * (end.x - screenPos.x) - screenPos.x);
                Hmx::Color white2(1.0f, 1.0f, 1.0f, 1.0f);
                TheRnd->DrawString(text, screenPos, white2, true);
            }
        }
    }
}

void BandCharacter::Teleport(Waypoint *way) {
    Character::Teleport(way);
    unk594 = way;
    if (mOutfitDir)
        mOutfitDir->mTeleported = true;
}

void BandCharacter::SetTempoGenreVenue(Symbol s1, Symbol s2, const char *cc) {
    mTempo = s1;
    mGenre = s2;
    mDrumVenue = NameToDrumVenue(cc);
    if (strstr(cc, "big_club"))
        mTestTourEndingVenue = "big_club";
    else if (strstr(cc, "arena"))
        mTestTourEndingVenue = "arena";
    else if (strstr(cc, "festival"))
        mTestTourEndingVenue = "festival";
}

void BandCharacter::DrawLodOrShadowMode(int i, DrawMode mode) {
    Character::DrawLodOrShadow(i, mode);
    if (mode == kCharDrawTranslucent) {
        mOutfitDir->DrawLodOrShadow(i, mode);
        if (!unk574)
            mInstDir->DrawLodOrShadow(i, mode);
    } else {
        if (!unk574)
            mInstDir->DrawLodOrShadow(i, mode);
        mOutfitDir->DrawLodOrShadow(i, mode);
    }
}

void BandCharacter::DrawLodOrShadow(int i, Character::DrawMode mode) {
    RndEnvironTracker tracker(mEnv, &WorldXfm().v);
    mInstDir->SetEnv(nullptr);
    mOutfitDir->SetEnv(nullptr);
    if (mode & 5) {
        DrawLodOrShadowMode(i, (DrawMode)(mode & 0xfffffffd));
    }
    if (mode & 2) {
        DrawLodOrShadowMode(i, (DrawMode)2);
    }
}

float BandCharacter::ComputeScreenSize(RndCam *cam) {
    if (mOutfitDir)
        return mOutfitDir->ComputeScreenSize(cam);
    else
        return 0;
}

bool BandCharacter::IsLoading() {
    if (mCompressedTextureIDs.size() != 0)
        return true;
    if (mFileMerger)
        return !mFileMerger->mFilesPending.empty();
    return false;
}

void BandCharacter::StartLoad(bool b1, bool b2, bool b3) {
    bool b4 = false;
    bool &_ref0 = mInCloset;
    if (!_ref0) {
        if (b2 || (unk224 & 7))
            b4 = true;
    }
    unk5a1 = b4;
    bool bvar1 = _ref0;
    _ref0 = b2;
    if (bvar1 && !mInCloset)
        b3 = true;
    if (!IsLoading() || !unk6bd || b3) {
        b4 = false;
        if (unk5a1 || b3)
            b4 = true;
        unk6bd = b4;
    }

#ifdef HX_NATIVE
    if (NativeReloadProbe())
        fprintf(stderr,
                "[STARTLOAD] poll=%d char='%s' caller='%s' inCloset(was->now)=%d->%d "
                "b1=%d b3=%d restPose=%d headLatched=%d torsoLatched=%d\n",
                gNativeBandCharPollSerial, Name() ? Name() : "?", gNativeStartLoadTag,
                (int)bvar1, (int)mInCloset, (int)b1, (int)b3,
                (int)mNativeRestPose.size(), mNativeHeadReboundOnce,
                mNativeReboundOnce);
    gNativeStartLoadTag = "?";
    // Re-arm the native skinning rebinds on every (re)load. The CharCache reuses one
    // persistent BandCharacter per slot across closet/salon outfit edits, so a new
    // head/torso mesh streamed by an outfit change has mNativeBonesRebound=false but
    // the Poll-time rebinds have already LATCHED (mNative*ReboundOnce) and would skip
    // it -> it shards. Un-latch so new meshes get scanned (already-rebound meshes are
    // skipped via RndMesh::mNativeBonesRebound, so the re-scan is idempotent).
    mNativeReboundOnce = 0;
    mNativeReboundQuiet = 0;
    mNativeHeadReboundOnce = 0;
    mNativeHeadReboundQuiet = 0;
    // frame-stall 2026-06-20 (TRACK A): a (re)load re-stuffs the dir tree, so the
    // cached skinned-mesh list may be stale — invalidate it so the next Poll's
    // collect re-walks once (and the dynamic_cast/draw-tree cost is paid once, not
    // every Poll). Same trigger as the rebind-latch re-arm above.
    NativeInvalidateSkinnedMeshCache();
    // wave-inststrings: re-arm the instrument-strings rebind on StartLoad too (an
    // instrument swap re-stuffs mInstDir's strings mesh; already-rebound meshes keep
    // RndMesh::mNativeBonesRebound so the re-scan is idempotent).
    mNativeInstReboundOnce = 0;
    mNativeInstReboundQuiet = 0;
    // W2.8 BL-A1: re-arm the rigid-anchor hands rebind on StartLoad too.
    mNativeHandsRigidOnce = 0;
    mNativeHandsRigidQuiet = 0;
    // W2.8c: drop the per-frame conjugation anchors on StartLoad too — a reload can
    // re-stuff/destroy the hand meshes, so the cached mesh/bone pointers must not
    // outlive them. Re-armed; re-claim recaptures. (A mid-song StartLoad recaptures
    // L_b(t0) at the then-current pose rather than true rest — a second-order residual
    // per plan Section 1.5, not the dominant growing twist, which is still cancelled.)
    mNativeHandsConj.clear();
    mNativeHandsConjOnce = 0;
    mNativeHandsConjQuiet = 0;
    // render-polish 2026-06-11 (char-render): do NOT blanket-clear the per-member
    // rest-pose snapshot here. StartLoad re-fires repeatedly through the gameplay
    // flow (venue clip/dircut loads, patch recompose, scripts — 5-6x per member,
    // including mid-song) while the per-member skeleton BONES persist mid-animation
    // (scout-char-render §2c: REBIND_STALE=0). Clearing made the next rebind scan
    // re-capture "rest" from a MID-CLIP pose -> garbage inverse-bind -> shard fans
    // and V24-guard-dropped invisible bodies. The captured rest WorldXfms remain
    // the correct bake basis across these reloads; SyncObjects additionally
    // re-seeds NEW bones right after SetDeformation() (deterministic rest pose).
    // ONLY a closet transition needs the full reset: a closet outfit/gender swap
    // re-poses (or re-genders) the skeleton, so the old snapshot really is stale.
    if (mInCloset || bvar1) {
        mNativeReboundBody = 0;
        mNativeRestCaptured = false;
        mNativeRestPose.clear();
        mNativeRestDistinct.clear();
    }
    // C13 backstop: mFileMerger is bound by the proxy-load of char/main/main.milo
    // (verified non-null for the chars.milo preview players); guard anyway so a
    // FileMerger-less char can't hard-crash here. Wii always has a FileMerger.
    if (!mFileMerger)
        return;
#endif
    if (!mFileMerger->StartLoad(b1) && (_ref0 || (bvar1 && !_ref0))) {
        mFileMerger->Select("blank", FilePath(""), true);
        mFileMerger->StartLoad(b1);
    }
}

#pragma push
#pragma pool_data off
#pragma dont_inline on
void BandCharacter::AddObject(Hmx::Object *o) {
    static Symbol ikScale("CharIKScale");
    static Symbol ikHand("CharIKHand");
    static Symbol collide("CharCollide");
    static Symbol charCuff("CharCuff");
    static Symbol charHair("CharHair");
    static Symbol meshDeform("MeshDeform");
    static Symbol charBoneOffset("CharBoneOffset");
    static Symbol outfitConfig("OutfitConfig");
    static Symbol charMeshHide("CharMeshHide");
    static Symbol ikMidi("CharIKMidi");
    static Symbol cdMidi("CharDriverMidi");
    static Symbol khMidi("CharKeyHandMidi");
    Symbol name = o->ClassName();
    if (name == ikScale)
        unk5c0.push_back(dynamic_cast<CharIKScale *>(o));
    else if (name == ikHand)
        unk5d0.push_back(dynamic_cast<CharIKHand *>(o));
    else if (name == collide)
        unk5e0.push_back(dynamic_cast<CharCollide *>(o));
    else if (name == charHair) {
        CharHair *h = dynamic_cast<CharHair *>(o);
        h->SetManagedHookup(true);
        unk5f0.push_back(h);
    } else if (name == charCuff)
        unk600.push_back(dynamic_cast<CharCuff *>(o));
    else if (name == meshDeform) {
        RndMeshDeform *md = dynamic_cast<RndMeshDeform *>(o);
        if (!md->Mesh())
            MILO_FAIL("RndMeshDeform(%s) has no deform mesh.", md->Name());
        unk610.push_back(md);
    } else if (name == outfitConfig) {
        OutfitConfig *cfg = dynamic_cast<OutfitConfig *>(o);
        unk738 |= cfg->OverlayFlags();
        unk620.push_back(cfg);
    } else if (name == charBoneOffset)
        unk640.push_back(dynamic_cast<CharBoneOffset *>(o));
    else if (name == charMeshHide)
        unk5b0.push_back(dynamic_cast<CharMeshHide *>(o));
    else if (name == ikMidi)
        unk650.push_back(dynamic_cast<CharIKMidi *>(o));
    else if (name == cdMidi)
        unk660.push_back(dynamic_cast<CharDriverMidi *>(o));
    else if (name == khMidi)
        unk670.push_back(dynamic_cast<CharKeyHandMidi *>(o));
}
#pragma pop

void BandCharacter::AddOverlays(BandPatchMesh &mesh) {
    for (ObjPtrList<OutfitConfig, ObjectDir>::iterator it = unk620.begin();
         it != unk620.end();
         ++it) {
        for (ObjVector<OutfitConfig::Overlay>::iterator oit = (*it)->mOverlays.begin();
             oit != (*it)->mOverlays.end();
             ++oit) {
            if ((*oit).mCategory & mesh.mCategory) {
                mesh.ConstructQuad((*oit).mTexture);
            }
        }
    }
}

void BandCharacter::DeformHead(SyncMeshCB *cb) {
    if (mOutfitDir) {
        RndMesh *mesh = mOutfitDir->Find<RndMesh>("head.mesh", false);
        if (mesh) {
            BandHeadShaper shaper;
            if (!shaper.Start(this, mGender, mesh, cb, false))
                return;
            else
                mHead.SetShape(shaper);
        }
    }
}

void BandCharacter::SyncOutfitConfig(OutfitConfig *cfg) {
    char buf[256];
    strcpy(buf, cfg->Name());
    char *dot = strchr(buf, '.');
    MILO_ASSERT(dot, 0x5EA);
    int colors[7];
    *dot = 0;
    Symbol sym(buf);
    if (sym == eyes) {
        colors[3] = 0;
        colors[4] = 0;
        colors[5] = 0;
        colors[3] = mHead.mEyeColor;
        cfg->SetColors(&colors[3]);
    } else if (sym == skin || sym == heads) {
        colors[0] = 0;
        colors[1] = 0;
        colors[2] = 0;
        colors[0] = mSkinColor;
        cfg->SetColors(colors);
    } else {
        BandCharDesc::OutfitPiece *piece = mOutfit.GetPiece(sym);
        if (piece)
            cfg->SetColors(piece->mColors);
        else {
            BandCharDesc::OutfitPiece *instpiece = mInstruments.GetPiece(sym);
            if (instpiece)
                cfg->SetColors(instpiece->mColors);
            else
                cfg->Recompose();
        }
    }
    if (sym == skin) {
        OutfitConfig::SetSkinTextures(this, mOutfitDir, this);
        if (unk738) {
            cfg->RecomposePatches(unk738);
            unk738 = 0;
        }
    }
}

void BandCharacter::SetDeformation() {
#ifdef HX_NATIVE
    { static int g=-1; if(g<0)g=getenv("RB3_NO_DEFORM")?1:0; if(g)return; }
    if (NativeReloadProbe())
        fprintf(stderr,
                "[SETDEFORM] poll=%d char='%s' inCloset=%d restCaptured=%d restPose=%d\n",
                gNativeBandCharPollSerial, Name() ? Name() : "?", (int)mInCloset,
                (int)mNativeRestCaptured, (int)mNativeRestPose.size());
#endif
    CharClip *clip = BandCharDesc::GetDeformClip(mGender);
    if (clip) {
        CharBonesMeshes meshes;
        meshes.SetName("tmp_bones", this);
        clip->StuffBones(meshes);
        clip->ScaleDown(meshes, 0);
        clip->ScaleAdd(meshes, 1, 0, 0);
        meshes.PoseMeshes();
        if (LOADMGR_EDITMODE && BoneServo()) {
            BoneServo()->AcquirePose();
        }
        for (ObjPtrList<CharIKScale, ObjectDir>::iterator it = unk5c0.begin();
             it != unk5c0.end();
             ++it) {
            (*it)->CaptureBefore();
        }
        CharMeshCacheMgr *mgr = new CharMeshCacheMgr();
        mgr->Disable(!mInCloset);
        for (ObjPtrList<RndMesh, ObjectDir>::iterator it = unk73c.begin();
             it != unk73c.end();
             ++it) {
            mgr->SyncMesh(*it, 0xBF);
        }
        DeformHead(mgr);
        for (ObjPtrList<CharCuff, ObjectDir>::iterator it = unk600.begin();
             it != unk600.end();
             ++it) {
            (*it)->Deform(mgr, mFileMerger);
        }
        unk73c.clear();
        mgr->StuffMeshes(unk73c);
        clip->ScaleDown(meshes, 0);
        float weights[18];
        ComputeDeformWeights(weights);
        for (int i = 0; i < 18; i++) {
            clip->ScaleAdd(meshes, weights[i], i, 0);
        }
        meshes.PoseMeshes();
        for (ObjPtrList<RndMeshDeform, ObjectDir>::iterator it = unk610.begin();
             it != unk610.end();
             ++it) {
            (*it)->Reskin(mgr, (unk224 >> 1) & 1);
        }
        for (ObjPtrList<CharCollide, ObjectDir>::iterator it = unk5e0.begin();
             it != unk5e0.end();
             ++it) {
            CharCollide *col = *it;
            RndMesh *colmesh = col->mMesh;
            if (colmesh && mgr->HasMesh(colmesh)) {
                col->Deform();
            }
        }
        for (ObjPtrList<CharIKScale, ObjectDir>::iterator it = unk5c0.begin();
             it != unk5c0.end();
             ++it) {
            (*it)->CaptureAfter();
        }
        for (ObjPtrList<CharIKHand, ObjectDir>::iterator it = unk5d0.begin();
             it != unk5d0.end();
             ++it) {
            (*it)->MeasureLengths();
        }
        for (ObjPtrList<OutfitConfig, ObjectDir>::iterator it = unk620.begin();
             it != unk620.end();
             ++it) {
            SyncOutfitConfig(*it);
            (*it)->ApplyAO(mgr);
        }
        delete mgr;
        unk224 &= 0xfffffffd; // i think this might be a bitfield
    }
}

void BandCharacter::PlayGroup(
    const char *cc, bool b, int i, float f, TaskUnits u, Symbol s
) {
    if (mOverrideGroup[0] != 0 && AllowOverride(cc)) {
        cc = mOverrideGroup;
        f = 0;
    }
    if (*cc) {
        bool b528 = mForceNextGroup;
        bool b3 = false;
        unk5a3 = false;
        mForceNextGroup = false;
        b3 = (b | b528) || f != 0;
        CharClipDriver *driver = SetState(cc, mPlayFlags, i, b3, true);
        if (driver) {
            mFrozen = false;
            driver->SetBeatOffset(f, u, s);
        }
        if (BoneServo()->mRegulate && !mTeleported) {
            Teleport(BoneServo()->mRegulate);
        }
    }
}

CharClipDriver *
BandCharacter::SetState(const char *cc, int playFlags, int mask, bool b4, bool b5) {
    if (!streq(mGroupName, cc)) {
        strcpy(mGroupName, cc);
        b4 = true;
    }
    CharDriver *oldDriver = unk454;
    mPlayFlags = playFlags;
    if (AddDriverClipDir() && streq(mGroupName, "realtime_idle")
        && (mPlayFlags & 0x38000)) {
        unk454 = mAddDriver;
    } else {
        unk454 = mDriver;
    }
    if (!b4 && unk454) {
        CharClip *clip = unk454->FirstPlayingClip();
        b4 = true;
        bool rej = unk454 != oldDriver || !clip;
        if (!rej) {
            if ((mPlayFlags & clip->Flags()) == mPlayFlags)
                b4 = false;
        }
    }
    if (b4)
        return PlayMainClip(mask, b5);
    return 0;
}

CharLipSyncDriver *BandCharacter::GetLipSyncDriver() {
    return Find<CharLipSyncDriver>("song.lipdrv", false);
}

DECOMP_FORCEACTIVE(
    BandCharacter,
    "BandCharacter::SetFaceOverrideClip couldn't find clip named %s for %s\n",
    "BandCharacter::SetFaceOverrideClip couldnt find  lip sync driver for %s\n",
    "!mFileMerger->IsLoading()",
    "head"
)

void BandCharacter::SetHeadLookatWeight(float f) {
    if (mHeadLookAt) {
        mHeadLookAt->SetWeight(f);
        if (mNeckLookAt)
            mNeckLookAt->SetWeight(f * 0.5f);
    }
}

bool BandCharacter::SetPrefab(BandCharDesc *desc) {
    mTestPrefab = desc;
    if (mTestPrefab)
        CopyCharDesc(mTestPrefab);
    return unk224;
}

void BandCharacter::ClearDircuts() { mDircuts.clear(); }

bool BandCharacter::AddDircut(Symbol s1, Symbol s2, int i) {
    Symbol animinst = BandCharDesc::GetAnimInstrument(mInstrumentType);
    FilePath fp;
    bool ismale = mGender != "female";
    int mask = 0x8000;
    if (!ismale)
        mask = 0x4000;
    if (i & mask) {
        fp.Set(
            FileRoot(),
            MakeString("char/main/anim/%s/dircut/%s/%s_%s.milo", animinst, mGender, s1, s2)
        );
    } else {
        fp.Set(
            FileRoot(),
            MakeString("char/main/anim/%s/dircut/%s/%s.milo", animinst, mGender, s1)
        );
    }
    return AddDircut(fp);
}

bool BandCharacter::AddDircut(const FilePath &f) {
    MILO_ASSERT(!f.empty(), 0x794);
    for (std::list<String>::iterator it = mDircuts.begin(); it != mDircuts.end(); ++it) {
        if ((const String &)f == *it) {
            return true;
        }
    }
    unsigned int mergerSize = mFileMerger->mMergers.size();
    int start = mFileMerger->FindMergerIndex("directed_cut_0", true);
    unsigned int maxNum = mergerSize - start;
    if (mDircuts.size() >= maxNum)
        return false;
    mDircuts.push_back(f);
    return true;
}

void BandCharacter::SetDircuts() {
    int start = mFileMerger->FindMergerIndex("directed_cut_0", true);
    int maxNum = mFileMerger->mMergers.size() - start;
    MILO_ASSERT(maxNum < 32, 0x7AE);
    int slots[32];
    int i = 0;
    for (int j = 0; j < maxNum; j++) {
        slots[j] = j + start;
    }
    for (std::list<String>::iterator it = mDircuts.begin(); it != mDircuts.end();
         ++it, ++i) {
        const String &str = *it;
        int idx;
        for (idx = i; idx < maxNum; idx++) {
            const FileMerger::Merger &cur = mFileMerger->mMergers[slots[idx]];
            if (cur.mSelected == str || cur.mLoaded == str || cur.loading == str) {
                int tmp = slots[idx];
                slots[idx] = slots[i];
                slots[i] = tmp;
                break;
            }
        }
        if (idx == maxNum) {
            const char *cstr = str.c_str();
            FilePath fp;
            fp.Set(FilePath::sRoot.c_str(), cstr);
            FileMerger::Merger &cur = mFileMerger->mMergers[slots[i]];
            cur.mSelected = fp;
            cur.unk29 = false;
        }
    }
    for (; i < maxNum; i++) {
        FilePath fp;
        fp.Set(FilePath::sRoot.c_str(), "");
        FileMerger::Merger &cur = mFileMerger->mMergers[slots[i]];
        cur.mSelected = fp;
        cur.unk29 = false;
    }
}

int BandCharacter::GetShotFlags(Symbol s) {
    BandCharDesc::CharInstrumentType ty =
        BandCharDesc::GetInstrumentFromSym(mInstrumentType);
    if (ty >= BandCharDesc::kNumInstruments)
        return 0;
    else {
        DataArray *arr = BandWardrobe::GetGroupArray(ty);
        for (int i = 0; i < arr->Size(); i++) {
            bool symEq = (strcmp(arr->Array(i)->Sym(0).mStr, s.mStr) == 0);
            if (symEq) {
                return arr->Array(i)->Int(1);
            }
        }
    }
    return 0;
}

void BandCharacter::SetContext(Symbol s) {
    CharWeightable *w = Find<CharWeightable>("venue.weight", false);
    if (w)
        w->SetWeight(s == "venue");
    CharWeightable *cw = Find<CharWeightable>("closet.weight", false);
    if (cw)
        cw->SetWeight(s == "closet");
    mOverrideGroup[0] = 0;
    int hideallint = 0;
    if (s == "vignette") {
        SetClipTypes(s, s);
        hideallint = 0x2000;
        mDriver->SetBlendWidth(1.0f);
    } else if (s == "closet") {
        SetClipTypes("shell", "shell");
        mDriver->SetBlendWidth(2.0f);
    } else if (s == "venue") {
        ObjectDir *clipsdir = Find<ObjectDir>("body_clips", true);
        mDriver->SetClips(clipsdir);
        mDriver->SetBlendWidth(1.0f);
        switch (BandCharDesc::GetInstrumentFromSym(mInstrumentType)) {
        case kGuitar:
        case kBass:
            SetClipTypes("guitar_all", "guitar_body");
            break;
        case kDrum:
            SetClipTypes("drum_all", "drum_body");
            break;
        case kMic:
            SetClipTypes("mic_body", "mic_body");
            break;
        case kKeyboard:
            SetClipTypes("keyboard_all", "keyboard_body");
            break;
        default:
            break;
        }
        HandleType(on_set_instrument_clip_types_msg);
#ifdef HX_NATIVE
        // WALK-ON SNAP (docs/native/walkon-2026-07-02/SCOUT.md, fix #1).
        // Native async load leaves the on-stage gameplay band frozen on the last
        // frame of a loading-screen vignette clip (seated/lying cab pose): the
        // vignette CharClip is cleared BEFORE venue entry, so with an empty
        // driver Character::Poll stops re-driving the bones and the skeleton
        // HOLDS the stale vignette pose until a NEW clip plays. Wii cannot reach
        // this: everything is preloaded and already in a stage idle at venue
        // entry. Arm the Poll() retry here (body_clips groups may not resolve
        // yet); Poll plays the default stage idle the moment a clip is available,
        // so no member is ever left frozen on a vignette pose if the intro shot's
        // group is late or misses this member (SCOUT.md H3). HX_NATIVE-only ->
        // Wii build byte-identical. Opt out with RB3_WALKON_SNAP_OFF=1.
        if (WalkonSnapEnabled())
            mNativeWalkonSnapPending = true;
#endif
    } else {
        MILO_WARN("%s illegal context %s", PathName(this), s);
    }
    CharMeshHide::HideAll(unk5b0, hideallint);
}

void ReplaceSubdir(ObjectDir *d1, ObjectDir *d2) {
    for (int i = 0; i < d1->mSubDirs.size(); i++) {
        ObjDirPtr<ObjectDir> dPtr(d1->mSubDirs[i].Ptr());
        d1->RemoveSubDir(dPtr);
    }
    ObjDirPtr<ObjectDir> dPtr(d2);
    d1->AppendSubDir(dPtr);
}

void BandCharacter::SetVisemes() {
    ObjectDir *visemedir = Find<ObjectDir>("visemes", false);
    if (visemedir) {
        ObjectDir *viseme = BandHeadShaper::GetViseme(mGender, false);
        if (viseme)
            ReplaceSubdir(visemedir, viseme);
        CharLipSyncDriver *lsdriver = Find<CharLipSyncDriver>("song.lipdrv", false);
        if (lsdriver)
            lsdriver->SetClips(visemedir);
        CharFaceServo *servo = Find<CharFaceServo>("face.faceservo", false);
        if (servo)
            servo->SetClips(visemedir);
    }
    ObjectDir *vignettedir = Find<ObjectDir>("vignette_visemes", false);
    if (vignettedir) {
        ObjectDir *viseme = BandHeadShaper::GetViseme(mGender, true);
        if (viseme)
            ReplaceSubdir(vignettedir, viseme);
        CharLipSyncDriver *lsdriver = Find<CharLipSyncDriver>("vignette.lipdrv", false);
        if (lsdriver)
            lsdriver->SetClips(vignettedir);
    }
}

void BandCharacter::SetGroupName(const char *name) { strcpy(mGroupName, name); }

OutfitConfig *BandCharacter::GetOutfitConfig(const char *cc) {
    ObjectDir *pObjectDir;
    if (strcmp(cc, "guitar.cfg") == 0 || strcmp(cc, "bass.cfg") == 0
        || strcmp(cc, "drum.cfg") == 0 || strcmp(cc, "mic.cfg") == 0
        || strcmp(cc, "keyboard.cfg") == 0) {
        pObjectDir = mInstDir;
    } else
        pObjectDir = mOutfitDir;
    MILO_ASSERT(pObjectDir, 0x8AD);
    return pObjectDir->Find<OutfitConfig>(cc, false);
}

RndTex *BandCharacter::GetPatchTex(Patch &patch) {
    static Message get_patch_tex("get_patch_tex", DataNode(0), DataNode(0));
    get_patch_tex[0] = DataNode(patch.mTexture);
    get_patch_tex[1] = DataNode(patch.mMeshName);
    const DataNode &handled = HandleType(get_patch_tex);
    if (handled.Type() == kDataUnhandled || !handled.Obj<RndTex>()) {
        if (!mPrefab.Null()) {
            return Find<RndTex>(MakeString("prefab_art%02d.tex", patch.mTexture), false);
        } else if (LOADMGR_EDITMODE)
            return Find<RndTex>("patchtest.tex", false);
        else
            return 0;
    }
    return handled.Obj<RndTex>();
}

RndMesh *BandCharacter::GetPatchMesh(Patch &patch) {
    ObjectDir *dir = this;
    if (patch.mCategory & 0x2E00) {
        dir = mInstDir;
    }
    return dir->Find<RndMesh>(patch.mMeshName.c_str(), false);
}

RndTex *BandCharacter::GetBandLogo() {
    if (LOADMGR_EDITMODE) {
        return TheRnd->GetNullTexture();
    } else {
        RndTex *ret;
        DataNode handled = HandleType(get_band_logo_msg);
        if (handled.Type() == kDataObject) {
            ret = handled.Obj<RndTex>();
        } else
            ret = 0;
        return ret;
    }
}

void BandCharacter::Compress(RndTex *tex, bool b) { tex->Compress(b); }

void BandCharacter::TextureCompressed(int i) {
    std::list<int>::iterator it;
    for (it = mCompressedTextureIDs.begin();
         it != mCompressedTextureIDs.end() && *it != i;
         ++it)
        ;
    if (it == mCompressedTextureIDs.end())
        MILO_WARN("%s Could not find compress texture id %d\n", PathName(this), i);
    else
        mCompressedTextureIDs.erase(it);
}

void BandCharacter::RecomposePatches(BandCharDesc *desc, int i) {
    CopyCharDesc(desc);
    if (!mInCloset) {
        unk224 |= 1;
#ifdef HX_NATIVE
        gNativeStartLoadTag = "RecomposePatches";
#endif
        StartLoad(true, mInCloset, true);
    } else {
        for (ObjPtrList<OutfitConfig, ObjectDir>::iterator it = unk620.begin();
             it != unk620.end();
             ++it) {
            (*it)->RecomposePatches(i);
        }
    }
}

void BandCharacter::SetInstrumentType(Symbol s) {
    if (s != mInstrumentType) {
        mInstrumentType = s;
        SetChanged(8);
    }
}

void BandCharacter::ClearGroup() {
    SetState("", mPlayFlags, 1, false, false);
    mGroupName[0] = 0;
    if (mDriver)
        mDriver->Clear();
    if (mAddDriver)
        mAddDriver->Clear();
}

void BandCharacter::MiloReload() {
#ifdef HX_NATIVE
    gNativeStartLoadTag = "MiloReload";
#endif
    StartLoad(false, mInCloset, false);
}

void BandCharacter::SetLipSync(CharLipSync *sync) {
    CharLipSyncDriver *driver = Find<CharLipSyncDriver>("song.lipdrv", false);
    if (driver) {
        driver->mSongOwner = 0;
        driver->SetLipSync(sync);
    }
}

void BandCharacter::SetSongOwner(CharLipSyncDriver *driver) {
    CharLipSyncDriver *drvr = Find<CharLipSyncDriver>("song.lipdrv", false);
    if (drvr) {
        drvr->mSongOwner = driver;
        drvr->SetLipSync(Find<CharLipSync>("blinktrack.lipsync", false));
        drvr->mSongOffset = RandomFloat(0, 1000.0f);
    }
}

void BandCharacter::SetSingalong(float f) {
    if (mSingalongWeight)
        mSingalongWeight->SetWeight(f);
}

#pragma push
#pragma dont_inline on
BEGIN_HANDLERS(BandCharacter)
    HANDLE_EXPR(get_play_flags, mPlayFlags)
    HANDLE(play_group, OnPlayGroup)
    HANDLE(group_override, OnGroupOverride)
    HANDLE(change_face_group, OnChangeFaceGroup)
    HANDLE_ACTION(clear_group, ClearGroup())
    HANDLE(set_play, OnSetPlay)
#ifdef HX_NATIVE
    HANDLE_ACTION(
        start_load,
        (gNativeStartLoadTag = "start_load_dta",
         StartLoad(_msg->Int(2), _msg->Size() > 3 ? _msg->Int(3) : mInCloset, false))
    )
#else
    HANDLE_ACTION(
        start_load,
        StartLoad(_msg->Int(2), _msg->Size() > 3 ? _msg->Int(3) : mInCloset, false)
    )
#endif
    HANDLE_EXPR(is_loading, IsLoading())
    HANDLE_EXPR(flag_string, FlagString(_msg->Int(2)))
    HANDLE(cam_teleport, OnCamTeleport)
    HANDLE(closet_teleport, OnClosetTeleport)
    HANDLE(install_filter, OnInstallFilter)
    HANDLE(pre_clear, OnPreClear)
    HANDLE(copy_prefab, OnCopyPrefab)
    HANDLE(save_prefab, OnSavePrefab)
    HANDLE(set_file_merger, OnSetFileMerger)
    HANDLE_EXPR(list_dircuts, OnListDircuts())
    HANDLE(load_dircut, OnLoadDircut)
    HANDLE_ACTION(set_context, SetContext(_msg->Sym(2)))
    HANDLE_ACTION(save_from_closet, SavePrefabFromCloset())
    HANDLE_ACTION(set_singalong, SetSingalong(_msg->Float(2)))
    HANDLE(on_post_merge, OnPostMerge)
    HANDLE(hide_categories, OnHideCategories)
    HANDLE(restore_categories, OnRestoreCategories)
    HANDLE_ACTION(game_over, GameOver())
#ifdef MILO_DEBUG
    HANDLE(toggle_interests_overlay, OnToggleInterestDebugOverlay)
#endif
    HANDLE(list_drum_venues, OnListDrumVenues)
    HANDLE(portrait_begin, OnPortraitBegin)
    HANDLE(portrait_end, OnPortraitEnd)
    HANDLE_SUPERCLASS(BandCharDesc)
    HANDLE_SUPERCLASS(Character)
    HANDLE_CHECK(0x9A6)
END_HANDLERS
#pragma pop

void BandCharacter::GameOver() {
    for (ObjPtrList<CharIKMidi, ObjectDir>::iterator it = unk650.begin();
         it != unk650.end();
         ++it) {
        CharIKMidi *cur = *it;
        cur->Handle(Message("game_over"), true);
    }
    for (ObjPtrList<CharDriverMidi, ObjectDir>::iterator it = unk660.begin();
         it != unk660.end();
         ++it) {
        CharDriverMidi *cur = *it;
        cur->Handle(Message("game_over"), true);
    }
    for (ObjPtrList<CharKeyHandMidi, ObjectDir>::iterator it = unk670.begin();
         it != unk670.end();
         ++it) {
        CharKeyHandMidi *cur = *it;
        cur->Handle(Message("game_over"), true);
    }
}

DataNode BandCharacter::ListAnimGroups(int mask) {
    BandCharDesc::CharInstrumentType instType =
        BandCharDesc::GetInstrumentFromSym(mInstrumentType);
    if (BandCharDesc::kNumInstruments <= instType) {
        DataArray *arr = new DataArray(1);
        arr->Node(0) = Symbol();
        DataNode ret(arr, kDataArray);
        arr->Release();
        return DataNode(ret);
    }
    DataArray *groups = BandWardrobe::GetGroupArray(instType);
    int count = 1;
    for (int i = 0; i < groups->Size(); i++) {
        int _tmp1 = groups->Array(i)->Int(1);
        int flags = _tmp1 & mask;
        if ((mask & 0xFF) == (flags & 0xFF) && (flags & 0x3F00))
            count++;
    }
    DataArray *result = new DataArray(count);
    int idx = 1;
    result->Node(0) = Symbol();
    for (int i = 0; i < groups->Size(); i++) {
        int _tmp2 = groups->Array(i)->Int(1);
        int flags = _tmp2 & mask;
        if ((mask & 0xFF) == (flags & 0xFF) && (flags & 0x3F00)) {
            result->Node(idx++) = groups->Array(i)->Sym(0);
        }
    }
    DataNode ret(result, kDataArray);
    result->Release();
    return DataNode(ret);
}

DataNode BandCharacter::OnListDircuts() {
    int mask = 0x3E00;
    if (mGender == "female") {
        if (mGenre == "banger")
            mask |= 0x4;
        else if (mGenre == "dramatic")
            mask |= 0x2;
        else if (mGenre == "rocker")
            mask |= 0x1;
        else if (mGenre == "spazz")
            mask |= 8;
    } else {
        if (mGenre == "banger")
            mask |= 0x40;
        else if (mGenre == "dramatic")
            mask |= 0x20;
        else if (mGenre == "rocker")
            mask |= 0x10;
        else if (mGenre == "spazz")
            mask |= 0x80;
    }
    return ListAnimGroups(mask);
}

DataNode BandCharacter::OnLoadDircut(DataArray *da) {
    Symbol sym = da->Sym(2);
    if (sym == "") {
        return DataNode(0);
    } else {
        ClearDircuts();
        return DataNode(AddDircut(sym, mGenre, GetShotFlags(sym)));
    }
}

DataNode BandCharacter::OnListDrumVenues(DataArray *da) {
    DataArrayPtr ptr;
    ptr->Resize(4);
    for (int i = 0; i < 4; i++) {
        ptr->Node(i) = Symbol(sDrumVenueMappings[i * 2]);
    }
    return DataNode(ptr);
}

DataNode BandCharacter::OnPlayGroup(DataArray *da) {
    bool b6 = false;
    if (da->Size() > 3)
        b6 = da->Int(3);
    bool b1 = false;
    if (da->Size() > 4)
        b1 = da->Int(4);
    float f7 = 0;
    int i5 = 0;
    Symbol s;
    if (da->Size() > 5) {
        f7 = da->Float(5);
        i5 = da->Int(6);
        s = da->Sym(7);
    }
    int i3 = 2;
    if (b1)
        i3 = 1;
    PlayGroup(da->Str(2), b6, i3, f7, (TaskUnits)i5, s);
    return DataNode(0);
}

DataNode BandCharacter::OnGroupOverride(DataArray *da) {
    strcpy(mOverrideGroup, da->Str(2));
    mForceNextGroup = true;
    return DataNode(0);
}

DataNode BandCharacter::OnSetPlay(DataArray *da) {
    SetState(mGroupName, mPlayFlags & 0xFFF80FFF | da->Int(2), 3, false, false);
    return DataNode(0);
}

DataNode BandCharacter::OnClosetTeleport(DataArray *da) {
    unk734->DirtyLocalXfm() = LocalXfm();
    Teleport(unk734);
    unk5a2 = false;
    return DataNode(0);
}

DataNode BandCharacter::OnCamTeleport(DataArray *da) {
    if (da->Int(2)) {
        Teleport(unk594);
    } else {
        Waypoint *w = unk594;
        Teleport(0);
        unk594 = w;
        unk5a2 = false;
    }
    return DataNode(0);
}

DataNode BandCharacter::OnChangeFaceGroup(DataArray *da) {
    if (!mFaceDriver || !mFaceDriver->ClipDir())
        return DataNode(0);
    else if (strcmp(mFaceGroupName, da->Str(2)) != 0) {
        strcpy(mFaceGroupName, da->Str(2));
        PlayFaceClip();
    }
    return DataNode(0);
}

void ReplaceRefs(Hmx::Object *theirs, Hmx::Object *mine) {
    MILO_ASSERT(mine, 0xA72);
#ifdef HX_NATIVE
    // V23 (salvage V33 re-apply): reallocation-safe index-based rewrite.
    // The matched fork caches raw std::vector<ObjRef*> iterators into
    // theirs->Refs(), walks them in reverse (it[-1]), and after each
    // ref->Replace() resets it = end(). ref->Replace() ERASES the replaced
    // ObjRef from theirs->mRefs, which under libstdc++ can REALLOCATE the
    // vector and invalidate both cached iterators → the next --it; it[-1]
    // dereferences a dangling iterator → SIGSEGV (fault @ +0x30). MWCC/PPC
    // tolerated the stale iterator; clang-LP64 does not. Semantics identical
    // (reverse walk over current refs; replace any whose owner.Dir() is in
    // {sOutfitDir, sResourceDir, sToDir}).
    while (true) {
        const std::vector<ObjRef *> &refs = theirs->Refs();
        int sz = (int)refs.size();
        bool replaced = false;
        for (int i = sz - 1; i >= 0; --i) {
            // Re-read size each iter — outer Replace() may have shortened it.
            if (i >= (int)theirs->Refs().size()) continue;
            ObjRef *ref = theirs->Refs()[i];
            if (!ref) continue;
            ref->RefOwner();
            if (ref->RefOwner() != NULL) {
                ObjectDir *dir = ref->RefOwner()->Dir();
                bool match =
                    (dir == sOutfitDir) || (dir == sResourceDir) || (dir == sToDir);
                if (match && theirs != mine) {
                    ref->Replace(theirs, mine);
                    replaced = true;
                    break;  // refs may have reallocated; restart from new end
                }
            }
        }
        if (!replaced) break;
    }
#else
    std::vector<ObjRef *>::const_iterator it = theirs->Refs().end();
    std::vector<ObjRef *>::const_iterator begin = theirs->Refs().begin();
    for (; it != begin; --it) {
        ObjRef *ref = it[-1];
        ref->RefOwner();
        if (ref->RefOwner() != NULL) {
            ObjectDir *dir = ref->RefOwner()->Dir();
            bool match = (dir == sOutfitDir) || (dir == sResourceDir) || (dir == sToDir);
            if (match && theirs != mine) {
                ref->Replace(theirs, mine);
                it = theirs->Refs().end();
                begin = theirs->Refs().begin();
            }
        }
    }
#endif
}

// just here temporarily until we match the corresponding funcs these strings belong to
DECOMP_FORCEACTIVE(
    BandCharacter,
    "Mesh",
    "%s is being merged into",
    "mine->Dir() == this",
    "bone_",
    "exo_",
    "world.wind",
    "instruments can only have one subdir, which is the resource or colorpalettes.milo",
    "bone_pelvis.mesh",
    "outfits can only have one subdir, which is the resource"
)

extern Symbol AmbientOcclusion;
extern Symbol CharWeightSetter;

MergeFilter::Action
BandCharacter::Filter(Hmx::Object *o1, Hmx::Object *o2, ObjectDir *dir) {
    static Symbol meshName("Mesh");
    if (o2 == mInstDir) {
        Character *character = dynamic_cast<Character *>(o1);
        mInstDir->CopyBoundingSphere(character);
        mInstDir->RepointSphereBase(this);
    }
    else if (!o2 && o1->ClassName() == AmbientOcclusion)
        return kIgnore;
    if (!o2 && o1->ClassName() == CharWeightSetter)
        return kKeep;
    if (o1->ClassName() == "OutfitConfig") {
        if (o2) {
            MILO_NOTIFY_ONCE("%s is being merged into", PathName(o2));
        }
        unk630.push_back(dynamic_cast<OutfitConfig *>(o1));
    }
    if (o1->Dir() == sCharSharedDir) {
        Hmx::Object *mine = Find<Hmx::Object>(o1->Name(), true);
        MILO_ASSERT(mine->Dir() == this, 0xAB8);
        ReplaceRefs(o1, mine);
        return kIgnore;
    }
    if (o1->Dir() == sInstrumentDir || o1->Dir() == sInstResourceDir) {
        RndTransformable *rt = dynamic_cast<RndTransformable *>(o1);
        if (rt) {
            Hmx::Object *found = FindObject(o1->Name(), false);
            if (found) {
                if (rt->TransParent()) {
                    dynamic_cast<RndTransformable *>(found)->SetLocalXfm(rt->LocalXfm());
                }
                ReplaceRefs(o1, found);
                return kIgnore;
            }
        }
    }
    if (!(o1->Dir() == sOutfitDir || o1->Dir() == sResourceDir || o1->Dir() == sToDir)) {
        if (o1->Dir() == sBoneMergeDir) {
            RndTransformable *rt = dynamic_cast<RndTransformable *>(o1);
            if (rt) {
                Hmx::Object *found = FindObject(o1->Name(), false);
                if (found)
                    ReplaceRefs(o1, found);
            }
        }
        return kIgnore;
    }
    if (strnicmp(o1->Name(), "bone_", 5) == 0) {
        RndTransformable *rt = dynamic_cast<RndTransformable *>(o1);
        if (rt) {
            if (rt->TransParent()) {
                if (strnicmp(rt->TransParent()->Name(), "bone_", 5) == 0 || strnicmp(rt->TransParent()->Name(), "exo_", 4) == 0)
                    return kMerge;
            }
            return kKeep;
        }
    }
    Action action = mFileMerger->MergeAction(o1, o2, dir);
    if (sDrawOrder != -1.0f && o1->ClassName() == meshName) {
        RndMesh *mesh = dynamic_cast<RndMesh *>(o1);
        if (mesh->GetOrder() == 0.0f)
            mesh->SetOrder(5.0f + sDrawOrder);
    }
    if (!o2 && dir != this && action <= kReplace) {
        AddObject(o1);
    }
    return action;
}

MergeFilter::Action BandCharacter::FilterSubdir(ObjectDir *o1, ObjectDir *toDir) {
#ifdef HX_NATIVE
    // Native load-order fix (char textures rendering white). A shared external
    // resource milo (its own file on disk, e.g. char/main/shared/colorpalettes.milo
    // — the base skin/cloth texture palette referenced by every character) is loaded
    // ONCE and referenced as a subdir by many milos via share=true. The matched
    // action for such a subdir under mSubdirs=kAllSubdirs is kMerge, which MOVES
    // (SetName) its texture objects into THIS character dir, draining the shared
    // instance. On Wii each referencing milo finishes its atomic load (its materials
    // resolving textures against the still-intact subdir) before any merge drains it.
    // On native the loader advances one state per poll, so a concurrent character
    // merge drains the shared palette mid-load of a sibling milo — that sibling's
    // materials then resolve their RndTex ObjPtrs to null (the "couldn't find
    // dummy_torso.tex" cascade) and render with the white fallback texture. Keep an
    // external shared resource subdir as a REFERENCE (kReplace appends it as a subdir
    // of the character) instead of draining it: the palette stays intact, every
    // character's materials resolve their textures through the kept shared subdir,
    // regardless of native load interleaving. Scoped to subdirs that are their own
    // on-disk milo (non-empty stored file). Guarded so the Wii-matched path below is
    // byte-identical to the original.
    //
    // NOTE (mixed-gender band-member skinning deformation): the deform is NOT fixed
    // here, and it is NOT caused by this shim. ROOT CAUSE (2026-06-06, hard-evidenced,
    // engine BAND_DRAW_PROBE / SKEL_LOAD_PROBE / INSTALL_PROBE): all four band outfit
    // meshes bind to ONE shared char/main/skeleton_unshared.milo instance (parent==nil
    // root) at the MALE bind. That shared root is established by NAME RESOLUTION, not by
    // this merge: each char RESOURCE milo (vocal/viseme/guitar/..._resource.milo) lists
    // `char/main/skeleton.milo` as a share=true non-inlined subdir, so the FIRST loader
    // creates it and every subsequent reference (DirLoader::Find) shares it
    // (ObjectDir::LoadSubDir, Dir.cpp). The per-member main.milo skeleton DOES load
    // fresh per member (kInlineCached, 4 distinct instances) but the OUTFIT meshes never
    // bind to it — they bind to the shared skeleton.milo root. The female member
    // (player1, trackjacket: inverse-binds baked for the FEMALE bind) therefore lands on
    // the male-bind shared skeleton and flings ~20u (skinPos=(19.8,3.8,0.4)). PROVEN
    // dead-ends: scoping this shim to kInlineNever palettes (outfits kMerge) — band
    // still binds the shared root, female still flung; full shim-off (retail kMerge) —
    // same shared root + white textures; pruning char_shared's `../skeleton.milo`
    // subdir — strips ALL outfit bones (they had already consolidated onto it). The
    // faithful fix must un-share `char/main/skeleton.milo` for the band at the
    // name-resolution / share layer (broad, high-risk; would also touch the crowd) AND
    // pose each per-member skeleton to its outfit's gender bind (skeleton_unshared.milo
    // is itself male-bind; the gender pose comes from the outfit/clip). The renderer
    // fling clamp (RB3_NO_SKIN_CLAMP) is the shipped fix. See
    // docs/native/CHAR_SKINNING_DEFORM_INVESTIGATION.md.
    MergeFilter::Action act =
        DefaultSubdirAction(o1, (Subdirs)mFileMerger->mFilesPending.front()->mSubdirs);
    if (act == MergeFilter::kMerge && o1 && !o1->mStoredFile.empty()) {
        act = MergeFilter::kReplace;
    }
    return act;
#else
    return DefaultSubdirAction(o1, (Subdirs)mFileMerger->mFilesPending.front()->mSubdirs);
#endif
}

DataNode BandCharacter::OnInstallFilter(DataArray *da) {
    sBoneMergeDir = 0;
    sOutfitDir = da->Obj<ObjectDir>(2);
    sToDir = da->Obj<ObjectDir>(3);
    sInstrumentDir = da->Obj<ObjectDir>(4);
    Symbol sym = da->Sym(5);
    ObjectDir *boneMeshDir;
    sResourceDir = 0;
    int inSession = 0;
    sCharSharedDir = 0;
    boneMeshDir = 0;
    if (BandCharDesc::GetInstrumentFromSym(sym) < BandCharDesc::kNumInstruments) {
        if (mInstDir) {
            inSession = 1;
        }
    }
    if (inSession) {
        mInstDir->mSphere.radius = 0.0f;
    }
    sDrawOrder = -1.0f;
    const char *bodyparts[] = { "hair",      "glasses",  "facehair", "earrings",
                                "piercings", "eyebrows", "wrist",    "torso",
                                "head",      "legs",     "feet",     "rings",
                                "hands",     0 };
    for (int i = 0; bodyparts[i] != 0; i++) {
        if (strcmp(sym.mStr, bodyparts[i]) == 0) {
            sDrawOrder = (i + 1) * 10;
            break;
        }
    }
    mFileMerger->mFilter = this;
    if (Hmx::Object *pelvis = FindObject("bone_pelvis.mesh", false)) {
        boneMeshDir = pelvis->Dir();
    }
    sInstResourceDir = 0;
    if (sInstrumentDir && sInstrumentDir->mSubDirs.size() != 0) {
        if (sOutfitDir->mSubDirs.size() > 1) {
            MILO_WARN("instruments can only have one subdir, which is the "
                      "resource or colorpalettes.milo");
        }
        ObjectDir *instSubdir = sInstrumentDir->mSubDirs[0];
        if (instSubdir != boneMeshDir) {
            sInstResourceDir = instSubdir;
        }
    }
    if (sOutfitDir) {
        RndTransformable *xfm = dynamic_cast<RndTransformable *>(
            sOutfitDir->FindObject("bone_pelvis.mesh", false)
        );
        if (xfm) {
            sBoneMergeDir = xfm->Dir();
        }
        Hmx::Object *feetObj = sOutfitDir->FindObject("feet_skin.mat", false);
        if (feetObj) {
            sCharSharedDir = feetObj->Dir();
        }
        if (sOutfitDir->mSubDirs.size() != 0) {
            if (sOutfitDir->mSubDirs.size() > 1) {
                MILO_WARN("outfits can only have one subdir, which is the resource");
            }
            ObjectDir *outfitSubdir = sOutfitDir->mSubDirs[0];
            if (outfitSubdir != sBoneMergeDir && outfitSubdir != boneMeshDir) {
                sResourceDir = outfitSubdir;
            }
        }
    }
    return DataNode(0);
}

DataNode BandCharacter::OnPreClear(DataArray *da) {
    Symbol sym = da->Sym(2);
    FileMerger *fm = da->Obj<FileMerger>(3);
    static Symbol ocn("OutfitConfig");
    FileMerger::Merger *m = fm->FindMerger(sym, true);
    while (!m->mLoadedObjects.empty()) {
        Hmx::Object *obj = m->mLoadedObjects.front();
        if (obj->ClassName() == ocn) {
            unk738 |= dynamic_cast<OutfitConfig *>(obj)->OverlayFlags();
        }
        delete obj;
    }
    return DataNode(0);
}

void BandCharacter::SavePrefabFromCloset() { MILO_ASSERT(0, 0xB95); }

DataNode BandCharacter::OnSavePrefab(DataArray *da) {
    if (mTestPrefab)
        mTestPrefab->CopyCharDesc(this);
    return DataNode(0);
}

DataNode BandCharacter::OnCopyPrefab(DataArray *da) {
    if (mTestPrefab)
        CopyCharDesc(mTestPrefab);
    return DataNode(0);
}

DataNode BandCharacter::OnSetFileMerger(DataArray *da) {
    FilePathTracker tracker(FileRoot());
    SetVisemes();
    unk224 &= 0xfffffff2;
    if (!mFileMerger)
        return DataNode(0);
    FilePath fp70;
    if (!mPrefab.Null())
        fp70.SetRoot(MakeString("char/main/prefab/%s.milo", mPrefab));
    mFileMerger->Select("prefab", fp70, unk5a1);
    const char *bodyparts[14] = { "head",     "eyebrows", "torso",     "legs", "hands",
                                  "wrist",    "rings",    "feet",      "hair", "facehair",
                                  "earrings", "glasses",  "piercings", 0 };
    for (int i = 0; bodyparts[i] != 0; i++) {
        FilePath fp7c;
        MakeOutfitPath(bodyparts[i], fp7c);
        mFileMerger->Select(bodyparts[i], fp7c, unk5a1);
    }
    for (int i = 0; i < 5; i++) {
        mFileMerger->Select(BandCharDesc::GetInstrumentSym(i), FilePath(0), false);
    }
    FilePath fp88("");
    FilePath fp94("");
    FilePath fpa0("");
    FilePath fpac("");
    FilePath fpb8("");
    FilePath fpc4("");
    FilePath fpd0("");
    FilePath fpdc("");
    FilePath fpe8("");
    FilePath fpf4("");
    mPlayFlags &= 0xffcfffff;
    Symbol animinst = BandCharDesc::GetAnimInstrument(mInstrumentType);
    BandCharDesc::CharInstrumentType ty =
        BandCharDesc::GetInstrumentFromSym(mInstrumentType);
    if (ty == BandCharDesc::kGuitar)
        mPlayFlags |= 0x100000;
    else if (ty == BandCharDesc::kBass)
        mPlayFlags |= 0x200000;
    if (ty != BandCharDesc::kNumInstruments) {
        if (!mGenre.Null() && !mTempo.Null()) {
            if (ty == BandCharacter::kMic) {
                mUseMicStandClips = mGenre != "banger";
            }
            fp94.SetRoot(MakeString(
                "char/main/anim/%s/body/%s/realtime_%s.milo", animinst, mGender, mGenre
            ));
            fpa0.SetRoot(MakeString(
                "char/main/anim/%s/body/%s/%s_%s.milo", animinst, mGender, mTempo, mGenre
            ));
            if (ty == BandCharDesc::kDrum) {
                fpac.SetRoot(MakeString(
                    "char/main/anim/%s/body_add/%s/%s_%s.milo",
                    animinst,
                    mGender,
                    mTempo,
                    mGenre
                ));
                fpb8.SetRoot(MakeString(
                    "char/main/anim/%s/body_add/%s/body_add_base.milo", animinst, mGender
                ));
            }
        }
        switch (ty) {
        case BandCharDesc::kGuitar:
        case BandCharDesc::kBass:
            fp88.SetRoot("char/main/rigging/guitar_rh.milo");
            if (mGender == "female")
                fpf4.SetRoot("char/main/anim/rigging/guitar/fret_left_female.milo");
            else
                fpf4.SetRoot("char/main/anim/rigging/guitar/fret_left.milo");
            break;
        case BandCharDesc::kDrum:
            fp88.SetRoot("char/main/rigging/drum.milo");
            fpc4.SetRoot(
                MakeString("char/main/anim/rigging/drum/stick_left_%s.milo", mGender)
            );
            fpd0.SetRoot(
                MakeString("char/main/anim/rigging/drum/stick_right_%s.milo", mGender)
            );
            if (mGender == "female") {
                fpdc.SetRoot("char/main/anim/rigging/drum/pedal_right_female.milo");
                fpe8.SetRoot("char/main/anim/rigging/drum/pedal_left_female.milo");
            } else {
                fpdc.SetRoot("char/main/anim/rigging/drum/pedal_right.milo");
                fpe8.SetRoot("char/main/anim/rigging/drum/pedal_left.milo");
            }
            break;
        case BandCharDesc::kMic:
            fp88.SetRoot("char/main/rigging/vocal.milo");
            break;
        case BandCharDesc::kKeyboard:
            fp88.SetRoot("char/main/rigging/keyboard.milo");
            break;
        default:
            MILO_FAIL("new instrument type added but not supported");
            break;
        }
        if (ty != BandCharDesc::kDrum || !mDrumVenue.Null()) {
            FilePath fp100("");
            MakeInstrumentPath(mInstrumentType, mDrumVenue, fp100);
            mFileMerger->Select(mInstrumentType, fp100, unk5a1);
        }
    }
    if (mInTourEnding && !mTestTourEndingVenue.Null()) {
        FilePath fp10c(MakeString(
            "char/main/anim/%s/finale/%s/%s/tour_endings.milo",
            animinst,
            mGender,
            mTestTourEndingVenue
        ));
        mFileMerger->Select("tour_ending_clips", fp10c, false);
    } else {
        if (LOADMGR_EDITMODE && !mTestTourEndingVenue.Null()) {
            FilePath fp118(MakeString(
                "char/main/anim/%s/finale/%s/%s/tour_endings.milo",
                animinst,
                mGender,
                mTestTourEndingVenue
            ));
            mFileMerger->Select("tour_ending_clips", fp118, false);
        } else {
            mFileMerger->Select("tour_ending_clips", FilePath(""), false);
        }
    }
    mFileMerger->Select("rigging", fp88, false);
    mFileMerger->Select("body_realtime_clips", fp94, false);
    mFileMerger->Select("body_tempo_clips", fpa0, false);
    mFileMerger->Select("body_add_clips", fpac, false);
    mFileMerger->Select("body_add_base", fpb8, false);
    mFileMerger->Select("stick_left", fpc4, false);
    mFileMerger->Select("stick_right", fpd0, false);
    mFileMerger->Select("drum_pedal_right", fpdc, false);
    mFileMerger->Select("drum_pedal_left", fpe8, false);
    mFileMerger->Select("guitar_fret", fpf4, false);
    SetDircuts();
    unk5a1 = false;
    return DataNode(0);
}

DataNode BandCharacter::OnPostMerge(DataArray *da) {
    Symbol category = da->Sym(2);
    ObjectDir *dir = da->Obj<ObjectDir>(3);
    bool noTextures = da->Int(4) != 0;
    while (unk630.size() != 0) {
        OutfitConfig *cfg = unk630.front();
        unk630.pop_front();
        SyncOutfitConfig(cfg);
        cfg->Recompose();
        if (!mInCloset)
            cfg->CompressTextures();
    }
    RndTransformable *bone = Find<RndTransformable>("bone_guitar_lh_mod.mesh", false);
    if (bone)
        bone->ResetLocalXfm();
    unk680 = mInstDir->Find<RndMesh>("mic_stand.mesh", false);
    unk68c = Find<RndMesh>("drum_L-stick.mesh", false);
    unk698 = Find<RndMesh>("drum_R-stick.mesh", false);
    unk6a4 = Find<RndMesh>("guitar_pick.mesh", false);
#ifdef HX_NATIVE
    // render-polish 2026-06-11 (char-render): merge-completion probe — attributes
    // the per-frame SyncObjects/SetDeformation churn to its merger category.
    if (NativeReloadProbe())
        fprintf(stderr,
                "[POSTMERGE] poll=%d char='%s' cat='%s' loadingLoad=%d asyncLoad=%d "
                "unk6bd=%d noTextures=%d -> sync=%d\n",
                gNativeBandCharPollSerial, Name() ? Name() : "?", category.Str(),
                (int)mFileMerger->mLoadingLoad, (int)mFileMerger->mAsyncLoad,
                (int)unk6bd, (int)noTextures,
                (int)(!mFileMerger->mLoadingLoad
                      && (noTextures || (mFileMerger->mAsyncLoad && !unk6bd))));
#endif
    if (!mFileMerger->mLoadingLoad
        && (noTextures || (mFileMerger->mAsyncLoad && !unk6bd))) {
        SyncObjects();
    }
    return DataNode(0);
}

void BandCharacter::SaveBoneAndChildren(RndTransformable *bone) {
    if (strncmp(bone->Name(), "bone_", 5) == 0) {
        BoneState state;
        state.mBone = bone;
        state.mXfm = bone->WorldXfm();
        unk6e4.push_back(state);
        for (std::vector<RndTransformable *>::const_iterator it =
                 bone->TransChildren().begin();
             it != bone->TransChildren().end();
             ++it) {
            SaveBoneAndChildren(*it);
        }
    }
}

DataNode BandCharacter::OnPortraitBegin(DataArray *da) {
    EnableBlinks(false, true);
    BoneState state;
    state.mBone = this;
    state.mXfm = mLocalXfm;
    unk6e4.push_front(state);
    RndTransformable *bone = Find<RndTransformable>("bone_pelvis.mesh", true);
    SaveBoneAndChildren(bone);
    strcpy(unk6f4, mGroupName);
    unk6ec = Hmx::Object::New<CharDriver>();
    unk6ec->Transfer(*mDriver);
    unk6f0 = mPlayFlags;
    return DataNode(0);
}

DataNode BandCharacter::OnPortraitEnd(DataArray *da) {
    EnableBlinks(true, false);
    mLocalXfm = unk6e4.front().mXfm;
    SetDirty();
    unk6e4.pop_front();
    for (std::list<BoneState>::iterator it = unk6e4.begin(); it != unk6e4.end();
         ++it) {
        it->mBone->SetWorldXfm(it->mXfm);
    }
    unk6e4.clear();
    strcpy(mGroupName, unk6f4);
    mDriver->Transfer(*unk6ec);
    delete unk6ec;
    unk6ec = 0;
    mPlayFlags = unk6f0;
    return DataNode(0);
}

DataNode BandCharacter::OnHideCategories(DataArray *da) {
    if (!mFileMerger)
        return DataNode(0);
    static Symbol rm("Mesh");
    for (int i = 2; i < da->Size(); i++) {
        FileMerger::Merger *merger = mFileMerger->FindMerger(da->Sym(i), true);
        for (ObjPtrList<Hmx::Object, ObjectDir>::iterator it =
                 merger->mLoadedObjects.begin();
             it != merger->mLoadedObjects.end();
             ++it) {
            Hmx::Object *obj = *it;
            if (obj->ClassName() == rm) {
                RndMesh *mesh = dynamic_cast<RndMesh *>(obj);
                if (mesh->Showing()) {
                    mesh->SetShowing(false);
                    unk74c.push_back(mesh);
                }
            }
        }
    }
    return DataNode(0);
}

DataNode BandCharacter::OnRestoreCategories(DataArray *da) {
    while (unk74c.size() != 0) {
        RndMesh *mesh = unk74c.front();
        mesh->SetShowing(true);
        unk74c.pop_front();
    }
    return DataNode(0);
}

BEGIN_PROPSYNCS(BandCharacter)
    SYNC_PROP(tempo, mTempo)
    SYNC_PROP(genre, mGenre)
    SYNC_PROP(drum_venue, mDrumVenue)
    SYNC_PROP(force_vertical, mForceVertical)
    SYNC_PROP_SET(instrument_type, mInstrumentType, SetInstrumentType(_val.Sym()))
    SYNC_PROP_SET(group_name, mGroupName, SetGroupName(_val.Str()))
    SYNC_PROP_SET(
        head_lookat_weight,
        mHeadLookAt ? mHeadLookAt->Weight() : 0,
        SetHeadLookatWeight(_val.Float())
    )
#ifdef HX_NATIVE
    SYNC_PROP_SET(
        in_closet,
        mInCloset,
        (gNativeStartLoadTag = "in_closet_propsync", StartLoad(false, _val.Int(), false))
    )
#else
    SYNC_PROP_SET(in_closet, mInCloset, StartLoad(false, _val.Int(), false))
#endif
    SYNC_PROP(test_prefab, mTestPrefab)
    SYNC_PROP(use_mic_stand_clips, mUseMicStandClips)
    SYNC_PROP(in_tour_ending, mInTourEnding)
    SYNC_PROP(test_tour_ending_venue, mTestTourEndingVenue)
    SYNC_SUPERCLASS(BandCharDesc)
    SYNC_SUPERCLASS(Character)
END_PROPSYNCS
