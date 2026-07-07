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
#endif

#endif
