// rb3 native — REAL joypad input path (keyboard + USB gamepad) for the port.
//
// Today the engine's working keyboard/gamepad impl (milo-native-engine's
// platform/Joypad_Native.cpp) is EXCLUDED for RB3 because it is DC3-shaped
// (mNumAnalogSticks / JoypadTerminateCommon / kAction_ShellOption / TheUI->),
// and JoypadInit/Poll/Reset/Terminate resolve to weak no-op stubs
// (native/src/dta_link_stubs.s:88-123). So SystemPoll(false) -> JoypadPoll()
// (System.cpp:673, called from App::RunOneFrame) does nothing: JoypadData::
// mButtons never changes, SendButtonMessages is never called, and no
// ButtonDownMsg/ButtonUpMsg ever reach the gameplay GuitarController — you
// cannot strum a note.
//
// This TU supplies STRONG defs that override those weak stubs (same mechanism
// the existing TheSynth / SampleInst / KeyChain glue uses). JoypadPoll() reads
// the keyboard (desktop: GLFW; web: window._rb3Keys) into a JoypadButton
// bitmask and calls SendButtonMessages(0, btns) (Joypad.cpp:321) — the single
// engine broadcast chokepoint that feeds BOTH menu nav (focused UIScreen) AND
// the gameplay GuitarController (already JoypadSubscribe'd). One path, one
// poll: live menu + live gameplay, no plastic instrument.
//
// KEYMAP / BREED — derived from the LIVE config chain in this port.
//
//   The DTA preprocessor define is `HX_WII` (System.cpp:223 calls
//   DataSetMacro("HX_WII") unconditionally — matched-fork Wii decomp code we
//   must not touch). So the loaded SystemConfig("joypad") `controllers` /
//   `button_meanings` / `adapters` arrays are the HX_WII branch, whose guitar
//   breeds are `wii_guitar`, `wii_guitar_rb2`, `wii_roguitar` — NOT the Xbox
//   `ro_guitar` family. The per-frame GuitarController::Poll + ::GetWhammyBar
//   both do `SystemConfig("joypad")->FindArray("controllers", <breed>)` with the
//   FATAL (two-arg) FindArray, so the pinned breed MUST live in that HX_WII
//   `controllers` block or gameplay OSFatals every frame. => we pin `wii_guitar`.
//
//   BUT the loaded `controller_mapping` / `instrument_mapping` arrays come from a
//   DIFFERENT, Xbox-flavoured joypad.dta fragment merged under (joypad ...) (via
//   band_keep.dta's `#include joypad.dta`). That fragment only lists Xbox/PS3
//   breeds and has NO `wii_guitar`, so GameConfig::GetController (controller_
//   mapping) and LocalBandUser::ConnectedControllerType (instrument_mapping)
//   would assert. This is a pure ASSET inconsistency (HX_WII controllers vs
//   Xbox mappings). Since we can't edit assets / matched-fork / the engine, the
//   glue injects the two missing mapping rows at JoypadInit time (EnsureWii
//   GuitarMapped below): `(wii_guitar guitar)` into controller_mapping and
//   `(wii_guitar kControllerGuitar)` into instrument_mapping. Runtime DataArray
//   wiring only — no source/asset edits.
//
//   With the rows present the chain is:
//   - JoypadControllerTypePadNum(0) returns the pinned `wii_guitar`.
//   - controller_mapping maps `wii_guitar` -> instrument `guitar`, so
//     GemPlayer::ResetController -> NewController builds a 5-lane GuitarController
//     whose cfg is beatmatch_controller.dta's `guitar` block:
//        (slots kPad_R2 0  kPad_Circle 1  kPad_Tri 2  kPad_X 3  kPad_Square 4)
//        (force_mercury kPad_Select)
//     i.e. frets target JoypadButtons R2/Circle/Tri/X/Square (NOT the menu's
//     X=confirm meaning — these are the controller's `slots`). The existing
//     keymap below already targets exactly these slots, so it is unchanged.
//   - instrument_mapping maps `wii_guitar` -> kControllerGuitar.
//   - Strum is GuitarController's default mStrumBarButtons = kPad_DUp/kPad_DDown.
//   - Star power / overdrive ("mercury") = force_mercury = kPad_Select.
//
//   Desktop keymap (conflict-free; menu actions come from the joypad
//   button_meanings, gameplay slots from beatmatch_controller.dta guitar):
//     Fret Green  (slot0) : 1 / A   -> kPad_R2     (menu: none)
//     Fret Red    (slot1) : 2 / S   -> kPad_Circle (menu: Cancel)
//     Fret Yellow (slot2) : 3 / D   -> kPad_Tri    (menu: Option)
//     Fret Blue   (slot3) : 4 / F   -> kPad_X      (menu: Confirm)
//     Fret Orange (slot4) : 5 / G   -> kPad_Square (menu: ShellOption)
//     Strum Up            : Up / J  -> kPad_DUp     (menu: Up)
//     Strum Down          : Down/ K -> kPad_DDown   (menu: Down)
//     Menu Left / Right   : Left/Rt -> kPad_DLeft / kPad_DRight
//     Menu Confirm        : Enter   -> kPad_X       (UIScreen Confirm)
//     Menu Cancel / back  : Backspc -> kPad_Circle  (UIScreen Cancel)
//     Start / Pause       : Esc     -> kPad_Start
//     Star Power / OD      : Tab     -> kPad_Select  (force_mercury / mercury)
//     PageUp / PageDown   : Q / E   -> kPad_L1 / kPad_R1
//     Whammy (held)       : Space   -> analog LY/RX (see note)
//   (Enter/Backspace reuse the blue/red fret buttons — exactly how a plastic
//   guitar navigates menus; harmless since a gameplay screen has no menu
//   buttons and a menu screen has no strum gems.)
//
//   Whammy: GuitarController::GetWhammyBar (GuitarController.cpp:86) picks the
//   axis from the controller cfg (ly_whammy -> GetLY, negative_rx_whammy_val ->
//   -GetRX, traditional_whammy_val -> -(GetRX()+1)/2), clamped min(0, val). The
//   HX_WII `wii_guitar` cfg carries TRADITIONAL_WHAMMY_VAL, so whammy reads
//   -(GetRX()+1)/2: holding Space drives RX = +1 -> whammy = -1 (engaged). We
//   drive both LY and RX from Space so the axis the cfg reads sees the value.
//
// Native-only glue — gated HX_NATIVE; no matched-fork (a) edits, no engine (b)
// edits. The Wii MWCC build never sees this file (HX_NATIVE undefined there);
// its JoypadPoll lives in the Wii-only Joypad_Wii.cpp.

