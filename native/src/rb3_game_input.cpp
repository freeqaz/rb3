// rb3 native — headless synthetic-input driver + screen-flow trace.
//
// RB3 has no Joypad_Native linked, so the real button path (Joypad ->
// JoypadClient -> UIManager sink -> focus screen/panel/component) never gets a
// physical button. This file injects synthetic ButtonDownMsg's into TheUI at
// scripted frames, exactly the way Automator::Poll does (UI.cpp:204-212):
//
//     static ButtonDownMsg b_msg(user, button, action, padnum);
//     TheUI.Handle(b_msg, false);
//
// which routes UIManager -> mCurrentScreen (HANDLE_MEMBER_PTR) -> FocusPanel()
// -> focus UIComponent. A focused UIButton turns a kAction_Confirm into a
// SendSelect -> UIComponentSelectMsg ("component_select" = SELECT_MSG), which is
// what the splash/main_hub/song-select panel DTAs gate their state machines on.
//
// Driven by RB3_GAME_INPUT="@30:start,@90:confirm,@120:down,@150:confirm"
// (frame:action pairs; actions: start/confirm/cancel/up/down/left/right/option).
//
// W3b: real browser keyboard input added under #ifdef __EMSCRIPTEN__. JS
// keydown/keyup listeners maintain window._rb3Keys bitmask; RB3GameInputPoll
// drains it per-frame (edge-detect) and calls ExecButton() — the same path the
// synthetic script + HTTP /api/input use. Zero native / Wii-asm impact.
//
// Native-only glue — no DTA edits, no matched-fork source edits.

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#include <emscripten/em_asm.h>
#endif

#include "ui/UI.h"
#include "ui/UIScreen.h"
#include "ui/UIPanel.h"
#include "ui/UIComponent.h"
#include "ui/UILabel.h"   // UILabel — N6 seldiff raw-token clear
#include "rndobj/Text.h"  // RndText::RawText — detect the raw %S %I SONGS leak
#include "meta_band/BandSongMgr.h"        // TheSongMgr (N6 song+artist text)
#include "meta_band/BandSongMetadata.h"   // BandSongMetadata Title/Artist (N6)
#include "meta_band/MusicLibrary.h"       // TheMusicLibrary::TryToSetHighlight (W3c)
#include "meta_band/SongSortNode.h"       // kNodeSong (W3c)
#include "utl/Locale.h"                   // Localize (N6 song_artist_fmt)
#include "utl/MakeString.h"               // MakeString (N6)
#include "os/Joypad.h"
#include "os/JoypadMsgs.h"
#include "os/User.h"
#include "game/BandUser.h"           // BandUser::SetTrackType
#include "game/BandUserMgr.h"
#include "game/Game.h"               // TheGame, Game::GetActivePlayers (autohit)
#include "game/Player.h"             // Player::SetAutoplay/IsAutoplay (autohit)
#include "game/Defines.h"
#include "beatmatch/TrackType.h"     // SymToTrackType
#include "meta_band/ProfileMgr.h"
#include "meta_band/MetaPerformer.h"   // SetBandNoFail (nofail directive)
#include "os/ContentMgr.h"
#include "os/System.h"
#include "obj/Object.h"
#include "obj/Data.h"
#include "obj/Dir.h"
#include "utl/Symbol.h"
#include "rndobj/Draw.h"    // RndDrawable — N4 song-select details-pane hide

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cctype>
#include <string>
#include <vector>
#include <mutex>
#include <algorithm>

