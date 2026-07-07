// rb3 Native Port — Embedded HTTP Debug Server (httplib half).
// Adapted from dc3-decomp/native/src/platform/HttpServer.cpp.
//
// THIS TU compiles as ordinary clang C++ (NO MWCC compat flags) because it
// includes vendored cpp-httplib, whose <regex>/mmap shared_ptr deleters need
// the normal RTTI ABI. It therefore includes NO decomp headers. The engine-
// touching command handlers (DTA eval / screenshot / input / health snapshot)
// live in rb3_http_handlers.cpp, which DOES use the MWCC compat flags. Both
// share the std-only rb3_http_server.h. See native/CMakeLists.txt for the flag
// split rationale.
//
// Phases 1-2 endpoints:
//   GET  /api/health      — {status, frame, songMs, currentScreen}
//   GET  /api/screenshot  — PNG of the current rendered frame
//   POST /api/dta/eval    — run a DTA expression on the main thread, return result
//   POST /api/input       — inject a synthetic-input verb on the next frame

#include "rb3_http_server.h"

// ----- libc poll()/send() collision shim -----------------------------------
// RB3's decomp interns global Symbols whose bare linker names collide with libc
// socket functions httplib uses (utl/Symbols4.cpp: `Symbol poll`, `Symbol
// send`). The rb3-native link uses -Wl,--allow-multiple-definition, so httplib's
// `poll(...)` / `send(...)` calls would bind to those bss Symbol *data* objects
// instead of the libc functions and jump into data → SIGSEGV the moment the
// httplib listener thread runs. Those Symbol globals can't be guarded out: the
// matched-fork decomp (rndobj/Poll.cpp, synth/Sfx.cpp, …) references them by
// identifier. (libc `close`/`select` don't collide — those Symbol globals ARE
// HX_NATIVE-guarded in Symbols2/4.cpp because the native decomp doesn't use them.)
//
// Fix, scoped to this TU only: redirect httplib's poll()/send() to uniquely-
// named forwarders that reach the kernel via syscall(), so NO `poll`/`send`
// linker symbol is referenced here and the collision never reaches the linker.
// (A plain ::poll/::send forwarder would itself emit a colliding reference that
// --allow-multiple-definition would re-bind to the bss Symbol.)
#include <poll.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/syscall.h>
#include <unistd.h>
static inline int rb3_httplib_poll(struct pollfd* fds, nfds_t nfds, int timeout) {
    return (int)syscall(SYS_poll, fds, (unsigned long)nfds, timeout);
}
static inline ssize_t rb3_httplib_send(int fd, const void* buf, size_t n, int flags) {
    // send(fd,buf,n,flags) == sendto(fd,buf,n,flags,NULL,0)
    return (ssize_t)syscall(SYS_sendto, fd, buf, n, flags, (void*)0, (unsigned)0);
}
#define poll rb3_httplib_poll
#define send rb3_httplib_send

#include <httplib.h>

#undef poll
#undef send

#include <cstdio>
#include <cstdlib>
#include <chrono>

RB3HttpServer* TheRB3HttpServer = nullptr;

// milo-trace W9 replay API (rb3_replay_api.cpp): gated by RB3_REPLAY_API=1.
// RB3ReplayApiHandle services the kCmdReplay* command types on the main thread;
// RB3ReplayApiEnabled reports whether the endpoints should be registered.
extern bool RB3ReplayApiHandle(int type, RB3HttpServer::Command& cmd);
extern bool RB3ReplayApiEnabled();

