// rb3_trace_taps.cpp — engine-side TAPS for the session-telemetry recorder
// (HX_NATIVE only). The recorder CORE + public Record* API live in
// rb3_session_trace.{h,cpp} (owned separately); this TU wires the engine seams
// the recorder can't reach from inside its own POD/ring:
//
//   1. RB3TraceCurrentSongMs() — the null-guarded
//      TheGame->GetBeatMaster()->GetAudio()->GetTime() chain (mirrors
//      rb3_http_handlers.cpp:437), returning -1 in menus. The frame tap calls
//      RB3TraceSetSongMs(RB3TraceCurrentSongMs()) so every event's envelope `sm`
//      is correct.
//
//   2. The NAV sink — a tiny Hmx::Object that subscribes to TheBandUI's
//      "screen_change" export (the UIScreenChangeMsg fired once per transition at
//      UI.cpp:683, the canonical nav event — see SESSION_TELEMETRY_DESIGN.md
//      §6.1 / D4). On each transition it records nav{from,to,focus,wentBack}.
//      RB3TraceEnsureNavSink() registers it once, idempotently, after TheBandUI
//      exists; it is safe to call every frame (the frame tap does, as a backstop)
//      and from the native main right after the App ctor.
//
// The frame / input / boot / log taps themselves are inline edits at their call
// sites (App::RunOneFrame, JoypadPoll, BootMark, Debug::Print) — this TU only
// owns the nav sink + the song-ms helper, which need engine types those sites
// already include but which the recorder core deliberately does not pull in.

#ifdef HX_NATIVE

#include "rb3_session_trace.h"

#include "game/Game.h"             // Game *TheGame, Game::GetBeatMaster()
#include "game/Player.h"           // Player (Performer subclass) GetScore/GetPercentComplete
#include "game/Performer.h"        // Performer::GetScore/GetPercentComplete
#include "beatmatch/BeatMaster.h"  // BeatMaster::GetAudio()
#include "beatmatch/MasterAudio.h" // MasterAudio::GetTime()

#include "ui/UI.h"                 // UIManager &TheUI, CurrentScreen()
#include "meta_band/BandUI.h"      // BandUI TheBandUI (the real TheUI; a MsgSource)
#include "ui/UIScreen.h"           // UIScreen, UIScreenChangeMsg, FocusPanel()
#include "ui/UIPanel.h"            // UIPanel::FocusComponent()
#include "ui/UIComponent.h"        // UIComponent::Name()
#include "obj/Object.h"            // Hmx::Object
#include "obj/Task.h"              // TaskMgr TheTaskMgr, Seconds(kRealTime)/Beat()
#include "obj/Data.h"              // DataArray, DataNode, kDataUnhandled
#include "obj/Msg.h"               // MsgSource::AddSink

#include <cstdlib>                 // getenv, atoi
#include <vector>

// ---------------------------------------------------------------------------
// Song ms. Walk TheGame->GetBeatMaster()->GetAudio()->GetTime() with explicit
// null guards (mMaster/mAudio are NULL in the menus, where GetTime would deref
// NULL). Returns -1 until the master audio is live, which the recorder treats as
// "not in a song" and omits `sm` from the envelope (D2 §4.5). Identical chain to
// rb3_http_handlers.cpp:437-442.
// ---------------------------------------------------------------------------
float RB3TraceCurrentSongMs() {
    if (!TheGame)
        return -1.0f;
    BeatMaster *bm = TheGame->GetBeatMaster();
    MasterAudio *ma = bm ? bm->GetAudio() : nullptr;
    if (!ma)
        return -1.0f;
    return ma->GetTime();
}

// ---------------------------------------------------------------------------
// CHECKPOINT sampler (M4, Tier-2 replay verification).
//
// Samples the replay-divergence state vector — screen + focus, the TaskMgr
// real-time clock + beat, the in-song clock, and every active player's exact
// score + percent-complete — under FULL null guards (this runs boot-through-
// gameplay, where TheGame / GetBand / players are NULL in menus), then hands the
// pulled scalars to RB3RecordCheckpoint, which computes the FNV-1a fast-equality
// hash and emits the additive `chk` event. Same safe sampling site as
// /api/health (rb3_http_handlers.cpp:427-445).
//
// Called every RB3_TRACE_CHK_EVERY frames WHILE IN A SONG from the per-frame
// hook (RB3TraceCheckpointFrame below) + once on each nav transition (the nav
// sink, force=true). In menus the periodic path is skipped (a menu has no song
// clock to diverge); nav-transition checkpoints still fire there as the menu
// milestone anchors trace-diff aligns on.
// ---------------------------------------------------------------------------
void RB3TraceSampleCheckpoint() {
    if (!gRB3TraceActive)
        return;

    // Screen + focus (the same chain the nav sink / main_web.cpp publishes).
    UIScreen *scr = TheUI.CurrentScreen();
    const char *scrName = (scr && scr->Name()) ? scr->Name() : "";
    const char *focus = "";
    if (scr && scr->FocusPanel() && scr->FocusPanel()->FocusComponent() &&
        scr->FocusPanel()->FocusComponent()->Name()) {
        focus = scr->FocusPanel()->FocusComponent()->Name();
    }

    // Sim clocks. TheTaskMgr is a global; Seconds(kRealTime)/Beat() are always
    // safe to read (they read the timeline members, no song dependency).
    float taskSec = TheTaskMgr.Seconds(TaskMgr::kRealTime);
    float beat    = TheTaskMgr.Beat();

    // In-song clock (-1 in menus -> RB3RecordCheckpoint omits the envelope sm).
    float songMs = RB3TraceCurrentSongMs();

    // Per-player exact scores + percent-complete. NULL/empty in menus.
    long scoreSum = 0;
    int  scores[RB3_CHK_MAX_SCORES];
    int  nScores = 0;
    int  nPlayers = 0;
    int  pct = -1;
    if (TheGame) {
        std::vector<Player *> &players = TheGame->GetActivePlayers();
        nPlayers = (int)players.size();
        for (int i = 0; i < nPlayers; ++i) {
            Player *p = players[i];
            if (!p)
                continue;
            int s = p->GetScore();
            scoreSum += s;
            if (nScores < RB3_CHK_MAX_SCORES)
                scores[nScores++] = s;
            if (i == 0)
                pct = p->GetPercentComplete();
        }
    }

    RB3RecordCheckpoint(scrName, focus, taskSec, beat, songMs, scoreSum,
                        scores, nScores, nPlayers, pct);
}

