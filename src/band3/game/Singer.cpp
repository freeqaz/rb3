#include "game/Singer.h"
#include "VocalScoreHistory.h"
#include "dsp/VibratoDetector.h"
#include "game/BandUser.h"
#include "game/Defines.h"
#include "game/GameMic.h"
#include "game/GameMicManager.h"
#include "game/SongDB.h"
#include "game/VocalPlayer.h"
#include "net/Net.h"
#include "net/NetSession.h"
#include "obj/Data.h"
#include "obj/Task.h"
#include "os/Debug.h"
#include "os/System.h"
#include "synth/MicManagerInterface.h"
#include "synth/VoiceBeat.h"
#include <algorithm>

MicClientID sNullClientID(-1, -1);

Singer::Singer(VocalPlayer *vp, int n)
    : mPlayer(vp), unkc(0), mSingerIndex(n), unk14(0), unk18(0), unk1c(0), mIsSinging(0),
      mDetune(0), unk2c(0), unk30(0), unk38(100.0f), unk3c(0), unk40(0), unk44(0),
      unk48(-1.0f), mScreamEnergyThreshold(0.8f), unk50(500.0f), mFrameMicPitch(0),
      unk60(0), unk64(0), unk6c(0), mFrameAssignedPart(-1), unk74(0), mOctaveOffset(0),
      unk7c(0), unk80(0), unk84(0), unk88(0), unka0(0), unka4(0), unka8(0), mVibrato(0),
      unk244(0), mVibratoFrameBonus(0), unk24c(-1.0f), mAutoplayPart(-1),
      mAutoplayVariationMagnitude(0), mAutoplayOffset(0),
      mTambourineDetector(vp->mTambourineManager, this), unk29c(0), unk2a0(0), unk2a4(0) {
    CreateMicClientID();
    Difficulty diff = mPlayer->GetUser()->GetDifficulty();
    DataArray *cfg = SystemConfig("scoring", "vocals");
    cfg->FindArray("pitch_margin")->Float(diff + 1); // lol what happened to this
    mMaxDetune = cfg->FindFloat("max_detune");
    mScreamEnergyThreshold = cfg->FindFloat("scream_energy_threshold");
    unk38 = cfg->FindFloat("tambourine_deployment_suppress_ms");
    mVibrato = new VibratoDetector(0, 100);
    mTalkyMatcher = new TalkyMatcher();
    for (int i = 0; i < 5; i++)
        mPitchHistory[i] = 0;

    if (n == 0) {
        GameMic *mic = TheGameMicManager->GetMic(mMicClientID);
        if (mic) {
            DataNode node = DataVariable("playback_file");
            if (node.Type() == kDataString) {
                if (strlen(node.Str()) != 0) {
                    mic->SetInputFile(node.Str());
                }
            }
        }
    }
}

Singer::~Singer() {
    RELEASE(mTalkyMatcher);
    RELEASE(mVibrato);
    RELEASE(unk18);
}

void Singer::PostLoad() {
    int numParts = mPlayer->NumVocalParts();
    for (int i = 0; i < numParts; i++) {
        mScoreHistories.push_back(VocalScoreHistory(i, mSingerIndex));
    }
    mScoreCaches.resize(numParts);
    mResultsData.resize(numParts);
    MILO_ASSERT(mTalkyMatcher, 0xB6);
    mTalkyMatcher->LoadEvents(
        TheSongDB->GetData()->mVocalFeatureVectorTimes,
        TheSongDB->GetData()->mVocalFeatureVectorPeaks
    );
}

void Singer::CreateMicClientID() {
    BandUser *u = mPlayer->GetUser();
    if ((!TheNet.GetNetSession()->HasUser(u) || !u->IsLocal()) && !u->IsNullUser()) {
        mMicClientID = sNullClientID;
    } else {
        mMicClientID = MicClientID(mSingerIndex, -1);
    }
}

GameMic *Singer::GetGameMic() const { return TheGameMicManager->GetMic(mMicClientID); }
MicClientID Singer::GetMicClientID() const { return mMicClientID; }

void Singer::SetMicProcessing(bool b1, bool b2) {
    GameMic *mic = TheGameMicManager->GetMic(mMicClientID);
    if (mic)
        mic->SetEnablePitchDetection(b1);
    if (mTalkyMatcher)
        mTalkyMatcher->SetEnableTalkyMatcher(b2);
}

void Singer::Start() {}
void Singer::StartIntro() {}

void Singer::Restart(bool b1) {
    CancelScream();
    mFrameAssignedPart = -1;
    ClearFreestyleDeployment();
    ClearScoreHistories();
    unk64 = 0;
    unk80 = 0;
    if (!b1) {
        FOREACH (it, mResultsData) {
            it->Reset();
        }
        unk29c = 0;
        unk2a0 = 0;
        unk2a4 = 0;
    }
    mAmbiguousData.clear();
}

void Singer::SetPaused(bool) {}

