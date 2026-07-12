#include "bandobj/BandDirector.h"
#include "bandobj/BandWardrobe.h"
#include "decomp.h"
#include "math/Utl.h"
#include "obj/Data.h"
#include "obj/Object.h"
#include "obj/Task.h"
#include "rndobj/Group.h"
#include "rndobj/PostProc.h"
#include "bandobj/CrowdAudio.h"
#include "utl/Loader.h"
#include "world/Dir.h"
#include "utl/Symbols.h"
#include "utl/Messages.h"
#ifdef HX_NATIVE
// V22 (salvage V33): for CamShot::GetCam() definition used in DrawShowing
// venue-cam-follow.
#include "world/CameraShot.h"
#include "rndobj/Cam.h"
#include <cstdlib> // Q9: getenv for RB3_VENUE_SYNC (guarded out of Wii build)
#include <cstdio>  // BOOTRNG A.S1: fprintf for the postproc-source-tuple probe
#include <cstring> // BOOTRNG A.S1: strcmp/strncpy for the throttle
#include "math/Rand.h" // BOOTRNG A.S1: RB3GRandDrawCount() stream-position probe
// crowd-venues: gDataDir (named-global object lookup) + Message (to Handle the
// MetaPerformer get_venue_override query) for the venue-override bridge below.
#include "obj/DataUtl.h"
#include "obj/Msg.h"

// Q9 (incremental-load): RB3_VENUE_SYNC gates the native-synthetic EnterVenue
// venue force-load (BandDirector::EnterVenue). Defaults to 1 (sync) because the
// load's side effect (instancing TheBandWardrobe + mVenue.Dir()) is consumed
// SYNCHRONOUSLY by the rest of BandDirector::Enter() on the same frame. Setting
// RB3_VENUE_SYNC=0 opts into an experimental async load (unsafe until Enter() is
// split into a multi-frame poll — see the note at the call site). Read once.
static bool NativeVenueSync() {
    static int sSync = -1;
    if (sSync < 0) {
        sSync = 1; // default: keep the venue load synchronous (correct ordering)
        if (const char *e = ::getenv("RB3_VENUE_SYNC")) {
            if (e[0] == '0')
                sSync = 0;
        }
    }
    return sSync != 0;
}
#endif

INIT_REVS(BandDirector)

DataArray *BandDirector::sPropArr;
BandDirector *TheBandDirector;
bool gIsLoadingDlc;

const char *gVenues[5] = { "arena", "big_club", "festival", "small_club", "video" };

BandDirector::VenueLoader::VenueLoader() : mDir(0), mLoader(0) {}

BandDirector::VenueLoader::~VenueLoader() {
    MILO_ASSERT(!mLoader, 0x3C);
    delete mDir;
}

void BandDirector::VenueLoader::Unload(bool async) {
    if (mDir)
        mDir->SetName(mDir->Name(), mDir);
    if (async)
        TheLoadMgr.StartAsyncUnload();
    RELEASE(mLoader);
    RELEASE(mDir);
    if (async)
        TheLoadMgr.FinishAsyncUnload();
    mName = "";
    TheBandDirector->VenueLoaded(0);
}

void BandDirector::VenueLoader::Load(const FilePath &fp, LoaderPos pos, bool async) {
    Unload(async);
    mName = FileGetBase(fp.c_str(), 0);
    if (!mName.Null() && mName != "none") {
        mLoader = new DirLoader(fp, pos, this, 0, 0, false);
        if (!async)
            TheLoadMgr.PollUntilLoaded(mLoader, 0);
    }
}

void BandDirector::VenueLoader::FinishLoading(Loader *l) {
    MILO_ASSERT(l == mLoader, 0x67);
    mDir = dynamic_cast<WorldDir *>(mLoader->GetDir());
    RELEASE(mLoader);
    TheBandDirector->VenueLoaded(mDir);
}

void BandDirector::Init() {
    Register();
    sPropArr = new DataArray(1);
}

void BandDirector::Terminate() { sPropArr->Release(); }

BandDirector::BandDirector()
    : mPropAnim(this), mMerger(this), mCurWorld(this), unk58(0), mWorldPostProc(this),
      mCamPostProc(this), mPostProcA(this), mPostProcB(this), mPostProcBlend(0),
      mLightPresetCatBlend(0), mLightPresetInterpEnabled(1), mDisabled(0), mAsyncLoad(0),
      mCurShot(this), mNextShot(this), mIntroShot(this), unke0(-kHugeFloat),
      mDisablePicking(0), unke5(1), unk108(-1.0f), mEndOfSongSec(0), unk110(0),
      mSongPref(0) {
    static DataNode &banddirector = DataVariable("banddirector");
    banddirector = this;
    mAsyncLoad = !LOADMGR_EDITMODE;
    if (TheBandDirector) {
        MILO_WARN("Trying to make > 1 BandDirector, which should be single");
    } else
        TheBandDirector = this;
    mDircuts.reserve(100);
}

BandDirector::~BandDirector() {
    if (TheBandDirector == this) {
        static DataNode &banddirector = DataVariable("banddirector");
        banddirector = NULL_OBJ;
        TheBandDirector = nullptr;
    } else
        MILO_WARN("Deleting second BandDirector, should be singleton");
}

void AddStageKitKeys(RndPropAnim *anim, BandDirector *dir) {
    DataNode fognode(Symbol("stagekit_fog"));
    DataArrayPtr ptr(fognode);
    anim->AddKeys(dir, ptr, PropKeys::kSymbol);
}

Symbol HiddenInstrument(Symbol s) {
    if (s == coop_bg)
        return keyboard;
    else if (s == coop_bk)
        return guitar;
    else if (s == coop_gk)
        return bass;
    else
        return gNullStr;
}

#pragma push
#pragma pool_data off
void BandDirector::Enter() {
    RndPollable::Enter();
    if (mMerger) {
        mNumPlayersFailed = 0;
        mExcitement = 3;
        unke0 = -1e+30f;
        mShotCategory = "";
        mWorldPostProc = GetWorld()->Find<RndPostProc>("world.pp", true);
        RndPostProc *profilm = GetWorld()->Find<RndPostProc>("ProFilm_a.pp", true);
        if (profilm)
            mWorldPostProc->Copy(profilm, kCopyDeep);
        mWorldPostProc->Select();
        mPostProcA = mWorldPostProc;
        mPostProcB = mWorldPostProc;
        mPostProcBlend = 0;
#ifdef HX_NATIVE
        // V3 — RB3's extracted config/rnd.dta has no 'motion_blur' entry
        // (apparently a post-shipped patch/360-platform-specific override
        // missing from this ARK). The matched-fork SystemConfig fails-hard with
        // "Couldn't find 'motion_blur' in array (file config/band_keep.dta,
        // line 38)" the moment BandDirector::Enter fires from the game_screen
        // world-panel load — which the V3 fix newly reaches. Fall back to the
        // Wave-2.4 default (0.0f) when the key is absent so the venue Enter
        // completes and the audio Play() path can run.
        DataArray *rndCfg = SystemConfig("rnd");
        DataArray *mb = rndCfg ? rndCfg->FindArray("motion_blur", false) : nullptr;
        RndPostProc::sMotionBlurBlendAmount = mb ? mb->Float(1) : 0.0f;
#else
        RndPostProc::sMotionBlurBlendAmount = SystemConfig("rnd", "motion_blur")->Float(1);
#endif
        mCamPostProc = 0;
        mLightPresetCatA = mLightPresetCatB = gNullStr;
        mLightPresetCatBlend = 0;
        unk108 = -1.0f;
        mDisabled = 0;
        mCurWorld = 0;
        if (mPropAnim) {
            mPropAnim->StartAnim();
            mPropAnim->SetFrame(-kHugeFloat, 1.0f);
        }
        EnterVenue();
        mDisablePicking = 0;
        mNextShot = 0;
        static Message allowMsg("allow_intro_shot", 0);
        bool handled = HandleType(allowMsg).Int();
        if (handled && !mIntroShot)
            PickIntroShot();
        if (handled && mIntroShot) {
            static Message msg("set_intro_shot", 0);
            msg[0] = mIntroShot.Ptr();
            DataNode handled = HandleType(msg);
            mNextShot = mIntroShot;
            mIntroShot = 0;
        } else
            FindNextShot();
        Symbol hidden = HiddenInstrument(TheBandWardrobe->GetPlayMode());
        static const char *modes[3] = { "keyboard", "guitar", "bass" };
        for (int i = 0; i < 3U; i++) {
            Symbol thismode = modes[i];
            RndGroup *grp =
                GetWorld()->Find<RndGroup>(MakeString("%s_spot.grp", modes[i]), false);
            if (grp)
                grp->SetShowing(thismode != hidden);
        }
        if (mPropAnim && unk110) {
            DataArrayPtr ptr(Symbol("stagekit_fog"));
            SymbolKeys *skeys = dynamic_cast<SymbolKeys *>(mPropAnim->GetKeys(this, ptr));
            if (!skeys) {
                AddStageKitKeys(mPropAnim, this);
                skeys = dynamic_cast<SymbolKeys *>(mPropAnim->GetKeys(this, ptr));
            }
            if (skeys && skeys->empty()) {
                float cap = Min(mEndOfSongSec, 241.0f);
                for (float f = 0.0f; f + 60.0f < cap; f += 60.0f) {
                    skeys->Add(on, (f + 15.0f) * 30.0f, false);
                    skeys->Add(off, (f + 30.0f) * 30.0f, false);
                }
            }
        }
    }
}
#pragma pop

void BandDirector::Exit() {
    RndPollable::Exit();
    if (mPropAnim)
        mPropAnim->EndAnim();
    if (mVenue.Dir())
        mVenue.Dir()->Exit();
}

DECOMP_FORCEACTIVE(BandDirector, "video")

bool BandDirector::PostProcsFromPresets(
    const RndPostProc *&p1, const RndPostProc *&p2, float &fref
) {
    MILO_ASSERT(IsMusicVideo() && LightPresetMgr(), 0x161);
    LightPreset *mgr1 = nullptr;
    LightPreset *mgr2 = nullptr;
    LightPresetMgr()->GetPresets(mgr1, mgr2);
    if (!mgr1)
        mgr1 = mgr2;
    if (mgr1) {
        RndPostProc *proc1 = mgr1->GetCurrentPostProc();
        if (proc1) {
            RndPostProc *proc2 = mgr2 ? mgr2->GetCurrentPostProc() : 0;
            if (proc2) {
                mWorldPostProc->Interp(proc1, proc2, mLightPresetCatBlend);
                p1 = proc1;
                p2 = proc2;
                fref = mLightPresetCatBlend;
            } else {
                mWorldPostProc->Copy(proc1, kCopyDeep);
                p1 = proc1;
                p2 = 0;
                fref = 1.0f;
            }
            return true;
        }
    }
    return false;
}

