#include "game/TambourineManager.h"
#include "bandobj/BandTrack.h"
#include "bandobj/TrackPanelDirBase.h"
#include "bandtrack/TrackPanel.h"
#include "bandtrack/VocalTrack.h"
#include "game/BandUser.h"
#include "game/Scoring.h"
#include "game/VocalPart.h"
#include "game/VocalPlayer.h"
#include "midi/MidiParser.h"
#include "obj/Data.h"
#include "obj/Dir.h"
#include "obj/MessageTimer.h"
#include "obj/Msg.h"
#include "obj/Object.h"
#include "obj/ObjMacros.h"
#include "os/Debug.h"
#include "os/System.h"
#include "synth/Sequence.h"
#include "utl/Messages4.h"
#include "utl/Symbols.h"
#include "utl/Symbols4.h"

TambourineManager::TambourineManager(VocalPlayer &p)
    : mPlayerRef(p), mIsLocal(p.IsLocal()), mTambourineSequence(0),
      mTambourineFader(Hmx::Object::New<Fader>()), mTambourineParser(0),
      mTambourineIdx(0), unk48(1), unk4c(0), mTambourineActive(0), unk60(0), unk68(0),
      unk74(0), unk78(0), unk7c(0) {
    DataArray *cfg = SystemConfig("scoring", "vocals");
    int diff = mPlayerRef.GetUser()->GetDifficulty();
    mTambourineWindowTicks = cfg->FindInt("tambourine_window_ticks");
    mTambourineCrowdSuccess = cfg->FindArray("tambourine_crowd_success")->Float(diff + 1);
    mTambourineCrowdFailure = cfg->FindArray("tambourine_crowd_failure")->Float(diff + 1);
    mTambourinePoints = cfg->FindFloat("tambourine_points");
}

TambourineManager::~TambourineManager() {
    RELEASE(mTambourineFader);
    RELEASE(mTambourineSequence);
    mBank = nullptr;
}

void TambourineManager::PostLoad() {
    mTambourineParser = ObjectDir::sMainDir->Find<MidiParser>("tambourine", true);
    mTambourineParser->AddSink(this);
    ComputeTambourinePoints();
    mTambourineFader->DoFade(-96.0f, 0);
}

void TambourineManager::PostDynamicAdd() { Restart(); }

void TambourineManager::Start() { mTambourineActive = true; }

void TambourineManager::Restart() {
    unk4c = 0;
    mTambourineIdx = 0;
    unk60 = 0;
    unk68 = 0;
    mPlayerRef.PopupHelp(tambourine, false);
    unk48 = true;
    mGemStates.clear();
    mGemStates.resize(mPlayerRef.mVocalParts[0]->mVocalNoteList->mTambourineGems.size());
}

void TambourineManager::Jump(float) {
    unk4c = 0;
    mTambourineIdx = 0;
    unk60 = 0;
    unk68 = 0;
    mPlayerRef.PopupHelp(tambourine, false);
    unk48 = true;
}

const std::vector<int> &TambourineManager::TambourineGems() const {
    return mPlayerRef.mVocalParts[0]->mVocalNoteList->mTambourineGems;
}

bool TambourineManager::IsTambourineButton(JoypadButton btn) const { return btn == kPad_X; }

void TambourineManager::HandleButtonDown() {}

bool TambourineManager::GemHit(int index) const {
    if ((unsigned int)index >= mGemStates.size())
        return false;
    return (mGemStates[index] & 1) != 0;
}

bool TambourineManager::GemProcessed(int index) const {
    if ((unsigned int)index >= mGemStates.size())
        return false;
    return (mGemStates[index] >> 2) & 1;
}

void TambourineManager::LocalTambourineSoloEnd(int pct, int numGems) {
    int points = 0;
    Symbol awardSym;
    Symbol trackSym = tambourine;
    TheScoring->GetSoloAward(pct, trackSym, points, awardSym);
    int total = points * numGems;
    mPlayerRef.AddBonusPoints(total);
    mPlayerRef.AddTambourinePointsStat((float)total);
    BandTrack *track = mPlayerRef.GetBandTrack();
    if (track) {
        Symbol awardSymCopy = awardSym;
        GetTrackPanelDir()->SoloEnd(track, total, awardSymCopy);
    }
}

