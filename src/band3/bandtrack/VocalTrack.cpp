#include "bandtrack/VocalTrack.h"
#include "GraphicsUtl.h"
#include "VocalStyle.h"
#include "bandobj/NoteTube.h"
#include "bandobj/PitchArrow.h"
#include "bandobj/StreakMeter.h"
#include "bandobj/VocalTrackDir.h"
#include "bandtrack/Lyric.h"
#include "bandtrack/TrackPanel.h"
#include "bandtrack/VocalStyle.h"
#include "beatmatch/VocalNote.h"
#include "decomp.h"
#include "game/BandUser.h"
#include "game/Game.h"
#include "game/GameConfig.h"
#include "game/Player.h"
#include "game/SongDB.h"
#include "math/Mtx.h"
#include "math/Utl.h"
#include "meta_band/BandSongMetadata.h"
#include "meta_band/BandSongMgr.h"
#include "meta_band/GameplayOptions.h"
#include "meta_band/MetaPerformer.h"
#include "obj/Data.h"
#include "obj/DataFile.h"
#include "obj/DataFunc.h"
#include "obj/ObjMacros.h"
#include "obj/Object.h"
#include "os/Debug.h"
#include "os/System.h"
#include "rndobj/Anim.h"
#include "rndobj/Group.h"
#include "rndobj/Mesh.h"
#include "synth/MicManagerInterface.h"
#include "utl/Std.h"
#include "utl/Symbols.h"
#include "utl/Symbols4.h"
#include "utl/TimeConversion.h"
#include <utility>

int maxPlatesQueued;
int maxVertsInPlate;
int maxFacesInPlate;
int maxNumLyricPlates;
bool dumpLyricShifts;
bool sDumpLyricPlates;
bool sDumpPlateStates;
bool gDebugSpew;

MicClientID sNullMicClientID(-1, -1);

inline TambourineGemPool::TambourineGemPool() {
    for (int i = 0; i < 25; i++) {
        mFreeGems.push_back(new TambourineGem());
    }
    mTambourineManager = 0;
}

inline TambourineGemPool::~TambourineGemPool() {
    FreeUsedGems();
    MILO_ASSERT(mUsedGems.empty(), 0x1B6);
    for (int i = 0; i < mFreeGems.size(); i++) {
        RELEASE(mFreeGems[i]);
    }
}

void VocalTrack::UpdateMarkerVisibility(float f1, float f2) {
    for (int i = 0; i < unk1a0.size(); i++) {
        std::pair<RndMesh *, float> &curMarker = unk1a0[i];
        curMarker.first->SetShowing(curMarker.second >= f1 && curMarker.second <= f2);
    }
}

void VocalTrack::InvalidateMarkers(float f1) {
    while (!unk1a0.empty()) {
        if (f1 < unk1a0.front().second)
            break;
        ReturnFirstMarker();
    }
}

void VocalTrack::ClearMarkers() {
    while (!unk1a0.empty()) {
        ReturnFirstMarker();
    }
}

void VocalTrack::UpdateTubePlates(
    std::deque<TubePlate *> &deque, float f2, float f3, bool b4
) {
    if (mIntroPlaying || deque.empty())
        return;
    while (!deque.empty() && !deque.front()->NoVerts()
           && (deque.front()->CurrentEndX(f3) < mDir->mTrackLeftX - unk78
               || deque.front()->InvalidateMs() < f2)) {
        if (!deque.front()->Baked()) {
            MILO_WARN("popping unbaked plate");
        }
        TubePlate *cur = deque.front();
        if (sDumpPlateStates) {
            MILO_LOG(
                "%s recycling plate at %.2f sec\n",
                cur->GetMatName().c_str(),
                f2 / 1000.0f
            );
            DumpPlates(deque, cur->GetMatName().c_str());
        }
        deque.pop_front();
        cur->Reset();
        deque.push_back(cur);
    }
    float fvar1 = TheGame->InTrainer() ? unk2a4 : f2; // fix game bool being checked
    FOREACH (it, deque) {
        TubePlate *cur = *it;
        if (cur->CurrentEndX(f3) < mDir->mTrackLeftX) {
            cur->SetShowing(false);
        } else {
            if (cur->CurrentStartX(f3) >= mDir->mTrackRightX) {
                cur->SetShowing(false);
                break;
            } else
                cur->SetShowing(true);
        }

        if (sDumpPlateStates && !cur->Baked()) {
            MILO_LOG(
                "%s baking plate at %.2f sec\n", cur->GetMatName().c_str(), f2 / 1000.0f
            );
            DumpPlates(deque, cur->GetMatName().c_str());
        }
        cur->Bake();
        if (mVocalStyleOverride == kVocalStyleScrolling && cur->Deploy()) {
            cur->PollDeploy(fvar1);
        }
    }
#ifdef MILO_DEBUG
    if (deque.size() != 0) {
        if (deque.size() > maxPlatesQueued) {
            maxPlatesQueued = std::max<int>(maxPlatesQueued, deque.size());
            if (maxPlatesQueued >= 24) {
                MILO_WARN(
                    "Too many tube plates - please file a bug to Josh Stoddard and include the Watson output."
                );
                DumpPlates(deque, deque.front()->GetMatName().c_str());
            }
            if (sDumpPlateStates) {
                MILO_LOG("max plates queued -> %d\n", maxPlatesQueued);
            }
        }
        int numverts = deque.front()->mMesh->mGeomOwner->mVerts.size();
        if (numverts > maxVertsInPlate) {
            maxVertsInPlate = std::max<int>(maxVertsInPlate, numverts);
            if (sDumpPlateStates) {
                MILO_LOG("max verts in a plate -> %d\n", maxVertsInPlate);
            }
        }
        int numfaces = deque.front()->mMesh->mGeomOwner->mFaces.size();
        if (numfaces > maxFacesInPlate) {
            maxFacesInPlate = std::max<int>(maxFacesInPlate, numfaces);
            if (sDumpPlateStates) {
                MILO_LOG("max faces in a plate -> %d\n", maxFacesInPlate);
            }
        }
    }
#endif
}

void VocalTrack::UpdateAllTubePlates(float f1) {
    if (!mPlayer->IsNet()) {
        for (int i = 0; i < 3; i++) {
            UpdateTubePlates(mFrontTubePlates[i], f1, unk2a8, false);
            UpdateTubePlates(mBackTubePlates[i], f1, unk2a8, false);
            UpdateTubePlates(mPhonemeTubePlates[i], f1, unk2a8, false);
        }
    }
    bool staticVox = !IsScrolling();
    UpdateTubePlates(mLeadDeployPlates, f1, staticVox ? unk2ac : unk2a8, staticVox);
    UpdateTubePlates(mHarmonyDeployPlates, f1, staticVox ? unk2b0 : unk2a8, staticVox);
}

void VocalTrack::ClearTubePlates(std::deque<TubePlate *> &plates) {
    while (!plates.empty()) {
        delete plates.front();
        plates.pop_front();
    }
}

