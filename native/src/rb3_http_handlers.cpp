// rb3 Native Port — HTTP debug server: main-thread command handlers + frame hooks.
//
// THIS TU uses the target-wide MWCC compat flags (it includes decomp headers:
// obj/Data.h, Rnd_Wgpu_RB3.h, ui/UI.h, game/Game.h). It includes NO httplib, so
// the RTTI-ABI clash that forces rb3_http_server.cpp to compile MS-compat-OFF
// never arises here. The two TUs share the std-only rb3_http_server.h. See
// native/CMakeLists.txt for the flag-split rationale.
//
// Everything here runs on the MAIN / render thread, drained from App.cpp's
// HX_NATIVE frame loop via RB3HttpServerPoll() (pre-draw) and
// RB3HttpServerPollScreenshots() (post-EndDrawing).

#include "rb3_http_server.h"

#include "obj/Data.h"
#include "obj/DataFile.h"   // DataReadString
#include "obj/DataFunc.h"   // DataRegisterFunc — live camera-tweak DTA func
#include "obj/Object.h"

#include "rb3_native_settings.h"  // TheNativeSettings() — live render/camera knobs

// Live-state sources for /api/health: the UI screen + the gameplay song clock.
#include "ui/UI.h"          // UIManager TheUI, UIScreen
#include "ui/UIScreen.h"
#include "game/Game.h"      // Game* TheGame, Game::GetBeatMaster()
#include "beatmatch/BeatMaster.h"   // BeatMaster::GetAudio()
#include "beatmatch/MasterAudio.h"  // MasterAudio::GetTime()

// RB3 GPU backend (graduated into the shared engine). gBandRnd owns the
// GpuDevice; readback + window size come from gBandRnd.Gpu().
#include "platform/Rnd_Wgpu_RB3.h"
#include "gfx/Screenshot.h"

// Native-only diagnosis: walk the synth user's overshell slot so a headless
// harness can read its current view symbol + focus list over /api/dta/eval.
#include "meta_band/OvershellPanel.h"
#include "meta_band/OvershellSlot.h"
#include "meta_band/BandUI.h"
#include "game/BandUserMgr.h"
#include "os/User.h"
#include "meta_band/CharCache.h"      // C13 probe: TheCharCache->GetCharacter
#include "bandobj/BandCharacter.h"    // C13 probe
#include "rndobj/Mesh.h"             // C13 probe: ObjDirItr<RndMesh>, NumBones/Verts
#include "obj/Dir.h"                 // ObjDirItr

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <signal.h>
#include <setjmp.h>

extern BandRnd gBandRnd;  // RB3 GPU backend (Rnd_Wgpu_RB3.cpp)

// rb3_game_input.cpp — main-thread synthetic-input executor.
bool RB3GameInputExecVerbMainThread(const std::string& verb, std::string* err);

// JSON string escaper (local copy — the one in rb3_http_server.cpp is static).
static std::string HJsonEscape(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:   out += c; break;
        }
    }
    return out;
}

// ---------------------------------------------------------------------------
// Capture-hygiene — render one fresh full frame into the single-buffered
// headless target immediately before each readback.
//
// ROOT CAUSE of the headless readback-retention quirk: GpuDevice::mHeadlessTex
// is created once and reused (single-buffered). The main pass already clears it
// every frame (BandRnd::BeginFrame loadOp=Clear), so this is NOT a missing
// LoadOp — it is capture-vs-draw TIMING. /api/screenshot is serviced from a
// queue; ReadbackHeadlessFrame returns whatever the one persistent texture held
// at the last submitted EndFrame, which can be one frame behind the poll-side
// state change (an input verb landed this frame but the visible frame submitted
// in RunOneFrame still reflects the pre-verb state, so a transient lingers 1-2
// captures). Fix: re-render a fresh full frame into the headless target right
// here — on the MAIN thread, after this frame's RunOneFrame EndDrawing — so the
// readback below always reflects current poll-side state.
//
// THREAD-SAFETY (the critical correctness check): HandleScreenshot runs on the
// MAIN / render thread. The httplib endpoint only QueueAndWait()s the request;
// the actual handler is drained by RB3HttpServerPollScreenshots() ->
// ProcessScreenshots() from App::RunWithoutDebugging's frame loop (App.cpp ~709),
// AFTER RunOneFrame's EndDrawing(). No draw is in flight, so issuing a fresh
// BeginDrawing/UI.Draw/EndDrawing here is safe — no pending-flag indirection is
// needed.
//
// We deliberately use the lighter BeginDrawing/UI.Draw/EndDrawing trio rather
// than full RunOneFrame: RunOneFrame also drives TheTaskMgr/Synth poll and the
// game clock, which already ran this iteration — re-running it would double-poll
// and advance the song clock twice per loop. We also do NOT call PresentFrame:
// this is the headless capture path; the readback reads mHeadlessTex directly,
// and PresentFrame is the windowed-swapchain present (untouched here). The
// windowed path is wholly unaffected — this helper is a no-op when !IsHeadless().
// ---------------------------------------------------------------------------
extern sigjmp_buf gDrawJmpBuf;   // native draw guard (defined in main_native.cpp)
extern bool gDrawJmpBufSet;

