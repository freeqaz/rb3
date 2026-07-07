#include "math/Rand.h"
#include "math/Utl.h"
#include "os/Debug.h"
#include "os/OSFuncs.h"
#ifdef HX_NATIVE
#include "rb3_replay.h"  // W0.3d-b: RB3FixedClockActive / RB3LoadDeterminism
#include <cstdio>
#endif

Rand gRand(0x29A);

#ifdef HX_NATIVE
// BOOTRNG (Wave 11 A.S1): count every draw off the shared global gRand stream.
// Bumped inside Rand::Int() only for the &gRand instance (other Rand objects —
// CameraManager::sRand, transient locals — are NOT the shared stream and are
// excluded). Behaviour-neutral (a single unsigned increment on the draw path);
// HX_NATIVE-only so the MWCC match build is byte-identical.
static unsigned long sGRandDrawCount = 0;
unsigned long RB3GRandDrawCount() { return sGRandDrawCount; }

// W0.3d-b (Wave 12, A-S2): H-RESEED load-determinism seam. See Rand.h. Reseeds
// the shared global gRand to a canonical 0x5EED constant at the is_playing 0->1
// anchor. Gated on RB3FixedClockActive() && RB3LoadDeterminism() so a normal
// user boot (either flag off) NEVER touches the stream — flag-OFF byte-identical.
// Reseeding at each song-start (StartGame fires once per song) makes each song's
// post-anchor gameplay stream a deterministic function of the constant,
// independent of the boot-varying pre-anchor consumer-order shuffle A-S1 traced.
static int sReseedLogged = 0;
void RB3ReseedGRandAtAnchor(const char *reason) {
    if (!(RB3FixedClockActive() && RB3LoadDeterminism()))
        return;
    unsigned long before = sGRandDrawCount;
    gRand.Seed(0x5EED);
    if (sReseedLogged < 4) {
        ++sReseedLogged;
        fprintf(stderr,
                "[LOADDET] reseed anchor=%s seed=0x5EED gdrawBefore=%lu\n",
                reason ? reason : "?", before);
    }
}
#endif

Rand::Rand(int i)
    : mRandIndex1(0), mRandIndex2(0), mRandTable(), mSpareGaussianAvailable(0) {
    Seed(i);
}

void Rand::Seed(int seed) {
    for (int i = 0; i < 0x100; i++) {
        int j = seed * 0x41C64E6D + 0x3039;
        seed = j * 0x41C64E6D + 0x3039;
        mRandTable[i] = ((j >> 16) & 0xFFFF) | (seed & 0x7FFF0000);
    }
    mRandIndex1 = 0;
    mRandIndex2 = 0x67;
}

int Rand::Int(int low, int high) {
    MILO_ASSERT(high > low, 0x2B);
    return low + Int() % (high - low);
}

inline float Rand::Float(float f1, float f2) { return ((f2 - f1) * Float() + f1); }

float Rand::Float() { return ((Int() & 0xFFFF) / 65536.0f); }

int Rand::Int() {
#ifdef HX_NATIVE
    if (this == &gRand) ++sGRandDrawCount;
#endif
    unsigned int u3 = mRandTable[mRandIndex1];
    unsigned int u1 = mRandTable[mRandIndex2];
    mRandTable[mRandIndex1] = u3 ^ u1;
    if (0xF9 <= ++mRandIndex1)
        mRandIndex1 = 0;
    if (0xF9 <= ++mRandIndex2)
        mRandIndex2 = 0;
    return u3 ^ u1;
}

float Rand::Gaussian() {
    float f2, f3, f4, f5;

    if (mSpareGaussianAvailable) {
        mSpareGaussianAvailable = false;
        return mSpareGaussianValue;
    } else {
        do {
            do {
                f2 = Float(-1.0f, 1.0f);
                f3 = Float(-1.0f, 1.0f);
                f5 = f2 * f2 + f3 * f3;
            } while (f5 >= 1.0f);
        } while (0 == f5);
        f4 = std::log(f5);
        f5 = std::sqrt((-2.0f * f4) / f5);
        mSpareGaussianValue = f2 * f5;
        mSpareGaussianAvailable = true;
        return f3 * f5;
    }
}

void SeedRand(int seed) { gRand.Seed(seed); }

int RandomInt() {
    MILO_ASSERT(MainThread(), 0x6C);
    return gRand.Int();
}

int RandomInt(int i1, int i2) {
    MILO_ASSERT(MainThread(), 0x73);
    return gRand.Int(i1, i2);
}

float RandomFloat() {
    MILO_ASSERT(MainThread(), 0x79);
    return gRand.Float();
}

float RandomFloat(float f1, float f2) {
    MILO_ASSERT(MainThread(), 0x7F);
    return gRand.Float(f1, f2);
}