#ifdef HX_NATIVE

#include "os/Joypad.h"
#include "os/JoypadMsgs.h"
#include "os/User.h"
#include "os/System.h"
#include "os/Debug.h"
#include "utl/Symbol.h"
#include "game/BandUser.h"
#include "game/BandUserMgr.h"
#include "game/Defines.h"   // ControllerType / kControllerGuitar

#ifdef __EMSCRIPTEN__
#include <emscripten/em_asm.h>
#else
// gBandRnd.Gpu().Window() — the GLFW window for desktop key polling. The RB3
// GPU backend header drags in webgpu_cpp.h; that is already on every native
// glue TU's include path (it links milo-engine).
#include "platform/Rnd_Wgpu_RB3.h"
#include <GLFW/glfw3.h>
#endif

#include <cstdlib>
#include <cstring>

// Defined non-static (external linkage) in os/Joypad.cpp:321 but not declared
// in any header — the engine broadcast chokepoint we drive.
void SendButtonMessages(int pad, unsigned int btns);

namespace {

// --- v1 keymap → JoypadButton bits (per the config chain documented above) ---
// Fret slots (from beatmatch_controller.dta `guitar` block `slots`):
const unsigned int kBtnFret0 = 1u << kPad_R2;     // green
const unsigned int kBtnFret1 = 1u << kPad_Circle; // red
const unsigned int kBtnFret2 = 1u << kPad_Tri;    // yellow
const unsigned int kBtnFret3 = 1u << kPad_X;      // blue
const unsigned int kBtnFret4 = 1u << kPad_Square; // orange
// Strum (GuitarController default mStrumBarButtons):
const unsigned int kBtnStrumUp   = 1u << kPad_DUp;
const unsigned int kBtnStrumDown = 1u << kPad_DDown;
// Star power / overdrive (force_mercury == kPad_Select; menu ViewModify):
const unsigned int kBtnStar = 1u << kPad_Select;
// Pause / menu Start:
const unsigned int kBtnStart = 1u << kPad_Start;
// Menu confirm / cancel (reuse the blue/red fret buttons, plastic-guitar style):
const unsigned int kBtnConfirm = 1u << kPad_X;      // == fret blue (slot3)
const unsigned int kBtnCancel  = 1u << kPad_Circle; // == fret red  (slot1)
// Extra menu d-pad:
const unsigned int kBtnDLeft   = 1u << kPad_DLeft;
const unsigned int kBtnDRight  = 1u << kPad_DRight;
// Page up/down:
const unsigned int kBtnPageUp   = 1u << kPad_L1;
const unsigned int kBtnPageDown = 1u << kPad_R1;

bool sLibInit = false;     // JoypadInit ran (JoypadInitCommon done)
bool sWebInputInit = false; // web JS listeners installed

// --- Runtime pad-press queue (headless pure-keyboard harness) ----------------
// A `pad:<bit>` HTTP verb enqueues a single button bit here; JoypadPoll() holds
// it down for kPadHoldPolls polls, then forces a clean release for kPadGapPolls
// polls before the next queued press, so SendButtonMessages edge-detects a real
// 0->1->0 press cycle (the same a physical Wii guitar button generates). This is
// the FAITHFUL headless equivalent of a keypress — it drives SendButtonMessages,
// NOT TheUI.Handle()/ExecButton — so menu nav (focus routing, override-flow
// SELECT_MSG dispatch, slot input gating) is exercised exactly as a real
// controller would. The script-driven RB3_JOYPAD_SEQ remains for fixed runs;
// this queue is for adaptive HTTP-driven runs (press, then poll state, repeat).
const int kPadHoldPolls = 4;   // polls a press is held down (matches SEQ hold)
const int kPadGapPolls  = 3;   // forced-release polls AFTER a press (clean edge)

struct PadPress { int bit; };
PadPress sPadQueue[128];
volatile int sPadQHead = 0;    // next slot to consume
volatile int sPadQTail = 0;    // next slot to fill (enqueue)
int sPadHoldLeft = 0;          // polls remaining in the current hold
int sPadGapLeft  = 0;          // polls remaining in the current release gap
int sPadCurBit   = -1;         // bit currently held (-1 == none)

} // namespace (reopened below)

