// rb3_movie_native.cpp — native/web backend for RB3's fullscreen cinematics
// (intro_movie_screen, win_credits screen).
//
// RB3's matched-fork Movie::Impl (src/system/movie/Movie.cpp) is the Wii Bink
// decoder. Under HX_NATIVE we route the public Movie lifecycle here instead of
// the Bink path (which has no native decoder). Two backends:
//
//   web  (__EMSCRIPTEN__): a hardware-decoded <video> element composited as a
//     fullscreen overlay over #canvas-container. The .bik filename is rewritten
//     to the pre-transcoded .webm sidecar (VP9 + Opus) served by native/web/
//     server.py with HTTP range requests. No GPU upload is needed — the browser
//     composites the <video> directly above the WebGPU canvas (z-index max), so
//     this works without touching the rb3 BandRnd backend. Mirrors the proven
//     engine WebMovieImpl overlay JS.
//
//   native desktop (!__EMSCRIPTEN__): there is no <video>; the intro is a
//     browser-specific feature here. Default behaviour is instant-skip
//     (Poll()->ended) so native boot is unchanged and still reaches main_hub
//     immediately. Set RB3_INTRO_SECS=<n> to "virtual-play" for n seconds, which
//     exercises the real screen-flow (intro_movie_screen stays up, then
//     movie_done advances to splash_screen) for headless verification.
//
// Only fullscreen cinematics are handled (gated by basename). TexMovie in-world
// videos still no-op on web, exactly as before this change.

#ifdef HX_NATIVE

#include <cstring>
#include <cstdlib>
#include <cstdio>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>

// Create a fullscreen <video> overlay above #canvas-container and start it.
// Returns 1 on success, 0 on failure. Path is the .webm asset path (already
// rewritten from .bik); it is fetched from /api/file/<path>.
EM_JS(int, rb3_webmovie_create, (const char *url, int loop), {
    try {
        if (!Module._rb3Movie) Module._rb3Movie = {};
        var st = Module._rb3Movie;
        // Tear down any previous overlay first.
        if (st.video) {
            try {
                st.video.pause();
                st.video.removeAttribute('src');
                st.video.load();
                if (st.video.parentNode) st.video.parentNode.removeChild(st.video);
            } catch (e) {}
        }
        var video = document.createElement('video');
        video.playsInline = true;
        video.loop = !!loop;
        video.preload = 'auto';
        // Same-origin (/api/file/...); don't set crossOrigin (no canvas readback).
        var rawUrl = UTF8ToString(url);
        video.src = (rawUrl.charAt(0) === '/') ? ('/api/file' + rawUrl)
                                               : ('/api/file/' + rawUrl);
        // Fullscreen letterboxed overlay on top of everything (incl. the loading
        // overlay), matching the canvas aspect.
        video.style.cssText =
            'position:absolute;top:0;left:0;width:100%;height:100%;' +
            'object-fit:contain;z-index:2147483647;background:#000;' +
            'transform:translateZ(0);pointer-events:none;';
        st.ready = false;
        st.ended = false;
        video.addEventListener('loadedmetadata', function () { st.ready = true; });
        video.addEventListener('ended', function () { st.ended = true; });
        video.addEventListener('error', function (e) {
            console.warn('rb3 intro video failed to load', e);
            st.ended = true;  // graceful: advance past the intro on any error
        });
        var container = document.getElementById('canvas-container') || document.body;
        container.appendChild(video);
        st.video = video;
        // Try unmuted; browsers block unmuted autoplay without a user gesture, so
        // fall back to muted playback (visuals still play; audio resumes if/when a
        // gesture has primed the page).
        video.volume = 1.0;
        video.muted = false;
        video.play().catch(function () {
            video.muted = true;
            video.play().catch(function () {});
        });
        return 1;
    } catch (e) {
        console.warn('rb3_webmovie_create threw', e);
        return 0;
    }
});

EM_JS(int, rb3_webmovie_ready, (), {
    var st = Module._rb3Movie;
    return (st && st.ready) ? 1 : 0;
});

EM_JS(int, rb3_webmovie_ended, (), {
    var st = Module._rb3Movie;
    return (st && st.ended) ? 1 : 0;
});

EM_JS(void, rb3_webmovie_set_paused, (int paused), {
    var st = Module._rb3Movie;
    if (!st || !st.video) return;
    if (paused) {
        st.video.pause();
    } else {
        st.video.play().catch(function () {});
    }
});

