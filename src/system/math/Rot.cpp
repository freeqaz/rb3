#include "Rot.h"
#include "math/Mtx.h"
#include "utl/BinStream.h"
#include <cmath>

Transform &TransformNoScale::ToTransform(Transform &tf) const {
    Hmx::Quat tmpq;
    q.ToQuat(tmpq);
    MakeRotMatrix(tmpq, tf.m);
    tf.v = v;
    return tf;
}

void TransformNoScale::Set(const Transform &tf) {
    SetRot(tf.m);
    v = tf.v;
}

void TransformNoScale::Set(const TransformNoScale &t) {
    q.x = t.q.x;
    q.y = t.q.y;
    q.z = t.q.z;
    q.w = t.q.w;
    v.x = t.v.x;
    v.y = t.v.y;
    v.z = t.v.z;
}

void TransformNoScale::SetRot(const Hmx::Matrix3 &m) {
    Hmx::Quat quat;
    quat.Set(m);

    float nu = 32767.0f * quat.x + 0.5f;
    q.x = floorf(nu > 32767.0f ? 32767.0f : (nu < -32767.0f ? -32767.0f : nu));

    nu = 32767.0f * quat.y + 0.5f;
    q.y = floorf(nu > 32767.0f ? 32767.0f : (nu < -32767.0f ? -32767.0f : nu));

    nu = 32767.0f * quat.z + 0.5f;
    q.z = floorf(nu > 32767.0f ? 32767.0f : (nu < -32767.0f ? -32767.0f : nu));

    nu = 32767.0f * quat.w + 0.5f;
    q.w = floorf(nu > 32767.0f ? 32767.0f : (nu < -32767.0f ? -32767.0f : nu));
}

void TransformNoScale::GetRot(Hmx::Quat &qout) const {
    short qw = q.w, qz = q.z, qy = q.y, qx = q.x;
    qout.w = qw * 0.000030518509f;
    qout.z = qz * 0.000030518509f;
    qout.y = qy * 0.000030518509f;
    qout.x = qx * 0.000030518509f;
}

void TransformNoScale::SetRot(const Hmx::Quat &quat) {
    float nu = 32767.0f * quat.x + 0.5f;
    q.x = floorf(nu > 32767.0f ? 32767.0f : (nu < -32767.0f ? -32767.0f : nu));

    nu = 32767.0f * quat.y + 0.5f;
    q.y = floorf(nu > 32767.0f ? 32767.0f : (nu < -32767.0f ? -32767.0f : nu));

    nu = 32767.0f * quat.z + 0.5f;
    q.z = floorf(nu > 32767.0f ? 32767.0f : (nu < -32767.0f ? -32767.0f : nu));

    nu = 32767.0f * quat.w + 0.5f;
    q.w = floorf(nu > 32767.0f ? 32767.0f : (nu < -32767.0f ? -32767.0f : nu));
}

void TransformNoScale::Reset() {
    q.Reset();
    v.Zero();
}

BinStream &operator>>(BinStream &bs, TransformNoScale &t) {
    Hmx::Matrix3 m;
    bs >> m;
    bs >> t.v;
    t.SetRot(m);
    return bs;
}

void Normalize(const Hmx::Quat &qin, Hmx::Quat &qout) {
#ifdef __MWERKS__
    typedef __vec2x32float__ psq;
    register const Hmx::Quat *_qin = &qin;
    register Hmx::Quat *_qout = &qout;
    register psq _xy;
    register psq _zw;
    register psq _xy2;
    register psq _zw2;
    register psq _len2;
    register float _eps = 1e-5f;
    register float _zero;
    register float _half = 0.5f;
    register float _three = 3.0f;
    asm {
        psq_l _xy, 0(_qin), 0, 0
        psq_l _zw, 8(_qin), 0, 0
        ps_mul _xy2, _xy, _xy
        ps_mul _zw2, _zw, _zw
        ps_sum0 _len2, _xy2, _zw2, _xy2
        ps_sum1 _zw2, _zw2, _xy2, _zw2
        ps_sub _zero, _eps, _eps
        ps_sum0 _len2, _len2, _xy2, _zw2
        frsqrte _xy2, _len2
        ps_sub _eps, _len2, _eps
        fmul _zw2, _xy2, _xy2
        fmul _xy2, _xy2, _half
        fnmsub _zw2, _zw2, _len2, _three
        fmul _xy2, _zw2, _xy2
        ps_sel _xy2, _eps, _xy2, _zero
        ps_muls0 _xy, _xy, _xy2
        ps_muls0 _zw, _zw, _xy2
        psq_st _xy, 0(_qout), 0, 0
        psq_st _zw, 8(_qout), 0, 0
    }
#else
    float lenSq = qin.x*qin.x + qin.y*qin.y + qin.z*qin.z + qin.w*qin.w;
    if (lenSq > 1e-5f) {
        float invLen = 1.0f / std::sqrt(lenSq);
        qout.x = qin.x * invLen;
        qout.y = qin.y * invLen;
        qout.z = qin.z * invLen;
        qout.w = qin.w * invLen;
    } else {
        qout = qin;
    }
#endif
}

