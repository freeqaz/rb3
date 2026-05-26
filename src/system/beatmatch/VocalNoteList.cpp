#include "beatmatch/VocalNote.h"
#include "beatmatch/SongData.h"
#include "os/System.h"
#include "utl/MemMgr.h"
#include <algorithm>
#include <functional>

inline const char *VocalNoteList::PrintTick(int tick) const {
    return TickFormat(tick, *mSongData->GetMeasureMap());
}

VocalPhrase::VocalPhrase()
    : unk0(0), unk4(0), unk8(-1), unkc(-1), unk10(-1), unk14(-1), unk18(0), unk19(0),
      unk1a(0), unk1c(0), unk20(0), unk24(3.4028235E+38f), unk28(-3.4028235E+38f),
      unk2c(0), mTambourinePhrase(0), unk30(0), unk34(0) {}

VocalNoteList::VocalNoteList(SongData *data)
    : mSongData(data), mFreestyleMinDuration(0), mFreestylePad(0) {
    DataArray *scoringArr = SystemConfig()->FindArray("scoring", false);
    if (scoringArr) {
        DataArray *vocalsArr = SystemConfig("scoring")->FindArray("vocals", false);
        if (vocalsArr) {
            mFreestyleMinDuration =
                vocalsArr->FindArray("freestyle_min_duration")->Array(1);
            mFreestylePad = vocalsArr->FindArray("freestyle_pad")->Array(1);
            if (mFreestyleMinDuration->Size() != mFreestylePad->Size()) {
                MILO_WARN(
                    "scoring.dta: must have same number of items in both freestyle_min_duration and freestyle_pad."
                );
            }
        }
    }
}

void VocalNoteList::Clear() {
    mPhrases.clear();
    mLyricPhrases.clear();
    mNotes.clear();
    mTambourineGems.clear();
    mFreestyleSections.clear();
}

void CopyPhraseVec(const std::vector<VocalPhrase> &src, std::vector<VocalPhrase> *dest) {
    MILO_ASSERT(dest, 0x56);
    dest->clear();
    MILO_ASSERT(dest->size() == 0, 0x59);
    for (std::vector<VocalPhrase>::const_iterator it = src.begin(); it != src.end();
         it++) {
        dest->push_back(*it);
    }
    MILO_ASSERT(dest->size() == src.size(), 0x62);
}

void VocalNoteList::CopyPhrasesFrom(const VocalNoteList *srcList) {
    MILO_ASSERT(srcList, 0x68);
    CopyPhraseVec(srcList->mPhrases, &mPhrases);
}

void VocalNoteList::CopyLyricPhrases() { CopyPhraseVec(mPhrases, &mLyricPhrases); }

// fn_80497850
void VocalNoteList::AddNote(const VocalNote &note) {
    MemDoTempAllocations tmp(true, false);
    if (!mNotes.empty() && mNotes.back().GetTick() == note.GetTick()) {
        MILO_WARN(
            "%s (%s): double note-on at %s",
            mSongData->SongFullPath(),
            mTrackName,
            PrintTick(note.GetTick())
        );
    } else
        mNotes.push_back(note);
}

