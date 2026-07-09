#include "char/CharDriver.h"
#include <cstdlib>
#ifdef HX_NATIVE
#include <map>
#include <set>
#include <string>
#endif
#include "CharClipDisplay.h"
#include "char/CharBoneDir.h"
#include "char/CharClip.h"
#include "char/CharClipDriver.h"
#include "char/CharClipGroup.h"
#include "char/Char.h"
#include "char/CharPollable.h"
#include "char/CharWeightable.h"
#include "decomp.h"
#include "math/Rand.h"
#include "math/Utl.h"
#include "obj/Data.h"
#include "obj/Dir.h"
#include "obj/Msg.h"
#include "obj/ObjMacros.h"
#include "obj/Object.h"
#include "obj/Task.h"
#include "obj/Utl.h"
#include "os/Debug.h"
#include "rndobj/Highlightable.h"
#include "rndobj/Poll.h"
#include "rndobj/Rnd.h"
#include "utl/Symbols.h"
#include "utl/Symbols2.h"
#include "utl/Symbols3.h"
#include "utl/Symbols4.h"

INIT_REVS(CharDriver)

DECOMP_FORCEACTIVE(CharDriver, "%s %s, beat: %.2f")

#ifdef HX_NATIVE
// ===========================================================================
// W25-CROWD FIX support (flag RB3_CROWD_CLIP_KEEP, default OFF).
// See src/system/char/CharDriver.cpp Poll() and docs/.../W25-CROWD/PLAN.md.
// Per-crowd-driver snapshot of the ambient walk clip's bank + name taken at
// first Play, used to re-establish the loop after an async load-merge destroys
// the playing clip and swaps the driver's mClips to a wrong sub-bank.
// ===========================================================================
struct CrowdKeepState {
    std::string clipName; // name of the ambient clip (survives the clip's destruction)
};
static std::map<const CharDriver *, CrowdKeepState> &gCrowdKeep() {
    static std::map<const CharDriver *, CrowdKeepState> m;
    return m;
}
static bool gCrowdClipKeepEnabled() {
    static int g = -1;
    if (g < 0) {
        const char *e = getenv("RB3_CROWD_CLIP_KEEP");
        g = (e && e[0] && e[0] != '0') ? 1 : 0;
    }
    return g != 0;
}
#endif

CharDriver::CharDriver()
    : mBones(this), mClips(this), mFirst(0), mTestClip(this), mDefaultClip(this),
      mDefaultPlayStarved(0), mStarvedHandler(), mLastNode(0), mOldBeat(kHugeFloat),
      mRealign(0), mBeatScale(1.0f), mBlendWidth(1.0f), mClipType(), mApply(kApplyBlend),
      mInternalBones(0), mPlayMultipleClips(0) {}

CharDriver::~CharDriver() {
    if (mFirst)
        mFirst->DeleteStack();
#ifdef HX_NATIVE
    // mBones can alias mInternalBones (SetBones / the blend path point mBones at
    // the internally-allocated CharBonesAlloc). `delete mInternalBones` frees that
    // object; the mBones ObjPtr member then auto-destructs (members destruct after
    // the body) and calls mPtr->Release(this) on the freed CharBonesObject → a
    // use-after-free SIGSEGV during venue/char teardown (e.g. unloading the splash
    // sv8 venue backdrop on the main_hub transition). Release the ObjPtr while its
    // target is still alive: if mBones aliases mInternalBones the Release runs on
    // live memory; otherwise it correctly drops the in-dir ref. Clears the dangling
    // before the raw delete.
    if (mBones.Ptr() == (CharBonesObject *)mInternalBones)
        mBones = (CharBonesObject *)nullptr;
#endif
    delete mInternalBones;
}

void CharDriver::Highlight() {
#ifdef MILO_DEBUG
    if (gCharHighlightY == -1.0f)
        CharDeferHighlight(this);
    else
        gCharHighlightY = Display(gCharHighlightY);
#endif
}

