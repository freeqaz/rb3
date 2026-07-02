#include "game/VocalPart.h"
#include "game/GameConfig.h"
#include "game/SongDB.h"
#include "game/VocalPlayer.h"
#include "obj/Data.h"
#include "os/Debug.h"
#include "os/System.h"
#include "synth/VoiceBeat.h"
#include <algorithm>
#include <cfloat>
#include <cmath>

VocalPart::VocalPart(VocalPlayer *vp, int idx)
    : mPlayer(vp), mPartIndex(idx), mVocalNoteList(0), mPhraseRatingTotal(0), mTotalPhrases(0), mLastPhraseScore(0),
      mRemotePhraseMeterFrac(0), mPhraseScorePartMultiplier(1.0f), mPhraseScoreMax(0),
      mPhraseScoreCapNote(0), mPhraseScore(0), mPhraseBandScore(0), mPhraseOverdriveScore(0), mPhrasesPercentagesSum(0), mPhrasesPercentagesCount(0), mLastMs(0),
      mLastBeginNote(0), mCurBestHit(0), mCurNoteMatched(-1), mSpotlightPhraseID(-1), mFrameMatchType(0), mMinSongPitch(FLT_MAX),
      mMaxSongPitch(-FLT_MAX), mMinPitch(0), mMaxPitch(0), mInFreestyleSection(0), mInDeployedFreeStyleSection(0), mCodaEndMs(0),
      mCodaResolved(0), mFirstPhraseMsToScore(0), mTempoVal(-1.0f), mBestSinger(0),
      mBestSingerPitchDistance(FLT_MAX), mColor(6), mScoringEnabled(1), mPhraseRank(0) {
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
    mMinSongPitch = FLT_MAX;
    mMaxSongPitch = -FLT_MAX;
    if (mVocalNoteList) {
        std::vector<VocalPhrase> &phrases = mVocalNoteList->mPhrases;
        FOREACH (it, phrases) {
            if (it->mNoteStart != it->mNoteEnd) {
                mMinSongPitch = Min(mMinSongPitch, it->mMinPitch);
                mMaxSongPitch = Max(mMaxSongPitch, it->mMaxPitch);
            }
        }
    }
}