void BandDirector::Poll() {
#ifdef HX_NATIVE
    // W31-SET-PLAY-DISPATCH one-shot diagnostic (read-only, default-OFF): confirm
    // the per-song `song.anim` mood/intensity keys are RESIDENT in the loaded
    // mPropAnim (bound at OnFileLoaded:1303). W31 proved the on-stage band leaving
    // idle in-song is gated on BandDirector::SyncProperty routing the intensity
    // key value to the correct band member (the arg-order fix below), NOT on any
    // missing pump — OnSelectCamera already advances this anim in-song.
    if (getenv("RB3_SETPLAY_PROBE") && mPropAnim && !mDisabled) {
        static bool sEnum = false;
        if (!sEnum) {
            sEnum = true;
            const char *props[] = { "guitar_intensity", "bass_intensity",
                                    "drum_intensity", "mic_intensity",
                                    "key_intensity", 0 };
            for (const char **p = props; *p; p++) {
                SymbolKeys *sk = dynamic_cast<SymbolKeys *>(
                    mPropAnim->GetKeys(this, DataArrayPtr(Symbol(*p))));
                fprintf(stderr,
                    "[SETPLAY_KEYS] prop='%s' present=%d size=%d\n",
                    *p, sk ? 1 : 0, sk ? (int)sk->size() : -1);
            }
        }
    }
#endif
    if (unke5) {
        if (mCurWorld) {
            mCurWorld->Poll();
            if (mLightPresetInterpEnabled
                && (mLightPresetCatA != gNullStr || mLightPresetCatB != gNullStr)) {
                LightPresetMgr()->Interp(
                    mLightPresetCatA, mLightPresetCatB, mLightPresetCatBlend
                );
            }
            LightPresetMgr()->Poll();
        }
    }
    if (mWorldPostProc) {
        const char *presets = "";
        const RndPostProc *p1 = nullptr;
        const RndPostProc *p2 = nullptr;
        float fref = 1.0f;
        if (mCamPostProc) {
            mWorldPostProc->Copy(mCamPostProc, kCopyDeep);
            presets = "camera";
            p1 = mCamPostProc;
        } else {
            bool ppfp = false;
            if (IsMusicVideo() && LightPresetMgr()) {
                ppfp = PostProcsFromPresets(p1, p2, fref);
                if (ppfp)
                    presets = "music video light presets";
            }
            if (!ppfp) {
                mWorldPostProc->Interp(mPostProcA, mPostProcB, mPostProcBlend);
                fref = mPostProcBlend;
                presets = "song authoring";
                p1 = mPostProcA;
                p2 = mPostProcB;
            }
        }
        UpdatePostProcOverlay(presets, p1, p2, fref);
#ifdef HX_NATIVE
        // BOOTRNG (Wave 11 A.S1, diagnosis-only): log the postproc SOURCE TUPLE
        // the native composite ultimately reads (A2 — RndPostProc::Current()
        // identity is uninformative because world.pp is rewritten in place every
        // frame; the tuple {source, p1, p2, blend} is what actually differs).
        // Sample the gRand stream position alongside so a per-boot tuple change
        // can be attributed to stream divergence. Throttled: emit only on change
        // or every 120th call. Additive; gated by RB3_BOOTRNG_PROBE + HX_NATIVE.
        {
            static int sBRP = -1;
            if (sBRP < 0) { const char* e = getenv("RB3_BOOTRNG_PROBE"); sBRP = (e && e[0] && e[0] != '0') ? 1 : 0; }
            if (sBRP) {
                static char sLast[256] = {0};
                static int sHB = 0;
                char buf[256];
                snprintf(buf, sizeof(buf),
                         "[BOOTRNG] PPSRC src=%s p1=%s p2=%s blend=%.3f gdraw=%lu",
                         presets ? presets : "<null>",
                         p1 && p1->Name() ? p1->Name() : "<null>",
                         p2 && p2->Name() ? p2->Name() : "<null>",
                         fref, RB3GRandDrawCount());
                if (std::strcmp(buf, sLast) != 0 || (++sHB % 120) == 0) {
                    fprintf(stderr, "%s\n", buf);
                    std::strncpy(sLast, buf, sizeof(sLast) - 1);
                }
            }
        }
#endif
#ifdef MILO_DEBUG
        DataNode &fps_var = DataVariable("cheat.emulate_fps");
        if (fps_var.Int() > 0) {
            int ifps = fps_var.Int();
            mWorldPostProc->mEmulateFPS = ifps;
        }
#endif
    }
}

void BandDirector::UpdatePostProcOverlay(
    const char *cc, const RndPostProc *p1, const RndPostProc *p2, float f
) {
#ifdef MILO_DEBUG
    RndOverlay *o = RndOverlay::Find("postproc", true);
    if (o->Showing()) {
        TextStream *ts = TheDebug.mReflect;
        TheDebug.mReflect = o;
        if (p1 && !p2) {
            MILO_LOG("Post Proc %s is not blended\n", p1->Name());
        } else {
            if (p1)
                MILO_LOG("Post Proc A %s\n", p1->Name());
            if (p2)
                MILO_LOG("Post Proc B %s\n", p2->Name());
        }
        MILO_LOG("PostProc set by %s, blend is %.2f%%\n", cc ? cc : "", f * 100.0f);
        TheDebug.SetReflect(ts);
    }
#endif
}

void BandDirector::ListPollChildren(std::list<RndPollable *> &polls) const {
    polls.push_back(mVenue.Dir());
}

void BandDirector::DrawShowing() {
#ifdef HX_NATIVE
    // V22 (salvage V33 re-apply): make the venue draw through the camera the
    // director is animating. V19 loaded the venue WorldDir as mCurWorld WITHOUT
    // merging it into GetWorld(), so mCurWorld != GetWorld() and the venue's
    // `WorldDir::DrawShowing` uses its own static `mCam` (CamOverride) — NOT
    // the shot-animated `world.cam` the director writes via PlayNextShot. The
    // engine (Rnd_Wgpu_RB3.cpp) already re-emits the scene uniforms per-mesh on
    // RndCam::sCurrent change, so the venue can draw through one cam and the
    // highway through another in the same frame. Bridge: point the venue
    // WorldDir's mCam at the director's active shot cam each frame.
    // Opt-out via env VENUE_CAM_LOCK=1 reverts to the static cam.
    if (mCurWorld && !getenv("VENUE_CAM_LOCK")) {
        WorldDir *gw = GetWorld();
        if (gw && gw != mCurWorld) {
            CamShot *shot = gw->mCameraManager.MiloCamera();
            if (!shot) shot = gw->mCameraManager.CurrentShot();
            if (shot) {
                RndCam *shotCam = shot->GetCam();
                // V36 (N2): venue-visibility fallback for void/wall camera cuts.
                // V22 points the venue draw-cam (CamOverride) at the director's
                // active shot cam every frame. SOME cuts frame near-black void or
                // a blank stage wall (e.g. coop_all_f00 / coop_bg_n* / the intro
                // pan): the shot has NO targets (path/offset-driven pan) and its
                // cam angle puts ALL the band/stage geometry outside the frustum,
                // so the backdrop renders nothing. The GOOD V22/V23 cuts (wide
                // stage shots that contain the band, and instrument closeups that
                // resolve a real band-character target — V23) must NOT be touched.
                //
                // Detection (validated on a 12000-frame sweep; instrumented under
                // VOIDCUT_DBG): a void/wall cut is a shot whose first keyframe has
                // NO resolved targets AND whose band-area sphere (the 4 wired
                // band-player roots, radius 120u) is entirely outside the shot
                // cam's frustum. This fired on 9% of gameplay frames (the worst
                // targetless void shots) and on ZERO of the resolved-target
                // closeups (3444 closeup frames left untouched). hasT=1 is the
                // load-bearing guard: every good closeup has a resolved target.
                //
                // Fallback: do NOT cut the venue draw-cam to the void shot — hold
                // it on the last shot cam that DID frame the band (sticky
                // last-good), so the backdrop keeps a band/stage framing instead
                // of dropping to void. Never invents a camera; never touches the
                // highway (which draws through its own game.cam, V12).
                //
                // Opt-out: RB3_CAM_FALLBACK_OFF=1 reverts to the raw V22 follow.
                // Diagnostic: VOIDCUT_DBG=1 logs each shot's verdict (canary).
                static RndCam *sVoidcutLastGoodCam = 0;
                bool fallbackOff = getenv("RB3_CAM_FALLBACK_OFF") != 0;
                bool dbg = getenv("VOIDCUT_DBG") != 0;
                bool framesVenue = true;
                if (shotCam && !fallbackOff) {
                    // resolved-target guard: a shot whose first keyframe resolves
                    // ANY real target is a framed shot (closeups, dircuts) — keep.
                    bool hasTarget = false;
                    BandCamShot *bcs = dynamic_cast<BandCamShot *>(shot);
                    if (bcs && !bcs->mKeyframes.empty())
                        hasTarget = bcs->mKeyframes[0].HasTargets();
                    if (!hasTarget) {
                        // targetless pan: void only if the band is out of frame.
                        Vector3 band; band.Zero(); int nb = 0;
                        if (TheBandWardrobe) {
                            for (int i = 0; i < 4; i++) {
                                BandCharacter *bc = TheBandWardrobe->GetCharacter(i);
                                if (bc) { Add(band, bc->WorldXfm().v, band); nb++; }
                            }
                        }
                        if (nb > 0) {
                            Vector3 bandC;
                            bandC.Set(band.x / nb, band.y / nb, band.z / nb);
                            // The wide INTRO_VENUE pan keeps the band in a WIDE
                            // (120u) frustum but its sweep dwells on the stage
                            // wall with the band tiny/peripheral. For the intro
                            // shot ONLY, use a tight (40u) band core so the dwell
                            // moments register as void; the wide establishing
                            // shots (coop_all_f0*) are NOT intro shots, so the
                            // tight test never touches them (no good-cut regress).
                            const char *snm = shot->Name();
                            float r = (snm && strstr(snm, "intro")) ? 40.0f : 120.0f;
                            Sphere bandSphere;
                            bandSphere.Set(bandC, r);
                            if (shotCam->CompareSphereToWorld(bandSphere))
                                framesVenue = false; // band fully outside frustum
                        }
                    }
                }
                if (dbg) {
                    MILO_LOG(
                        "VOIDCUT_DBG: shot=%s cam=%p framesVenue=%d lastGood=%p\n",
                        shot->Name(), (void *)shotCam, (int)framesVenue,
                        (void *)sVoidcutLastGoodCam);
                }
                if (shotCam && (framesVenue || fallbackOff)) {
                    // good cut (or fallback disabled): commit + remember it.
                    sVoidcutLastGoodCam = shotCam;
                    if (shotCam != mCurWorld->CamOverride())
                        mCurWorld->SetCam(shotCam);
                } else if (sVoidcutLastGoodCam) {
                    // void/wall cut: hold the last cam that framed the band.
                    if (sVoidcutLastGoodCam != mCurWorld->CamOverride())
                        mCurWorld->SetCam(sVoidcutLastGoodCam);
                } else if (shotCam) {
                    // no last-good yet (very first cut is bad): take it anyway so
                    // the venue at least draws through *a* cam.
                    if (shotCam != mCurWorld->CamOverride())
                        mCurWorld->SetCam(shotCam);
                }
            }
        }
    }
#endif
    if (mCurWorld)
        mCurWorld->DrawShowing();
}

void BandDirector::ListDrawChildren(std::list<RndDrawable *> &draws) {
    draws.push_back(mVenue.Dir());
}

void BandDirector::CollideList(const Segment &seg, std::list<Collision> &colls) {
    if (mCurWorld)
        mCurWorld->CollideListSubParts(seg, colls);
    RndDrawable::CollideList(seg, colls);
}

SAVE_OBJ(BandDirector, 0x1FA)