void Singer::Jump(float, bool) {
    CancelScream();
    mFrameAssignedPart = -1;
    ClearFreestyleDeployment();
    ClearScoreHistories();
    mAmbiguousData.clear();
}

void Singer::Rollback(float, float) {
    CancelScream();
    ClearFreestyleDeployment();
    mAmbiguousData.clear();
}

void Singer::ProcessTalkyData() {
    MILO_ASSERT(mTalkyMatcher, 0x2DC);
    GameMic *mic = GetGameMic();
    if (!mic)
        mTalkyMatcher->Reset();
    else {
        float secs = TheTaskMgr.Seconds(TaskMgr::kRealTime);
        const short *s = nullptr;
        int i28 = 0;
        mic->AccessContinuousSamples(s, i28);
        mTalkyMatcher->Analyze(s, i28, secs * 1000.0f);
    }
}

void Singer::DetectScream(float f1, float f2, float f3) {
    MILO_ASSERT(mPlayer->IsLocal(), 0x2F6);
    if (f3 >= mScreamEnergyThreshold) {
        if (unk48 < 0) {
            unk48 = f1;
        } else if (f1 - unk48 > unk50 && mPlayer->mIsInCoda && !unk80) {
            unk80 = true;
            mPlayer->HitCoda();
        }
    } else
        CancelScream();
}

void Singer::CancelScream() { unk48 = -1.0f; }

void Singer::SetIsSinging(bool b1) { mIsSinging = b1; }
void Singer::Detune(float f1) { mDetune = f1; }

void Singer::HandlePhraseEnd(float, const std::vector<float> &micPitches) {
    MILO_ASSERT(micPitches.size() == mResultsData.size(), 0x3BF);
    for (int i = 0; i < (int)mResultsData.size(); i++) {
        float micPitch = micPitches[i];
        if (micPitch > 0.0f) {
            mResultsData[i].unk18 +=
                std::max(std::min(mResultsData[i].unk10 / micPitch, 1.0f), 0.0f);
            mResultsData[i].unk1c++;
        }
        mResultsData[i].unk10 = 0.0f;
        mResultsData[i].unk14 = 0.0f;
        mResultsData[i].unkc = 0.0f;
        if (micPitch > 0.0f) {
            mResultsData[i].unk4 +=
                std::max(std::min(mResultsData[i].unk0 / micPitch, 1.0f), 0.0f);
            mResultsData[i].unk8++;
        }
        mResultsData[i].unk0 = 0.0f;
    }
    mAmbiguousData.clear();
}

void Singer::SetFrameMicPitch(float f1) { mFrameMicPitch = f1; }
void Singer::EnableController() {}
void Singer::DisableController() {}

void Singer::SetOctaveOffset(int i1) {
    if (i1 != mOctaveOffset)
        mOctaveOffset = i1;
}

void Singer::AppendToScoreHistory(float f1, int i2, float f3, int i4) {
    VocalScoreHistory &history = mScoreHistories[i2];
    history.AddScore(f1, f3);
    history.SetOctaveOffset(i4);
}

float Singer::GetHistoricalScore(float f1, int i2) const {
    return mScoreHistories[i2].CalculateSum(f1);
}

VocalScoreHistory &Singer::AccessScoreHistory(int idx) { return mScoreHistories[idx]; }
VocalScoreCache &Singer::AccessScoreCache(int idx) { return mScoreCaches[idx]; }
const VocalScoreCache &Singer::AccessScoreCache(int idx) const {
    return mScoreCaches[idx];
}

void Singer::AllScoresAreIn(const std::vector<int> &scores) {
    MILO_ASSERT(mResultsData.size() == mScoreCaches.size(), 0x4B6);
    for (int i = 0; i < (int)mResultsData.size(); i++) {
        float sum = mResultsData[i].unk10 + mScoreCaches[i].unk4;
        float cacheUnk8 = mScoreCaches[i].unk8;
        mResultsData[i].unk10 = (cacheUnk8 < sum) ? cacheUnk8 : sum;
        mResultsData[i].unk14 += mScoreCaches[i].unkc;
        mResultsData[i].unkc += mScoreCaches[i].unk0;
    }
    for (AmbiguousData *entry = &mAmbiguousData[0]; entry != &mAmbiguousData[0] + mAmbiguousData.size(); entry++) {
        if (entry->unk8)
            continue;
        int part0 = entry->unk0;
        if (part0 != mFrameAssignedPart) {
            if (std::find(scores.begin(), scores.end(), part0) != scores.end()) {
                entry->unk8 = true;
                continue;
            }
        }
        int part4 = entry->unk4;
        if (part4 != mFrameAssignedPart) {
            if (std::find(scores.begin(), scores.end(), part4) != scores.end()) {
                entry->unk8 = true;
            }
        }
    }
}

void Singer::NoteTambourineSwing(float f1) {
    ClearFreestyleDeployment();
    unk3c = f1 + unk38;
}

void Singer::ClearFreestyleDeployment() {
    unk3c = 0;
    unk40 = 0;
    unk44 = 0;
}

