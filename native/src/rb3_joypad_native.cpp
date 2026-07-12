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
#include "rb3_session_trace.h" // RB3RecordInput (session-telemetry input tap)
#include "rb3_replay.h"        // Tier-1 replay: override live input from a trace

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
bool sWebInputInit = false; // web gameplay-key JS listeners installed
bool sWebGuitarInit = false; // web USB-guitar (Gamepad API) JS installed

// ── SESSION-TELEMETRY input tap helper ───────────────────────────────────────
// Records one input edge into the trace at a SendButtonMessages(0, btns)
// chokepoint: the same `btns` the engine broadcasts. Called at EVERY real
// broadcast site in JoypadPoll — the keyboard/gamepad read AND the runtime
// pad-press queue (the `pad:<bit>` / HTTP-`/api/input` / RB3_GAME_INPUT verb
// path), so a HEADLESS replay/HTTP-driven session also records what the player
// (or the replay) did, not just a windowed keyboard run. Edge-only: the recorder
// drops the call when `btns` is unchanged, so calling it at multiple sites is
// safe (it dedups against the last RECORDED edge, not per-site). dn/up are
// computed here against a file-static prev. Whammy is read live from the LY stick
// (mSticks[0][1]); tilt is 0 (no native keybind yet — reserved). gRB3TraceActive
// gates inside RB3RecordInput → one predicted branch when tracing is off.
void RB3JoypadTraceInput(unsigned int btns, float whammy) {
    static unsigned int sPrevBtns = 0;
    // Suppress the leading all-zero poll(s) before the first real press. The
    // recorder's first-call has haveInput==false so it CANNOT dedup that opening
    // {b:0} edge, which would make the first recorded `in` a content-free null
    // row. On web the first poll is at frame 0 with nothing held, so the live
    // recording emitted a leading {f:0,b:0} that the Tier-1 replay override
    // deliberately does NOT (its sReplayStarted guard skips the leading zero) —
    // so a record-vs-replay trace-diff differed by exactly that one row. Waiting
    // for the first non-zero bitmask here makes record + replay start at the SAME
    // first real edge (the replay override's documented "trace-diff exit 0" goal),
    // and drops only an information-free null edge (no button state lost).
    static bool sStarted = false;
    if (!sStarted) {
        if (btns == 0)
            return;
        sStarted = true;
    }
    RB3RecordInput(0, btns, btns & ~sPrevBtns, ~btns & sPrevBtns, whammy, 0.0f);
    sPrevBtns = btns;
}

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
        // Frets: the digit row 1-5 AND a/s/d/f/g -> the guitar `slots`
        // JoypadButtons. Aliasing a/s/d is safe because InitWebInput
        // (rb3_game_input.cpp) navigates menus with the ARROW keys, not WASD — so
        // across BOTH installed listener sets no key maps to a fret bit in one and
        // a d-pad/strum bit in the other (the collision that caused a stuck red
        // fret + phantom strums). Each key -> exactly one bit.
        m['1'] = 1 << 1;  m['a'] = 1 << 1;  m['A'] = 1 << 1;   // kPad_R2  (green)
        m['2'] = 1 << 5;  m['s'] = 1 << 5;  m['S'] = 1 << 5;   // kPad_Circle (red)
        m['3'] = 1 << 4;  m['d'] = 1 << 4;  m['D'] = 1 << 4;   // kPad_Tri (yellow)
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

