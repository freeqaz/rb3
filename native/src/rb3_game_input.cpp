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
// part/difficulty selection is done with REAL pad presses, NOT by mutating
// engine state (the former track:/difficulty: skip hack is gone):
//   part:guitar   → on the part_difficulty_screen choose_part overshell view,
//                   press Confirm to commit the (default-focus) guitar part.
//                   Only `guitar` is supported; other syms log + skip.
//   diff:<easy|medium|hard|expert|0-3>
//                 → on the choose_diff view, press DDown to the top-down list
//                   index (Easy0/Medium1/Hard2/Expert3) then Confirm.
// Both are multi-frame state machines (StepPadVerb): readiness-gated on the real
// overshell view, self-paced on the pad queue, and they dismiss a confirm_action
// dialog (part denial / diff confirm) with a bounded extra Confirm. This is the
// exact path keyboard-to-gameplay.py drives with raw pad: presses.
//
// W3b: real browser keyboard input added under #ifdef __EMSCRIPTEN__. JS
// keydown/keyup listeners maintain window._rb3Keys bitmask; RB3GameInputPoll
// drains it per-frame (edge-detect) and calls ExecButton() — the same path the
// synthetic script + HTTP /api/input use. Zero native / Wii-asm impact.
//
// C6: overshell:<action>[:arg] verb — resolves the synth user's OvershellSlot
// and calls slot->Handle({action, args...}), routing through the real
// OvershellSlot BEGIN_HANDLERS:
//   overshell:show_options        → kState_Options (pause-menu options list)
//   overshell:show_game_options   → kState_GameOptions (Lefty/VocalStyle in-song)
//   overshell:leave_options       → dismiss options / return to gameplay
//   overshell:toggle_lefty_flip   → ToggleLeftyFlip (in-session; SESSION-ONLY,
//                                   not persisted until C2 SaveLoadManager)
//   overshell:toggle_vocal_style  → ToggleVocalStyle (same session-only note)
//   overshell:show_state:<int>    → ShowState(id) generic
//
// GameplayOptions persistence: Lefty/VocalStyle changes take effect immediately
// in-session via GameplayOptions members but are NOT saved across restart until
// C2 (SaveLoadManager) lands — the SaveLoadManager is a no-op stub natively.
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
#include "ui/UIPicture.h" // UIPicture::SetHookTex — N8 album-art smear re-show kill
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
#include "game/BandUser.h"           // BandUser (GetBandUser readiness gate)
#include "game/BandUserMgr.h"
#include "game/Game.h"               // TheGame, Game::GetActivePlayers (autohit)
#include "game/Player.h"             // Player::SetAutoplay/IsAutoplay (autohit)
#include "game/Defines.h"
#include "meta_band/ProfileMgr.h"
#include "meta_band/MetaPerformer.h"   // SetBandNoFail (nofail directive)
#include "meta_band/BandUI.h"          // TheBandUI.GetOvershell() (difficulty/part select)
#include "meta_band/OvershellPanel.h"  // OvershellPanel::GetSlot / EndOverrideFlow
#include "meta_band/OvershellSlot.h"   // OvershellSlot::SelectPart / kOverrideFlow_SongSettings
#include "os/ContentMgr.h"
#include "os/System.h"
#include "obj/Object.h"
#include "obj/Data.h"
#include "obj/Dir.h"
#include "obj/Msg.h"        // MsgSource — NativeRockCentralStub base (add_sink/remove_sink)
#include "utl/Symbol.h"
#include "rndobj/Draw.h"    // RndDrawable — N4 song-select details-pane hide

#include "rb3_session_trace.h"  // RB3TraceRecordAid — capture out-of-band run aids (M4 GAP 2)
#include "rb3_replay.h"         // RB3ReplayPendingAids — re-apply recorded aids on replay (M4 GAP 2)

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cctype>
#include <string>
#include <vector>
#include <mutex>
#include <algorithm>