namespace {

// ============================================================================
// W3b — Browser keyboard input (web build only)
// ============================================================================
// JS keydown/keyup listeners on document maintain window._rb3Keys as a bitmask
// matching the JoypadButton enum (same bit positions as DC3's _dc3Keys). Read
// once per frame in RB3GameInputPoll; edge-detected presses call ExecButton()
// exactly as the synthetic script + HTTP paths do. Zero native impact.
//
// Bit positions (matching JoypadButton enum in os/Joypad.h):
//   kPad_L2=0  kPad_R2=1  kPad_L1=2  kPad_R1=3
//   kPad_Tri=4 kPad_Circle=5 kPad_X=6 kPad_Square=7
//   kPad_Select=8 kPad_L3=9 kPad_R3=10 kPad_Start=11
//   kPad_DUp=12 kPad_DRight=13 kPad_DDown=14 kPad_DLeft=15
#ifdef __EMSCRIPTEN__

static bool sWebInputInitialized = false;
static unsigned int sWebPrevButtons = 0;

// Map key → action/button for ExecButton. Returns kAction_None if unmapped.
// Mirrors ActionFromName() but keyed on JoypadButton bit positions for the
// bitmask approach; individual button→action pairs match the synthetic driver.
struct WebKeyMapping {
    int bit;           // bit index in the _rb3Keys bitmask (= JoypadButton value)
    JoypadButton btn;
    JoypadAction action;
};

static const WebKeyMapping kWebKeyMap[] = {
    { 6,  kPad_X,      kAction_Confirm  },  // Enter   → X / Confirm
    { 11, kPad_Start,  kAction_Start    },  // Space   → Start
    { 5,  kPad_Circle, kAction_Cancel   },  // Escape / Backspace → Circle / Cancel
    { 8,  kPad_Select, kAction_Option   },  // Tab     → Select / Option
    { 12, kPad_DUp,    kAction_Up       },  // ArrowUp / W
    { 14, kPad_DDown,  kAction_Down     },  // ArrowDown / S
    { 15, kPad_DLeft,  kAction_Left     },  // ArrowLeft / A
    { 13, kPad_DRight, kAction_Right    },  // ArrowRight / D
    { 2,  kPad_L1,     kAction_PageUp   },  // Q → L1 / PageUp
    { 3,  kPad_R1,     kAction_PageDown },  // E → R1 / PageDown
};
static const int kWebKeyMapSize = (int)(sizeof(kWebKeyMap) / sizeof(kWebKeyMap[0]));

// Read the current JS key bitmask (set by keydown/keyup listeners).
static unsigned int GetWebKeyBitmask() {
    return (unsigned int)EM_ASM_INT({ return window._rb3Keys || 0; });
}

// Install JS keydown/keyup listeners on document. Called once on first poll.
static void InitWebInput() {
    if (sWebInputInitialized) return;
    sWebInputInitialized = true;

    // Build window._rb3Keys as a bitmask matching JoypadButton enum bits.
    // EM_ASM JS blocks must not contain C-style comments or unescaped braces
    // in object literals — use bracket assignment instead.
    EM_ASM({
        window._rb3Keys = 0;
        var m = new Object();
        // Arrows + WASD → d-pad
        m['ArrowUp']    = 1<<12;  // kPad_DUp
        m['ArrowDown']  = 1<<14;  // kPad_DDown
        m['ArrowLeft']  = 1<<15;  // kPad_DLeft
        m['ArrowRight'] = 1<<13;  // kPad_DRight
        m['w'] = 1<<12;
        m['W'] = 1<<12;
        m['s'] = 1<<14;
        m['S'] = 1<<14;
        m['a'] = 1<<15;
        m['A'] = 1<<15;
        m['d'] = 1<<13;
        m['D'] = 1<<13;
        // Face / menu buttons
        m['Enter']     = 1<<6;   // kPad_X → kAction_Confirm
        m['Escape']    = 1<<5;   // kPad_Circle → kAction_Cancel
        m['Backspace'] = 1<<5;   // kPad_Circle → kAction_Cancel
        m[' ']         = 1<<11;  // kPad_Start → kAction_Start
        m['Tab']       = 1<<8;   // kPad_Select → kAction_Option
        m['q'] = 1<<2;           // kPad_L1 → kAction_PageUp
        m['Q'] = 1<<2;
        m['e'] = 1<<3;           // kPad_R1 → kAction_PageDown
        m['E'] = 1<<3;
        // Keys to consume (prevent browser scroll / tab-switch / etc.)
        var consume = new Object();
        consume['ArrowUp']    = 1;
        consume['ArrowDown']  = 1;
        consume['ArrowLeft']  = 1;
        consume['ArrowRight'] = 1;
        consume[' ']          = 1;
        consume['Tab']        = 1;
        consume['Escape']     = 1;
        consume['Backspace']  = 1;
        document.addEventListener('keydown', function(e) {
            var bit = m[e.key];
            if (bit) {
                window._rb3Keys |= bit;
                if (consume[e.key]) e.preventDefault();
            }
            // AudioContext resume on first user gesture (W3c will hook the real impl).
            if (window._rb3AudioCtx && window._rb3AudioCtx.state === 'suspended') {
                window._rb3AudioCtx.resume();
            }
        }, true);
        document.addEventListener('keyup', function(e) {
            var bit = m[e.key];
            if (bit) {
                window._rb3Keys &= ~bit;
            }
        }, true);
        console.log('RB3 Web: keyboard input ready (W3b)');
    });

    printf("RB3 Web: keyboard input initialized (W3b)\n");
}

#endif // __EMSCRIPTEN__

struct ScriptedInput {
    int frame;
    JoypadAction action;
    JoypadButton button;
};

// A "select:<name>" directive: find a UIComponent by name in the current screen's
// focus panel (or anywhere in the current screen) and SendSelect — the exact real
// flow a focused UIButton runs on Confirm (UIComponent::SendSelect ->
// UIComponentSelectMsg = the DTA SELECT_MSG/component_select). Lets a script drive
// a specific hub/menu button without depending on the milo's d-pad nav graph.
struct ScriptedSelect {
    int frame;
    std::string button;
};

// A "msg:<object>:<action>[:arg]..." directive: send a DTA message to a named
// ObjectDir::Main() object — the exact handler a DTA SELECT_MSG case invokes
// (e.g. {music_library select_highlighted_node $user}). With no extra args the
// single arg is $synthUser (the song-confirm case). Extra colon-separated args
// are appended literally: an integer literal, a `$user` placeholder (the synth
// user object), or a bare symbol name. e.g.
//   msg:overshell:end_override_flow:1:0   -> {overshell end_override_flow kOverrideFlow_SongSettings FALSE}
struct ScriptedMsg {
    int frame;
    std::string object;
    std::string action;
    std::vector<std::string> args; // empty => default {action $user}
};

// K8: a "track:<sym>" directive sets the synth user's track type at the given
// frame. Mirrors what OvershellSlot::SelectPart does on the real flow — without
// it, Band::Band sees mTrackType=kTrackNone (sym=`none`) on every participating
// user, so MetaPerformer::PartPlaysInSong(none)=false → every user is SKIPPED
// → mActivePlayers stays empty → no gems, no scoring, no highway notes.
// Sym is one of: guitar/bass/drum/vocals/keys/real_guitar/real_bass/real_keys.
struct ScriptedTrack {
    int frame;
    std::string trackSym;
};

// A "nofail" directive enables band No-Fail at the given frame. Without it, a
// headless demo run (no synthetic note input) drains the crowd meter and the
// player gets booed off (~song 13s) — Player::CheckCrowdFailure ->
// SetEnabledState(kPlayerDisabled) -> BandTrack::DisablePlayer ->
// GemManager::SetGemsEnabled(-1), which makes GemManager::GetTypeForGem return
// `invisible` for every gem from that point on, so the highway goes empty for
// the rest of the song. No-Fail (MetaPerformer::SetBandNoFail) is the proper
// retail switch that gates CheckCrowdFailure, keeping gems flowing.
struct ScriptedNoFail {
    int frame;
};

std::vector<ScriptedInput> gScript;
std::vector<ScriptedSelect> gSelectScript;
std::vector<ScriptedMsg> gMsgScript;
std::vector<ScriptedTrack> gTrackScript;
std::vector<ScriptedNoFail> gNoFailScript;
bool gScriptParsed = false;
Symbol gLastScreen;
LocalUser *gSynthUser = nullptr;

// === State-driven verb queue ==============================================
// BOOT RELIABILITY: the original dispatcher fired each verb on an EXACT frame
// match (frame == @N). On a slow/contended host the targeted screen/object can
// still be loading at frame N, so the verb fired against a not-yet-existent or
// mid-transition screen and dereferenced null/garbage -> SIGSEGV in the menu
// before gameplay. (Agents worked around it by running under gdb, whose
// slowdown let loading win the race.)
//
// We now treat the whole script as ONE ORDERED QUEUE and dispatch it
// SEQUENTIALLY + READINESS-GATED: a verb fires only once (a) frame >= its @N
// (now a MINIMUM frame / ordering hint, not an exact trigger), (b) all earlier
// verbs have already fired, and (c) its readiness predicate holds — for the
// common case, the UI has a stable current screen (CurrentScreen() != null &&
// !InTransition()), and for object/component-targeted verbs the target
// actually resolves. If the predicate is not yet met the verb WAITS and is
// retried next frame instead of firing blind. A per-verb deadline
// (kVerbTimeoutFrames past its @N) skips a verb whose target never appears, so
// a mis-timed/bad script degrades gracefully (LOG + SKIP) rather than hanging
// or crashing. The documented RB3_GAME_INPUT scripts still drive
// boot->song-select->load->gameplay->nofail, just robustly to timing.
enum VerbKind { kVerbButton, kVerbSelect, kVerbMsg, kVerbTrack, kVerbNoFail, kVerbAutohit };

struct Verb {
    int        kind;
    int        minFrame;   // @N — earliest frame this verb may fire (ordering hint)
    int        origIndex;  // stable tiebreak so equal-@N verbs keep script order
    // Payload (only the field matching `kind` is meaningful):
    JoypadAction action = kAction_None;
    JoypadButton button = kPad_NumButtons;
    ScriptedSelect sel{0, ""};
    ScriptedMsg    msg;
    ScriptedTrack  trk{0, ""};
};

std::vector<Verb> gVerbs;     // sorted by (minFrame, origIndex)
size_t gVerbCursor = 0;       // next un-fired verb
int    gVerbWaitSince = -1;   // frame at which the cursor verb became eligible (>=minFrame)
int    gLastFiredFrame = -1;  // frame the previous verb was dispatched (button-settle guard)

// How many frames past a verb's eligibility window we keep retrying a not-ready
// target before giving up and skipping it. Generous: screen loads on a cold
// host can take a few hundred frames. Tunable via RB3_INPUT_VERB_TIMEOUT.
int VerbTimeoutFrames() {
    const char *e = getenv("RB3_INPUT_VERB_TIMEOUT");
    int v = e ? atoi(e) : 3000;
    return v > 0 ? v : 3000;
}

// HTTP-injected verbs (from rb3_http_server.cpp's /api/input). The HTTP handler
// thread enqueues a raw verb string ("start", "confirm", "select:foo.btn",
// "msg:obj:action[:arg]...", "track:guitar", "up"/"down"/...); the main-thread
// RB3GameInputPoll drains + executes them frame-agnostically (they fire on the
// next frame after injection, exactly like a scripted directive whose frame
// matched). Reuses the same execution paths as the RB3_GAME_INPUT script.
std::mutex gInjectMutex;
std::vector<std::string> gPendingInject;

JoypadAction ActionFromName(const std::string &name, JoypadButton &btnOut) {
    // Default the raw button to the nav d-pad equivalent so list nav (UIList)
    // and OverloadHorizontalNav see a consistent button<->action pairing.
    btnOut = kPad_NumButtons;
    if (name == "confirm") { btnOut = kPad_X;      return kAction_Confirm; }
    if (name == "start")   { btnOut = kPad_Start;  return kAction_Start; }
    if (name == "cancel" || name == "back")
                           { btnOut = kPad_Circle; return kAction_Cancel; }
    if (name == "option")  { btnOut = kPad_Tri;    return kAction_Option; }
    if (name == "up")      { btnOut = kPad_DUp;   return kAction_Up; }
    if (name == "down")    { btnOut = kPad_DDown; return kAction_Down; }
    if (name == "left")    { btnOut = kPad_DLeft; return kAction_Left; }
    if (name == "right")   { btnOut = kPad_DRight;return kAction_Right; }
    return kAction_None;
}

void ParseScript() {
    gScriptParsed = true;
    const char *spec = getenv("RB3_GAME_INPUT");
    if (!spec || !*spec)
        return;
    // Tokens are comma-separated "@<frame>:<action>".
    std::string s(spec);
    size_t pos = 0;
    while (pos < s.size()) {
        size_t comma = s.find(',', pos);
        std::string tok = s.substr(pos, comma == std::string::npos ? std::string::npos : comma - pos);
        pos = (comma == std::string::npos) ? s.size() : comma + 1;
        // trim spaces
        size_t a = tok.find_first_not_of(" \t");
        if (a == std::string::npos) continue;
        size_t b = tok.find_last_not_of(" \t");
        tok = tok.substr(a, b - a + 1);
        if (tok.empty() || tok[0] != '@') continue;
        size_t colon = tok.find(':');
        if (colon == std::string::npos) continue;
        int frame = atoi(tok.substr(1, colon - 1).c_str());
        std::string action = tok.substr(colon + 1);
        // "select:<button>" directive — drive the real SELECT_MSG on a named
        // UIComponent in the current screen (bypasses milo d-pad nav).
        if (action.rfind("select:", 0) == 0) {
            ScriptedSelect ss = { frame, action.substr(7) };
            gSelectScript.push_back(ss);
            Verb v; v.kind = kVerbSelect; v.minFrame = frame; v.origIndex = (int)gVerbs.size();
            v.sel = ss;
            gVerbs.push_back(v);
            MILO_LOG("RB3 input: scheduled @%d (min) -> select '%s'\n", frame, ss.button.c_str());
            continue;
        }
        // "track:<sym>" directive — set the synth user's track type at the given
        // frame so the gameplay-side Band::Band picks the user up as an active
        // player. K8.
        if (action.rfind("track:", 0) == 0) {
            ScriptedTrack st = { frame, action.substr(6) };
            gTrackScript.push_back(st);
            Verb v; v.kind = kVerbTrack; v.minFrame = frame; v.origIndex = (int)gVerbs.size();
            v.trk = st;
            gVerbs.push_back(v);
            MILO_LOG("RB3 input: scheduled @%d (min) -> track '%s'\n", frame, st.trackSym.c_str());
            continue;
        }
        // "nofail" directive — enable band No-Fail at frame F so an
        // input-less demo run keeps its gems on the highway for the whole song
        // (otherwise the player is booed off ~13s and gems go invisible).
        if (action == "nofail") {
            ScriptedNoFail nf = { frame };
            gNoFailScript.push_back(nf);
            Verb v; v.kind = kVerbNoFail; v.minFrame = frame; v.origIndex = (int)gVerbs.size();
            gVerbs.push_back(v);
            MILO_LOG("RB3 input: scheduled @%d (min) -> nofail\n", frame);
            continue;
        }
        // "autohit" directive — turn on autoplay (the retail kiosk/E3 path) for
        // every active player at frame F, so the engine auto-hits gems at the
        // strike window: SetAutoplay(true) -> BeatMatcher::SetCheating(true) ->
        // TrackWatcherImpl::CheckForAutoplay fires HitGem -> GemSmasher::Hit ->
        // hit.trig particles + score tick. Exercises the hit-FX path a passive
        // (nofail-only) run never reaches.
        if (action == "autohit") {
            Verb v; v.kind = kVerbAutohit; v.minFrame = frame; v.origIndex = (int)gVerbs.size();
            gVerbs.push_back(v);
            MILO_LOG("RB3 input: scheduled @%d (min) -> autohit\n", frame);
            continue;
        }
        // "msg:<object>:<action>[:arg]..." directive — send a message to a named
        // ObjectDir::Main() object (the real DTA-handler path).
        if (action.rfind("msg:", 0) == 0) {
            std::string rest = action.substr(4);
            // Split rest on ':' into [object, action, arg, arg, ...].
            std::vector<std::string> parts;
            size_t p = 0;
            while (p <= rest.size()) {
                size_t c = rest.find(':', p);
                if (c == std::string::npos) { parts.push_back(rest.substr(p)); break; }
                parts.push_back(rest.substr(p, c - p));
                p = c + 1;
            }
            if (parts.size() >= 2) {
                ScriptedMsg sm;
                sm.frame = frame;
                sm.object = parts[0];
                sm.action = parts[1];
                for (size_t k = 2; k < parts.size(); ++k)
                    sm.args.push_back(parts[k]);
                gMsgScript.push_back(sm);
                Verb v; v.kind = kVerbMsg; v.minFrame = frame; v.origIndex = (int)gVerbs.size();
                v.msg = sm;
                gVerbs.push_back(v);
                MILO_LOG("RB3 input: scheduled @%d (min) -> msg {%s %s} (+%d args)\n", frame,
                         sm.object.c_str(), sm.action.c_str(), (int)sm.args.size());
            }
            continue;
        }
        JoypadButton btn;
        JoypadAction act = ActionFromName(action, btn);
        if (act == kAction_None) {
            MILO_LOG("RB3 input: unknown action '%s' in RB3_GAME_INPUT\n", action.c_str());
            continue;
        }
        ScriptedInput si = { frame, act, btn };
        gScript.push_back(si);
        Verb v; v.kind = kVerbButton; v.minFrame = frame; v.origIndex = (int)gVerbs.size();
        v.action = act; v.button = btn;
        gVerbs.push_back(v);
        MILO_LOG("RB3 input: scheduled @%d (min) -> %s (action %d)\n", frame, action.c_str(), act);
    }

    // Stable sort by minFrame so the sequential dispatcher walks verbs in the
    // intended order even if the script lists them out of @N order. origIndex
    // breaks ties so equal-@N verbs keep their written order.
    std::stable_sort(gVerbs.begin(), gVerbs.end(), [](const Verb &a, const Verb &b) {
        if (a.minFrame != b.minFrame) return a.minFrame < b.minFrame;
        return a.origIndex < b.origIndex;
    });
}

LocalUser *SynthUser() {
    if (gSynthUser)
        return gSynthUser;
    // The first local BandUser (BandUserMgr(4,3) builds 4 LocalBandUsers in its
    // ctor, App.cpp:214 BandUserMgrInit). Bind it to pad 0 so the whole pad/user
    // resolution path (UserMgr::GetLocalUserFromPadNum, OvershellPanel add-user,
    // ProfileMgr set_primary_profile_by_user) sees a real, pad-associated user.
    if (TheBandUserMgr) {
        std::vector<LocalBandUser *> &locals = TheBandUserMgr->GetLocalBandUsers();
        if (!locals.empty()) {
            gSynthUser = locals[0];
            AssociateUserAndPad(gSynthUser, 0);
            // Populate the shared joypad config tables (gControllersCfg /
            // gButtonMeanings). The real JoypadInit() lives in the Wii-only
            // Joypad_Wii.cpp (excluded on native) so its config-loading half —
            // JoypadInitCommon(SystemConfig("joypad")) — never ran; without it
            // JoypadControllerTypePadNum / ShellInputInterceptor::FilterAction
            // assert on a null gControllersCfg. This is the hardware-free part
            // of JoypadInit (no UsbWii / WPAD), so it is safe natively.
            JoypadInitCommon(SystemConfig("joypad"));
            // Headless has no physical Joypad_Native. The overshell add-user /
            // slot-join flow (which gates splash -> main_hub) requires the user
            // to report a connected controller (OvershellPanel::AddJoinUserEntry
            // -> ConnectedControllerType != kControllerNone). Mark pad 0
            // connected in the joypad table (only pad 0 — using the global
            // `fake_controllers` DataVariable would make the 3 *unassociated*
            // BandUsers report connected too, then crash in
            // DebugGetControllerTypeOverride(GetPadNum()=-1)). Pair it with a
            // debug controller-type override so ConnectedControllerType()
            // yields a real instrument (guitar) for pad 0.
            JoypadData *pad0 = JoypadGetPadData(0);
            if (pad0)
                pad0->mConnected = true;
            locals[0]->DebugSetControllerTypeOverride(kControllerGuitar);
            MILO_LOG("RB3 input: bound synthetic user %p to pad 0 "
                     "(connected, controller=guitar)\n", (void *)gSynthUser);
        }
    }
    return gSynthUser;
}

// === Verb execution helpers ================================================
// Extracted from the per-frame script loops so both the RB3_GAME_INPUT script
// AND the HTTP /api/input injection drive the SAME real engine paths. Each runs
// on the main thread (script loop or RB3HttpServerPoll). `cur` is the current
// UIScreen (may be null).

void ExecMsg(const ScriptedMsg &m, UIScreen *cur) {
    (void)cur;
    LocalUser *user = SynthUser();
    Hmx::Object *obj = ObjectDir::sMainDir
        ? ObjectDir::sMainDir->FindObject(m.object.c_str(), true)
        : nullptr;
    if (!obj) {
        MILO_LOG("RB3 input: msg target '%s' NOT FOUND\n", m.object.c_str());
        return;
    }
    const std::vector<std::string> &args = m.args;
    if (args.empty()) {
        MILO_LOG("RB3 input: msg {%s %s $user}\n", m.object.c_str(), m.action.c_str());
        Message msg(Symbol(m.action.c_str()), DataNode((Hmx::Object *)user));
        obj->Handle(msg, true);
    } else {
        DataArray *da = new DataArray((int)args.size() + 2);
        da->Node(1) = Symbol(m.action.c_str());
        std::string argdump;
        for (size_t k = 0; k < args.size(); ++k) {
            const std::string &a = args[k];
            if (a == "$user")
                da->Node((int)k + 2) = DataNode((Hmx::Object *)user);
            else if (!a.empty() &&
                     (isdigit((unsigned char)a[0]) || (a[0] == '-' && a.size() > 1)))
                da->Node((int)k + 2) = DataNode(atoi(a.c_str()));
            else
                da->Node((int)k + 2) = DataNode(Symbol(a.c_str()));
            argdump += " " + a;
        }
        MILO_LOG("RB3 input: msg {%s %s%s}\n", m.object.c_str(), m.action.c_str(),
                 argdump.c_str());
        obj->Handle(da, true);
        da->Release();
    }
}

// Resolve a named UIComponent in the current screen (focus panel first, then
// every panel ref). Shared by ExecSelect AND the readiness predicate, so "is
// this select target loaded yet?" uses the exact same lookup that will run it.
// Returns the component (and its owning panel via *ownerOut) or null.
UIComponent *FindSelectComponent(const std::string &button, UIScreen *cur,
                                 UIPanel **ownerOut) {
    UIComponent *comp = nullptr;
    UIPanel *ownerPanel = nullptr;
    if (cur) {
        UIPanel *fp = cur->FocusPanel();
        if (fp && fp->LoadedDir()) {
            comp = fp->LoadedDir()->Find<UIComponent>(button.c_str(), false);
            if (comp)
                ownerPanel = fp;
        }
        if (!comp) {
            const std::vector<PanelRef> &refs = cur->GetPanelRefs();
            for (size_t r = 0; !comp && r < refs.size(); ++r) {
                UIPanel *p = refs[r].mPanel;
                if (p && p->LoadedDir()) {
                    comp = p->LoadedDir()->Find<UIComponent>(button.c_str(), false);
                    if (comp)
                        ownerPanel = p;
                }
            }
        }
    }
    if (ownerOut) *ownerOut = ownerPanel;
    return comp;
}

void ExecSelect(const std::string &button, UIScreen *cur) {
    LocalUser *user = SynthUser();
    UIPanel *ownerPanel = nullptr;
    UIComponent *comp = FindSelectComponent(button, cur, &ownerPanel);
    if (comp) {
        MILO_LOG("RB3 input: SELECT '%s' on screen '%s'\n", button.c_str(),
                 cur ? cur->Name() : "(none)");
        if (ownerPanel)
            ownerPanel->SetFocusComponent(comp);
        comp->SendSelect(user);
    } else {
        MILO_LOG("RB3 input: SELECT '%s' NOT FOUND on screen '%s'\n", button.c_str(),
                 cur ? cur->Name() : "(none)");
    }
}

void ExecTrack(const std::string &trackSym) {
    LocalUser *user = SynthUser();
    BandUser *bu = TheBandUserMgr ? TheBandUserMgr->GetBandUser(user) : nullptr;
    if (bu) {
        Symbol sym(trackSym.c_str());
        TrackType ty = SymToTrackType(sym);
        bu->SetTrackType(ty);
        bu->SetDifficulty(kDifficultyExpert);
        MILO_LOG("RB3 input: track set: user=%p sym='%s' -> TrackType=%d diff=expert "
                 "(GetTrackSym='%s' IsFullyInGame=%d)\n",
                 (void *)bu, sym.Str(), (int)ty, bu->GetTrackSym().Str(),
                 (int)bu->IsFullyInGame());
    } else {
        MILO_LOG("RB3 input: track set FAILED: no BandUser for synth user\n");
    }
}

void ExecNoFail() {
    MetaPerformer *mp = MetaPerformer::Current();
    if (mp) {
        mp->SetBandNoFail(true);
        MILO_LOG("RB3 input: nofail enabled -> IsNoFailActive=%d IsBandNoFailSet=%d\n",
                 (int)mp->IsNoFailActive(), (int)mp->IsBandNoFailSet());
    } else {
        MILO_LOG("RB3 input: nofail FAILED: no MetaPerformer::Current()\n");
    }
}

// Turn on autoplay for every active player — the retail kiosk/E3 path mirrored
// from Game::E3CheatAutoplayAccuracy (Game.cpp:1041). SetAutoplay(true) flows to
// BeatMatcher::SetCheating(true); TrackWatcherImpl::CheckForAutoplay then auto-
// hits each gem at the strike window (HitGem -> GemSmasher::Hit -> hit.trig
// particles), which both fires the gameplay hit-FX and ticks the score off 0.
void ExecAutohit() {
    if (!TheGame) {
        MILO_LOG("RB3 input: autohit FAILED: no TheGame\n");
        return;
    }
    std::vector<Player *> &players = TheGame->GetActivePlayers();
    int n = 0;
    for (size_t i = 0; i < players.size(); ++i) {
        Player *p = players[i];
        if (p) {
            p->SetAutoplay(true);
            n++;
        }
    }
    MILO_LOG("RB3 input: autohit enabled on %d active player(s)\n", n);
}

void ExecButton(JoypadAction action, JoypadButton button, UIScreen *cur) {
    LocalUser *user = SynthUser();
    ButtonDownMsg msg(user, button, action, 0);
    const char *focusBtn = "(none)";
    if (cur && cur->FocusPanel() && cur->FocusPanel()->FocusComponent())
        focusBtn = cur->FocusPanel()->FocusComponent()->Name();
    MILO_LOG("RB3 input: injecting action %d (button %d) on '%s' focus='%s'\n",
             action, button, cur ? cur->Name() : "(none)", focusBtn);
    TheUI.Handle(msg, false);
}

#ifdef __EMSCRIPTEN__
// === W3c Part B — web part-select advance ==================================
// On part_difficulty_screen the overshell part-select sub-flow does NOT consume
// the screen-level Confirm (the synth user's slot focus is "(none)"), so a raw
// ExecButton(Confirm) never crosses part_difficulty -> game_screen. Native v1
// crosses it with the same synthetic verbs the RB3_GAME_INPUT script uses:
//   track:guitar                       (set the synth user's track type so the
//                                        part is committed and the user is
//                                        IsFullyInGame)
//   msg:overshell:end_override_flow:1:0 ({overshell end_override_flow
//                                        kOverrideFlow_SongSettings FALSE} —
//                                        commits the part-select override flow,
//                                        which fires Game::LoadSong and brings up
//                                        game_screen)
// then, once gameplay is live, two headless-playback verbs so the song actually
// plays through (no physical instrument to hit gems):
//   nofail   (MetaPerformer::SetBandNoFail — keeps gems flowing past the crowd
//             meter so a no-input run doesn't get booed off ~13s in)
//   autohit  (Player::SetAutoplay on every active player — auto-hits each gem at
//             its strike window, ticking the score HUD and firing hit-FX)
//
// Driven by a small per-frame state machine: when the user presses Confirm on
// part_difficulty_screen we ARM the sequence; each subsequent frame fires AT MOST
// ONE verb, readiness-gated via VerbReady (same gate the script queue uses), so a
// verb never lands on a half-loaded screen/object. Mirrors the native readiness
// loop, scoped to the part-select crossing only. Zero native impact (web-only).
//
// Forward decls: the readiness gate + dispatch helpers are defined further down.
struct Verb;
bool VerbReady(const Verb &v, UIScreen *cur, const char **reason);
const char *VerbName(const Verb &v);
void DispatchVerb(const Verb &v, UIScreen *cur);

enum WebPartStage {
    kWebPartIdle = 0,   // not on part_difficulty / not armed
    kWebPartTrack,      // fire track:guitar
    kWebPartEndFlow,    // fire msg:overshell:end_override_flow:1:0
    kWebPartNoFail,     // fire nofail (once gameplay live)
    kWebPartAutohit,    // fire autohit (once players active)
    kWebPartDone,
};
static int sWebPartStage = kWebPartIdle;

// Build a Verb for the readiness gate, then fire it via the same path as the
// script queue. Returns true if it fired (or is unconditionally safe), false if
// the readiness predicate says "wait" (retry next frame).
static bool WebFireGatedVerb(const Verb &v, UIScreen *cur) {
    const char *why = nullptr;
    if (!VerbReady(v, cur, &why)) {
        MILO_LOG("RB3 web part-select: WAIT %s: %s\n", VerbName(v), why ? why : "not ready");
        return false;
    }
    MILO_LOG("RB3 web part-select: FIRE %s on '%s'\n", VerbName(v),
             cur ? cur->Name() : "(none)");
    DispatchVerb(v, cur);
    return true;
}

// Drive the part-select crossing one step per frame. Called from the web input
// poll once the sequence is armed.
static void WebDrivePartSelect(int frame, UIScreen *cur) {
    if (sWebPartStage == kWebPartIdle || sWebPartStage == kWebPartDone)
        return;

    switch (sWebPartStage) {
    case kWebPartTrack: {
        Verb v;
        v.kind = kVerbTrack;
        v.trk.trackSym = "guitar";
        if (WebFireGatedVerb(v, cur))
            sWebPartStage = kWebPartEndFlow;
        break;
    }
    case kWebPartEndFlow: {
        Verb v;
        v.kind = kVerbMsg;
        v.msg.object = "overshell";
        v.msg.action = "end_override_flow";
        v.msg.args.clear();
        v.msg.args.push_back("1");   // kOverrideFlow_SongSettings
        v.msg.args.push_back("0");   // FALSE
        if (WebFireGatedVerb(v, cur))
            sWebPartStage = kWebPartNoFail;
        break;
    }
    case kWebPartNoFail: {
        // Wait for the song to actually finish loading (game_screen up + a live
        // MetaPerformer) before enabling no-fail; gate via the nofail predicate.
        Verb v;
        v.kind = kVerbNoFail;
        if (WebFireGatedVerb(v, cur))
            sWebPartStage = kWebPartAutohit;
        break;
    }
    case kWebPartAutohit: {
        Verb v;
        v.kind = kVerbAutohit;
        if (WebFireGatedVerb(v, cur)) {
            sWebPartStage = kWebPartDone;
            MILO_LOG("RB3 web part-select: sequence complete (frame %d)\n", frame);
        }
        break;
    }
    default:
        break;
    }
}
#endif // __EMSCRIPTEN__

// === Readiness predicate ===================================================
// Decide whether a queued verb may safely fire THIS frame. The dominant crash
// mode was a verb firing while the UI was mid-transition or before its target
// screen/object existed; gating on these conditions is what makes plain runs
// reliable. `reason` (optional) receives a short human string for the wait log.
bool VerbReady(const Verb &v, UIScreen *cur, const char **reason) {
    // A stable, loaded current screen is the baseline for every UI-facing verb:
    // never inject input or drive a SELECT while a screen swap is in flight.
    bool uiStable = (cur != nullptr) && !TheUI.InTransition();

    switch (v.kind) {
    case kVerbButton:
        // start/confirm/dpad: only on a stable current screen (so a transition
        // verb like song-select 'down'/'confirm' lands on the right screen).
        if (!uiStable) { if (reason) *reason = "UI in transition / no screen"; return false; }
        return true;

    case kVerbSelect: {
        // The named component must actually resolve in the (stable) current
        // screen before we SendSelect — otherwise the original code logged
        // "NOT FOUND" and the intended transition simply never happened
        // (forcing the next verb to fire against the wrong screen).
        if (!uiStable) { if (reason) *reason = "UI in transition / no screen"; return false; }
        UIPanel *owner = nullptr;
        if (!FindSelectComponent(v.sel.button, cur, &owner)) {
            if (reason) *reason = "select target not loaded on current screen";
            return false;
        }
        return true;
    }

    case kVerbMsg: {
        // The target object must exist in ObjectDir::Main() before we Handle()
        // a message on it (Handle on a null/half-built object is the crash).
        Hmx::Object *obj = ObjectDir::sMainDir
            ? ObjectDir::sMainDir->FindObject(v.msg.object.c_str(), false)
            : nullptr;
        if (!obj) { if (reason) *reason = "msg target object not present yet"; return false; }
        // Screen-transition messages (e.g. music_library select_highlighted_node
        // -> part_difficulty; overshell end_override_flow -> game_screen) must
        // also start from a stable screen so they don't stack on a transition.
        if (!uiStable) { if (reason) *reason = "UI in transition"; return false; }
        return true;
    }

    case kVerbTrack: {
        // track:<sym> needs the synth user's BandUser (built once the part flow
        // is entered) before SetTrackType.
        LocalUser *user = SynthUser();
        BandUser *bu = (TheBandUserMgr && user) ? TheBandUserMgr->GetBandUser(user) : nullptr;
        if (!bu) { if (reason) *reason = "BandUser for synth user not ready"; return false; }
        return true;
    }

    case kVerbNoFail: {
        // nofail gates on the song-load -> gameplay handoff: a live
        // MetaPerformer::Current() exists only once the performance is set up.
        if (!MetaPerformer::Current()) { if (reason) *reason = "no MetaPerformer (song not loaded)"; return false; }
        return true;
    }

    case kVerbAutohit: {
        // autohit needs gameplay to be live: a MetaPerformer::Current() AND at
        // least one active player (the synth user is only picked up after
        // track:guitar). Gating on both means SetAutoplay lands on a real
        // BeatMatcher and never derefs an empty/absent player list.
        if (!MetaPerformer::Current()) { if (reason) *reason = "no MetaPerformer (song not loaded)"; return false; }
        if (!TheGame || TheGame->GetActivePlayers().empty()) {
            if (reason) *reason = "no active players yet";
            return false;
        }
        return true;
    }
    }
    return true;
}

const char *VerbName(const Verb &v) {
    switch (v.kind) {
    case kVerbButton: return "button";
    case kVerbSelect: return "select";
    case kVerbMsg:    return "msg";
    case kVerbTrack:  return "track";
    case kVerbNoFail: return "nofail";
    case kVerbAutohit: return "autohit";
    }
    return "?";
}

// Fire a queued verb through the real engine paths (same helpers the per-type
// dispatch + HTTP path use).
void DispatchVerb(const Verb &v, UIScreen *cur) {
    switch (v.kind) {
    case kVerbButton: ExecButton(v.action, v.button, cur); break;
    case kVerbSelect: ExecSelect(v.sel.button, cur);       break;
    case kVerbMsg:    ExecMsg(v.msg, cur);                  break;
    case kVerbTrack:  ExecTrack(v.trk.trackSym);           break;
    case kVerbNoFail: ExecNoFail();                        break;
    case kVerbAutohit: ExecAutohit();                      break;
    }
}

// Parse + execute a single verb string (the HTTP /api/input path). Mirrors the
// RB3_GAME_INPUT token grammar, minus the "@frame:" prefix (HTTP verbs fire on
// the next frame). Returns false (with *err set) on an unparseable verb.
bool ExecVerb(const std::string &verb, UIScreen *cur, std::string *err) {
    if (verb.rfind("select:", 0) == 0) {
        ExecSelect(verb.substr(7), cur);
        return true;
    }
    if (verb.rfind("track:", 0) == 0) {
        ExecTrack(verb.substr(6));
        return true;
    }
    if (verb == "nofail") {
        ExecNoFail();
        return true;
    }
    if (verb == "autohit") {
        ExecAutohit();
        return true;
    }
    if (verb.rfind("msg:", 0) == 0) {
        std::string rest = verb.substr(4);
        std::vector<std::string> parts;
        size_t p = 0;
        while (p <= rest.size()) {
            size_t c = rest.find(':', p);
            if (c == std::string::npos) { parts.push_back(rest.substr(p)); break; }
            parts.push_back(rest.substr(p, c - p));
            p = c + 1;
        }
        if (parts.size() < 2) {
            if (err) *err = "msg verb needs object:action";
            return false;
        }
        ScriptedMsg sm;
        sm.object = parts[0];
        sm.action = parts[1];
        for (size_t k = 2; k < parts.size(); ++k)
            sm.args.push_back(parts[k]);
        ExecMsg(sm, cur);
        return true;
    }
    JoypadButton btn;
    JoypadAction act = ActionFromName(verb, btn);
    if (act == kAction_None) {
        if (err) *err = "unknown verb '" + verb + "'";
        return false;
    }
    ExecButton(act, btn, cur);
    return true;
}

// === Native DTA-manager stubs (DTA_MANAGER_STUBS §4) =======================
// Two boot-path managers live in subsystems excluded from the native link:
//   - saveload_mgr (SaveLoadManager.cpp is _NATIVE_FORK_EXCLUDE'd — it is 2266
//     lines deeply tied to MemcardMgr_Wii/WiiProfileMgr; its is_idle gate would
//     never reach idle natively anyway: DTA_MANAGER_STUBS §4 verification #1
//     explicitly sanctions the NativeSaveLoadStub fallback, mirroring DC3's).
//   - net_cache_mgr (the Wii net/store cache subsystem is not on the link).
// Both answer the splash/boot DTAs with safe single-player/offline defaults so
// the splash state machine advances. Registered by name into ObjectDir::Main()
// after TheUI.Init() (DTA_MANAGER_STUBS §4 placement). registerStub no-ops if a
// real singleton already claimed the name.

class NativeSaveLoadStub : public Hmx::Object {
public:
    virtual DataNode Handle(DataArray *msg, bool warn) {
        Symbol s = msg->Sym(1);
        if (s == "activate")             return DataNode(0);
        if (s == "is_idle")              return DataNode(1);
        if (s == "is_initial_load_done") return DataNode(1);
        if (s == "is_autosave_enabled")  return DataNode(0);
        if (s == "autosave")             return DataNode(0);
        if (s == "enable_autosave" || s == "disable_autosave") return DataNode(0);
        return Hmx::Object::Handle(msg, warn);
    }
};

class NativeNetCacheMgrStub : public Hmx::Object {
public:
    virtual DataNode Handle(DataArray *msg, bool warn) {
        Symbol s = msg->Sym(1);
        if (s == "init")            return DataNode(0); // no-op offline
        if (s == "is_ready")        return DataNode(1);
        if (s == "is_done_loading") return DataNode(1);
        return Hmx::Object::Handle(msg, warn);
    }
};

} // namespace