// External enqueue — called from the HTTP input verb (`pad:<bit>`), main thread.
// Returns false if the queue is full. Bits are JoypadButton indices (kPad_*).
extern "C" bool RB3JoypadEnqueuePad(int bit) {
    if (bit < 0 || bit >= kPad_NumButtons)
        return false;
    int next = (sPadQTail + 1) % 128;
    if (next == sPadQHead)
        return false; // full
    sPadQueue[sPadQTail].bit = bit;
    sPadQTail = next;
    return true;
}

// True while the pad queue still has presses pending or a hold/gap in flight —
// lets the harness know when a press has fully drained (clean release seen).
extern "C" bool RB3JoypadPadQueueBusy() {
    return sPadQHead != sPadQTail || sPadHoldLeft > 0 || sPadGapLeft > 0;
}

namespace {

// Add the two `wii_guitar` rows the HX_WII config is missing from its
// controller_mapping / instrument_mapping (see the BREED note up top). The
// loaded SystemConfig("joypad") has HX_WII `controllers` (which DO list
// wii_guitar) but an Xbox-flavoured controller_mapping/instrument_mapping that
// does NOT — so GameConfig::GetController + ConnectedControllerType would
// OSFatal on the pinned breed. We inject the rows once, idempotently, as a
// 2-node child array each (mirroring the dta `(wii_guitar guitar)` /
// `(wii_guitar kControllerGuitar)` rows). Runtime DataArray wiring only.
void EnsureWiiGuitarMapped() {
    static const Symbol kWiiGuitar("wii_guitar");
    DataArray *joypad = SystemConfig(Symbol("joypad"));
    if (!joypad)
        return;
    // controller_mapping: (wii_guitar guitar)
    DataArray *cm = joypad->FindArray(Symbol("controller_mapping"), false);
    if (cm && !cm->FindArray(kWiiGuitar, false)) {
        DataArray *row = new DataArray(2);
        row->Node(0) = DataNode(kWiiGuitar);
        row->Node(1) = DataNode(Symbol("guitar"));
        cm->Insert(cm->Size(), DataNode(row, kDataArray));
        row->Release();
    }
    // instrument_mapping: (wii_guitar kControllerGuitar)  (kControllerGuitar==1)
    DataArray *im = joypad->FindArray(Symbol("instrument_mapping"), false);
    if (im && !im->FindArray(kWiiGuitar, false)) {
        DataArray *row = new DataArray(2);
        row->Node(0) = DataNode(kWiiGuitar);
        row->Node(1) = DataNode((int)kControllerGuitar);
        im->Insert(im->Size(), DataNode(row, kDataArray));
        row->Release();
    }
}

// Wire pad 0: associate the first local BandUser, mark connected, pin a guitar
// breed so JoypadControllerTypePadNum(0) -> wii_guitar -> NewController builds a
// GuitarController. Idempotent — runs once, and is compatible with the lazy
// SynthUser() wiring in rb3_game_input.cpp (same user, same pad, additive type).
void EnsurePad0Wired() {
    JoypadData *d = JoypadGetPadData(0);
    if (!d)
        return;
    if (!d->mUser && TheBandUserMgr) {
        std::vector<LocalBandUser *> &locals = TheBandUserMgr->GetLocalBandUsers();
        if (!locals.empty()) {
            AssociateUserAndPad(locals[0], 0);
            locals[0]->DebugSetControllerTypeOverride(kControllerGuitar);
        }
    }
    if (!d->mConnected)
        d->mConnected = true;
    // Pin the controller breed/type so the whole resolution chain is
    // deterministic. `wii_guitar` is the HX_WII 5-lane guitar breed present in
    // the loaded `controllers` block (its detect type is kJoypadWiiHxGuitar),
    // which the per-frame GuitarController::Poll/GetWhammyBar require. The
    // controller_mapping/instrument_mapping rows for it are injected by
    // EnsureWiiGuitarMapped() (called from JoypadInit).
    if (d->mControllerType.Null()) {
        d->mControllerType = Symbol("wii_guitar");
        d->mType = kJoypadWiiHxGuitar;
    }
}

#ifdef __EMSCRIPTEN__
// Install JS keydown/keyup listeners maintaining window._rb3Keys as a bitmask
// whose bits ARE JoypadButton values. rb3_game_input.cpp's InitWebInput also
// builds this map for the menu/d-pad bits; we extend it with the gameplay fret/
// strum keys so the SAME bitmask drives the real joypad path. Both listener
// sets stay installed simultaneously, so a key must not be mapped to a
// DIFFERENT bit in each set (it would OR both bits on every press). Fret keys
// therefore avoid a/s/d (claimed by InitWebInput's d-pad). We guard so we only
// add our extra keys once.
void InitWebGameplayKeys() {
    if (sWebInputInit)
        return;
    sWebInputInit = true;
    EM_ASM({
        if (!window._rb3Keys) window._rb3Keys = 0;
        var m = new Object();
        // Frets 1-5 (and A/S/D/F/G) -> the guitar `slots` JoypadButtons.
        // NB: do NOT alias a/s/d/A/S/D to frets here — rb3_game_input.cpp's
        // InitWebInput maps those same keys to the d-pad (DLeft/DDown/DRight,
        // bits 15/14/13). Both listener sets stay installed, so aliasing them
        // would OR a fret bit AND a d-pad/strum bit from one keypress: 's' =
        // red fret + DDown(strum) -> phantom overstrums (combo breaks) in
        // gameplay, a stuck red fret on song entry, and DDown auto-repeat
        // scrolling menus to the bottom. Frets use the digit row + f/g only.
        m['1'] = 1 << 1;   // kPad_R2  (green)
        m['2'] = 1 << 5;   // kPad_Circle (red)
        m['3'] = 1 << 4;   // kPad_Tri (yellow)
        m['4'] = 1 << 6;  m['f'] = 1 << 6;  m['F'] = 1 << 6;   // kPad_X   (blue)
        m['5'] = 1 << 7;  m['g'] = 1 << 7;  m['G'] = 1 << 7;   // kPad_Square (orange)
        // Strum: J/K mirror ArrowUp/ArrowDown (already mapped by InitWebInput).
        m['j'] = 1 << 12; m['J'] = 1 << 12;                    // kPad_DUp
        m['k'] = 1 << 14; m['K'] = 1 << 14;                    // kPad_DDown
        var consume = new Object();
        var keys = Object.keys(m);
        for (var i = 0; i < keys.length; i++) consume[keys[i]] = 1;
        document.addEventListener('keydown', function(e) {
            var bit = m[e.key];
            if (bit) { window._rb3Keys |= bit; if (consume[e.key]) e.preventDefault(); }
        }, true);
        document.addEventListener('keyup', function(e) {
            var bit = m[e.key];
            if (bit) { window._rb3Keys &= ~bit; }
        }, true);
        console.log('RB3 Web: gameplay keyboard input ready (frets/strum)');
    });
}

unsigned int ReadWebButtons() {
    return (unsigned int)EM_ASM_INT({ return window._rb3Keys || 0; });
}

// navigator.getGamepads() pad 0 -> JoypadButton bitmask (mirror engine
// Joypad_Native.cpp GetWebGamepadButtons; standard-mapping face buttons).
unsigned int ReadWebGamepadButtons() {
    return (unsigned int)EM_ASM_INT({
        var gps = navigator.getGamepads ? navigator.getGamepads() : [];
        var gp = gps[0];
        if (!gp || !gp.connected) return 0;
        var b = 0;
        var btn = gp.buttons;
        if (btn[0] && btn[0].pressed)  b |= (1 << 6);  // A  -> kPad_X
        if (btn[1] && btn[1].pressed)  b |= (1 << 5);  // B  -> kPad_Circle
        if (btn[2] && btn[2].pressed)  b |= (1 << 7);  // X  -> kPad_Square
        if (btn[3] && btn[3].pressed)  b |= (1 << 4);  // Y  -> kPad_Tri
        if (btn[4] && btn[4].pressed)  b |= (1 << 2);  // LB -> kPad_L1
        if (btn[5] && btn[5].pressed)  b |= (1 << 3);  // RB -> kPad_R1
        if (btn[6] && btn[6].value > 0.3) b |= (1 << 0); // LT -> kPad_L2
        if (btn[7] && btn[7].value > 0.3) b |= (1 << 1); // RT -> kPad_R2
        if (btn[8] && btn[8].pressed)  b |= (1 << 8);  // Back  -> kPad_Select
        if (btn[9] && btn[9].pressed)  b |= (1 << 11); // Start -> kPad_Start
        if (btn[12] && btn[12].pressed) b |= (1 << 12); // DUp
        if (btn[13] && btn[13].pressed) b |= (1 << 14); // DDown
        if (btn[14] && btn[14].pressed) b |= (1 << 15); // DLeft
        if (btn[15] && btn[15].pressed) b |= (1 << 13); // DRight
        return b;
    });
}
#endif // __EMSCRIPTEN__

} // namespace