void VocalTrack::ClearAllTubePlates() {
    for (int i = 0; i < 3; i++) {
        ClearTubePlates(mFrontTubePlates[i]);
        ClearTubePlates(mBackTubePlates[i]);
        ClearTubePlates(mPhonemeTubePlates[i]);
    }
    ClearTubePlates(mLeadDeployPlates);
    ClearTubePlates(mHarmonyDeployPlates);
}

void VocalTrack::ResetTubePlates(std::deque<TubePlate *> &plates) {
    std::deque<TubePlate *>::iterator it = plates.begin();
    std::deque<TubePlate *>::iterator end = plates.end();
    for (; it != end; ++it) {
        (*it)->Reset();
    }
}

void VocalTrack::ResetAllTubePlates() {
    for (int i = 0; i < 3; i++) {
        ResetTubePlates(mFrontTubePlates[i]);
        ResetTubePlates(mBackTubePlates[i]);
        ResetTubePlates(mPhonemeTubePlates[i]);
    }
    ResetTubePlates(mLeadDeployPlates);
    ResetTubePlates(mHarmonyDeployPlates);
    if (sDumpPlateStates) {
        MILO_LOG("resetting all plates\n");
    }
}

void VocalTrack::DumpPlates(std::deque<TubePlate *> &plates, const char *str) {
    MILO_LOG("dumping plates in %s\n", str);
    int idx = 0;
    std::deque<TubePlate *>::iterator it = plates.begin();
    std::deque<TubePlate *>::iterator end = plates.end();
    for (; it != end; ++it) {
        TubePlate *cur = *it;
        if (!cur->NoVerts()) {
            Transform &xfm = cur->mMesh->TransParent()->WorldXfm();
            MILO_LOG(
                "\t[%d] @ %x, xPos: %.2f, xStart: %.2f, XEnd: %.2f, verts: %d, faces: %d, baked: %d\n",
                idx++,
                cur,
                -xfm.v.x,
                cur->GetBeginX(),
                cur->GetBeginX() + cur->GetWidthX(),
                cur->mMesh->Verts().size(),
                cur->mMesh->Faces().size(),
                cur->Baked()
            );
        } else {
            MILO_LOG(
                "\t[%d] @ %x, <empty>, verts: %d, faces: %d, baked: %d\n",
                idx++,
                cur,
                cur->mMesh->Verts().size(),
                cur->mMesh->Faces().size(),
                cur->Baked()
            );
        }
    }
}

void VocalTrack::DumpAllPlates() {
    for (int i = 0; i < 3; i++) {
        DumpPlates(mFrontTubePlates[i], MakeString("part %d front", i));
        DumpPlates(mBackTubePlates[i], MakeString("part %d back", i));
        DumpPlates(mPhonemeTubePlates[i], MakeString("part %d phoneme", i));
    }
    DumpPlates(mLeadDeployPlates, "lead deploy");
    DumpPlates(mHarmonyDeployPlates, "harmony deploy");
}

TubePlate *VocalTrack::GetCurrentPlate(std::deque<TubePlate *> &plates, int i2) {
    FOREACH (it, plates) {
        if (!(*it)->Baked())
            return *it;
    }
    plates.push_back(new TubePlate(i2));
#ifdef MILO_DEBUG
    static Symbol leadDeployMat = "deploy_mask_lead.mat";
    static Symbol harmDeployMat = "deploy_mask_harmony.mat";

    String matName = plates.front()->GetMatName();
    if (!mIntroPlaying && matName != leadDeployMat && matName != harmDeployMat) {
        MILO_WARN(
            "%s new plate added.  Please alert HUD/Track owner and include the Watson output.",
            matName.c_str()
        );
        DumpPlates(plates, plates.front()->GetMatName().c_str());
    }
#endif
    return plates.back();
}

void VocalTrack::HookupTubePlates(NoteTube *tube) {
    if (tube->Pitched()) {
        tube->SetFrontPlate(GetCurrentPlate(mFrontTubePlates[tube->Part()], 0x80));
        tube->SetBackPlate(GetCurrentPlate(mBackTubePlates[tube->Part()], 0x80));
    } else if (tube->unk_0x24) {
        bool islead = tube->Part() == 0;
        tube->SetFrontPlate(nullptr);
        tube->SetBackPlate(
            GetCurrentPlate(islead ? mLeadDeployPlates : mHarmonyDeployPlates, 0x20)
        );
    } else {
        tube->SetFrontPlate(nullptr);
        tube->SetBackPlate(GetCurrentPlate(mPhonemeTubePlates[tube->Part()], 0x40));
    }
}

DataNode ToggleDebugSpew(DataArray *) {
    gDebugSpew = !gDebugSpew;
    return gDebugSpew;
}

VocalTrack::VocalTrack(BandUser *u)
    : Track(u), unk68(0), mVocalStyleOverride(kVocalStyleScrolling), unk70(2),
      unk78(24.0f), unk7c(0), mDir(this), mPlayer(this), mPhraseStartMs(0),
      mPhraseEndMs(0), mNextPhraseEndMs(0), unkf4(0), unkf8(0), unkfc(0), unk100(0),
      unk104(1), unk108(0), unk128(0), unk19c(0), unk1c8(this), mTambourineGemPool(0),
      mCharOptMicID(-1), unk208(60), unk20c(0), unk210(0), unk23c(0.1f), unk240(0.1f),
      unk294(0), unk298(0), unk2a4(-1.0f), unk2a8(0), unk2ac(0), unk2b0(0),
      mStaticDeployZoneXSize(2.0f), mStaticDeployBufferX(0.5f),
      mStaticDeployMarginX(0.1f), mLyricShiftMs(100.0f), mLyricShiftQuickMs(20.0f),
      mLyricShiftAnticipationMs(250.0f), mMinLyricHighlightMs(100.0f),
      mMinPhraseHighlightMs(500.0f), mLyricOverlapWindowMs(100.0f), unk2e4(0),
      mNoteTube(new NoteTube()), unk2ec(1) {
    DataRegisterFunc("vocal_jitter_debug", ToggleDebugSpew);
    for (int i = 0; i < 3; i++) {
        mFrontTubePlates.push_back(std::deque<TubePlate *>());
        mBackTubePlates.push_back(std::deque<TubePlate *>());
        mPhonemeTubePlates.push_back(std::deque<TubePlate *>());
        mAlternateNoteList[i] = 0;
    }
    InitPlatePool();
}

VocalTrack::~VocalTrack() {
    RELEASE(mTambourineGemPool);
    ClearLyrics();
    ClearMarkers();
    ClearAllTubePlates();
    DeleteAll(mMeshPool);
    RELEASE(mNoteTube);
}

void VocalTrack::InitPlateList(std::deque<TubePlate *> &list, int i2, int i3) {
    MILO_ASSERT(list.empty(), 0x25C);
    for (int i = 0; i < i2; i++) {
        list.push_back(new TubePlate(i3));
    }
}

