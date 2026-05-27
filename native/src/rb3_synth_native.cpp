// rb3_synth_native.cpp — native CreateNativeSynth() for the RB3 (clang LP64) build.
//
// synth/Synth.cpp SynthPreInit() (the real boot path, via App::App -> SynthInit)
// calls CreateNativeSynth() when the config doesn't request use_null_synth. On
// Wii that seam is Synth::New() -> the Wii audio-HW synth. The shared engine
// supplies a miniaudio-backed NativeSynth (platform/Synth_Stub.cpp), but it's
// excluded from the RB3 link because RB3's StandardStream ctor differs (see
// native/CMakeLists.txt). Until the audio path is brought up (Phase 3) — and we
// have no .mogg/.mid song assets extracted yet anyway — return the base no-op
// Synth, exactly equivalent to the engine's `use_null_synth` path
// (Synth.cpp:270 `TheSynth = new Synth()`). This keeps the real SynthPreInit/
// SynthInit code path intact (TheSynth non-null, Fail()==false) with no audio
// device. Phase 3 swaps this for the engine's NativeSynth once RB3's stream
// shape is reconciled.
#ifdef HX_NATIVE

#include "synth/Synth.h"

Synth *CreateNativeSynth() { return new Synth(); }

#endif // HX_NATIVE