void Hmx::Quat::Set(const Vector3 &v, float f) {
    float half = f * 0.5f;
    float scale = Sine(half);
    w = Cosine(half);
    x = v.x * scale;
    y = v.y * scale;
    z = v.z * scale;
}

void MakeEuler(const Hmx::Matrix3 &m, Vector3 &v) {
    if (fabsf(m.y.z) > 0.99999988f) {
        if (m.y.z > 0) {
            v.x = PI / 2;
        } else {
            v.x = -PI / 2;
        }
        v.z = std::atan2(m.x.y, m.x.x);
        v.y = 0;
    } else {
        v.z = std::atan2(-m.y.x, m.y.y);
        v.x = std::asin(m.y.z);
        v.y = std::atan2(-m.x.z, m.z.z);
    }
}

void MakeVertical(Hmx::Matrix3 &m) {
    m.z.Set(0.0f, 0.0f, 1.0f);
    m.y.z = 0.0f;
    Normalize(m.y, m.y);
    Cross(m.y, m.z, m.x);
}

void MakeScale(const Hmx::Matrix3 &m, Vector3 &v) {
    float zlen = Length(m.z);
    float cx = m.x.y * m.y.z - m.x.z * m.y.y;
    float cy = m.x.z * m.y.x - m.x.x * m.y.z;
    float cz = m.x.x * m.y.y - m.x.y * m.y.x;
    float xlen = Length(m.x);
    float ylen = Length(m.y);
    float dot = cx * m.z.x + cy * m.z.y + cz * m.z.z;
    if (dot <= 0.0f)
        zlen = -zlen;
    v.Set(xlen, ylen, zlen);
}

void MakeEulerScale(const Hmx::Matrix3 &m1, Vector3 &v2, Vector3 &v3) {
    MakeScale(m1, v3);
    Hmx::Matrix3 m38;
    float inv_x = v3.x ? 1.0f / v3.x : 0.0f;
    m38.x.z = inv_x;
    Scale(m1.x, inv_x, m38.x);
    float inv_y = v3.y ? 1.0f / v3.y : 0.0f;
    m38.y.z = inv_y;
    Scale(m1.y, inv_y, m38.y);
    float inv_z = v3.z ? 1.0f / v3.z : 0.0f;
    m38.z.z = inv_z;
    Scale(m1.z, inv_z, m38.z);
    MakeEuler(m38, v2);
}

float GetXAngle(const Hmx::Matrix3 &m) {
    float z = m.y.z;
    return std::atan2(z, m.y.y);
}

float GetYAngle(const Hmx::Matrix3 &m) {
    float z = -m.x.z;
    return std::atan2(z, m.z.z);
}

float GetZAngle(const Hmx::Matrix3 &m) {
    float x = m.y.x;
    return -std::atan2(x, m.y.y);
}

void Hmx::Quat::Set(const Vector3 &v) {
    float sx = v.x * 0.5f;
    float sz = v.z * 0.5f;
    float sy = v.y * 0.5f;
    float f1 = Sine(sx);
    float f2 = Cosine(sx);
    float f3 = Sine(sy);
    float f4 = Cosine(sy);
    Set(f1 * f4, f2 * f3, f1 * f3, f2 * f4);
    f1 = Sine(sz);
    f2 = Cosine(sz);
    Set(f2 * x - f1 * y, f2 * y + f1 * x, f2 * z + f1 * w, f2 * w - f1 * z);
}

void Hmx::Quat::Set(const Hmx::Matrix3 &m) {
    float mxx = m.x.x;
    float myy = m.y.y;
    float mzz = m.z.z;
    float trace = mxx + myy + mzz;
    if (trace > 0.0f) {
        w = trace + 1.0f;
        x = m.y.z - m.z.y;
        y = m.z.x - m.x.z;
        z = m.x.y - m.y.x;
    } else if (mzz > mxx && mzz > myy) {
        // i=2: m.z.z is the largest diagonal
        z = mzz - mxx - myy + 1.0f;
        w = m.x.y - m.y.x;
        x = m.z.x + m.x.z;
        y = m.z.y + m.y.z;
    } else if (myy > mxx) {
        // i=1: m.y.y is the largest diagonal
        y = myy - mzz - mxx + 1.0f;
        w = m.z.x - m.x.z;
        z = m.y.z + m.z.y;
        x = m.y.x + m.x.y;
    } else {
        // i=0: m.x.x is the largest diagonal
        x = mxx - myy - mzz + 1.0f;
        w = m.y.z - m.z.y;
        y = m.x.y + m.y.x;
        z = m.x.z + m.z.x;
    }
    Normalize(*this, *this);
}