EM_JS(void, rb3_webmovie_destroy, (), {
    var st = Module._rb3Movie;
    if (st && st.video) {
        try {
            st.video.pause();
            st.video.removeAttribute('src');
            st.video.load();
            if (st.video.parentNode) st.video.parentNode.removeChild(st.video);
        } catch (e) {}
        st.video = null;
        st.ready = false;
        st.ended = false;
    }
});

#else  // native desktop
#include <chrono>
#endif

namespace {
    bool gActive = false;  // a real fullscreen cinematic is in flight

#ifndef __EMSCRIPTEN__
    // Virtual-playback deadline for native desktop (RB3_INTRO_SECS).
    std::chrono::steady_clock::time_point gNativeEnd;

    double NativeRemainingSecs() {
        auto now = std::chrono::steady_clock::now();
        return std::chrono::duration<double>(gNativeEnd - now).count();
    }
#endif

    // Only intercept the fullscreen cinematics. Everything else (TexMovie
    // in-world videos) keeps its prior no-op web behaviour.
    bool IsFullscreenCinematic(const char *path) {
        if (!path) return false;
        return std::strstr(path, "rb3_intro_cinematic") != nullptr
            || std::strstr(path, "rb3_end_credits") != nullptr;
    }
}

extern "C" {

// Start a fullscreen cinematic. Returns 1 if a real movie began (so Poll keeps
// the screen up until it ends), 0 if skipped (not a cinematic, native default
// skip, or failure) — in which case Poll reports "done" immediately and the
// screen advances, preserving the prior boot behaviour.
int RB3MovieNativeBegin(const char *path, int loop, float /*volumeDb*/) {
    if (!IsFullscreenCinematic(path)) {
        gActive = false;
        return 0;
    }
#ifdef __EMSCRIPTEN__
    // Rewrite the .bik filename to the pre-transcoded .webm sidecar the browser
    // can decode (e.g. "videos/rb3_intro_cinematic.bik" -> ".../rb3_intro_cinematic.webm").
    char webm[512];
    {
        size_t n = std::strlen(path);
        if (n >= sizeof(webm) - 6) n = sizeof(webm) - 6;
        std::memcpy(webm, path, n);
        webm[n] = '\0';
        char *dot = std::strrchr(webm, '.');
        char *slash = std::strrchr(webm, '/');
        if (dot && (!slash || dot > slash)) {
            std::strcpy(dot, ".webm");
        } else {
            std::strcat(webm, ".webm");
        }
    }
    int ok = rb3_webmovie_create(webm, loop);
    gActive = (ok != 0);
    if (gActive) {
        printf("[rb3-intro] web overlay playing: %s\n", webm);
    }
    return gActive ? 1 : 0;
#else
    const char *env = getenv("RB3_INTRO_SECS");
    double secs = env ? atof(env) : 0.0;
    if (secs > 0.0) {
        gNativeEnd = std::chrono::steady_clock::now()
                   + std::chrono::duration_cast<std::chrono::steady_clock::duration>(
                         std::chrono::duration<double>(secs));
        gActive = true;
        printf("[rb3-intro] native virtual-play %.2fs: %s\n", secs, path);
        return 1;
    }
    // Default native: skip (unchanged boot).
    gActive = false;
    printf("[rb3-intro] native skip (set RB3_INTRO_SECS to play): %s\n", path);
    return 0;
#endif
}

int RB3MovieNativeReady(void) {
    if (!gActive) return 1;  // nothing playing -> let the screen finish loading
#ifdef __EMSCRIPTEN__
    return rb3_webmovie_ready();
#else
    return 1;
#endif
}

// 1 = still playing, 0 = ended/done/none.
int RB3MovieNativePoll(void) {
    if (!gActive) return 0;
#ifdef __EMSCRIPTEN__
    return rb3_webmovie_ended() ? 0 : 1;
#else
    return (NativeRemainingSecs() <= 0.0) ? 0 : 1;
#endif
}

void RB3MovieNativeSetPaused(int paused) {
    if (!gActive) return;
#ifdef __EMSCRIPTEN__
    rb3_webmovie_set_paused(paused);
#else
    // Native virtual-play: extend the deadline by the paused span (approximate —
    // we simply ignore pause for the virtual timer; the intro rarely pauses in
    // headless tests).
    (void)paused;
#endif
}

void RB3MovieNativeEnd(void) {
    if (!gActive) return;
    gActive = false;
#ifdef __EMSCRIPTEN__
    rb3_webmovie_destroy();
#endif
}

int RB3MovieNativeIsOpen(void) { return gActive ? 1 : 0; }

}  // extern "C"

#endif  // HX_NATIVE
