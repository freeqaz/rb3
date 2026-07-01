// rb3_synth_native.cpp — native CreateNativeSynth() for the RB3 (clang LP64) build.
//
// X6: V1 audio backend bring-up. This is now a real NativeSynth: it registers
// the RB3-shaped StreamReceiver factory (RB3CreateNativeStreamReceiver,
// implemented in rb3_stream_receiver_native.cpp), boots the miniaudio AudioDevice,
// and pulls the optional audio_offset_ms latency tweak from SystemConfig("synth").
//
// We deliberately DO NOT override Synth::NewStream, NewBufStream, NewStreamDecoder,
// or NewStreamFile — RB3's base Synth already implements them correctly under
// HX_NATIVE (StandardStream + VorbisReader + .mogg file lookup, see
// rb3/src/system/synth/Synth.cpp:536-586). The engine's Synth_Stub.cpp duplicates
// those overrides for its own (DC3-shaped) StandardStream ctor; RB3's are fine.
#ifdef HX_NATIVE

#include "synth/Synth.h"
#include "synth/StandardStream.h"
#include "synth/StreamReceiver.h"
#include "obj/Data.h"
#include "os/System.h"          // SystemConfig(Symbol)
#include "audio/AudioDevice.h"  // miniaudio-backed mixer (engine layer)
#include "meta_band/ProfileMgr.h"  // TheProfileMgr (AV-calibration latency)
#include <cstdio>               // printf
#include <cstdlib>              // getenv

// Defined in rb3_stream_receiver_native.cpp.
extern StreamReceiver *RB3CreateNativeStreamReceiver(int numBuffers, int sampleRate, bool slip, int channel);

// ── Native AV-calibration: lock the note-highway to the audio clock ──────────
//
// THE BUG (audio leads the visuals at song start). The gameplay song clock that
// scrolls the note highway is derived from the audio clock minus a fixed Wii
// A/V calibration term applied every frame:
//
//   Game::Poll (src/band3/game/Game.cpp:1712-1716):
//     audioMs += GetSongToTaskMgrMs();          // = audio + offset
//     TheTaskMgr.SetSeconds(songMs / 1000.0f);  // drives BOTH track scroll AND
//                                               // hit-timing (kRealTime clock)
//
//   GetSongToTaskMgrMs(kGame) (ProfileMgr.cpp:1218-1220) =
//       mSongToTaskMgrMs - mInGameExtraVideoLatency
//
// With the boot defaults (ProfileMgr ctor):
//     mPlatformVideoLatency   = 50   →  mSongToTaskMgrMs = 50 - 0 = 50
//     mInGameExtraVideoLatency = 70
//   ⇒ GetSongToTaskMgrMs(kGame) = 50 - 70 = -20 ms
//
// So the visual highway (and the hit-timing clock) runs ~20 ms BEHIND the audio
// at EVERY frame. On the real Wii the 70 ms mInGameExtraVideoLatency compensated
// the display + GX pipeline latency; the native/web WebGPU renderer has no such
// pipeline latency, so the -20 ms is uncompensated and reads as "audio leads the
// visuals". (Empirically measured ~13-29 ms lead, present every gameplay frame.)
//
// FIX (native-only, match-neutral — the shared Wii decomp byte stays untouched):
// raise mInGameExtraVideoLatency to equal mSongToTaskMgrMs so the offset term is
// exactly 0, locking the track clock to the audio clock. This also keeps
// hit-timing self-consistent: the player hits to the audio, the gem visual is at
// the audio position, and the kRealTime clock that timestamps the hit equals the
// audio position — all three aligned. Opt out with RB3_NO_AV_CALIBRATION=1 to
// restore the literal Wii -20 ms behaviour.
//
// mInGameExtraVideoLatency is set ONLY in the ProfileMgr ctor and this setter —
// it is never reloaded from the save data — so a single boot-time call sticks.
// NOTE: this runs VERY early in boot (RunGame / web boot, before SystemPreInit),
// so the Milo string/MILO_LOG machinery isn't ready yet — use plain printf.
void RB3ApplyNativeAVCalibration() {
    if (getenv("RB3_NO_AV_CALIBRATION")) {
        printf("RB3 AV-cal: DISABLED (RB3_NO_AV_CALIBRATION) — Wii -20ms track lag\n");
        return;
    }
    // GetSongToTaskMgrMs(kGame) = mSongToTaskMgrMs - mInGameExtraVideoLatency.
    // Set the extra-video term equal to mSongToTaskMgrMsRaw() so the offset is 0.
    float target = TheProfileMgr.GetSongToTaskMgrMsRaw();
    TheProfileMgr.SetInGameExtraVideoLatency(target);
    printf("RB3 AV-cal: locked track to audio "
           "(mInGameExtraVideoLatency=%.1f -> GetSongToTaskMgrMs(kGame)=0)\n",
           target);
}

class NativeSynth final : public Synth {
public:
    NativeSynth() = default;

    void Init() override {
        Synth::Init();

        // Register the RB3-shaped StreamReceiver factory. StandardStream::InitInfo
        // calls StreamReceiver::New(numBuffers, sampleRate, slip, channel) per
        // channel; with sFactory bound, each call lands in our native impl.
        StreamReceiver::sFactory = &RB3CreateNativeStreamReceiver;

        // Initialize miniaudio output. MILO_HEADLESS=1 / DC3_NO_AUDIO=1 short-
        // circuit inside AudioDevice::Init (no real device opened), so the smoke
        // tests run silent but still exercise the synth/stream code paths.
        AudioDevice::GetInstance().Init(44100);

        // Optional latency offset (ms): mirrors the engine's Synth_Stub flow.
        // StandardStream::GetTime() returns mLastStreamTime + sAudioOffsetMs.
        DataArray *cfg = SystemConfig("synth");
        if (cfg) {
            DataArray *offsetArr = cfg->FindArray("audio_offset_ms", false);
            if (offsetArr) {
                StandardStream::sAudioOffsetMs = offsetArr->Float(1);
            }
        }
    }

    void Terminate() override {
        AudioDevice::GetInstance().Terminate();
        Synth::Terminate();
    }
};

Synth *CreateNativeSynth() { return new NativeSynth(); }

#endif // HX_NATIVE
