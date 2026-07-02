// rb3_trace_taps_stub.cpp — rb3-dta-ONLY stand-in for the single symbol
// rb3_replay.cpp pulls from rb3_trace_taps.cpp: the live song-clock accessor
// RB3TraceCurrentSongMs(). The real tap walks
// TheGame->GetBeatMaster()->GetAudio()->GetTime(), dragging band3 game/UI
// types (Game/Player/BandUI/UIScreen) that rb3-dta's obj/utl/os/math fork
// subset deliberately does not compile.
//
// Returning -1.0f is the real tap's exact "engine not booted" value
// (TheGame == NULL) — rb3_replay.cpp's ReplayLiveSongMs()/audio-start latch
// documents and handles live < 0 by falling back to the recorded-curve
// signal, so replay lookups (never exercised by the DTA parse anyway) stay
// well-defined.
//
// WEAK so the real rb3_trace_taps.cpp definition wins if a future target
// ever compiles both (rb3-native / rb3-tests / rb3-web link the real tap and
// never see this TU).

#ifdef HX_NATIVE

__attribute__((weak)) float RB3TraceCurrentSongMs() { return -1.0f; }

#endif // HX_NATIVE