BEGIN_LOADS(BandDirector)
    LOAD_REVS(bs)
    ASSERT_REVS(6, 0)
    MILO_ASSERT(gRev > 2, 0x204);
    LOAD_SUPERCLASS(Hmx::Object)
    LOAD_SUPERCLASS(RndPollable)
    LOAD_SUPERCLASS(RndDrawable)
    if (gRev < 5)
        LOAD_SUPERCLASS(Hmx::Object)
    if (gRev < 6) {
        Symbol s;
        bs >> s;
    }
    if (gRev < 4) {
        char buf[0x100];
        bs.ReadString(buf, 0x100);
    }
END_LOADS

BEGIN_COPYS(BandDirector)
    COPY_SUPERCLASS(Hmx::Object)
    COPY_SUPERCLASS(RndPollable)
    CREATE_COPY(BandDirector)
END_COPYS

void BandDirector::Replace(Hmx::Object *from, Hmx::Object *to) {
    Hmx::Object::Replace(from, to);
    RndDrawable::Replace(from, to);
}

#pragma push
#pragma dont_inline on
DECOMP_FORCEFUNC(BandDirector, BandDirector, LightPresetMgr())
#pragma pop

WorldDir *BandDirector::GetWorld() {
    if (mMerger)
        return dynamic_cast<WorldDir *>(mMerger->Dir());
    else
        return 0;
}

DECOMP_FORCEACTIVE(BandDirector, "directed_", "BFTB_", "coop_")

static inline const char *DirectedCutStringHack() { return "directed_"; }

static inline const char *BFTBStringHack() { return "BFTB_"; }

static inline const char *FacingCameraStringHack() { return "coop_"; }

#pragma push
#pragma force_active on
inline bool BandDirector::DirectedCut(Symbol s) const {
    return strncmp(s.mStr, DirectedCutStringHack(), 9) == 0;
}

inline bool BandDirector::BFTB(Symbol s) const {
    return strncmp(s.mStr, BFTBStringHack(), 5) == 0;
}

inline bool BandDirector::FacingCamera(Symbol s) const {
    return (strnicmp(s.mStr, FacingCameraStringHack(), 5) == 0 && !BehindCamera(s));
}
#pragma pop

bool BandDirector::BehindCamera(Symbol s) const {
    const char *str = s.mStr;
    int len = strlen(str);
    return (len > 7 && strcmp(str + (len - 7), "_behind") == 0);
}

void BandDirector::FindNextShot() {
    mNextShot = 0;
    if (!mShotCategory.Null()) {
        WorldDir *dir = mVenue.Dir();
        if (dir) {
            std::vector<CameraManager::PropertyFilter> filts;
            CameraManager::PropertyFilter curfilt;
            BandCamShot *shot = mCurShot;
            if (!FacingCamera(mShotCategory))
                shot = nullptr;
            if (shot && !BehindCamera(shot->Category())) {
                curfilt.prop = flags_any;
                curfilt.match = 1;
                curfilt.mask = ~shot->Flags() & 0x7000;
                filts.push_back(curfilt);
            }
            mNextShot = dynamic_cast<BandCamShot *>(
                dir->mCameraManager.FindCameraShot(mShotCategory, filts)
            );
#ifdef MILO_DEBUG
            if (!mNextShot) {
                const char *pathName = dir->mPathName;
                MILO_LOG(
                    "NOTIFY could not find BandCamShot %s in %s at %s, ignoring\n",
                    mShotCategory,
                    pathName,
                    TheTaskMgr.GetMBT()
                );
            }
#endif
        }
    }
}

void BandDirector::PlayNextShot() {
    unk58 = false;
    if (mNextShot) {
        BandCamShot *oldnextshot = mNextShot;
        mNextShot = nullptr;
        if (oldnextshot && DirectedCut(oldnextshot->Category())) {
            float oldz = oldnextshot->mZeroTime;
            oldnextshot->ConvertFrames(oldz);
            float oldmin = oldnextshot->mMinTime;
            bool ret = oldnextshot->ConvertFrames(oldmin);
            MILO_ASSERT(ret, 0x2A1);
            MaxEq(oldmin, 0.25f);
            unke0 = oldmin + oldz + TheTaskMgr.Seconds(TaskMgr::kRealTime);
        } else {
            if (oldnextshot && BFTB(oldnextshot->Category())) {
                unke0 = TheTaskMgr.Seconds(TaskMgr::kRealTime)
                    + oldnextshot->GetTotalDurationSeconds();
            } else {
                bool b2 = false;
                bool b1 = (unke0 == -1000.0f && mCurShot) ? true : false;
                if (b1) {
                    b1 = true;
                    if (!DirectedCut(mCurShot->Category())) {
                        if (!BFTB(mCurShot->Category()))
                            b1 = false;
                    }
                    if (b1)
                        b2 = true;
                }
                if (b2)
                    unke0 = TheTaskMgr.Seconds(TaskMgr::kRealTime) + 1.0f;
                else
                    unke0 = -kHugeFloat;
            }
        }
        mCurShot = oldnextshot;
        BandCamShot *curshot = mCurShot;
        WorldDir *wdir = GetWorld();
        wdir->mCameraManager.ForceCameraShot(curshot);
        if (mCurWorld)
            mCurWorld->Handle(cam_cut_msg, false);
    }
}

void BandDirector::EnterVenue() {
#ifdef HX_NATIVE
    // V19 (salvage V33 re-apply): the retail `load_venue <sym>` dispatch (which
    // reads the world WorldDir's authored `venue` field and loads
    // world/venue/<class>/<name>/<name>.milo into mVenue) is data/game-mode driven
    // and never fires in the native flow — the only DTA `load_venue` is the
    // editor-only `load_and_play_song` preview. So mVenue.Dir() stays null,
    // EnterVenue is a no-op, mCurWorld stays null, and TheBandWardrobe is null
    // (it's instanced from world/shared/world_chars.milo, only loaded as a side
    // effect of the venue load).
    //
    // Bridge: read the world's authored `venue` symbol (falls back to
    // small_club_01 for the gameplay world) and load the venue synchronously,
    // exactly as retail `load_venue` would. The venue load pulls in
    // world_chars.milo so TheBandWardrobe becomes non-null and EnterVenue's
    // normal wardrobe path (SetVenueDir / SyncTransProxies) then runs and sets
    // mCurWorld.
    if (!mVenue.Dir() && GetWorld()) {
        // Crowd-venues bridge: honor the MetaPerformer venue override before
        // falling back to the world's authored `venue` prop. The retail
        // `load_venue` dispatch (which would consult the performer flow) never
        // fires natively, so {meta_performer set_venue_override <sym>} otherwise
        // sticks but is ignored and every native run pins small_club_01.
        //
        // Cross-layer access without a band3 #include (BandDirector is engine
        // layer; MetaPerformer is game layer): reach the gDataDir-findable named
        // global `meta_performer` and Handle the same `get_venue_override` query
        // DTA uses. GetVenueOverride() (not GetVenue()) is the value that sticks
        // in the native quickplay flow — SetVenue()/SelectRandomVenue() only run
        // in the tour/setlist path, so mVenue stays empty natively. When no
        // override is set the handler returns the `no_venue_override` sentinel,
        // so this falls straight through to the small_club_01 fallback below
        // (default behavior byte-for-byte unchanged).
        Symbol venueSym;
        if (gDataDir) {
            if (Hmx::Object *mp = gDataDir->FindObject("meta_performer", true)) {
                static Message getVenueOverrideMsg("get_venue_override");
                Symbol ov = mp->Handle(getVenueOverrideMsg, false).Sym();
                if (!ov.Null() && ov != Symbol("no_venue_override")) {
                    venueSym = ov;
                    if (getenv("VENUE_DBG"))
                        MILO_LOG("VENUE_DBG: EnterVenue honoring MetaPerformer "
                                 "venue override='%s'\n",
                                 ov.mStr ? ov.mStr : "(null)");
                }
            }
        }
        if (venueSym.Null()) {
            const DataNode *venueProp = GetWorld()->Property(Symbol("venue"), false);
            venueSym = venueProp ? venueProp->Sym(nullptr) : Symbol("small_club_01");
            if (venueSym.Null()) venueSym = Symbol("small_club_01");
        }
        if (getenv("VENUE_DBG"))
            MILO_LOG("VENUE_DBG: EnterVenue force-loading venue='%s'\n",
                     venueSym.mStr ? venueSym.mStr : "(null)");
        // Q9 (incremental-load): this native-synthetic venue force-load must stay
        // SYNCHRONOUS for correctness. The retail `load_venue` data dispatch never
        // fires natively, so this bridge force-loads the venue here; the load is the
        // side-effect that instances TheBandWardrobe and populates mVenue.Dir(). The
        // ENCLOSING BandDirector::Enter() then unconditionally dereferences
        // TheBandWardrobe (->GetPlayMode(), BandDirector.cpp:174) and walks the venue
        // groups on the SAME frame, with no re-entry/poll. So forcing this load async
        // here would null-deref TheBandWardrobe and skip the whole wardrobe path.
        //
        // A truly-async venue load requires splitting Enter()'s tail (lines ~160-198)
        // and EnterVenue's wardrobe path into a multi-frame poll gated on venue-load
        // completion — out of scope for the T4 small-fixes bundle (the wardrobe-
        // ordering invariant the verifier flagged at :604-607 cannot be preserved at
        // this single site). RB3_VENUE_SYNC=0 opts INTO the experimental async path
        // for that future work; it is UNSAFE until Enter() is split, so default = sync.
        bool prevAsync = mAsyncLoad;
        if (NativeVenueSync())
            mAsyncLoad = false;
        LoadVenue(venueSym, kLoadStayBack);
        mAsyncLoad = prevAsync;
    }
    if (getenv("VENUE_DBG"))
        MILO_LOG("VENUE_DBG: EnterVenue() wardrobe=%p venueDir=%p venueName='%s'\n",
                 (void *)TheBandWardrobe, (void *)mVenue.Dir(),
                 mVenue.Name().mStr ? mVenue.Name().mStr : "(null)");
#endif
    if (TheBandWardrobe) {
        WorldDir *dir = mVenue.Dir();
        if (dir) {
            dir->SetName(dir->Name(), GetWorld());
            dir->Enter();
            if (dir != mCurWorld) {
#ifdef HX_NATIVE
                // V23 (salvage V33 re-apply): drive the character-load step retail
                // runs from OnFileLoaded(song). At that earlier moment mVenue.Name()
                // is still null natively (venue is deferred until V19 force-load
                // above), so LoadMainCharacters never runs and mVenueNames /
                // mInstrumentType stay unset — the venue's player_<inst>0_*.tp
                // closeup-target proxies all collapse onto a shared stand-in dir.
                // Calling LoadCharacters here (post-V19 force-load, pre-SetVenueDir)
                // sets mVenueNames=[player_guitar0, player_bass0, player_mic0,
                // player_drum0] and assigns instruments so SyncTransProxies matches
                // 32 instrument-keyed proxies (was 0).
                if (TheBandWardrobe && !mVenue.Name().Null()) {
                    if (getenv("CHAR_DBG") || getenv("VENUE_DBG"))
                        MILO_LOG("VENUE_DBG: EnterVenue calling LoadCharacters('%s')\n",
                                 mVenue.Name().mStr ? mVenue.Name().mStr : "(null)");
                    TheBandWardrobe->LoadCharacters(mVenue.Name(), mAsyncLoad);
                }
#endif
                TheBandWardrobe->SetVenueDir(dir);
                if (mCurWorld)
                    mCurWorld->Handle(remove_midi_parsers_msg, false);
                mCurWorld = dir;
                unk58 = true;
                if (mCurWorld) {
                    if (TheCrowdAudio)
                        TheCrowdAudio->SetBank(mCurWorld);
#ifdef MILO_DEBUG
                    if (LOADMGR_EDITMODE) {
                        GetWorld()->mSphere = mCurWorld->mSphere;
                    }
#endif
                    mCurWorld->Handle(setup_midi_parsers_msg, false);
                    ClearLighting();
#ifdef HX_NATIVE
                    // V23 (salvage V33 re-apply): re-run HarvestDircuts now that
                    // mVenue.Dir() is real. The earlier harvest (driven from the
                    // load flow before V19's venue force-load above) bailed at its
                    // `mPropAnim && mVenue.Dir()` gate (venueDir=(nil)), so the
                    // song's authored MIDI DIRECTED_CUTs were never harvested and
                    // the director fell back to generic shot-category cycling.
                    // With the venue + song.anim both live now we get dircuts=15
                    // for 20thcenturyboy (beat-synced authored camerawork).
                    if (mPropAnim && mVenue.Dir()) {
                        if (getenv("CAMDIR_DBG") || getenv("VENUE_DBG"))
                            MILO_LOG("VENUE_DBG: EnterVenue re-running HarvestDircuts "
                                     "(propAnim=%p venueDir=%p)\n",
                                     (void *)mPropAnim, (void *)mVenue.Dir());
                        HarvestDircuts();
                    }
#endif
                }
            }
        }
    }
}