void VocalTrack::InitPlatePool() {
    for (int i = 0; i < 3; i++) {
        InitPlateList(mFrontTubePlates[i], 4, 0x80);
        InitPlateList(mBackTubePlates[i], 4, 0x80);
        InitPlateList(mPhonemeTubePlates[i], 4, 0x40);
    }
    InitPlateList(mLeadDeployPlates, 4, 0x20);
    InitPlateList(mHarmonyDeployPlates, 4, 0x20);
}

void VocalTrack::Init() {
    const BandUser *pUser = mTrackConfig.GetBandUser();
    MILO_ASSERT(pUser, 0x275);
    mTrackConfig.SetTrackNum(TheGameConfig->GetTrackNum(pUser->mUserGuid));
    unk74 = 3000.0f;
    RELEASE(mTambourineGemPool);
    mTambourineGemPool = new TambourineGemPool();
    if (mPlayer)
        mTambourineGemPool->SetTambourineManager(&mPlayer->mTambourineManager);
    BandUser *user = (BandUser *)mTrackConfig.GetBandUser();
    GameplayOptions *options = user->GetGameplayOptions();
    if (options) {
        DataArray *staticArr = SystemConfig()->FindArray("force_static_vocals", false);
        if (staticArr) {
            if (SystemConfig()->FindInt("force_static_vocals")) {
                SetVocalStyle((VocalStyle)0);
            }
            goto next;
        }
        SetVocalStyle(options->GetVocalStyle());
    }
next:
    ReadTimingData(SystemConfig()->FindArray("track_graphics"));
    unk1c8 = mDir->Find<RndGroup>("markers.grp", true);
    unk19c = 0;
    for (int i = 0; i < 0x20; i++) {
        CreateMarker("beat_marker.mesh", 0, false);
    }
    ClearMarkers();
}

void VocalTrack::ResetTimingData() {
    ReadTimingData(DataReadFile("config/track_graphics.dta", true));
    RebuildHUD();
}

void VocalTrack::ReadTimingData(const DataArray *a) {
    mLyricOverlapWindowMs = a->FindFloat("lyric_overlap_ms");
    DataArray *staticCfg = a->FindArray("static_vocal_parameters");
    mStaticDeployZoneXSize = staticCfg->FindFloat("static_deploy_x_size");
    mStaticDeployBufferX = staticCfg->FindFloat("static_deploy_buffer_x");
    mStaticDeployMarginX = staticCfg->FindFloat("static_phrase_margin_x");
    mLyricShiftMs = staticCfg->FindArray("lyric_shift_ms")->Float(1);
    mLyricShiftQuickMs = staticCfg->FindArray("lyric_shift_ms")->Float(2);
    mLyricShiftAnticipationMs = staticCfg->FindFloat("lyric_shift_anticipation_ms");
    mMinLyricHighlightMs = staticCfg->FindFloat("min_lyric_highlight_ms");
    mMinPhraseHighlightMs = staticCfg->FindFloat("phrase_highlight_ms");
    static bool sDump;
    if (sDump) {
        MILO_LOG("lyric timing data:\n");
        MILO_LOG("\t overlap window ms %.0f\n", mLyricOverlapWindowMs);
        MILO_LOG("\t static deploy size %.2f\n", mStaticDeployZoneXSize);
        MILO_LOG("\t static deploy gap size %.2f\n", mStaticDeployBufferX);
        MILO_LOG("\t now bar offset %.2f\n", mStaticDeployMarginX);
        MILO_LOG("\t standard lyric shift ms %.0f\n", mLyricShiftMs);
        MILO_LOG("\t fast lyric shift ms %.0f\n", mLyricShiftQuickMs);
        MILO_LOG("\t lyric shift anticipation ms %.0f\n", mLyricShiftAnticipationMs);
        MILO_LOG("\t min lyric highlight ms %.0f\n", mMinLyricHighlightMs);
        MILO_LOG("\t phrase highlight anticipation ms %.0f\n", mMinPhraseHighlightMs);
    }
}

bool VocalTrack::ShowPitchCorrectionNotice() const {
    if (mPlayer)
        return mPlayer->ShowPitchCorrectionNotice();
    else
        return false;
}

void VocalTrack::ConfigNoteTube(bool pitched, int pts, int part, bool b4, float alpha) {
    mNoteTube->SetPitched(pitched);
    mNoteTube->SetNumPoints(pts);
    mNoteTube->SetPart(part);
    mNoteTube->unk_0x24 = b4;
    mNoteTube->SetAlpha(alpha);
    if (pitched) {
        switch (part) {
        case 1:
            mNoteTube->SetBackMat(mDir->mHarm1BackMat);
            mNoteTube->SetBackParent(mDir->mTubeBack1Grp);
            mNoteTube->SetFrontMat(mDir->mHarm1FrontMat);
            mNoteTube->SetFrontParent(mDir->mTubeFront1Grp);
            break;
        case 2:
            mNoteTube->SetBackMat(mDir->mHarm2BackMat);
            mNoteTube->SetBackParent(mDir->mTubeBack2Grp);
            mNoteTube->SetFrontMat(mDir->mHarm2FrontMat);
            mNoteTube->SetFrontParent(mDir->mTubeFront2Grp);
            break;
        default:
            mNoteTube->SetBackMat(mDir->mLeadBackMat);
            mNoteTube->SetBackParent(mDir->mTubeBack0Grp);
            mNoteTube->SetFrontMat(mDir->mLeadFrontMat);
            mNoteTube->SetFrontParent(mDir->mTubeFront0Grp);
            break;
        }
    } else if (!b4) {
        mNoteTube->SetFrontMat(nullptr);
        mNoteTube->SetFrontParent(nullptr);
        switch (part) {
        case 1:
            mNoteTube->SetBackMat(mDir->mHarm1PhonemeMat);
            mNoteTube->SetBackParent(mDir->mTubePhoneme1Grp);
            break;
        case 2:
            mNoteTube->SetBackMat(mDir->mHarm2PhonemeMat);
            mNoteTube->SetBackParent(mDir->mTubePhoneme2Grp);
            break;
        default:
            mNoteTube->SetBackMat(mDir->mLeadPhonemeMat);
            mNoteTube->SetBackParent(mDir->mTubePhoneme0Grp);
            break;
        }
    } else {
        MILO_ASSERT(part < 3, 0x30D);
        mNoteTube->SetFrontMat(nullptr);
        mNoteTube->SetFrontParent(nullptr);
        mNoteTube->SetBackParent(nullptr);
        if (part != 0)
            mNoteTube->SetBackMat(mDir->mHarmDeployMat);
        else
            mNoteTube->SetBackMat(mDir->mLeadDeployMat);
    }
}

