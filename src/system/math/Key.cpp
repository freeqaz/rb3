#include "math/Key.h"
#include "Mtx.h"
#include "math/Vec.h"
#include "os/Debug.h"

void SplineTangent(const Keys<Vector3, Vector3> &keys, int i, Vector3 &vout) {
    int size = keys.size();
    MILO_ASSERT(size > 1, 0x17);
    if (size == 2) {
        Subtract(keys[1].value, keys[0].value, vout);
    } else if (i <= 0) {
        const Vector3 &k0 = keys[0].value;
        const Vector3 &k1 = keys[1].value;
        const Vector3 &k2 = keys[2].value;
        float dz = k1.z - k0.z;
        float dy = k1.y - k0.y;
        float dx = k1.x - k0.x;
        vout.z = dz * 1.5f;
        vout.y = dy * 1.5f;
        vout.x = dx * 1.5f;
        float ez = k2.z - k0.z;
        float ey = k2.y - k0.y;
        float ex = k2.x - k0.x;
        vout.z -= ez * 0.25f;
        vout.y -= ey * 0.25f;
        vout.x -= ex * 0.25f;
    } else if (i >= size - 1) {
        const Vector3 &k1 = keys[size - 1].value;
        const Vector3 &k2 = keys[size - 2].value;
        const Vector3 &k3 = keys[size - 3].value;
        float dz = k1.z - k2.z;
        float dy = k1.y - k2.y;
        float dx = k1.x - k2.x;
        vout.z = dz * 1.5f;
        vout.y = dy * 1.5f;
        vout.x = dx * 1.5f;
        float ez = k1.z - k3.z;
        float ey = k1.y - k3.y;
        float ex = k1.x - k3.x;
        vout.z -= ez * 0.25f;
        vout.y -= ey * 0.25f;
        vout.x -= ex * 0.25f;
    } else {
        Subtract(keys[i + 1].value, keys[i - 1].value, vout);
        Scale(vout, 0.5f, vout);
    }
}

void InterpTangent(
    const Vector3 &v1,
    const Vector3 &v2,
    const Vector3 &v3,
    const Vector3 &v4,
    float f,
    Vector3 &vout
) {
    float fsq = f * f;
    float fsq3 = 3.0f * fsq;
    float f6 = 6.0f * f;
    float fsq6 = 6.0f * fsq;
    float a = fsq6 - f6;
    float b = 1.0f + (fsq3 - 4.0f * f);
    float c = -6.0f * fsq + f6;
    float d = fsq3 - 2.0f * f;

    vout.z = v1.z * a + v2.z * b + v3.z * c + v4.z * d;
    vout.y = v1.y * a + v2.y * b + v3.y * c + v4.y * d;
    vout.x = v1.x * a + v2.x * b + v3.x * c + v4.x * d;
}

// fn_802E36D4 - InterpVector(const Keys<Vector3, Vector3>&, const Key<Vector3>*, const
// Key<Vector3>*, float, bool, Vector3&, Vector3*) https://decomp.me/scratch/hblrn -
// retail
#pragma fp_contract on
void InterpVector(
    const Keys<Vector3, Vector3> &keys,
    const Key<Vector3> *prev,
    const Key<Vector3> *next,
    float ref,
    bool spline,
    Vector3 &vref,
    Vector3 *vptr
) {
    Vector3 v7c;
    if (keys.size() < 3) {
        spline = false;
        if (keys.size() < 2) {
            if (vptr)
                vptr->Set(0.0f, 1.0f, 0.0f);
            if (keys.size() != 0)
                vref = prev->value;
            else
                vref.Set(0, 0, 0);
            return;
        }
    }
    int idx = prev - keys.begin();
    if (spline) {
        float fsq = ref * ref;
        float fsq3 = fsq * 3.0f;
        float fcubed = fsq * ref;
        float scale0 = fcubed * 2.0f;
        scale0 = scale0 - fsq3;
        scale0 = scale0 + 1.0f;
        Scale(prev->value, scale0, vref);
        Vector3 v70;
        SplineTangent(keys, idx, v70);
        Vector3 v88;
        float scale1 = fsq * 2.0f;
        scale1 = fcubed - scale1;
        scale1 = scale1 + ref;
        Scale(v70, scale1, v88);
        Add(vref, v88, vref);
        float scale2 = fcubed * -2.0f;
        scale2 = scale2 + fsq3;
        Scale(next->value, scale2, v88);
        Add(vref, v88, vref);
        SplineTangent(keys, idx + 1, v7c);
        Scale(v7c, fcubed - fsq, v88);
        Add(vref, v88, vref);
        if (vptr) {
            InterpTangent(prev->value, v70, next->value, v7c, ref, *vptr);
        }
    } else {
        Interp(prev->value, next->value, ref, vref);
        if (vptr) {
            if (idx == keys.size() - 1) {
                idx--;
            }
            Subtract(keys[idx + 1].value, keys[idx].value, *vptr);
        }
    }
}

void InterpVector(
    const Keys<Vector3, Vector3> &keys,
    bool spline,
    float frame,
    Vector3 &vref,
    Vector3 *vptr
) {
    const Key<Vector3> *prev;
    const Key<Vector3> *next;
    float ref;
    keys.AtFrame(frame, prev, next, ref);
    InterpVector(keys, prev, next, ref, spline, vref, vptr);
}

#pragma fp_contract on
static inline void NormalizeToInline(const Hmx::Quat &qin, Hmx::Quat &qout) {
    if (qin * qout < 0) {
        qout.w = -qout.w;
        qout.z = -qout.z;
        qout.y = -qout.y;
        qout.x = -qout.x;
    }
}

void QuatSpline(
    const Keys<Hmx::Quat, Hmx::Quat> &keys,
    const Key<Hmx::Quat> *prev,
    const Key<Hmx::Quat> *next,
    float ref,
    Hmx::Quat &qout
) {
    auto _tmp0 = keys.size();
    MILO_ASSERT(_tmp0, 0x9B);
    if (prev == next) {
        qout = prev->value;
    } else {
        int idx = prev - &keys.front();
        float fsq = ref * ref;
        float fcubed = fsq * ref;
        int idx1 = idx + 1;
        Hmx::Quat q58;
        Hmx::Quat nextQuat;
        Hmx::Quat prevQuat;
        Hmx::Quat q88;
        prevQuat = prev->value;
        nextQuat = next->value;
        q88 = idx == 0 ? prevQuat : keys[idx - 1].value;
        q58 = idx1 == keys.size() - 1 ? nextQuat : keys[idx1 + 1].value;
        NormalizeToInline(prevQuat, q88);
        NormalizeToInline(prevQuat, nextQuat);
        NormalizeToInline(prevQuat, q58);
        int i = 0;
        while (i < 4) {
            float p = prevQuat[i];
            float pp = q88[i];
            float n = nextQuat[i];
            float nn = q58[i];
            qout[i] = 0.5f * ((fcubed * (nn - (3.0f * n - (3.0f * p - pp))) + (fsq * ((4.0f * n + (2.0f * pp - 5.0f * p)) - nn) + (2.0f * p + ref * (n - pp)))));
            i++;
        }
        Normalize(qout, qout);
    }
}
#pragma fp_contract off