// Per-frame entry: emit a periodic checkpoint every N in-song frames. N is
// RB3_TRACE_CHK_EVERY (default 30; <=0 disables the periodic path, nav-transition
// chk still fires). Skipped entirely in menus (no song clock to diverge); the
// nav-transition checkpoints anchor the menu milestones. Cheap no-op when off.
namespace {
int sChkEvery = -1;   // parsed once: -1 unchecked, then the resolved cadence
}
void RB3TraceCheckpointFrame(int frame) {
    if (!gRB3TraceActive)
        return;
    if (sChkEvery < 0) {
        const char *v = std::getenv("RB3_TRACE_CHK_EVERY");
        sChkEvery = (v && v[0]) ? std::atoi(v) : 30;
        if (sChkEvery < 0) sChkEvery = 0;
    }
    if (sChkEvery == 0)
        return;   // periodic path disabled
    // In-song only: no master audio clock => no periodic checkpoint (the nav
    // sink still emits one per transition for the menu milestones).
    if (RB3TraceCurrentSongMs() < 0.0f)
        return;
    if ((frame % sChkEvery) != 0)
        return;
    RB3TraceSampleCheckpoint();
}

// ---------------------------------------------------------------------------
// NAV sink.
//
// TheBandUI (== TheUI, see BandUI.cpp:45 `UIManager &TheUI = TheBandUI;`) is a
// `BandUI : public UIManager, public MsgSource`. UIManager::GotoScreenImpl fires
// `UIScreenChangeMsg(newScr, mCurrentScreen, mWentBack)` via Handle(msg,false)
// (UI.cpp:682-683) once per transition → BandUI::OnMsg(UIScreenChangeMsg) does
// `Export(msg, true)`, fanning it out to every MsgSource sink subscribed to
// "screen_change". So a sink registered with
// `TheBandUI.AddSink(this, UIScreenChangeMsg::Type())` (exactly like
// Game.cpp:193) receives the exported msg as a Handle().
//
// In our Handle(), the DataArray layout matches UIScreenChangeMsg's accessors:
//   Node(1) = type symbol ("screen_change")
//   Node(2) = new screen  (Obj<UIScreen>)  → `to`
//   Node(3) = old screen  (Obj<UIScreen>)  → `from`
//   Node(4) = wentBack    (Int)            → `wb`
// (UIScreen.h:90-94.)
// ---------------------------------------------------------------------------
namespace {

class RB3TraceNavSink : public Hmx::Object {
public:
    RB3TraceNavSink() {}
    virtual ~RB3TraceNavSink() {}

    virtual DataNode Handle(DataArray *msg, bool warn) {
        // Only react to the screen-change export; ignore anything else routed here.
        if (msg && msg->Size() >= 4 && msg->Sym(1) == UIScreenChangeMsg::Type()) {
            UIScreen *toScr   = msg->Obj<UIScreen>(2);
            UIScreen *fromScr = msg->Obj<UIScreen>(3);
            bool wentBack     = msg->Int(4) != 0;

            const char *from = (fromScr && fromScr->Name()) ? fromScr->Name() : "";
            const char *to   = (toScr && toScr->Name()) ? toScr->Name() : "";

            // Focus = the component a Confirm would act on, lifted from the NEW
            // screen's focus chain (the same chain main_web.cpp:291 publishes).
            const char *focus = "";
            if (toScr && toScr->FocusPanel() &&
                toScr->FocusPanel()->FocusComponent() &&
                toScr->FocusPanel()->FocusComponent()->Name()) {
                focus = toScr->FocusPanel()->FocusComponent()->Name();
            }

            RB3RecordNav(from, to, focus, wentBack);
            // One checkpoint per nav transition (the milestone anchor trace-diff
            // aligns segments on). Fires in menus too (where the periodic in-song
            // path is skipped), so every milestone boundary has a chk.
            RB3TraceSampleCheckpoint();
        }
        return DataNode(kDataUnhandled, 0);
    }
};

RB3TraceNavSink *sNavSink = nullptr;
bool sNavSinkRegistered = false;

} // namespace

// Register the nav sink exactly once, after TheBandUI is alive. Idempotent +
// cheap to call every frame (the frame tap calls it as a backstop, and the
// native main calls it right after the App ctor so the earliest transitions are
// captured). No-op when tracing is off so we don't allocate/subscribe needlessly.
void RB3TraceEnsureNavSink() {
    if (sNavSinkRegistered)
        return;
    if (!gRB3TraceActive)
        return;   // tracing not armed — don't subscribe (RB3RecordNav would no-op)
    sNavSink = new RB3TraceNavSink();
    TheBandUI.AddSink(sNavSink, UIScreenChangeMsg::Type(), Symbol(),
                      MsgSource::kHandle);
    sNavSinkRegistered = true;
}

#endif // HX_NATIVE
