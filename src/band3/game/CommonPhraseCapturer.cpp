#include "game/CommonPhraseCapturer.h"
#include "Player.h"
#include "bandtrack/TrackPanel.h"
#include "beatmatch/GameGem.h"
#include "game/Game.h"
#include "game/GemPlayer.h"
#include "game/SongDB.h"
#include "os/Debug.h"

CommonPhraseCapturer::CommonPhraseCapturer() {
    ExtendPhraseStates(50);
    Reset();
}

void CommonPhraseCapturer::Reset() {
    mDisabledTracks = 0;
    mLastStartedPhraseID = -1;
    std::vector<Player *> &players = TheGame->GetActivePlayers();
    for (int i = 0; i < players.size(); i++) {
        if (players[i]->GetEnabledState() == kPlayerDisconnected)
            mDisabledTracks |= 1 << players[i]->GetTrackNum();
    }
    mFinishedTracks = 0;
    mInUnisonPhrase = 0;
    for (int i = 0; i < mPhraseStates.size(); i++) {
        mPhraseStates[i].unk0 = 0;
        mPhraseStates[i].unk4 = 0;
        mPhraseStates[i].unk8 = 0;
    }
}

void CommonPhraseCapturer::HandlePhraseNote(GemPlayer *p, int i2, int i3, bool b4) {
    if (i3 != -1) {
        int tracks;
        int phraseID = TheSongDB->GetPhraseID(i2, i3);
        if (phraseID != -1) {
            ExtendPhraseStates(phraseID);
            tracks = TheSongDB->GetCommonPhraseTracks(phraseID);
            tracks &= ~mDisabledTracks;
            bool unison = TheSongDB->IsUnisonPhrase(phraseID);
            if (phraseID != TheSongDB->GetPhraseID(i2, i3 + 1) && unison) {
                mFinishedTracks |= 1 << i2;
                if (tracks == (tracks & mFinishedTracks)) {
                    if (IsMultiplayerPhrase(phraseID)) {
                        GetTrackPanelDir()->UnisonEnd();
                    }
                    mFinishedTracks = 0;
                    mInUnisonPhrase = false;
                }
            }

            if (unison && (i3 == 0 || TheSongDB->GetPhraseID(i2, i3 - 1) == -1)
                && mLastStartedPhraseID != phraseID && !mInUnisonPhrase) {
                if (IsMultiplayerPhrase(phraseID)) {
                    GetTrackPanel()->UnisonStart(tracks);
                }
                mInUnisonPhrase = true;
                mLastStartedPhraseID = phraseID;
            }

            if (b4) {
                if (HasPlayedWholePhrase(p, phraseID, i2, i3)) {
                    HitLastGem(p, phraseID, i2);
                }
            } else
                Fail(p, phraseID, i2);
        }
    }
}

void CommonPhraseCapturer::HandleVocalPhrase(Player *p, int track, int i3, bool b4) {
    ExtendPhraseStates(i3);
    if (!mPhraseStates[i3].unk0) {
        if (b4) {
            int trackBits = TheSongDB->GetCommonPhraseTracks(i3);
            MILO_ASSERT((trackBits & (1<<track)) == (1<<track), 0x7F);
            HitLastGem(p, i3, track);
        } else
            Fail(p, i3, track);
    }
}

void CommonPhraseCapturer::LocalHitLastGem(Player *p, int i2, int i3) {
    OneTrackCompletedPhrase(i2, i3);
    if (TheSongDB->IsUnisonPhrase(i2)) {
        ExtendPhraseStates(i2);
        PhraseState &state = mPhraseStates[i2];
        if (state.unk0 != 2) {
            state.unk4 |= 1 << i3;
            int trackBits = TheSongDB->GetCommonPhraseTracks(i2);
            trackBits |= mPhraseStates[i2].unk8;
            if (trackBits == (trackBits & mDisabledTracks | mPhraseStates[i2].unk4)) {
                AllTracksCompletedPhrase(i2);
            }
            GetTrackPanel()->UnisonPlayerSuccess(p);
        }
    }
}