void FastInterp(const Hmx::Quat &q1, const Hmx::Quat &q2, float f, Hmx::Quat &qout) {
    if (f == 0) {
        qout = q1;
    } else if (f == 1) {
        qout = q2;
    } else {
        if (q1 * q2 < 0) {
            qout.x = -(f * (q1.x + q2.x) - q1.x);
            qout.y = -(f * (q1.y + q2.y) - q1.y);
            qout.z = -(f * (q1.z + q2.z) - q1.z);
            qout.w = -(f * (q1.w + q2.w) - q1.w);
        } else {
            qout.x = f * (q2.x - q1.x) + q1.x;
            qout.y = f * (q2.y - q1.y) + q1.y;
            qout.z = f * (q2.z - q1.z) + q1.z;
            qout.w = f * (q2.w - q1.w) + q1.w;
        }
        Normalize(qout, qout);
    }
}

void IdentityInterp(const Hmx::Quat &qin, float f, Hmx::Quat &qout) {
    if (f == 0) {
        qout = qin;
    } else if (f == 1) {
        qout.Set(0, 0, 0, 1);
    } else {
        float diff = 1.0f - f;
        qout.x = qin.x * diff;
        qout.y = qin.y * diff;
        qout.z = qin.z * diff;
        if (qin.w < 0) {
            qout.w = qin.w * diff - f;
        } else {
            qout.w = qin.w * diff + f;
        }
        Normalize(qout, qout);
    }
}

void Interp(const Hmx::Quat &q1, const Hmx::Quat &q2, float r, Hmx::Quat &qres) {
    if (r == 0) {
        qres = q1;
    } else if (r == 1) {
        qres = q2;
    } else {
        if (q1 * q2 < 0) {
            qres.x = -((q1.x + q2.x) * r - q1.x);
            qres.y = -((q1.y + q2.y) * r - q1.y);
            qres.z = -((q1.z + q2.z) * r - q1.z);
            qres.w = -((q1.w + q2.w) * r - q1.w);
        } else {
            qres.x = -((q1.x - q2.x) * r - q1.x);
            qres.y = -((q1.y - q2.y) * r - q1.y);
            qres.z = -((q1.z - q2.z) * r - q1.z);
            qres.w = -((q1.w - q2.w) * r - q1.w);
        }
        Normalize(qres, qres);
    }
}

void Interp(const Hmx::Matrix3 &m1, const Hmx::Matrix3 &m2, float r, Hmx::Matrix3 &res) {
    Hmx::Quat q40(m1);
    Hmx::Quat q50(m2);
    Hmx::Quat q60;
    Interp(q40, q50, r, q60);
    MakeRotMatrix(q60, res);
}

void MakeRotMatrix(const Vector3 &v, Hmx::Matrix3 &mtx, bool lookup) {
    float xcos, xsin, ycos, ysin, zsin, zcos;
    if (lookup) {
        zsin = Sine(v.z);
        zcos = Cosine(v.z);
        ysin = Sine(v.y);
        ycos = Cosine(v.y);
        xsin = Sine(v.x);
        xcos = Cosine(v.x);
    } else {
        float vz = v.z;
        zsin = sinf(vz);
        zcos = cosf(vz);
        float vy = v.y;
        ysin = sinf(vy);
        ycos = cosf(vy);
        float vx = v.x;
        xsin = sinf(vx);
        xcos = cosf(vx);
    }

    mtx.y.z = xsin;
    mtx.y.y = xcos * zcos;
    mtx.x.x = -(xsin * ysin * zsin - ycos * zcos);
    mtx.z.z = xcos * ycos;
    mtx.x.y = xsin * ysin * zcos + ycos * zsin;
    mtx.x.z = -(xcos * ysin);
    mtx.y.x = -(xcos * zsin);
    mtx.z.y = -(ycos * zcos * xsin + ysin * zsin);
    mtx.z.x = ycos * zsin * xsin + ysin * zcos;
}

void MakeRotMatrix(const Vector3 &v1, const Vector3 &v2, Hmx::Matrix3 &mtx) {
    mtx.y = v1;
    Normalize(mtx.y, mtx.y);
    Cross(mtx.y, v2, mtx.x);
    Normalize(mtx.x, mtx.x);
    Cross(mtx.x, mtx.y, mtx.z);
}