void RB3RegisterNativeManagerStubs() {
    auto registerStub = [](const char *name, Hmx::Object *obj) {
        if (!ObjectDir::sMainDir ||
            !ObjectDir::sMainDir->FindObject(name, false)) {
            obj->SetName(name, ObjectDir::sMainDir);
            MILO_LOG("RB3 native: registered manager stub '%s'\n", name);
        } else {
            delete obj; // real singleton already claimed this name
        }
    };
    registerStub("saveload_mgr",  new NativeSaveLoadStub());
    registerStub("net_cache_mgr", new NativeNetCacheMgrStub());

    // Mark first-time calibration as already seen. The splash kSplashScreen_End
    // Overshell step does `{cond ({! {profile_mgr get_has_seen_first_time_
    // calibration}} {ui push_screen first_time_calibration}) {ui goto_screen
    // main_hub_screen}}` — with a profile-less native boot the flag defaults to
    // 0, detouring boot into the calibration screen (an interactive audio/video
    // A/V-sync flow we cannot complete headless). The real game sets this flag
    // when calibration finishes; do the same up front so boot goes straight to
    // main_hub. This is the real game mechanism (ProfileMgr::SetHasSeenFirst
    // TimeCalibration), not a DTA/splash edit.
    TheProfileMgr.SetHasSeenFirstTimeCalibration(true);

    // Trigger the real DTA-driven content refresh. On console this fires when a
    // DTA handler sends `{content_mgr start_refresh}` (game.dta:348/376/446/483,
    // main_hub.dta:102, meta_loading.dta:319, song_select.dta:1879, …) at boot;
    // ContentMgr::PollRefresh then scans disc/NAND content sources, dispatches
    // each .dta to its registered Callbacks (BandSongMgr is registered in
    // BandSongMgr::Init), and settles at RefreshDone()=true. NativeContentMgr
    // (rb3_platform_native.cpp) overrides StartRefresh() to do exactly that
    // synchronously against `$RB3_DATA/songs/`: load songs.dta -> TheSongMgr.
    // AddSongs (which fires ContentDone internally) -> dispatch ContentDone to
    // every other registered Callback -> settle at kDiscoveryEnumerating. This
    // call is the same one those DTA handlers eventually make (since this is
    // the literal `start_refresh` entry point); kicking it once up front
    // pre-warms TheSongMgr before any DTA gate polls `{content_mgr refresh_done}`,
    // so song_select / meta_loading / part_difficulty all see a populated set.
    if (TheContentMgr)
        TheContentMgr->StartRefresh();
}