void RB3RenderFreshHeadlessFrame() {
    if (!gBandRnd.mGpuReady || !gBandRnd.Gpu().IsHeadless())
        return;  // windowed mode (or pre-init): leave the swapchain path alone
    if (!TheRnd)
        return;

    // Same sigsetjmp draw-guard used in App::RunOneFrame (App.cpp ~543): a
    // partially-loaded scene that segfaults in Draw() skips the frame instead of
    // killing the process / the HTTP server.
    TheRnd->BeginDrawing();
    if (sigsetjmp(gDrawJmpBuf, 1) == 0) {
        gDrawJmpBufSet = true;
        TheUI.Draw();
        gDrawJmpBufSet = false;
    } else {
        gDrawJmpBufSet = false;
        MILO_LOG("RB3 Native: caught crash in fresh-headless-frame Draw(), skipping\n");
    }
    TheRnd->EndDrawing();
    // NOTE: no PresentFrame() — readback reads mHeadlessTex directly.
}

// ---------------------------------------------------------------------------
// Screenshot — read back the just-rendered headless frame to PNG bytes.
// ---------------------------------------------------------------------------
void RB3HttpServer::HandleScreenshot(Command& cmd) {
    if (!gBandRnd.mGpuReady) {
        cmd.result.error = "Renderer not initialized";
        return;
    }

    // Capture hygiene: render a fresh frame into the single-buffered headless
    // target so the readback reflects current poll-side state, not a 1-2-frame-
    // stale alias. No-op (returns immediately) in windowed mode. Safe here
    // because HandleScreenshot runs on the main thread after EndDrawing.
    RB3RenderFreshHeadlessFrame();

    int w = gBandRnd.Gpu().WindowWidth();
    int h = gBandRnd.Gpu().WindowHeight();
    size_t pixelSize = (size_t)w * h * 4;
    std::vector<uint8_t> pixels(pixelSize);

    if (!gBandRnd.Gpu().ReadbackHeadlessFrame(pixels.data(), pixelSize)) {
        cmd.result.error = "Framebuffer readback failed (headless mode required)";
        return;
    }
    std::vector<uint8_t> png;
    if (!WritePNGToMemory(png, pixels.data(), w, h)) {
        cmd.result.error = "PNG encoding failed";
        return;
    }
    cmd.result.ok = true;
    cmd.result.binaryData = std::move(png);
}

// ---------------------------------------------------------------------------
// DTA eval — run a DTA expression against the live engine on the main thread.
// Crash recovery via sigsetjmp/siglongjmp (mirrors DC3): a malformed expr can
// stack-overflow ParseArray or segfault in Evaluate; we catch and report a clean
// error instead of killing the process. RB3_GAME mode installs its own draw-
// guard SIGSEGV handler (main_native.cpp); we save/restore around the eval so it
// is reinstated afterward.
// ---------------------------------------------------------------------------
static sigjmp_buf sDtaEvalJmpBuf;
static volatile sig_atomic_t sInDtaEval = 0;
static volatile int sDtaEvalSignal = 0;