LyricPlate *VocalTrack::GetNextLyricPlate(std::deque<LyricPlate *> &plates, bool b2) {
    std::deque<LyricPlate *>::iterator it = plates.begin();
    std::deque<LyricPlate *>::iterator end = plates.end();
    for (; it != end; ++it) {
        if ((*it)->Empty())
            return *it;
    }
    RndText *text = b2 ? mDir->mLeadText : mDir->mHarmText;
    RndText *phonemeText = b2 ? mDir->mLeadPhonemeText : mDir->mHarmPhonemeText;
    RndText *newText = NewRndCopy(text);
    plates.push_back(new LyricPlate(newText, text, phonemeText));
    if (sDumpLyricPlates) {
        MILO_LOG("creating new %s lyric plate\n", b2 ? "lead" : "harmony");
        DumpLyricPlates(plates, b2);
    }
    int numplates = plates.size();
    bool grew;
    if (maxNumLyricPlates < numplates) {
        maxNumLyricPlates = numplates;
        grew = true;
    } else {
        grew = false;
    }
    bool doDump = grew && sDumpLyricPlates;
    if (doDump) {
        MILO_LOG("Max Lyric Plates: %d\n", maxNumLyricPlates);
    }
    return plates.back();
}

Lyric *VocalTrack::GetLastLyric(std::deque<LyricPlate *> &plates) {
    Lyric *last = nullptr;
    std::deque<LyricPlate *>::iterator it = plates.begin();
    std::deque<LyricPlate *>::iterator end = plates.end();
    for (; it != end; ++it) {
        if ((*it)->Empty())
            break;
        last = (*it)->LatestLyric();
    }
    return last;
}

Lyric *VocalTrack::GetLastBakedLyric(std::deque<LyricPlate *> &plates) {
    Lyric *last = nullptr;
    std::deque<LyricPlate *>::iterator it = plates.begin();
    std::deque<LyricPlate *>::iterator end = plates.end();
    for (; it != end; ++it) {
        if (!(*it)->Baked())
            break;
        last = (*it)->LatestLyric();
    }
    return last;
}

RndMesh *VocalTrack::CreateMarker(Symbol s1, float f2, bool warn) {
    RndMesh *mesh = nullptr;
    if (mMeshPool.empty()) {
        mesh = Hmx::Object::New<RndMesh>();
        unk19c++;
        if (warn) {
            MILO_WARN(
                "VocalTrack::CreateMarker() added new %s mesh at run-time (total %d); please alert HUD/Track owner",
                s1.mStr,
                unk19c
            );
        }
    } else {
        mesh = mMeshPool.back();
        mMeshPool.pop_back();
    }
    RndMesh *found = mDir->Find<RndMesh>(s1.mStr, true);
    mesh->SetGeomOwner(found->GeomOwner());
    mesh->SetMat(found->Mat());
    mesh->SetShowing(true);
    mesh->SetTransParent(found->TransParent(), false);
    mesh->SetLocalXfm(found->mLocalXfm);
    mesh->SetTransParent(mDir->mScroller, true);
    Vector3 v18 = mesh->mLocalXfm.v;
    v18.x = unk78 * (f2 / unk74);
    mesh->SetLocalPos(v18);
    unk1c8->AddObject(mesh);
    unk1a0.push_back(std::make_pair(mesh, f2));
    return mesh;
}

void VocalTrack::ReturnFirstMarker() {
    RndMesh *mesh = unk1a0.front().first;
    MILO_ASSERT(mesh, 0x393);
    MILO_ASSERT(mesh->GeomOwner() != mesh, 0x394);
    mMeshPool.push_back(mesh);
    unk1c8->RemoveObject(mesh);
    unk1a0.pop_front();
}

void VocalTrack::SetDir(RndDir *dir) {
    mDir = dynamic_cast<VocalTrackDir *>(dir);
    Init();
}

bool VocalTrack::WantBeatLines(int i1) {
    if (mPlayer->IsNet())
        return false;
    else {
        VocalNoteList *notes = GetVocalNoteList(0);
        std::vector<VocalPhrase> &phrases = notes->mPhrases;
        FOREACH (it, phrases) {
            if (i1 >= it->unk8 && (i1 <= it->unk8 + it->unkc)) {
                return it->mTambourinePhrase;
            }
        }
        return false;
    }
}

int VocalTrack::NumSingers() const {
    if (mPlayer)
        return mPlayer->NumSingers();
    else
        return 0;
}

bool VocalTrack::UseVocalHarmony() {
    if (mPlayer)
        return mPlayer->NumVocalParts() > 1;
    else
        return 0;
}

void VocalTrack::SetVocalStyle(VocalStyle style) {
    if (HasNetPlayer())
        unk2e5 = true;
    else
        unk2e5 = false;
    if (mVocalStyleOverride != style) {
        mVocalStyleOverride = style;
        UpdateVocalStyle();
        TrackPanel *panel = GetTrackPanel();
        panel->unk5f = false;
    }
}

bool VocalTrack::IsScrolling() const {
    if (unk70 == 2)
        return mVocalStyleOverride == kVocalStyleScrolling;
    else
        return unk70 == 1;
}

void VocalTrack::UpdateVocalStyle() {
    std::vector<Player *> &players = TheGame->GetActivePlayers();
    if (mPlayer && mPlayer->IsLocal()) {
        for (int i = 0; i < players.size(); i++) {
            Player *cur = players[i];
            if (cur && cur->GetTrackType() == kTrackVocals) {
                if (cur->GetTrackNum() != mTrackConfig.TrackNum() && cur->IsNet()) {
                    VocalTrack *track =
                        dynamic_cast<VocalTrack *>(cur->GetUser()->GetTrack());
                    if (track)
                        track->SetVocalStyle(mVocalStyleOverride);
                }
            }
        }
    }
    if (mDir) {
        if (mPlayer) {
            EnabledState estate = mPlayer->GetEnabledState();
            if (estate == kPlayerDisabled || estate == kPlayerDisconnected)
                return;
        }
        mDir->UpdateConfiguration();
        unk78 = mDir->mTrackRightX - mDir->mTrackLeftX;
        BandSongMetadata *data = (BandSongMetadata *)TheSongMgr.Data(
            TheSongMgr.GetSongIDFromShortName(MetaPerformer::Current()->Song(), true)
        );
        unk74 = data->ScrollSpeed() * (unk78 / 16.8f);
        mDir->Find<RndAnimatable>("tambourine_preview.anim", true)->SetFrame(0, 1);
        RebuildHUD();
    }
}

