// rb3 Native Port — Embedded HTTP Debug Server
// Background thread serving REST endpoints for live engine introspection +
// remote control. Adapted from dc3-decomp/native/src/platform/HttpServer.{h,cpp}.
//
// Desktop-only, opt-in: the server only starts when RB3_HTTP=1 is set in the
// environment (RB3HttpServerInit() is a no-op otherwise — zero impact on the
// regression / CI runs). Port defaults to 8080 (override with RB3_HTTP_PORT).
//
// Design: an HTTP handler thread pushes a Command onto a queue; the main /
// render thread drains the queue once per frame (RB3HttpServerPoll, wired into
// App.cpp's HX_NATIVE frame loop) and signals the handler (condition variable),
// so any engine-touching work (DTA eval, screenshot readback, input inject)
// runs on the main thread where the engine expects it.

#pragma once

#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <cstdint>

class RB3HttpServer {
public:
    RB3HttpServer();
    ~RB3HttpServer();

    // Start the server on the given port. Returns true if started.
    bool Start(int port = 8080);

    // Stop the server and join the background thread.
    void Stop();

    bool IsRunning() const { return mRunning; }
    int Port() const { return mPort; }

    // --- Command queue (main thread processes these each frame) ---
    enum CommandType {
        kCmdScreenshot,   // Capture framebuffer as PNG (after EndDrawing)
        kCmdDtaEval,      // Evaluate a DTA expression
        kCmdInput,        // Inject a synthetic-input verb (rb3_game_input.cpp)
        // milo-trace W9 replay API (gated by RB3_REPLAY_API=1; handlers live in
        // rb3_replay_api.cpp). param1 carries the JSON request body.
        kCmdReplayMemory, // /api/memory   — sandboxed arena alloc/read/write/clear
        kCmdReplayCall,   // /api/call     — resolve + invoke a native symbol
        kCmdReplayInfo,   // /api/replay/info — PIE load bias + arena geometry
    };

    struct CommandResult {
        bool ok = false;
        int httpStatus = 500;             // HTTP status for errors (400=client, 500=server)
        std::string error;
        std::string jsonData;             // For JSON responses
        std::vector<uint8_t> binaryData;  // For screenshot PNG
    };

    struct Command {
        CommandType type;
        std::string param1;
        std::string param2;

        std::mutex mtx;
        std::condition_variable cv;
        bool done = false;
        CommandResult result;
    };

    // Called from the main thread each frame (App.cpp HX_NATIVE loop).
    void ProcessCommands();
    // Called from the main thread after EndDrawing (so readback sees the frame).
    void ProcessScreenshots();
    // Snapshot the live frame/screen/song state for /api/health (called per frame).
    void NotifyFrame(int frame, const char* screenName, float songMs);

private:
    void ServerThread();
    void RegisterEndpoints();

    CommandResult QueueAndWait(CommandType type, const std::string& p1 = "",
                               const std::string& p2 = "");

    void HandleScreenshot(Command& cmd);
    void HandleDtaEval(Command& cmd);
    void HandleInput(Command& cmd);

    volatile bool mRunning = false;
    int mPort = 0;
    std::thread mServerThread;

    std::mutex mQueueMutex;
    std::vector<Command*> mPendingCommands;
    std::vector<Command*> mPendingScreenshots;

    void* mServer = nullptr; // opaque httplib::Server*

    // Live state snapshot for /api/health (written by NotifyFrame on main thread).
    std::mutex mStateMutex;
    int mCurrentFrame = 0;
    std::string mCurrentScreen;
    float mCurrentSongMs = -1.0f;
};

extern RB3HttpServer* TheRB3HttpServer;

// Frame-loop + lifecycle hooks (called from App.cpp / main_native.cpp).
void RB3HttpServerInit();      // start if RB3_HTTP=1 (else no-op)
void RB3HttpServerShutdown();
void RB3HttpServerPoll(int frame);          // drain commands + notify state (pre-draw)
void RB3HttpServerPollScreenshots();        // drain screenshot queue (post-EndDrawing)

// Capture hygiene: render one fresh full frame into the single-buffered headless
// target (BeginDrawing/UI.Draw/EndDrawing, no PresentFrame) so /api/screenshot
// readbacks reflect current poll-side state instead of a 1-2-frame-stale alias.
// No-op in windowed mode (!IsHeadless()). MUST be called on the main thread.
void RB3RenderFreshHeadlessFrame();