// Install the USB-guitar (Gamepad API) support once: a per-family default
// mapping table, a device classifier, and gamepadconnected/disconnected
// listeners that log what plugged in (users report unknowns via the console).
//
// Classification (checked in this order):
//   guitar   — id matches /guitar|harmonix|santroller/i OR a known instrument
//              vendor hex (12ba = PS3/Wii RB dongle, 1bad = Harmonix Xbox,
//              0738 = MadCatz/RedOctane, 1430 = GH). Chrome id strings look
//              like "Harmonix Guitar for Nintendo Wii (Vendor: 12ba Product:
//              0100)", so the vendor substring match works.
//   standard — mapping === "standard" (non-guitar; existing face-button path).
//   unknown  — logged and ignored.
//
// Per-family button/axis indices are best-effort DEFAULTS derived from the
// documented HID report order (santroller reverse-engineering docs); real
// browsers renumber non-standard HID pads, so the exact indices vary by unit.
// window.rb3GuitarMap (shallow-merged over the family default) lets a user fix
// a mismatch from the console with no rebuild, and window._rb3GpDebug=1 prints
// the raw pressed-button indices + non-idle axis values to calibrate it.
void InitWebGuitar() {
    if (sWebGuitarInit)
        return;
    sWebGuitarInit = true;
    // NB: EM_ASM stringifies via the preprocessor, which splits on commas that
    // are NOT inside parentheses — so JS object/array LITERALS ({a:1, b:2} /
    // [x, y]) would be shredded into bogus macro args. Build every object with
    // `new Object()` + property assignment and avoid comma'd array literals.
    EM_ASM({
        // Per-family default guitar mappings. The mapped bits (RIGHT side of the
        // poll) are JoypadButton enum bits (green=1/R2, red=5/Circle,
        // yellow=4/Tri, blue=6/X, orange=7/Square, star=8/Select,
        // start=11/Start, strum up=12/DUp, down=14/DDown, dpad right=13/left=15).
        // The indices HERE are the browser Gamepad API button/axis indices.
        var F = new Object();
        // PS3 / Wii Rock Band guitar dongle (12ba:0100, "Harmonix Guitar for
        // Nintendo Wii"), non-standard mapping (gp.mapping === ""). HID button-bit
        // order Y=0,G=1,R=2,B=3,O=4,pedal=5,select=8,start=9. Strum/d-pad ride the
        // HID hat, which Chrome exposes as an extra axis (commonly axes[9]) with
        // 8-step fractional values.
        var ps = new Object();
        ps.green=1; ps.red=2; ps.yellow=0; ps.blue=3; ps.orange=4;
        ps.start=9; ps.select=8; ps.strumMode='hat'; ps.hatAxis=9;
        ps.whammyAxis=0; ps.whammyInvert=false;
        ps.tiltAxis=null; ps.tiltThreshold=0.5; ps.tiltButton=5;
        F['ps3wii_rb'] = ps;
        // Xbox 360 Rock Band guitar via xpad -> browser STANDARD mapping. Standard
        // face buttons A=0 green, B=1 red, X=2 blue, Y=3 yellow, LB=4 orange; d-pad
        // on buttons 12-15; whammy on right-stick X (axes[2]); tilt often axes[3].
        var xi = new Object();
        xi.green=0; xi.red=1; xi.yellow=3; xi.blue=2; xi.orange=4;
        xi.start=9; xi.select=8; xi.strumMode='buttons';
        xi.whammyAxis=2; xi.whammyInvert=false;
        xi.tiltAxis=3; xi.tiltThreshold=0.5; xi.tiltButton=null;
        F['xinput_rb'] = xi;
        // Generic Guitar Hero PS3 guitar (12ba / 1430 / 0738). Same HID family as
        // the RB dongle for our purposes.
        var gh = new Object();
        gh.green=1; gh.red=2; gh.yellow=0; gh.blue=3; gh.orange=4;
        gh.start=9; gh.select=8; gh.strumMode='hat'; gh.hatAxis=9;
        gh.whammyAxis=0; gh.whammyInvert=false;
        gh.tiltAxis=null; gh.tiltThreshold=0.5; gh.tiltButton=5;
        F['gh_ps3'] = gh;
        window._rb3GuitarFamilies = F;
        // Classify one Gamepad -> object with .kind and .family.
        window._rb3ClassifyPad = function(gp) {
            var id = (gp.id || '').toLowerCase();
            var isGuitar = /guitar|harmonix|santroller/.test(id) ||
                           /12ba|1bad|0738|1430/.test(id);
            var r = new Object();
            if (isGuitar) {
                r.kind = 'guitar';
                if (gp.mapping === 'standard') r.family = 'xinput_rb';
                else if (/1430|guitar hero/.test(id)) r.family = 'gh_ps3';
                else r.family = 'ps3wii_rb';
                return r;
            }
            r.family = null;
            r.kind = (gp.mapping === 'standard') ? 'standard' : 'unknown';
            return r;
        };
        window.addEventListener('gamepadconnected', function(e) {
            var gp = e.gamepad;
            var c = window._rb3ClassifyPad(gp);
            console.log('[rb3-guitar] connected idx=' + gp.index +
                ' id="' + gp.id + '" mapping="' + gp.mapping + '"' +
                ' buttons=' + gp.buttons.length + ' axes=' + gp.axes.length +
                ' -> ' + c.kind + (c.family ? (' (' + c.family + ')') : ''));
            if (c.kind === 'unknown')
                console.log('[rb3-guitar] unknown device — set window.rb3GuitarMap' +
                    ' and window._rb3GpDebug=1 to map it, and please report id above');
        });
        window.addEventListener('gamepaddisconnected', function(e) {
            console.log('[rb3-guitar] disconnected idx=' + e.gamepad.index +
                ' id="' + e.gamepad.id + '"');
        });
        console.log('[rb3-guitar] USB guitar support ready (Gamepad API)');
    });
}

