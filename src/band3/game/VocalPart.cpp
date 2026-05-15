#include "game/VocalPart.h"
#include "game/SongDB.h"
#include "game/VocalPlayer.h"
#include "obj/Data.h"
#include "os/Debug.h"
#include "os/System.h"
#include <cfloat>

VocalPart::VocalPart(VocalPlayer *vp, int idx)
    : mPlayer(vp), mPartIndex(idx), mVocalNoteList(0), unk18(0), unk1c(0), unk20(0),
      mRemotePhraseMeterFrac(0), mPhraseScorePartMultiplier(1.0f), mPhraseScoreMax(0),
      unk3c(0), mPhraseScore(0), unk44(0), unk48(0), unk4c(0), unk50(0), unk54(0),
      unk58(0), unk84(0), unk88(-1), mSpotlightPhraseID(-1), unk98(0), unk9c(FLT_MAX),
      unka0(-FLT_MAX), unka4(0), unka8(0), mInFreestyleSection(0), unkad(0), unkb0(0),
      unkb4(0), mFirstPhraseMsToScore(0), unkbc(-1.0f), mBestSinger(0),
      mBestSingerPitchDistance(FLT_MAX), unkc8(6), mScoringEnabled(1), mPhraseRank(0) {
    SetDifficultyVariables(mPlayer->GetUser()->GetDifficulty());
}

VocalPart::~VocalPart() {}

void VocalPart::SetDifficultyVariables(int diff) {
    DataArray *voxCfg = SystemConfig("scoring", "vocals");
    mSlop = voxCfg->FindArray("slop")->Float(diff + 1);
    mPitchMaximumDistance = voxCfg->FindArray("pitch_margin")->Float(diff + 1);
    float log = std::log(0.1);
    mPitchSigma = -(mPitchMaximumDistance * mPitchMaximumDistance) / log;
    mPhraseValue = voxCfg->FindArray("phrase_value")->Int(diff + 1);
    mNoteLengthFactor = voxCfg->FindArray("note_length_factor")->Float(diff + 1);
    mPitchHitMultiplier = voxCfg->FindArray("pitch_hit_multiplier")->Float(diff + 1);
    mNonPitchHitMultiplier =
        voxCfg->FindArray("nonpitch_hit_multiplier")->Float(diff + 1);
    mNonPitchEasyMultiplier = voxCfg->FindArray("nonpitch_easy_multiplier")->Float(1);
    mPhraseScoreCapGrowth = voxCfg->FindArray("vocal_cap_growth")->Float(diff + 1);
    mShortNoteThresh = voxCfg->FindFloat("short_note_threshold_ms");
    mShortNoteMult = voxCfg->FindArray("short_note_multiplier")->Float(diff + 1);
    mTalkyEnergyThreshold = voxCfg->FindFloat("nonpitch_energy_threshold");
}

void VocalPart::PostLoad() {
    mVocalNoteList = TheSongDB->GetVocalNoteList(mPartIndex);
    mFreestyleSection = mVocalNoteList->mFreestyleSections.begin();
    mVocalNoteList->CapLastFreestyleSection(TheSongDB->GetSongDurationMs());
    CalcNoteWeights();
}

void VocalPart::Start() {}
void VocalPart::StartIntro() {}

void VocalPart::UpdateSongMinMaxPitch() {
    unk9c = FLT_MAX;
    unka0 = -FLT_MAX;
    if (mVocalNoteList) {
        std::vector<VocalPhrase> &phrases = mVocalNoteList->mPhrases;
        FOREACH (it, phrases) {
            if (it->unk10 != it->unk14) {
                unk9c = Min(unk9c, it->unk24);
                unka0 = Max(unka0, it->unk28);
            }
        }
    }
}

void VocalPart::Restart(bool b1) {
    mSpotlightPhraseID = -1;
    if (!b1) {
        unkbc = -1.0f;
        unk58 = 0;
        unk3c = 0;
        unk54 = 0;
        mPhraseScore = 0;
        unk44 = 0;
        unk48 = 0;
        unk18 = 0;
        unk20 = 0;
        unk4c = 0;
        unk50 = 0;
        mInFreestyleSection = 0;
        unkad = 0;
        unkb4 = 0;
        mRemotePhraseMeterFrac = 0;
        mFirstPhraseMsToScore = 0;
        CalcNoteWeights();
        if (mVocalNoteList) {
            mThisPhrase = mVocalNoteList->mPhrases.begin();
            mPhraseScoreMax = 0;
            UpdateMinMaxPitch(mThisPhrase);
            UpdateSongMinMaxPitch();
            mFreestyleSection = mVocalNoteList->mFreestyleSections.begin();
        }
    }
}

void VocalPart::SetPaused(bool) {}

void VocalPart::Jump(float f1, bool) {
    unk58 = 0;
    unk3c = 0;
    unk54 = f1;
    mPhraseScore = 0;
    unk44 = 0;
    unk48 = 0;
    unk18 = 0;
    unk20 = 0;
    unk4c = 0;
    unk50 = 0;
    mInFreestyleSection = 0;
    unkad = 0;
    unkb4 = 0;
    mRemotePhraseMeterFrac = 0;
    mFirstPhraseMsToScore = 0;
    if (mVocalNoteList) {
        mThisPhrase = mVocalNoteList->mPhrases.begin();
        while (mThisPhrase != mVocalNoteList->mPhrases.end()
               && mThisPhrase->unk0 + mThisPhrase->unk4 < f1) {
            mThisPhrase++;
        }
        mFreestyleSection = mVocalNoteList->mFreestyleSections.begin();
        while (mFreestyleSection != mVocalNoteList->mFreestyleSections.end()
               && f1 > mFreestyleSection->second) {
            mFreestyleSection++;
        }
        mSpotlightPhraseID = -1;
        UpdateMinMaxPitch(mThisPhrase);
    }
}

