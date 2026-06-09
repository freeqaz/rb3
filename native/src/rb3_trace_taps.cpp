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
#include "beatmatch/BeatMaster.h"  // BeatMaster::GetAudio()
#include "beatmatch/MasterAudio.h" // MasterAudio::GetTime()

#include "meta_band/BandUI.h"      // BandUI TheBandUI (the real TheUI; a MsgSource)
#include "ui/UIScreen.h"           // UIScreen, UIScreenChangeMsg, FocusPanel()
#include "ui/UIPanel.h"            // UIPanel::FocusComponent()
#include "ui/UIComponent.h"        // UIComponent::Name()
#include "obj/Object.h"            // Hmx::Object
#include "obj/Data.h"              // DataArray, DataNode, kDataUnhandled
#include "obj/Msg.h"               // MsgSource::AddSink

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
