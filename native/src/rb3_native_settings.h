#pragma once
// rb3-native live-tunable settings.
//
// Consolidates the scattered render / camera / gameplay TUNABLE knobs that the
// V12 camera + V13 gem-descent work introduced as ad-hoc `getenv` reads into a
// single struct that is:
//
//   * seeded from environment variables once at startup (InitFromEnv), so the
//     existing `CAM_ROTX=...` invocations keep working unchanged, and
//   * mutable at runtime via a generic name/value interface (SetByName) so the
//     HTTP debug server's future `PUT /api/settings` endpoint (Phase 3) can
//     drive it without knowing each field — the next frame picks up the change
//     (live camera / gem tuning, no rebuild).
//
// SCOPE: TUNABLE values only. The boolean diagnostic flags (CAM_DBG, GEM_DBG,
// RENDER_DBG, CLOCK_DBG, PART_DBG, K8_DBG, ...) deliberately stay as `getenv`
// gates at their call sites — they are on/off logging, not live-tuned values.
//
// This header is intentionally dependency-free (only libc + <string>) so it can
// be included from BOTH the matched-fork engine sources (e.g. TrackDir.cpp,
// compiled in the consumer's context) and the native harness/glue files.

#include <string>

struct NativeSettings {
    // ---------------------------------------------------------------------
    // Camera pose (the live gameplay camera, game.cam, framed in TrackDir).
    // ---------------------------------------------------------------------

    // Lateral centering offset applied to rotater.grp's local X for the lone
    // player (the V12 CAMERA_FRAME_FIX). -4.0 is the empirically-centred value
    // for the single guitar surface; this is the field the old `CAM_ROTX` env
    // var seeded. Larger negative = camera shifts further left of frame.
    float camRotX = -4.0f;

    // Additional pose offsets applied on top of the centred camera. These are
    // wired as neutral (0) defaults — they are the ready-to-tune knobs the
    // V12/V13 work probed by hand; the HTTP endpoint can nudge them live.
    // Convention matches DC3's NativeSettings (view-space offsets):
    //   forward: + = closer to subject, - = farther back
    //   height : + = camera up,         - = down
    //   lateral: + = camera right,      - = left
    float camForwardOffset = 0.0f;
    float camHeightOffset = 0.0f;
    float camLateralOffset = 0.0f;
    // Euler pose deltas (degrees) the camera-pose work touched, neutral default.
    float camPitch = 0.0f;
    float camYaw = 0.0f;
    float camRoll = 0.0f;

    // FOV scale (1.0 = original). DX9 vs WebGPU projection differ slightly;
    // lets you compensate framing width without a rebuild.
    float fovScale = 1.0f;

    // ---------------------------------------------------------------------
    // Gem / highway gameplay-visual tuning.
    // ---------------------------------------------------------------------

    // Multiplier on the track scroll rate (mYPerSecond). 1.0 = authored speed.
    // Affects how fast gems descend the highway. Neutral by default so the V13
    // gem-descent behaviour is preserved exactly unless explicitly tuned.
    float gemScrollRateMult = 1.0f;

    // Look-ahead window (seconds) used when deciding which upcoming gems to
    // spawn/draw. <= 0 means "use the engine's authored window" (no override).
    float gemLookAheadSec = 0.0f;

    // Uniform scale applied to gem geometry. 1.0 = authored size.
    float gemScale = 1.0f;

    // ---------------------------------------------------------------------
    // Render.
    // ---------------------------------------------------------------------

    // Clear / background colour (0..1 per channel). Default black, matching the
    // gBandRnd.SetClearColor(Hmx::Color(0,0,0)) used at boot.
    float clearColorR = 0.0f;
    float clearColorG = 0.0f;
    float clearColorB = 0.0f;

    // -------------------------------------------------------------------------
    // Lifecycle / generic interface.
    // -------------------------------------------------------------------------

    // Seed every field from its environment variable (if present). Called once
    // at startup (RunGame in main_native.cpp). Logs any active overrides.
    void InitFromEnv();

    // Generic setter used by `PUT /api/settings`. `name` is a field name (the
    // same identifier used in GetAllAsJson, e.g. "camRotX"); `value` is its
    // textual form. Returns false for an unknown name. The caller should apply
    // this on the main thread between frames so the next frame sees the change.
    bool SetByName(const char *name, const char *value);

    // Generic getter (textual). Returns false for an unknown name; otherwise
    // fills `out` with the field's current value. Companion to SetByName.
    bool GetByName(const char *name, std::string &out) const;

    // Serialize all fields as a flat JSON object, e.g.
    //   {"camRotX":-4.000,"fovScale":1.000, ... }
    // for `GET /api/settings`.
    std::string GetAllAsJson() const;
};

// Process-wide singleton accessor.
NativeSettings &TheNativeSettings();
