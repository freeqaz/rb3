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
#include "os/Debug.h"       // MILO_TRY/MILO_CATCH + TheDebug — clean eval-fail recovery

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
#include "platform/RB3DrawLogDebug.h"  // W0.3.S3: RB3DebugGetDrawLog() for /api/drawlog
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

// crowd-origin position-dump tool ({rb3_pos_dump}): walk the live object tree and
// emit world positions for crowd members + band gear + static props. See
// docs/native/crowd-origin/PLAN.md §2.
#include "char/Character.h"          // W23 CROWD: sv3_a crowd Character actors (census)
#include "char/CharDriver.h"         // W23 CROWD: CharDriver::FirstPlayingClip (branch d)
#include "char/CharClip.h"           // W23 CROWD: CharClip::Name (playing-clip name)
#include "math/Sphere.h"             // Sphere (MakeWorldSphere)
#include "world/Crowd.h"             // WorldCrowd, CharData::Char3D::unk0 (per-member xfm)
#include "world/Dir.h"               // WorldDir::mCrowds (live ObjPtrList<WorldCrowd>), TheWorld
#include "bandobj/BandDirector.h"    // TheBandDirector->mVenue.Dir() = live per-song venue WorldDir
#include "rndobj/MultiMesh.h"        // RndMultiMesh::Instance::mXfm (pre-Set3DCharAll crowd positions)
#include "rndobj/Trans.h"            // RndTransformable::WorldXfm()/TransParent()
#include "math/Vec.h"                // Length(Vector3)
#include "obj/DirLoader.h"           // DirLoader::Find/GetDir (resident venue dir)
#include "ui/UIPanel.h"             // UIPanel::LoadedDir (world_panel)
#include "utl/FilePath.h"           // FilePath for the resident world.milo lookups
#include <set>                       // de-dup objects reachable from multiple roots
#include <unordered_map>              // W0.3.S3: per-stream dense bind-group ids

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
// Draw log — serialize the just-completed frame's per-draw state-log ring
// (W0.3 RB3_DRAWLOG regression net) to the SAME { frame, count, draws:[...] }
// JSON shape BandRnd::DumpDrawLog() writes to disk (engine repo,
// Rnd_Wgpu_RB3.cpp), so drawlog-golden.py's comparator can diff a live
// /api/drawlog response against the committed golden with no reshaping. Reads
// RB3DebugGetDrawLog() (declared in platform/RB3DrawLogDebug.h) rather than
// duplicating engine internals; the dense-id assignment (first-seen order per
// bind-group stream, erasing raw pointers while preserving the sharing
// pattern) mirrors the engine dumper exactly. Empty ({"draws":[]}) when
// RB3_DRAWLOG (or the debug override) was not enabled for this frame — that is
// a normal, non-error response, not a failure.
//
// Runs via ProcessCommands (kCmdDrawLog), called from App.cpp right after
// RunOneFrame(frame) each iteration, so the ring reflects the frame that just
// finished drawing (DrawMesh populates it; the NEXT frame's BeginFrame clears
// it) — no extra synchronization needed, this is the main thread.
// Minimal JSON string escape for milo object names (quotes/backslashes/control).
static std::string RB3JsonEscape(const std::string& s) {
    std::string o;
    o.reserve(s.size() + 4);
    for (char c : s) {
        switch (c) {
            case '"':  o += "\\\""; break;
            case '\\': o += "\\\\"; break;
            case '\n': o += "\\n";  break;
            case '\t': o += "\\t";  break;
            case '\r': o += "\\r";  break;
            default:
                if ((unsigned char)c < 0x20) { char t[8]; snprintf(t, sizeof(t), "\\u%04x", c); o += t; }
                else o += c;
        }
    }
    return o;
}

