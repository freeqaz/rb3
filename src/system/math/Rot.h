#pragma once
#include "utl/TextStream.h"
#include "math/Vec.h"
#include "math/Mtx.h"

#define PI 3.1415927f
#define RAD2DEG 57.2957763671875f
#define DEG2RAD 0.01745329238474369049f

void Multiply(const Vector3 &, const Transform &, Vector3 &);
inline void Multiply(const Transform &t, const Vector3 &v, Vector3 &out) {
    float dy = v.y - t.v.y;
    float dx = v.x - t.v.x;
    float sz = t.m.z.y * dy;
    float sy = t.m.y.y * dy;
    float sx = t.m.x.y * dy;
    sz += t.m.z.x * dx;
    sy += t.m.y.x * dx;
    float dz = v.z - t.v.z;
    sx += t.m.x.x * dx;
    out.z = t.m.z.z * dz + sz;
    out.y = t.m.y.z * dz + sy;
    out.x = t.m.x.z * dz + sx;
}
void Multiply(const Plane &, const Transform &, Plane &);
void MakeRotMatrix(const Vector3 &, Hmx::Matrix3 &, bool);
void MakeScale(const Hmx::Matrix3 &, Vector3 &);
void MakeEuler(const Hmx::Matrix3 &, Vector3 &);
void MakeEulerScale(const Hmx::Matrix3 &, Vector3 &, Vector3 &);
void Normalize(const Hmx::Matrix3 &, Hmx::Matrix3 &);
void MakeRotMatrix(const Hmx::Quat &, Hmx::Matrix3 &);
void MakeRotMatrix(const Vector3 &, const Vector3 &, Hmx::Matrix3 &);
void Interp(const Hmx::Quat &, const Hmx::Quat &, float, Hmx::Quat &);
void RotateAboutX(const Hmx::Matrix3 &, float, Hmx::Matrix3 &);
void RotateAboutZ(const Hmx::Matrix3 &, float, Hmx::Matrix3 &);
void MakeRotQuat(const Vector3 &, const Vector3 &, Hmx::Quat &);
void MakeRotQuatUnitX(const Vector3 &, Hmx::Quat &);
float GetXAngle(const Hmx::Matrix3 &);
float GetYAngle(const Hmx::Matrix3 &);
float GetZAngle(const Hmx::Matrix3 &);
void MakeEuler(const Hmx::Quat&, Vector3&);
void MakeVertical(Hmx::Matrix3 &);
void MultiplyStoreTransposed(const Transform &, const Transform &, float (&)[3][4]);
void MultiplyStoreTransposed(const Transform &, const Transform &, const Transform &, float (&)[3][4]);

TextStream &operator<<(TextStream &ts, const Hmx::Quat &v);
TextStream &operator<<(TextStream &ts, const Vector3 &v);
TextStream &operator<<(TextStream &ts, const Vector2 &v);
TextStream &operator<<(TextStream &ts, const Hmx::Matrix3 &m);
TextStream &operator<<(TextStream &ts, const Transform &t);