static void DtaEvalCrashHandler(int sig) {
    if (sInDtaEval) {
        sDtaEvalSignal = sig;
        siglongjmp(sDtaEvalJmpBuf, sig);
    }
    signal(sig, SIG_DFL);
    raise(sig);
}

static const int kMaxDtaNesting = 256;
static int MaxNestingDepth(const char* s) {
    int depth = 0, maxDepth = 0;
    bool inString = false;
    for (; *s; s++) {
        if (*s == '"') { inString = !inString; continue; }
        if (inString) continue;
        if (*s == '(' || *s == '{' || *s == '[') { if (++depth > maxDepth) maxDepth = depth; }
        else if (*s == ')' || *s == '}' || *s == ']') { if (depth > 0) depth--; }
    }
    return maxDepth;
}

void RB3HttpServer::HandleDtaEval(Command& cmd) {
    int nesting = MaxNestingDepth(cmd.param1.c_str());
    if (nesting > kMaxDtaNesting) {
        cmd.result.httpStatus = 400;
        cmd.result.error = "DTA expression too deeply nested (" +
            std::to_string(nesting) + " levels, max " +
            std::to_string(kMaxDtaNesting) + ")";
        return;
    }

    struct sigaction sa, old_segv, old_bus, old_fpe, old_abrt;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = DtaEvalCrashHandler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGSEGV, &sa, &old_segv);
    sigaction(SIGBUS,  &sa, &old_bus);
    sigaction(SIGFPE,  &sa, &old_fpe);
    sigaction(SIGABRT, &sa, &old_abrt);

    sInDtaEval = 1;
    sDtaEvalSignal = 0;

    if (sigsetjmp(sDtaEvalJmpBuf, 1) != 0) {
        sInDtaEval = 0;
        sigaction(SIGSEGV, &old_segv, nullptr);
        sigaction(SIGBUS,  &old_bus,  nullptr);
        sigaction(SIGFPE,  &old_fpe,  nullptr);
        sigaction(SIGABRT, &old_abrt, nullptr);
        const char* sigName = "unknown signal";
        switch (sDtaEvalSignal) {
            case SIGSEGV: sigName = "SIGSEGV (null pointer or bad memory access)"; break;
            case SIGBUS:  sigName = "SIGBUS (alignment or bus error)"; break;
            case SIGFPE:  sigName = "SIGFPE (arithmetic error)"; break;
            case SIGABRT: sigName = "SIGABRT (abort)"; break;
        }
        cmd.result.error = std::string("DTA eval crashed: ") + sigName;
        fprintf(stderr, "[RB3HttpServer] DTA eval recovered from %s for: %.200s\n",
                sigName, cmd.param1.c_str());
        return;
    }

    DataArray* parsed = DataReadString(cmd.param1.c_str());
    if (!parsed || parsed->Size() == 0) {
        cmd.result.httpStatus = 400;
        cmd.result.error = "Failed to parse DTA expression";
        if (parsed) parsed->Release();
        goto cleanup;
    }
    {
        DataNode result(0);
        for (int i = 0; i < parsed->Size(); i++)
            result = parsed->Evaluate(i);
        parsed->Release();

        cmd.result.ok = true;
        switch (result.Type()) {
            case kDataInt:
                cmd.result.jsonData = "{\"type\":\"int\",\"value\":" +
                    std::to_string(result.UncheckedInt()) + "}";
                break;
            case kDataFloat:
                cmd.result.jsonData = "{\"type\":\"float\",\"value\":" +
                    std::to_string(result.Float()) + "}";
                break;
            case kDataSymbol: {
                const char* s = result.UncheckedStr();
                cmd.result.jsonData = "{\"type\":\"symbol\",\"value\":\"" +
                    HJsonEscape(s ? s : "") + "\"}";
                break;
            }
            case kDataString: {
                // kDataString stores mValue.array (a DataArray*), NOT a raw
                // char*; UncheckedStr() would reinterpret that pointer as a
                // string (garbage). Str() dereferences the array correctly.
                const char* s = result.Str();
                cmd.result.jsonData = "{\"type\":\"string\",\"value\":\"" +
                    HJsonEscape(s ? s : "") + "\"}";
                break;
            }
            case kDataObject: {
                Hmx::Object* obj = result.GetObj(nullptr);
                const char* name = obj ? obj->Name() : "null";
                cmd.result.jsonData = "{\"type\":\"object\",\"value\":\"" +
                    HJsonEscape(name ? name : "null") + "\"}";
                break;
            }
            default:
                cmd.result.jsonData = "{\"type\":" +
                    std::to_string((int)result.Type()) + ",\"value\":null}";
                break;
        }
    }