unsigned int ReadWebButtons() {
    return (unsigned int)EM_ASM_INT({ return window._rb3Keys || 0; });
}

// navigator.getGamepads() -> JoypadButton bitmask. Prefers the first connected
// guitar-classified pad (frets/strum/star/start/d-pad per the family or
// window.rb3GuitarMap), decoding the HID hat from either an axis or buttons
// 12-15, and stashing the whammy (0..1, 0=rest) in window._rb3GpWhammy for the
// C++ side to read the SAME frame via ReadWebGamepadWhammy(). Tilt (axis
// threshold or dedicated button) is OR'd in as bit 8 (kPad_Select / star). If
// no guitar is present it falls back to the original standard-mapping
// face-button handling on pad 0 (unchanged), leaving whammy at rest.
unsigned int ReadWebGamepadButtons() {
    return (unsigned int)EM_ASM_INT({
        var gps = navigator.getGamepads ? navigator.getGamepads() : [];
        // Decode a Chrome-style hat axis (8-step fractional, idle out of [-1,1]).
        var decodeHat = function(v) {
            var h = new Object();
            h.up=false; h.down=false; h.left=false; h.right=false;
            if (v >= -1.1 && v <= 1.1) {
                var step = Math.round((v + 1) * 3.5); // -1..1 -> 0..7
                // 0=up 1=up-right 2=right 3=down-right 4=down 5=down-left 6=left 7=up-left
                if (step === 7 || step === 0 || step === 1) h.up = true;
                if (step === 1 || step === 2 || step === 3) h.right = true;
                if (step === 3 || step === 4 || step === 5) h.down = true;
                if (step === 5 || step === 6 || step === 7) h.left = true;
            }
            return h;
        };
        // Find the first connected guitar-classified pad.
        var guitar = null;
        if (window._rb3ClassifyPad) {
            for (var i = 0; i < gps.length; i++) {
                var g = gps[i];
                if (g && g.connected && window._rb3ClassifyPad(g).kind === 'guitar') {
                    guitar = g;
                    window._rb3GuitarFamily = window._rb3ClassifyPad(g).family;
                    break;
                }
            }
        }

        if (!guitar) {
            // No guitar: original standard-mapping face-button path on pad 0.
            window._rb3GpWhammy = 0;
            window._rb3GpTilt = 0;
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
        }

        // Guitar path: family default shallow-merged with a runtime override.
        var base = window._rb3GuitarFamilies[window._rb3GuitarFamily] ||
                   window._rb3GuitarFamilies['ps3wii_rb'];
        var fam = base;
        if (window.rb3GuitarMap) {
            fam = new Object();
            for (var k in base) fam[k] = base[k];
            for (var k2 in window.rb3GuitarMap) fam[k2] = window.rb3GuitarMap[k2];
        }
        var btns = guitar.buttons;
        var axes = guitar.axes;
        var pb = function(idx) {
            return (idx !== null && idx !== undefined &&
                    btns[idx] && btns[idx].pressed);
        };
        var m = 0;
        if (pb(fam.green))  m |= (1 << 1);   // kPad_R2     (green)
        if (pb(fam.red))    m |= (1 << 5);   // kPad_Circle (red)
        if (pb(fam.yellow)) m |= (1 << 4);   // kPad_Tri    (yellow)
        if (pb(fam.blue))   m |= (1 << 6);   // kPad_X      (blue)
        if (pb(fam.orange)) m |= (1 << 7);   // kPad_Square (orange)
        if (pb(fam.start))  m |= (1 << 11);  // kPad_Start
        if (pb(fam.select)) m |= (1 << 8);   // kPad_Select (star power)

        // Strum + d-pad: hat axis OR discrete buttons 12-15, per family.
        var hat;
        if (fam.strumMode === 'hat') {
            var hv = (fam.hatAxis !== null && fam.hatAxis !== undefined &&
                      axes.length > fam.hatAxis) ? axes[fam.hatAxis] : 2;
            hat = decodeHat(hv);
        } else {
            hat = new Object();
            hat.up = pb(12); hat.down = pb(13); hat.left = pb(14); hat.right = pb(15);
        }
        if (hat.up)    m |= (1 << 12); // kPad_DUp    (strum up)
        if (hat.down)  m |= (1 << 14); // kPad_DDown  (strum down)
        if (hat.left)  m |= (1 << 15); // kPad_DLeft
        if (hat.right) m |= (1 << 13); // kPad_DRight

        // Whammy axis -> 0..1 (0 = rest). Browser signed axis -1..1 -> (v+1)/2.
        var whammy01 = 0;
        if (fam.whammyAxis !== null && fam.whammyAxis !== undefined &&
            axes.length > fam.whammyAxis) {
            whammy01 = (axes[fam.whammyAxis] + 1) / 2;
            if (fam.whammyInvert) whammy01 = 1 - whammy01;
        }
        if (whammy01 < 0) whammy01 = 0;
        if (whammy01 > 1) whammy01 = 1;

        // Tilt (star power / overdrive): axis over threshold OR a button.
        var tilt = false;
        if (fam.tiltButton !== null && fam.tiltButton !== undefined && pb(fam.tiltButton))
            tilt = true;
        if (fam.tiltAxis !== null && fam.tiltAxis !== undefined &&
            axes.length > fam.tiltAxis && axes[fam.tiltAxis] > (fam.tiltThreshold || 0.5))
            tilt = true;
        if (tilt) m |= (1 << 8); // kPad_Select (force_mercury route)

        window._rb3GpWhammy = whammy01;
        window._rb3GpTilt = tilt ? 1 : 0;

        // Calibration aid: log raw pressed buttons + non-idle axes, on change.
        if (window._rb3GpDebug) {
            var pressed = [];
            for (var bi = 0; bi < btns.length; bi++)
                if (btns[bi] && btns[bi].pressed) pressed.push(bi);
            var ax = [];
            for (var ai = 0; ai < axes.length; ai++)
                if (Math.abs(axes[ai]) > 0.15) ax.push(ai + ':' + axes[ai].toFixed(2));
            var sig = pressed.join(',') + '|' + ax.join(',');
            if (sig !== window._rb3GpDbgLast) {
                window._rb3GpDbgLast = sig;
                console.log('[rb3-guitar] fam=' + window._rb3GuitarFamily +
                    ' btns[' + pressed.join(',') + '] axes[' + ax.join(',') + ']' +
                    ' whammy=' + whammy01.toFixed(2) + ' tilt=' + (tilt ? 1 : 0));
            }
        }
        return m;
    });
}