// fn_80497928
void VocalNoteList::NotesDone(const TempoMap &tmap, bool b) {
    static bool sDump;
    if (mPhrases.empty()) {
        if (!mNotes.empty()) {
            MILO_WARN(
                "%s (PART VOCALS): Vocal notes exist, but no vocal phrases found",
                mSongData->SongFullPath()
            );
        }
        return;
    }
    if (mNotes.empty())
        return;

    int ticktouse = mPhrases[0].unk8 < mNotes[0].GetTick() ? mPhrases[0].unk8
                                                           : mNotes[0].GetTick();
    if (b) {
        VocalPhrase phrase;
        phrase.unk8 = 0;
        if (0x280 < ticktouse)
            phrase.unkc = ticktouse - 0x280;
        else
            phrase.unkc = ticktouse;
        mPhrases.insert(mPhrases.begin(), phrase);
    }

    int noteIdx = 0;
    float currentMin = 3.4028235E+38f;
    float currentMax = -3.4028235E+38f;
    int lastRangeBoundingPhrase = -1;
    if (sDump)
        MILO_LOG("parsing phrase data\n");
    for (int phraseIdx = 1; phraseIdx < mPhrases.size(); phraseIdx++) {
        VocalPhrase &phrase = mPhrases[phraseIdx];
        phrase.unk18 = 0;
        phrase.unk19 = 0;
        phrase.unk10 = noteIdx;
        phrase.unk14 = noteIdx;
        for (; noteIdx != mNotes.size(); noteIdx++) {
            VocalNote &note = mNotes[noteIdx];
            if (note.GetTick() < phrase.unk8) {
                if (b) {
                    MILO_WARN(
                        "%s (%s): vocal note at tick %s is outside any phrases",
                        mSongData->SongFullPath(),
                        mTrackName,
                        PrintTick(note.GetTick())
                    );
                    phrase.unkc += phrase.unk8 - note.GetTick();
                    phrase.unk8 = note.GetTick();
                } else {
                    MILO_WARN(
                        "%s (%s): vocal note [%d-%d] at tick %s is outside any phrases",
                        mSongData->SongFullPath(),
                        mTrackName,
                        note.StartPitch(),
                        note.EndPitch(),
                        PrintTick(note.GetTick())
                    );
                }
            }
            if (note.GetTick() >= phrase.unk8 + phrase.unkc)
                break;
            phrase.unk14++;
            if (note.IsUnpitched())
                phrase.unk19 = 1;
            if (b && note.GetTick() + note.GetDurationTicks() > phrase.unk8 + phrase.unkc) {
                MILO_WARN(
                    "%s (%s): vocal note at tick %s extends beyond phrase",
                    mSongData->SongFullPath(),
                    mTrackName,
                    PrintTick(note.GetTick())
                );
            }
        }

        if (phrase.unk10 != phrase.unk14) {
            mNotes[phrase.unk14 - 1].SetPhraseEnd(true);
        }
        if (b) {
            mLyricPhrases.push_back(phrase);
        }

        for (int j = phrase.unk10; j < phrase.unk14; j++) {
            if (!mNotes[j].IsUnpitched()) {
                phrase.unk18 = 1;
                phrase.unk24 = Min<float>((float)mNotes[j].StartPitch(), phrase.unk24);
                phrase.unk24 = Min<float>((float)mNotes[j].EndPitch(), phrase.unk24);
                phrase.unk28 = Max<float>(phrase.unk28, (float)mNotes[j].StartPitch());
                phrase.unk28 = Max<float>(phrase.unk28, (float)mNotes[j].EndPitch());
            }
            if (b && mNotes[j].LyricShift()) {
                VocalPhrase &backphrase = mLyricPhrases.back();
                int endtick = mNotes[j].EndTick();
                int oldStart = backphrase.unk8;
                int oldDur = backphrase.unkc;
                backphrase.unkc = endtick - oldStart;
                int oldEnd = oldStart + oldDur;
                VocalPhrase newphrase;
                newphrase.unk8 = endtick;
                newphrase.unkc = oldEnd - endtick;
                mLyricPhrases.push_back(newphrase);
            }
        }

        currentMin = Min<float>(phrase.unk24, currentMin);
        currentMax = Max<float>(currentMax, phrase.unk28);
        if (phrase.unk1a || phraseIdx + 1 == mPhrases.size()) {
            for (int k = lastRangeBoundingPhrase + 1; k <= phraseIdx; k++) {
                mPhrases[k].unk24 = currentMin;
                mPhrases[k].unk28 = currentMax;
            }
            currentMin = 3.4028235E+38f;
            lastRangeBoundingPhrase = phraseIdx;
            currentMax = -3.4028235E+38f;
        }
    }

    if (sDump) {
        for (int i = 0; i < mPhrases.size(); i++) {
            MILO_LOG(
                "[%d] ticks: (%d, %d), min: %.0f max: %.0f bounding: %d\n",
                i,
                mPhrases[i].unk8,
                mPhrases[i].unk8 + mPhrases[i].unkc,
                mPhrases[i].unk24,
                mPhrases[i].unk28,
                mPhrases[i].unk1a
            );
        }
    }

    if (noteIdx != mNotes.size()) {
        MILO_WARN(
            "%s (%s): vocal notes past end of last phrase are being discarded",
            mSongData->SongFullPath(),
            mTrackName
        );
        mNotes.resize(noteIdx);
    }

    for (int i = 0; i < mTambourineGems.size(); i++) {
        int gem = mTambourineGems[i];
        int phraseIdx = 0;
        while (phraseIdx < mPhrases.size()
               && gem >= mPhrases[phraseIdx].unk8 + mPhrases[phraseIdx].unkc) {
            phraseIdx++;
        }
        if (phraseIdx < mPhrases.size() && gem >= mPhrases[phraseIdx].unk8
            && mPhrases[phraseIdx].unk10 == mPhrases[phraseIdx].unk14) {
            mPhrases[phraseIdx].mTambourinePhrase = true;
        } else {
            MILO_LOG(
                "NOTIFY: %s (%s): tambourine gem at tick %s not in phrase or in singing phrase; discarding\n",
                mSongData->SongFullPath(),
                mTrackName,
                PrintTick(gem)
            );
            mTambourineGems.erase(mTambourineGems.begin() + i);
            i--;
        }
    }

    if (b)
        DeterminePhraseTimes(tmap);

    for (int i = 0; i != mPhrases.size(); i++) {
        VocalPhrase &phrase = mPhrases[i];
        for (int j = phrase.unk10; j < phrase.unk14; j++) {
            mNotes[j].mPhrase = i;
            mNotes[j].mPlayerMask = phrase.unk2c;
        }
    }
    Finalize();
}