void VocalTrack::RebuildHUD() {
    static bool sDump;
    for (int i = 0; i < 3; i++) {
        mNextScrollNote[i] = 0;
    }
    for (int i = 0; i < 2; i++) {
        mNextDeployZone[i] = 0;
    }
    for (int i = 0; i < 2; i++) {
        mCurLyricPhrase[i] = 0;
    }

    unk108 = 0;
    unk104 = 1;
    unk100 = 0;
    unkf4 = 0;
    unkf8 = 0;
    unkfc = 0;
    unk23c = mStaticDeployMarginX;
    unk240 = mStaticDeployMarginX;
    mLeadLyricShifts.clear();
    mHarmonyLyricShifts.clear();
    mDir->mLeadLyricScroller->DirtyLocalXfm().v.x = unk23c;
    mDir->mHarmonyLyricScroller->DirtyLocalXfm().v.x = unk240;
    unk2ac = unk23c;
    unk2b0 = unk240;
    unk294 = 0;
    unk298 = 0;
    ClearLyrics();
    ClearMarkers();
    ResetAllTubePlates();
    mTambourineGemPool->FreeUsedGems();
    VocalNoteList *notes = GetVocalNoteList(0);
    if (mPlayer) {
        const VocalPhrase *const &cur = mPlayer->CurrentPhrase();
        const VocalPhrase *next = mPlayer->GetNextPhraseMarker(cur);
        if (HasNetPlayer()) {
            unk70 = 0;
        } else {
            unk70 = 2;
        }
        if (mPlayer->AtFirstPhrase()) {
            mPhraseEndMs = 0;
            BuildPhrase(cur->unk0 + cur->unk4, next->unk0 + next->unk4);
        } else {
            std::vector<VocalPhrase> &phrases = notes->mPhrases;
            if (cur != &*phrases.end()) {
                const VocalPhrase *prev = &*phrases.begin();
                while (prev != &*phrases.end()) {
                    if (mPlayer->GetNextPhraseMarker(prev) == cur)
                        break;
                    prev++;
                }
                if (prev != &*phrases.end()) {
                    if (!IsScrolling()) {
                        mPhraseEndMs = prev->unk0;
                        BuildPhrase(
                            prev->unk0 + prev->unk4, cur->unk0 + cur->unk4
                        );
                    }
                    mPhraseEndMs = prev->unk0 + prev->unk4;
                    float curEnd = cur->unk0 + cur->unk4;
                    float endMs;
                    if (next == &*phrases.end()) {
                        endMs = TheSongDB->GetSongDurationMs();
                    } else {
                        endMs = next->unk0 + next->unk4;
                    }
                    BuildPhrase(curEnd, endMs);
                }
            }
        }
        if (mPlayer->InTambourinePhrase()) {
            mDir->SetTambourine(true);
        }
        unk208 = -1;
        if (mDir->Property(pitch_guides, true)->Sym() == harmonic) {
            int tonic =
                ((BandSongMetadata *)TheSongMgr.Data(TheSongMgr.GetSongIDFromShortName(
                     MetaPerformer::Current()->Song(), true
                 )))
                    ->VocalTonicNote();
            if (tonic != -1)
                unk208 = tonic + 60;
        }
        VocalHUDColor colors[3] = { kVocalColorInvalid,
                                    kVocalColorInvalid,
                                    kVocalColorInvalid };
        Hmx::Object *tubestyle = mDir->mTubeStyle;
        colors[0] = GetVocalHUDColor(tubestyle->Property("lead_color", true)->Sym());
        colors[1] = GetVocalHUDColor(tubestyle->Property("harmony_1_color", true)->Sym());
        colors[2] = GetVocalHUDColor(tubestyle->Property("harmony_2_color", true)->Sym());
        for (int i = 0; i < mPlayer->NumVocalParts(); i++) {
            mPlayer->mVocalParts[i]->unkc8 = colors[i];
        }
        mDir->SetVocalLineColors(colors);
        mDir->mStreakMeter->SetNumParts(mPlayer->NumVocalParts());
        float margin = mDir->mPitchDisplayMargin;
        mRangeShifts.clear();
        std::vector<RangeSection> &sections = TheSongDB->GetRangeSections();
        float prevMin = sections[0].unk8 - margin;
        float prevMax = margin + sections[0].unkc;
        float maxRange = mDir->mMinPitchRange;
        if (sDump) {
            MILO_LOG("Range Shift Data\n");
        }
        for (int i = 0; i < sections.size(); i++) {
            RangeSection &section = sections[i];
            float secMin = section.unk8;
            float secMax = section.unkc;
            if (!(secMax < secMin)) {
                float secIntro = section.unk4;
                RangeShift rs;
                rs.unk0 = TickToMs((float)section.unk0);
                rs.unk4 = prevMin;
                rs.unk8 = prevMax;
                rs.unkc = secMin - margin;
                rs.unk10 = secMax + margin;
                rs.unk14 = secIntro;
                mRangeShifts.push_back(rs);
                prevMin = section.unk8 - margin;
                prevMax = section.unkc + margin;
                float range = prevMax - prevMin;
                float *bigger = (maxRange < range) ? &range : &maxRange;
                maxRange = *bigger;
                if (sDump) {
                    MILO_LOG(
                        "[%d]\tstart ms: %.2f, intro ms: %.2f, min: %.1f -> %.1f, "
                        "max: %.1f -> %.1f\n",
                        i,
                        mRangeShifts.back().unk0,
                        mRangeShifts.back().unk14,
                        mRangeShifts.back().unk4,
                        mRangeShifts.back().unkc,
                        mRangeShifts.back().unk8,
                        mRangeShifts.back().unk10
                    );
                }
            }
        }
        if (maxRange > 0) {
            int idx = 0;
            std::deque<RangeShift>::iterator it = mRangeShifts.begin();
            std::deque<RangeShift>::iterator end = mRangeShifts.end();
            for (; it != end; ++it) {
                float diffFrom = it->unk4 + (maxRange - it->unk8);
                if (diffFrom > 0) {
                    diffFrom *= 0.5f;
                    it->unk4 -= diffFrom;
                    it->unk8 += diffFrom;
                }
                float diffTo = it->unkc + (maxRange - it->unk10);
                if (diffTo > 0) {
                    diffTo *= 0.5f;
                    it->unkc -= diffTo;
                    it->unk10 += diffTo;
                }
                if (sDump) {
                    MILO_LOG(
                        "[%d]\tstart ms: %.2f, intro ms: %.2f, min: %.1f -> %.1f, "
                        "max: %.1f -> %.1f\n",
                        idx++,
                        it->unk0,
                        it->unk14,
                        it->unk4,
                        it->unkc,
                        it->unk8,
                        it->unk10
                    );
                }
            }
        }
        if (mDir->mStreakMeter) {
            int parts = GetNumVocalParts();
            for (int i = 0; i < parts; i++) {
                bool active = false;
                VocalPart *part = mPlayer->mVocalParts[i];
                if (part && !part->InEmptyPhrase()) {
                    active = true;
                }
                mDir->mStreakMeter->SetPartActive(i, active);
            }
        }
        for (int i = 0; i < mPlayer->NumSingers(); i++) {
            if (mPlayer->mSingers[i]) {
                MicClientID id = mPlayer->mSingers[i]->GetMicClientID();
                if (id.unk0 != -1) {
                    PitchArrow *arrow = mDir->GetPitchArrow(id.unk0);
                    if (arrow) {
                        arrow->ClearParticles();
                    }
                }
            }
        }
        mDir->RefreshCrowdRating(mLastRating, mLastRatingState);
        unk2ec = true;
    }
}

float VocalTrack::GetBottomDisplayPitch() const {
    if (mDir)
        return mDir->mLastMin;
    else
        return 0;
}