float CharDriver::Display(float f) {
    CharClipDisplay::Init(Dir());
    Vector2 curPos;
    std::vector<CharClipDisplay> displays;
    for (CharClipDriver *it = mFirst; it != nullptr; it = it->Next()) {
        displays.push_back(CharClipDisplay());
        displays.back().mBeat = it->mBeat;
        displays.back().SetClip(it->mClip, false);
        displays.back().mBlendFrac = it->mBlendFrac;
    }

    float lineSpacing = CharClipDisplay::LineSpacing();
    float y = f * (float)TheRnd->Height() + (float)displays.size() * lineSpacing;
    for (int i = 0.0f; i < displays.size(); i++) {
        displays[i].mY = -((float)i * lineSpacing - y);
    }

    MsgSource *source = CharClipDisplay::FindSource(this);
    float result = (y + (float)(1 + (source != nullptr)) * lineSpacing) / (float)TheRnd->Height();
    Hmx::Color bgColor(0, 0, 0, 0.5f);
    Hmx::Rect rect(0, f, 1.0f, result - f);
    TheRnd->DrawRectScreen(rect, bgColor, nullptr, nullptr, nullptr);

    Hmx::Color textColor(1, 1, 1, 1);
    TheRnd->DrawString(
        MakeString("%s %s, beat: %.2f", Dir()->Name(), PathName(this), mOldBeat),
        Vector2(CharClipDisplay::sEm, lineSpacing * 0.1f + f * (float)TheRnd->Height()),
        textColor,
        true
    );

    for (unsigned int i = 0; i < displays.size(); i++) {
        displays[i].DrawTrack();
    }

    int idx = 0;
    CharClipDriver *prev = mFirst;
    CharClipDriver *next;
    while (prev && (next = prev->Next()) != nullptr) {
        CharClipDisplay *prevDisplay = &displays[idx];
        CharClipDisplay *nextDisplay = &displays[idx + 1];
        CharClip::NodeVector *nodes = next->GetClip()->mTransitions.FindNodes(prev->GetClip());
        if (nodes) {
            for (int i = 0; i < nodes->size; i++) {
                int curOfs = 0;
                int nextOfs = 0;
                curPos.x = nextDisplay->GetX(nodes->nodes[i].curBeat);
                for (int j = 0; j < i; j++) {
                    if (std::fabs(curPos.x - nextDisplay->GetX(nodes->nodes[j].curBeat)) < 8.0f) {
                        curOfs += 11;
                    }
                }
                Hmx::Color redColor(1, 0, 0);
                curPos.y = (nextDisplay->mY + (1.0f + (float)curOfs));
                TheRnd->DrawString(MakeString("%d", i), curPos, redColor, true);

                Vector2 nextPos;
                nextPos.x = prevDisplay->GetX(nodes->nodes[i].nextBeat);
                for (int j = 0; j < i; j++) {
                    if (std::fabs(nextPos.x - prevDisplay->GetX(nodes->nodes[j].nextBeat)) < 8.0f) {
                        nextOfs += 11;
                    }
                }
                Hmx::Color greenColor(0, 1, 0);
                nextPos.y = prevDisplay->mY - 14.0f - (float)nextOfs;
                TheRnd->DrawString(MakeString("%d", i), nextPos, greenColor, true);
            }
        }
        nextDisplay->DrawBlend(next->mBeat + prev->mRampIn, prev->mBlendWidth);
        float rampIn = prev->mRampIn;
        if (rampIn < 0.0f) rampIn = 0.0f;
        prevDisplay->DrawBlend(prev->mBeat + rampIn, prev->mBlendWidth);
        prev = prev->Next();
        idx++;
    }

    for (unsigned int i = 0; i < displays.size(); i++) {
        displays[i].DrawCursor();
    }

    if (source) {
        static Message msg("debug_draw", DataNode(2.0f), DataNode(2.0f));
        msg[0] = displays[0].mY + lineSpacing;
        msg[1] = TheTaskMgr.Beat();
        source->Handle(msg, false);
    }

    return result;
}

void CharDriver::Enter() {
#ifdef HX_NATIVE
    // W25-CROWD probe: capture mFirst BEFORE Clear() wipes it, so the ENTER
    // probe below can report whether a live clip was playing at re-Enter.
    CharClipDriver *sEnterMFirstAtEntry = mFirst;
#endif
    Clear();
    mLastNode = 0;
    mOldBeat = kHugeFloat;
    mBeatScale = 1.0f;
    RndPollable::Enter();
#ifdef HX_NATIVE
    // W25-CROWD STEP 0: confirm CharDriver::Enter fires on the crowd proxy
    // driver, and whether mDefaultClip is authored at that moment. Inert unless
    // CHARDRV_PROBE set; matches by owning-dir name / clipType.
    {
        const char *dp = getenv("CHARDRV_PROBE");
        if (dp) {
            ObjectDir *owndir = Dir();
            const char *dn = (owndir && owndir->Name()) ? owndir->Name() : "";
            const char *ct = mClipType.Str();
            if (dp[0] == '*' || (dn[0] && strstr(dn, dp)) || (ct && strstr(ct, dp))) {
                Hmx::Object *dc = mDefaultClip.Ptr();
                static int gSeq = 0;
                // NOTE: this probe runs AFTER Clear() at the top of Enter, so
                // mFirst is already null here. mFirstAtEntry captured below.
                fprintf(stderr,
                    "[CHARDRV_ENTER] seq=%d this=%p dir='%s' clipType='%s' defClip=%p "
                    "defName='%s' beat=%.3f mFirstAtEntry=%p\n",
                    gSeq++, (void *)this, dn[0] ? dn : "?", ct ? ct : "?", (void *)dc,
                    (dc && dc->Name()) ? dc->Name() : "-", TheTaskMgr.Beat(),
                    (void *)sEnterMFirstAtEntry);
            }
        }
    }
#endif
    if (mDefaultClip)
        Play(DataNode(mDefaultClip), 1, -1.0f, kHugeFloat, 0.0f);
}

void CharDriver::Exit() { RndPollable::Exit(); }

void CharDriver::Clear() {
#ifdef HX_NATIVE
    // W25-CROWD: catch every Clear() that wipes a LIVE clip on a crowd driver,
    // so we can identify the caller that kills the just-started walk clip
    // (Enter / SyncInternalBones / SetClipType / SetApply / Transfer). Probe-only.
    {
        const char *dp = getenv("CHARDRV_PROBE");
        if (dp && mFirst) {
            ObjectDir *owndir = Dir();
            const char *dn = (owndir && owndir->Name()) ? owndir->Name() : "";
            const char *ct = mClipType.Str();
            if (dp[0] == '*' || (dn[0] && strstr(dn, dp)) || (ct && strstr(ct, dp)))
                fprintf(stderr,
                    "[CHARDRV_CLEAR] dir='%s' clipType='%s' mFirst=%p beat=%.3f\n",
                    dn[0] ? dn : "?", ct ? ct : "?", (void *)mFirst, TheTaskMgr.Beat());
        }
    }
#endif
    if (mFirst)
        mFirst->DeleteStack();
    mFirst = nullptr;
}