// ---------------------------------------------------------------------------
// JSON helpers — hand-rolled to avoid pulling in a JSON library.
// ---------------------------------------------------------------------------
static std::string JsonEscape(const std::string& s) {
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

static std::string JsonOk(const std::string& dataJson) {
    return "{\"ok\":true,\"data\":" + dataJson + "}";
}

static std::string JsonError(const std::string& msg) {
    return "{\"ok\":false,\"error\":\"" + JsonEscape(msg) + "\"}";
}

// Minimal extractor for {"<key>":"<value>"} JSON bodies. Returns "" if absent.
static std::string JsonStringField(const std::string& body, const char* key) {
    std::string needle = std::string("\"") + key + "\"";
    auto pos = body.find(needle);
    if (pos == std::string::npos) return "";
    auto colon = body.find(':', pos + needle.size());
    if (colon == std::string::npos) return "";
    auto q1 = body.find('"', colon + 1);
    if (q1 == std::string::npos) return "";
    std::string out;
    for (size_t i = q1 + 1; i < body.size(); i++) {
        if (body[i] == '\\' && i + 1 < body.size()) { out += body[i + 1]; i++; }
        else if (body[i] == '"') break;
        else out += body[i];
    }
    return out;
}

// ---------------------------------------------------------------------------
// RB3HttpServer — lifecycle + queue plumbing (no engine access).
// ---------------------------------------------------------------------------
RB3HttpServer::RB3HttpServer() {}
RB3HttpServer::~RB3HttpServer() { Stop(); }

bool RB3HttpServer::Start(int port) {
    if (mRunning) return true;

    auto* svr = new httplib::Server();
    mServer = svr;
    RegisterEndpoints();

    // Loopback only — a local dev tool, not a network service. bind on the
    // calling thread so we fail fast if the port is taken.
    if (!svr->bind_to_port("127.0.0.1", port)) {
        fprintf(stderr, "[RB3HttpServer] FATAL: port %d already in use\n", port);
        delete svr;
        mServer = nullptr;
        return false;
    }

    mPort = port;
    mRunning = true;
    mServerThread = std::thread(&RB3HttpServer::ServerThread, this);

    fprintf(stdout, "RB3_HTTP_PORT=%d\n", port);  // machine-readable for scripts
    fflush(stdout);
    fprintf(stderr, "[RB3HttpServer] Listening on 127.0.0.1:%d\n", port);
    return true;
}

void RB3HttpServer::Stop() {
    if (!mRunning) return;
    mRunning = false;

    auto* svr = static_cast<httplib::Server*>(mServer);
    if (svr) svr->stop();
    if (mServerThread.joinable())
        mServerThread.join();
    delete static_cast<httplib::Server*>(mServer);
    mServer = nullptr;
    fprintf(stderr, "[RB3HttpServer] Stopped\n");
}

void RB3HttpServer::ServerThread() {
    auto* svr = static_cast<httplib::Server*>(mServer);
    if (!svr->listen_after_bind()) {
        fprintf(stderr, "[RB3HttpServer] listen_after_bind() failed\n");
        mRunning = false;
    }
}

RB3HttpServer::CommandResult RB3HttpServer::QueueAndWait(
    CommandType type, const std::string& p1, const std::string& p2
) {
    Command cmd;
    cmd.type = type;
    cmd.param1 = p1;
    cmd.param2 = p2;

    {
        std::lock_guard<std::mutex> lk(mQueueMutex);
        if (!mRunning) {
            CommandResult down;
            down.error = "Server shutting down";
            return down;
        }
        if (type == kCmdScreenshot)
            mPendingScreenshots.push_back(&cmd);
        else
            mPendingCommands.push_back(&cmd);
    }

    {
        std::unique_lock<std::mutex> lk(cmd.mtx);
        cmd.cv.wait_for(lk, std::chrono::seconds(10), [&] { return cmd.done; });
    }

    if (!cmd.done) {
        CommandResult timeout;
        timeout.error = "Command timed out (main thread not polling?)";
        return timeout;
    }
    return cmd.result;
}

void RB3HttpServer::ProcessCommands() {
    std::vector<Command*> batch;
    {
        std::lock_guard<std::mutex> lk(mQueueMutex);
        batch.swap(mPendingCommands);
    }
    for (Command* cmd : batch) {
        switch (cmd->type) {
            case kCmdDtaEval: HandleDtaEval(*cmd); break;
            case kCmdInput:   HandleInput(*cmd); break;
            case kCmdDrawLog: HandleDrawLog(*cmd); break;
            case kCmdReplayMemory:
            case kCmdReplayCall:
            case kCmdReplayInfo:
                RB3ReplayApiHandle(cmd->type, *cmd); break;
            default: cmd->result.error = "Unknown command type"; break;
        }
        {
            std::lock_guard<std::mutex> lk(cmd->mtx);
            cmd->done = true;
        }
        cmd->cv.notify_one();
    }
}

void RB3HttpServer::ProcessScreenshots() {
    std::vector<Command*> batch;
    {
        std::lock_guard<std::mutex> lk(mQueueMutex);
        batch.swap(mPendingScreenshots);
    }
    for (Command* cmd : batch) {
        HandleScreenshot(*cmd);
        {
            std::lock_guard<std::mutex> lk(cmd->mtx);
            cmd->done = true;
        }
        cmd->cv.notify_one();
    }
}

void RB3HttpServer::NotifyFrame(int frame, const char* screenName, float songMs) {
    std::lock_guard<std::mutex> lk(mStateMutex);
    mCurrentFrame = frame;
    mCurrentScreen = screenName ? screenName : "";
    mCurrentSongMs = songMs;
}

// ---------------------------------------------------------------------------
// Endpoint registration (httplib types — main-thread work goes via QueueAndWait).
// ---------------------------------------------------------------------------
void RB3HttpServer::RegisterEndpoints() {
    auto* svr = static_cast<httplib::Server*>(mServer);

    svr->set_post_routing_handler([](const httplib::Request&, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
        res.set_header("Access-Control-Allow-Headers", "Content-Type");
    });
    svr->set_exception_handler([](const httplib::Request&, httplib::Response& res,
                                  std::exception_ptr ep) {
        std::string msg = "Internal server error";
        try { std::rethrow_exception(ep); }
        catch (std::exception& e) { msg = e.what(); }
        catch (...) {}
        res.status = 500;
        res.set_content(JsonError(msg), "application/json");
    });
    svr->Options(".*", [](const httplib::Request&, httplib::Response& res) {
        res.status = 204;
    });

    // GET /api/health — {status, frame, songMs, currentScreen}
    svr->Get("/api/health", [this](const httplib::Request&, httplib::Response& res) {
        int frame; std::string screen; float songMs;
        {
            std::lock_guard<std::mutex> lk(mStateMutex);
            frame = mCurrentFrame; screen = mCurrentScreen; songMs = mCurrentSongMs;
        }
        char buf[512];
        snprintf(buf, sizeof(buf),
            "{\"status\":\"ok\",\"frame\":%d,\"songMs\":%.1f,\"currentScreen\":\"%s\"}",
            frame, songMs, JsonEscape(screen).c_str());
        res.set_content(JsonOk(buf), "application/json");
    });

    // GET /api/screenshot — PNG of the current rendered frame
    svr->Get("/api/screenshot", [this](const httplib::Request&, httplib::Response& res) {
        auto result = QueueAndWait(kCmdScreenshot);
        if (result.ok) {
            res.set_content(reinterpret_cast<const char*>(result.binaryData.data()),
                            result.binaryData.size(), "image/png");
        } else {
            res.status = result.httpStatus;
            res.set_content(JsonError(result.error), "application/json");
        }
    });

    // POST /api/dta/eval — run a DTA expression on the main thread.
    // Body: raw DTA text, or JSON {"expr":"..."}.
    svr->Post("/api/dta/eval", [this](const httplib::Request& req, httplib::Response& res) {
        std::string expr;
        if (req.has_header("Content-Type") &&
            req.get_header_value("Content-Type").find("application/json") != std::string::npos) {
            expr = JsonStringField(req.body, "expr");
        }
        if (expr.empty()) expr = req.body; // fallback: raw body is the expression
        if (expr.empty()) {
            res.status = 400;
            res.set_content(JsonError("No expression provided"), "application/json");
            return;
        }
        auto result = QueueAndWait(kCmdDtaEval, expr);
        if (result.ok) {
            res.set_content(JsonOk(result.jsonData), "application/json");
        } else {
            res.status = result.httpStatus;
            res.set_content(JsonError(result.error), "application/json");
        }
    });

    // POST /api/input — inject a synthetic-input verb (rb3_game_input vocabulary:
    // start / confirm / cancel / up / down / left / right / option /
    // select:<comp> / msg:<obj>:<action>[:arg...] / track:<sym> /
    // overshell:<action>[:arg...]  — C6 pause/options slot navigation:
    //   overshell:show_options       → kState_Options (pause-menu options list)
    //   overshell:show_game_options  → kState_GameOptions (in-song Lefty/Vocal)
    //   overshell:leave_options      → dismiss options, return to gameplay
    //   overshell:toggle_lefty_flip  → toggle Lefty Flip (session-only until C2)
    //   overshell:toggle_vocal_style → toggle Vocal Style (session-only until C2)
    //   overshell:show_state:<int>   → ShowState(id) generic).
    // Body: raw verb text, or JSON {"verb":"..."}.
    svr->Post("/api/input", [this](const httplib::Request& req, httplib::Response& res) {
        std::string verb;
        if (req.has_header("Content-Type") &&
            req.get_header_value("Content-Type").find("application/json") != std::string::npos) {
            verb = JsonStringField(req.body, "verb");
        }
        if (verb.empty()) verb = req.body;
        size_t a = verb.find_first_not_of(" \t\r\n");
        size_t b = verb.find_last_not_of(" \t\r\n");
        verb = (a == std::string::npos) ? "" : verb.substr(a, b - a + 1);
        if (verb.empty()) {
            res.status = 400;
            res.set_content(JsonError("No input verb provided"), "application/json");
            return;
        }
        auto result = QueueAndWait(kCmdInput, verb);
        if (result.ok) {
            res.set_content(JsonOk(result.jsonData), "application/json");
        } else {
            res.status = result.httpStatus;
            res.set_content(JsonError(result.error), "application/json");
        }
    });

    // GET /api/drawlog — W0.3 per-draw state-log ring (RB3_DRAWLOG regression
    // net) for the just-completed frame, as the same { frame, count, draws:[...] }
    // JSON shape the S1 engine dump writes (see RB3DrawLogDebug.h / DumpDrawLog):
    // dense per-stream (scene/mat/obj/bone) bind-group ids, column-major world
    // xfm, pipeline/blend/zmode/layout/format/flags, index/tri/vert counts, and
    // the mesh-name hash. Returned unwrapped (no ok/data envelope) so
    // drawlog-golden.py's comparator can diff the response body directly
    // against the committed golden file with no unwrapping step. Empty
    // ({"draws":[]}) when RB3_DRAWLOG (or the debug override) is not enabled.
    // ?prov=1 adds the W17 R3-UIDUMP provenance sidecar per draw (names/rect/pass/
    // scope; requires RB3_DRAWLOG_PROV at boot). ?roi=x,y,w,h server-side rect-
    // intersect filter (implies prov). Absent both -> byte-identical to the golden.
    svr->Get("/api/drawlog", [this](const httplib::Request& req, httplib::Response& res) {
        std::string prov = req.has_param("prov") ? req.get_param_value("prov") : "";
        std::string roi  = req.has_param("roi")  ? req.get_param_value("roi")  : "";
        auto result = QueueAndWait(kCmdDrawLog, prov, roi);
        if (result.ok) {
            res.set_content(result.jsonData, "application/json");
        } else {
            res.status = result.httpStatus;
            res.set_content(JsonError(result.error), "application/json");
        }
    });

    // -----------------------------------------------------------------------
    // milo-trace W9 replay API (opt-in via RB3_REPLAY_API=1). All three route
    // the JSON body to the main thread via QueueAndWait; the handlers live in
    // rb3_replay_api.cpp. Registered only when enabled so the surface is absent
    // (404) in a normal RB3_HTTP deployment.
    // -----------------------------------------------------------------------
    if (RB3ReplayApiEnabled()) {
        auto replayRoute = [this](CommandType type) {
            return [this, type](const httplib::Request& req, httplib::Response& res) {
                auto result = QueueAndWait(type, req.body);
                if (result.ok) {
                    res.set_content(JsonOk(result.jsonData), "application/json");
                } else {
                    res.status = result.httpStatus;
                    res.set_content(JsonError(result.error), "application/json");
                }
            };
        };
        // POST /api/memory — {op:alloc|read|write|clear|info, ...} on the arena.
        svr->Post("/api/memory", replayRoute(kCmdReplayMemory));
        // POST /api/call — {symbol|static_addr, args:[...], readback:[...]} invoke.
        svr->Post("/api/call", replayRoute(kCmdReplayCall));
        // GET /api/replay/info — {base (PIE load bias), arena_size}.
        svr->Get("/api/replay/info", [this](const httplib::Request&, httplib::Response& res) {
            auto result = QueueAndWait(kCmdReplayInfo);
            if (result.ok) res.set_content(JsonOk(result.jsonData), "application/json");
            else { res.status = result.httpStatus; res.set_content(JsonError(result.error), "application/json"); }
        });
    }
}

// ---------------------------------------------------------------------------
// Lifecycle hooks (no engine access — safe in the httplib TU).
// ---------------------------------------------------------------------------
// Defined in rb3_http_handlers.cpp (decomp-header TU): registers the live
// render-tweak DTA func {rb3_set <field> <value>}.
extern void RB3HttpRegisterDtaFuncs();

void RB3HttpServerInit() {
    const char* env = getenv("RB3_HTTP");
    if (!env || atoi(env) == 0) return;  // opt-in only — zero impact otherwise

    int port = 8080;
    if (const char* portEnv = getenv("RB3_HTTP_PORT")) {
        port = atoi(portEnv);
        if (port <= 0) port = 8080;
    }
    RB3HttpRegisterDtaFuncs();
    TheRB3HttpServer = new RB3HttpServer();
    if (!TheRB3HttpServer->Start(port)) {
        delete TheRB3HttpServer;
        TheRB3HttpServer = nullptr;
    }
}

void RB3HttpServerShutdown() {
    if (TheRB3HttpServer) {
        TheRB3HttpServer->Stop();
        delete TheRB3HttpServer;
        TheRB3HttpServer = nullptr;
    }
}