void MakeRotMatrix(const Hmx::Quat &q, Hmx::Matrix3 &mtx) {
    float x2 = q.x * 2.0f;
    float y2 = q.y * 2.0f;
    float z2 = q.z * 2.0f;
    float qxx = x2 * q.x;
    float qxy = x2 * q.y;
    float qxz = x2 * q.z;
    float qxw = x2 * q.w;
    float qyy = y2 * q.y;
    float qyz = y2 * q.z;
    float qyw = y2 * q.w;
    float qzz = z2 * q.z;
    float qzw = z2 * q.w;
    mtx.x.x = (1.0f - qyy) - qzz;
    mtx.x.y = qzw + qxy;
    mtx.x.z = qxz - qyw;
    mtx.y.x = qxy - qzw;
    mtx.y.y = (1.0f - qzz) - qxx;
    mtx.y.z = qyz + qxw;
    mtx.z.x = qyw + qxz;
    mtx.z.y = qyz - qxw;
    mtx.z.z = (1.0f - qxx) - qyy;
}

void RotateAboutX(const Hmx::Matrix3 &min, float f, Hmx::Matrix3 &mout) {
    float fcos = Cosine(f);
    float fsin = Sine(f);
    float new_zy = min.z.y * fcos - min.z.z * fsin;
    float new_zz = min.z.y * fsin + min.z.z * fcos;
    mout.x.y = min.x.y * fcos - min.x.z * fsin;
    mout.x.x = min.x.x;
    mout.x.z = min.x.y * fsin + min.x.z * fcos;
    mout.y.x = min.y.x;
    mout.y.y = min.y.y * fcos - min.y.z * fsin;
    mout.y.z = min.y.y * fsin + min.y.z * fcos;
    mout.z.x = min.z.x;
    mout.z.y = new_zy;
    mout.z.z = new_zz;
}

void RotateAboutZ(const Hmx::Matrix3 &min, float f, Hmx::Matrix3 &mout) {
    float fcos = Cosine(f);
    float fsin = Sine(f);
    mout.x.x = min.x.x * fcos - min.x.y * fsin;
    mout.x.y = min.x.x * fsin + min.x.y * fcos;
    mout.x.z = min.x.z;
    mout.y.x = min.y.x * fcos - min.y.y * fsin;
    mout.y.y = min.y.x * fsin + min.y.y * fcos;
    mout.y.z = min.y.z;
    mout.z.x = min.z.x * fcos - min.z.y * fsin;
    mout.z.y = min.z.x * fsin + min.z.y * fcos;
    mout.z.z = min.z.z;
}

void MakeEuler(const Hmx::Quat &q, Vector3 &v) {
    Hmx::Matrix3 m;
    MakeRotMatrix(q, m);
    MakeEuler(m, v);
}

void MakeRotQuat(const Vector3 &v1, const Vector3 &v2, Hmx::Quat &q) {
    Vector3 vec;
    Cross(v1, v2, vec);
    float sq = std::sqrt(LengthSquared(v1) * LengthSquared(v2));
    float sq2 = std::sqrt(((Dot(v1, v2) / sq + 1.0f) * 0.5f));
    if (sq2 > 1e-7f) {
        float f1 = 0.5f / (sq * sq2);
        q.Set(vec.x * f1, vec.y * f1, vec.z * f1, sq2);
    } else {
        q.Set(0, 0, 1, 0);
    }
}

void MakeRotQuatUnitX(const Vector3 &vec, Hmx::Quat &q) {
    float sq = std::sqrt(vec.x / 2.0f + 0.5f);
    if (sq > 1e-7f) {
        q.Set(0, vec.z * (0.5f / sq), -vec.y * (0.5f / sq), sq);
    } else {
        q.Set(0, 0, 1, 0);
    }
}

void Multiply(const Vector3 &vin, const Hmx::Quat &q, Vector3 &vout) {
    float qx = q.x;
    float qy = q.y;
    float qz = q.z;
    float qw = q.w;

    float qxqy = qy * qx;
    float qzqw = qz * qw;
    float viny = vin.y;
    float qyqz = qz * qy;
    float vinz = vin.z;
    float qxqw = qx * qw;
    float vinx = vin.x;
    float qxqz = qz * qx;
    float qyqw = qy * qw;

    float neg_qxqx = -(qx * qx);
    float neg_qzqz = -(qz * qz);
    float neg_qyqy = -(qy * qy);

    vout.z = ((neg_qyqy + neg_qxqx) * vinz + (qxqz - qyqw) * vinx + (qyqz + qxqw) * viny) * 2.0f + vinz;
    vout.x = ((qzqw + qxqy) * vinz + (neg_qzqz + neg_qyqy) * vinx + (qxqy - qzqw) * viny) * 2.0f + vinx;
    vout.y = ((qyqz - qxqw) * vinz + (qxqy + qzqw) * vinx + (neg_qzqz + neg_qxqx) * viny) * 2.0f + viny;
}

TextStream &operator<<(TextStream &ts, const Hmx::Quat &v) {
    ts << "(x:" << v.x << " y:" << v.y << " z:" << v.z << " w:" << v.w << ")";
    return ts;
}