void CharDriver::Transfer(const CharDriver &driver) {
    Clear();
    mClips = driver.mClips;
    mLastNode = driver.mLastNode;
    mRealign = driver.mRealign;
    mBeatScale = driver.mBeatScale;
    mBlendWidth = driver.mBlendWidth;
    if (driver.mFirst)
        mFirst = new CharClipDriver(this, *driver.mFirst);
}

void CharDriver::SetClips(ObjectDir *dir) {
    if (dir != mClips) {
        mLastNode = NULL_OBJ;
        mClips = dir;
    }
}

void CharDriver::SetBones(CharBonesObject *obj) { mBones = obj; }

void CharDriver::SetApply(ApplyMode mode) {
    if (mode != mApply) {
        mApply = mode;
        SyncInternalBones();
    }
}

void CharDriver::SetClipType(Symbol ty) {
    if (mClipType != ty) {
        mClipType = ty;
        SyncInternalBones();
    }
}

void CharDriver::SyncInternalBones() {
    Clear();
    mLastNode = NULL_OBJ;
    if (mInternalBones && mClipType.Null()) {
        RELEASE(mInternalBones);
    } else if (!mInternalBones && mApply == kApplyBlendWeights && !mClipType.Null()) {
        mInternalBones = new CharBonesAlloc();
    }
    if (mInternalBones) {
        mInternalBones->ClearBones();
        CharBoneDir::StuffBones(*mInternalBones, mClipType);
    }
}

bool CharDriver::Starved() {
    bool ret;
    if (mFirst) {
        ret = false;
        if (!mFirst->Next() && (mFirst->mPlayFlags & 0xF0) != 0x10)
            ret = true;
    } else {
        ret = true;
    }
    return ret;
}

CharClip *MyFindClip(const DataNode &n, ObjectDir *dir) {
    const DataNode &node = n.Evaluate();
    Hmx::Object *obj;
    if (node.Type() == kDataObject) {
        obj = node.mValue.object;
    } else {
        MILO_ASSERT(node.Type() == kDataSymbol || node.Type() == kDataString, 0x12A);
        obj = dir->FindObject(node.LiteralStr(), false);
    }
    if (!obj)
        return nullptr;
    else {
        CharClip *clip = dynamic_cast<CharClip *>(obj);
        if (clip)
            return clip;
        else {
            CharClipGroup *group = dynamic_cast<CharClipGroup *>(obj);
            if (!group) {
                MILO_NOTIFY_ONCE(
                    "%s: MyFindClip %s bad object type, not CharClip or CharClipGroup",
                    PathName(dir),
                    PathName(obj)
                );
                clip = nullptr;
            } else
                clip = group->GetClip();
        }
        return clip;
    }
}

CharClip *CharDriver::FindClip(const DataNode &node, bool warn) {
    if (!mClips) {
        MILO_FAIL("%s: trying to FindClip with no mClips", PathName(this));
    }
    CharClip *clip = MyFindClip(node, mClips);
    if (!clip && warn) {
        String str;
        str << node;
        MILO_NOTIFY_ONCE("%s: missing \"%s\" in %s", PathName(this), str, mClips->Name());
    }
    return clip;
}

CharClipDriver *CharDriver::Play(CharClip *clip, int i, float f1, float f2, float f3) {
    if (!clip) {
        MILO_NOTIFY_ONCE("%s: Could not find clip to play.", PathName(this));
        return nullptr;
    } else {
        mLastNode = clip;
        if (f1 == -1.0f)
            f1 = mBlendWidth;
        if (mPlayMultipleClips) {
            for (CharClipDriver *it = mFirst; it != nullptr; it = it->Next()) {
                if (clip == it->GetClip())
                    return nullptr;
            }
        }
        mFirst =
            new CharClipDriver(this, clip, i, f1, mFirst, f2, f3, mPlayMultipleClips);
#ifdef HX_NATIVE
        // W25-CROWD FIX (RB3_CROWD_CLIP_KEEP): snapshot, per crowd driver, the
        // bank (mClips) and clip NAME of the FIRST ambient walk clip it plays.
        // A later async load-merge both destroys the playing clip AND swaps this
        // driver's mClips ObjPtr to a wrong player-only sub-bank — but the
        // W25-CROWD (RB3_CROWD_CLIP_KEEP): snapshot, per crowd driver, the NAME
        // of the first ambient walk clip it plays. Keyed on the clip NAME prefix
        // (crowd*) — precise to the crowd walk clips and independent of when
        // mClipType is stamped 'crowd' (the merge sets that AFTER this initial
        // play). Name-only (survives the clip's later destruction); consumed by
        // the clipType=='crowd'-scoped re-arm in Poll (A7).
        if (gCrowdClipKeepEnabled() && clip->Name() &&
            strncmp(clip->Name(), "crowd", 5) == 0) {
            CrowdKeepState &st = gCrowdKeep()[this];
            if (st.clipName.empty())
                st.clipName = clip->Name();
        }
        // W25-CROWD: log each Play() on a crowd driver — clip name, flags, beat.
        // Tells us HOW OFTEN and at WHAT BEAT the vignette re-triggers a walk
        // clip (the loop cadence) vs. it only ever firing once at Enter.
        {
            const char *dp = getenv("CHARDRV_PROBE");
            if (dp) {
                ObjectDir *owndir = Dir();
                const char *dn = (owndir && owndir->Name()) ? owndir->Name() : "";
                const char *ct = mClipType.Str();
                if (dp[0] == '*' || (dn[0] && strstr(dn, dp)) || (ct && strstr(ct, dp)))
                    fprintf(stderr,
                        "[CHARDRV_PLAY] dir='%s' clip='%s' flags=0x%x beat=%.3f\n",
                        dn[0] ? dn : "?", clip->Name() ? clip->Name() : "?", (unsigned)i,
                        TheTaskMgr.Beat());
            }
        }
#endif
        return mFirst;
    }
}