cleanup:
    sInDtaEval = 0;
    sigaction(SIGSEGV, &old_segv, nullptr);
    sigaction(SIGBUS,  &old_bus,  nullptr);
    sigaction(SIGFPE,  &old_fpe,  nullptr);
    sigaction(SIGABRT, &old_abrt, nullptr);
}

// ---------------------------------------------------------------------------
// Input — execute a synthetic-input verb via rb3_game_input.cpp (main thread).
// ---------------------------------------------------------------------------
void RB3HttpServer::HandleInput(Command& cmd) {
    std::string err;
    if (RB3GameInputExecVerbMainThread(cmd.param1, &err)) {
        cmd.result.ok = true;
        cmd.result.jsonData = "{\"verb\":\"" + HJsonEscape(cmd.param1) + "\"}";
    } else {
        cmd.result.httpStatus = 400;
        cmd.result.error = err.empty() ? "Input verb failed" : err;
    }
}

// ---------------------------------------------------------------------------
// Live render-tweak DTA funcs — make NativeSettings' camera/clear knobs (which
// the matched-fork TrackDir/render read LIVE every frame) mutable over HTTP via
// {rb3_set <field> <value>}. Lets `POST /api/dta/eval {rb3_set cam_rot_x 30}`
// rotate the highway on a RUNNING instance — no rebuild, no restart. Registered
// once from RB3HttpServerInit().
// ---------------------------------------------------------------------------
static DataNode RB3DtaSetSetting(DataArray* a) {
    Symbol field = a->Sym(1);
    float v = a->Float(2);
    NativeSettings& s = TheNativeSettings();
    if      (field == "cam_rot_x")       s.camRotX = v;
    else if (field == "cam_pitch")       s.camPitch = v;
    else if (field == "cam_yaw")         s.camYaw = v;
    else if (field == "cam_roll")        s.camRoll = v;
    else if (field == "cam_forward")     s.camForwardOffset = v;
    else if (field == "cam_height")      s.camHeightOffset = v;
    else if (field == "cam_lateral")     s.camLateralOffset = v;
    else if (field == "fov_scale")       s.fovScale = v;
    else if (field == "clear_r")         s.clearColorR = v;
    else if (field == "clear_g")         s.clearColorG = v;
    else if (field == "clear_b")         s.clearColorB = v;
    else {
        MILO_WARN("rb3_set: unknown field '%s'", field.Str());
        return DataNode(0);
    }
    return DataNode(1);
}

// Native-only state probe: {rb3_overshell} -> a "view:<v>|track:<t>|diff:<d>"
// string for the pad-0 user's overshell slot. Lets the headless pure-keyboard
// harness watch the part/difficulty sub-flow advance (kState_ChoosePart ->
// kState_ChooseDiff -> kState_ReadyToPlay) without any input aids. Read-only.
static std::string sOvershellProbe;
static DataNode RB3DtaOvershellState(DataArray*) {
    if (!TheBandUserMgr) { sOvershellProbe = "view:no_usermgr"; return DataNode(sOvershellProbe.c_str()); }
    // Walk the overshell's slots for the FIRST local user with a slot — the
    // synth pad-0 user. JoypadGetUserFromPadNum can lag the BandUser wiring, so
    // prefer resolving the BandUser straight off a slot.
    OvershellPanel* ov = TheBandUI.GetOvershell();
    if (!ov) { sOvershellProbe = "view:no_overshell"; return DataNode(sOvershellProbe.c_str()); }
    BandUser* bu = nullptr;
    OvershellSlot* slot = nullptr;
    for (int i = 0; i < 4; i++) {
        OvershellSlot* s = ov->GetSlot(i);
        if (s && s->GetUser() && s->GetUser()->IsLocal()) { slot = s; bu = s->GetUser(); break; }
    }
    if (!slot) { sOvershellProbe = "view:no_local_slot"; return DataNode(sOvershellProbe.c_str()); }
    Symbol v = slot->GetCurrentView();
    const char* view = v.Str() ? v.Str() : "?";
    const char* track = bu ? bu->GetTrackSym().Str() : "?";
    const char* diff = bu ? bu->GetDifficultySym().Str() : "?";
    sOvershellProbe = std::string("view:") + view +
                      "|track:" + (track ? track : "?") +
                      "|diff:" + (diff ? diff : "?");
    return DataNode(sOvershellProbe.c_str());
}

