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

void VocalPart::Rollback(float, float ms) {
    unk58 = 0;
    unk54 = ms;
    if (mVocalNoteList != nullptr) {
        mThisPhrase = mVocalNoteList->mPhrases.begin();
        while (mThisPhrase != mVocalNoteList->mPhrases.end()
               && mThisPhrase->unk0 + mThisPhrase->unk4 < ms) {
            mThisPhrase++;
        }
        mFreestyleSection = mVocalNoteList->mFreestyleSections.begin();
        while (mFreestyleSection != mVocalNoteList->mFreestyleSections.end()
               && ms > mFreestyleSection->second) {
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

float VocalPart::FramePhraseMeterFrac() const {
    if (!mPlayer->IsNet()) {
        float ratio = 0.0f;
        if (mPhraseScoreMax != 0.0f)
            ratio = mPhraseScore / mPhraseScoreMax;
        if (ratio > 1.0f) return 1.0f;
        if (ratio < 0.0f) return 0.0f;
        return ratio;
    }
    return mRemotePhraseMeterFrac;
}

void VocalPart::UpdateMinMaxPitch(const VocalPhrase *const &phraseRef) {
    VocalNoteList *list = mVocalNoteList;
    const VocalPhrase *cur = phraseRef;
    const VocalPhrase *end = list->mPhrases.data() + list->mPhrases.size();
    if (cur == end) {
        unka8 = 0.0f;
        unka4 = 0.0f;
        return;
    }
    bool foundPitchedNote = false;
    unka4 = FLT_MAX;
    unka8 = -FLT_MAX;
    while (cur != end) {
        if (cur->unk10 != cur->unk14) {
            int noteIdx = cur->unk10;
            int noteCount = cur->unk14 - cur->unk10;
            if (cur->unk10 < cur->unk14) {
                while (true) {
                    const VocalNote &note = list->mNotes[noteIdx];
                    if (!note.mUnpitchedNote) {
                        foundPitchedNote = true;
                        if (cur->unk24 < unka4)
                            unka4 = cur->unk24;
                        if (unka8 < cur->unk28)
                            unka8 = cur->unk28;
                        break;
                    } else {
                        noteIdx++;
                        noteCount--;
                        if (noteCount == 0)
                            break;
                    }
                }
            }
        }
        if (cur->unk1a)
            break;
        cur++;
    }
    if (!foundPitchedNote) {
        unka4 = 50.0f;
        unka8 = 67.0f;
        return;
    }
    if (unka4 == unka8) {
        unka4 = unka4 - 5.0f;
        unka8 = unka8 + 5.0f;
    }
}

int VocalPart::CalculateRemainingTambourineTicks() {
    MILO_ASSERT(mThisPhrase->mTambourinePhrase, 0x614);
    int dur = mThisPhrase->unkc;
    const VocalPhrase *sp8 = GetNextPhraseMarker(mThisPhrase);
    while (true) {
        const VocalPhrase *end =
            mVocalNoteList->mPhrases.data() + mVocalNoteList->mPhrases.size();
        if (sp8 == end)
            break;
        if (!sp8->mTambourinePhrase)
            break;
        dur += sp8->unkc;
        sp8 = GetNextPhraseMarker(sp8);
    }
    return dur;
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

float VocalPart::GetOverallPartHitPercentage() const {
    if (unk50 == 0) return 0.0f;
    float fPercentage = unk4c / (float)unk50;
    MILO_ASSERT_RANGE_EQ(fPercentage, 0.0f, 1.0f, 0x6d6);
    return fPercentage;
}

float VocalPart::GetPartHitPercentage(const std::vector<VocalPhrase> &phrases, int, int) const {
    if (unk50 == 0) return 0.0f;
    int numPhrases = NumPracticePhrases(phrases);
    float fPercentage = unk4c / (float)numPhrases;
    MILO_ASSERT_RANGE_EQ(fPercentage, 0.0f, 1.0f, 0x6e4);
    return fPercentage;
}

float VocalPart::GetFreestyleSectionDurationMs() const {
    MILO_ASSERT(mInFreestyleSection, 0x6ab);
    VocalNoteList *list = mVocalNoteList;
    const std::pair<float, float> *end =
        list->mFreestyleSections.data() + list->mFreestyleSections.size();
    if (mFreestyleSection == end)
        return 0.0f;
    return mFreestyleSection->second - mFreestyleSection->first;
}

float VocalPart::CalcPhraseScoreMax(const VocalPhrase *const &phrase) const {
    const VocalPhrase *p = phrase;
    VocalNoteList *list = mVocalNoteList;
    int start = p->unk10;
    if (start > 0) {
        const VocalNote &prev = list->mNotes[start - 1];
        if (prev.mMs + prev.mDurationMs > p->unk0) {
            start--;
        }
    }
    unsigned int end = p->unk14;
    float result = 0.0f;
    if ((unsigned int)start == end) return result;
    float phraseStart = p->unk0;
    float phraseEnd = p->unk0 + p->unk4;
    for (unsigned int i = start; i != end; i++) {
        const VocalNote &note = list->mNotes[i];
        float noteMs = note.mMs;
        float noteDurationMs = note.mDurationMs;
        float clampedStart = (noteMs < phraseStart) ? phraseStart : noteMs;
        float noteEnd = noteMs + noteDurationMs;
        float clampedEnd = (phraseEnd < noteEnd) ? phraseEnd : noteEnd;
        float duration = clampedEnd - clampedStart;
        float weight = mNoteWeights[i];
        result += (duration / noteDurationMs) * weight;
    }
    return result;
}

bool VocalPart::FramePhraseMeterFracSorter(const VocalPart *i_pA, const VocalPart *i_pB) {
    MILO_ASSERT(i_pA, 0x6c8);
    MILO_ASSERT(i_pB, 0x6c9);
    return i_pA->FramePhraseMeterFrac() > i_pB->FramePhraseMeterFrac();
}