void BandDirector::ClearLighting() {
    if (mCurWorld)
        mCurWorld->Handle(clear_lighting_msg, false);
}

void GetVenuePath(FilePath &fp, const char *cc) {
    FilePathTracker tracker(FileRoot());
    fp.SetRoot("none");
    if (*cc == '\0' || streq(cc, "none")) {
        return;
    } else {
        for (int i = 0; gVenues[i] != 0; i++) {
#ifdef HX_NATIVE
            // host <cstring> strstr(const char*) returns const char*; only nul-tested.
            const char *str = strstr(cc, gVenues[i]);
#else
            char *str = strstr(cc, gVenues[i]);
#endif
            if (str) {
                fp.SetRoot(MakeString("world/venue/%s/%s/%s.milo", gVenues[i], cc, cc));
                return;
            }
        }
        MILO_WARN("BandDirector unknown venue %s", cc);
    }
}

static inline const char *BandDirectorMusicVideoStrHack() { return "video"; }

#pragma push
#pragma force_active on
inline bool BandDirector::IsMusicVideo() {
    return strstr(mVenue.Name().mStr, BandDirectorMusicVideoStrHack());
}
#pragma pop

void BandDirector::SetShot(Symbol cat, Symbol s2) {
    bool b1 = true;
    if (TheBandWardrobe) {
        if (!DirectedCut(cat))
            b1 = false;
    }
    if (!b1) {
        MILO_ASSERT(!BFTB(cat), 0x326);
        bool shot5 = s2 == "shot_5";
        bool playshot5 = TheBandWardrobe->PlayShot5();
        if (shot5 == playshot5) {
            if (shot5) {
                cat = RemapCat(cat, TheBandWardrobe->GetPlayMode());
            } else {
                if (s2 != TheBandWardrobe->GetPlayMode())
                    return;
            }
            mShotCategory = cat;
            unk58 = true;
        }
    }
}

bool BandDirector::ReadyForMidiParsers() {
    if (!mPropAnim && gIsLoadingDlc) {
        static Message msg("on_pre_merge", 0, 0, 0);
        msg[0] = song;
        msg[1] = NULL_OBJ;
        OnFileLoaded(msg);
    }
#ifdef HX_NATIVE
    // The gameplay 3D venue (the stage backdrop the band plays on) is a cosmetic
    // shell venue, deferred natively like the menu venues (WorldInstance::SyncDir
    // defers world/vignette/ + world/shared/ proxies — no working venue render).
    // So mVenue.Dir() is null and mVenue.Name() is empty here (not "none"), which
    // blocks the console gate forever. The midi-parser / track / scoring / audio
    // game logic does NOT depend on the 3D venue, so treat the venue as satisfied
    // natively (require only the loaded song.anim + chars). This advances
    // GamePanel::PollForLoading -> CreateGame -> Game::LoadSong (the goal).
    bool cond = mPropAnim != 0;
    if (cond) cond = TheBandWardrobe->AllCharsLoaded();
    if (getenv("GAME_DBG")) {
        static int rmp_spam = 0;
        if ((rmp_spam++ % 120) == 0)
            MILO_LOG("GAME_DBG: ReadyForMidiParsers propAnim=%p venueDir=%p "
                     "venueName='%s' -> %d (native venue-deferred gate)\n",
                     (void *)mPropAnim, (void *)mVenue.Dir(),
                     mVenue.Name().mStr ? mVenue.Name().mStr : "(null)", cond);
    }
    return cond;
#else
    bool cond = mPropAnim && (mVenue.Dir() || mVenue.Name() == "none");
    if (cond) cond = TheBandWardrobe->AllCharsLoaded();
    return cond;
#endif
}

void BandDirector::SendMessage(Symbol s1, Symbol s2) {
#ifdef HX_NATIVE
    // W31-SET-PLAY-DISPATCH probe (read-only, default-OFF). Logs every
    // intensity/singalong routing dispatch so we can see whether the song.anim
    // intensity keys are firing in-song and with which mood values. s1 is the
    // name-match token, s2 the message type (BandWardrobe::SendMessage matches
    // s1 against player_<inst>0 venue names and sends s2).
    if (getenv("RB3_SETPLAY_PROBE"))
        fprintf(stderr, "[SETPLAY_SEND] s1='%s' s2='%s' wardrobe=%p\n",
                s1.mStr ? s1.mStr : "?", s2.mStr ? s2.mStr : "?",
                (void *)TheBandWardrobe);
#endif
    if (TheBandWardrobe)
        TheBandWardrobe->SendMessage(s1, s2, true);
}

void BandDirector::SetCrowd(Symbol s) {
    static Message msg("");
    msg.SetType(s);
    HandleType(msg);
}

inline BandCharacter *BandDirector::GetCharacter(int idx) const {
    if (TheBandWardrobe)
        return TheBandWardrobe->GetCharacter(idx);
    else
        return 0;
}

DECOMP_FORCEACTIVE(BandDirector, "ADD_", "BLEND_")

DataNode BandDirector::OnGetFaceOverrideClips(DataArray *da) {
    std::list<Symbol> syms;
    BandCharacter *bchar = GetCharacter(da->Int(2));
    if (bchar) {
        CharLipSyncDriver *driver = bchar->GetLipSyncDriver();
        if (driver) {
            ObjectDir *overridedir = driver->OverrideDir();
            if (overridedir) {
                for (ObjDirItr<CharClip> it(overridedir, false); it; ++it) {
                    String blendstr("BLEND_");
                    blendstr += it->Name();
                    syms.push_back(blendstr.c_str());
                    String addstr("ADD_");
                    addstr += it->Name();
                    syms.push_back(addstr.c_str());
                }
            }
        }
    }
    DataArray *arr = new DataArray(syms.size() + 1);
    arr->Node(0) = Symbol();
    int idx = 1;
    for (std::list<Symbol>::iterator it = syms.begin(); it != syms.end(); ++it, ++idx) {
        arr->Node(idx) = *it;
    }
    DataNode ret(arr, kDataArray);
    arr->Release();
    return ret;
}

void BandDirector::ForceShot(BandCamShot *shot) {
    mNextShot = shot;
    mDisablePicking = mNextShot;
}

void BandDirector::PickIntroShot() {
    mNextShot = nullptr;
    DataNode handled = HandleType(pick_intro_shot_msg);
    mIntroShot = mNextShot;
    mNextShot = nullptr;
}

Symbol GetShotTrack() {
    if (TheBandWardrobe->PlayShot5()) {
        return "shot_5";
    } else {
        char buf[10];
        strcpy(buf, TheBandWardrobe->GetPlayMode().mStr);
        memcpy(buf, "shot", 4);
        return buf;
    }
}

void BandDirector::AddDircut(Symbol cat, float frame) {
    MILO_ASSERT(DirectedCut(cat), 0x40D);
    std::vector<CameraManager::PropertyFilter> filts;
    BandCamShot *shot = dynamic_cast<BandCamShot *>(
        mVenue.Dir()->mCameraManager.FindCameraShot(cat, filts)
    );
    if (TheBandWardrobe && !TheBandWardrobe->AddDircut(shot)) {
        MILO_WARN("Too many non-free Dircuts, not playing %s", PathName(shot));
        shot = nullptr;
    }
    if (shot) {
        float f2c = shot->mZeroTime;
        if (shot->ConvertFrames(f2c)) {
            frame = frame - f2c;
        } else
            MILO_FAIL("couldn't convert, in beats!");
    }
    mDircuts.Add(shot, frame, false);
}

void BandDirector::VenueLoaded(WorldDir *) { mDircuts.clear(); }

void BandDirector::HarvestDircuts() {
    if (!mPropAnim || !mVenue.Dir())
        return;
    if (!mDircuts.empty())
        mDircuts.erase(mDircuts.begin(), mDircuts.end());
    TheBandWardrobe->ClearDircuts();
    mIntroShot = nullptr;
    if (!TheBandWardrobe->DemandLoadSym()) {
        CameraManager::PropertyFilter filt;
        int mask = 0;
        Symbol playmode = TheBandWardrobe->GetPlayMode();
        if (strncmp(playmode.mStr, "coop", 4) == 0) {
            if (playmode == coop_bg)
                mask = 0x100000;
            else if (playmode == coop_bk)
                mask = 0x200000;
            else
                mask = 0x400000;
        }
        WorldDir *wdir = mVenue.Dir();
        FOREACH (it, wdir->mCameraManager.mCameraShotCategories) {
            FOREACH_PTR (cit, it->unk4) {
                CamShot *shot = *cit;
                shot->Disable(!TheBandWardrobe->ValidGenreGender(shot), 2);
                shot->Disable((mask & shot->Flags()) == 0, 4);
            }
        }
        PickIntroShot();
        DataArrayPtr ptr(GetShotTrack());
        SymbolKeys *skeys = dynamic_cast<SymbolKeys *>(mPropAnim->GetKeys(this, ptr));
        if (skeys) {
            Keys<Symbol, Symbol> &keys = skeys->AsSymbolKeys();
            for (int i = 0; i < keys.size(); i++) {
                Symbol val = keys[i].value;
                if (DirectedCut(val)) {
                    if (TheBandWardrobe->PlayShot5()) {
                        val = RemapCat(val, TheBandWardrobe->GetPlayMode());
                    }
                    AddDircut(val, keys[i].frame / 30.0f);
                }
            }
        }
    }
    TheBandWardrobe->StartClipLoads(true, 0);
}