// C13 probe: {rb3_char_probe <slot>} -> "playerN meshes=M skinned=S verts=V loading=L"
// for a CharCache preview char. Confirms the opt-in (RB3_CHAR_PREVIEW) Stage-1/2/3
// enable actually loaded a BODY (the proxy-load of char/main/main.milo +
// FileMerger's 13 bodyparts) headlessly, decoupled from the closet UI. Read-only.
static std::string sCharProbe;
static DataNode RB3DtaCharProbe(DataArray *da) {
    int slot = da->Size() > 1 ? da->Int(1) : 0;
    if (!TheCharCache) {
        sCharProbe = "no_charcache";
        return DataNode(sCharProbe.c_str());
    }
    BandCharacter *bc = TheCharCache->GetCharacter(slot);
    if (!bc) {
        sCharProbe = "null_char (preview cache off? set RB3_CHAR_PREVIEW=1)";
        return DataNode(sCharProbe.c_str());
    }
    int meshes = 0, skinned = 0, verts = 0;
    for (ObjDirItr<RndMesh> it(bc, true); it != 0; ++it) {
        meshes++;
        if (it->NumBones() > 0)
            skinned++;
        verts += (int)it->Verts().size();
    }
    char buf[160];
    snprintf(
        buf, sizeof(buf), "player%d meshes=%d skinned=%d verts=%d loading=%d", slot,
        meshes, skinned, verts, bc->IsLoading() ? 1 : 0
    );
    sCharProbe = buf;
    return DataNode(sCharProbe.c_str());
}

void RB3HttpRegisterDtaFuncs() {
    DataRegisterFunc(Symbol("rb3_set"), RB3DtaSetSetting);
    DataRegisterFunc(Symbol("rb3_overshell"), RB3DtaOvershellState);
    DataRegisterFunc(Symbol("rb3_char_probe"), RB3DtaCharProbe);
}

// ---------------------------------------------------------------------------
// Frame-loop hooks (called from App.cpp HX_NATIVE loop).
// ---------------------------------------------------------------------------

// Drain non-screenshot commands + refresh the /api/health state snapshot.
// Called once per frame BEFORE Draw() (after the input poll) on the main thread.
void RB3HttpServerPoll(int frame) {
    if (!TheRB3HttpServer) return;

    UIScreen* cur = TheUI.CurrentScreen();
    const char* screen = cur ? cur->Name() : "";

    // Song clock. Game::GetSongMs() is mMaster->GetAudio()->GetTime(), which
    // derefs NULL before a song is loaded (mMaster/mAudio are 0 in the menus).
    // Walk the chain with explicit null guards so /api/health is safe to poll
    // from boot through gameplay; report -1 until the master audio is live.
    float songMs = -1.0f;
    if (TheGame) {
        BeatMaster* bm = TheGame->GetBeatMaster();
        MasterAudio* ma = bm ? bm->GetAudio() : nullptr;
        if (ma) songMs = ma->GetTime();
    }

    TheRB3HttpServer->NotifyFrame(frame, screen, songMs);
    TheRB3HttpServer->ProcessCommands();
}

// Drain screenshot commands. Called once per frame AFTER EndDrawing() so the
// readback captures the just-rendered frame.
void RB3HttpServerPollScreenshots() {
    if (!TheRB3HttpServer) return;
    TheRB3HttpServer->ProcessScreenshots();
}