CharClipDriver *
CharDriver::Play(const DataNode &node, int i, float f1, float f2, float f3) {
    DataNode thisnode(node);
    CharClip *found = FindClip(node, true);
    CharClipDriver *driver = Play(found, i, f1, f2, f3);
    mLastNode = thisnode;
    return driver;
}

CharClipDriver *
CharDriver::PlayGroup(const char *cc, int i, float f1, float f2, float f3) {
    if (!mClips) {
        MILO_WARN("%s has no clips", PathName(this));
        return nullptr;
    } else {
        CharClipGroup *grp = mClips->Find<CharClipGroup>(cc, false);
        if (!grp) {
            MILO_WARN("%s could not find group %s", PathName(this), cc);
            return nullptr;
        } else
            return Play(grp->GetClip(), i, f1, f2, f3);
    }
}

void CharDriver::SetStarved(Symbol starved) { mStarvedHandler = starved; }

void CharDriver::SetBeatScale(float beatscale, bool) {
    CharClipDriver *playing = FirstPlaying();
    if (playing) {
        float oldbeatscale = mBeatScale;
        for (CharClipDriver *d = playing; d != nullptr; d = d->Next()) {
            if ((playing->mPlayFlags & 0xF600) != 0x200) {
                CharClip::SetDefaultBeatAlignModeFlag(d->mPlayFlags, 0);
                d->mTimeScale *= oldbeatscale / beatscale;
            }
        }
    }
    mBeatScale = beatscale;
}

