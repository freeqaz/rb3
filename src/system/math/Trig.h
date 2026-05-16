#pragma once
#include "obj/Data.h"
#include <cmath>

void TrigTableInit(); // fn_802E2E28
void TrigTableTerminate();
float Lookup(float); // fn_802E2F90
float Sine(float); // fn_802E2F38
float FastSin(float); // fn_802E2FE8

inline float FastCos(float f) { return FastSin(f + 1.570796370506287f); }

// fn_802DE4D4
inline float Cosine(float f) { return Sine(f + 1.5707964f); }

inline float DegreesToRadians(float deg) { return 0.017453292f * deg; }

inline float RadiansToDegrees(float rad) { return 57.295776f * rad; }

inline float LimitAng(float ang) {
    float r = (float)fmod(ang + 3.1415927f, 2.0 * 3.1415927f);
    if (r < 0.0f)
        return r + 3.1415927f;
    else
        return r - 3.1415927f;
}

DataNode DataSin(DataArray *);
DataNode DataCos(DataArray *);
DataNode DataTan(DataArray *);
DataNode DataASin(DataArray *);
DataNode DataACos(DataArray *);
DataNode DataATan(DataArray *);
void TrigInit();
