#include "math/Rand.h"
#include "math/Utl.h"
#include "os/Debug.h"
#include "os/OSFuncs.h"
#ifdef HX_NATIVE
#include "rb3_replay.h"  // W0.3d-b: RB3FixedClockActive / RB3LoadDeterminism
#include <cstdio>
#include <map>
#include <string>
#endif

Rand gRand(0x29A);

#ifdef HX_NATIVE
// R4 (Wave 17, Lane L): per-consumer isolated Rand streams. Registry is created
// and touched only under the seam (RB3FixedClockActive() && RB3LoadDeterminism()),
// main-thread only, so a normal user boot never allocates it — flag-OFF is
// byte-identical. See Rand.h for the determinism rationale.
namespace {
    std::map<std::string, Rand *> sDetStreams;
    // Active per-tag redirect (main-thread). Non-null only inside an
    // RB3LoadDetRedirect scope AND only when the seam is on; the four free-fn
    // wrappers draw from it instead of gRand when set. Null on default boots ->
    // one predicted-not-taken branch, byte-identical.
    Rand *sDetRedirect = NULL;

    // FNV-1a over the tag, folded into the canonical 0x5EED anchor constant so
    // each stream seeds independently but deterministically.
    unsigned RB3DetTagSeed(const char *tag) {
        unsigned h = 2166136261u;
        for (const char *p = tag; *p; ++p) {
            h ^= (unsigned char)*p;
            h *= 16777619u;
        }
        return 0x5EEDu ^ h;
    }
}
#endif

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
    // Reset every registered per-tag stream too, so post-anchor per-consumer
    // state is independent of pre-anchor consumption (same rationale as gRand).
    for (std::map<std::string, Rand *>::iterator it = sDetStreams.begin();
         it != sDetStreams.end(); ++it)
        it->second->Seed((int)RB3DetTagSeed(it->first.c_str()));
    if (sReseedLogged < 4) {
        ++sReseedLogged;
        fprintf(stderr,
                "[LOADDET] reseed anchor=%s seed=0x5EED gdrawBefore=%lu\n",
                reason ? reason : "?", before);
    }
}

// See Rand.h. Lazily create + seed the per-tag stream; nullptr when the seam is
// off (default boots) -> callers fall back to the shared gRand path.
Rand *RB3LoadDetStream(const char *tag) {
    if (!(RB3FixedClockActive() && RB3LoadDeterminism()))
        return NULL;
    Rand *&r = sDetStreams[tag];
    if (!r)
        r = new Rand((int)RB3DetTagSeed(tag));
    return r;
}

// Scoped redirect guard. Engages only when the seam is on (RB3LoadDetStream !=
// null); otherwise sDetRedirect stays whatever it was (null on default boots) and
// the guard is a no-op save/restore -> byte-identical flag-OFF.
RB3LoadDetRedirect::RB3LoadDetRedirect(const char *tag) : mPrev(sDetRedirect) {
    if (Rand *r = RB3LoadDetStream(tag))
        sDetRedirect = r;
}
RB3LoadDetRedirect::~RB3LoadDetRedirect() { sDetRedirect = mPrev; }
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

#ifdef HX_NATIVE
// R4 (Wave-17 Lane L) attribution tap. gRB3LoadDetAttribOn is armed once at static
// init in rb3_loaddet_probe.cpp; flag-OFF (normal boots AND the M2/M3 gate arms)
// is a single predicted-not-taken load+branch — no call, no return-address eval.
// __builtin_return_address(0) is captured INSIDE each wrapper so it resolves to
// the actual gRand consumer (CamShot::Shake, WorldCrowd::OnIterateFrac, ...).
// Every free-function wrapper produces exactly one gRand draw, so one record per
// wrapper entry matches the sGRandDrawCount increment count.
extern int gRB3LoadDetAttribOn;
extern void RB3LoadDetAttribRecord(void *pc);
#define RB3_LOADDET_ATTRIB_TAP()                                                   \
    do {                                                                           \
        if (gRB3LoadDetAttribOn)                                                   \
            RB3LoadDetAttribRecord(__builtin_return_address(0));                   \
    } while (0)
// R4: when inside an RB3LoadDetRedirect scope (seam-ON), draw from the isolated
// per-tag stream and return, bypassing gRand entirely. `expr` is the matching
// Rand member call. Predicted-not-taken load+branch on default boots.
#define RB3_LOADDET_REDIR(expr)                                                    \
    do {                                                                           \
        if (sDetRedirect)                                                          \
            return sDetRedirect->expr;                                             \
    } while (0)
#else
#define RB3_LOADDET_ATTRIB_TAP() ((void)0)
#define RB3_LOADDET_REDIR(expr) ((void)0)
#endif

int RandomInt() {
    MILO_ASSERT(MainThread(), 0x6C);
    RB3_LOADDET_ATTRIB_TAP();
    RB3_LOADDET_REDIR(Int());
    return gRand.Int();
}

int RandomInt(int i1, int i2) {
    MILO_ASSERT(MainThread(), 0x73);
    RB3_LOADDET_ATTRIB_TAP();
    RB3_LOADDET_REDIR(Int(i1, i2));
    return gRand.Int(i1, i2);
}

float RandomFloat() {
    MILO_ASSERT(MainThread(), 0x79);
    RB3_LOADDET_ATTRIB_TAP();
    RB3_LOADDET_REDIR(Float());
    return gRand.Float();
}

float RandomFloat(float f1, float f2) {
    MILO_ASSERT(MainThread(), 0x7F);
    RB3_LOADDET_ATTRIB_TAP();
    RB3_LOADDET_REDIR(Float(f1, f2));
    return gRand.Float(f1, f2);
}