void VocalNoteList::DeterminePhraseTimes(const TempoMap &tmap) {
    for (unsigned int i = 0; mPhrases.size() != i; i++) {
        VocalPhrase *phrase = mPhrases.begin() + i;
        int prevEnd = 0;
        if (i != 0) {
            prevEnd = phrase[-1].unk8 + phrase[-1].unkc;
        }
        if (i != 0 && phrase->mTambourinePhrase
            && phrase->unk8 > prevEnd + 0x780) {
            VocalPhrase newPhrase;
            newPhrase.unk8 = prevEnd;
            newPhrase.unkc = (phrase->unk8 - prevEnd) - 0x280;
            VocalPhrase *insertPos = &mPhrases[i];
            newPhrase.mTambourinePhrase = insertPos[-1].mTambourinePhrase;
            mPhrases.insert(insertPos, newPhrase);
            i--;
        } else {
            phrase->unkc = phrase->unkc + (phrase->unk8 - prevEnd);
            phrase->unk8 = prevEnd;
            float startTime = tmap.TickToTime(prevEnd);
            float endTime = tmap.TickToTime(phrase->unk8 + phrase->unkc);
            phrase->unk0 = startTime;
            phrase->unk4 = endTime - startTime;
        }
    }
}

void VocalNoteList::StartPlayerPhrase(int tick, int player) {
    if (!mPhrases.empty() && mPhrases.back().unkc == -1) {
        if (tick > mPhrases.back().unk8 + 0x1e0) {
            MILO_WARN(
                "%s (%s): confused by vocal phrase overlap around tick %s",
                mSongData->SongFullPath(),
                mTrackName,
                PrintTick(tick)
            );
        }
    } else {
        VocalPhrase phrase;
        mPhrases.push_back(phrase);
        mPhrases.back().unk8 = tick;
    }
    mPhrases.back().unk2c |= 1 << player;
}

void VocalNoteList::EndPlayerPhrase(int tick, int) {
    MILO_ASSERT(!mPhrases.empty(), 0x24d);
    if (mPhrases.back().unkc != -1
        && tick > mPhrases.back().unk8 + mPhrases.back().unkc + 0x1e0) {
        MILO_WARN(
            "%s (%s): confused by vocal phrase overlap around tick %s",
            mSongData->SongFullPath(),
            mTrackName,
            PrintTick(tick)
        );
    }
    int duration = tick - mPhrases.back().unk8;
    if (duration < 0x1e0) {
        MILO_WARN(
            "%s (%s): confused by vocal phrase overlap around tick %s",
            mSongData->SongFullPath(),
            mTrackName,
            PrintTick(tick)
        );
    }
    mPhrases.back().unkc = duration;
}

void VocalNoteList::Finalize() {
    std::vector<VocalNote>(mNotes).swap(mNotes);
    DetermineFreestyleSections();
}