TextStream &operator<<(TextStream &ts, const Vector3 &v) {
    ts << "(x:" << v.x << " y:" << v.y << " z:" << v.z << ")";
    return ts;
}

TextStream &operator<<(TextStream &ts, const Vector2 &v) {
    ts << "(x:" << v.x << " y:" << v.y << ")";
    return ts;
}

TextStream &operator<<(TextStream &ts, const Hmx::Matrix3 &m) {
    ts << "\n\t" << m.x << "\n\t" << m.y << "\n\t" << m.z;
    return ts;
}

TextStream &operator<<(TextStream &ts, const Transform &t) {
    ts << t.m << "\n\t" << t.v;
    return ts;
}

#ifndef __MWERKS__
void Multiply(const Hmx::Matrix3 &a, const Hmx::Matrix3 &b, Hmx::Matrix3 &out) {
    out.Set(
        a.x.x * b.x.x + a.x.y * b.y.x + a.x.z * b.z.x,
        a.x.x * b.x.y + a.x.y * b.y.y + a.x.z * b.z.y,
        a.x.x * b.x.z + a.x.y * b.y.z + a.x.z * b.z.z,
        a.y.x * b.x.x + a.y.y * b.y.x + a.y.z * b.z.x,
        a.y.x * b.x.y + a.y.y * b.y.y + a.y.z * b.z.y,
        a.y.x * b.x.z + a.y.y * b.y.z + a.y.z * b.z.z,
        a.z.x * b.x.x + a.z.y * b.y.x + a.z.z * b.z.x,
        a.z.x * b.x.y + a.z.y * b.y.y + a.z.z * b.z.y,
        a.z.x * b.x.z + a.z.y * b.y.z + a.z.z * b.z.z
    );
}
#endif

void Multiply(const Transform &a, const Transform &b, Transform &res) {
#ifdef __MWERKS__
    typedef __vec2x32float__ psq;
    register const Transform *_a = &a;
    register const Transform *_b = &b;
    register Transform *_res = &res;
    register psq _f0;
    register psq _f1;
    register psq _f2;
    register psq _f3;
    register psq _f4;
    register psq _f5;
    register psq _f6;
    register psq _f7;
    register psq _f8;
    register psq _f9;
    register psq _f10;
    register psq _f11;
    register psq _f12;
    register psq _f13;
    register psq _f31;
    register psq _f30;
    register psq _f29;
    register psq _f28;
    register psq _f27;
    register psq _f26;
    asm {
        psq_l    _f7,  0(_a),   0, 0
        ps_sub   _f10, _f10, _f10
        psq_l    _f9,  12(_a),  0, 0
        psq_l    _f8,  8(_a),   1, 0
        psq_l    _f11, 20(_a),  1, 0
        ps_merge00 _f0, _f7, _f9
        ps_merge11 _f2, _f7, _f9
        psq_l    _f30, 24(_a),  0, 0
        ps_merge00 _f4, _f8, _f11
        psq_l    _f28, 36(_a),  0, 0
        psq_l    _f27, 44(_a),  1, 0
        psq_l    _f29, 32(_a),  1, 0
        psq_l    _f7,  0(_b),   0, 0
        ps_merge00 _f1, _f30, _f28
        ps_merge11 _f3, _f30, _f28
        psq_l    _f9,  12(_b),  0, 0
        ps_merge00 _f5, _f29, _f27
        psq_l    _f8,  8(_b),   1, 0
        psq_l    _f11, 20(_b),  1, 0
        ps_merge00 _f6, _f7, _f9
        ps_merge11 _f7, _f7, _f9
        psq_l    _f30, 24(_b),  0, 0
        ps_merge00 _f8, _f8, _f11
        psq_l    _f28, 36(_b),  0, 0
        ps_merge01 _f26, _f10, _f27
        ps_muls0   _f10, _f1, _f6
        ps_muls0   _f12, _f1, _f7
        psq_l    _f29, 32(_b),  1, 0
        ps_muls0   _f31, _f1, _f8
        psq_l    _f27, 44(_b),  1, 0
        ps_muls0   _f9,  _f0, _f6
        ps_muls0   _f11, _f0, _f7
        ps_muls0   _f13, _f0, _f8
        ps_merge00 _f0,  _f30, _f28
        ps_madds1  _f10, _f3, _f6, _f10
        ps_madds1  _f9,  _f2, _f6, _f9
        ps_merge11 _f1,  _f30, _f28
        ps_madds1  _f12, _f3, _f7, _f12
        ps_madds0  _f10, _f5, _f0, _f10
        ps_madds1  _f11, _f2, _f7, _f11
        ps_madds0  _f12, _f5, _f1, _f12
        ps_merge00 _f6,  _f29, _f27
        ps_madds1  _f31, _f3, _f8, _f31
        ps_madds1  _f13, _f2, _f8, _f13
        ps_madds0  _f9,  _f4, _f0, _f9
        ps_madds0  _f31, _f5, _f6, _f31
        ps_madds0  _f11, _f4, _f1, _f11
        ps_madds0  _f13, _f4, _f6, _f13
        ps_madd    _f31, _f26, _f6, _f31
        ps_merge00 _f7,  _f9, _f11
        psq_st   _f13, 8(_res),  1, 0
        ps_merge11 _f9,  _f9, _f11
        ps_merge11 _f11, _f13, _f13
        psq_st   _f7,  0(_res),  0, 0
        ps_merge11 _f27, _f31, _f31
        ps_madd    _f10, _f26, _f0, _f10
        psq_st   _f9,  12(_res), 0, 0
        ps_madd    _f12, _f26, _f1, _f12
        psq_st   _f11, 20(_res), 1, 0
        ps_merge00 _f30, _f10, _f12
        psq_st   _f31, 32(_res), 1, 0
        ps_merge11 _f28, _f10, _f12
        psq_st   _f30, 24(_res), 0, 0
        psq_st   _f28, 36(_res), 0, 0
        psq_st   _f27, 44(_res), 1, 0
    }
#else
    Multiply(a.m, b.m, res.m);
    res.v.x = a.m.x.x * b.v.x + a.m.x.y * b.v.y + a.m.x.z * b.v.z + a.v.x;
    res.v.y = a.m.y.x * b.v.x + a.m.y.y * b.v.y + a.m.y.z * b.v.z + a.v.y;
    res.v.z = a.m.z.x * b.v.x + a.m.z.y * b.v.y + a.m.z.z * b.v.z + a.v.z;
#endif
}