void CharDriver::Poll() {
#ifdef HX_NATIVE
    { static int g = -1; if (g < 0) g = getenv("RB3_NO_CLIP") ? 1 : 0; if (g) return; }
    // wave-07 CHARDRV_PROBE: confirm CharDriver::Poll runs + whether a clip is
    // playing (mFirst non-null) for the named clip-type. Default OFF. Match a
    // driver by ClipType() substring or "*".
    {
        const char *dp = getenv("CHARDRV_PROBE");
        if (dp) {
            const char *ct = mClipType.Str();
            // W25-CROWD STEP 0: also match by owning-dir name so the crowd
            // proxy driver ("main.drv" under crowd_maleNN) is caught even when
            // its clipType is empty. Print mDefaultClip + mClips size/name to
            // resolve the A4 decision tree (null default vs. Enter-never-fired
            // vs. no-default-authored).
            ObjectDir *owndir = Dir();
            const char *dn = (owndir && owndir->Name()) ? owndir->Name() : "";
            bool match = (dp[0] == '*') || (ct && strstr(ct, dp)) ||
                         (dn[0] && strstr(dn, dp));
            if (match) {
                static int n = 0;
                if ((n++ % 60) == 0) {
                    Hmx::Object *dc = mDefaultClip.Ptr();
                    ObjectDir *cl = mClips.Ptr();
                    int nclips = 0;
                    if (cl) { for (ObjDirItr<CharClip> ci(cl, true); ci; ++ci) nclips++; }
                    fprintf(stderr,
                        "[CHARDRV] this=%p dir='%s' clipType='%s' mFirst=%p bones=%p "
                        "defClip=%p defName='%s' defStarved=%d clips=%p clipsName='%s' "
                        "nclips=%d apply=%d\n",
                        (void *)this, dn[0] ? dn : "?", ct ? ct : "?", (void *)mFirst,
                        (void *)mBones, (void *)dc,
                        (dc && dc->Name()) ? dc->Name() : "-", (int)mDefaultPlayStarved,
                        (void *)cl, (cl && cl->Name()) ? cl->Name() : "-", nclips,
                        (int)mApply);
                }
                // W25-CROWD lifecycle counters: how many frames this driver has
                // mFirst set, a non-null FirstPlaying() (mBlendFrac>0), and is
                // Starved(). Rolled-up print every 120 polls so we see the true
                // duty-cycle rather than a %60 snapshot. Uses static maps keyed
                // by `this` — probe-only, HX_NATIVE.
                {
                    static std::map<const CharDriver *, int> gFrames, gFirst, gPlaying, gStarved;
                    static std::map<const CharDriver *, void *> gPrevFirst;
                    int &fr = gFrames[this];
                    fr++;
                    // Transition detector: log the exact Poll where mFirst goes
                    // set -> null (the frame the walk clip dies).
                    {
                        void *&pf = gPrevFirst[this];
                        if (pf && !mFirst)
                            fprintf(stderr,
                                "[CHARDRV_DIE] dir='%s' pollFrame=%d beat=%.3f (mFirst set->null)\n",
                                dn[0] ? dn : "?", fr, TheTaskMgr.Beat());
                        pf = (void *)mFirst;
                    }
                    if (mFirst) gFirst[this]++;
                    if (FirstPlaying()) gPlaying[this]++;
                    if (Starved()) gStarved[this]++;
                    if ((fr % 120) == 0)
                        fprintf(stderr,
                            "[CHARDRV_LIFE] dir='%s' frames=%d firstSet=%d playing=%d starved=%d\n",
                            dn[0] ? dn : "?", fr, gFirst[this], gPlaying[this], gStarved[this]);
                }
            }
        }
    }
#endif
    float f17 = mBeatScale * TheTaskMgr.Beat();
    float f13 = mBeatScale * TheTaskMgr.DeltaBeat();
    if (mRealign && f17 > 0) {
        const SongPos &thePos = TheTaskMgr.mSongPos;
        f17 =
            mBeatScale * ((float)(thePos.GetBeat()) + (float)(thePos.GetTick()) / 480.0f);
        if (mOldBeat == kHugeFloat)
            mOldBeat = f17;
        if (std::floor(f17) != std::floor(mOldBeat)) {
            CharClipDriver *playing = FirstPlaying();
            if (playing) {
                unsigned int firstFlags = CharClipDriver::GetUpperFlags(playing->mPlayFlags);
                int flags = firstFlags;
                for (CharClipDriver *it = playing->Next(); it != nullptr;
                     it = it->Next()) {
                    MaxEq(flags, CharClipDriver::GetUpperFlags(it->mPlayFlags));
                }
                flags--;
                if (flags > 0) {
                    int i12 = (int)std::floor(f17) ^ (int)std::floor(mOldBeat) + 1;
                    if (i12 & flags) {
                        CharClipDriver *d = playing;
                        while (d) {
                            d->mPlayFlags &= 0xffff0fff;
                            d = d->Next();
                        }
                        if (firstFlags - 1 > 0 && (i12 & firstFlags - 1)) {
                            Play(playing->GetClip(), 0x38, -1, kHugeFloat, 0);
                        }
                    }
                }
            }
        }
    }
    mOldBeat = f17;
#ifdef HX_NATIVE
    // W25-CROWD: one-shot report of the crowd driver's starved-replay wiring —
    // mStarvedHandler symbol + the playFlags of the current mFirst (if any) —
    // to determine WHICH starved-replay path (handler / 0x30 / 0x40 / default)
    // is supposed to loop these ambient walk clips. Probe-only.
    {
        const char *dp = getenv("CHARDRV_PROBE");
        if (dp) {
            ObjectDir *owndir = Dir();
            const char *dn = (owndir && owndir->Name()) ? owndir->Name() : "";
            const char *ct = mClipType.Str();
            if (dp[0] == '*' || (dn[0] && strstr(dn, dp)) || (ct && strstr(ct, dp))) {
                static std::set<const CharDriver *> seen;
                if (Starved() && seen.insert(this).second) {
                    const char *sh = mStarvedHandler.Null() ? "-" : mStarvedHandler.Str();
                    fprintf(stderr,
                        "[CHARDRV_STARVE] dir='%s' starvedHandler='%s' mFirst=%p "
                        "firstFlags=0x%x defStarved=%d\n",
                        dn[0] ? dn : "?", sh ? sh : "-", (void *)mFirst,
                        mFirst ? (unsigned)mFirst->mPlayFlags : 0u,
                        (int)mDefaultPlayStarved);
                }
            }
        }
    }
#endif
    if (Starved() && !mStarvedHandler.Null()) {
        Dir()->Handle(Message(mStarvedHandler), true);
    }
    if (Starved() && mFirst) {
        if ((mFirst->mPlayFlags & 0xF0) == 0x30) {
            int flags = mFirst->mPlayFlags;
            CharClip::SetDefaultBlendFlag(flags, 4);
            Play(mFirst->GetClip(), flags, -1, kHugeFloat, 0);
        }
    }
    if (Starved() && mFirst && (mFirst->mPlayFlags & 0xF0) == 0x40) {
        Play(mLastNode, 0x44, -1, kHugeFloat, 0);
    }
    if (Starved() && mDefaultClip && mDefaultPlayStarved) {
        Play(DataNode(mDefaultClip), 0x44, -1, kHugeFloat, 0);
    }
#ifdef HX_NATIVE
    // ---------------------------------------------------------------------
    // W25-CROWD (flag RB3_CROWD_CLIP_KEEP, default OFF; scoped to
    // clipType=='crowd' ONLY). Root cause (full evidence in W25-CROWD/STATUS.md
    // + PLAN.md): the sv3_a hub crowd proxies are driven by a one-shot
    // `play_clip` (kPlayLoop|kPlayRealTime walk, e.g. `crowd3.clp`) fired at
    // boot, but a native async load-merge at ~beat 2.4 does TWO things:
    //   (a) DESTROYS the playing clip object -> Hmx::Object::~Object ->
    //       Replace(clip,NULL) -> CharDriver::Replace -> DeleteClip pops it ->
    //       mFirst NULL; and
    //   (b) swaps THIS driver's `mClips` ObjPtr to a wrong player-only sub-bank
    //       that holds zero `crowd*` clips. The one crowd proxy that was never
    //       triggered keeps its full crowd bank intact.
    // These drivers author NO default clip / starved handler and kPlayLoop(0x20)
    // is not a starved-replay branch, so nothing re-establishes the loop -> the
    // skeleton is undriven -> the skin palette scrambles (RB3-native only).
    //
    // PARTIAL re-arm (the only lane-scoped, crash-safe recovery available):
    // when a crowd driver is Starved with mFirst==NULL, re-resolve the ambient
    // clip name snapshotted at first Play against THIS driver's OWN current
    // (live ObjPtr) mClips and re-Play it looping/real-time. This recovers any
    // crowd driver whose bank survived the merge intact. It does NOT recover the
    // drivers whose mClips was swapped to a crowd-less sub-bank (defect (b)) —
    // re-arming those requires reaching another bank, and every cross-driver /
    // cached-pointer path proved to be a use-after-free against the active merge
    // (proc SIGSEGV at beat 2.4). The clip-destruction + bank-swap is a native
    // load-merge defect that must be fixed engine-side; see STATUS.md for the
    // hand-off charter (that fix is out of this lane's A7/A5 scope).
    //
    // A7-safe: gated by clipType=='crowd' (UNIQUE to the 8 hub proxies — band
    // players/extras are 'vignette', gameplay WorldCrowd has NO CharDriver at
    // all), only this driver's own live ObjPtr bank is dereferenced, and the
    // whole block is HX_NATIVE-only so the Wii object is byte-identical.
    // mLastNode is NOT reused (it dangles at the destroyed clip).
    if (gCrowdClipKeepEnabled() && mClipType == Symbol("crowd") && !mFirst &&
        mClips && Starved()) {
        std::map<const CharDriver *, CrowdKeepState>::iterator it =
            gCrowdKeep().find(this);
        if (it != gCrowdKeep().end() && !it->second.clipName.empty()) {
            Hmx::Object *obj = mClips->FindObject(it->second.clipName.c_str(), false);
            CharClip *amb = obj ? dynamic_cast<CharClip *>(obj) : nullptr;
            if (amb) {
                int flags = CharClip::kPlayLoop | CharClip::kPlayRealTime;
                Play(amb, flags, -1.0f, kHugeFloat, 0.0f);
            }
        }
    }
#endif
    if (mFirst) {
#ifdef HX_NATIVE
        // W25-CROWD: capture the moment PreEvaluate pops the looping crowd clip
        // (mFirst set -> null). Report mPlayMultipleClips + beat vs EndBeat so we
        // know whether the mPlayMultipleClips PreEvaluate Exit(false) path (which
        // fires BEFORE Evaluate's kPlayLoop wrap) is what kills the loop.
        {
            const char *dp = getenv("CHARDRV_PROBE");
            CharClipDriver *before = mFirst;
            CharClip *bclip = before ? before->GetClip() : nullptr;
            float bbeat = before ? before->mBeat : 0.0f;
            mFirst = mFirst->PreEvaluate(f17, f13, TheTaskMgr.DeltaSeconds());
            if (dp && before && !mFirst) {
                ObjectDir *owndir = Dir();
                const char *dn = (owndir && owndir->Name()) ? owndir->Name() : "";
                const char *ct = mClipType.Str();
                if (dp[0] == '*' || (dn[0] && strstr(dn, dp)) || (ct && strstr(ct, dp)))
                    fprintf(stderr,
                        "[CHARDRV_POP] dir='%s' clip='%s' multiClips=%d beat=%.3f "
                        "endBeat=%.3f flags=0x%x\n",
                        dn[0] ? dn : "?", bclip && bclip->Name() ? bclip->Name() : "?",
                        (int)mPlayMultipleClips, bbeat,
                        bclip ? bclip->EndBeat() : -1.0f, (unsigned)before->mPlayFlags);
            }
        }
#else
        mFirst = mFirst->PreEvaluate(f17, f13, TheTaskMgr.DeltaSeconds());
#endif
    }
    if (mFirst) {
        float f14 = Weight();
        f13 = mFirst->Evaluate(f17, f13, TheTaskMgr.DeltaSeconds());
        f13 = -(f14 * f13 - 1.0f);
        if (mPlayMultipleClips)
            f13 = f14;
#ifdef HX_NATIVE
        // wave-07 CHARDRV_APPLY: did we reach the ScaleAdd(*mBones) apply for a
        // driver with a real playing clip? Reports weight f14 + whether mBones set.
        {
            const char *dp = getenv("CHARDRV_PROBE");
            if (dp) {
                const char *ct = mClipType.Str();
                if (dp[0] == '*' || (ct && strstr(ct, dp))) {
                    static int m = 0;
                    if ((m++ % 60) == 0)
                        fprintf(stderr,
                            "[CHARDRV_APPLY] this=%p clipType='%s' mBones=%p weight=%.3f "
                            "apply=%d clip='%s'\n",
                            (void *)this, ct ? ct : "?", (void *)mBones, f14, (int)mApply,
                            mFirst->GetClip() && mFirst->GetClip()->Name()
                                ? mFirst->GetClip()->Name() : "?");
                }
            }
        }
#endif
        if (mBones) {
            if (mApply == kApplyBlend || mApply == kApplyBlendWeights) {
                if (mInternalBones) {
                    mInternalBones->Enter();
                    mFirst->ScaleAdd(*mInternalBones, f14);
                    mInternalBones->Blend(*mBones);
                } else {
                    mFirst->GetClip()->ScaleDown(*mBones, f13);
                    mFirst->ScaleAdd(*mBones, f14);
                }
            } else if (mApply == kApplyAdd) {
                mFirst->ScaleAdd(*mBones, f14);
            } else { // kApplyRotateTo
                MILO_ASSERT(mApply == kApplyRotateTo, 0x22F);
                mFirst->RotateTo(*mBones, f14);
            }
        }
    }
}