void VocalNoteList::DetermineFreestyleSections() {
    MILO_ASSERT(mFreestyleSections.empty(), 0x287);
    float sectionStart = 0.0f;
    bool atWordBoundary = true;
    for (std::vector<VocalNote>::iterator note = mNotes.begin(); note != mNotes.end();
         ++note) {
        if (atWordBoundary) {
            float gap = note->GetMs() - sectionStart;
            for (int i = 0; i < mFreestyleMinDuration->Size(); i++) {
                float pad = mFreestylePad->Float(i);
                float minDuration = mFreestyleMinDuration->Float(i);
                if (gap > 64.0f * pad + minDuration) {
                    mFreestyleSections.push_back(
                        std::make_pair(sectionStart + pad, note->GetMs() - pad)
                    );
                    break;
                }
            }
        }
        atWordBoundary = false;
        sectionStart = note->EndMs();
        if (note->mText.empty()
            || (note->mText.rindex(-1) != '-' && note->mText.rindex(-1) != '=')) {
            atWordBoundary = true;
        }
    }
    mFreestyleSections.push_back(std::make_pair(
        sectionStart + mFreestyleMinDuration->Float(0), 3.4028235E+38f
    ));
}

void VocalNoteList::AddTambourineGem(int gem) { mTambourineGems.push_back(gem); }

void VocalNoteList::SetFreestyleSections(const std::vector<std::pair<float, float> > &sects
) {
    mFreestyleSections = sects;
}

bool VocalNoteList::IsIllegalFreestyleSection(
    DataArray *arr, const std::pair<float, float> &section
) {
    float duration = section.second - section.first;
    for (int i = 0; i < arr->Size(); i++) {
        if (duration >= arr->Float(i))
            return false;
    }
    return true;
}

void VocalNoteList::GenerateLegalFreestyleSections(
    std::vector<std::pair<float, float> > &out
) const {
    float sectionStart = 0.0f;
    float pad = mFreestylePad->Float(0);
    for (const VocalNote *note = mNotes.data();
         note != mNotes.data() + mNotes.size();
         ++note) {
        if (note->IsUnpitched()) {
            float sectionEnd = note->GetMs() - pad;
            std::pair<float, float> p(sectionStart, sectionEnd);
            if (p.second - p.first > 0.0f) {
                out.push_back(p);
            }
            sectionStart = pad + note->EndMs();
        }
    }
    out.push_back(std::make_pair(sectionStart, 3.4028235E+38f));
}

void VocalNoteList::RemoveInvalidFreestyleSections() {
    std::binder1st<
        std::pointer_to_binary_function<
            DataArray *, const std::pair<float, float> &, bool> >
        pred(std::ptr_fun(IsIllegalFreestyleSection), mFreestyleMinDuration);
    std::vector<std::pair<float, float> >::iterator first =
        mFreestyleSections.begin();
    std::vector<std::pair<float, float> >::iterator last =
        mFreestyleSections.end();
    int tripCount = (last - first) >> 2;
    for (; tripCount > 0; --tripCount) {
        if (pred(*first))
            goto found;
        ++first;
        if (pred(*first))
            goto found;
        ++first;
        if (pred(*first))
            goto found;
        ++first;
        if (pred(*first))
            goto found;
        ++first;
    }
    switch (last - first) {
    case 3:
        if (pred(*first))
            goto found;
        ++first;
    case 2:
        if (pred(*first))
            goto found;
        ++first;
    case 1:
        if (pred(*first))
            goto found;
    default:
        first = last;
    }
found:
    if (first != last) {
        std::vector<std::pair<float, float> >::iterator it = first;
        for (++it; it != last; ++it) {
            if (!pred(*it)) {
                *first = *it;
                ++first;
            }
        }
        mFreestyleSections.erase(first, last);
    }
}

void VocalNoteList::CapLastFreestyleSection(float ms) {
    while (!mFreestyleSections.empty() && mFreestyleSections.back().first >= ms) {
        mFreestyleSections.erase(mFreestyleSections.end() - 1, mFreestyleSections.end());
    }
    if (!mFreestyleSections.empty() && mFreestyleSections.back().second > ms) {
        mFreestyleSections.back().second = ms;
    }
}

bool VocalNoteCmp(float ms, const VocalNote &note) { return ms < note.GetMs(); }

VocalNote *VocalNoteList::NextNote(float ms) const {
    if (mNotes.size() == 0)
        return NULL;
    std::vector<VocalNote>::const_iterator it =
        std::upper_bound(mNotes.begin(), mNotes.end(), ms, VocalNoteCmp);
    if (it == mNotes.begin())
        return (VocalNote *)it;
    if (ms <= it[-1].GetMs() + it[-1].GetDurationMs())
        return (VocalNote *)(it - 1);
    if (it == mNotes.end())
        return NULL;
    return (VocalNote *)it;
}