void VocalPart::LocalDeployBandEnergy() {
    if (mInFreestyleSection)
        unkad = true;
}

void VocalPart::CalcNoteWeights() {
    mNoteWeights.clear();
    if (mVocalNoteList) {
        mNoteWeights.reserve(mVocalNoteList->mNotes.size());
        for (unsigned int i = 0; i != mVocalNoteList->mNotes.size(); i++) {
            const VocalNote &note = mVocalNoteList->mNotes[i];
            float weight =
                GetNoteSliceWeight(note.mMs, note.mMs + note.mDurationMs, i);
            mNoteWeights.push_back(weight);
        }
        mThisPhrase = mVocalNoteList->mPhrases.begin();
        mPhraseScoreMax = 0;
        unk1c = 0;
        for (std::vector<VocalPhrase>::const_iterator it =
                 mVocalNoteList->mPhrases.begin();
             it != mVocalNoteList->mPhrases.end();
             ++it) {
            if (it->unk10 != it->unk14) {
                unk1c++;
            }
        }
    }
}

void VocalPart::EnableScoring(bool b) { mScoringEnabled = b; }
bool VocalPart::ScoringEnabled() const { return mScoringEnabled; }

void VocalPart::ResetScoring() {
    if (!IsEmptyPhrase(mThisPhrase)) {
        mPhraseScoreMax = CalcPhraseScoreMax(mThisPhrase);
    } else
        mPhraseScoreMax = 0;
}

void VocalPart::AddScore(const VocalScoreCache &c) { AddPhrasePoints(c.unk4); }
void VocalPart::ForcePhrasePointDelta(float f1) { mPhraseScore += f1; }

void VocalPart::SetPhraseScoreMultiplier(float f1) { mPhraseScorePartMultiplier = f1; }
void VocalPart::SetPhraseRank(int i) { mPhraseRank = i; }

void VocalPart::SetRemotePhraseMeterFrac(float f1) { mRemotePhraseMeterFrac = f1; }
void VocalPart::OnGameOver() {}

int VocalPart::GetSpotlightPhrase() const { return mSpotlightPhraseID; }

const VocalPhrase *VocalPart::GetFirstPhraseMarker() const {
    return mVocalNoteList->mPhrases.data();
}

const VocalPhrase *VocalPart::GetNextPhraseMarker(const VocalPhrase *const &p) const {
    const VocalPhrase *curPhrase = p;
    const VocalPhrase *end = mVocalNoteList->mPhrases.data() + mVocalNoteList->mPhrases.size();
    if (curPhrase == end) return curPhrase;
    return curPhrase + 1;
}

bool VocalPart::IsPhraseMarkerAtEnd(const VocalPhrase *const &p) const {
    const VocalPhrase *end = mVocalNoteList->mPhrases.data() + mVocalNoteList->mPhrases.size();
    return p == end;
}

bool VocalPart::IsEmptyPhrase(const VocalPhrase *const &p) const {
    const VocalPhrase *phrase = p;
    const VocalPhrase *end = mVocalNoteList->mPhrases.data() + mVocalNoteList->mPhrases.size();
    if (phrase == end) return true;
    if (phrase->mTambourinePhrase) return false;
    if (phrase->unk10 != phrase->unk14) return false;
    if (phrase->unk10 <= 0) return true;
    const VocalNote &note = mVocalNoteList->mNotes[phrase->unk10 - 1];
    if (note.mMs + note.mDurationMs > phrase->unk0) return false;
    return true;
}

bool VocalPart::InEmptyPhrase() const {
    return IsEmptyPhrase(mThisPhrase);
}

bool VocalPart::PhraseHasUnpitchedNotes() const {
    const VocalPhrase *end = mVocalNoteList->mPhrases.data() + mVocalNoteList->mPhrases.size();
    if (mThisPhrase == end) return false;
    return mThisPhrase->unk19;
}

bool VocalPart::InPlayablePhrase() const { return true; }

bool VocalPart::InTambourinePhrase() const {
    bool result = false;
    const VocalPhrase *end = mVocalNoteList->mPhrases.data() + mVocalNoteList->mPhrases.size();
    if (mThisPhrase != end && mThisPhrase->mTambourinePhrase)
        result = true;
    return result;
}

void VocalPart::SetFirstPhraseMsToScore(float f1) { mFirstPhraseMsToScore = f1; }

void VocalPart::AddSingerCandidate(Singer *singer, float dist) {
    if (mBestSinger) {
        if (!(dist > mBestSingerPitchDistance)) return;
    }
    mBestSinger = singer;
    mBestSingerPitchDistance = dist;
}

void VocalPart::ClearSingerCandidates() {
    mBestSinger = nullptr;
    mBestSingerPitchDistance = FLT_MAX;
}

Singer *VocalPart::GetBestSingerCandidate() { return mBestSinger; }

bool VocalPart::HasBestSingerCandidate() { return mBestSinger != nullptr; }

int VocalPart::CurrentPhraseIndex() const {
    return mThisPhrase - mVocalNoteList->mPhrases.data();
}

void VocalPart::SetVocalNoteList(VocalNoteList *list) {
    MILO_ASSERT(list, 0x771);
    mVocalNoteList = list;
    CalcNoteWeights();
    ResetScoring();
}

int VocalPart::NumPracticePhrases(const std::vector<VocalPhrase> &phrases) const {
    if (!mVocalNoteList) return 0;
    return mVocalNoteList->GetNumPracticePhrases(phrases);
}