float CharDriver::EvaluateFlags(int flags) {
    float f4 = 1;
    float f3 = 0;
    for (CharClipDriver *it = mFirst; it != nullptr; it = it->Next()) {
        float sigmoid = Sigmoid(it->mBlendFrac);
        if (flags & it->GetClip()->Flags()) {
            f3 += f4 * sigmoid;
        }
        f4 *= 1.0f - sigmoid;
    }
    return f3;
}

CharClipDriver *CharDriver::Last() {
    CharClipDriver *d = mFirst;
    while (d && d->Next())
        d = d->Next();
    return d;
}

CharClipDriver *CharDriver::Before(CharClipDriver *driver) {
    CharClipDriver *d = mFirst;
    while (d && d->Next() != driver)
        d = d->Next();
    return d;
}

CharClipDriver *CharDriver::FirstPlaying() {
    CharClipDriver *d;
    for (d = mFirst; d != nullptr && !d->mBlendFrac; d = d->Next())
        ;
    return d;
}

#pragma push
#pragma force_active on
inline CharClip *CharDriver::FirstClip() {
    if (mFirst)
        return mFirst->GetClip();
    else
        return nullptr;
}

inline CharClip *CharDriver::FirstPlayingClip() {
    CharClipDriver *d = FirstPlaying();
    if (d)
        return d->GetClip();
    else
        return nullptr;
}
#pragma pop