void BandDirector::AddSymbolKey(Symbol s1, Symbol s2, float f) {
    const ObjPtr<RndPropAnim> &_ref0 = mPropAnim;
    if (_ref0) {
        DataArrayPtr ptr(s1);
        SymbolKeys *keys = dynamic_cast<SymbolKeys *>(_ref0->GetKeys(this, ptr));
        if (keys)
            keys->Add(s2, f * 30.0f, false);
    }
}

void BandDirector::ClearSymbolKeys(Symbol s) {
    const ObjPtr<RndPropAnim> &_ref0 = mPropAnim;
    if (_ref0) {
        DataArrayPtr ptr(s);
        SymbolKeys *keys = dynamic_cast<SymbolKeys *>(_ref0->GetKeys(this, ptr));
        if (keys)
            keys->clear();
    }
}

void BandDirector::ClearSymbolKeysFrameRange(Symbol s, float fstart, float fend) {
    if (mPropAnim) {
        DataArrayPtr ptr(s);
        SymbolKeys *keys = dynamic_cast<SymbolKeys *>(mPropAnim->GetKeys(this, ptr));
        if (keys) {
            int numkeys = keys->NumKeys();
            for (int i = 0; i < numkeys;) {
                float frame = 0;
                keys->FrameFromIndex(i, frame);
                frame /= 30.0f;
                if (frame >= fstart && frame <= fend)
                    numkeys = keys->RemoveKey(i);
                else
                    i++;
            }
        }
    }
}

void BandDirector::SetSongEnd(float f) { mEndOfSongSec = f; }

Symbol BandDirector::RemapCat(Symbol s1, Symbol s2) {
    DataArray *remaparr = BandWardrobe::GetRemap(s2);
    DataArray *foundarr = remaparr->FindArray(s1, false);
    if (foundarr) {
        s1 = foundarr->Sym(RandomInt(1, foundarr->Size()));
    }
    return s1;
}

#pragma push
#pragma dont_inline on
BEGIN_HANDLERS(BandDirector)
    HANDLE(first_shot_ok, OnFirstShotOK)
    HANDLE(shot_over, OnShotOver)
    HANDLE_ACTION(
        midi_add_postproc, OnMidiAddPostProc(_msg->Sym(2), _msg->Float(3), _msg->Float(4))
    )
    HANDLE(postproc_interp, OnPostProcInterp)
    HANDLE(save_song, OnSaveSong)
    HANDLE(on_file_loaded, OnFileLoaded)
    HANDLE(select_camera, OnSelectCamera)
    HANDLE_EXPR(is_music_video, IsMusicVideo())
    HANDLE(lightpreset_interp, OnLightPresetInterp)
    HANDLE(lightpreset_keyframe_interp, OnLightPresetKeyframeInterp)
    HANDLE(cycle_shot, OnCycleShot)
    HANDLE(force_shot, OnForceShot)
    HANDLE_EXPR(camera_source, mVenue.Dir())
    HANDLE(get_face_overrides, OnGetFaceOverrideClips)
    HANDLE_EXPR(facing_camera, FacingCamera(_msg->Sym(2)))
    HANDLE_ACTION(load_venue, LoadVenue(_msg->Sym(2), kLoadStayBack))
    HANDLE_ACTION(
        set_character_hide_hack_enabled, SetCharacterHideHackEnabled(_msg->Int(2))
    )
#ifdef MILO_DEBUG
    HANDLE(debug_char_interests, OnDebugInterestsForNextCharacter)
    HANDLE(toggle_interests_overlay, OnToggleInterestDebugOverlay)
    HANDLE(shot_annotate, OnShotAnnotate)
#endif
    HANDLE(cur_postprocs, OnPostProcs)
    HANDLE_EXPR(get_curworld, mCurWorld.Ptr())
    HANDLE_EXPR(get_world, mMerger ? mMerger->Dir() : (ObjectDir *)nullptr)
    HANDLE(set_dircut, OnSetDircut)
    HANDLE(force_preset, OnForcePreset)
    HANDLE(stomp_presets, OnStompPresets)
    HANDLE(midi_add_preset, OnMidiAddPreset)
    HANDLE_ACTION(midi_cleanup_presets, OnMidiPresetCleanup())
    HANDLE(get_cat_list, OnGetCatList)
    HANDLE(copy_cats, OnCopyCats)
    HANDLE(load_song, OnLoadSong)
    HANDLE(midi_shot_cat, OnMidiShotCategory)
    HANDLE_ACTION(add_symbol_key, AddSymbolKey(_msg->Sym(2), _msg->Sym(3), _msg->Float(4)))
    HANDLE_ACTION(clear_symbol_keys, ClearSymbolKeys(_msg->Sym(2)))
    HANDLE_ACTION(
        clear_symbol_keys_in_range,
        ClearSymbolKeysFrameRange(_msg->Sym(2), _msg->Float(3), _msg->Float(4))
    )
    HANDLE_ACTION(midi_harvest_dircuts, HarvestDircuts())
    HANDLE_ACTION(pick_new_shot, unk58 = 1)
    HANDLE_ACTION(set_end, SetSongEnd(_msg->Float(2)))
    HANDLE_MEMBER_PTR(LightPresetMgr())
    HANDLE_SUPERCLASS(RndPollable)
    HANDLE_SUPERCLASS(RndDrawable)
    HANDLE_SUPERCLASS(Hmx::Object)
    HANDLE_CHECK(0x513)
END_HANDLERS
#pragma pop

void BandDirector::FilterShot(int &flags) {
    if (mCurShot) {
        if (strstr(mCurShot->Category().mStr, "_behind")) {
            flags |= 0x10;
        } else {
            bool b1 = false;
            if (!(flags & 0x20U)) {
                if (strstr(mCurShot->Category().mStr, "_far")) {
                    b1 = true;
                }
            }
            if (b1)
                flags |= 0x100;
        }
    }
}

const char *BandDirector::PickDist(float *fp, char *c1, char *c2) {
    static const char *distNames[] = { "closeup", "near", "far", "behind" };
    float f1 = 0;
    for (int i = 0; i < 4; i++)
        f1 += fp[i];
    float randfl = RandomFloat(0.0f, f1);
    int d = 0;
    for (; d < 3; d++) {
        randfl = randfl - fp[d];
        if (randfl < 0.0f)
            break;
    }
    if (d == 0 && *c1 != 'v') {
        strcpy(c2, RandomFloat(0.0f, 100.0f) > 30.0f ? "_hand" : "_head");
    } else
        *c2 = '\0';
    MILO_ASSERT(d >= 0 && d < 4, 0x54D);
    return distNames[d];
}

DataNode BandDirector::OnMidiShotCategory(DataArray *da) {
    static int fs[] = { 1, 2, 4, 8, 3, 5, 9, 6, 0xA, 0xC, 0xD, 0xF };
    int mask = da->Int(2);
    Symbol s3c = TheBandWardrobe->GetPlayMode();
    int bits = CountBits(mask & 0xF);
    if (bits == 3 && (mask & 2)) {
        mask &= 0xfffffffd;
        bits = 2;
    }
    if (bits == 0) {
        mask |= fs[RandomInt(0, 0xC)];
        bits = CountBits(mask & 0xF);
    }
    if ((mask & 0x40) && (bits != 1)) {
        mask &= 0xffffffbf;
    }
    char buf[8];
    char buf2[8];
    float fls[] = { 4.0f, 3.0f, 4.0f, 1.0f };
    FilterShot(mask);
    if (bits == 4) {
        if (mask & 0x100) {
            mask &= 0xfffffffd;
            bits = 3;
        }
    }
    if (((mask & 0x10) != 0) || (!((bits != 2) || ((mask & 2) == 0)))) {
        fls[3] = 0.0f;
    }

    if ((mask & 0x20) || bits == 4) {
        bits = 4;
        mask |= 0x2F;
    } else
        fls[2] = 0.0f;

    if (bits != 1)
        mask |= 0x80;
    if (mask & 0x80)
        fls[0] = 0;
    if (mask & 0x60)
        fls[1] = 0;
    if (bits == 4)
        strcpy(buf, "all");
    else if (bits == 3)
        strcpy(buf, "front");
    else {
        bool b1 = mask & 1;
        int u5 = 0;
        if (b1) {
            buf[0] = 'b';
            u5 = 1;
        }
        if (mask & 2) {
            buf[u5] = 'd';
            u5++;
        }
        if (mask & 4) {
            buf[u5] = 'g';
            u5++;
        }
        if (mask & 8) {
            buf[u5] = 'v';
            u5++;
        }
        buf[u5] = 0;
    }
    return Symbol(MakeString("coop_%s_%s%s", buf, PickDist(fls, buf, buf2), buf2));
}

DataNode BandDirector::OnCycleShot(DataArray *da) {
    WorldDir *wdir = mVenue.Dir();
    if (wdir) {
        BandCamShot *shot =
            dynamic_cast<BandCamShot *>(wdir->CamManager().ShotAfter(mCurShot));
        ForceShot(shot);
    }
    return 0;
}

DataNode BandDirector::OnForceShot(DataArray *da) {
    WorldDir *wdir = mVenue.Dir();
    if (wdir) {
        ForceShot(wdir->Find<BandCamShot>(da->Str(2), false));
    }
    return 0;
}

DataNode BandDirector::OnLoadSong(DataArray *da) {
    FilePathTracker tracker(FileRoot());
    const char *songfile = da->Str(2);
    gIsLoadingDlc = FileIsDLC(songfile);
    MILO_LOG("BandDirector::OnLoadSong: is dlc? %s\n", gIsLoadingDlc ? "yes" : "no");
    Symbol s3 = da->Sym(3);
    int i4 = da->Int(4);
    Symbol s5 = da->Sym(5);
    bool i6 = da->Int(6);
    DataArray *genrearr = TypeDef()->FindArray("anim_genres");
    DataArray *s3arr = genrearr->FindArray(s3, false);
    if (s3arr)
        s3 = s3arr->Sym(1);
    else {
        s3 = "rocker";
        MILO_WARN("song %s has unknown genre %s, forcing to rocker", songfile, s3);
    }

    Symbol speed;
    if (i4 == 0x10)
        speed = "slow";
    else if (i4 == 0x40)
        speed = "fast";
    else
        speed = "medium";

    if (TheBandWardrobe) {
        TheBandWardrobe->SetSongInfo(speed, s5);
        TheBandWardrobe->SetSongAnimGenre(s3);
    }

    mMerger->Select("song", FilePath(songfile), true);
    if (i6)
        mMerger->StartLoad(mAsyncLoad);
    return 0;
}