float VocalTrack::GetTopDisplayPitch() const {
    if (mDir)
        return mDir->mLastMax;
    else
        return 0;
}

VocalNoteList *VocalTrack::GetVocalNoteList(int part) {
    if (mAlternateNoteList[part])
        return mAlternateNoteList[part];
    else
        return TheSongDB->GetVocalNoteList(part);
}

void VocalTrack::SetAlternateNoteList(int part, VocalNoteList *notes) {
    MILO_ASSERT_RANGE(part, 0, 3, 0x53E);
    mAlternateNoteList[part] = notes;
}

void VocalTrack::HideCoda() {
    unk2ec = false;
    mDir->mBREGrp->SetShowing(false);
    mDir->mLeadBREGrp->SetShowing(false);
    mDir->mHarmonyBREGrp->SetShowing(false);
}

void VocalTrack::DumpLyricPlates(std::deque<LyricPlate *> &plates, bool lead) {
    MILO_LOG("Dumping %s lyric plates\n", lead ? "lead" : "harmony");
    int idx = 0;
    std::deque<LyricPlate *>::iterator it = plates.begin();
    std::deque<LyricPlate *>::iterator end = plates.end();
    for (; it != end; ++it) {
        LyricPlate *cur = *it;
        MILO_LOG(
            "[%d] %x (%.2f - %.2f) %s\n",
            idx,
            cur,
            !cur->mSyllables.empty() ? (cur->mSyllables.front()->mHighlightMs) / 1000.0f
                                     : -1.0f,
            cur->mInvalidateMs / 1000.0f,
            cur->mText->mText.c_str()
        );
        if (cur->Empty()) {
            MILO_LOG("\t<empty>\n");
        } else {
            for (int i = 0; i < cur->mSyllables.size(); i++) {
                Lyric *curLyric = cur->mSyllables[i];
                MILO_LOG("\t[%d] %x", i, curLyric);
                if (curLyric) {
                    MILO_LOG(
                        " %s x:%.2f (%.2f - %.2f)\n",
                        curLyric->mText.c_str(),
                        curLyric->mBeginPos.x,
                        curLyric->mActiveMs / 1000.0f,
                        curLyric->mEndMs / 1000.0f
                    );
                } else
                    MILO_LOG("\n");
            }
        }
        idx++;
    }
    MILO_LOG("\n");
}

void VocalTrack::Poll(float f1) {
    bool gamebool = TheGame->InRollback();
    if (f1 < unk2a4 && !gamebool) {
        RebuildHUD();
    }
    float f6 = unk78 * -(f1 / unk74);
    mDir->mScroller->SetLocalPos(f6, 0, 0);
    unk2a8 = f6 + mDir->mNowBarX;
    Track::Poll(f1);
    mDir->UpdatePartIsolation();
    mDir->SortArrowFx();
    UpdateScrolling(f1);
    UpdateTambourineGems();
    if (f1 > 0) {
        PollLyricAnimations(mLyricsLead, f1, true);
        PollLyricAnimations(mLyricsHarmony, f1, false);
    }
    PollKaraoke(f1);
    if (unk68) {
        const char *txt = MakeString("current: %i\n", mPlayer->PhraseScore());
        mDir->Find<RndText>("debug_score_current.txt", true)->SetText(txt);
    }
    if (!gamebool)
        unk2a4 = f1;
    if (mPlayer && unk2ec) {
        if (!mPlayer->CanDeployCoda()) {
            HideCoda();
        }
    }
}

void VocalTrack::PollKaraoke(float f1) {
    if (mPlayer) {
        int numSingers = mPlayer->NumSingers();
        int i;
        if (!unk2e5) {
            StartUpdateArrows();
            for (i = 0; i < numSingers; i++) {
                UpdatePitchArrow(f1, i);
            }
            UpdateUnusedArrows();
        }
        float f7 = 0;
        for (i = 0; i < 3; i++) {
            float clamped = Clamp<float>(0, 1, mPlayer->FramePhraseMeterFrac(i));
            int rating = mPlayer->CalculatePhraseRating(clamped);
            mDir->mStreakMeter->SetPartPct(i, clamped, rating <= 4);
            if (clamped > f7)
                f7 = clamped;
        }
        mDir->SetStreakPct(f7);
    }
}

bool VocalTrack::InTambourinePhrase() const {
    Player *p = GetPlayer();
    if (!p)
        return false;
    return p->InTambourinePhrase();
}

void VocalTrack::StartUpdateArrows() {
    for (int i = 0; i < 3; i++) {
        if (mDir->GetPitchArrow(i)) {
            mDir->GetPitchArrow(i)->unk18c = true;
        }
    }
}