void RB3HttpServer::HandleDrawLog(Command& cmd) {
    const std::vector<RB3DrawRecord>& log = RB3DebugGetDrawLog();
    // W17 R3-UIDUMP: provenance sidecar (index-aligned; empty unless RB3_DRAWLOG_PROV).
    const std::vector<RB3DrawProv>& prov = RB3DebugGetDrawProv();
    bool wantProv = (cmd.param1 == "1" || cmd.param1 == "true");
    // roi=x,y,w,h -> server-side rect-intersect filter (implies prov).
    float roi[4] = {0,0,0,0};
    bool wantRoi = false;
    if (!cmd.param2.empty() &&
        sscanf(cmd.param2.c_str(), "%f,%f,%f,%f", &roi[0], &roi[1], &roi[2], &roi[3]) == 4) {
        wantRoi = true; wantProv = true;
    }
    bool provAvail = wantProv && prov.size() == log.size();

    std::unordered_map<const void*, int> sceneIds, matIds, objIds, boneIds;
    auto denseId = [](std::unordered_map<const void*, int>& m, const void* p) -> int {
        auto it = m.find(p);
        if (it != m.end()) return it->second;
        int id = (int)m.size();
        m.emplace(p, id);
        return id;
    };

    char buf[640];
    std::string json;
    json.reserve(log.size() * 320 + 64);
    snprintf(buf, sizeof(buf), "{ \"frame\": %d, \"count\": %d,\n  \"draws\": [",
             gBandRnd.mFrameCount, (int)log.size());
    json += buf;

    // Emit one draw's base record; append its prov object when emitProv. Returns
    // whether anything was written (always true here — kept for symmetry).
    auto emitDraw = [&](size_t i, bool leadComma, bool emitProv) {
        const RB3DrawRecord& r = log[i];
        int sceneId = denseId(sceneIds, r.sceneBG);
        int matId   = denseId(matIds,   r.matBG);
        int objId   = denseId(objIds,   r.objBG);
        int boneId  = denseId(boneIds,  r.boneBG);
        snprintf(buf, sizeof(buf),
                 "%s\n    { \"i\":%d, \"name\":\"0x%llx\", \"pipe\":\"0x%llx\", "
                 "\"blend\":%d, \"zmode\":%d, \"layout\":%d, \"fmt\":%u, "
                 "\"hasDepth\":%s, \"alphaCut\":%s, \"alphaWrite\":%s, \"skinned\":%s, "
                 "\"idx\":%u, \"tris\":%u, \"verts\":%u, "
                 "\"scene\":%d, \"mat\":%d, \"obj\":%d, \"bone\":%d,\n"
                 "      \"world\":[",
                 (leadComma ? "," : ""),
                 (int)i,
                 (unsigned long long)r.meshNameHash,
                 (unsigned long long)r.pipelineHash,
                 (int)r.blend, (int)r.zMode, (int)r.layout, (unsigned)r.targetFormat,
                 (r.flags & 1) ? "true" : "false",
                 (r.flags & 2) ? "true" : "false",
                 (r.flags & 4) ? "true" : "false",
                 (r.flags & 8) ? "true" : "false",
                 (unsigned)r.indexCount, (unsigned)r.triCount, (unsigned)r.vertCount,
                 sceneId, matId, objId, boneId);
        json += buf;
        for (int e = 0; e < 16; ++e) {
            snprintf(buf, sizeof(buf), "%s%.6g", (e == 0 ? "" : ","), (double)r.world[e]);
            json += buf;
        }
        json += "]";                    // close world array
        if (emitProv) {
            const RB3DrawProv& p = prov[i];
            const char* dl = p.passDepthLoadOp == 0 ? "Clear"
                           : p.passDepthLoadOp == 1 ? "Load" : "none";
            // T2-WORLDROI (Wave 19): the prov object stays OPEN here (closing brace
            // appended after the optional boneRects, per plan B7.2) so rectKind==3
            // skinned rows carry per-bone screen sub-rects + the bind-pose fallback
            // count without disturbing every other row's field order.
            snprintf(buf, sizeof(buf),
                     ",\n      \"prov\": { \"mesh\":\"%s\", \"mat\":\"%s\", \"cam\":\"%s\", "
                     "\"trans\":\"%s\", \"panel\":\"%s\", \"owner\":\"%s\", "
                     "\"matColor\":[%.4g,%.4g,%.4g,%.4g], \"boundColor\":[%.4g,%.4g,%.4g,%.4g], "
                     "\"rect\":[%.1f,%.1f,%.1f,%.1f], \"rectKind\":%u, "
                     "\"pass\":%u, \"passDepthLoad\":\"%s\", \"boneFallback\":%d",
                     RB3JsonEscape(p.meshName).c_str(), RB3JsonEscape(p.matName).c_str(),
                     RB3JsonEscape(p.camName).c_str(), RB3JsonEscape(p.transParent).c_str(),
                     RB3JsonEscape(p.scopePanel).c_str(), RB3JsonEscape(p.scopeOwner).c_str(),
                     p.matColor[0], p.matColor[1], p.matColor[2], p.matColor[3],
                     p.boundColor[0], p.boundColor[1], p.boundColor[2], p.boundColor[3],
                     p.rect[0], p.rect[1], p.rect[2], p.rect[3], (unsigned)p.rectKind,
                     (unsigned)p.passIdx, dl, p.boneFallback);
            json += buf;
            if (p.rectKind == 3 && !p.boneRects.empty()) {
                json += ", \"boneRects\":[";
                for (size_t k = 0; k < p.boneRects.size(); ++k) {
                    const RB3ProvBoneRect& br = p.boneRects[k];
                    snprintf(buf, sizeof(buf),
                             "%s{\"bone\":\"%s\",\"rect\":[%.1f,%.1f,%.1f,%.1f]}",
                             k ? "," : "", RB3JsonEscape(br.bone).c_str(),
                             br.rect[0], br.rect[1], br.rect[2], br.rect[3]);
                    json += buf;
                }
                json += "]";
            }
            json += " }";                   // close prov object
        }
        json += " }";                   // close draw (byte-identical to old "] }" when no prov)
    };

    if (wantRoi) {
        // Filter to draws whose projected rect intersects the ROI, in submission
        // order. lastWriter = index (into the returned array) of the last such draw.
        int emitted = 0;
        for (size_t i = 0; i < log.size(); ++i) {
            if (!provAvail) break;
            const RB3DrawProv& p = prov[i];
            if (p.rectKind == 2 || p.rect[2] < 0) continue;  // degenerate / unavailable
            bool overlap = p.rect[0] < roi[0] + roi[2] && p.rect[0] + p.rect[2] > roi[0] &&
                           p.rect[1] < roi[1] + roi[3] && p.rect[1] + p.rect[3] > roi[1];
            if (!overlap) continue;
            emitDraw(i, emitted != 0, true);
            emitted++;
        }
        int lastWriter = emitted > 0 ? emitted - 1 : -1;
        snprintf(buf, sizeof(buf),
                 "%s],\n  \"roi\":[%.1f,%.1f,%.1f,%.1f], \"matched\":%d, \"lastWriter\":%d, "
                 "\"provAvailable\":%s, \"coverage\":\"BandRnd::DrawMesh only\" }\n",
                 emitted == 0 ? "" : "\n  ", roi[0], roi[1], roi[2], roi[3],
                 emitted, lastWriter, provAvail ? "true" : "false");
        json += buf;
    } else if (wantProv) {
        for (size_t i = 0; i < log.size(); ++i) emitDraw(i, i != 0, provAvail);
        snprintf(buf, sizeof(buf),
                 "%s],\n  \"provAvailable\":%s, \"provSize\":%d, \"logSize\":%d, \"coverage\":\"BandRnd::DrawMesh only\" }\n",
                 log.empty() ? "" : "\n  ", provAvail ? "true" : "false",
                 (int)prov.size(), (int)log.size());
        json += buf;
    } else {
        // Default path: byte-identical to the committed golden.
        for (size_t i = 0; i < log.size(); ++i) emitDraw(i, i != 0, false);
        json += log.empty() ? "] }\n" : "\n  ] }\n";
    }

    cmd.result.ok = true;
    cmd.result.jsonData = std::move(json);
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

    // Snapshot the engine's global FAIL / data-call-stack state so we can RESTORE
    // it on the recovery path. This is the fix for the "latent SIGSEGV across
    // waves": when the sigsetjmp below catches an abort, the C-level siglongjmp
    // unwinds out of the recursive DataArray::Execute WITHOUT running the
    // ~DataCallStackFrame dtors that pop gCallStackPtr, and the Debug::Fail that
    // reached Modal leaves mTry / mFailing dirty. Left uncorrected, gCallStackPtr
    // stays advanced at stale DataArray* and mFailing stays true, so the NEXT
    // eval's DataCallStackFrame ctor / DataAppendStackTrace walks freed pointers
    // -> a delayed crash. Restoring this snapshot in the recovery block leaves
    // the engine in the exact pre-eval state, so a recovered eval never bleeds
    // corruption into the next one.
    DataArray** const savedCallStackPtr = gCallStackPtr;
    const int savedTry = TheDebug.mTry;
    const bool savedFailing = TheDebug.mFailing;

    if (sigsetjmp(sDtaEvalJmpBuf, 1) != 0) {
        sInDtaEval = 0;
        sigaction(SIGSEGV, &old_segv, nullptr);
        sigaction(SIGBUS,  &old_bus,  nullptr);
        sigaction(SIGFPE,  &old_fpe,  nullptr);
        sigaction(SIGABRT, &old_abrt, nullptr);
        // Undo the skipped ~DataCallStackFrame pops + the dirty Debug fail flags
        // so the next eval starts clean (no latent corruption).
        gCallStackPtr = savedCallStackPtr;
        TheDebug.mTry = savedTry;
        TheDebug.mFailing = savedFailing;
        const char* sigName = "unknown signal";
        switch (sDtaEvalSignal) {
            case SIGSEGV: sigName = "SIGSEGV (null pointer or bad memory access)"; break;
            case SIGBUS:  sigName = "SIGBUS (alignment or bus error)"; break;
            case SIGFPE:  sigName = "SIGFPE (arithmetic error)"; break;
            case SIGABRT: sigName = "SIGABRT (abort)"; break;
        }
        // A miss / bad-type read that tripped a hard MILO_FAIL is a graceful
        // outcome for this debug tool, not a server fault: report it as a clean
        // 400 error (not a 500 "crashed") with a null value so callers can
        // distinguish it from a real crash and keep inspecting.
        cmd.result.ok = false;
        cmd.result.httpStatus = 400;
        cmd.result.error =
            std::string("DTA eval hit a hard fail (") + sigName +
            ") — likely a missing key or a type-mismatched property read; "
            "value is null";
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
        // A bad eval (a {find ...} miss, an {exists <expr>} whose sub-expr
        // executes an object handler that reads an out-of-range arg, a
        // Color/Array/Str sub-property read on a wrong-typed node) reaches a
        // hard MILO_FAIL deep inside DataArray::Execute / DataNode::Str /
        // DataNode::Array. On native, Debug::Fail with NO try-scope active
        // rides the full Debug::Modal path, where formatting the fail message
        // (String temporaries on the failing heap) can ABORT inside _MemFree /
        // hit OSFatal. The sigsetjmp guard catches the signal, but recovering
        // from a glibc malloc-abort leaves the allocator lock held -> the MAIN
        // THREAD then WEDGES on its next malloc (observed: every later eval
        // times out with "main thread not polling"). So we must avoid reaching
        // Modal at all for the common miss.
        //
        // MILO_TRY sets TheDebug.mTry, so Debug::Fail takes its clean
        // longjmp-back branch BEFORE Modal -> no Modal, no heap abort, no main-
        // thread wedge; we land in MILO_CATCH with the fail message and return a
        // graceful 400. The longjmp skips the in-flight ~DataCallStackFrame
        // pops, so we restore gCallStackPtr (snapshotted above) in the catch.
        // The sigsetjmp guard remains a backstop for a genuine non-MILO_FAIL
        // fault (stack overflow, a real null-deref) and also restores state.
        MILO_TRY {
            DataNode result(0);
            for (int i = 0; i < parsed->Size(); i++)
                result = parsed->Evaluate(i);

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
                    // A node CAN carry kDataObject with a NULL object pointer
                    // (e.g. a missing {... find <key>} or {find_obj} returns a
                    // null kDataObject). GetObj() returns null then; report a
                    // clean "null" rather than dereferencing it.
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
            parsed->Release();
        } MILO_CATCH(failMsg) {
            // Debug::Fail longjmped here (clean — never reached Modal). Restore
            // the data-call-stack pointer the longjmp skipped, then report a
            // graceful error. (mTry is reset by MILO_CATCH's SetTry(false);
            // mFailing was never set since we never reached the mFailing branch
            // of Debug::Fail.)
            gCallStackPtr = savedCallStackPtr;
            // Release the parsed DataArray on the fail path. The earlier comment
            // here claimed re-touching the heap was "avoided for safety" and let
            // `parsed` LEAK on every bad eval — but the MILO_TRY longjmp is a
            // CLEAN unwind (it fires at Debug.cpp:175 BEFORE Debug::Modal, so the
            // allocator is fully intact and unlocked). The leak was the
            // accelerant for the burst stack-overflow SIGSEGV (wave-6 residual):
            // ~15 consecutive hard-fails leaked enough that the heap-stack
            // bookkeeping (MemPushHeap) overflowed -> its MILO_ASSERT re-entered
            // Debug::Fail -> recursive MemPushHeap->Debug::Fail->MakeString loop
            // -> stack overflow. Freeing `parsed` here removes the accelerant.
            // gCallStackPtr was already restored to its pre-eval value above, so
            // ~DataArray's bookkeeping sees the same call-stack state as a normal
            // release. Null after release: `cleanup`/the signal path never touch
            // it, but keep it tidy in case future edits add a use.
            if (parsed) { parsed->Release(); parsed = nullptr; }
            cmd.result.ok = false;
            cmd.result.httpStatus = 400;
            cmd.result.error = std::string("DTA eval failed: ") +
                (failMsg ? failMsg : "missing key or type-mismatched read");
            fprintf(stderr, "[RB3HttpServer] DTA eval MILO_FAIL for %.200s: %s\n",
                    cmd.param1.c_str(), failMsg ? failMsg : "(no message)");
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
    bool dump = ::getenv("CHAR_PROBE_DUMP") != nullptr;
    for (ObjDirItr<RndMesh> it(bc, true); it != 0; ++it) {
        meshes++;
        if (it->NumBones() > 0)
            skinned++;
        verts += (int)it->Verts().size();
        if (dump) {
            const char *mn = it->Name() ? it->Name() : "?";
            // log outfit/body-relevant meshes + any with bones
            if (it->NumBones() > 0 || strstr(mn, "trackjacket") || strstr(mn, "vestdenim")
                || strstr(mn, "plaidshirt") || strstr(mn, "shred") || strstr(mn, "resource")
                || strstr(mn, "head") || strstr(mn, "hands") || strstr(mn, "skin."))
                fprintf(stderr, "[CHAR_PROBE_DUMP] slot%d mesh='%s' NumBones=%d verts=%d\n",
                        slot, mn, it->NumBones(), (int)it->Verts().size());
        }
    }
    char buf[160];
    snprintf(
        buf, sizeof(buf), "player%d meshes=%d skinned=%d verts=%d loading=%d", slot,
        meshes, skinned, verts, bc->IsLoading() ? 1 : 0
    );
    sCharProbe = buf;
    return DataNode(sCharProbe.c_str());
}

// crowd-origin debug tool: {rb3_pos_dump} -> dump live world positions for the
// crowd members, the band characters (+ their instrument dir = the drum kit), and
// static venue props (the dartboard control). Adjudicates H1 (shared venue
// reparent collapse) vs H2 (band root never placed) vs H4 (crowd decode-to-zero)
// from docs/native/crowd-origin/PLAN.md §2. Read-only. Verbose per-object
// [POSDUMP] stderr lines are gated behind env POS_DUMP_VERBOSE; the HTTP call
// always returns a one-line summary for the harness verdict.
// Dump every authored per-member world position for one WorldCrowd. Returns the
// number of members emitted; accumulates into the at-origin counters. The
// audience positions live in CharData::m3DChars[i].unk0 (the Transform the draw
// loop SetWorldXfm's, Crowd.cpp:328-408) once WorldCrowd::Set3DCharAll() has run.
// BEFORE that runs, the same positions still live in mMMesh->mInstances[i].mXfm
// (Set3DCharAll copies instances -> m3DChars THEN clears mInstances,
// Crowd.cpp:227-230) — so we fall back to the live multimesh instance list when
// m3DChars is empty, to never report a spurious crowd=0 just because the copy
// step hasn't happened yet. `src` records which source the positions came from.
static int DumpOneWorldCrowd(WorldCrowd *crowd, const char *fromTag, bool verbose,
                             int &atOrigin, int &crowdAtOrigin) {
    int emitted = 0;
    const char *cname = crowd->Name() ? crowd->Name() : "?";
    int archIdx = 0;
    FOREACH (charIt, crowd->mCharacters) {
        const char *aname =
            charIt->mDef.mChar.mPtr && charIt->mDef.mChar->Name()
                ? charIt->mDef.mChar->Name()
                : cname;
        unsigned int n3d = (unsigned int)charIt->m3DChars.size();
        if (n3d > 0) {
            for (unsigned int i = 0; i < n3d; i++) {
                const Vector3 &p = charIt->m3DChars[i].unk0.v;
                float mag = Length(p);
                emitted++;
                if (mag < 1.0f) { crowdAtOrigin++; atOrigin++; }
                if (verbose) {
                    fprintf(stderr,
                            "[POSDUMP] kind=crowd   name=%s i=%u  pos=%.2f,%.2f,%.2f "
                            "src=m3DChars crowd=%s arch=%d from=%s\n",
                            aname, i, p.x, p.y, p.z, cname, archIdx, fromTag);
                }
            }
        } else if (charIt->mMMesh) {
            // Pre-Set3DCharAll fallback: read the authored xfm straight off the
            // multimesh instance list (the source Set3DCharAll copies from).
            unsigned int i = 0;
            FOREACH (inst, charIt->mMMesh->mInstances) {
                const Vector3 &p = inst->mXfm.v;
                float mag = Length(p);
                emitted++;
                if (mag < 1.0f) { crowdAtOrigin++; atOrigin++; }
                if (verbose) {
                    fprintf(stderr,
                            "[POSDUMP] kind=crowd   name=%s i=%u  pos=%.2f,%.2f,%.2f "
                            "src=mInstances crowd=%s arch=%d from=%s\n",
                            aname, i, p.x, p.y, p.z, cname, archIdx, fromTag);
                }
                ++i;
            }
        }
        ++archIdx;
    }
    return emitted;
}

static std::string sPosDump;
static DataNode RB3DtaPosDump(DataArray *) {
    const bool verbose = ::getenv("POS_DUMP_VERBOSE") != nullptr;

    // The live venue / band / crowd are NOT merged into the walkable sMainDir
    // tree on native — they live in resident DirLoader dirs (world/world.milo,
    // world/shared/chars.milo, world/shared/director.milo) plus the world_panel's
    // LoadedDir(). Walk sMainDir AND those roots (mirrors the gameplay-warm root
    // gather in rb3_gamewarm_native.cpp:402-427). De-dup by object pointer since
    // some objects are reachable from more than one root.
    //
    // CROWD GAP (audience-measure.md): the audience WorldCrowd lives in the
    // PER-SONG venue WorldDir (world/venue/<class>/<name>/<name>.milo), loaded
    // into TheBandDirector->mVenue.Dir() (== mCurWorld). That dir is NOT in the
    // mSubDirs chain of world/world.milo or world_panel — the resident roots
    // above — so a recursive ObjDirItr from them never reaches it (the prior
    // `crowd=0`). Add the live venue dir(s) as explicit roots, AND read each
    // WorldDir's live `mCrowds` list directly (populated by SyncObjects via the
    // SAME ObjDirItr<WorldCrowd> recursion; reading the cached list does not
    // depend on the crowd being in OUR walk's reachable subtree).
    static const char *kResidentDirMilos[] = {
        "world/world.milo",            // venue base (+ per-song venue proxy subdir)
        "world/shared/director.milo",  // venue director / lighting / props
        "world/shared/chars.milo",     // band character meshes
    };
    const int kMaxRoots = 12;
    ObjectDir *roots[kMaxRoots] = {nullptr};
    const char *rootTags[kMaxRoots] = {nullptr};
    int nroots = 0;
    if (ObjectDir::sMainDir) {
        roots[nroots] = ObjectDir::sMainDir; rootTags[nroots] = "sMainDir"; nroots++;
    }
    if (ObjectDir::sMainDir) {
        if (UIPanel *worldPanel =
                ObjectDir::sMainDir->Find<UIPanel>("world_panel", true)) {
            if (ObjectDir *wd = worldPanel->LoadedDir()) {
                roots[nroots] = wd; rootTags[nroots] = "world_panel"; nroots++;
            }
        }
    }
    for (size_t i = 0;
         i < sizeof(kResidentDirMilos) / sizeof(kResidentDirMilos[0]); ++i) {
        FilePath fp(kResidentDirMilos[i]);
        DirLoader *dl = DirLoader::Find(fp);
        if (dl && dl->IsLoaded() && nroots < kMaxRoots) {
            if (ObjectDir *d = dl->GetDir()) {
                roots[nroots] = d; rootTags[nroots] = kResidentDirMilos[i]; nroots++;
            }
        }
    }
    // The live per-song venue WorldDir (where the audience WorldCrowd lives).
    // mVenue.Dir() is the loaded venue; mCurWorld is the entered one (== mVenue
    // after EnterVenue). Add both as roots (de-dup handles overlap). Also keep
    // them as explicit WorldDir handles so we can read mCrowds directly below.
    WorldDir *venueDirs[2] = {nullptr, nullptr};
    int nVenue = 0;
    if (TheBandDirector) {
        WorldDir *vd = TheBandDirector->mVenue.Dir();
        if (vd) {
            venueDirs[nVenue++] = vd;
            if (nroots < kMaxRoots) {
                roots[nroots] = vd; rootTags[nroots] = "venue(mVenue.Dir)"; nroots++;
            }
        }
        WorldDir *cw = TheBandDirector->mCurWorld;
        if (cw && cw != vd) {
            venueDirs[nVenue++] = cw;
            if (nroots < kMaxRoots) {
                roots[nroots] = cw; rootTags[nroots] = "venue(mCurWorld)"; nroots++;
            }
        }
    }
    // TheWorld is transiently set during DrawShowing and usually NULL at HTTP
    // time, but include it opportunistically when live.
    if (TheWorld && nroots < kMaxRoots) {
        roots[nroots] = TheWorld; rootTags[nroots] = "TheWorld"; nroots++;
    }

    if (nroots == 0) {
        sPosDump = "posdump no_roots";
        return DataNode(sPosDump.c_str());
    }

    int crowdMembers = 0, bandCount = 0, propCount = 0;
    int crowdContainers = 0;  // distinct WorldCrowd objects reached
    int atOrigin = 0;        // objects whose |pos| < 1.0 (any kind)
    int bandAtOrigin = 0;    // band roots with |WorldXfm().v| < 1.0
    int crowdAtOrigin = 0;   // crowd members with |unk0.v| < 1.0

    std::set<const Hmx::Object *> seen;
    std::set<const WorldCrowd *> seenCrowd;  // de-dup crowds across roots + mCrowds

    // PRIMARY PATH: read each live venue WorldDir's cached mCrowds list directly.
    // This does NOT rely on the crowd being inside OUR walk's reachable mSubDirs
    // subtree — WorldDir::SyncObjects already enumerated the crowds at Enter and
    // cached the pointers. This is the path that closes the `crowd=0` gap.
    for (int vi = 0; vi < nVenue; ++vi) {
        WorldDir *wd = venueDirs[vi];
        if (!wd) continue;
        const char *wn = wd->Name() ? wd->Name() : "?";
        if (verbose) {
            fprintf(stderr,
                    "[POSDUMP] venue_dir name=%s nCrowds=%d\n",
                    wn, wd->mCrowds.size());
        }
        FOREACH (cit, wd->mCrowds) {
            WorldCrowd *crowd = *cit;
            if (!crowd) continue;
            if (!seenCrowd.insert(crowd).second) continue;
            seen.insert(crowd);  // keep the ObjDirItr pass from re-emitting it
            crowdContainers++;
            crowdMembers += DumpOneWorldCrowd(crowd, "mCrowds", verbose,
                                              atOrigin, crowdAtOrigin);
        }
    }

  for (int ri = 0; ri < nroots; ++ri) {
    // Cast in PLAN.md §2 order: WorldCrowd first (members are NOT separate
    // objects), then BandCharacter, then any other RndTransformable (props).
    for (ObjDirItr<Hmx::Object> it(roots[ri], true); it; ++it) {
        Hmx::Object *o = it;
        if (!seen.insert(o).second) continue;  // already counted via another root

        if (WorldCrowd *crowd = dynamic_cast<WorldCrowd *>(o)) {
            // Secondary path: any WorldCrowd we reach by recursive walk that the
            // mCrowds sweep above missed (e.g. a crowd in a dir whose WorldDir we
            // didn't enumerate). De-dup against the mCrowds sweep.
            if (seenCrowd.insert(crowd).second) {
                crowdContainers++;
                crowdMembers += DumpOneWorldCrowd(crowd, rootTags[ri] ? rootTags[ri] : "walk",
                                                  verbose, atOrigin, crowdAtOrigin);
            }
            continue;
        }

        if (BandCharacter *bc = dynamic_cast<BandCharacter *>(o)) {
            const char *bname = bc->Name() ? bc->Name() : "?";
            const Vector3 &root_v = bc->WorldXfm().v;
            float rootMag = Length(root_v);
            RndTransformable *parent = bc->TransParent();
            const char *pname = (parent && parent->Name()) ? parent->Name() : "NULL";
            bandCount++;
            if (rootMag < 1.0f) { bandAtOrigin++; atOrigin++; }

            // The drum kit / instrument geometry lives in mInstDir and rides the
            // character bones; its world-sphere center is the kit position. If the
            // kit is far from the root while the root is staged, that's H5 (merge).
            float ix = 0.f, iy = 0.f, iz = 0.f;
            bool haveInst = false;
            Character *inst = bc->mInstDir.mPtr;
            if (inst) {
                Sphere sph;
                if (inst->MakeWorldSphere(sph, false)) {
                    ix = sph.center.x; iy = sph.center.y; iz = sph.center.z;
                    haveInst = true;
                }
            }
            if (verbose) {
                fprintf(stderr,
                        "[POSDUMP] kind=band    name=%s root=%.2f,%.2f,%.2f parent=%s "
                        "inst=%.2f,%.2f,%.2f%s\n",
                        bname, root_v.x, root_v.y, root_v.z, pname,
                        ix, iy, iz, haveInst ? "" : " (no_inst_sphere)");
            }
            continue;
        }

        if (RndTransformable *t = dynamic_cast<RndTransformable *>(o)) {
            const Vector3 &w = t->WorldXfm().v;
            float mag = Length(w);
            propCount++;
            if (mag < 1.0f) atOrigin++;
            if (verbose) {
                const char *tn = t->Name() ? t->Name() : "?";
                const char *cn = t->ClassName().Str() ? t->ClassName().Str() : "?";
                fprintf(stderr,
                        "[POSDUMP] kind=prop    name=%s class=%s world=%.2f,%.2f,%.2f\n",
                        tn, cn, w.x, w.y, w.z);
            }
            continue;
        }
    }
  }

    if (verbose) ::fflush(stderr);  // durably flush [POSDUMP] lines for the harness

    char buf[320];
    snprintf(buf, sizeof(buf),
             "posdump roots=%d crowd_containers=%d crowd=%d band=%d props=%d "
             "at_origin=%d band_at_origin=%d/%d crowd_at_origin=%d/%d",
             nroots, crowdContainers, crowdMembers, bandCount, propCount, atOrigin,
             bandAtOrigin, bandCount, crowdAtOrigin, crowdMembers);
    sPosDump = buf;
    return DataNode(sPosDump.c_str());
}

// ---------------------------------------------------------------------------
// W23 CROWD lane discriminator: census the shell-vignette (sv3_a) street-crowd
// Character actors BY NAME in the LIVE tree — the walkers SWEEP S1 says retail
// draws down the center street and native does not. These are NOT WorldCrowd
// (`strings sv3_a | grep -c WorldCrowd` == 0), so rb3_pos_dump lumps them under
// "prop" with no showing/pollstate/clip signal. This tool reports, per crowd
// Character (name begins `crowd_`, or whose dir path contains streetslomo/
// vignette), the four discriminator fields:
//   show=<0|1>      RndDrawable::Showing()   (branch b: loaded-but-not-drawn)
//   poll=<state>    Character::GetPollState() (kCharPolled==3 => in the poll set)
//   frz=<0|1>       mFrozen                   (branch d: driver frozen)
//   drv=<0|1>       GetDriver()!=NULL
//   clip=<name>     driver FirstPlayingClip() (branch d: never-animated => none)
//   sph=<cx,cy,cz,r> MakeWorldSphere          (branch c: mis-posed/off-screen)
// READ-ONLY. HX_NATIVE-only (native harness tool); no engine edit, no pin bump,
// Wii-neutral. Reuses the same resident-root walk as rb3_pos_dump.
// ---------------------------------------------------------------------------
static std::string sCrowdCensus;
static DataNode RB3DtaCrowdCensus(DataArray *) {
    const bool verbose = ::getenv("CROWD_CENSUS_VERBOSE") != nullptr;

    // Same roots pos_dump walks: sMainDir, world_panel LoadedDir, resident milos.
    static const char *kResidentDirMilos[] = {
        "world/world.milo", "world/shared/director.milo", "world/shared/chars.milo",
    };
    const int kMaxRoots = 12;
    ObjectDir *roots[kMaxRoots] = {nullptr};
    const char *rootTags[kMaxRoots] = {nullptr};
    int nroots = 0;
    if (ObjectDir::sMainDir) {
        roots[nroots] = ObjectDir::sMainDir; rootTags[nroots] = "sMainDir"; nroots++;
        if (UIPanel *wp = ObjectDir::sMainDir->Find<UIPanel>("world_panel", true)) {
            if (ObjectDir *wd = wp->LoadedDir()) {
                roots[nroots] = wd; rootTags[nroots] = "world_panel"; nroots++;
            }
        }
    }
    for (size_t i = 0; i < sizeof(kResidentDirMilos) / sizeof(kResidentDirMilos[0]); ++i) {
        DirLoader *dl = DirLoader::Find(FilePath(kResidentDirMilos[i]));
        if (dl && dl->IsLoaded() && nroots < kMaxRoots) {
            if (ObjectDir *d = dl->GetDir()) {
                roots[nroots] = d; rootTags[nroots] = kResidentDirMilos[i]; nroots++;
            }
        }
    }

    int total = 0, showing = 0, polled = 0, driven = 0, animating = 0;
    int onscreen = 0;  // sphere center is finite & radius > 0
    std::set<const Hmx::Object *> seen;
    for (int ri = 0; ri < nroots; ++ri) {
        for (ObjDirItr<Character> it(roots[ri], true); it; ++it) {
            Character *c = it;
            if (!seen.insert(c).second) continue;
            const char *nm = c->Name() ? c->Name() : "?";
            // Scope to the shell-vignette crowd actors: name-gated `crowd_` (the
            // sv3_a crowd_male0N / crowd_female0N) OR dir named for the vignette.
            ObjectDir *dir = c->Dir();
            const char *dnm = (dir && dir->Name()) ? dir->Name() : "";
            bool isCrowd = (::strncmp(nm, "crowd_", 6) == 0) ||
                           (::strstr(dnm, "streetslomo") != nullptr) ||
                           (::strstr(dnm, "vignette") != nullptr);
            if (!isCrowd) continue;
            total++;

            bool sh = c->Showing();
            if (sh) showing++;
            Character::PollState ps = c->GetPollState();
            if (ps == Character::kCharPolled) polled++;
            CharDriver *drv = c->GetDriver();
            if (drv) driven++;
            const char *clipNm = "-";
            if (drv) {
                CharClip *pc = drv->FirstPlayingClip();
                if (pc) { animating++; clipNm = pc->Name() ? pc->Name() : "?"; }
            }
            Sphere sph;
            float cx = 0, cy = 0, cz = 0, rad = 0;
            bool haveSph = c->MakeWorldSphere(sph, false);
            if (haveSph) { cx = sph.center.x; cy = sph.center.y; cz = sph.center.z; rad = sph.radius; }
            if (haveSph && rad > 0.0f) onscreen++;

            // Branch-b/d refinement: count RndMesh under this Character and how
            // many are Showing + carry verts. A char that is Showing() but whose
            // body meshes are all hidden / vert-less produces no skinned draw.
            int meshCount = 0, meshShowing = 0, meshVerts = 0;
            const bool meshDump = ::getenv("CROWD_CENSUS_MESHES") != nullptr;
            for (ObjDirItr<RndMesh> mit(c, true); mit; ++mit) {
                meshCount++;
                if (mit->Showing()) meshShowing++;
                int nv = (int)mit->Verts().size();
                if (nv > 0) meshVerts++;
                if (meshDump && verbose) {
                    // GeomOwner tells us whether a 0-vert mesh is a proxy whose
                    // geometry lives in another mesh (owner != self) vs a genuine
                    // load-failure/empty mesh (owner == self, nv == 0).
                    // W24 STEP 0: also report the compressed-vert fields + faces,
                    // matching the draw-gate metric in rb3_render_mesh.cpp:455
                    //   hasGeom = owner->mFaces>0 && (mVerts>0 || mNumCompressedVerts>0)
                    // ERRATA-C1: mVerts is empty BY DESIGN for compressed native
                    // meshes; the true "has geometry" signal is comp>0 OR verts>0.
                    RndMesh *go = mit->GeomOwner();
                    if (!go) go = (RndMesh *)mit;
                    const char *mn = mit->Name() ? mit->Name() : "?";
                    const char *gon = (go && go != (RndMesh *)mit && go->Name()) ? go->Name() : "self";
                    int comp = go->mCompressedVerts ? (int)go->mNumCompressedVerts : 0;
                    int compPtr = go->mCompressedVerts ? 1 : 0;
                    int faces = (int)go->mFaces.size();
                    int hasGeom = (faces > 0 && (go->mVerts.size() > 0 || go->mNumCompressedVerts > 0)) ? 1 : 0;
                    fprintf(stderr,
                            "[CROWDMESH] char=%s mesh='%s' show=%d bones=%d verts=%d "
                            "compPtr=%d comp=%d faces=%d hasGeom=%d geomOwner=%s ownerVerts=%d\n",
                            nm, mn, mit->Showing() ? 1 : 0, mit->NumBones(), nv,
                            compPtr, comp, faces, hasGeom, gon,
                            go ? (int)go->mVerts.size() : -1);
                }
            }

            if (verbose) {
                // W24 STEP 0: report LOD count + proxy state. DrawShowing draws
                // the selected LOD's Group; a Character with mLods.size()==0
                // (a proxy whose LODs live in its master) draws only
                // RndDir::DrawShowing() and never the LOD-group body meshes.
                int nlods = (int)c->mLods.size();
                bool isProxy = c->IsProxy();
                FilePath &pf = c->ProxyFile();
                const char *pfn = pf.empty() ? "-" : pf.c_str();
                // RndDir::DrawShowing draws mDraws, populated by SyncDrawables
                // ONLY when !IsSubDir(). A crowd proxy that is a sub-dir has an
                // EMPTY mDraws -> RndDir::DrawShowing draws nothing, and lods=0
                // means the LOD-group body draw is skipped too => zero draws.
                RndDir *rd = static_cast<RndDir *>(c);
                int ndraws = (int)rd->mDraws.size();
                bool subdir = c->IsSubDir();
                // Enumerate mDraws entries: name + class + showing. Tells us
                // whether the body mesh (or a lodN.grp) is in the actual draw
                // list, or whether only props are.
                if (::getenv("CROWD_CENSUS_DRAWS")) {
                    for (int di = 0; di < ndraws; ++di) {
                        RndDrawable *dr = rd->mDraws[di];
                        Hmx::Object *dobj = dynamic_cast<Hmx::Object *>(dr);
                        fprintf(stderr, "  [CROWDDRAW] char=%s draw[%d]='%s' class=%s show=%d\n",
                                nm, di,
                                dobj && dobj->Name() ? dobj->Name() : "?",
                                dobj ? dobj->ClassName().Str() : "?",
                                dr->Showing() ? 1 : 0);
                    }
                }
                fprintf(stderr,
                        "[CROWDCENSUS] name=%s dir=%s show=%d poll=%d drv=%d "
                        "clip=%s sph=%.1f,%.1f,%.1f,r=%.1f mesh=%d/%d show=%d vert=%d "
                        "lods=%d isProxy=%d isSubDir=%d nDraws=%d proxyFile=%s\n",
                        nm, dnm[0] ? dnm : "?", sh ? 1 : 0, (int)ps,
                        drv ? 1 : 0, clipNm, cx, cy, cz, rad,
                        meshShowing, meshCount, meshShowing, meshVerts,
                        nlods, isProxy ? 1 : 0, subdir ? 1 : 0, ndraws, pfn);
            }
        }
    }
    // -----------------------------------------------------------------------
    // W24 STEP 0 POSITIVE CONTROL (ERRATA-C1). Census meshes that DO render on
    // HX_NATIVE so we know what a known-good mesh's (verts, comp, faces) triple
    // looks like on this build. We walk the SAME roots and dump every mesh that
    // passes the draw-gate hasGeom test (rb3_render_mesh.cpp:455). These are the
    // band-player outfits + resident scene geometry that appear in the drawlog.
    // Env: CROWD_POSCTRL=1 (verbose implied). Caps at 40 lines to avoid flood.
    // -----------------------------------------------------------------------
    if (::getenv("CROWD_POSCTRL") != nullptr) {
        int posGeom = 0, posShown = 0, dumped = 0;
        std::set<const Hmx::Object *> pseen;
        for (int ri = 0; ri < nroots; ++ri) {
            for (ObjDirItr<RndMesh> mit(roots[ri], true); mit; ++mit) {
                RndMesh *m = mit;
                if (!pseen.insert(m).second) continue;
                RndMesh *go = m->GeomOwner();
                if (!go) go = m;
                int nv = (int)go->mVerts.size();
                int comp = go->mCompressedVerts ? (int)go->mNumCompressedVerts : 0;
                int faces = (int)go->mFaces.size();
                bool hasGeom = faces > 0 && (nv > 0 || comp > 0);
                if (!hasGeom) continue;
                posGeom++;
                bool sh = m->Showing();
                if (sh) posShown++;
                // Only dump the first 40 SHOWING geometry-bearing meshes (the
                // ones that actually draw): that's the direct control set.
                if (sh && dumped < 40) {
                    const char *mn = m->Name() ? m->Name() : "?";
                    const char *gon = (go != m && go->Name()) ? go->Name() : "self";
                    fprintf(stderr,
                            "[POSCTRL] mesh='%s' show=%d bones=%d verts=%d "
                            "compPtr=%d comp=%d faces=%d hasGeom=1 geomOwner=%s\n",
                            mn, sh ? 1 : 0, m->NumBones(), nv,
                            go->mCompressedVerts ? 1 : 0, comp, faces, gon);
                    dumped++;
                }
            }
        }
        fprintf(stderr, "[POSCTRL] summary geomBearing=%d showingGeom=%d dumped=%d\n",
                posGeom, posShown, dumped);
        ::fflush(stderr);
    }

    if (verbose) ::fflush(stderr);

    char buf[256];
    snprintf(buf, sizeof(buf),
             "crowdcensus roots=%d crowd_chars=%d showing=%d polled=%d driven=%d "
             "animating=%d onscreen=%d",
             nroots, total, showing, polled, driven, animating, onscreen);
    sCrowdCensus = buf;
    return DataNode(sCrowdCensus.c_str());
}

// ---------------------------------------------------------------------------
// Deterministic "force band closeup" harness hooks (converge-2026-06-20).
//
// The native build registers NO `band_director` DTA accessor and TheBandDirector
// is a C++ global, not a name-resolvable DTA object — so the editor verbs
// `{band_director force_shot ...}` / `{$band_director set disabled 1}` are SILENT
// no-ops (probe-data §1). Without a way to PIN a venue camera shot, every A/B
// capture lands on a different auto-director angle (the camera-desync
// false-positive). These three native-only accessors give the Python harness a
// real signal + a hard determinism gate.
//
// Pinning needs BOTH: ForceShot(shot) (sets mNextShot + mDisablePicking) AND
// mDisabled=1 (stops OnSelectCamera's per-frame re-pick, BandDirector.cpp:1446).
// Set mDisabled FIRST so no intervening frame can re-pick over the forced shot.
// Everything touched (mDisabled, mVenue, mCurShot, ForceShot) lives on the RB3
// BandDirector (src/, not the shared engine) and is already reachable via the
// existing rb3_pos_dump plumbing — no engine change, no pin bump, Wii-neutral.
// See docs/native/converge-2026-06-20/scout-harness.md §1.
// ---------------------------------------------------------------------------

// {rb3_force_shot "<name>"} -> pin a venue camera shot by name. Idempotent once
// mDisabled is set: the forced shot applies on the next OnSelectCamera and then
// stays (mNextShot is consumed, nothing re-picks). Returns a status STRING so the
// harness gets a real signal instead of the silent 0 the probe saw.
static std::string sForceShotResult;  // back the not_found:%s branch (MakeString is transient)
static DataNode RB3DtaForceShot(DataArray* a) {
    if (!TheBandDirector) return DataNode("force_shot no_director");
    WorldDir* wdir = TheBandDirector->mVenue.Dir();   // same handle rb3_pos_dump uses
    if (!wdir)            return DataNode("force_shot no_venue");
    const char* name = a->Size() > 1 ? a->Str(1) : "";  // 1-based: index 0 is the func sym
    BandCamShot* shot = wdir->Find<BandCamShot>(name, false);
    if (!shot) {
        sForceShotResult = std::string("force_shot not_found:") + (name ? name : "");
        return DataNode(sForceShotResult.c_str());
    }
    TheBandDirector->mDisabled = true;   // STOP the per-frame auto re-pick FIRST
    TheBandDirector->ForceShot(shot);    // then queue our shot (sets mNextShot + mDisablePicking)
    return DataNode("force_shot ok");
}

// {rb3_director_disable <0|1>} -> explicit director freeze/unfreeze. Echoes the
// current state so the harness can (a) freeze before forcing, (b) assert the
// echo, (c) unfreeze so the auto-director resumes (for multi-member capture).
static DataNode RB3DtaDirectorDisable(DataArray* a) {
    if (!TheBandDirector) return DataNode(0);
    if (a->Size() > 1) TheBandDirector->mDisabled = (a->Int(1) != 0);
    return DataNode(TheBandDirector->mDisabled ? 1 : 0);  // echo current state
}

// {rb3_cur_shot} -> the live mCurShot name. The cheapest machine-checkable
// determinism proof: after forcing, poll across N frames — it must equal the
// forced name every frame. (mCurShot is an ObjPtr<BandCamShot>; it converts to a
// raw BandCamShot* via operator T1*.)
static DataNode RB3DtaCurShot(DataArray*) {
    if (!TheBandDirector) return DataNode("");
    BandCamShot* s = TheBandDirector->mCurShot;
    return DataNode(s && s->Name() ? s->Name() : "");
}

void RB3HttpRegisterDtaFuncs() {
    DataRegisterFunc(Symbol("rb3_set"), RB3DtaSetSetting);
    DataRegisterFunc(Symbol("rb3_overshell"), RB3DtaOvershellState);
    DataRegisterFunc(Symbol("rb3_char_probe"), RB3DtaCharProbe);
    DataRegisterFunc(Symbol("rb3_pos_dump"), RB3DtaPosDump);
    DataRegisterFunc(Symbol("rb3_force_shot"), RB3DtaForceShot);            // NEW
    DataRegisterFunc(Symbol("rb3_director_disable"), RB3DtaDirectorDisable);  // NEW
    DataRegisterFunc(Symbol("rb3_cur_shot"), RB3DtaCurShot);               // NEW
    DataRegisterFunc(Symbol("rb3_crowd_census"), RB3DtaCrowdCensus);       // W23 CROWD
}

// ---------------------------------------------------------------------------
// Frame-loop hooks (called from App.cpp HX_NATIVE loop).
// ---------------------------------------------------------------------------

// Drain non-screenshot commands + refresh the /api/health state snapshot.
// Called once per frame BEFORE Draw() (after the input poll) on the main thread.
void RB3HttpServerPoll(int frame) {
    // Session-telemetry M4 replay CHECKPOINT (chk). Sampled on THIS proven
    // main-thread, boot-through-gameplay-safe site, but BEFORE the HTTP-server
    // guard below — checkpoints must fire whenever tracing is armed (e.g. a
    // headless RB3_REPLAY run with no RB3_HTTP). No-op when tracing is off. The
    // sampler is in-song-only periodic (RB3_TRACE_CHK_EVERY); nav transitions emit
    // their own chk from the nav sink (rb3_trace_taps.cpp).
    extern void RB3TraceCheckpointFrame(int frame);
    RB3TraceCheckpointFrame(frame);

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

    // Wave-19 T1 (Lane F) songClock axis: sample the audio clock per frame at this
    // clean, main-thread, post-RunOneFrame site (A3 option-b; NEVER touches the dirty
    // rb3_session_trace.cpp). Gated internally on RB3_LOADDET_TIMELINE (default-OFF);
    // keys on gRB3TraceFrame so it shares the one frame axis. Behind the RB3_HTTP
    // guard above -> songClock requires RB3_HTTP=1 (disclosed).
    {
        extern void RB3LoadDetSongMs(float);
        RB3LoadDetSongMs(songMs);
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