// Whammy for the guitar found in ReadWebGamepadButtons this frame: 0..1, 0=rest.
double ReadWebGamepadWhammy() {
    return EM_ASM_DOUBLE({ return window._rb3GpWhammy || 0; });
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

    // ── TIER-1 REPLAY ────────────────────────────────────────────────────────
    // When RB3_REPLAY=<trace.jsonl> (native) or ?replay=<sid> (web) is set, drive
    // the joypad from the RECORDED input timeline instead of the live keyboard /
    // USB gamepad / pad-queue. The held bitmask at this frame is the carry-forward
    // of the trace's edge-only `in` events; we re-assert it through the SAME
    // SendButtonMessages chokepoint a live press uses, so it re-derives the down/up
    // edges internally AND the recorder tap re-records the replayed input as fresh
    // `in` rows (that record->replay->compare round-trip is the Tier-1 bar). We
    // read gRB3TraceFrame — set by the frame tap at App::RunOneFrame entry BEFORE
    // SystemPoll->JoypadPoll runs this poll — so the frame index is current.
    //
    // Init is lazy + once (RB3ReplayInit is idempotent), invoked here so no
    // App.cpp/main edit is needed; it parses the trace the first time JoypadPoll
    // runs (after RB3_DATA chdir, so a relative RB3_REPLAY path resolves) and is a
    // cheap no-op thereafter. RB3ReplayActive() stays false unless replay is armed,
    // so a normal run is unaffected (one predicted branch).
    {
        static bool sReplayInit = false;
        if (!sReplayInit) {
            sReplayInit = true;
            RB3ReplayInit();
        }
        if (RB3ReplayActive()) {
            unsigned int rbits = RB3ReplayBitsForFrame(gRB3TraceFrame);
            // Re-record the replayed edge (edge-only inside RB3RecordInput) so the
            // replay's own trace reproduces the `in` rows for the compare step.
            // Skip the leading all-zero polls before the first recorded press: the
            // live recording never emits a leading 0-edge (its first poll bits==prev==0
            // coalesces), so suppressing it here makes the replay in-stream byte-match
            // the recording (trace-diff exit 0), not carry one spurious f=0/b=0 row.
            static bool sReplayStarted = false;
            if (rbits != 0u) sReplayStarted = true;
            if (sReplayStarted) RB3JoypadTraceInput(rbits, 0.0f);
            SendButtonMessages(0, rbits);
            return;  // ignore live keyboard / gamepad / harness injectors
        }
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
        RB3JoypadTraceInput(bits, 0.0f); // session-telemetry (pad-queue/HTTP path)
        SendButtonMessages(0, bits);
        if (--sPadHoldLeft == 0) {
            sPadGapLeft = kPadGapPolls; // begin forced release
        }
        return;
    }
    if (sPadGapLeft > 0) {
        // Forced release between presses — guarantees a clean falling edge.
        RB3JoypadTraceInput(0u, 0.0f);  // session-telemetry (release edge)
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
        RB3JoypadTraceInput(bits, 0.0f); // session-telemetry (pad-queue press edge)
        SendButtonMessages(0, bits);
        --sPadHoldLeft;
        if (sPadHoldLeft == 0)
            sPadGapLeft = kPadGapPolls;
        return;
    }

#ifdef __EMSCRIPTEN__
    InitWebGameplayKeys();
    InitWebGuitar();
    // Order matters: ReadWebGamepadButtons() runs the poll that publishes
    // window._rb3GpWhammy for the SAME frame; read it right after.
    btns = ReadWebButtons() | ReadWebGamepadButtons();
    // USB-guitar whammy (keyboard has none). wii_guitar cfg uses
    // TRADITIONAL_WHAMMY_VAL -> GetWhammyBar = min(0, -(RX+1)/2), so RX=-1 at
    // rest gives 0 (disengaged) and RX=+1 fully pressed gives -1 (engaged).
    // Map whammy01 (0..1, 0=rest, from the guitar poll above; 0 when no guitar)
    // to RX and clamp to [-1,1] (out-of-range whammy can MILO_ASSERT abort).
    float whammy01 = (float)ReadWebGamepadWhammy();
    if (whammy01 < 0.0f) whammy01 = 0.0f;
    if (whammy01 > 1.0f) whammy01 = 1.0f;
    float webRx = -1.0f + 2.0f * whammy01;
    if (webRx < -1.0f) webRx = -1.0f;
    if (webRx > 1.0f) webRx = 1.0f;
    d->mSticks[1][0] = webRx; // RX (negative_rx / traditional whammy)
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
    if (glfwGetKey(w, GLFW_KEY_ENTER)     == GLFW_PRESS) btns |= kBtnFret0;   // green; R2->Confirm via button_meanings
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

    // SESSION-TELEMETRY input tap (keyboard / USB-gamepad path).
    RB3JoypadTraceInput(btns, d ? d->mSticks[0][1] : 0.0f);

    // The single broadcast: diffs against mButtons, fills mNewPressed/Released,
    // and sends ButtonDownMsg/ButtonUpMsg through gJoypadMsgSource to every
    // subscribed sink — menu (focused UIScreen) AND gameplay GuitarController.
    SendButtonMessages(0, btns);
}

#endif // HX_NATIVE