void Singer::SetAutoplayToPart(int part) { mAutoplayPart = part; }
int Singer::GetAutoplayToPart() const { return mAutoplayPart; }
void Singer::SetAutoplayVariationMagnitude(float f1) { mAutoplayVariationMagnitude = f1; }
float Singer::GetAutoplayVariationMagnitude() const {
    return mAutoplayVariationMagnitude;
}
void Singer::SetAutoplayOffset(float f1) { mAutoplayOffset = f1; }
float Singer::GetAutoplayOffset() const { return mAutoplayOffset; }

void Singer::ClearScoreHistories() {
    FOREACH (it, mScoreHistories) {
        it->Reset();
    }
}

void Singer::ClearPitchHistory() {
    unka0 = 0;
    unka4 = 0;
    unka8 = 0;
    mPitchHistory[0] = 0;
    mPitchHistory[1] = 0;
    mPitchHistory[2] = 0;
    mPitchHistory[3] = 0;
    mPitchHistory[4] = 0;
}

void Singer::UpdatePitchHistory(float pitch) {
    if ((unsigned int)unka4 > 4) {
        TheDebug.Notify(MakeString("pitch history index out of bounds (%d) singer %d", unka4, mSingerIndex));
        ClearPitchHistory();
    }
    float prev = mPitchHistory[unka4];
    if ((pitch > 0.0f) != (prev > 0.0f)) {
        if (pitch > 0.0f) {
            unka8 += 1;
            unka0 = unka0 + (pitch - unka0) / (float)unka8;
        } else {
            unka8 -= 1;
            if (unka8 == 0) ClearPitchHistory();
            if ((unsigned int)unka8 > 5) {
                TheDebug.Notify(MakeString("pitch history valid frames out of bounds (%d)", unka8));
                ClearPitchHistory();
            }
        }
    } else if (pitch > 0.0f) {
        unka0 = unka0 + (pitch - prev) / (float)unka8;
    }
    mPitchHistory[unka4] = pitch;
    unka4 = (unka4 + 1) % 5;
}

int Singer::SuddenOctaveShift(float pitch) const {
    if (unka8 < 1) return 0;
    if (!(pitch > 0.0f)) return 0;
    int sign;
    if (pitch > unka0) sign = 1;
    else sign = -1;
    int shift = 0;
    float step = 12.0f * (float)sign;
    while (true) {
        float diff = pitch - unka0;
        if (!(diff > 0.0f)) diff = -diff;
        if (!(diff > 10.0f)) break;
        pitch -= step;
        shift += sign;
    }
    return shift;
}

void Singer::UpdatePitchDeviation(float pitch) {
    float mean = unk29c;
    int count = unk2a4 + 1;
    float dev = unk2a0;
    unk2a4 = count;
    float newMean = mean + (pitch - mean) / (float)count;
    unk29c = newMean;
    unk2a0 = dev + (std::fabs(pitch - newMean) - dev) / (float)count;
}

float Singer::GetPartPercentage(int part) const {
    const SingerResultsData &rd = mResultsData[part];
    if (rd.unk1c == 0) return 0.0f;
    return rd.unk18 / (float)rd.unk1c;
}

int Singer::GetFrameMatchType() {
    if (mFrameAssignedPart != -1) {
        return mPlayer->mVocalParts[mFrameAssignedPart]->unk98;
    }
    return 4;
}

float Singer::AddToFreestyleDeployment(float val) {
    if (mFrameMicPitch < mScreamEnergyThreshold) {
        unk40 = 0;
        unk44 = 0;
    } else if (val >= unk3c) {
        if (unk40 > 0.0f) {
            float diff = val - unk40;
            if (diff > 0.0f) {
                unk44 += diff;
            }
        }
        unk40 = val;
    }
    return unk44;
}

void Singer::ResolveAmbiguity() {
    for (AmbiguousData *entry = &mAmbiguousData[0];
         entry != &mAmbiguousData[0] + mAmbiguousData.size(); entry++) {
        if (!entry->unk8 || entry->unkc == -1)
            continue;
        int part1 = entry->unk0;
        int part2 = entry->unk4;
        float points1 = mResultsData[part1].unkc;
        float points2 = mResultsData[part2].unkc;
        float delta = points1 - points2;
        float maxPoints = (points1 < points2) ? points2 : points1;
        if (std::fabs(delta) / maxPoints > 0.1f) {
            int iWinningPart = (delta > 0.0f) ? part1 : part2;
            int iLosingPart = (delta < 0.0f) ? part1 : part2;
            MILO_ASSERT(iWinningPart != iLosingPart, 0x1B4);
            if (iWinningPart != entry->unkc) {
                float pts = entry->unk10;
                mPlayer->SwapAmbiguousPoints(pts, iLosingPart, iWinningPart);
                mResultsData[iLosingPart].unk0 -= pts;
                if (mResultsData[iLosingPart].unk0 < 0.0f)
                    mResultsData[iLosingPart].unk0 = 0.0f;
                mResultsData[iWinningPart].unk0 += pts;
            }
            entry->unkc = -1;
        }
    }
}