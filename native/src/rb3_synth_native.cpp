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

// Defined in rb3_stream_receiver_native.cpp.
extern StreamReceiver *RB3CreateNativeStreamReceiver(int numBuffers, int sampleRate, bool slip, int channel);

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