void MultiplyStoreTransposed(const Transform &a, const Transform &b, float (&out)[3][4]) {
#ifdef __MWERKS__
    typedef __vec2x32float__ psq;
    register const Transform *_a = &a;
    register const Transform *_b = &b;
    register float (*_out)[4] = out;
    register psq _f0;
    register psq _f1;
    register psq _f2;
    register psq _f3;
    register psq _f4;
    register psq _f5;
    register psq _f6;
    register psq _f7;
    register psq _f8;
    register psq _f9;
    register psq _f10;
    register psq _f11;
    register psq _f12;
    register psq _f13;
    register psq _f31;
    register psq _f30;
    register psq _f29;
    register psq _f28;
    register psq _f27;
    register psq _f26;
    asm {
        psq_l    _f7,  0(_a),   0, 0
        ps_sub   _f11, _f11, _f11
        psq_l    _f9,  12(_a),  0, 0
        psq_l    _f30, 24(_a),  0, 0
        psq_l    _f28, 36(_a),  0, 0
        ps_merge00 _f0, _f7, _f9
        ps_merge11 _f2, _f7, _f9
        psq_l    _f7,  0(_b),   0, 0
        psq_l    _f9,  12(_b),  0, 0
        ps_merge00 _f1, _f30, _f28
        psq_l    _f8,  8(_a),   1, 0
        ps_merge11 _f3, _f30, _f28
        psq_l    _f10, 20(_a),  1, 0
        ps_merge00 _f6, _f7, _f9
        ps_merge11 _f7, _f7, _f9
        psq_l    _f27, 44(_a),  1, 0
        ps_merge00 _f4, _f8, _f10
        psq_l    _f29, 32(_a),  1, 0
        ps_merge01 _f26, _f11, _f27
        ps_muls0   _f9,  _f0, _f6
        ps_merge00 _f5, _f29, _f27
        psq_l    _f8,  8(_b),   1, 0
        psq_l    _f10, 20(_b),  1, 0
        ps_muls0   _f11, _f0, _f7
        ps_muls0   _f12, _f1, _f7
        ps_merge00 _f8, _f8, _f10
        ps_muls0   _f10, _f1, _f6
        psq_l    _f30, 24(_b),  0, 0
        psq_l    _f28, 36(_b),  0, 0
        ps_madds1  _f9,  _f2, _f6, _f9
        ps_muls0   _f13, _f0, _f8
        ps_muls0   _f31, _f1, _f8
        ps_merge00 _f0,  _f30, _f28
        psq_l    _f29, 32(_b),  1, 0
        ps_madds1  _f10, _f3, _f6, _f10
        psq_l    _f27, 44(_b),  1, 0
        ps_merge11 _f1,  _f30, _f28
        ps_madds1  _f12, _f3, _f7, _f12
        ps_madds0  _f10, _f5, _f0, _f10
        ps_madds0  _f9,  _f4, _f0, _f9
        ps_madds1  _f11, _f2, _f7, _f11
        ps_madd    _f10, _f26, _f0, _f10
        psq_st   _f9,  0(_out),  0, 0
        ps_madds0  _f12, _f5, _f1, _f12
        ps_madds0  _f11, _f4, _f1, _f11
        psq_st   _f10, 8(_out),  0, 0
        ps_merge00 _f0,  _f29, _f27
        ps_madds1  _f31, _f3, _f8, _f31
        psq_st   _f11, 16(_out), 0, 0
        ps_madds1  _f13, _f2, _f8, _f13
        ps_madd    _f12, _f26, _f1, _f12
        ps_madds0  _f31, _f5, _f0, _f31
        ps_madds0  _f13, _f4, _f0, _f13
        psq_st   _f12, 24(_out), 0, 0
        ps_madd    _f31, _f26, _f0, _f31
        psq_st   _f13, 32(_out), 0, 0
        psq_st   _f31, 40(_out), 0, 0
    }
#else
    Transform res;
    Multiply(a, b, res);
    out[0][0] = res.m.x.x; out[0][1] = res.m.y.x; out[0][2] = res.m.z.x; out[0][3] = res.v.x;
    out[1][0] = res.m.x.y; out[1][1] = res.m.y.y; out[1][2] = res.m.z.y; out[1][3] = res.v.y;
    out[2][0] = res.m.x.z; out[2][1] = res.m.y.z; out[2][2] = res.m.z.z; out[2][3] = res.v.z;
#endif
}