void CommonPhraseCapturer::LocalFail(Player *p, int i2, int i3) {
    if ((TheSongDB->GetCommonPhraseTracks(i2) & ~mDisabledTracks)
        && (1 << p->GetTrackNum()) != 0) {
        p->UnisonMiss(i2);
    }
    ExtendPhraseStates(i2);
    mPhraseStates[i2].unk0 = 2;
    mPhraseStates[i2].unk8 |= 1 << i3;
    if (TheSongDB->IsUnisonPhrase(i2) && GetTrackPanel()) {
        GetTrackPanel()->UnisonPlayerFailure(p);
    }
}

bool CommonPhraseCapturer::DidTrackFail(int i1, int i2) const {
    if (i1 == -1 || i1 >= mPhraseStates.size())
        return false;
    else
        return (1 << i2) & mPhraseStates[i1].unk8;
}

void CommonPhraseCapturer::Enabled(Player *p, int i2, int i3, bool b4) {
    if (b4) {
        mDisabledTracks &= ~(1 << i2);
    } else {
        int phraseID = TheSongDB->GetCommonPhraseID(i2, i3);
        if (phraseID != -1) {
            Fail(p, phraseID, i2);
        }
        mDisabledTracks |= 1 << i2;
    }
}

bool CommonPhraseCapturer::HasPlayedWholePhrase(GemPlayer *p, int phraseID, int trackNum, int gemIdx) {
    if ((1 << trackNum) & mPhraseStates[phraseID].unk8)
        return false;
    const std::vector<GameGem> &gems = TheSongDB->GetGems(trackNum);
    while ((unsigned)(gemIdx + 1) < gems.size()
           && TheSongDB->GetPhraseID(trackNum, gemIdx + 1) == phraseID) {
        gemIdx++;
    }
    for (; gemIdx >= 0; gemIdx--) {
        if (TheSongDB->GetPhraseID(trackNum, gemIdx) != phraseID)
            return true;
        if (gems[gemIdx].PlayableBy(p->GetSlot()) && !p->HasDealtWithGem(gemIdx))
            return false;
    }
    return true;
}

void CommonPhraseCapturer::HitLastGem(Player *p, int i2, int i3) {
    if (p->IsLocal()) {
        LocalHitLastGem(p, i2, i3);
        static Message msg("send_hit_last_unison_gem", 0, 0);
        msg[0] = i2;
        msg[1] = i3;
        p->HandleType(msg);
    }
}

void CommonPhraseCapturer::Fail(Player *p, int i2, int i3) {
    if (p->IsLocal()) {
        LocalFail(p, i2, i3);
        static Message msg("send_fail_unison_phrase", 0, 0);
        msg[0] = i2;
        msg[1] = i3;
        p->HandleType(msg);
    }
}

void CommonPhraseCapturer::OneTrackCompletedPhrase(int phraseID, int trackNum) {
    Player *player = TheGame->GetPlayerFromTrack(trackNum, true);
    player->CompleteCommonPhrase(false, false);
    player->UnisonHit();
}

void CommonPhraseCapturer::AllTracksCompletedPhrase(int n) {
    int tracks = TheSongDB->GetCommonPhraseTracks(n);
    tracks &= ~mDisabledTracks;
    bool b4 = (tracks != 0 && (tracks & tracks - 1));
    for (int i = 0; i < TheSongDB->GetNumTracks(); i++) {
        if (tracks & (1 << i)) {
            TheGame->GetPlayerFromTrack(i, true)->CompleteCommonPhrase(true, b4);
        }
    }
    GetTrackPanelDir()->UnisonSucceed();
    ExtendPhraseStates(n);
    mPhraseStates[n].unk0 = 1;
}

bool CommonPhraseCapturer::IsMultiplayerPhrase(int phraseID) {
    int tracks = TheSongDB->GetCommonPhraseTracks(phraseID) | mPhraseStates[phraseID].unk8;
    return (tracks & (tracks - 1)) != 0;
}

void CommonPhraseCapturer::ExtendPhraseStates(int n) {
    if (mPhraseStates.size() <= (unsigned)n) {
        PhraseState defState;
        defState.unk0 = 0;
        defState.unk4 = 0;
        defState.unk8 = 0;
        mPhraseStates.resize(n + 1, defState);
    }
}