void VocalPart::Restart(bool b1) {
    mSpotlightPhraseID = -1;
    if (!b1) {
        mTempoVal = -1.0f;
        mLastBeginNote = 0;
        mPhraseScoreCapNote = 0;
        mLastMs = 0;
        mPhraseScore = 0;
        mPhraseBandScore = 0;
        mPhraseOverdriveScore = 0;
        mPhraseRatingTotal = 0;
        mLastPhraseScore = 0;
        mPhrasesPercentagesSum = 0;
        mPhrasesPercentagesCount = 0;
        mInFreestyleSection = 0;
        mInDeployedFreeStyleSection = 0;
        mCodaResolved = 0;
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
    mLastBeginNote = 0;
    mPhraseScoreCapNote = 0;
    mLastMs = f1;
    mPhraseScore = 0;
    mPhraseBandScore = 0;
    mPhraseOverdriveScore = 0;
    mPhraseRatingTotal = 0;
    mLastPhraseScore = 0;
    mPhrasesPercentagesSum = 0;
    mPhrasesPercentagesCount = 0;
    mInFreestyleSection = 0;
    mInDeployedFreeStyleSection = 0;
    mCodaResolved = 0;
    mRemotePhraseMeterFrac = 0;
    mFirstPhraseMsToScore = 0;
    if (mVocalNoteList) {
        mThisPhrase = mVocalNoteList->mPhrases.begin();
        while (mThisPhrase != mVocalNoteList->mPhrases.end()
               && mThisPhrase->mStartMs + mThisPhrase->mDurationMs < f1) {
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
    mLastBeginNote = 0;
    VocalNoteList * &_ref0 = mVocalNoteList;
    mLastMs = ms;
    if (_ref0 != nullptr) {
        mThisPhrase = _ref0->mPhrases.begin();
        while (mThisPhrase != _ref0->mPhrases.end()
               && mThisPhrase->mStartMs + mThisPhrase->mDurationMs < ms) {
            mThisPhrase++;
        }
        mFreestyleSection = _ref0->mFreestyleSections.begin();
        while (mFreestyleSection != _ref0->mFreestyleSections.end()
               && ms > mFreestyleSection->second) {
            mFreestyleSection++;
        }
        mSpotlightPhraseID = -1;
        UpdateMinMaxPitch(mThisPhrase);
    }
}

void VocalPart::LocalDeployBandEnergy() {
    if (mInFreestyleSection)
        mInDeployedFreeStyleSection = true;
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
        mTotalPhrases = 0;
        for (std::vector<VocalPhrase>::const_iterator it =
                 mVocalNoteList->mPhrases.begin();
             it != mVocalNoteList->mPhrases.end();
             ++it) {
            if (it->mNoteStart != it->mNoteEnd) {
                mTotalPhrases++;
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

bool VocalPart::CouldScoreAgainstPart(
    float ms, TalkyMatcher *i_pTalkyMatcher, float pitch, float maxPitchDist, float &outPitch
) {
    int beginNote = -1;
    int endNote = -1;
    GetNoteRange(ms, beginNote, endNote);
    for (int noteIdx = beginNote; noteIdx < endNote; noteIdx++) {
        const VocalNote &note = mVocalNoteList->mNotes[noteIdx];
        if (note.mUnpitchedNote) {
            MILO_ASSERT(i_pTalkyMatcher, 0x1F2);
            bool unk1 = i_pTalkyMatcher->mVoiceBeat.unk1;
            bool unk0 = i_pTalkyMatcher->mVoiceBeat.unk0;
            bool overEnergy = i_pTalkyMatcher->mVoiceBeat.unk4 > mTalkyEnergyThreshold;
            if (mPlayer->IsAutoplay() || (unk1 && !unk0 && overEnergy)) {
                return true;
            }
        } else if (pitch != 0.0f) {
            float localPitch;
            float sloppyPitch = GetSloppyPitch(ms, noteIdx, pitch, localPitch);
            float absDiff = fabs(sloppyPitch - pitch);
            float diff = (float)fmod(absDiff, 12.0);
            float wrapped = 12.0f - diff;
            diff = Min(wrapped, diff);
            if (diff < maxPitchDist) {
                outPitch = sloppyPitch;
                return true;
            }
        }
    }
    outPitch = 0.0f;
    return false;
}

bool PitchBetween(float pitch, float a, float b, float &out);

float VocalPart::GetSloppyPitch(float ms, int noteIdx, float pitch, float &outPitch)
    const {
    const VocalNote &note = mVocalNoteList->mNotes[noteIdx];
    float pitchHi;
    float msPlus = ms + mSlop;
    if (note.mEndPitch == note.mBeginPitch) {
        pitchHi = (float)note.mBeginPitch;
    } else {
        float dur = note.mDurationMs;
        float noteMs = note.mMs;
        float endMs = noteMs + dur;
        msPlus = Min(endMs, msPlus);
        float rel = Max(msPlus - noteMs, 0.0f);
        float t = rel / dur;
        pitchHi = t * (float)note.mEndPitch + (1.0f - t) * (float)note.mBeginPitch;
    }
    float msMinus = ms - mSlop;
    float pitchLo;
    if (note.mEndPitch == note.mBeginPitch) {
        pitchLo = (float)note.mBeginPitch;
    } else {
        float dur = note.mDurationMs;
        float noteMs = note.mMs;
        float endMs = noteMs + dur;
        msMinus = Min(endMs, msMinus);
        float rel = Max(msMinus - noteMs, 0.0f);
        float t = rel / dur;
        pitchLo = t * (float)note.mEndPitch + (1.0f - t) * (float)note.mBeginPitch;
    }
    float modPitch = (float)fmod(pitch, 12.0);
    float modHi = (float)fmod(pitchHi, 12.0);
    float modLo = (float)fmod(pitchLo, 12.0);
    float between = -1.0f;
    if (!PitchBetween(pitch, pitchHi, pitchLo, between)) {
        float diffHi = fabsf(modPitch - modHi);
        float diffLo = fabs(modPitch - modLo);
        if (diffHi < diffLo) {
            float spEnd = note.mMs + note.mDurationMs;
            float spHi = ms + mSlop;
            const float *p = (spHi <= spEnd) ? &spHi : &spEnd;
            outPitch = *p;
            return pitchHi;
        }
        if (diffHi > diffLo) {
            float spMs = note.mMs;
            float spLo = ms - mSlop;
            const float *p = (spLo <= spMs) ? &spMs : &spLo;
            outPitch = *p;
            return pitchLo;
        }
        float noteMs = note.mMs;
        bool inRange = false;
        if (ms >= noteMs && ms < noteMs + note.mDurationMs)
            inRange = true;
        if (inRange) {
            outPitch = ms;
        } else {
            float endMs = noteMs + note.mDurationMs;
            if (endMs > noteMs) {
                if (endMs < ms)
                    noteMs = ms;
                else
                    noteMs = endMs;
            }
            outPitch = noteMs;
        }
        return pitchHi;
    }
    outPitch = pitch;
    return between;
}

void VocalPart::AddScore(const VocalScoreCache &c) { AddPhrasePoints(c.mFramePoints); }
void VocalPart::ForcePhrasePointDelta(float f1) { mPhraseScore += f1; }

void VocalPart::AddPhrasePoints(float pts) {
    float oldScore = mPhraseScore;
    float newScore = oldScore + pts;
    float cap = mPhraseScoreMax;
    cap = Min(mPhraseScoreCap, cap);
        cap = Min(cap, newScore);
    mPhraseScore = cap;
    float delta = mPhraseScore - oldScore;
    int i1, i2, i3;
    mPlayer->GetMultiplier(true, i1, i2, i3);
    mPhraseBandScore += delta * (float)(i2 - 1);
    mPhraseOverdriveScore += delta * (float)(i3 - 1);
}

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
    if (phrase->mNoteStart != phrase->mNoteEnd) return false;
    int idx = phrase->mNoteStart - 1;
    if (idx >= 0) {
        const VocalNote &note = mVocalNoteList->mNotes[idx];
        if (note.mMs + note.mDurationMs > phrase->mStartMs) return false;
    }
    return true;
}

bool VocalPart::AtPhraseEnd(float ms) const {
    const VocalPhrase *end =
        mVocalNoteList->mPhrases.data() + mVocalNoteList->mPhrases.size();
    if (mThisPhrase != end && ms > mThisPhrase->mStartMs + mThisPhrase->mDurationMs)
        return true;
    return false;
}

bool VocalPart::InEmptyPhrase() const {
    return IsEmptyPhrase(mThisPhrase);
}

bool VocalPart::PhraseHasUnpitchedNotes() const {
    const VocalPhrase *end = mVocalNoteList->mPhrases.data() + mVocalNoteList->mPhrases.size();
    if (mThisPhrase == end) return false;
    return mThisPhrase->mPitchRangeEnd;
}

bool VocalPart::InPlayablePhrase() const { return true; }

bool VocalPart::InTambourinePhrase() const {
    bool result = false;
    VocalNoteList *list = mVocalNoteList;
    const VocalPhrase *phrase = mThisPhrase;
    if (phrase != list->mPhrases.data() + list->mPhrases.size() && phrase->mTambourinePhrase)
        result = true;
    return result;
}

float VocalPart::FramePhraseMeterFrac() const {
    bool _cond = !mPlayer->IsNet();
    if (_cond) {
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
        mMaxPitch = 0.0f;
        mMinPitch = 0.0f;
        return;
    }
    bool foundPitchedNote = false;
    mMinPitch = FLT_MAX;
    mMaxPitch = -FLT_MAX;
    while (cur != end) {
        int lastNote = cur->mNoteEnd;
        int noteIdx = cur->mNoteStart;
        if (noteIdx != lastNote) {
            int noteCount = lastNote - noteIdx;
            for (int i = 0; i < noteCount; ++i) {
                if (!list->mNotes[noteIdx].mUnpitchedNote) {
                    foundPitchedNote = true;
                    mMinPitch = (cur->mMinPitch < mMinPitch) ? cur->mMinPitch : mMinPitch;
                    mMaxPitch = (mMaxPitch < cur->mMaxPitch) ? cur->mMaxPitch : mMaxPitch;
                    break;
                }
                ++noteIdx;
            }
        }
        if (cur->unk1a)
            break;
        cur++;
    }
    if (!foundPitchedNote) {
        mMinPitch = 50.0f;
        mMaxPitch = 67.0f;
        return;
    }
    if (mMinPitch == mMaxPitch) {
        mMinPitch -= 5.0f;
        mMaxPitch += 5.0f;
    }
}

int VocalPart::CalculateRemainingTambourineTicks() {
    MILO_ASSERT(mThisPhrase->mTambourinePhrase, 0x614);
    int dur = mThisPhrase->mDurationTicks;
    const VocalPhrase *sp8 = GetNextPhraseMarker(mThisPhrase);
    while (sp8 != mVocalNoteList->mPhrases.data() + mVocalNoteList->mPhrases.size()
           && sp8->mTambourinePhrase) {
        dur += sp8->mDurationTicks;
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
    if (mPhrasesPercentagesCount == 0) return 0.0f;
    float fPercentage = mPhrasesPercentagesSum / (float)mPhrasesPercentagesCount;
    MILO_ASSERT_RANGE_EQ(fPercentage, 0.0f, 1.0f, 0x6d6);
    return fPercentage;
}

float VocalPart::GetPartHitPercentage(const std::vector<VocalPhrase> &phrases, int, int) const {
    if (mPhrasesPercentagesCount == 0) return 0.0f;
    int numPhrases = NumPracticePhrases(phrases);
    float fPercentage = mPhrasesPercentagesSum / (float)numPhrases;
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

bool VocalNoteEndCmp(float f, const VocalNote &note) {
    return f < note.mMs + note.mDurationMs;
}

void VocalPart::AfterPoll(float ms) {
    int beginNote;
    int endNote;
    GetNoteRange(ms, beginNote, endNote);
    mLastBeginNote = beginNote & ~(beginNote >> 31);
    mLastMs = ms;
}

bool PitchBetween(float pitch, float a, float b, float &out) {
    float lo = (b < a) ? b : a;
    float hi = (a < b) ? b : a;
    while (pitch > hi)
        pitch -= 12.0f;
    while (pitch < lo)
        pitch += 12.0f;
    out = pitch;
    if (pitch >= lo && pitch <= hi)
        return true;
    return false;
}

static const float kFrameTimeMs = 16.666668f;

float VocalPart::GetNoteSliceWeight(float fBegin, float fEnd, int noteIdx) const {
    if (fEnd < fBegin) {
        float tmp = fBegin;
        fBegin = fEnd;
        fEnd = tmp;
    }
    const VocalNote &note = mVocalNoteList->mNotes[noteIdx];
    float noteMs = note.mMs;
    float noteDurationMs = note.mDurationMs;
    float fEndRel = fEnd - noteMs;
    float fBeginRel = fBegin - noteMs;
    float fDurationCap = 150.0f;
    if (fEndRel < fDurationCap)
        fDurationCap = noteDurationMs;
    float accum = 0.0f;
    if (note.mBeginPitch == note.mEndPitch) {
        // Loop 1: no pitch bend (unpitched or single pitch)
        float threshold = 0.0f;
        float frameMs = kFrameTimeMs;
        while (fBeginRel < fEndRel) {
            float spC = fEndRel - fBeginRel;
            float stepMs = std::min(spC, frameMs);
            float weight;
            if (fBeginRel < threshold) {
                weight = threshold;
            } else if (fBeginRel < fDurationCap) {
                weight = (float)pow((double)(fBeginRel / fDurationCap), 0.5);
            } else {
                weight = 1.0f;
            }
            accum = weight * stepMs + accum;
            fBeginRel += stepMs;
        }
    } else {
        // Loop 2: pitch bend
        float f22 = 1.0f - (float)pow((double)(4.0f / 7.0f), 2.0);
        float zeroThresh = accum;
        float half = 0.5f;
        float two = 2.0f;
        float seventeenFourths = 1.75f;
        while (fBeginRel < fEndRel) {
            float sp8 = fEndRel - fBeginRel;
            float stepMs = std::min(kFrameTimeMs, sp8);
            float weight;
            if (fBeginRel < zeroThresh) {
                weight = 1.0f;
            } else if (fBeginRel > noteDurationMs) {
                weight = 1.0f;
            } else {
                float t = fBeginRel / noteDurationMs;
                float x = (two * (t - half)) / seventeenFourths;
                weight = f22 + (float)pow((double)x, 2.0);
            }
            accum = weight * stepMs + accum;
            fBeginRel += stepMs;
        }
    }
    return accum;
}

float VocalPart::CalcPhraseScoreMax(const VocalPhrase *const &phrase) const {
    const VocalPhrase *p = phrase;
    VocalNoteList *list = mVocalNoteList;
    int start = p->mNoteStart;
    if (start > 0) {
        const VocalNote &prev = list->mNotes[start - 1];
        if (prev.mMs + prev.mDurationMs > p->mStartMs) {
            start--;
        }
    }
    unsigned int end = p->mNoteEnd;
    float result = 0.0f;
    if ((unsigned int)start == end) return result;
    float phraseStart = p->mStartMs;
    float phraseEnd = p->mStartMs + p->mDurationMs;
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

extern "C" float kInvalidPitch__11VocalPlayer;
extern "C" VocalNote *NoteAt__13VocalNoteListCFf(const VocalNoteList *, float);
extern "C" float PitchAt__13VocalNoteListCFf(const VocalNoteList *, float);

void VocalPart::Poll(float ms, const SongPos &) {
    while (mFreestyleSection
               != mVocalNoteList->mFreestyleSections.data()
                   + mVocalNoteList->mFreestyleSections.size()
           && ms > mFreestyleSection->second) {
        mFreestyleSection++;
    }
    if ((mPlayer->CanDeployOverdrive() || mPlayer->mIsInCoda
         || mPlayer->IsDeployingBandEnergy())
        && (mThisPhrase
                == mVocalNoteList->mPhrases.data() + mVocalNoteList->mPhrases.size()
            || (mFreestyleSection
                    != mVocalNoteList->mFreestyleSections.data()
                        + mVocalNoteList->mFreestyleSections.size()
                && ms >= mFreestyleSection->first
                && ms < mFreestyleSection->second))) {
        mInFreestyleSection = true;
    } else {
        mInFreestyleSection = false;
        mInDeployedFreeStyleSection = false;
    }
    mPlayer->IsNet();
    if (mPlayer->mIsInCoda && ms > mCodaEndMs) {
        mCodaResolved = true;
    }
    if (mInFreestyleSection) {
        mFrameMatchType = 3;
    } else if (mPlayer->InTambourinePhrase()) {
        mFrameMatchType = 2;
    } else {
        mFrameMatchType = 0;
    }
    int beginNote = -1;
    int endNote = -1;
    GetNoteRange(ms, beginNote, endNote);
    while (endNote > mPhraseScoreCapNote && mPhraseScoreCapNote < mThisPhrase->mNoteEnd) {
        mPhraseScoreCap += mPhraseScoreCapGrowth * mNoteWeights[mPhraseScoreCapNote];
        mPhraseScoreCapNote++;
    }
    int noteCount = mVocalNoteList->mNotes.size();
    int *pEnd = (noteCount < endNote) ? &noteCount : &endNote;
    int lastNote = *pEnd;
    endNote = lastNote;
    bool allUnpitched = true;
    for (int i = beginNote; i < lastNote; i++) {
        if (!mVocalNoteList->mNotes[i].mUnpitchedNote) {
            allUnpitched = false;
            break;
        }
    }
    if (allUnpitched && beginNote != lastNote) {
        mFrameMatchType = 1;
    }
    VocalFrameSpewData *spew = mPlayer->mFrameSpewData;
    if (spew) {
        float pitch = PitchAt__13VocalNoteListCFf(mVocalNoteList, ms);
        spew->mPartData[mPartIndex].mPartPitch = pitch;
    }
}

void VocalPart::HandlePhraseEnd(
    int &o_rRating, float &o_rStartMs, float &o_rEndMs, int &o_rPrevScore, float ms
) {
    if (mVocalNoteList) {
        mPlayer->IsNet();
        const VocalPhrase *nextPhrase = GetNextPhraseMarker(mThisPhrase);
        const VocalPhrase *phrase = nextPhrase;
        float startMs;
        float endMs;
        if (nextPhrase
            != mVocalNoteList->mPhrases.data() + mVocalNoteList->mPhrases.size()) {
            startMs = nextPhrase->mStartMs + nextPhrase->mDurationMs;
            const VocalPhrase *nextNext = GetNextPhraseMarker(phrase);
            if (nextNext
                != mVocalNoteList->mPhrases.data()
                    + mVocalNoteList->mPhrases.size()) {
                endMs = nextNext->mStartMs + nextNext->mDurationMs;
            } else {
                endMs = TheSongDB->GetSongDurationMs();
            }
        } else {
            startMs = TheSongDB->GetSongDurationMs();
            endMs = startMs;
        }
        o_rRating = -1;
        mLastPhraseScore = 0.0f;
        if (mPlayer->ScoringEnabled()
            && mThisPhrase
                != mVocalNoteList->mPhrases.data() + mVocalNoteList->mPhrases.size()
            && mPlayer->GetEnabledState() == kPlayerEnabled
            && mThisPhrase->mStartMs >= mFirstPhraseMsToScore) {
            float scoreMax = mPhraseScoreMax;
            if (scoreMax != 0.0f) {
                int rating = mPlayer->CalculatePhraseRating(mPhraseScore / scoreMax);
                o_rRating = rating;
                mPhraseRatingTotal += rating;
                float denom = mPhraseScoreMax;
                float mult = mPhraseScorePartMultiplier * (float)mPhraseValue;
                int accPts = (int)(0.5 + (double)((mPhraseScore * mult) / denom));
                int bandPts = (int)(0.5 + (double)((mPhraseBandScore * mult) / denom));
                int odPts = (int)(0.5 + (double)((mPhraseOverdriveScore * mult) / denom));
                int total = odPts + (bandPts + accPts);
                if (total > 0) {
                    int m1, m2, m3;
                    mPlayer->GetMultiplier(true, m1, m2, m3);
                    int indMult = mPlayer->GetIndividualMultiplier();
                    mLastPhraseScore = (float)(indMult * total);
                    if (mPhraseRank == 0) {
                        mPlayer->AddAccuracyStat(accPts);
                    } else {
                        mPlayer->AddHarmonyStat(accPts);
                    }
                    mPlayer->AddScoreStreakStat((float)(accPts * (indMult - 1)));
                    mPlayer->AddOverdriveStat((float)(odPts * indMult));
                    mPlayer->AddBandContributionStat((float)(bandPts * indMult));
                    mPlayer->AddPoints(mLastPhraseScore, false, false);
                }
            }
        }
        VocalNoteList *list = mVocalNoteList;
        if (phrase != list->mPhrases.data() + list->mPhrases.size()
            && phrase->mNoteStart != phrase->mNoteEnd) {
            mSpotlightPhraseID = TheSongDB->GetCommonPhraseID(
                TheGameConfig->GetTrackNum(mPlayer->GetUserGuid()),
                list->mNotes[phrase->mNoteStart].mTick
            );
        } else {
            mSpotlightPhraseID = -1;
        }
        if (mThisPhrase
                != mVocalNoteList->mPhrases.data() + mVocalNoteList->mPhrases.size()
            && mThisPhrase->unk1a) {
            UpdateMinMaxPitch(phrase);
        }
        if (mPlayer->ScoringEnabled() && mPhraseScoreMax > 0.0f) {
            mPhrasesPercentagesSum += FramePhraseMeterFrac();
            mPhrasesPercentagesCount += 1;
        }
        int prevScore = (int)mPhraseScore;
        if (mPlayer->ScoringEnabled()) {
            mPhraseScore = 0.0f;
            mPhraseBandScore = 0.0f;
            mPhraseOverdriveScore = 0.0f;
            mPhraseScoreCap = 0.0f;
        }
        if (phrase != mVocalNoteList->mPhrases.data() + mVocalNoteList->mPhrases.size()) {
            if (phrase->mNoteStart > 0) {
                const VocalNote &prev = mVocalNoteList->mNotes[phrase->mNoteStart - 1];
                if (prev.mMs + prev.mDurationMs > phrase->mStartMs) {
                    mPhraseScoreCapNote -= 1;
                }
            }
            mPhraseScoreMax = CalcPhraseScoreMax(phrase);
        } else {
            mPhraseScoreMax = 0.0f;
        }
        mThisPhrase = phrase;
        o_rStartMs = startMs;
        o_rEndMs = endMs;
        o_rPrevScore = prevScore;
    }
    static bool dump;
    if (dump) {
        TheDebug << MakeString("=== HandlePhraseEnd singer %d ms %f\n", mPartIndex, ms);
        if (mThisPhrase
            != mVocalNoteList->mPhrases.data() + mVocalNoteList->mPhrases.size()) {
            MILO_LOG("\tNext Phrase Data:\n");
            TheDebug << MakeString("\tStart ms: %f\n", mThisPhrase->mStartMs);
            TheDebug << MakeString(
                "\tEnd ms: %f\n", mThisPhrase->mStartMs + mThisPhrase->mDurationMs
            );
            TheDebug << MakeString("\tBegin Note: %d\n", mThisPhrase->mNoteStart);
            TheDebug << MakeString("\tEnd Note: %d\n", mThisPhrase->mNoteEnd);
        } else {
            MILO_LOG("\tEnd Of Song\n");
        }
    }
}

void VocalPart::ScoreSinger(
    float ms, float arg1, float arg2, float arg3, int arg4,
    TalkyMatcher *i_pTalkyMatcher, VocalScoreCache &o_rCache, int &o_rNote,
    float &o_rPitchDiff
) {
    MILO_ASSERT(o_rCache.GetHitPercentage() == 0.0f, 0x2C3);
    o_rCache.mPhrasePointsCap = Min(mPhraseScoreCap, mPhraseScoreMax);
    o_rPitchDiff = kInvalidPitch__11VocalPlayer;
    if (arg1 == 0.0f && NoteAt__13VocalNoteListCFf(mVocalNoteList, ms) == 0) {
        o_rCache.mHitPercentage = 1.0f;
        o_rNote = arg4;
        return;
    }
    int beginNote = -1;
    int endNote = -1;
    int noteMatched;
    float bestPitch = 0.0f;
    float pitch = arg1;
    int octaves = arg4;
    float sloppyArg;
    bool talkyHit;
    GetNoteRange(ms, beginNote, endNote);
    float score = GetBestHit(
        ms, beginNote, endNote, i_pTalkyMatcher, pitch, arg3, octaves, noteMatched,
        bestPitch, sloppyArg, talkyHit
    );
    o_rNote = octaves;
    if (noteMatched != -1) {
        float diff = (float)fmod(arg1 - bestPitch, 12.0);
        o_rPitchDiff = diff;
        if (diff > 6.0f) {
            o_rPitchDiff = diff - 12.0f;
        } else if (diff < -6.0f) {
            o_rPitchDiff = diff + 12.0f;
        }
        if (mVocalNoteList->mNotes[noteMatched].mUnpitchedNote) {
            mFrameMatchType = 1;
        } else {
            mFrameMatchType = 0;
        }
    }
    o_rCache.mHitPercentage = score;
    o_rCache.mTargetPitch = bestPitch;
    o_rCache.mTargetPitchMs = sloppyArg;
    o_rCache.mOctaveOffset = octaves;
    o_rCache.mVoiced = talkyHit;
    CalculateScore(ms, noteMatched, score, o_rCache);
}

float VocalPart::GetBestHit(
    float ms, int beginNote, int endNote, TalkyMatcher *i_pTalkyMatcher,
    float &io_rPitch, float arg5, int &o_rOctaves, int &noteMatched,
    float &o_rArg8, float &o_rArg9, bool &o_rTalkyHit
) {
    noteMatched = -1;
    float bestScore = 0.0f;
    float savedPitch = io_rPitch;
    int foundTalky = 0;
    o_rTalkyHit = false;
    for (int i = beginNote; i < endNote; i++) {
        const VocalNote &note = mVocalNoteList->mNotes[i];
        if (note.mUnpitchedNote) {
            MILO_ASSERT(i_pTalkyMatcher, 0x46D);
            bool vb1 = i_pTalkyMatcher->mVoiceBeat.unk1;
            bool vb0 = i_pTalkyMatcher->mVoiceBeat.unk0;
            bool overEnergy =
                i_pTalkyMatcher->mVoiceBeat.unk4 > mTalkyEnergyThreshold;
            float score = 1.0f;
            if (mPlayer->IsAutoplay() || (vb1 && !vb0 && overEnergy)) {
                if (foundTalky) {
                    MILO_ASSERT(noteMatched != -1, 0x486);
                    const VocalNote &best = mVocalNoteList->mNotes[noteMatched];
                    const VocalNote &cur = mVocalNoteList->mNotes[i];
                    if ((float)fabs((best.mMs + best.mDurationMs) - ms)
                        < (float)fabs(cur.mMs - ms)) {
                        score = 0.0f;
                    }
                }
                if (score >= bestScore) {
                    io_rPitch = savedPitch;
                    bestScore = score;
                    foundTalky = 1;
                    noteMatched = i;
                    o_rArg8 = -1.0f;
                    o_rOctaves = 0;
                    o_rArg9 = ms;
                    o_rTalkyHit = true;
                }
            }
        } else if (0.0f != io_rPitch) {
            float pitch = savedPitch;
            float sloppyPitch;
            int octaves = o_rOctaves;
            float spArg5;
            float score = ScoreNote(ms, i, pitch, octaves, sloppyPitch, spArg5);
            if (score >= bestScore || (score > 0.0 && foundTalky)) {
                bestScore = score;
                io_rPitch = pitch;
                foundTalky = 0;
                noteMatched = i;
                o_rArg8 = sloppyPitch;
                o_rOctaves = octaves;
                o_rArg9 = spArg5;
                o_rTalkyHit = false;
            }
        }
    }
    return bestScore;
}

float VocalPart::ScoreNote(
    float ms, int noteIdx, float &pitch, int &octavesOut, float &sloppyPitchOut,
    float &arg5
) const {
    sloppyPitchOut = GetSloppyPitch(ms, noteIdx, pitch, arg5);
    float diff = (float)sloppyPitchOut - pitch;
    float absDiff = (float)fabs(diff);
    float pitchClassDist = (float)fmod(absDiff, 12.0);
    pitchClassDist = Min(pitchClassDist, 12.0f - pitchClassDist);
    if (pitchClassDist <= 2.5f) {
        float fMagnitude = 0.5f + absDiff / 12.0f;
        int mag = (int)fMagnitude;
        int sign = (diff > 0.0f) ? 1 : -1;
        int octaves = mag * sign;
        diff = pitchClassDist;
        octavesOut = octaves;
        pitch += 12.0f * (float)octaves;
    }
    float score = 0.0f;
    if ((float)fabs(diff) <= mPitchMaximumDistance) {
        score = (float)exp(-(diff * diff) / mPitchSigma);
        if (score < 0.01f)
            score = 0.0f;
    }
    if (GetNoteSliceWeight(mLastMs, ms, noteIdx) == 0.0f)
        score = 0.0f;
    return score;
}

void VocalPart::CalculateScore(
    float ms, int noteIdx, float mult, VocalScoreCache &cache
) const {
    if (noteIdx == -1)
        return;
    float sliceWeight = GetNoteSliceWeight(mLastMs, ms, noteIdx);
    VocalNote &note = mVocalNoteList->mNotes[noteIdx];
    float noteMult;
    if (!note.mUnpitchedNote) {
        noteMult = mPitchHitMultiplier;
    } else if (note.mUnpitchedEasy) {
        noteMult = mNonPitchHitMultiplier * mNonPitchEasyMultiplier;
    } else {
        noteMult = mNonPitchHitMultiplier;
    }
    if (note.mDurationMs < mShortNoteThresh)
        noteMult *= mShortNoteMult;
    float framePoints = noteMult * (mult * sliceWeight);
    VocalFrameSpewData *spew = mPlayer->mFrameSpewData;
    if (spew) {
        spew->mPartData[mPartIndex].mUncappedFramePoints = framePoints;
        spew->mPartData[mPartIndex].mPointsCap = mPhraseScoreCap;
        spew->mPartData[mPartIndex].mHitPercentage = mult;
        spew->mPartData[mPartIndex].mWeight = sliceWeight;
        spew->mPartData[mPartIndex].mNoteMultiplier = noteMult;
    }
    cache.mUncappedFramePoints = framePoints;
    if (mPhraseScoreCap < mPhraseScore + framePoints)
        framePoints = mPhraseScoreCap - mPhraseScore;
    cache.mFramePoints = framePoints;
    float capped =
        Min(Min(mPhraseScoreCap, mPhraseScoreMax), sliceWeight * noteMult + mPhraseScore);
    float delta = capped - mPhraseScore;
    if (delta < 0.0f)
        delta = 0.0f;
    cache.mVibratoPoints = delta;
}

void VocalPart::GetNoteRange(float ms, int &startOut, int &endOut) {
    float slop = mSlop;
    float lower = ms - slop;
    float upper = ms + slop;
    const VocalNoteList *list = mVocalNoteList;
    startOut = -1;
    endOut = -1;
    const VocalNote *it = std::upper_bound(
        list->mNotes.data(),
        list->mNotes.data() + list->mNotes.size(),
        lower,
        VocalNoteEndCmp
    );
    if (it != list->mNotes.data() + list->mNotes.size()) {
        while (it->mMs < upper && it != list->mNotes.data() + list->mNotes.size()) {
            int idx = it - list->mNotes.data();
            if (startOut == -1)
                startOut = idx;
            endOut = idx + 1;
            ++it;
        }
    }
}

bool VocalPart::NearNote(float ms) {
    int start = -1, end = -1;
    GetNoteRange(ms, start, end);
    return start < end;
}

bool VocalPart::FramePhraseMeterFracSorter(const VocalPart *i_pA, const VocalPart *i_pB) {
    MILO_ASSERT(i_pA, 0x6c8);
    MILO_ASSERT(i_pB, 0x6c9);
    return i_pA->FramePhraseMeterFrac() > i_pB->FramePhraseMeterFrac();
}