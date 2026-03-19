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
    if (sat == 0) {
        color.Set(val, val, val);
        return;
    }
    float q;
    if (val < 0.5f) {
        q = (sat + 1.0f) * val;
    } else {
        q = -(sat * val - (sat + val));
    }
    float p = val * 2.0f - q;
    float qmp = q - p;
    float third = 1.0f / 3.0f;
    float two_thirds = 2.0f / 3.0f;
    float t0 = hue + third;
    float t2 = hue - third;
    float one = 1.0f;
    float six = 6.0f;
    for (int i = 0; i < 3; i++) {
        float t;
        if (i == 0) { t = t0; }
        else if (i == 1) { t = hue; }
        else { t = t2; }
        if (t < 0) { t += one; }
        else if (t > one) { t -= one; }
        if (t * six < one) { color[i] = qmp * t * six + p; }
        else if (t * 2.0f < one) { color[i] = q; }
        else if (t * 3.0f < 2.0f) { t = two_thirds - t; color[i] = qmp * t * six + p; }
        else { color[i] = p; }
    }
}

void MakeHSL(const Hmx::Color &color, float &f1, float &f2, float &f3) {
    float maxCol = Max(color.red, color.green, color.blue);
    float minCol = Min(color.red, color.green, color.blue);
    f3 = (maxCol + minCol) / 2.0f;
    if (maxCol == minCol) {
        f1 = 0;
        f2 = 0;
    } else {
        float deltaCol = maxCol - minCol;
        if (f3 < 0.5f)
            f2 = deltaCol / (minCol + maxCol);
        else
            f2 = deltaCol / ((2.0f - maxCol) - minCol);
        if (color.red == maxCol) {
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