void MultiplyStoreTransposed(
    const Transform &a, const Transform &b, const Transform &c, float (&out)[3][4]
) {
#ifdef __MWERKS__
    typedef __vec2x32float__ psq;
    register const Transform *_a = &a;
    register const Transform *_b = &b;
    register const Transform *_c = &c;
    register float (*_out)[4] = out;
    register psq _f0;
    register psq _f1;
    register psq _f2;
    register psq _f3;
    register psq _f4;
    register psq _f5;
    register psq _f6;
    register psq _f7;
    register psq _f8;
    register psq _f9;
    register psq _f10;
    register psq _f11;
    register psq _f12;
    register psq _f13;
    register psq _f31;
    register psq _f30;
    register psq _f29;
    register psq _f28;
    register psq _f27;
    register psq _f26;
    register psq _f25;
    register psq _f24;
    register psq _f23;
    register psq _f22;
    register psq _f21;
    register psq _f20;
    register psq _f19;
    asm {
        psq_l    _f26, 0(_b),    0, 0
        ps_sub   _f0,  _f0, _f0
        psq_l    _f24, 12(_b),   0, 0
        psq_l    _f19, 44(_b),   1, 0
        ps_merge00 _f1,  _f26, _f24
        psq_l    _f25, 8(_b),    1, 0
        ps_merge11 _f3,  _f26, _f24
        psq_l    _f23, 20(_b),   1, 0
        psq_l    _f26, 0(_c),    0, 0
        ps_merge01 _f0,  _f0, _f19
        psq_l    _f24, 12(_c),   0, 0
        ps_merge00 _f5,  _f25, _f23
        psq_l    _f25, 8(_c),    1, 0
        ps_merge00 _f7,  _f26, _f24
        psq_l    _f23, 20(_c),   1, 0
        ps_merge11 _f9,  _f26, _f24
        psq_l    _f21, 32(_b),   1, 0
        ps_merge00 _f11, _f25, _f23
        psq_l    _f22, 24(_b),   0, 0
        psq_l    _f20, 36(_b),   0, 0
        ps_merge00 _f6,  _f21, _f19
        ps_muls0   _f13, _f1, _f7
        psq_l    _f21, 32(_c),   1, 0
        ps_merge00 _f2,  _f22, _f20
        psq_l    _f19, 44(_c),   1, 0
        ps_merge11 _f4,  _f22, _f20
        ps_merge00 _f12, _f21, _f19
        ps_muls0   _f30, _f1, _f9
        psq_l    _f22, 24(_c),   0, 0
        psq_l    _f20, 36(_c),   0, 0
        ps_muls0   _f28, _f1, _f11
        ps_muls0   _f31, _f2, _f7
        ps_muls0   _f29, _f2, _f9
        ps_madds1  _f28, _f3, _f11, _f28
        psq_l    _f21, 32(_a),   1, 0
        ps_muls0   _f27, _f2, _f11
        psq_l    _f19, 44(_a),   1, 0
        ps_merge00 _f8,  _f22, _f20
        ps_merge11 _f10, _f22, _f20
        ps_madds1  _f13, _f3, _f7, _f13
        psq_l    _f22, 24(_a),   0, 0
        psq_l    _f20, 36(_a),   0, 0
        ps_madds1  _f30, _f3, _f9, _f30
        ps_madds1  _f31, _f4, _f7, _f31
        ps_madds1  _f29, _f4, _f9, _f29
        ps_madds1  _f27, _f4, _f11, _f27
        psq_l    _f26, 0(_a),    0, 0
        psq_l    _f24, 12(_a),   0, 0
        ps_madds0  _f31, _f6, _f8, _f31
        ps_madds0  _f29, _f6, _f10, _f29
        ps_madds0  _f27, _f6, _f12, _f27
        ps_merge00 _f2,  _f22, _f20
        psq_l    _f25, 8(_a),    1, 0
        ps_madds0  _f30, _f5, _f10, _f30
        psq_l    _f23, 20(_a),   1, 0
        ps_madds0  _f28, _f5, _f12, _f28
        ps_madds0  _f13, _f5, _f8, _f13
        ps_merge00 _f1,  _f26, _f24
        ps_merge11 _f4,  _f22, _f20
        ps_muls0   _f5,  _f2, _f13
        ps_muls0   _f9,  _f2, _f30
        ps_muls0   _f11, _f2, _f28
        ps_merge00 _f6,  _f21, _f19
        ps_madd    _f31, _f0, _f8, _f31
        ps_madds1  _f5,  _f4, _f13, _f5
        ps_merge11 _f3,  _f26, _f24
        ps_muls0   _f2,  _f1, _f13
        ps_muls0   _f7,  _f1, _f30
        ps_madds0  _f5,  _f6, _f31, _f5
        ps_muls0   _f1,  _f1, _f28
        ps_madd    _f29, _f0, _f10, _f29
        ps_madds1  _f9,  _f4, _f30, _f9
        ps_madd    _f5,  _f0, _f31, _f5
        ps_madd    _f27, _f0, _f12, _f27
        ps_madds1  _f11, _f4, _f28, _f11
        psq_st   _f5,  8(_out),   0, 0
        ps_madds0  _f9,  _f6, _f29, _f9
        ps_merge00 _f5,  _f25, _f23
        ps_madds0  _f11, _f6, _f27, _f11
        ps_madds1  _f2,  _f3, _f13, _f2
        ps_madds1  _f7,  _f3, _f30, _f7
        ps_madds1  _f1,  _f3, _f28, _f1
        ps_madd    _f9,  _f0, _f29, _f9
        ps_madds0  _f2,  _f5, _f31, _f2
        ps_madds0  _f7,  _f5, _f29, _f7
        psq_st   _f9,  24(_out),  0, 0
        ps_madds0  _f1,  _f5, _f27, _f1
        ps_madd    _f11, _f0, _f27, _f11
        psq_st   _f2,  0(_out),   0, 0
        psq_st   _f7,  16(_out),  0, 0
        psq_st   _f1,  32(_out),  0, 0
        psq_st   _f11, 40(_out),  0, 0
    }
#else
    Transform tmp;
    Multiply(a, b, tmp);
    MultiplyStoreTransposed(tmp, c, out);
#endif
}