DataNode BandDirector::OnFileLoaded(DataArray *da) {
    Symbol sym = da->Sym(2);
    ObjectDir *dir = da->Obj<ObjectDir>(3);
#ifdef HX_NATIVE
    if (getenv("GAME_DBG"))
        MILO_LOG("GAME_DBG: BandDirector::OnFileLoaded(sym='%s', dir=%p)\n",
                 sym.mStr ? sym.mStr : "(null)", (void *)dir);
#endif
    if (sym == song) {
        mEndOfSongSec = 0;
        if (dir) {
            mPropAnim = dir->Find<RndPropAnim>("song.anim", false);
            mSongPref = dir->Find<BandSongPref>("BandSongPref", false);
            if (TheBandWardrobe && mSongPref) {
                TheBandWardrobe->SetSongAnimGenre(mSongPref->GetAnimGenre());
            }
        }
        if (!mPropAnim) {
            unk110 = true;
            mPropAnim = Hmx::Object::New<RndPropAnim>();
            FileMerger::Merger *merger = mMerger->FindMerger("song", true);
            merger->mLoadedObjects.push_back(mPropAnim);
            mPropAnim->SetName("song.anim", mMerger->Dir());
            mPropAnim->SetType("song_anim");
            mPropAnim->SetRate(RndAnimatable::k480_fpb);
            mPropAnim->AddKeys(this, DataArrayPtr(Symbol("shot_bg")), PropKeys::kSymbol);
            mPropAnim->AddKeys(
                this, DataArrayPtr(Symbol("bass_intensity")), PropKeys::kSymbol
            );
            mPropAnim->AddKeys(
                this, DataArrayPtr(Symbol("drum_intensity")), PropKeys::kSymbol
            );
            mPropAnim->AddKeys(
                this, DataArrayPtr(Symbol("guitar_intensity")), PropKeys::kSymbol
            );
            mPropAnim->AddKeys(
                this, DataArrayPtr(Symbol("mic_intensity")), PropKeys::kSymbol
            );
            mPropAnim->AddKeys(this, DataArrayPtr(Symbol("crowd")), PropKeys::kSymbol);
            mPropAnim->AddKeys(
                this, DataArrayPtr(Symbol("world_event")), PropKeys::kSymbol
            );
            mPropAnim->AddKeys(
                this, DataArrayPtr(Symbol("lightpreset")), PropKeys::kSymbol
            );
            mPropAnim->SetInterpHandler(
                this, DataArrayPtr(Symbol("lightpreset")), Symbol("lightpreset_interp")
            );
            mPropAnim->AddKeys(
                this, DataArrayPtr(Symbol("lightpreset_keyframe")), PropKeys::kSymbol
            );
            mPropAnim->SetInterpHandler(
                this,
                DataArrayPtr(Symbol("lightpreset_keyframe")),
                Symbol("lightpreset_keyframe_interp")
            );
            mPropAnim->AddKeys(this, DataArrayPtr(Symbol("postproc")), PropKeys::kObject);
            mPropAnim->SetInterpHandler(
                this, DataArrayPtr(Symbol("postproc")), Symbol("postproc_interp")
            );
            mPropAnim->AddKeys(this, DataArrayPtr(Symbol("spot_bass")), PropKeys::kSymbol);
            mPropAnim->AddKeys(
                this, DataArrayPtr(Symbol("spot_drums")), PropKeys::kSymbol
            );
            mPropAnim->AddKeys(
                this, DataArrayPtr(Symbol("spot_guitar")), PropKeys::kSymbol
            );
            mPropAnim->AddKeys(
                this, DataArrayPtr(Symbol("spot_keyboard")), PropKeys::kSymbol
            );
            mPropAnim->AddKeys(
                this, DataArrayPtr(Symbol("spot_vocal")), PropKeys::kSymbol
            );
            AddStageKitKeys(mPropAnim, this);
            mPropAnim->AddKeys(
                this, DataArrayPtr(Symbol("part2_sing")), PropKeys::kSymbol
            );
            mPropAnim->AddKeys(
                this, DataArrayPtr(Symbol("part3_sing")), PropKeys::kSymbol
            );
            mPropAnim->AddKeys(
                this, DataArrayPtr(Symbol("part4_sing")), PropKeys::kSymbol
            );
        } else
            unk110 = false;
        const char *instIntensities[] = { "mic_intensity",  "bass_intensity",
                                          "drum_intensity", "guitar_intensity",
                                          "key_intensity",  0 };
        for (const char **ptr = instIntensities; *ptr != 0; ptr++) {
            DataArrayPtr dptr = DataArrayPtr(Symbol(*ptr));
            SymbolKeys *skeys =
                dynamic_cast<SymbolKeys *>(mPropAnim->GetKeys(this, dptr));
            if (skeys)
                skeys->unk30 = true;
        }
        if (!mVenue.Name().Null()) {
            if (TheBandWardrobe) {
                TheBandWardrobe->LoadCharacters(mVenue.Name(), mAsyncLoad);
                if (mCurWorld) {
                    FileMerger *m = mCurWorld->Find<FileMerger>("extras.fm", false);
                    if (m)
                        m->StartLoad(true);
                }
                if (dir) {
                    CharLipSync *sync = dir->Find<CharLipSync>("song.lipsync", false);
                    Symbol guitarmodeinst = mSongPref
                        ? GetModeInst(mSongPref->Part2Inst())
                        : GetModeInst("guitar");
                    Symbol bassmodeinst = mSongPref ? GetModeInst(mSongPref->Part3Inst())
                                                    : GetModeInst("bass");
                    Symbol drummodeinst = mSongPref ? GetModeInst(mSongPref->Part3Inst())
                                                    : GetModeInst("drum");

                    BandCharacter *bchar2 = nullptr;
                    BandCharacter *bchar3 = nullptr;
                    BandCharacter *bchar4 = nullptr;
                    CharLipSyncDriver *thelipsyncdriver = nullptr;
                    for (int i = 0; i < 4; i++) {
                        BandCharacter *bcharcur = TheBandWardrobe->GetCharacter(i);
                        Symbol bcharinst = bcharcur->InstrumentType();
                        if (bcharinst == "mic") {
                            bcharcur->SetLipSync(sync);
                            bcharcur->SetSingalong(1.0f);
                            thelipsyncdriver = bcharcur->GetLipSyncDriver();
                        } else if (bcharinst == guitarmodeinst)
                            bchar2 = bcharcur;
                        else if (bcharinst == bassmodeinst)
                            bchar3 = bcharcur;
                        else if (bcharinst == drummodeinst)
                            bchar4 = bcharcur;
                    }

                    if (bchar2) {
                        bchar2->SetSingalong(0.0f);
                        CharLipSync *lipsync2 =
                            dir->Find<CharLipSync>("part2.lipsync", false);
                        if (lipsync2)
                            bchar2->SetLipSync(lipsync2);
                        else
                            bchar2->SetSongOwner(thelipsyncdriver);
                    }
                    if (bchar3) {
                        bchar3->SetSingalong(0.0f);
                        CharLipSync *lipsync3 =
                            dir->Find<CharLipSync>("part3.lipsync", false);
                        if (lipsync3)
                            bchar3->SetLipSync(lipsync3);
                        else
                            bchar3->SetSongOwner(thelipsyncdriver);
                    }
                    if (bchar4) {
                        bchar4->SetSingalong(0.0f);
                        CharLipSync *lipsync4 =
                            dir->Find<CharLipSync>("part4.lipsync", false);
                        if (lipsync4)
                            bchar4->SetLipSync(lipsync4);
                        else
                            bchar4->SetSongOwner(thelipsyncdriver);
                    }
                }
            } else {
                FilePathTracker tracker(FileRoot());
                mChars.LoadFile(
                    FilePath("world/shared/world_chars.milo"),
                    false,
                    true,
                    kLoadFront,
                    false
                );
            }
        }
    }
    return 0;
}

void BandDirector::LoadVenue(Symbol s, LoaderPos pos) {
    FilePath fp;
    GetVenuePath(fp, s.mStr);
    mVenue.Load(fp, pos, mAsyncLoad);
}

void BandDirector::UnloadVenue(bool b) { mVenue.Unload(b); }

DataNode BandDirector::OnSaveSong(DataArray *da) { return 0; }

DataNode BandDirector::OnSelectCamera(DataArray *a) {
    if (!mDisabled) {
        if (mPropAnim) {
            float f3 = Max(TheTaskMgr.Seconds(TaskMgr::kRealTime) * 30.0f, 0.0f);
            if (LOADMGR_EDITMODE) {
                if (f3 == mPropAnim->GetFrame())
                    goto ok;
            }
            mPropAnim->SetFrame(f3, 1);
        }
    ok:
        if (LOADMGR_EDITMODE && TheTaskMgr.DeltaSeconds() < 0) {
            unke0 = -kHugeFloat;
        }
        if (!mNextShot && TheTaskMgr.Seconds(TaskMgr::kRealTime) >= unke0
            && !NoWorlds()) {
            mNextShot = FindNextDircut();
            if (!mNextShot && unk58)
                FindNextShot();
        }
    }
    PlayNextShot();
    return 0;
}

void ExtractPstCatAdjs(DataArray *arr, Symbol &s1, Symbol &s2) {
    DataNode eval2(arr->Evaluate(2));
    if (eval2.Type() == kDataSymbol) {
        s1 = eval2.Sym(arr);
    } else {
        MILO_WARN("unhandled light preset category at %f seconds\n", arr->Evaluate(4));
    }
    DataNode eval3(arr->Evaluate(3));
    if (eval3.Type() == kDataArray) {
        DataArray *evalarr = eval3.Array(arr);
        int arrsize = evalarr->Size();
        if (arrsize > 0) {
            s2 = evalarr->Sym(0);
            if (arrsize > 1) {
                MILO_WARN(
                    "unhandled light preset adjective: %s, %f secs\n",
                    evalarr->Str(1),
                    arr->Evaluate(4)
                );
            }
        }
    }
}

void ExtractCatAdj(Symbol s, Symbol &s1, Symbol &s2) {
    const char *sStr = s.mStr;
    char buf[256];
    StrNCopy(buf, sStr, 255);
    char *strplus = strstr(buf, "+");
    if (strplus) {
        *strplus = 0;
        s1 = strplus == buf ? gNullStr : buf;
        s2 = !(strplus + 1) ? gNullStr : strplus + 1;
    } else {
        s1 = s;
        s2 = gNullStr;
    }
}

Symbol ConcatCatAdj(Symbol s1, Symbol s2) {
    Symbol ret;
    if (s2 != gNullStr) {
        ret = MakeString("%s+%s", s1.mStr, s2.mStr);
    } else
        ret = s1;
    return ret;
}

void BandDirector::OnMidiPresetCleanup() {
    if (!mPropAnim || !mVenue.Dir())
        return;
    DataArrayPtr dptr(Symbol("lightpreset"));
    PropKeys *keys = mPropAnim->GetKeys(this, dptr);
    if (!keys)
        return;
    LightPresetManager *pm = &mVenue.Dir()->mPresetManager;
    Keys<Symbol, Symbol> &skeys = keys->AsSymbolKeys();
    Keys<Symbol, Symbol> local_keys;
    for (int i = 0; i < skeys.size(); i++) {
        Symbol s1, s2;
        ExtractCatAdj(skeys[i].value, s1, s2);
        if (s2 != gNullStr)
            s1 = s2;
        skeys[i].value = s1;
        LightPreset *lpreset = pm->PickRandomPreset(skeys[i].value);
        if (lpreset && i > 0) {
            float fadein = lpreset->LegacyFadeIn() / 480.0f;
            ClampEq(fadein, 0.0f, 4.0f);
            float stb = SecondsToBeat(skeys[i].frame / 30.0f);
            float bts = BeatToSeconds(Max(0.0f, stb - fadein));
            float newframe = bts * 30.0f;
            if (skeys[i - 1].frame <= newframe) {
                Key<Symbol> newkey(skeys[i - 1].value, newframe);
                local_keys.push_back(newkey);
            }
        } else if (!lpreset) {
            MILO_WARN(
                "Can't find light preset %s, %f secs", s1.mStr, skeys[i].frame / 30.0f
            );
        }
    }
    for (int i = 0; i < local_keys.size(); i++) {
        skeys.Add(local_keys[i].value, local_keys[i].frame, true);
    }
}