void VocalTrack::UpdatePitchArrow(float ms, int singerIdx) {
    int phraseID =
        TheSongDB->GetCommonPhraseID(mTrackConfig.TrackNum(), MsToTickInt(ms));
    VocalPlayer *player = mPlayer;
    bool spotlight = phraseID != -1;
    bool enabled = player && player->GetEnabledState() == kPlayerEnabled;
    Singer *singer = player->mSingers[singerIdx];
    float pitchFrame = 0.0f;
    int arrowIdx = singerIdx;
    if (singer) {
        arrowIdx = singer->GetMicClientID().unk0;
    }
    PitchArrow *arrow = mDir->GetPitchArrow(arrowIdx);
    if (arrow) {
        int matchType = singer->GetFrameMatchType();
        bool inPhonemePhrase = matchType == 1;
        arrow->SetPitched(!inPhonemePhrase);
        arrow->SetSpotlight(spotlight);
        bool clampPitch = true;
        if (gDebugSpew) {
            MILO_LOG("--------\n");
            TheDebug << "singer->FrameTargetPitch()" << ": "
                     << singer->mFrameTargetPitch << "\n";
            TheDebug << "singer->FrameMicPitch()" << ": " << singer->mFrameMicPitch
                     << "\n";
            TheDebug << "singer->FrameBestHit()" << ": " << singer->unk6c << "\n";
            TheDebug << "mPlayer->Freestyling()" << ": " << mPlayer->Freestyling()
                     << "\n";
        }
        if (enabled && inPhonemePhrase) {
            VocalPart *part = NULL;
            if (singer->mFrameAssignedPart > -1) {
                part = mPlayer->mVocalParts[singer->mFrameAssignedPart];
            }
            VocalHUDColor color = (VocalHUDColor)-1;
            if (part) {
                color = (VocalHUDColor)part->unkc8;
            }
            arrow->SetFrameScore(singer->unk60, color, 0.0f);
            if (gDebugSpew) {
                MILO_LOG("phoneme phrase\n");
            }
        } else if (matchType != 0 || 0.0f == singer->mFrameMicPitch) {
            arrow->SetFrameScore(0.0f, (VocalHUDColor)-1, 0.0f);
            if (gDebugSpew) {
                MILO_LOG("non-singing section\n");
            }
        } else if (enabled && singer->mFrameTargetPitch > 0.0f) {
            VocalPart *part = NULL;
            float frameScore = singer->unk6c;
            if (singer->mFrameAssignedPart > -1) {
                part = mPlayer->mVocalParts[singer->mFrameAssignedPart];
            }
            VocalHUDColor color = (VocalHUDColor)-1;
            if (part) {
                color = (VocalHUDColor)part->unkc8;
            }
            pitchFrame = singer->mFrameTargetPitch - singer->mFrameMicPitch;
            float harmonyScore = GetHarmonyScore(singerIdx);
            arrow->SetFrameScore(frameScore, color, harmonyScore);
            if (gDebugSpew) {
                MILO_LOG("singing\n");
                TheDebug << "pitchFrame" << ": " << pitchFrame << "\n";
                TheDebug << "frameScore" << ": " << frameScore << "\n";
                TheDebug << "harmonyScore" << ": " << harmonyScore << "\n";
            }
        } else {
            arrow->SetFrameScore(0.0f, (VocalHUDColor)-1, 0.0f);
            clampPitch = false;
        }
        if (singer->mFrameMicPitch > 0.0f) {
            const Vector3 &v = arrow->LocalXfm().v;
            float vx = v.x;
            float vy = v.y;
            float vz = v.z;
            float pitchZ = mDir->PitchToZ(singer->mFrameMicPitch, clampPitch);
            if (gDebugSpew) {
                TheDebug << "pitchZ" << ": " << pitchZ << "\n";
            }
            if (std::fabs((pitchZ - vz / mDir->mPitchTopZ) - mDir->mPitchBottomZ) >
                0.9f) {
            } else {
                pitchZ += mDir->mArrowSmoothing * (vz - pitchZ);
            }
            if (gDebugSpew) {
                TheDebug << "v.z" << ": " << pitchZ << "\n";
            }
            arrow->SetLocalPos(vx, vy, pitchZ);
        }
        if (gDebugSpew) {
            TheDebug << "pitchFrame" << ": " << pitchFrame << "\n";
        }
        arrow->SetTiltDegrees(5.0f * pitchFrame);
        float volume = Clamp<float>(0, 1, 4.0f * singer->unk60);
        if (singer->mFrameAssignedPart != -1) {
            volume = std::max<float>(volume, 0.5f);
        }
        arrow->SetVolume(volume);
        arrow->SetGhostFade(0.0f);
        arrow->SetSplit(false);
        if (mPlayer->Freestyling()) {
            mDir->Find<RndAnimatable>("vocal_feedback.anim", true)
                ->SetFrame(singer->unk60, 1.0f);
        }
        arrow->unk18c = false;
    }
}

void VocalTrack::UpdateUnusedArrows() {
    for (int i = 0; i < 3; i++) {
        PitchArrow *arrow = mDir->GetPitchArrow(i);
        if (arrow && arrow->unk18c) {
            arrow->SetFrameScore(0.0f, (VocalHUDColor)-1, 0.0f);
            arrow->SetVolume(0.0f);
            arrow->unk18c = false;
        }
    }
}

void UpdateSyllableText(String &str, bool b2, bool &bref) {
    bref = false;
    if (b2 && !str.empty() && str.rindex(-1) == '-') {
        if (str.length() > 1) {
            str = str.substr(0, str.length() - 1);
            return;
        }
        str = "";
        return;
    }
    if (!str.empty() && str.rindex(-1) == '=') {
        str.rindex(-1) = '-';
        if (!b2)
            str += ' ';
    } else {
        str += ' ';
        if (b2)
            str += ' ';
        bref = true;
    }
}

void PrintLyricOneLine(const Lyric &lyric) {
    MILO_LOG("\t%3.2f\t(%6.2fms)\t", lyric.mBeginPos.x, lyric.mActiveMs);
    if (lyric.mDeployIdx > -1) {
        MILO_LOG("| ");
    }
    MILO_LOG("\"%s\"", lyric.mText.c_str());
    if (lyric.mChunkEnd) {
        MILO_LOG(" |");
    }
    MILO_LOG("\n");
}

bool VocalTrack::CheckDeploySections(
    Lyric *l1,
    float f2,
    int &i3,
    const std::vector<std::pair<float, float> > &pairs,
    bool b5,
    Lyric *l2,
    float &fref
) {
    bool ret = false;
    while (i3 < pairs.size() && pairs[i3].first < f2) {
        l1->SetAfterDeploy(i3);
        if (b5) {
            fref += mStaticDeployZoneXSize;
            if (l2)
                l2->SetChunkEnd(true);
        }
        ret = true;
        i3++;
    }
    return ret;
}

bool VocalTrack::IdenticalLyric(const VocalNote &n1, const VocalNote &n2) const {
    float f6 = Abs(n2.GetMs() - n1.GetMs());
    if (f6 == 0)
        return true;
    else if (f6 > mLyricOverlapWindowMs)
        return false;
    else if (n1.mText.length() != n2.mText.length())
        return false;
    else if (n1.mText == n2.mText)
        return true;
    else {
        String t1 = n1.mText;
        String t2 = n2.mText;
        t1.ToLower();
        t2.ToLower();
        return t1 == t2;
    }
}

void VocalTrack::BuildStaticDeployZone(
    int i1,
    const std::pair<float, float> &fpair,
    float f3,
    float &fref,
    std::deque<LyricShift> &shifts
) {
    ConfigNoteTube(false, 2, std::min(i1, 1), true, 1);
    HookupTubePlates(mNoteTube);
    float f10 = fref + mStaticDeployBufferX;
    fref = (f10 + mStaticDeployZoneXSize) - mStaticDeployMarginX;
    shifts.push_back(LyricShift(fpair.second, -fref));
    if (f3 != -1.0f) {
        float max = std::max<float>(mLyricShiftMs + fpair.second, f3);
        shifts.push_back(
            LyricShift(max, mStaticDeployMarginX + (-fref - mStaticDeployBufferX))
        );
    }
    bool i6 = TheSongDB->IsInCoda(MsToTickInt(fpair.first));
    float f1;
    RndGroup *u4;
    float f2;
    if (i1 == 0) {
        f1 = mDir->mTrackBottomZ + mDir->mPitchBottomZ;
        f1 = f1 * 0.5f;
        u4 = i6 ? mDir->mLeadBREGrp : mDir->mLeadLyricScrollGroup;
        f2 = mDir->mLeadLyricHeight * 0.5f;
    } else {
        f1 = mDir->mTrackTopZ + mDir->mPitchTopZ;
        f1 = f1 * 0.5f;
        u4 = i6 ? mDir->mHarmonyBREGrp : mDir->mHarmonyLyricScrollGroup;
        f2 = mDir->mHarmLyricHeight * 0.5f;
    }
    mNoteTube->SetPointPos(0, Vector3(0, 0, f1));
    mNoteTube->SetPointPos(1, Vector3(fref - f10, 0, f1));
    mNoteTube->unk_0x30 = f2;
    mNoteTube->SetBackParent(u4);
    mNoteTube->SetXPos(f10);
    mNoteTube->CreateMeshes();
    mNoteTube->SetDeployTiming(fpair.first, fpair.second);
    mNoteTube->BakePlates();
    if (gDebugSpew)
        MILO_LOG("new final deploy section for part %d\n", i1);
}