const VocalNote *VocalNoteList::NoteAt(float ms) const {
    const VocalNote *it =
        std::upper_bound(mNotes.begin(), mNotes.end(), ms, VocalNoteCmp);
    if (it == mNotes.begin())
        return NULL;
    --it;
    MILO_ASSERT(it->GetMs() <= ms, 0x22f);
    if (ms <= it->GetMs() + it->GetDurationMs())
        return it;
    return NULL;
}

float VocalNoteList::PitchAt(float ms) const {
    const VocalNote *it =
        std::upper_bound(mNotes.begin(), mNotes.end(), ms, VocalNoteCmp);
    if (it == mNotes.begin())
        return 0.0f;
    --it;
    MILO_ASSERT(it->GetMs() <= ms, 0x1ff);
    float noteMs = it->GetMs();
    float noteDur = it->GetDurationMs();
    if (ms <= noteMs + noteDur) {
        if (it->EndPitch() == it->StartPitch())
            return (float)it->StartPitch();
        float fraction =
            Max<float>(0.0f, Min<float>(ms, noteMs + noteDur) - noteMs)
            / noteDur;
        return (1.0f - fraction) * (float)it->StartPitch()
            + fraction * (float)it->EndPitch();
    }
    return 0.0f;
}

void VocalNoteList::GetPracticePhrases(
    std::vector<VocalPhrase> &out, int startTick, int endTick
) const {
    for (const VocalPhrase *phrase = mPhrases.data();
         phrase != mPhrases.data() + mPhrases.size();
         ++phrase) {
        if (startTick < phrase->unk8 + phrase->unkc
            && endTick > phrase->unk8) {
            out.push_back(*phrase);
        }
    }
}

void VocalNoteList::GetPracticePhrases2(
    std::vector<VocalPhrase> &out, int startTick, int endTick
) const {
    for (const VocalPhrase *phrase = mPhrases.data();
         phrase != mPhrases.data() + mPhrases.size();
         ++phrase) {
        if (startTick < phrase->unk8 + phrase->unkc && endTick > phrase->unk8
            && phrase->unk8 + phrase->unkc <= endTick) {
            out.push_back(*phrase);
        }
    }
}

int VocalNoteList::GetNumPracticePhrases(const std::vector<VocalPhrase> &phrases) const {
    int count = 0;
    for (const VocalPhrase *phrase = phrases.data();
         phrase != phrases.data() + phrases.size();
         ++phrase) {
        if (HasNoteInRange(phrase->unk8, phrase->unk8 + phrase->unkc) != -1)
            count++;
    }
    return count;
}

void VocalNoteList::AddLyricShift(float ms) {
    std::vector<VocalNote>::iterator it =
        std::upper_bound(mNotes.begin(), mNotes.end(), ms, VocalNoteCmp);
    if (it == mNotes.begin()) {
        MILO_WARN(
            "%s: Added lyric shift before lyrics at time %f",
            mSongData->SongFullPath(),
            ms
        );
    } else {
        it[-1].mLyricShift = true;
    }
}

bool VocalNote::PlayableBy(int activeNum) const {
    MILO_ASSERT(activeNum == 0 || activeNum == 1, 0x3d2);
    return (mPlayerMask & (1 << activeNum)) != 0;
}

void VocalNoteList::UpdatePitchRangeTickDelimited(
    int startTick, int endTick, float &min, float &max
) {
    VocalNote *end = mNotes.data() + mNotes.size();
    for (VocalNote *it = mNotes.data(); it != end; ++it) {
        if (it->IsUnpitched())
            continue;
        if (it->GetTick() < startTick)
            continue;
        if (endTick > -1 && it->GetTick() > endTick)
            break;
        int startPitch = it->StartPitch();
        if ((float)startPitch < min)
            min = (float)startPitch;
        if ((float)startPitch > max)
            max = (float)startPitch;
        int endPitch = it->EndPitch();
        if ((float)endPitch < min)
            min = (float)endPitch;
        if ((float)endPitch > max)
            max = (float)endPitch;
    }
}

int VocalNoteList::HasNoteInRange(int startTick, int endTick) const {
    for (const VocalNote *it = mNotes.data(); it != mNotes.data() + mNotes.size();
         ++it) {
        if (!it->IsUnpitched() && it->GetTick() <= endTick
            && it->EndTick() >= startTick) {
            return it->GetTick();
        }
    }
    return -1;
}