void CharDriver::Offset(float f1, float f2) {
    if (mFirst)
        mFirst->mBeat += RandomFloat(f1, f2);
}

float CharDriver::TopClipFrame() {
    CharClipDriver *it = mFirst;
    if (!it)
        return 0;
    else {
        while (it->Next())
            it = it->Next();
        if (!it->GetClip())
            return 0;
        else {
            float avg = it->GetClip()->AverageBeatsPerSecond();
            float frame = 0;
            if (avg < 0)
                return frame;
            else
                frame = (it->mBeat - it->GetClip()->StartBeat()) / avg;
            return frame;
        }
    }
}

void CharDriver::PollDeps(
    std::list<Hmx::Object *> &changedBy, std::list<Hmx::Object *> &change
) {
    change.push_back(mBones);
}

void CharDriver::Replace(Hmx::Object *from, Hmx::Object *to) {
    RndHighlightable::Replace(from, to);
    CharWeightable::Replace(from, to);
    CharPollable::Replace(from, to);
#ifdef HX_NATIVE
    // W25-CROWD: does a Replace() drop the live walk clip on a crowd driver
    // (async load-completion swapping the clip object out of the stack)? Probe.
    {
        const char *dp = getenv("CHARDRV_PROBE");
        if (dp && mFirst) {
            ObjectDir *owndir = Dir();
            const char *dn = (owndir && owndir->Name()) ? owndir->Name() : "";
            const char *ct = mClipType.Str();
            if (dp[0] == '*' || (dn[0] && strstr(dn, dp)) || (ct && strstr(ct, dp)))
                fprintf(stderr,
                    "[CHARDRV_REPLACE] dir='%s' clipType='%s' from='%s' to='%s' beat=%.3f\n",
                    dn[0] ? dn : "?", ct ? ct : "?",
                    from && from->Name() ? from->Name() : "?",
                    to && to->Name() ? to->Name() : "?", TheTaskMgr.Beat());
        }
    }
#endif
    if (mFirst)
        mFirst = mFirst->DeleteClip(from);
}

SAVE_OBJ(CharDriver, 0x2D6);

BEGIN_LOADS(CharDriver)
    LOAD_REVS(bs)
    ASSERT_REVS(0xE, 0)
    LOAD_SUPERCLASS(Hmx::Object)
    LOAD_SUPERCLASS(CharWeightable)
    if (gRev < 3) {
        int x;
        bs >> x;
    }
    bs >> mBones;
    if (gRev < 8) {
        FilePath fp;
        bs >> fp;
        if (gRev > 6 && fp.empty()) {
            bs >> mClips;
        }
    } else
        bs >> mClips;
    if (gRev > 8)
        bs >> mBlendWidth;
    if (gRev > 1)
        bs >> mRealign;
    else
        mRealign = false;
    if (gRev > 5)
        bs >> (int &)mApply;
    else if (gRev > 4) {
        bool b48;
        bs >> b48;
        mApply = (ApplyMode)(b48 != false);
    } else
        mApply = kApplyBlend;
    if (gRev > 9)
        bs >> mClipType;
    if (gRev > 0xC)
        bs >> mPlayMultipleClips;
    if (gRev <= 9 && mClips) {
        mClipType = mClips->Type();
        if (mClipType.Null()) {
            for (ObjDirItr<CharClip> it(mClips, true); it != nullptr; ++it) {
                mClipType = it->Type();
                break;
            }
        }
    }
    SyncInternalBones();
    if (gRev > 3) {
        mTestClip.Load(bs, false, mClips);
    }
    if (gRev > 0xB) {
        mDefaultClip.Load(bs, false, mClips);
    }
    if (gRev > 0xD)
        bs >> mDefaultPlayStarved;
END_LOADS

BEGIN_COPYS(CharDriver)
    COPY_SUPERCLASS(Hmx::Object)
    COPY_SUPERCLASS(CharWeightable)
    CREATE_COPY(CharDriver)
    BEGIN_COPYING_MEMBERS
        mBones = c->GetBones();
        COPY_MEMBER(mClips)
        COPY_MEMBER(mRealign)
        COPY_MEMBER(mBeatScale)
        COPY_MEMBER(mBlendWidth)
        COPY_MEMBER(mTestClip)
        COPY_MEMBER(mClipType)
        COPY_MEMBER(mApply)
        COPY_MEMBER(mDefaultClip)
        COPY_MEMBER(mDefaultPlayStarved)
        COPY_MEMBER(mPlayMultipleClips)
        SyncInternalBones();
    END_COPYING_MEMBERS
END_COPYS

