#include "math/Color.h"
#include "math/Utl.h"
#include "utl/TextStream.h"
#include "os/Debug.h"

Hmx::Color Hmx::Color::kWhite(1.0f, 1.0f, 1.0f, 1.0f);
Hmx::Color Hmx::Color::kTransparent(0.0f, 0.0f, 0.0f, 0.0f);
Hmx::Color Hmx::Color::kBlack(0.0f, 0.0f, 0.0f, 1.0f);

DECOMP_FORCEACTIVE(
    Color,
    __FILE__,
    "( 0.f) <= (hue) && (hue) <= ( 360.f)",
    "( 0.f) <= (sat) && (sat) <= ( 1.f)",
    "( 0.f) <= (val) && (val) <= ( 1.f)"
);

void MakeColor(float hue, float sat, float val, Hmx::Color &color) {
    if (sat == 0.0f) {
        color.Set(val, val, val);
        return;
    }
    float q;
    if (val < 0.5f) {
        q = val * (sat + 1.0f);
    } else {
        q = -(sat * val - (val + sat));
    }
    float t0 = (1.0f / 3.0f) + hue;
    float p = val * 2.0f - q;
    float qmp = q - p;
    float qmp_six = 6.0f * qmp;
    float t;
    for (int i = 0; i < 3; i++) {
        switch (i) {
            default: t = hue - (1.0f / 3.0f); break;
            case 1: t = hue; break;
            case 0: t = t0; break;
        }

        if (t < 0.0f) {
            t += 1.0f;
        } else if (t > 1.0f) {
            t -= 1.0f;
        }

        if (t * 6.0f < 1.0f) {
            color[i] = t * qmp_six + p;
        } else if (t * 2.0f < 1.0f) {
            color[i] = q;
        } else if (t * 3.0f < 2.0f) {
            color[i] = 6.0f * (((2.0f / 3.0f) - t) * qmp) + p;
        } else {
            color[i] = p;
        }
    }
}

void MakeHSL(const Hmx::Color &color, float &f1, float &f2, float &f3) {
    float maxCol = Max(Max(color.green, color.red), color.blue);
    float minCol = Min(Min(color.red, color.green), color.blue);
    f3 = (maxCol + minCol) / 2.0f;
    if (minCol == maxCol) {
        f1 = 0;
        f2 = 0;
    } else {
        float deltaCol = maxCol - minCol;
        if (f3 < 0.5f)
            f2 = deltaCol / (minCol + maxCol);
        else
            f2 = deltaCol / ((2.0f - maxCol) - minCol);
        if (maxCol == color.red) {
            f1 = (color.green - color.blue) / deltaCol;
        } else if (color.green == maxCol) {
            f1 = (color.blue - color.red) / deltaCol + 2.0f;
        } else {
            f1 = (color.red - color.green) / deltaCol + 4.0f;
        }
        f1 /= 6.0f;
        if (f1 < 0.0f)
            f1 += 1.0f;
    }
}

TextStream &operator<<(TextStream &ts, const Hmx::Color &color) {
    ts << "(r:" << color.red << " g:" << color.green << " b:" << color.blue
       << " a:" << color.alpha << ")";
    return ts;
}