void VocalTrack::ProcessStaticLyrics(
    bool b1,
    Lyric *l2,
    float &f3,
    float &f4,
    Lyric *&l5,
    Lyric *&l6,
    float &f7,
    bool b8,
    LyricPlate *lp9
) {
    if (b1) {
        if (b8) {
            float v = f4;
            f3 = v;
            f7 = v;
            l5 = nullptr;
            l6 = nullptr;
        }
        float width = mDir->unk42c - mDir->mNowBarX;
        float halfWidth = 0.5f * width;
        f4 += lp9->EstimateLyricWidth(l2);
        float d2 = f4 - f3;
        if (l5 && !l6)
            l6 = l2;
        if (l5 && d2 > width) {
            l5->SetChunkEnd(true);
            l5 = nullptr;
            f3 = f7;
            d2 = f4 - f3;
            l6->SetAfterMidPhraseLyricShift(true);
            l6 = nullptr;
        }
        if (!l5 && d2 > halfWidth && l2->mWordEnd) {
            l5 = l2;
            f7 = f4;
        }
    }
}

void VocalTrack::Restart(VocalPlayer *player, float f1, float f2) {
    unk2a4 = -1.0f;
    mPlayer = player;
    mPhraseStartMs = 0;
    mPhraseEndMs = 0;
    mNextPhraseEndMs = 0;
    for (int i = 0; i < 3; i++)
        mNextScrollNote[i] = 0;
    for (int i = 0; i < 2; i++)
        mNextDeployZone[i] = 0;
    for (int i = 0; i < 2; i++)
        mCurLyricPhrase[i] = 0;
    unk108 = 0;
    unk104 = 1;
    unk100 = 0;
    unkf4 = 0;
    unkf8 = 0;
    unkfc = 0;
    mLeadLyricShifts.clear();
    mHarmonyLyricShifts.clear();
    unk23c = mStaticDeployMarginX;
    unk240 = unk23c;
    mDir->mLeadLyricScroller->DirtyLocalXfm().v.x = unk23c;
    mDir->mHarmonyLyricScroller->DirtyLocalXfm().v.x = unk240;
    unk2ac = unk23c;
    unk2b0 = unk240;
    mTambourineGemPool->FreeUsedGems();
    mTambourineGemPool->SetTambourineManager(&mPlayer->mTambourineManager);
    mDir->mBREGrp->SetShowing(true);
    mDir->mLeadBREGrp->SetShowing(true);
    mDir->mHarmonyBREGrp->SetShowing(true);
    unk2ec = true;
    UpdateVocalStyle();
}

void VocalTrack::HitTambourineGem(int id) {
    std::deque<TambourineGem *> &gems = mTambourineGemPool->mUsedGems;
    int count = gems.size();
    for (int i = 0; i != count; i++) {
        if (id == gems[i]->unk4) {
            gems[i]->unk8 = 1;
            gems[i];
            break;
        }
    }
    mDir->Tambourine(hit);
}

void VocalTrack::MissTambourineGem(int, bool b) {
    if (b)
        mDir->Tambourine(miss);
}

void VocalTrack::OnPhraseComplete(float f1, float f2, int i3) {
    BuildPhrase(f1, f2);
    if (unk68) {
        const char *txt = MakeString("last: %i\n", i3);
        mDir->Find<RndText>("debug_score_current.txt", true)->SetText(txt);
    }
}

void VocalTrack::ClearLyrics() {
    if (sDumpLyricPlates) {
        MILO_WARN("clearing all lyric plates\n");
        DumpLyricPlates(mLyricsLead, true);
        DumpLyricPlates(mLyricsHarmony, false);
    }
    while (mLyricsLead.size() != 0) {
        RELEASE(mLyricsLead.front());
        mLyricsLead.pop_front();
    }
    while (mLyricsHarmony.size() != 0) {
        RELEASE(mLyricsHarmony.front());
        mLyricsHarmony.pop_front();
    }
}

void VocalTrack::BuildPhrase(float f1, float f2) {
    mPhraseStartMs = mPhraseEndMs;
    mPhraseEndMs = f1;
    mNextPhraseEndMs = f2;
}

void VocalTrack::PushGameplayOptions(VocalParam p, int id) {
    Track::PushGameplayOptions(p, id);
    mCharOptParam = p;
    mCharOptMicID = id;
}

DataNode VocalTrack::OnGetDisplayMode(const DataArray *a) {
    if (IsScrolling()) {
        return "scrolling";
    } else
        return "static";
}

DataNode VocalTrack::OnSetDisplayMode(const DataArray *a) {
    if (a->Sym(2) == "static") {
        mVocalStyleOverride = kVocalStyleStatic;
        return a->Node(2);
    } else if (a->Sym(2) == "scrolling") {
        mVocalStyleOverride = kVocalStyleScrolling;
        return a->Node(2);
    } else
        return "unrecognized";
}

void VocalTrack::SetCanDeploy(bool can) {
    if (mDir->mPitchScrollGroup) {
        mDir->mPitchScrollGroup->SetShowing(can);
    }
    if (mDir->mLeadLyricScrollGroup) {
        mDir->mLeadLyricScrollGroup->SetShowing(can);
    }
    if (mDir->mHarmonyLyricScrollGroup) {
        mDir->mHarmonyLyricScrollGroup->SetShowing(can);
    }
}

int VocalTrack::GetNumVocalParts() {
    if (mPlayer)
        return mPlayer->NumVocalParts();
    else {
        MILO_NOTIFY_ONCE("invalid vocal player");
        return 0;
    }
}

BEGIN_HANDLERS(VocalTrack)
    HANDLE_ACTION(initialize, Init())
    HANDLE(set_display_mode, OnSetDisplayMode)
    HANDLE(display_mode, OnGetDisplayMode)
    HANDLE_ACTION(dump_plates, DumpAllPlates())
    HANDLE_EXPR(set_verbose_plates, sDumpPlateStates = _msg->Int(2))
    HANDLE_ACTION(reset_timing_data, ResetTimingData())
    HANDLE_SUPERCLASS(Track)
    HANDLE_CHECK(0xE6C)
END_HANDLERS

VocalTrack::LyricShift::LyricShift(float f1, float f2) : unk0(f2), unk4(f1), unk8(0) {
    if (dumpLyricShifts) {
        MILO_LOG(
            "New LyricShift begin %.2f sec, end %.2f x, fast: %d\n",
            f1 / 1000.0f,
            f2,
            false
        );
    }
}

VocalTrack::LyricShift::LyricShift(float f1, float f2, bool fast)
    : unk0(f2), unk4(f1), unk8(fast) {
    if (dumpLyricShifts) {
        MILO_LOG(
            "New LyricShift begin %.2f sec, end %.2f x, fast: %d\n", f1 / 1000.0f, f2, unk8
        );
    }
}