void TambourineManager::SetTambourine(bool iIsActive) {
    if ((unk60 > 0) == iIsActive)
        return;

    if (iIsActive) {
        const VocalPhrase *const &thisPhrase = mPlayerRef.CurrentPhrase();
        MILO_ASSERT(thisPhrase->mTambourinePhrase, 0x141);
        MILO_ASSERT(mTambourineIdx != TambourineGems().size(), 0x142);
        int remaining =
            mPlayerRef.mVocalParts.front()->CalculateRemainingTambourineTicks();
        int start = thisPhrase->unk8;
        int end = start + remaining;
        while (TambourineGems()[mTambourineIdx] < start) {
            mTambourineIdx++;
            MILO_ASSERT(mTambourineIdx != TambourineGems().size(), 0x14e);
        }
        int i = mTambourineIdx;
        while ((unsigned int)i < TambourineGems().size()) {
            int tick = TambourineGems()[i];
            MILO_ASSERT(tick >= start, 0x155);
            if (tick >= end)
                break;
            i++;
        }
        unk60 = i - mTambourineIdx;
        if (mPlayerRef.ScoringEnabled()) {
            unk68 = 0;
        }
    } else {
        mPlayerRef.PopupHelp(tambourine, false);
        if (mIsLocal && unk60 > 0 && mPlayerRef.ScoringEnabled()) {
            int pct = (int)((float)unk68 * 100.0f / (float)unk60);
            LocalTambourineSoloEnd(pct, unk60);
            static Message send_solo_end("send_solo_end", 0, 0);
            send_solo_end[0] = pct;
            send_solo_end[1] = unk68;
            mPlayerRef.Handle(send_solo_end, true);
            mPlayerRef.EndTambourineSection(pct);
        }
        unk60 = 0;
    }

    if (iIsActive == false || unk60 != 0) {
        BandTrack *track = mPlayerRef.GetBandTrack();
        if (track) {
            track->SetTambourine(iIsActive);
            if (iIsActive && mPlayerRef.ScoringEnabled()) {
                track->SoloStart();
            }
        }
    }
}

void TambourineManager::TambourineSucceed(int index) {
    if (GemProcessed(index))
        return;
    MILO_ASSERT(mIsLocal, 0x244);
    mTambourineFader->DoFade(0.0f, 0.0f);
    if (mPlayerRef.mTrack) {
        mPlayerRef.mTrack->HitTambourineGem(index);
    }
    unk48 = true;
    MILO_ASSERT((unsigned int)index < mGemStates.size(), 0x251);
    mGemStates[index] |= 1;
    mPlayerRef.AddPoints(mTambourinePoints, true, false);
    mPlayerRef.AddTambourinePointsStat(mTambourinePoints);
    unk68++;
    int pct = (int)((float)unk68 * 100.0f / (float)unk60);
    BandTrack *track = mPlayerRef.GetBandTrack();
    if (track) {
        track->SoloHit(pct);
    }
    static Message msg("send_tambourine_succeeding", 1, 0);
    msg[1] = pct;
    mPlayerRef.HandleType(msg);
    mPlayerRef.Handle(tambourine_hit_msg, false);
    if (unk4c >= 8) {
        mPlayerRef.PopupHelp(tambourine, false);
    }
    unk4c = 0;
    mPlayerRef.AddTambourineSeen();
    mPlayerRef.AddTambourineHit();
}

void TambourineManager::TambourineFail(int index, bool swing) {
    if (GemProcessed(index))
        return;
    MILO_ASSERT(mIsLocal, 0x27b);
    mTambourineFader->DoFade(-96.0f, 0.0f);
    if (index != -1) {
        if (mPlayerRef.mTrack) {
            mPlayerRef.mTrack->MissTambourineGem(index, swing);
        }
        MILO_ASSERT((unsigned int)index < mGemStates.size(), 0x285);
        mGemStates[index] |= 2;
        mPlayerRef.AddTambourineSeen();
    }
    unk48 = false;
    static Message msg("send_tambourine_succeeding", 0, 0);
    int pct = (int)((float)unk68 * 100.0f / (float)unk60);
    msg[1] = pct;
    mPlayerRef.HandleType(msg);
    Handle(tambourine_miss_msg, false);
}

DataNode TambourineManager::OnPlayTambourine(DataArray *d) {
    if ((int)mPlayerRef.mVocalParts.size() > 1) {
        return 0;
    }
    Symbol sym = d->Sym(2);
    Symbol seq_name;
    if (sym == "tambourine_gem") {
        seq_name = Symbol("percussion.cue");
    } else if (sym == "tambourine_implicit") {
        seq_name = Symbol("percussion1.cue");
    }
    MILO_ASSERT(!seq_name.Null(), 0x1ef);
    RELEASE(mTambourineSequence);
    Sequence *seq = mBank->Find<Sequence>(seq_name.Str(), false);
    MILO_ASSERT(seq, 0x1f5);
    mTambourineSequence = dynamic_cast<Sequence *>(
        Hmx::Object::NewObject(seq->ClassName())
    );
    mTambourineSequence->Copy(seq, Hmx::Object::kCopyShallow);
    mTambourineSequence->mFaders.Add(mTambourineFader);
    mTambourineSequence->Play(0, 0, 0);
    return 0;
}

void TambourineManager::OnRemoteTambourineSucceeding(DataArray *msg) {
    int succeeding = msg->Int(2);
    int pct = msg->Int(3);
    mTambourineActive = succeeding != 0;
    if (succeeding) {
        BandTrack *track = mPlayerRef.GetBandTrack();
        if (track) {
            track->SoloHit(pct);
        }
    }
}

BEGIN_HANDLERS(TambourineManager)
    HANDLE_ACTION(remote_solo_end, LocalTambourineSoloEnd(_msg->Int(2), _msg->Int(3)))
    HANDLE_ACTION(remote_tambourine_succeeding, OnRemoteTambourineSucceeding(_msg))
    HANDLE(play_tambourine, OnPlayTambourine)
    HANDLE_CHECK(0x2f2)
END_HANDLERS