// Called once per frame from App::RunWithoutDebugging's native frame loop, AFTER
// TheUI.Poll() (so a transition kicked off by a prior frame's input has advanced
// before the next input fires).
void RB3GameInputPoll(int frame) {
    if (!gScriptParsed)
        ParseScript();

#ifdef __EMSCRIPTEN__
    // W3b: initialize JS key listeners on first poll (lazy, main-thread-safe).
    InitWebInput();

    // Read the JS bitmask, edge-detect newly-pressed bits, call ExecButton for
    // each. Only fires on press (rising edge), not while held — matches the
    // synthetic / HTTP path which always sends a single ButtonDownMsg. The
    // SynthUser() setup (pad 0 connected, controller=guitar) is shared with the
    // script/HTTP paths so the overshell add-user / slot-join flow (which gates
    // splash → main_hub) sees a real, pad-associated user.
    {
        unsigned int curButtons = GetWebKeyBitmask();
        unsigned int newPressed = curButtons & ~sWebPrevButtons;
        sWebPrevButtons = curButtons;

        if (newPressed) {
            // Ensure the synth user / pad 0 is wired before dispatching.
            SynthUser();
            UIScreen *webCur = TheUI.CurrentScreen();
            const char *webScr = webCur ? webCur->Name() : "(none)";
            // strcmp (not Symbol == Symbol): the screen name and a fresh
            // Symbol("...") need not intern to the same pointer on web, so the
            // pointer-equality operator== silently misses. Compare the strings.
            bool onPartDiff = webCur && strcmp(webScr, "part_difficulty_screen") == 0;
            bool onSongSelect = webCur && strcmp(webScr, "song_select_screen") == 0;
            for (int i = 0; i < kWebKeyMapSize; ++i) {
                if (newPressed & (1u << kWebKeyMap[i].bit)) {
                    MILO_LOG("RB3 web-input: frame %d  key bit=%d btn=%d action=%d"
                             " screen='%s'\n",
                             frame, kWebKeyMap[i].bit, (int)kWebKeyMap[i].btn,
                             (int)kWebKeyMap[i].action, webScr);
                    // W3c Part B: a Confirm on part_difficulty_screen arms the
                    // part-select crossing verb sequence (track:guitar ->
                    // end_override_flow -> nofail -> autohit) instead of (only) a
                    // raw ButtonDownMsg the overshell part-select won't consume.
                    if (onPartDiff && kWebKeyMap[i].action == kAction_Confirm) {
                        if (sWebPartStage == kWebPartIdle) {
                            sWebPartStage = kWebPartTrack;
                            MILO_LOG("RB3 web part-select: armed on part_difficulty "
                                     "(frame %d)\n", frame);
                        }
                    } else if (onSongSelect && kWebKeyMap[i].action == kAction_Confirm) {
                        // W3c Part B: a Confirm on song_select_screen confirms a
                        // song via the same DTA handler the native RB3_GAME_INPUT
                        // script uses ({music_library select_highlighted_node
                        // $user}). To reach the W3c target song (20th Century Boy)
                        // DETERMINISTICALLY — without blind keyboard list nav that
                        // can land on a non-song header node (a null-deref ->
                        // wasm trap in MusicLibrary::SelectNode's
                        // dynamic_cast<OwnedSongSortNode*>) — first pin the
                        // highlight to the target song via TryToSetHighlight, then
                        // select. The target defaults to 20thcenturyboy (the only
                        // W3c-required playable song); window.rb3WebTargetSong can
                        // override it for testing other songs.
                        char tgt[64] = "20thcenturyboy";
                        EM_ASM({
                            var s = window.rb3WebTargetSong;
                            if (s && s.length && s.length < 63)
                                stringToUTF8(s, $0, 64);
                        }, tgt);
                        if (TheMusicLibrary && tgt[0]) {
                            MILO_LOG("RB3 web song-select: pinning highlight to "
                                     "'%s' (frame %d)\n", tgt, frame);
                            TheMusicLibrary->TryToSetHighlight(Symbol(tgt), kNodeSong, false);
                        }
                        ScriptedMsg m;
                        m.object = "music_library";
                        m.action = "select_highlighted_node";
                        m.args.clear();
                        MILO_LOG("RB3 web song-select: confirm via "
                                 "music_library:select_highlighted_node (frame %d)\n",
                                 frame);
                        ExecMsg(m, webCur);
                    } else {
                        ExecButton(kWebKeyMap[i].action, kWebKeyMap[i].btn, webCur);
                    }
                }
            }
        }

        // W3c Part B: drive the armed part-select sequence one verb/frame.
        UIScreen *webCur2 = TheUI.CurrentScreen();
        WebDrivePartSelect(frame, webCur2);
    }
#endif // __EMSCRIPTEN__

    // Screen-flow trace: log every currentScreen change.
    UIScreen *cur = TheUI.CurrentScreen();
    Symbol curName = cur ? cur->Name() : Symbol();
    if (curName != gLastScreen) {
        MILO_LOG("RB3 screen: frame %d  currentScreen = '%s'%s\n",
                 frame, cur ? cur->Name() : "(none)",
                 TheUI.InTransition() ? "  (in transition)" : "");
        gLastScreen = curName;
    }

    // N4 fix — song-select stray SAVE/details pane.  On the Music Library
    // (song_select) screen, the `song_select_details` sub-pane (the song-detail /
    // leaderboard view with the "view gamer card" + save-prompt widgets) is only
    // meant to be visible while the panel is in `details_mode` (player pressed
    // Options on a song).  Retail hides it via the `details_hide.trg` PropAnim,
    // whose terminal `showing=FALSE` keyframe does not apply natively (the same
    // class of PropAnim/transform-keyframe gap seen elsewhere), so it is left
    // drawing over the right of the list — the documented "overlapping SAVE
    // panel".  Enforce the retail invariant directly: details pane visible iff
    // details_mode.  We only force-HIDE when details_mode is off, so opening the
    // details view later (which sets details_mode=1 and runs details_show.trg)
    // is unaffected.  Glue-layer, permuter-safe; opt-out via RB3_NO_DETAILS_FIX.
    if (!getenv("RB3_NO_DETAILS_FIX") && cur
        && curName == Symbol("song_select_screen") && ObjectDir::sMainDir) {
        UIPanel *ssp = ObjectDir::sMainDir->Find<UIPanel>("song_select_panel", false);
        ObjectDir *pd = ssp ? (ObjectDir *)ssp->LoadedDir() : nullptr;
        if (pd && ssp->GetState() == UIPanel::kUp) {
            const DataNode *dm = ssp->Property(Symbol("details_mode"), false);
            bool detailsMode = dm && dm->Int();
            if (!detailsMode) {
                RndDrawable *d =
                    dynamic_cast<RndDrawable *>(pd->FindObject("song_select_details", true));
                if (d && d->Showing())
                    d->SetShowing(false);
            }
        }
    }

    // N6 fix — seldiff `%S %I SONGS` raw setlist-token leak.  The on-screen leak
    // is the marquee song-preview label `song_preview.lbl`, NOT `setlist_title.lbl`
    // (the latter resolves correctly via update_setlist_label — confirmed by
    // walking every UILabel in the panel dir; only `song_preview.lbl` carries a
    // raw `%` at the seldiff frame).  The seldiff milo bakes a default
    // `text_token = set_list_named_title` ("%s <alt>%i songs</alt>") on
    // `song_preview.lbl`; UILabel::PostLoad fires SetTextToken with NO format args,
    // so SuperFormatString leaves the literal `%s`/`%i` and RndText kForceUpper
    // renders the raw "%S" / "%I SONGS".  Retail fills it via the
    // `update_preview_song` DTA handler → `set_song_and_artist_name_from_sym`,
    // which is an *AppLabel* handler — but this object loads as a base BandLabel
    // (class='BandLabel'), so that message is unhandled and the song name is never
    // set, leaving the baked default on screen.  Enforce the retail intent
    // directly: when `song_preview.lbl` still shows a raw `%`-token, fill it with
    // the current song+artist text — computed exactly as
    // AppLabel::SetSongAndArtistNameFromSymbol does — via the base
    // UILabel::SetDisplayText (class-agnostic).  We act only on the raw-token
    // state, so a legitimately-substituted preview is never clobbered.
    // Glue-layer, permuter-safe; opt-out via RB3_NO_SETLIST_FIX.
    // NumSongs()/GetSongSymbol() are the real strong MetaPerformer defs (the weak
    // .s stubs are overridden).
    if (!getenv("RB3_NO_SETLIST_FIX") && cur
        && curName == Symbol("part_difficulty_screen") && ObjectDir::sMainDir) {
        UIPanel *sdp =
            ObjectDir::sMainDir->Find<UIPanel>("part_difficulty_panel", false);
        ObjectDir *pd = sdp ? (ObjectDir *)sdp->LoadedDir() : nullptr;
        if (getenv("RB3_SETLIST_DBG") && pd && (frame % 40) == 0) {
            // Diag: report any UILabel in the panel dir whose RAW text still
            // carries a `%` specifier (an on-screen raw-token leak).
            for (ObjDirItr<UILabel> it(pd, true); it != nullptr; ++it) {
                RndText *dt = it->TextObj();
                const char *r = dt ? dt->RawText() : nullptr;
                if (r && strchr(r, '%'))
                    MILO_LOG("RB3 N6 diag: frame %d LABEL '%s' class='%s' "
                             "raw='%s' showing=%d\n",
                             frame, it->Name(), it->ClassName().Str(), r,
                             (int)it->Showing());
            }
        }
        if (pd && sdp->GetState() == UIPanel::kUp) {
            UILabel *prev =
                dynamic_cast<UILabel *>(pd->FindObject("song_preview.lbl", true));
            RndText *txt = prev ? prev->TextObj() : nullptr;
            // Copy the current raw text — RawText() aliases RndText::mText, which
            // SetDisplayText below reallocates (the alias would dangle).
            String rawText = txt ? txt->RawText() : "";
            const char *raw = rawText.c_str();
            // Raw token leak iff the displayed text still carries a `%` specifier.
            if (raw && strchr(raw, '%')) {
                MetaPerformer *mp = MetaPerformer::Current();
                int numsongs = mp ? mp->NumSongs() : 0;
                if (mp && numsongs > 0) {
                    // Compose the song+artist string exactly as
                    // AppLabel::SetSongAndArtistNameFromSymbol (AppLabel.cpp:206):
                    // title + (master ? artist : "as made famous by <artist>"),
                    // joined via the song_artist_fmt[_number] locale template.
                    Symbol song = mp->GetSongSymbol(0);
                    int songID = TheSongMgr.GetSongIDFromShortName(song, true);
                    BandSongMetadata *data =
                        (BandSongMetadata *)TheSongMgr.Data(songID);
                    String titleStr, artistStr;
                    if (!data) {
                        titleStr = Localize(Symbol("unknown_song"), (bool *)0);
                    } else {
                        titleStr = data->Title();
                        if (!data->IsMasterRecording())
                            artistStr = MakeString(
                                "%s %s", Localize(Symbol("store_famous_by"), (bool *)0),
                                data->Artist());
                        else
                            artistStr = data->Artist();
                    }
                    int idx = numsongs > 1 ? 1 : 0;  // multi-song marquee → "N."
                    if (idx <= 0)
                        prev->SetDisplayText(
                            MakeString(Localize(Symbol("song_artist_fmt"), (bool *)0),
                                       titleStr.c_str(), artistStr.c_str()),
                            true);
                    else
                        prev->SetDisplayText(
                            MakeString(
                                Localize(Symbol("song_artist_fmt_number"), (bool *)0),
                                idx, titleStr.c_str(), artistStr.c_str()),
                            true);
                } else {
                    // No song known — blank rather than leave the raw token.
                    prev->SetDisplayText("", true);
                }
                RndText *t2 = prev->TextObj();
                MILO_LOG("RB3 N6: frame %d replaced seldiff song_preview.lbl raw "
                         "token '%s' -> '%s' (numsongs=%d)\n",
                         frame, raw, t2 ? t2->RawText() : "(none)", numsongs);
            }
        }
    }

    // RB3_HIDE_MESH=a,b,c : debug — hide named drawables (searching subdirs) in
    // the song_select_panel dir; used to localize render artifacts.
    if (getenv("RB3_HIDE_MESH") && cur && curName == Symbol("song_select_screen")
        && ObjectDir::sMainDir) {
        UIPanel *ssp = ObjectDir::sMainDir->Find<UIPanel>("song_select_panel", false);
        ObjectDir *pd = ssp ? (ObjectDir *)ssp->LoadedDir() : nullptr;
        if (pd) {
            std::string spec = getenv("RB3_HIDE_MESH");
            size_t pos = 0;
            while (pos < spec.size()) {
                size_t comma = spec.find(',', pos);
                std::string nm = spec.substr(pos, comma == std::string::npos ? std::string::npos : comma - pos);
                pos = comma == std::string::npos ? spec.size() : comma + 1;
                if (nm.empty()) continue;
                RndDrawable *d = dynamic_cast<RndDrawable *>(pd->FindObject(nm.c_str(), true));
                if (d) d->SetShowing(false);
            }
        }
    }

    // RB3_SCREEN_DBG=1: log transition state (and the transition screen) every N
    // frames so a stalled transition is visible. Dump per-panel load states of
    // the transition screen so a stuck panel is visible.
    if (getenv("RB3_SCREEN_DBG") && (frame % 40) == 0) {
        UIScreen *ts = TheUI.TransitionScreen();
        MILO_LOG("RB3 screen-dbg: frame %d  cur='%s' trans=%d transScreen='%s'\n",
                 frame, cur ? cur->Name() : "(none)", TheUI.InTransition(),
                 ts ? ts->Name() : "(none)");
        if (ts) {
            const std::vector<PanelRef> &refs = ts->GetPanelRefs();
            for (size_t r = 0; r < refs.size(); ++r) {
                UIPanel *p = refs[r].mPanel;
                MILO_LOG("    panel[%zu] '%s' active=%d loaded=%d state=%d isLoaded=%d\n",
                         r, p ? p->Name() : "(null)", refs[r].mActive, refs[r].mLoaded,
                         p ? (int)p->GetState() : -1, p ? (int)p->IsLoaded() : -1);
            }
        }
    }

    // RB3_PANEL_DBG=1: dump the CURRENT screen's panels (showing/state/active)
    // every N frames — to find which panel is drawing the song-select grey
    // occluder / stray SAVE panel (N4). Showing()==true && Active() && state==kUp
    // => the panel draws (UIScreen::Draw / BandUI::Draw gate on exactly that).
    if (getenv("RB3_PANEL_DBG") && cur && (frame % 20) == 0) {
        const std::vector<PanelRef> &refs = cur->GetPanelRefs();
        MILO_LOG("RB3 panel-dbg: frame %d screen='%s' npanels=%zu\n",
                 frame, cur->Name(), refs.size());
        for (size_t r = 0; r < refs.size(); ++r) {
            UIPanel *p = refs[r].mPanel;
            MILO_LOG("    panel[%zu] '%s' active=%d showing=%d state=%d "
                     "entering=%d exiting=%d\n",
                     r, p ? p->Name() : "(null)", refs[r].Active(),
                     p ? (int)p->Showing() : -1, p ? (int)p->GetState() : -1,
                     p ? (int)p->Entering() : -1, p ? (int)p->Exiting() : -1);
        }
        // song_select_panel details state (the N4 SAVE/details-pane bug): the
        // details/leaderboard pane should only show while details_mode is set.
        if (ObjectDir::sMainDir) {
            UIPanel *ssp2 = ObjectDir::sMainDir->Find<UIPanel>("song_select_panel", false);
            ObjectDir *pd2 = ssp2 ? (ObjectDir *)ssp2->LoadedDir() : nullptr;
            if (ssp2) {
                const DataNode *dm = ssp2->Property(Symbol("details_mode"), false);
                RndDrawable *d = pd2 ? dynamic_cast<RndDrawable *>(
                                     pd2->FindObject("song_select_details", true))
                                     : nullptr;
                MILO_LOG("    details_mode=%d  song_select_details.showing=%d\n",
                         dm ? dm->Int() : -99, d ? (int)d->Showing() : -1);
            }
        }
    }

    // Optional overshell-slot state dump (PART_DBG=1): the overshell panel's
    // override flow + all-slots-ready, queried via the real DTA handlers.
    if (getenv("PART_DBG") && ObjectDir::sMainDir && (frame % 40) == 0) {
        Hmx::Object *ov = ObjectDir::sMainDir->FindObject("overshell", false);
        if (ov) {
            Message qf(Symbol("in_override_flow"), DataNode(1)); // SongSettings=1
            DataNode r = ov->Handle(qf, false);
            Message qr(Symbol("all_slots_ready_to_play"));
            DataNode rr = ov->Handle(qr, false);
            MILO_LOG("PART_DBG: frame %d screen='%s' overshell inSongSettingsFlow=%d allReady=%d\n",
                     frame, cur ? cur->Name() : "(none)",
                     (r.Type() == kDataInt) ? r.Int() : -1,
                     (rr.Type() == kDataInt) ? rr.Int() : -1);
        }
    }

    // Optional splash/overshell state dump (RB3_INPUT_DEBUG=1).
    if (getenv("RB3_INPUT_DEBUG") && ObjectDir::sMainDir) {
        Hmx::Object *sp = ObjectDir::sMainDir->FindObject("splash_panel", false);
        Hmx::Object *ov = ObjectDir::sMainDir->FindObject("overshell", false);
        const DataNode *st = sp ? sp->Property(Symbol("splash_state"), false) : nullptr;
        int splashState = st ? st->Int() : -99;
        int allowing = -1;
        if (ov) {
            Message q(Symbol("is_any_slot_allowing_input_to_shell"));
            DataNode r = ov->Handle(q, false);
            allowing = (r.Type() == kDataInt) ? r.Int() : -2;
        }
        static int lastSplash = -100, lastAllow = -100;
        if (splashState != lastSplash || allowing != lastAllow) {
            MILO_LOG("RB3 dbg: frame %d  splash_state=%d  overshell_allowing=%d\n",
                     frame, splashState, allowing);
            lastSplash = splashState;
            lastAllow = allowing;
        }
    }

    // === State-driven sequential verb dispatch =============================
    // Walk the ordered verb queue. The CURSOR verb fires only when frame has
    // reached its @N minimum AND its readiness predicate holds (target screen/
    // object loaded + UI stable). Otherwise it WAITS (retried next frame) — no
    // blind dispatch against an unloaded screen/object, which was the SIGSEGV.
    // After the cursor fires, advance; we fire AT MOST ONE verb per frame so a
    // transition kicked off by one verb settles before the next is even
    // evaluated (the per-frame Poll runs between frames). A verb whose target
    // never appears within kVerbTimeout frames of becoming eligible is skipped
    // (LOG) so a bad/over-eager script degrades gracefully instead of stalling.
    if (gVerbCursor < gVerbs.size()) {
        const Verb &v = gVerbs[gVerbCursor];
        // Button presses (joypad navigation, down/up/confirm etc.) update UI
        // focus synchronously but the engine only re-evaluates the focused-node
        // state on the NEXT Poll(). Gate the next verb: if the PREVIOUS verb was
        // a button action, wait at least 2 frames after it fired so the UI can
        // fully propagate the new focus/highlight before we fire a followup msg.
        // The original exact-frame scripts relied on the 30-frame gaps in their
        // @N hints for this; the state-driven queue collapsed them to 1 frame,
        // causing e.g. @320:down + @350:select_highlighted_node to pick the
        // wrong node (the old focused node, not the one just navigated to).
        static int gLastButtonFrame = -1;
        if (gVerbCursor > 0 && gVerbs[gVerbCursor - 1].kind == kVerbButton)
            gLastButtonFrame = gLastFiredFrame;
        bool buttonSettled = (gLastButtonFrame < 0 || frame >= gLastButtonFrame + 2);

        if (frame >= v.minFrame && buttonSettled) {
            if (gVerbWaitSince < 0)
                gVerbWaitSince = frame;   // became eligible this frame
            const char *why = nullptr;
            if (VerbReady(v, cur, &why)) {
                MILO_LOG("RB3 input: frame %d  FIRE [%zu/%zu] %s (min @%d) on '%s'\n",
                         frame, gVerbCursor + 1, gVerbs.size(), VerbName(v), v.minFrame,
                         cur ? cur->Name() : "(none)");
                DispatchVerb(v, cur);
                gLastFiredFrame = frame;
                gVerbCursor++;
                gVerbWaitSince = -1;
            } else {
                // Not ready yet — wait. Log the first wait + give up after the
                // deadline so we never hang on a target that never loads.
                if (frame == gVerbWaitSince)
                    MILO_LOG("RB3 input: frame %d  WAIT [%zu/%zu] %s (min @%d): %s\n",
                             frame, gVerbCursor + 1, gVerbs.size(), VerbName(v), v.minFrame,
                             why ? why : "not ready");
                if (frame - gVerbWaitSince >= VerbTimeoutFrames()) {
                    MILO_LOG("RB3 input: frame %d  SKIP [%zu/%zu] %s (min @%d): "
                             "target never became ready (%s) after %d frames\n",
                             frame, gVerbCursor + 1, gVerbs.size(), VerbName(v), v.minFrame,
                             why ? why : "not ready", VerbTimeoutFrames());
                    gVerbCursor++;
                    gVerbWaitSince = -1;
                }
            }
        }
    }

    // HTTP-injected verbs (RB3HttpServer /api/input). Drain + execute on this
    // (main) thread, frame-agnostic — each fires on the frame it is drained.
    std::vector<std::string> inject;
    {
        std::lock_guard<std::mutex> lk(gInjectMutex);
        inject.swap(gPendingInject);
    }
    for (const std::string &verb : inject) {
        std::string err;
        MILO_LOG("RB3 input: frame %d  HTTP verb '%s'\n", frame, verb.c_str());
        if (!ExecVerb(verb, cur, &err))
            MILO_LOG("RB3 input: HTTP verb rejected: %s\n", err.c_str());
    }
}

// === HTTP /api/input bridge ================================================
// Called from the HTTP handler thread: enqueue a raw verb string to fire on the
// next main-thread RB3GameInputPoll. Thread-safe.
void RB3GameInputInjectVerb(const std::string &verb) {
    std::lock_guard<std::mutex> lk(gInjectMutex);
    gPendingInject.push_back(verb);
}

// Execute a verb directly on the MAIN thread (called from RB3HttpServer's
// command-queue HandleInput via RB3HttpServerPoll). Resolves CurrentScreen here
// (main-thread-safe) and runs the real engine path immediately, returning a
// synchronous ok/err to the HTTP handler. Returns false (+ *err) on a bad verb.
bool RB3GameInputExecVerbMainThread(const std::string &verb, std::string *err) {
    UIScreen *cur = TheUI.CurrentScreen();
    return ExecVerb(verb, cur, err);
}
