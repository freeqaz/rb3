#ifndef MATH_RAND_H
#define MATH_RAND_H

#include "math/Utl.h"
#include "utl/MemMgr.h"

class Rand {
public:
    Rand(int);
    void Seed(int);
    int Int();
    int Int(int, int);
    float Float();
    float Float(float, float);
    float Gaussian();

    unsigned int mRandIndex1;
    unsigned int mRandIndex2;
    unsigned int mRandTable[256];
    float mSpareGaussianValue;
    bool mSpareGaussianAvailable;

    NEW_OVERLOAD
    DELETE_OVERLOAD
};

void SeedRand(int);
int RandomInt();
int RandomInt(int, int);
float RandomFloat();
float RandomFloat(float, float);

#ifdef HX_NATIVE
// BOOTRNG (Wave 11 A.S1, diagnosis-only): global-stream position probe. Every
// draw off the shared global gRand instance (Rand.cpp) bumps this counter, so
// any consumer (LightPresetManager preset picks, BandDirector, etc.) can log the
// stream position at a pinned capture and prove whether it varies per boot under
// RB3_FIXED_CLOCK. Additive, HX_NATIVE-only -> MWCC match build byte-identical.
unsigned long RB3GRandDrawCount();

// W0.3d-b (Wave 12, A-S2): H-RESEED load-determinism seam. Reseeds the shared
// global gRand stream to a canonical constant, but ONLY when
// RB3FixedClockActive() && RB3LoadDeterminism() (both default-OFF). Called at the
// is_playing 0->1 anchor (GamePanel::StartGame) so the post-anchor stream — and
// thus the pinned BOOTRNG capture — collapses to one boot-invariant position.
// `reason` is a short tag logged once for provenance. Inert (no reseed, no log)
// unless BOTH gates are on; flag-OFF is byte-identical.
void RB3ReseedGRandAtAnchor(const char *reason);

// R4 (Wave 17, Lane L): per-consumer isolated Rand streams. Returns the stream
// for `tag` when the load-determinism seam is active
// (RB3FixedClockActive() && RB3LoadDeterminism()), else nullptr. Streams are
// lazily created, seeded 0x5EED ^ fnv1a(tag), and reset by
// RB3ReseedGRandAtAnchor so post-anchor per-consumer state is boot-invariant.
// Main-thread only (same MainThread() contract as RandomInt). M1 attribution
// (evidence/M1-divergent-consumers.md) named the variable-count gRand consumers
// whose per-frame draw COUNT diverges run-to-run under fixed clock; routing each
// onto its own stream removes it from the shared gRand count, so the global
// per-frame gRand draw count becomes a sum of fixed-count consumers -> the
// post-anchor gRand stream position (the PRIMARY gate metric) is boot-invariant
// by construction. Default boots (either flag off) get nullptr -> the normal
// RandomInt/RandomFloat path, byte-identical.
Rand *RB3LoadDetStream(const char *tag);

// Scoped redirect: for the dynamic extent of this guard, the shared-stream
// RandomInt/RandomFloat free functions draw from the per-tag isolated stream
// instead of gRand (seam-ON only; when the seam is off RB3LoadDetStream returns
// nullptr and the guard is inert — no redirect, byte-identical). Declared at the
// top of a measured variable-count gRand consumer (M1: RndParticleSys::InitParticle
// / CreateParticles, CamShot::Shake, CharEyes::NextLook, RandomGroupSeq::PickNextIndex)
// so ALL of that consumer's draws — across every caller, and any particle helper
// it calls — leave the shared gRand count with a single line. That makes the
// global per-frame gRand count a sum of fixed-count consumers -> the post-anchor
// gRand stream position (PRIMARY gate) is boot-invariant. Nesting is safe: the
// inner guard restores the outer stream on scope exit. Main-thread only.
struct RB3LoadDetRedirect {
    Rand *mPrev;
    RB3LoadDetRedirect(const char *tag);
    ~RB3LoadDetRedirect();
};
#endif

#endif