namespace {

// C3: set by the `quit` input verb; read by App's HX_NATIVE frame loop via the
// RB3CleanExitRequested() accessor below the namespace. volatile because the
// loop polls it each frame after the input drain.
volatile bool gCleanExitRequested = false;

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
        // Arrows → d-pad menu nav. WASD is intentionally NOT mapped here: a/s/d/
        // f/g are the gameplay fret keys (rb3_joypad_native.cpp InitWebGameplayKeys)
        // and both JS listener sets stay installed, so mapping a key to a d-pad bit
        // here AND a fret bit there would OR both onto one keypress — the collision
        // that caused a stuck red fret + phantom strums. Nav is arrows-only.
        m['ArrowUp']    = 1<<12;  // kPad_DUp
        m['ArrowDown']  = 1<<14;  // kPad_DDown
        m['ArrowLeft']  = 1<<15;  // kPad_DLeft
        m['ArrowRight'] = 1<<13;  // kPad_DRight
        // Face / menu buttons. Enter → green fret (kPad_R2); button_meanings in
        // config/joypad.dta maps R2 → kAction_Confirm so Enter still confirms menus.
        m['Enter']     = 1<<1;   // kPad_R2 (green fret) → kAction_Confirm
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

// C6: an "overshell:<action>[:arg]..." directive — resolves the synth user's
// OvershellSlot and routes the message through slot->Handle().
struct ScriptedOvershell {
    int frame;
    std::string action;
    std::vector<std::string> args;
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
std::vector<ScriptedNoFail> gNoFailScript;
bool gScriptParsed = false;
Symbol gLastScreen;
LocalUser *gSynthUser = nullptr;

// Raw pad-press queue feeder (rb3_joypad_native.cpp) — `pad:<bit>` HTTP verb.
extern "C" bool RB3JoypadEnqueuePad(int bit);
// True while a queued pad press is still draining (hold/gap in flight or presses
// pending) — the part:/diff: state machines self-pace on this.
extern "C" bool RB3JoypadPadQueueBusy();

// === Difficulty parsing ====================================================
// The `diff:<easy|medium|hard|expert|0-3>` verb drives the REAL choose_diff
// overshell view with actual pad presses (DDown x index + Confirm) — no
// programmatic BandUser::SetDifficulty. ParseDifficulty/DifficultyName below
// turn the token into the top-down list INDEX the pad presses navigate to
// (Easy=0 / Medium=1 / Hard=2 / Expert=3, default focus Easy).

// Parse a difficulty token: easy/medium/hard/expert OR the numeric 0-3. We do
// NOT call the game's SymToDifficulty for the numeric form — it asserts on a
// bad sym. Returns true (and writes *out) on a recognized token.
bool ParseDifficulty(const std::string &tok, Difficulty &out) {
    if (tok == "easy")   { out = kDifficultyEasy;   return true; }
    if (tok == "medium") { out = kDifficultyMedium; return true; }
    if (tok == "hard")   { out = kDifficultyHard;   return true; }
    if (tok == "expert") { out = kDifficultyExpert; return true; }
    if (tok.size() == 1 && tok[0] >= '0' && tok[0] <= '3') {
        out = (Difficulty)(tok[0] - '0');
        return true;
    }
    return false;
}

const char *DifficultyName(Difficulty d) {
    switch (d) {
    case kDifficultyEasy:   return "easy";
    case kDifficultyMedium: return "medium";
    case kDifficultyHard:   return "hard";
    case kDifficultyExpert: return "expert";
    default: return "?";
    }
}

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
enum VerbKind { kVerbButton, kVerbSelect, kVerbMsg, kVerbPart, kVerbNoFail, kVerbAutohit, kVerbDiff, kVerbOvershell };

struct Verb {
    int        kind;
    int        minFrame;   // @N — earliest frame this verb may fire (ordering hint)
    int        origIndex;  // stable tiebreak so equal-@N verbs keep script order
    // Payload (only the field matching `kind` is meaningful):
    JoypadAction action = kAction_None;
    JoypadButton button = kPad_NumButtons;
    ScriptedSelect sel{0, ""};
    ScriptedMsg    msg;
    Difficulty     diff = kDifficultyExpert;  // payload for kVerbDiff (list index)
    ScriptedOvershell overshell;              // payload for kVerbOvershell
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
        // "part:<sym>" directive — confirm a part on the REAL part_difficulty
        // choose_part overshell view with an actual Confirm pad press (guitar is
        // the default focus). Multi-frame: driven by StepPadVerb once the
        // choose_part view is up. Only `guitar` is supported today; any other sym
        // is logged + skipped (all shipping scripts use guitar).
        if (action.rfind("part:", 0) == 0) {
            std::string sym = action.substr(5);
            if (sym != "guitar") {
                MILO_LOG("RB3 input: unsupported part '%s' (only 'guitar') — skipping\n",
                         sym.c_str());
                continue;
            }
            Verb v; v.kind = kVerbPart; v.minFrame = frame; v.origIndex = (int)gVerbs.size();
            gVerbs.push_back(v);
            MILO_LOG("RB3 input: scheduled @%d (min) -> part '%s'\n", frame, sym.c_str());
            continue;
        }
        // "diff:<easy|medium|hard|expert|0-3>" directive — pick a difficulty on
        // the REAL choose_diff overshell view with actual pad presses (DDown x
        // list-index + Confirm). Multi-frame: driven by StepPadVerb once the
        // choose_diff view is up.
        if (action.rfind("diff:", 0) == 0) {
            Difficulty d;
            if (!ParseDifficulty(action.substr(5), d)) {
                MILO_LOG("RB3 input: bad diff '%s' in RB3_GAME_INPUT\n",
                         action.substr(5).c_str());
                continue;
            }
            Verb v; v.kind = kVerbDiff; v.minFrame = frame; v.origIndex = (int)gVerbs.size();
            v.diff = d;
            gVerbs.push_back(v);
            MILO_LOG("RB3 input: scheduled @%d (min) -> diff '%s'\n", frame, DifficultyName(d));
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
        // "overshell:<action>[:arg]..." directive — resolve the synth user's
        // OvershellSlot and route the action through slot->Handle().
        // C6 pause/options navigation.
        if (action.rfind("overshell:", 0) == 0) {
            std::string rest = action.substr(10);
            std::vector<std::string> parts;
            size_t p = 0;
            while (p <= rest.size()) {
                size_t c = rest.find(':', p);
                if (c == std::string::npos) { parts.push_back(rest.substr(p)); break; }
                parts.push_back(rest.substr(p, c - p));
                p = c + 1;
            }
            if (!parts.empty() && !parts[0].empty()) {
                ScriptedOvershell so;
                so.frame = frame;
                so.action = parts[0];
                for (size_t k = 1; k < parts.size(); ++k)
                    so.args.push_back(parts[k]);
                Verb v; v.kind = kVerbOvershell; v.minFrame = frame; v.origIndex = (int)gVerbs.size();
                v.overshell = so;
                gVerbs.push_back(v);
                MILO_LOG("RB3 input: scheduled @%d (min) -> overshell {%s} (+%d args)\n", frame,
                         so.action.c_str(), (int)so.args.size());
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
            bool numeric = !a.empty() &&
                (isdigit((unsigned char)a[0]) || (a[0] == '-' && a.size() > 1));
            if (a == "$user")
                da->Node((int)k + 2) = DataNode((Hmx::Object *)user);
            else if (numeric && a.find('.') != std::string::npos)
                // C3: a fractional token (e.g. -45.5) carries a float so the
                // msg: verb can drive sub-ms lag offsets. Int args still take the
                // atoi branch; Float() coerces int->float for the int-arg case.
                da->Node((int)k + 2) = DataNode((float)atof(a.c_str()));
            else if (numeric)
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

// === Real part/difficulty selection via pad presses ========================
// part:/diff: drive the VISIBLE part_difficulty overshell sub-flow with actual
// button presses (RB3JoypadEnqueuePad -> SendButtonMessages), exactly the path
// keyboard-to-gameplay.py takes and the faithful equivalent of a physical
// guitar button. No programmatic slot->SelectPart()/SetDifficulty() — the engine
// state machine transitions on its own from the real Confirm/DDown edges.

// The synth (pad-0) user's overshell slot current view Symbol, or a null Symbol
// if the slot isn't resolvable yet. Mirrors rb3_http_handlers.cpp's {rb3_overshell}
// probe — the exact lookup keyboard-to-gameplay.py reads to watch the sub-flow
// (choose_part_* -> choose_diff -> confirm_action -> ready_to_play).
Symbol OvershellView() {
    OvershellPanel *ov = TheBandUI.GetOvershell();
    if (!ov)
        return Symbol();
    for (int i = 0; i < 4; i++) {
        OvershellSlot *s = ov->GetSlot(i);
        if (s && s->GetUser() && s->GetUser()->IsLocal())
            return s->GetCurrentView();
    }
    return Symbol();
}

// Pad bits pressed by the part/diff state machines (same bits the keyboard
// harness presses): Confirm = kPad_X, list-scroll = kPad_DDown.
const int kPadBitConfirm = kPad_X;      // 6
const int kPadBitDDown   = kPad_DDown;  // 14

// A part:/diff: verb spans several frames: enqueue press(es) -> wait for the pad
// queue to drain -> if a confirm_action dialog (part denial / diff confirm) came
// up, press Confirm again (bounded retries). One step per frame, only advancing
// when the pad queue is idle (RB3JoypadPadQueueBusy()==false).
enum PadVerbPhase {
    kPadPhaseIdle = 0,   // not begun (state inactive)
    kPadPhaseAct,        // enqueue the verb's primary presses (scroll + Confirm)
    kPadPhaseDrain,      // wait for the pad queue to drain, then check for a dialog
};
struct PadVerbState {
    int cursor  = -1;    // gVerbCursor this state belongs to (-1 == inactive)
    int phase   = kPadPhaseIdle;
    int retries = 0;     // confirm_action dismissals used (bounded to 4)
};
PadVerbState gPadVerb;

// Enqueue a pad verb's primary presses (called once, entering kPadPhaseAct).
// diff:<d> scrolls DDown to the top-down list index (Easy0/Medium1/Hard2/Expert3,
// default focus Easy) then Confirms; part:guitar Confirms the default focus.
void PadVerbEnqueuePrimary(const Verb &v) {
    if (v.kind == kVerbDiff) {
        int idx = (int)v.diff;
        for (int i = 0; i < idx; i++)
            RB3JoypadEnqueuePad(kPadBitDDown);
        RB3JoypadEnqueuePad(kPadBitConfirm);
    } else {  // kVerbPart — guitar default focus: a single Confirm commits it
        RB3JoypadEnqueuePad(kPadBitConfirm);
    }
}

// Returns true iff the aid took effect (a live MetaPerformer existed). On replay
// a false return leaves the aid pending so it is retried next frame.
bool ExecNoFail() {
    MetaPerformer *mp = MetaPerformer::Current();
    if (mp) {
        mp->SetBandNoFail(true);
        MILO_LOG("RB3 input: nofail enabled -> IsNoFailActive=%d IsBandNoFailSet=%d\n",
                 (int)mp->IsNoFailActive(), (int)mp->IsBandNoFailSet());
        // M4 GAP 2: capture the out-of-band aid into the trace as a one-shot,
        // replayable mark (the HTTP/script verb that enabled it is NOT an `in`
        // edge). De-duped + no-op when not tracing. RB3ReplayPendingAids re-fires
        // this on replay; recording the re-applied aid again is harmless (deduped).
        RB3TraceRecordAid("nofail");
        return true;
    }
    MILO_LOG("RB3 input: nofail FAILED: no MetaPerformer::Current()\n");
    return false;
}

// Turn on autoplay for every active player — the retail kiosk/E3 path mirrored
// from Game::E3CheatAutoplayAccuracy (Game.cpp:1041). SetAutoplay(true) flows to
// BeatMatcher::SetCheating(true); TrackWatcherImpl::CheckForAutoplay then auto-
// hits each gem at the strike window (HitGem -> GemSmasher::Hit -> hit.trig
// particles), which both fires the gameplay hit-FX and ticks the score off 0.
// Returns true iff autoplay armed >=1 ready player (the score-producing effect).
// On replay a false return (no game / no ready player yet) leaves the aid pending
// so it is retried next frame until the load chain wires the players up.
bool ExecAutohit() {
    if (!TheGame) {
        MILO_LOG("RB3 input: autohit FAILED: no TheGame\n");
        return false;
    }
    std::vector<Player *> &players = TheGame->GetActivePlayers();
    int n = 0;
    for (size_t i = 0; i < players.size(); ++i) {
        Player *p = players[i];
        // SetAutoplay -> BeatMatcher::SetAutoplay -> mWatcher->SetCheating(); the
        // watcher is built in BeatMatcher::SetTrack (Game::PostLoad). Player::
        // IsReady() (== mMatcher->IsReady(), null-watcher-safe) confirms the load
        // chain wired this player up. The kVerbAutohit gate already requires
        // Game::IsLoaded(), but skip-if-not-ready here too so the IsLoaded()
        // audio-Fail early-true path can never drive a null-watcher deref.
        if (p && p->IsReady()) {
            p->SetAutoplay(true);
            n++;
        } else if (p) {
            MILO_LOG("RB3 input: autohit skipped player %d (not ready / watcher unbuilt)\n", (int)i);
        }
    }
    MILO_LOG("RB3 input: autohit enabled on %d active player(s)\n", n);
    // M4 GAP 2: capture autohit into the trace ONLY when it actually took effect
    // (>=1 player armed). The autoplay is what produced the recorded score; the
    // one-shot replayable mark lets replay re-arm it at this frame so the same
    // gems auto-hit -> same score. De-duped + no-op when not tracing. Recording it
    // only on success means a replay won't re-fire it before players are ready
    // (the n==0 case is retried by the same readiness path it took at record time).
    if (n > 0)
        RB3TraceRecordAid("autohit");
    return n > 0;
}

// C6: route an overshell:<action>[:arg...] verb through the synth user's
// OvershellSlot. Resolves the slot via TheBandUI.GetOvershell()->GetSlot(bu),
// then builds a DataArray for the message and sends it via slot->Handle(),
// mirroring ExecMsg for named-object targets.
void ExecOvershellVerb(const ScriptedOvershell &ov) {
    LocalUser *user = SynthUser();
    BandUser *bu = (TheBandUserMgr && user) ? TheBandUserMgr->GetBandUser(user) : nullptr;
    if (!bu) {
        MILO_LOG("RB3 input: overshell '%s' FAILED: no BandUser for synth user\n",
                 ov.action.c_str());
        return;
    }
    OvershellPanel *ovp = TheBandUI.GetOvershell();
    if (!ovp) {
        MILO_LOG("RB3 input: overshell '%s' FAILED: no OvershellPanel\n",
                 ov.action.c_str());
        return;
    }
    OvershellSlot *slot = ovp->GetSlot(bu);
    if (!slot) {
        MILO_LOG("RB3 input: overshell '%s' FAILED: no OvershellSlot for BandUser %p\n",
                 ov.action.c_str(), (void *)bu);
        return;
    }
    const std::vector<std::string> &args = ov.args;
    if (args.empty()) {
        MILO_LOG("RB3 input: overshell {%s}\n", ov.action.c_str());
        Message msg(Symbol(ov.action.c_str()));
        slot->Handle(msg, true);
    } else {
        DataArray *da = new DataArray((int)args.size() + 1);
        da->Node(0) = DataNode(Symbol(ov.action.c_str()));
        std::string argdump;
        for (size_t k = 0; k < args.size(); ++k) {
            const std::string &a = args[k];
            if (!a.empty() && (isdigit((unsigned char)a[0]) ||
                               (a[0] == '-' && a.size() > 1 && isdigit((unsigned char)a[1]))))
                da->Node((int)k + 1) = DataNode(atoi(a.c_str()));
            else
                da->Node((int)k + 1) = DataNode(Symbol(a.c_str()));
            argdump += " " + a;
        }
        MILO_LOG("RB3 input: overshell {%s%s}\n", ov.action.c_str(), argdump.c_str());
        slot->Handle(da, true);
        da->Release();
    }
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

    case kVerbPart: {
        // part:<sym> presses Confirm on the choose_part overshell view. Gate on
        // the real part_difficulty_screen being up AND the slot showing a
        // choose_part view — the exact state keyboard-to-gameplay.py waits for.
        if (!uiStable) { if (reason) *reason = "UI in transition / no screen"; return false; }
        if (strcmp(cur->Name(), "part_difficulty_screen") != 0) {
            if (reason) *reason = "not on part_difficulty_screen"; return false;
        }
        Symbol view = OvershellView();
        const char *vs = view.Str();
        if (!vs || strncmp(vs, "choose_part", 11) != 0) {
            if (reason) *reason = "overshell not in choose_part view yet"; return false;
        }
        return true;
    }

    case kVerbDiff: {
        // diff:<d> scrolls to + Confirms a difficulty on the choose_diff view.
        if (!uiStable) { if (reason) *reason = "UI in transition / no screen"; return false; }
        Symbol view = OvershellView();
        if (view != Symbol("choose_diff")) {
            if (reason) *reason = "overshell not in choose_diff view yet"; return false;
        }
        return true;
    }

    case kVerbNoFail: {
        // nofail gates on the song-load -> gameplay handoff: a live
        // MetaPerformer::Current() exists only once the performance is set up.
        if (!MetaPerformer::Current()) { if (reason) *reason = "no MetaPerformer (song not loaded)"; return false; }
        return true;
    }

    case kVerbAutohit: {
        // autohit needs gameplay to be FULLY live, not merely a constructed Game.
        // SetAutoplay(true) -> BeatMatcher::SetAutoplay -> mWatcher->SetCheating().
        // BeatMatcher::mWatcher is null until BeatMatcher::SetTrack runs, which
        // happens in Game::PostLoad (the async MIDI-parse + audio-bring-up chain
        // driven by Game::IsLoaded). The Game ctor finishes at tv3/game_screen
        // with mLoadState==kLoadingSong and mWatcher==NULL, so firing autohit the
        // instant a player exists derefs a null TrackWatcher -> wasm "memory
        // access out of bounds". Gate on Game::IsLoaded() (== PostLoad ran,
        // matchers wired, watchers built) so SetAutoplay lands on a live watcher.
        if (!MetaPerformer::Current()) { if (reason) *reason = "no MetaPerformer (song not loaded)"; return false; }
        if (!TheGame || TheGame->GetActivePlayers().empty()) {
            if (reason) *reason = "no active players yet";
            return false;
        }
        if (!TheGame->IsLoaded()) {
            if (reason) *reason = "song still loading (PostLoad/watchers not built)";
            return false;
        }
        return true;
    }

    case kVerbOvershell: {
        // overshell:<action> needs the synth user's OvershellSlot to exist.
        // The slot is allocated when the user joins the overshell (part flow
        // entered) — GetSlot(bu) resolves it once that has happened.
        LocalUser *user = SynthUser();
        BandUser *bu = (TheBandUserMgr && user) ? TheBandUserMgr->GetBandUser(user) : nullptr;
        if (!bu) { if (reason) *reason = "BandUser for synth user not ready"; return false; }
        OvershellPanel *ovp = TheBandUI.GetOvershell();
        if (!ovp) { if (reason) *reason = "OvershellPanel not available"; return false; }
        if (!ovp->GetSlot(bu)) { if (reason) *reason = "OvershellSlot not allocated for BandUser"; return false; }
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
    case kVerbPart:   return "part";
    case kVerbNoFail: return "nofail";
    case kVerbAutohit: return "autohit";
    case kVerbDiff:   return "diff";
    case kVerbOvershell: return "overshell";
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
    case kVerbNoFail: ExecNoFail();                        break;
    case kVerbAutohit: ExecAutohit();                      break;
    case kVerbOvershell: ExecOvershellVerb(v.overshell);   break;
    // part:/diff: are multi-frame pad verbs driven by StepPadVerb from the poll
    // loop, never dispatched here (listed so the switch is exhaustive).
    case kVerbPart:
    case kVerbDiff:   break;
    }
}

// Drive one step of a multi-frame pad verb (part:/diff:). Returns true when the
// verb is fully complete (the queue cursor may advance). Called every frame the
// cursor points at a started pad verb; advances at most one step per frame, and
// only when the pad queue is idle (a press has fully drained to a clean release).
// gPadVerb.phase must be kPadPhaseAct on the first call (set by the poll loop).
bool StepPadVerb(const Verb &v, int frame) {
    // Never step while a press is still in flight — StepPadVerb self-paces on the
    // pad queue exactly as keyboard-to-gameplay.py's drain_pad does.
    if (RB3JoypadPadQueueBusy())
        return false;

    switch (gPadVerb.phase) {
    case kPadPhaseAct:
        PadVerbEnqueuePrimary(v);
        MILO_LOG("RB3 input: %s -> enqueued primary pad presses (frame %d)\n",
                 VerbName(v), frame);
        gPadVerb.phase = kPadPhaseDrain;
        return false;

    case kPadPhaseDrain: {
        // Queue idle (guarded above). If a part-denial / diff-confirm dialog is
        // up, dismiss it with another Confirm — bounded to 4 retries so a stuck
        // dialog can never hang the queue.
        Symbol view = OvershellView();
        if (view == Symbol("confirm_action") && gPadVerb.retries < 4) {
            RB3JoypadEnqueuePad(kPadBitConfirm);
            gPadVerb.retries++;
            MILO_LOG("RB3 input: %s -> confirm_action dialog, Confirm (retry %d)\n",
                     VerbName(v), gPadVerb.retries);
            return false;  // stay in Drain; re-check after this Confirm drains
        }
        MILO_LOG("RB3 input: %s complete (view='%s' retries=%d frame %d)\n",
                 VerbName(v), view.Str() ? view.Str() : "(none)",
                 gPadVerb.retries, frame);
        return true;
    }
    default:
        return true;
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
    // part:<sym> — confirm a part on the choose_part overshell view via a REAL
    // Confirm pad press. Multi-frame, so it is SCHEDULED onto the verb queue
    // (minFrame 0 = fire as soon as the choose_part view is up) and driven by the
    // per-frame StepPadVerb state machine, not executed synchronously here. Only
    // `guitar` is supported today.
    if (verb.rfind("part:", 0) == 0) {
        std::string sym = verb.substr(5);
        if (sym != "guitar") {
            if (err) *err = "unsupported part '" + sym + "' (only 'guitar')";
            return false;
        }
        Verb pv; pv.kind = kVerbPart; pv.minFrame = 0; pv.origIndex = (int)gVerbs.size();
        gVerbs.push_back(pv);
        return true;
    }
    // diff:<d> — pick a difficulty on the choose_diff overshell view via REAL
    // DDown + Confirm pad presses. Scheduled onto the verb queue like part:.
    if (verb.rfind("diff:", 0) == 0) {
        Difficulty d;
        if (!ParseDifficulty(verb.substr(5), d)) {
            if (err) *err = "bad diff '" + verb.substr(5) + "' (easy/medium/hard/expert/0-3)";
            return false;
        }
        Verb pv; pv.kind = kVerbDiff; pv.minFrame = 0; pv.origIndex = (int)gVerbs.size();
        pv.diff = d;
        gVerbs.push_back(pv);
        return true;
    }
    // pad:<bit> — enqueue a RAW joypad button press (kPad_* index) into the
    // headless pad queue (rb3_joypad_native.cpp). Unlike `confirm`/`start`/etc.
    // (which inject a ButtonDownMsg straight into TheUI.Handle via ExecButton),
    // this drives the REAL SendButtonMessages broadcast with a clean 0->1->0
    // edge — the faithful equivalent of a physical guitar button press. Use it
    // to exercise menu nav (focus routing, override-flow SELECT_MSG dispatch,
    // slot input gating) exactly as a controller would, with no input aids.
    if (verb.rfind("pad:", 0) == 0) {
        int bit = atoi(verb.c_str() + 4);
        if (!RB3JoypadEnqueuePad(bit)) {
            if (err) *err = "pad queue full or bad bit '" + verb.substr(4) + "'";
            return false;
        }
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
    // C3: `quit` — request a clean exit. The HX_NATIVE frame loop breaks on the
    // next iteration, so the App dtor's TheDebug.Exit fires the save callbacks
    // (RB3SaveSaveGlobalOptions). Lets a headless test persist + exit code 0
    // without SIGTERM (which skips the callback chain).
    if (verb == "quit") {
        gCleanExitRequested = true;
        MILO_LOG("RB3 input: clean-exit requested\n");
        return true;
    }
    // C3: `nav:cal` — sugar over {ui goto_screen cal_welcome_screen}. Routes the
    // A/V calibration welcome screen through the same ExecMsg goto_screen path as
    // msg:ui:goto_screen:cal_welcome_screen. Optional ergonomic alias.
    if (verb == "nav:cal") {
        ScriptedMsg sm;
        sm.object = "ui";
        sm.action = "goto_screen";
        sm.args.push_back("cal_welcome_screen");
        ExecMsg(sm, cur);
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
    // C6: overshell:<action>[:arg...] — OvershellSlot navigation (pause/options).
    if (verb.rfind("overshell:", 0) == 0) {
        std::string rest = verb.substr(10);
        std::vector<std::string> parts;
        size_t p = 0;
        while (p <= rest.size()) {
            size_t c = rest.find(':', p);
            if (c == std::string::npos) { parts.push_back(rest.substr(p)); break; }
            parts.push_back(rest.substr(p, c - p));
            p = c + 1;
        }
        if (parts.empty() || parts[0].empty()) {
            if (err) *err = "overshell verb needs an action (e.g. overshell:show_options)";
            return false;
        }
        ScriptedOvershell so;
        so.action = parts[0];
        for (size_t k = 1; k < parts.size(); ++k)
            so.args.push_back(parts[k]);
        ExecOvershellVerb(so);
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

// rock_central — the online Rock Central service (RockCentral.cpp, public
// MsgSource). The real TheRockCentral GLOBAL is constructed, but its Init()
// (RockCentral.cpp:115 — SetName("rock_central", sMainDir) + ContextWrapperPool
// + AddSinks) is gated `#ifndef HX_NATIVE` at its sole caller (App.cpp:298), so
// natively the object is never NAMED in sMainDir. DTA scripts that address it —
// song_select.dta:38 `{rock_central add_sink $this (server_status_changed)}`,
// :99 `{rock_central remove_sink ...}`, :976 / song_select_extras.dta:375
// `{rock_central is_online}` — therefore resolve to nothing: DataNode::GetObj
// FindObject returns null and the engine emits the benign "rock_central not
// function or object" NOTIFY (reproduced: no crash, but the song_select_panel's
// sink bookkeeping silently no-ops and is_online falls through to a default 0).
//
// We register a minimal NativeRockCentralStub : MsgSource under that name. It
// answers the offline-meaningful queries with the same values the real
// RockCentral would offline (RockCentral.cpp:1885 is_online => IsOnline() =>
// mState==2 => false; ForceLogout()/BlockLoginToggle() are no-ops at mState 0),
// and delegates `add_sink`/`remove_sink` (and anything else) to MsgSource::Handle
// so the panel's `add_sink $this (server_status_changed)` registers a real sink
// on a real (empty) sink list — proper bookkeeping, never a SIGILL. Mirrors the
// NativeSaveLoadStub / NativeNetCacheMgrStub precedent; offline-safe, never
// reaches any Wii net/Quazal code (that all lived behind the gated Init()).
class NativeRockCentralStub : public MsgSource {
public:
    virtual DataNode Handle(DataArray *msg, bool warn) {
        Symbol s = msg->Sym(1);
        // Offline answers (match the real RockCentral::Handle's offline values).
        if (s == "is_online")          return DataNode(0); // signed-out / offline
        if (s == "state")              return DataNode(0); // mState == 0 (idle)
        if (s == "force_logout")       return DataNode(0); // no-op at mState 0
        if (s == "toggle_block_login") return DataNode(0); // bool flip, harmless
        if (s == "block_login")        return DataNode(0);
        // add_sink / remove_sink (+ everything else) go through the MsgSource
        // base: real sink-list bookkeeping on a properly-constructed (empty) list.
        return MsgSource::Handle(msg, warn);
    }
};

} // namespace

// C3: clean-exit request flag. A `quit` input verb sets it; App's HX_NATIVE
// frame loop polls RB3CleanExitRequested() and breaks out, returning from
// RunWithoutDebugging() so App::~App() -> TheDebug.Exit(0,true) fires the exit
// callbacks (incl. RB3SaveSaveGlobalOptions). SIGTERM/SIGKILL skip that chain,
// so the persistence harness MUST use this verb (or MILO_MAX_FRAMES) to exit.
bool RB3CleanExitRequested() { return gCleanExitRequested; }

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
    // rock_central: the real TheRockCentral global exists but is never NAMED
    // natively (its Init() is HX_NATIVE-gated). Register an offline MsgSource
    // stub so song_select.dta's `{rock_central add_sink/remove_sink/is_online}`
    // resolve to a real (offline-safe) object instead of the "not function or
    // object" NOTIFY. registerStub no-ops if a real singleton ever claims it.
    registerStub("rock_central",  new NativeRockCentralStub());

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

// (Former RB3SongSelectHideAlbumSmear removed.) The song-select "etched glass"
// album-region hide is gone: with the engine now honoring RndPostProc per-screen
// (postproc-rtt, engine c70be2a) plus working char-bone skinning + mesh-RTT, the
// etched-art groups and the album cover render correctly. Verified hide-OFF on the
// web swapchain with a real cover (The Beautiful People / Marilyn Manson): cover
// present, no center smear, etched groups showing. See RTT_HACK_UNWIND_ROADMAP.md
// (Stage 3) and OFFSCREEN_RTT_INVESTIGATION.md.

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
            // Pure-keyboard is the DEFAULT: a raw Confirm flows through
            // SendButtonMessages (JoypadPoll) exactly like native, so song-select
            // confirm, choose_part, choose_diff and ready all work as real button
            // presses now that the engine gates are fixed (offline-guest overshell
            // join, single-user part-resolve loop, RG chord_name overflow, song-DB
            // path). The one remaining opt-in aid is the song_select
            // select_highlighted_node confirm (+ rb3WebTargetSong highlight pin),
            // kept ONLY for the deterministic capture harness via
            // window.rb3WebUseAids=1; off by default so menu nav is pure keyboard.
            // (The old part_difficulty skip sequence — track:/end_override_flow —
            // is GONE; part/difficulty is now crossed by real pad presses on web
            // too, either the player's keyboard or the part:/diff: pad verbs.)
            bool useAids = (bool)EM_ASM_INT({ return (window.rb3WebUseAids ? 1 : 0); });
            bool onSongSelect = useAids && webCur && strcmp(webScr, "song_select_screen") == 0;
            for (int i = 0; i < kWebKeyMapSize; ++i) {
                if (newPressed & (1u << kWebKeyMap[i].bit)) {
                    MILO_LOG("RB3 web-input: frame %d  key bit=%d btn=%d action=%d"
                             " screen='%s'\n",
                             frame, kWebKeyMap[i].bit, (int)kWebKeyMap[i].btn,
                             (int)kWebKeyMap[i].action, webScr);
                    if (onSongSelect && kWebKeyMap[i].action == kAction_Confirm) {
                        // A Confirm on song_select_screen confirms a song via the
                        // same DTA handler the native RB3_GAME_INPUT script uses
                        // ({music_library select_highlighted_node $user}), which
                        // selects whatever the player has highlighted — i.e. the
                        // song their Up/Down navigation already moved the cursor to.
                        //
                        // We must NOT re-pin the highlight here in normal play: the
                        // original W3c code force-pinned it to 20thcenturyboy so the
                        // automated capture harness could reach one song
                        // deterministically, but that override clobbered real user
                        // selection (every confirm landed on 20th Century Boy). The
                        // pin is now opt-in ONLY: it fires when window.rb3WebTargetSong
                        // is set (the capture scripts set it). Empty default => no
                        // re-pin => the user's actual highlight is selected.
                        char tgt[64] = "";
                        EM_ASM({
                            var s = window.rb3WebTargetSong;
                            if (s && s.length && s.length < 63)
                                stringToUTF8(s, $0, 64);
                        }, tgt);
                        if (TheMusicLibrary && tgt[0]) {
                            MILO_LOG("RB3 web song-select: TEST override — pinning "
                                     "highlight to '%s' (frame %d)\n", tgt, frame);
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
                        // DOUBLE-FIRE GUARD (Phase 2): live keys now flow through
                        // the REAL joypad path — rb3_joypad_native.cpp's JoypadPoll()
                        // reads the SAME window._rb3Keys bitmask and calls
                        // SendButtonMessages(0, btns), which broadcasts ButtonDownMsg
                        // through gJoypadMsgSource -> TheUI's JoypadClient -> TheUI
                        // (the same menu nav this ExecButton->TheUI.Handle injection
                        // used to fake). Routing the key here too would deliver the
                        // menu event twice. So we no longer raw-inject menu keys;
                        // JoypadPoll owns them. The part_difficulty arming and the
                        // song_select select_highlighted_node crossing above are NOT
                        // raw button injection (they arm a flow / send a DTA msg), so
                        // they stay and do not double-fire with SendButtonMessages.
                    }
                }
            }
        }
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

    // HACK/TODO(guest-profile, roadmap C11): once the splash OvershellSlot add-user
    // flow has parked pad 0 in kState_JoinedDefault and we have REACHED main_hub,
    // install the fake pad-0 guest profile (one-shot inside the callee). Doing it
    // earlier would flip GetProfileForUser non-null DURING the add-user flow and
    // route the user to kState_ChooseProfile, stalling splash->main_hub.
    if (cur && strcmp(cur->Name(), "main_hub_screen") == 0) {
        extern void RB3InstallGuestProfile();
        RB3InstallGuestProfile();
    }

    // N4 stray SAVE/details pane: RETIRED. The `song_select_details` sub-pane was
    // left showing with details_mode=0 because the original native engine (pin
    // cfaaa5bc) ran AnimTask::Poll with the SetFrame args swapped — the
    // `details_hide.trg` PropAnim was advanced with frame=blend(=1.0) instead of
    // its terminal end frame, so the terminal `showing=FALSE` BoolKeys keyframe
    // never applied. The W6-V2 arg-swap fix (Anim.cpp ca671682, HX_NATIVE
    // `SetFrame(frame, blend)`) corrected that, so the triggered-terminal keyframe
    // now applies and the engine hides the pane on its own (verified: the nested
    // probe `{{song_select_panel find song_select_details} showing}` returns 0
    // after `hide_details` / `details_hide.trg`). The force-hide glue is therefore
    // redundant and has been removed.

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

        // A multi-frame pad verb (part:/diff:) that has already begun is driven
        // every frame until StepPadVerb reports complete — bypassing the
        // readiness/settle gate below, because its target overshell view CHANGES
        // as it progresses (choose_part -> choose_diff -> confirm_action ...), so
        // re-checking VerbReady mid-sequence would spuriously fail. StepPadVerb
        // self-paces on the pad queue and is internally bounded, so it always
        // completes (no timeout needed once started).
        if ((v.kind == kVerbPart || v.kind == kVerbDiff)
            && gPadVerb.cursor == (int)gVerbCursor) {
            if (StepPadVerb(v, frame)) {
                gLastFiredFrame = frame;
                gVerbCursor++;
                gVerbWaitSince = -1;
                gPadVerb = PadVerbState();   // reset for the next pad verb
            }
            goto verb_dispatch_done;         // one verb-action per frame
        }

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
                if (v.kind == kVerbPart || v.kind == kVerbDiff) {
                    // Begin the multi-frame pad verb; StepPadVerb drives it from
                    // here (this frame fires its first step: enqueue the presses).
                    gPadVerb = PadVerbState();
                    gPadVerb.cursor = (int)gVerbCursor;
                    gPadVerb.phase  = kPadPhaseAct;
                    if (StepPadVerb(v, frame)) {   // (won't complete on frame 1)
                        gVerbCursor++;
                        gVerbWaitSince = -1;
                        gPadVerb = PadVerbState();
                    }
                    gLastFiredFrame = frame;
                } else {
                    DispatchVerb(v, cur);
                    gLastFiredFrame = frame;
                    gVerbCursor++;
                    gVerbWaitSince = -1;
                }
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
verb_dispatch_done: ;

    // M4 GAP 2: re-apply recorded run aids (autohit/nofail) on replay. The aids
    // were toggled out-of-band at record time (HTTP verb / script), so they are
    // NOT in the replayed `in` stream — without this a recorded autoplay score
    // (e.g. 110) replays to 0. RB3ReplayPendingAids returns the aids whose recorded
    // frame has been reached (one-shot per aid). We route each through the SAME
    // ExecAutohit/ExecNoFail path the live HTTP verb drives, so the autoplay re-
    // arms identically -> same gem hits -> same score. Cheap no-op for aid-free
    // traces (RB3ReplayHasAids gate). ExecAutohit self-gates on player readiness;
    // if a player isn't ready yet at the recorded frame, RB3ReplayPendingAids has
    // already marked it applied, so we retry-arm here on the next due frames via a
    // pending-until-effective latch.
    if (RB3ReplayHasAids()) {
        const char *due[4];
        int nd = RB3ReplayPendingAids(frame, due, 4);
        for (int i = 0; i < nd; ++i) {
            bool ok = false;
            if (std::strcmp(due[i], "autohit") == 0)      ok = ExecAutohit();
            else if (std::strcmp(due[i], "nofail") == 0)  ok = ExecNoFail();
            else                                          ok = true;  // unknown: latch
            if (ok) {
                MILO_LOG("RB3 input: frame %d  REPLAY aid '%s' applied\n", frame, due[i]);
                RB3ReplayMarkAidApplied(due[i]);   // latch one-shot once effective
            }
            // else: not ready yet (load-skew) -> stays pending, retried next frame.
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