DataNode BandDirector::OnMidiAddPreset(DataArray *da) {
    MILO_ASSERT(mPropAnim, 0x7C4);
    DataArrayPtr dptr(Symbol("lightpreset"));
    SymbolKeys *skeys = dynamic_cast<SymbolKeys *>(mPropAnim->GetKeys(this, dptr));
    if (skeys) {
        Symbol s1, s2;
        ExtractPstCatAdjs(da, s1, s2);
        float frame = da->Float(4) * 30.0f;
        const Key<Symbol> *prev;
        const Key<Symbol> *next;
        float ref;
        int at = skeys->AtFrame(frame, prev, next, ref);
        if (prev) {
            Symbol s50 = prev->value;
            Symbol s54, s58;
            ExtractCatAdj(s50, s54, s58);
            if (s2 == gNullStr) {
                s2 = s1 == gNullStr ? gNullStr : s58;
            }
            if (s1 == gNullStr)
                s1 = s54;
            Symbol s5c = ConcatCatAdj(s1, s2);
            if (at >= 0 && frame == skeys->at(at).frame) {
                skeys->at(at).value = s5c;
            } else
                skeys->Add(s5c, frame, false);
        } else {
            Symbol s60 = ConcatCatAdj(s1, s2);
            if (s60 != gNullStr)
                skeys->Add(s60, frame, false);
        }
    }
    return 0;
}

BandCamShot *BandDirector::FindNextDircut() {
    float secs = TheTaskMgr.Seconds(TaskMgr::kRealTime);
    float delta = secs - TheTaskMgr.DeltaSeconds();
    Key<BandCamShot *> *key = mDircuts.GetFirstInRange(secs, delta);
    if (!key)
        return 0;
    else {
        if (key->value)
            unk58 = true;
        return key->value;
    }
}

int curInterestDebugChar = 5;

DataNode BandDirector::OnDebugInterestsForNextCharacter(DataArray *da) {
    int i4 = curInterestDebugChar;
    curInterestDebugChar = (curInterestDebugChar + 1) % 6;
    if (curInterestDebugChar == 4) {
        for (i4 = 0; i4 < 4; i4++) {
            BandCharacter *bc = GetCharacter(i4);
            if (bc)
                bc->SetDebugDrawInterestObjects(true);
        }
    } else if (curInterestDebugChar == 5) {
        RndOverlay *o = RndOverlay::Find("eye_status", false);
        if (o)
            o->SetShowing(false);
        for (i4 = 0; i4 < 4; i4++) {
            BandCharacter *bc = GetCharacter(i4);
            if (bc)
                bc->SetDebugDrawInterestObjects(false);
        }
    } else {
        RndOverlay *o = RndOverlay::Find("eye_status", false);
        if (o)
            o->SetShowing(true);
        int c = curInterestDebugChar;
        BandCharacter *bc = GetCharacter(c);
        if (bc)
            bc->SetDebugDrawInterestObjects(true);
        if (i4 < 4) {
            BandCharacter *bc = GetCharacter(i4);
            if (bc)
                bc->SetDebugDrawInterestObjects(false);
        }
    }
    return 0;
}

DataNode BandDirector::OnToggleInterestDebugOverlay(DataArray *da) {
    RndOverlay *o = RndOverlay::Find("eye_status", false);
    if (o)
        o->SetShowing(o->Showing() == 0);
    return 0;
}

DataNode BandDirector::OnShotAnnotate(DataArray *da) {
    char _slotpad[16]; (void)_slotpad;
    if (!mPropAnim)
        return 0;
    else {
        RndPropAnim *propanim = da->Obj<RndPropAnim>(2);
        DataArray *arr3 = da->Array(3);
        PropKeys *keys = propanim->GetKeys(this, arr3);
        int i7 = da->Int(4);
        if (!keys || 0 > i7)
            return 0;
        else {
            Keys<Symbol, Symbol> &skeys = keys->AsSymbolKeys();
            Key<Symbol> &skey = skeys[i7];
            Symbol symval = skey.value;
            DataArrayPtr ptr;
            float f1 = 0;
            if (i7 + 1 < skeys.size()) {
                f1 = (skeys[i7 + 1].frame - skey.frame) / 30.0f;
            }
            ptr->Insert(ptr->Size(), DataArrayPtr(skey.frame, MakeString("%.1f sec", f1)));
            if (strneq(symval.mStr, "directed_", 9)) {
                static DataArray *limits =
                    SystemConfig(
                        "objects", "BandCamShot", "types", "band", "dircut_limits"
                    )
                        ->Array(1);
                static DataArray *freeDircuts =
                    SystemConfig("objects", "BandCamShot", "types", "band", "free_dircuts")
                        ->Array(1);
                float f19 = 7.5f;
                float f21 = 0;
                DataArray *symarr = limits->FindArray(symval, false);
                float f18 = f19;
                float f20 = f19;
                if (symarr) {
                    f21 = symarr->Float(1);
                    float f17 = symarr->Float(2);
                    if (f17 > 7.5f)
                        f20 = f17;
                    f17 = symarr->Float(3);
                    if (f17 > 7.5f)
                        f18 = f17;
                    f17 = symarr->Float(4);
                    if (f17 > 7.5f)
                        f19 = f17;
                } else
                    MILO_NOTIFY_ONCE("could not find %s in dircut_limits", symval);

                ptr->Insert(ptr->Size(), DataArrayPtr(skey.frame + 7.5f, "fallback_end"));
                ptr->Insert(ptr->Size(), DataArrayPtr(skey.frame - f21, "zero_time"));
                ptr->Insert(ptr->Size(), DataArrayPtr(skey.frame + f20, "min_time"));
                ptr->Insert(ptr->Size(), DataArrayPtr(skey.frame + f18, "dur_min"));
                ptr->Insert(ptr->Size(), DataArrayPtr(skey.frame + f19, "dur_max"));

                for (int i = 0; i < freeDircuts->Size(); i++) {
                    if (symval == freeDircuts->Sym(i)) {
                        ptr->Insert(ptr->Size(), DataArrayPtr(skey.frame, "free_dircut"));
                        break;
                    }
                }
            }
            DataNode ret = DataNode(ptr, kDataArray);
        }
    }
}

DataNode BandDirector::OnPostProcs(DataArray *da) {
    DataNode *v2 = da->Var(2);
    DataNode *v3 = da->Var(3);
    DataNode *v4 = da->Var(4);
    *v2 = mPostProcA.Ptr();
    *v3 = mPostProcB.Ptr();
    *v4 = mPostProcBlend;
    return 0;
}

DataNode BandDirector::OnPostProcInterp(DataArray *da) {
    mPostProcA = da->Obj<RndPostProc>(2);
    mPostProcB = da->Obj<RndPostProc>(3);
    mPostProcBlend = da->Float(4);
    return 0;
}

DataNode BandDirector::OnShotOver(DataArray *da) {
    BandCamShot *shot = da->Obj<BandCamShot>(2);
    if (DirectedCut(shot->Category()) || BFTB(shot->Category())) {
        unk58 = true;
        unke0 = -1000.0f;
    } else
        unke0 = -1e+30f;
    return 0;
}

DataNode BandDirector::OnSetDircut(DataArray *da) {
    WorldDir *wdir = mVenue.Dir();
    if (wdir && !NoWorlds()) {
        Symbol sym = da->Sym(2);
        std::vector<CameraManager::PropertyFilter> filts;
        mNextShot =
            dynamic_cast<BandCamShot *>(wdir->CamManager().FindCameraShot(sym, filts));
    }
    return mNextShot.Ptr();
}

DataNode BandDirector::OnLightPresetInterp(DataArray *da) {
    if (da->Type(2) == kDataSymbol && da->Type(3) == kDataSymbol) {
        mLightPresetCatA = da->Sym(2);
        mLightPresetCatB = da->Sym(3);
        mLightPresetCatBlend = da->Float(4);
        if (da->Type(6) == kDataSymbol) {
            Symbol sym = da->Sym(6);
            if (mLightPresetCatA != mLightPresetCatB && sym != mLightPresetCatA) {
                mLightPresetCatB = mLightPresetCatA;
            }
        } else
            mLightPresetCatB = mLightPresetCatA;
    }
    return 0;
}

float FindFrameWithLeadIn() {
    float beat = TheTaskMgr.Beat();
    float bts = BeatToSeconds(beat + 4.0f);
    return bts * 30.0f;
}

void BandDirector::FindNextPstKeyframe(float f1, float f2, Symbol s) {
    unk108 = kHugeFloat;
    float stb = SecondsToBeat(f2 / 30.0f);
    float beat = TheTaskMgr.Beat();
    if ((stb - beat) + 1.1920929E-7f > 4.0f)
        unk108 = f2;
    else {
        DataArrayPtr ptr(Symbol("lightpreset_keyframe"));
        SymbolKeys *skeys = dynamic_cast<SymbolKeys *>(mPropAnim->GetKeys(this, ptr));
        if (skeys) {
            int idx = skeys->KeyGreaterEq(f1);
            Keys<Symbol, Symbol> &keys = skeys->AsSymbolKeys();
            for (; idx < keys.size(); idx++) {
                unk108 = keys[idx].frame;
                if (keys[idx].value != none) {
                    if (f1 != unk108)
                        break;
                }
            }
        }
    }
}

DataNode BandDirector::OnLightPresetKeyframeInterp(DataArray *da) {
    if (!mCurWorld)
        return 0;
    else {
        float leadin = FindFrameWithLeadIn();
        bool b10 = unk108 < 0.0f;
        if (leadin >= unk108) {
            Symbol s58;
            DataArrayPtr ptr(Symbol("lightpreset_keyframe"));
            SymbolKeys *skeys = dynamic_cast<SymbolKeys *>(mPropAnim->GetKeys(this, ptr));
            if (skeys) {
                int idx = skeys->KeyLessEq(unk108);
                Keys<Symbol, Symbol> &keys = skeys->AsSymbolKeys();
                if (idx > 0)
                    s58 = keys[idx].value;
            }
            if (SymToPstKeyframe(s58) != LightPreset::kPresetKeyframeNum) {
                LightPresetMgr()->SchedulePstKey(SymToPstKeyframe(s58));
            }
            b10 = true;
        }
        if (b10 || LOADMGR_EDITMODE) {
            float f5 = da->Float(5);
            Symbol s60 = da->Sym(3);
            float f108 = unk108;
            FindNextPstKeyframe(leadin, f5, s60);
            if (!b10 && f108 != unk108)
                LightPreset::ResetEvents();
        }
        return 0;
    }
}

DataNode BandDirector::OnForcePreset(DataArray *da) {
    if (LightPresetMgr()) {
        const DataNode &eval = da->Evaluate(2);
        float f3;
        if (da->Size() > 3)
            f3 = da->Float(3);
        else
            f3 = 0.0f;
        LightPreset *lp;
        if (eval.Type() == kDataSymbol || eval.Type() == kDataString) {
            lp = mCurWorld->Find<LightPreset>(eval.Str(), false);
        } else
            lp = eval.Obj<LightPreset>();
        LightPresetMgr()->ForcePreset(lp, f3);
    }
    return 0;
}