BEGIN_HANDLERS(CharDriver)
    HANDLE(play, OnPlay)
    HANDLE(play_group, OnPlayGroup)
    HANDLE(play_group_flags, OnPlayGroupFlags)
    HANDLE_ACTION(offset, Offset(_msg->Float(2), _msg->Float(_msg->Size() - 1)))
    HANDLE(get_first_playing_flags, OnGetFirstPlayingFlags)
    HANDLE(get_first_flags, OnGetFirstFlags)
    HANDLE_EXPR(first_clip, FirstClip())
    HANDLE_ACTION(set_starved, SetStarved(_msg->Sym(2)))
    HANDLE_ACTION(set_beat_scale, SetBeatScale(_msg->Float(2), true))
    HANDLE_ACTION(transfer, Transfer(*_msg->Obj<CharDriver>(2)))
    HANDLE(print, OnPrint)
    HANDLE(set_default_clip, OnSetDefaultClip)
    HANDLE(set_first_beat_offset, OnSetFirstBeatOffset)
    HANDLE_ACTION(clear, Clear())
    HANDLE(get_clip_or_group_list, OnGetClipOrGroupList)
    HANDLE_SUPERCLASS(RndPollable)
    HANDLE_SUPERCLASS(Hmx::Object)
    HANDLE_CHECK(0x378)
END_HANDLERS

DataNode CharDriver::OnPlay(const DataArray *msg) {
    int i2 = msg->Size() > 3 ? msg->Int(3) : 4;
    MILO_ASSERT(msg->Size()<=4, 0x381);
    return Play(msg->Node(2), i2, -1, kHugeFloat, 0) != nullptr;
}

DataNode CharDriver::OnPlayGroup(const DataArray *msg) {
    MILO_ASSERT(msg->Size() <= 4, 0x387);
    int i2 = msg->Size() > 3 ? msg->Int(3) : 4;
    return PlayGroup(msg->Str(2), i2, -1, kHugeFloat, 0) != nullptr;
}

DataNode CharDriver::OnPlayGroupFlags(const DataArray *msg) {
    MILO_ASSERT(msg->Size() <= 5, 0x392);
    CharClipGroup *group = mClips->Find<CharClipGroup>(msg->Str(2), false);
    if (!group) {
        MILO_WARN("%s could not find group %s", PathName(this), msg->Str(2));
        return 0;
    } else {
        int clipIdx = msg->Int(3);
        int i2 = msg->Size() > 4 ? msg->Int(4) : 4;
        return Play(group->GetClip(clipIdx), i2, -1, kHugeFloat, 0) != nullptr;
    }
}

DataNode CharDriver::OnSetFirstBeatOffset(DataArray *msg) {
    if (mFirst) {
        mFirst->SetBeatOffset(msg->Float(2), (TaskUnits)msg->Int(3), msg->Sym(4));
    }
    return 0;
}

DataNode CharDriver::OnGetFirstPlayingFlags(const DataArray *) {
    CharClip *clip = FirstPlayingClip();
    return clip ? clip->Flags() : 0;
}

DataNode CharDriver::OnGetFirstFlags(const DataArray *) {
    CharClip *clip = FirstPlayingClip();
    return clip ? clip->Flags() : 0;
}

DataNode CharDriver::OnPrint(const DataArray *) {
    MILO_LOG("%s\n", PathName(this));
    for (CharClipDriver *it = mFirst; it != nullptr; it = it->Next()) {
        MILO_LOG("   clip %s blend %.3f\n", it->GetClip()->Name(), it->mBlendFrac);
    }
    return 0;
}

DataNode CharDriver::OnSetDefaultClip(DataArray *arr) {
    if (mClips) {
        mDefaultClip = FindClip(arr->Str(2), true);
    }
    return mDefaultClip.Ptr();
}

DataNode CharDriver::OnGetClipOrGroupList(DataArray *) {
    Symbol clipName = "CharClip";
    Symbol clipGrpName = "CharClipGroup";
    std::list<Hmx::Object *> objects;
    if (mClips) {
        for (ObjDirItr<Hmx::Object> it(mClips, true); it != nullptr; ++it) {
            if (IsASubclass(it->ClassName(), clipName)
                || IsASubclass(it->ClassName(), clipGrpName)) {
                objects.push_back(it);
            }
        }
    }
    DataArrayPtr ptr;
    ptr->Resize(objects.size() + 1);
    int idx = 0;
    ptr->Node(idx++) = NULL_OBJ;
    for (std::list<Hmx::Object *>::iterator it = objects.begin(); it != objects.end();
         ++it) {
        ptr->Node(idx++) = *it;
    }
    ptr->SortNodes();
    return ptr;
}

BEGIN_PROPSYNCS(CharDriver)
    SYNC_PROP(bones, mBones)
    SYNC_PROP_SET(clips, mClips, SetClips(_val.Obj<ObjectDir>()))
    SYNC_PROP_SET(clip_type, mClipType, SetClipType(_val.Sym()))
    SYNC_PROP(realign, mRealign)
    SYNC_PROP_SET(apply, mApply, SetApply((ApplyMode)_val.Int()))
    SYNC_PROP_SET(first_playing_clip, FirstPlayingClip(), )
    SYNC_PROP(beat_scale, mBeatScale)
    SYNC_PROP(blend_width, mBlendWidth)
    SYNC_PROP(default_clip_or_group, mDefaultClip)
    SYNC_PROP(default_play_starved, mDefaultPlayStarved)
    SYNC_PROP(test_clip, mTestClip)
    SYNC_PROP(play_multiple_clips, mPlayMultipleClips)
    SYNC_PROP(display_zoom, CharClipDisplay::sZoom)
    SYNC_SUPERCLASS(CharWeightable)
END_PROPSYNCS