void FastInvert(const Hmx::Matrix3 &min, Hmx::Matrix3 &mout) {
    float xx = min.x.x, xy = min.x.y, xz = min.x.z;
    float yx = min.y.x, yy = min.y.y, yz = min.y.z;
    float zx = min.z.x, zy = min.z.y, zz = min.z.z;
    float xdot = 1.0f / (xx * xx + xy * xy + xz * xz);
    float ydot = 1.0f / (yx * yx + yy * yy + yz * yz);
    float zdot = 1.0f / (zx * zx + zy * zy + zz * zz);
    mout.Set(
        xx * xdot, yx * ydot, zx * zdot,
        xy * xdot, yy * ydot, zy * zdot,
        xz * xdot, yz * ydot, zz * zdot
    );
}

void Invert(const Hmx::Matrix3 &min, Hmx::Matrix3 &mout) {
    float det = (min.y.y * min.z.z - min.y.z * min.z.y) * min.x.x
                - (min.y.x * min.z.z - min.z.x * min.y.z) * min.x.y
                + (min.y.x * min.z.y - min.z.x * min.y.y) * min.x.z;
    float mult = 0.0f;
    if (det != 0.0f) {
        mult = 1.0f / det;
    }
    mout.Set(
        (min.z.z * min.y.y - min.y.z * min.z.y) * mult,
        -((min.z.z * min.x.y - min.x.z * min.z.y) * mult),
        (min.y.z * min.x.y - min.x.z * min.y.y) * mult,
        -((min.z.z * min.y.x - min.y.z * min.z.x) * mult),
        (min.z.z * min.x.x - min.x.z * min.z.x) * mult,
        -((min.y.z * min.x.x - min.x.z * min.y.x) * mult),
        (min.z.y * min.y.x - min.z.x * min.y.y) * mult,
        -((min.z.y * min.x.x - min.z.x * min.x.y) * mult),
        (min.y.y * min.x.x - min.x.y * min.y.x) * mult
    );
}

Hmx::Quat::Quat(const Vector3 &v, float f) { Set(v, f); }