DataNode BandDirector::OnStompPresets(DataArray *da) {
    if (LightPresetMgr()) {
        const DataNode &eval2 = da->Evaluate(2);
        const DataNode &eval3 = da->Evaluate(3);
        LightPreset *lp1;
        LightPreset *lp2;

        if (eval2.Type() == kDataSymbol || eval2.Type() == kDataString) {
            lp1 = mCurWorld->Find<LightPreset>(eval2.Str(), false);
        } else
            lp1 = eval2.Obj<LightPreset>();

        if (eval3.Type() == kDataSymbol || eval3.Type() == kDataString) {
            lp2 = mCurWorld->Find<LightPreset>(eval3.Str(), false);
        } else
            lp2 = eval3.Obj<LightPreset>();

        LightPresetMgr()->StompPresets(lp1, lp2);
    }
    return 0;
}

DataNode BandDirector::OnGetCatList(DataArray *da) {
    Symbol s2 = da->Sym(2);
    DataArray *arr3 = da->Array(3);
    DataArray *arr = new DataArray(arr3->Size());

    int i1 = 0;
    for (int i = 0; i < arr3->Size(); i++) {
        Symbol cursym = arr3->Sym(i);
        if (RemapCat(cursym, s2) != cursym) {
            // ???
        } else {
            arr->Node(i1++) = cursym;
        }
    }
    arr->Resize(i1);
    DataNode ret(arr, kDataArray);
    arr->Release();
    return DataNode(arr, kDataArray);
}

DataNode BandDirector::OnCopyCats(DataArray *da) {
    if (!mPropAnim)
        return 0;
    else {
        Symbol s2 = da->Sym(2);
        String str30(s2.Str());
        str30.replace(0, 4, "shot");
        PropKeys *shotkeys =
            mPropAnim->GetKeys(this, DataArrayPtr(Symbol(str30.c_str())));
        PropKeys *shot5keys = mPropAnim->GetKeys(this, DataArrayPtr(Symbol("shot_5")));
        if (!shotkeys || !shot5keys)
            return 0;
        else {
            Keys<Symbol, Symbol> &sym5keys = shot5keys->AsSymbolKeys();
            Keys<Symbol, Symbol> &symkeys = shotkeys->AsSymbolKeys();
            symkeys.clear();
            for (int i = 0; i < sym5keys.size(); i++) {
                Key<Symbol> curkey(sym5keys[i]);
                curkey.value = RemapCat(curkey.value, s2);
                symkeys.push_back(curkey);
            }
        }
        return 0;
    }
}

#pragma push
#pragma fp_contract off
void BandDirector::OnMidiAddPostProc(Symbol s, float f1, float f2) {
    MILO_ASSERT(mPropAnim, 0x9A1);
    DataArrayPtr dptr(Symbol("postproc"));
    ObjectKeys *okeys = dynamic_cast<ObjectKeys *>(mPropAnim->GetKeys(this, dptr));
    if (okeys && mVenue.Dir()) {
        RndPostProc *proc = mVenue.Dir()->Find<RndPostProc>(s.Str(), false);
        if (proc) {
            float frame = f1 * 30.0f;
            const Key<ObjectStage> *prev;
            const Key<ObjectStage> *next;
            float ref;
            okeys->AtFrame(frame, prev, next, ref);
            if (prev) {
                okeys->Add(prev->value, frame, false);
            } else {
                RndPostProc *profilma =
                    mVenue.Dir()->Find<RndPostProc>("ProFilm_a.pp", false);
                if (profilma) {
                    if (frame > 0)
                        okeys->Add(profilma, 0, false);
                    okeys->Add(profilma, frame, false);
                }
            }
            okeys->Add(proc, frame + f2 * 30.0f, false);
        } else
            MILO_WARN("PostProc %s not found.  Cannot add to song.anim!\n", s.Str());
    }
}
#pragma pop

void BandDirector::ExportWorldEvent(Symbol s) {
    if (s != none) {
        if (mCurWorld) {
            static Message msg("");
            msg.SetType(s);
            mCurWorld->Export(msg, false);
        }
    }
}

void BandDirector::SendCurWorldMsg(Symbol s, bool b) {
    static Message msg("");
    if (mCurWorld) {
        msg.SetType(s);
        if (b)
            mCurWorld->HandleType(msg);
        else
            mCurWorld->Handle(msg, false);
    }
}

void BandDirector::SetCharSpot(Symbol s1, Symbol s2) {
    Symbol playmode = TheBandWardrobe->GetPlayMode();
    if (HiddenInstrument(playmode) != s1) {
        SendCurWorldMsg(MakeString("spotlight_%s_%s", s1.Str(), s2.Str()), false);
    }
}

void BandDirector::SetFog(Symbol) {}

Symbol BandDirector::GetModeInst(Symbol s) {
    if (s == "guitar" || s == "bass") {
        Symbol playmode = TheBandWardrobe->GetPlayMode();
        if (s == "guitar" && playmode == coop_bk)
            return keyboard;
        if (s == "bass" && playmode == coop_gk)
            return keyboard;
    }
    return s;
}

DataNode BandDirector::OnFirstShotOK(DataArray *da) {
    Symbol s2 = da->Sym(2);
    if (strncmp(s2.mStr, "coop_", 5) != 0)
        return 0;
    else {
        float f10 = mPropAnim->GetFrame();
        float f3c = kHugeFloat;
        Symbol symshottouse =
            TheBandWardrobe->PlayShot5() ? shot_5 : TheBandWardrobe->GetPlayMode();
        if (symshottouse == coop_bg)
            symshottouse = shot_bg;
        else if (symshottouse == coop_bk)
            symshottouse = shot_bk;
        else if (symshottouse == coop_gk)
            symshottouse = shot_gk;

        sPropArr->Node(0) = symshottouse;
        Keys<Symbol, Symbol> &skeys = mPropAnim->GetKeys(this, sPropArr)->AsSymbolKeys();

        Keys<Symbol, Symbol> *keys = &skeys;
        MILO_ASSERT(keys, 0xA42);

        bool b2 = false;
        bool b1 = false;
        if (unke0 == -1000.0f && mCurShot)
            b1 = true;
        if (b1) {
            if (DirectedCut(mCurShot->Category()) || BFTB(mCurShot->Category()))
                b2 = true;
        }
        if (b2)
            f10 += 30.0f;

        int idxafter = skeys.KeyGreaterEq(f10);
        if (idxafter < skeys.size()) {
            f3c = skeys[idxafter].frame;
            if (f3c == f10) {
                if (idxafter + 1 < skeys.size()) {
                    float fr = skeys[idxafter + 1].frame;
                    f3c = fr;
                } else
                    f3c = kHugeFloat;
            }
        }
        if (f3c == kHugeFloat)
            f3c = mEndOfSongSec * 30.0f;

        int dircutidxafter =
            mDircuts.KeyGreaterEq(TheTaskMgr.Seconds(TaskMgr::kRealTime));
        if (dircutidxafter < mDircuts.size() && mDircuts[dircutidxafter].value) {
            MinEq(f3c, mDircuts[dircutidxafter].frame * 30.0f);
        }
        return DataNode(f3c - mPropAnim->GetFrame());
    }
}

void BandDirector::SetCharacterHideHackEnabled(bool b) {
    int hack = BandCamShot::sHideAllCharactersHack - 1;
    bool hax = BandCamShot::sHideAllCharactersHack != 0;
    if (b)
        hack = BandCamShot::sHideAllCharactersHack + 1;
    BandCamShot::sHideAllCharactersHack = hack;
    if ((hack != 0) == hax)
        return;
    unk58 = true;
    unke0 = -1000.0f;
}

BEGIN_PROPSYNCS(BandDirector)
    SYNC_PROP_SET(shot_5, mShotCategory, SetShot(_val.Sym(), "shot_5"))
    SYNC_PROP_SET(shot_bg, mShotCategory, SetShot(_val.Sym(), coop_bg))
    SYNC_PROP_SET(shot_bk, mShotCategory, SetShot(_val.Sym(), coop_bk))
    SYNC_PROP_SET(shot_gk, mShotCategory, SetShot(_val.Sym(), coop_gk))
    SYNC_PROP_SET(postproc, NULL_OBJ, )
    SYNC_PROP_SET(lightpreset, verse, )
    SYNC_PROP_SET(lightpreset_keyframe, next, )
    SYNC_PROP_SET(world_event, none, ExportWorldEvent(_val.Sym()))
    SYNC_PROP(merger, mMerger)
    SYNC_PROP(disable_picking, mDisablePicking)
    SYNC_PROP(disabled, mDisabled)
    SYNC_PROP(lightpreset_interp_enabled, mLightPresetInterpEnabled)
    SYNC_PROP(excitement, mExcitement)
    SYNC_PROP(num_players_failed, mNumPlayersFailed)
    SYNC_PROP(cam_postproc, mCamPostProc)
    SYNC_PROP_SET(cur_shot, mCurShot, )
    SYNC_PROP_SET(cur_world, mCurWorld, )
    SYNC_PROP_SET(bass_intensity, Symbol("idle_realtime"), SendMessage("bass", _val.Sym()))
    SYNC_PROP_SET(drum_intensity, Symbol("idle_realtime"), SendMessage("drum", _val.Sym()))
    SYNC_PROP_SET(
        guitar_intensity, Symbol("idle_realtime"), SendMessage("guitar", _val.Sym())
    )
    SYNC_PROP_SET(mic_intensity, Symbol("idle_realtime"), SendMessage("mic", _val.Sym()))
    SYNC_PROP_SET(
        keyboard_intensity, Symbol("idle_realtime"), SendMessage("keyboard", _val.Sym())
    )
    SYNC_PROP_SET(
        part2_sing,
        Symbol("singalong_off"),
        SendMessage(
            mSongPref ? GetModeInst(mSongPref->Part2Inst()) : GetModeInst("guitar"),
            _val.Sym()
        )
    )
    SYNC_PROP_SET(
        part3_sing,
        Symbol("singalong_off"),
        SendMessage(
            mSongPref ? GetModeInst(mSongPref->Part3Inst()) : GetModeInst("bass"),
            _val.Sym()
        )
    )
    SYNC_PROP_SET(
        part4_sing,
        Symbol("singalong_off"),
        SendMessage(
            mSongPref ? GetModeInst(mSongPref->Part4Inst()) : GetModeInst("drum"),
            _val.Sym()
        )
    )
    SYNC_PROP_SET(crowd, Symbol("crowd_realtime"), SetCrowd(_val.Sym()))
    SYNC_PROP(propanim, mPropAnim)
    SYNC_PROP_SET(spot_bass, Symbol("off"), SetCharSpot(Symbol("bass"), _val.Sym()))
    SYNC_PROP_SET(spot_drums, Symbol("off"), SetCharSpot(Symbol("drums"), _val.Sym()))
    SYNC_PROP_SET(spot_guitar, Symbol("off"), SetCharSpot(Symbol("guitar"), _val.Sym()))
    SYNC_PROP_SET(
        spot_keyboard, Symbol("off"), SetCharSpot(Symbol("keyboard"), _val.Sym())
    )
    SYNC_PROP_SET(spot_vocal, Symbol("off"), SetCharSpot(Symbol("vocal"), _val.Sym()))
    SYNC_PROP_SET(stagekit_fog, Symbol("off"), SetFog(_val.Sym()))
    SYNC_SUPERCLASS(RndDrawable)
END_PROPSYNCS