// === Strong defs (override the weak no-op stubs in dta_link_stubs.s) =========

void JoypadInit() {
    if (sLibInit)
        return;
    sLibInit = true;
    // Populate gControllersCfg / gButtonMeanings / gJoypadMsgSource. Guarded by
    // gJoypadLibInitialized internally, so it is safe even if SynthUser() in
    // rb3_game_input.cpp already ran it.
    JoypadInitCommon(SystemConfig(Symbol("joypad")));
    // The loaded joypad config is HX_WII (controllers) but ships an Xbox-only
    // controller_mapping/instrument_mapping; add the wii_guitar rows so the
    // breed we pin resolves through GameConfig::GetController + instrument
    // lookup without OSFatal. Must run after the config is loaded.
    EnsureWiiGuitarMapped();
    JoypadReset();
}

void JoypadReset() {
    // Re-establish pad 0's user association + connected guitar breed. Do NOT
    // ResetAllUsersPads() here: the synth-user wiring in rb3_game_input.cpp may
    // already have bound pad 0, and blowing it away would desync gSynthUser.
    EnsurePad0Wired();
}

void JoypadTerminate() {
    // RB3 has no JoypadTerminateCommon; nothing to tear down natively.
}

void JoypadPoll() {
    // Make sure the lib config is up before the first broadcast (SendButtonMessages
    // needs gButtonMeanings/gJoypadMsgSource). SynthUser() may have run it; if
    // not, do it now.
    if (!sLibInit)
        JoypadInit();

    JoypadData *d = JoypadGetPadData(0);
    if (!d)
        return;
    // Until a local user exists + pad 0 is connected, there is nothing to drive.
    // (rb3_game_input.cpp's SynthUser() also connects pad 0; either path is fine.)
    if (!d->mConnected) {
        EnsurePad0Wired();
        if (!d->mConnected)
            return;
    }

    unsigned int btns = 0;
    bool whammyHeld = false;

    // Headless message-path proof (off by default): RB3_JOYPAD_TEST_BTN=<bit>
    // holds that JoypadButton bit down so SendButtonMessages broadcasts a
    // ButtonDownMsg to every subscribed sink (TheUI's JoypadClient, the gameplay
    // GuitarController) WITHOUT needing a $DISPLAY/GLFW window. Verify with
    // {joypad_is_button_down 0 <bit>} over /api/dta/eval, or watch the focus
    // change / GuitarController hit FX. Env-gated; no effect when unset.
    {
        static int sTestBtn = -2;
        if (sTestBtn == -2) {
            const char *e = getenv("RB3_JOYPAD_TEST_BTN");
            sTestBtn = e ? atoi(e) : -1;
        }
        if (sTestBtn >= 0 && sTestBtn < kPad_NumButtons) {
            SendButtonMessages(0, 1u << sTestBtn);
            return;
        }
    }

    // Headless SEQUENCE injector (off by default): RB3_JOYPAD_SEQ="f:bit,f:bit,..."
    // presses the given JoypadButton bit at poll-count f, holds it for a few polls,
    // then releases — so SendButtonMessages edge-detects a real press/release pair.
    // Drives the EXACT same real path (SendButtonMessages -> ButtonToAction ->
    // gJoypadMsgSource -> TheUI's JoypadClient) a live keyboard would, with no
    // window. Purely a test harness; env-gated; no effect when unset. Each press
    // is held kSeqHold polls so the UI sees a stable down then a clean up.
    {
        struct SeqEntry { int at; int bit; };
        static SeqEntry sSeq[64];
        static int sSeqCount = -2;
        static int sPoll = 0;
        const int kSeqHold = 4; // polls a press is held before release
        if (sSeqCount == -2) {
            sSeqCount = 0;
            const char *e = getenv("RB3_JOYPAD_SEQ");
            if (e && *e) {
                const char *p = e;
                while (*p && sSeqCount < 64) {
                    int at = atoi(p);
                    const char *colon = strchr(p, ':');
                    if (!colon) break;
                    int bit = atoi(colon + 1);
                    sSeq[sSeqCount].at = at;
                    sSeq[sSeqCount].bit = bit;
                    sSeqCount++;
                    const char *comma = strchr(colon, ',');
                    if (!comma) break;
                    p = comma + 1;
                }
                MILO_LOG("RB3 joypad SEQ: parsed %d entries (hold=%d)\n",
                         sSeqCount, kSeqHold);
            }
        }
        if (sSeqCount > 0) {
            unsigned int seqBtns = 0;
            for (int i = 0; i < sSeqCount; i++) {
                if (sPoll >= sSeq[i].at && sPoll < sSeq[i].at + kSeqHold &&
                    sSeq[i].bit >= 0 && sSeq[i].bit < kPad_NumButtons)
                    seqBtns |= 1u << sSeq[i].bit;
            }
            SendButtonMessages(0, seqBtns);
            sPoll++;
            return;
        }
    }

    // Runtime pad-press queue (the `pad:<bit>` HTTP verb). Drives a clean
    // 0->1->0 edge per queued press through the real SendButtonMessages path.
    // When empty AND idle this falls through to the normal keyboard read below,
    // so a windowed run is unaffected unless presses are queued.
    if (sPadHoldLeft > 0) {
        // Currently holding a press down.
        unsigned int bits = (sPadCurBit >= 0) ? (1u << sPadCurBit) : 0u;
        SendButtonMessages(0, bits);
        if (--sPadHoldLeft == 0) {
            sPadGapLeft = kPadGapPolls; // begin forced release
        }
        return;
    }
    if (sPadGapLeft > 0) {
        // Forced release between presses — guarantees a clean falling edge.
        SendButtonMessages(0, 0u);
        --sPadGapLeft;
        if (sPadGapLeft == 0)
            sPadCurBit = -1;
        return;
    }
    if (sPadQHead != sPadQTail) {
        // Pop the next queued press and begin its hold.
        sPadCurBit = sPadQueue[sPadQHead].bit;
        sPadQHead = (sPadQHead + 1) % 128;
        sPadHoldLeft = kPadHoldPolls;
        unsigned int bits = (sPadCurBit >= 0) ? (1u << sPadCurBit) : 0u;
        SendButtonMessages(0, bits);
        --sPadHoldLeft;
        if (sPadHoldLeft == 0)
            sPadGapLeft = kPadGapPolls;
        return;
    }

#ifdef __EMSCRIPTEN__
    InitWebGameplayKeys();
    btns = ReadWebButtons() | ReadWebGamepadButtons();
    // Web whammy: Space is mapped to kPad_Start in InitWebInput; treating a held
    // Start on a gameplay screen as whammy too is risky (it pauses), so for web
    // v1 we leave whammy to its axis default (RX/LY untouched here). Documented.
    (void)whammyHeld;
#else
    GLFWwindow *w = gBandRnd.Gpu().Window();
    if (!w)
        return; // headless (MILO_HEADLESS=1) — no window; HTTP/script harness drives input
    // --- USB gamepad (optional, Phase 4): face -> fret, dpad -> strum --------
    if (glfwJoystickIsGamepad(0)) {
        GLFWgamepadstate gp;
        if (glfwGetGamepadState(0, &gp)) {
            if (gp.buttons[GLFW_GAMEPAD_BUTTON_A]) btns |= kBtnFret0;
            if (gp.buttons[GLFW_GAMEPAD_BUTTON_B]) btns |= kBtnFret1;
            if (gp.buttons[GLFW_GAMEPAD_BUTTON_Y]) btns |= kBtnFret2;
            if (gp.buttons[GLFW_GAMEPAD_BUTTON_X]) btns |= kBtnFret3;
            if (gp.buttons[GLFW_GAMEPAD_BUTTON_RIGHT_BUMPER]) btns |= kBtnFret4;
            if (gp.buttons[GLFW_GAMEPAD_BUTTON_DPAD_UP])    btns |= kBtnStrumUp;
            if (gp.buttons[GLFW_GAMEPAD_BUTTON_DPAD_DOWN])  btns |= kBtnStrumDown;
            if (gp.buttons[GLFW_GAMEPAD_BUTTON_DPAD_LEFT])  btns |= kBtnDLeft;
            if (gp.buttons[GLFW_GAMEPAD_BUTTON_DPAD_RIGHT]) btns |= kBtnDRight;
            if (gp.buttons[GLFW_GAMEPAD_BUTTON_BACK])  btns |= kBtnStar;
            if (gp.buttons[GLFW_GAMEPAD_BUTTON_START]) btns |= kBtnStart;
            // Right trigger as whammy.
            if ((gp.axes[GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER] + 1.0f) * 0.5f > 0.3f)
                whammyHeld = true;
        }
    }
    // --- Keyboard (the headline path) ----------------------------------------
    if (glfwGetKey(w, GLFW_KEY_1) == GLFW_PRESS || glfwGetKey(w, GLFW_KEY_A) == GLFW_PRESS) btns |= kBtnFret0;
    if (glfwGetKey(w, GLFW_KEY_2) == GLFW_PRESS || glfwGetKey(w, GLFW_KEY_S) == GLFW_PRESS) btns |= kBtnFret1;
    if (glfwGetKey(w, GLFW_KEY_3) == GLFW_PRESS || glfwGetKey(w, GLFW_KEY_D) == GLFW_PRESS) btns |= kBtnFret2;
    if (glfwGetKey(w, GLFW_KEY_4) == GLFW_PRESS || glfwGetKey(w, GLFW_KEY_F) == GLFW_PRESS) btns |= kBtnFret3;
    if (glfwGetKey(w, GLFW_KEY_5) == GLFW_PRESS || glfwGetKey(w, GLFW_KEY_G) == GLFW_PRESS) btns |= kBtnFret4;
    if (glfwGetKey(w, GLFW_KEY_UP)   == GLFW_PRESS || glfwGetKey(w, GLFW_KEY_J) == GLFW_PRESS) btns |= kBtnStrumUp;
    if (glfwGetKey(w, GLFW_KEY_DOWN) == GLFW_PRESS || glfwGetKey(w, GLFW_KEY_K) == GLFW_PRESS) btns |= kBtnStrumDown;
    if (glfwGetKey(w, GLFW_KEY_LEFT)  == GLFW_PRESS) btns |= kBtnDLeft;
    if (glfwGetKey(w, GLFW_KEY_RIGHT) == GLFW_PRESS) btns |= kBtnDRight;
    // Menu Confirm / Cancel / Start / Star power / page (conflict-free):
    if (glfwGetKey(w, GLFW_KEY_ENTER)     == GLFW_PRESS) btns |= kBtnConfirm; // kPad_X
    if (glfwGetKey(w, GLFW_KEY_BACKSPACE) == GLFW_PRESS) btns |= kBtnCancel;  // kPad_Circle
    if (glfwGetKey(w, GLFW_KEY_ESCAPE)    == GLFW_PRESS) btns |= kBtnStart;   // pause/Start
    if (glfwGetKey(w, GLFW_KEY_TAB)       == GLFW_PRESS) btns |= kBtnStar;    // mercury/OD
    if (glfwGetKey(w, GLFW_KEY_Q)         == GLFW_PRESS) btns |= kBtnPageUp;
    if (glfwGetKey(w, GLFW_KEY_E)         == GLFW_PRESS) btns |= kBtnPageDown;
    // Whammy: hold Space (desktop). wii_guitar uses TRADITIONAL_WHAMMY_VAL ->
    // -(RX+1)/2, so driving RX below engages whammy; we set both axes so any
    // breed (ly_whammy / negative_rx_whammy / traditional) reads the value.
    if (glfwGetKey(w, GLFW_KEY_SPACE) == GLFW_PRESS) whammyHeld = true;

    // Drive both whammy axes so whichever the controller cfg reads sees it.
    // GetWhammyBar does min(0, axis); engaged = negative.
    d->mSticks[0][1] = whammyHeld ? -1.0f : 0.0f; // LY  (ly_whammy)
    d->mSticks[1][0] = whammyHeld ?  1.0f : 0.0f; // RX  (negative_rx_whammy -> -RX)
#endif // __EMSCRIPTEN__

    // The single broadcast: diffs against mButtons, fills mNewPressed/Released,
    // and sends ButtonDownMsg/ButtonUpMsg through gJoypadMsgSource to every
    // subscribed sink — menu (focused UIScreen) AND gameplay GuitarController.
    SendButtonMessages(0, btns);
}

#endif // HX_NATIVE
