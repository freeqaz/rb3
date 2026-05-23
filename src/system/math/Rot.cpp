#include "Rot.h"
#include "math/Mtx.h"
#include "utl/BinStream.h"
#include <cmath>

DECOMP_FORCEACTIVE(Rot, "Rot.cpp", "false")

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
    float nux, nuy, nuz, nuw;

    nux = 32767.0f * quat.x + 0.5f;
    q.x = floorf(nux > 32767.0f ? 32767.0f : (nux < -32767.0f ? -32767.0f : nux));

    nuy = 32767.0f * quat.y + 0.5f;
    q.y = floorf(nuy > 32767.0f ? 32767.0f : (nuy < -32767.0f ? -32767.0f : nuy));

    nuz = 32767.0f * quat.z + 0.5f;
    q.z = floorf(nuz > 32767.0f ? 32767.0f : (nuz < -32767.0f ? -32767.0f : nuz));

    nuw = 32767.0f * quat.w + 0.5f;
    q.w = floorf(nuw > 32767.0f ? 32767.0f : (nuw < -32767.0f ? -32767.0f : nuw));
}

Hmx::Quat &TransformNoScale::GetRot(Hmx::Quat &qout) const {
    qout.w = q.w * 0.000030518509f;
    qout.z = q.z * 0.000030518509f;
    qout.y = q.y * 0.000030518509f;
    qout.x = q.x * 0.000030518509f;
    return qout;
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
    float yz = m.y.z;
    if (fabsf(yz) > 0.99999988f) {
        if (yz > 0) {
            v.x = PI / 2;
        } else {
            v.x = -PI / 2;
        }
        float xy = m.x.y;
        v.z = std::atan2(xy, m.x.x);
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
    if (v3.x != 0.0f) {
        float xz = m1.x.z;
        float xy = m1.x.y;
        float inv_x = 1.0f / v3.x;
        float xx = m1.x.x;
        m38.x.z = xz * inv_x;
        m38.x.x = xx * inv_x;
        m38.x.y = xy * inv_x;
    }
    if (v3.y != 0.0f) {
        float yz = m1.y.z;
        float yy = m1.y.y;
        float inv_y = 1.0f / v3.y;
        float yx = m1.y.x;
        m38.y.z = yz * inv_y;
        m38.y.x = yx * inv_y;
        m38.y.y = yy * inv_y;
    }
    if (v3.z != 0.0f) {
        float zz = m1.z.z;
        float zy = m1.z.y;
        float inv_z = 1.0f / v3.z;
        float zx = m1.z.x;
        m38.z.z = zz * inv_z;
        m38.z.x = zx * inv_z;
        m38.z.y = zy * inv_z;
    }
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
    float oz = z, ow = w, ox = x, oy = y;
    z = f2 * oz + f1 * ow;
    x = f2 * ox - f1 * oy;
    y = f2 * oy + f1 * ox;
    w = f2 * ow - f1 * oz;
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
            qout.x = -(f * (q2.x + q1.x) - q1.x);
            qout.y = -(f * (q2.y + q1.y) - q1.y);
            qout.z = -(f * (q2.z + q1.z) - q1.z);
            qout.w = -(f * (q2.w + q1.w) - q1.w);
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
    float ycos_zcos = ycos * zcos;
    float ysin_zsin = ysin * zsin;
    mtx.y.y = xcos * zcos;
    mtx.x.x = ycos_zcos - xsin * ysin_zsin;
    mtx.z.z = xcos * ycos;
    mtx.x.y = xsin * ysin * zcos + ycos * zsin;
    mtx.x.z = -ysin * xcos;
    mtx.y.x = -xcos * zsin;
    mtx.z.y = ysin_zsin - ycos_zcos * xsin;
    mtx.z.x = ycos * zsin * xsin + ysin * zcos;
}

void MakeRotMatrix(const Vector3 &v1, const Vector3 &v2, Hmx::Matrix3 &mtx) {
    mtx.y = v1;
    Normalize(mtx.y, mtx.y);
    float y1 = mtx.y.y;
    float z2 = v2.z;
    float x2 = v2.x;
    float z1 = mtx.y.z;
    float y2 = v2.y;
    float x1 = mtx.y.x;
    float yz = y1 * z2;
    float yx = y1 * x2;
    float zx = z1 * x2;
    float zy = z1 * y2;
    float xy = x1 * y2;
    float xz = x1 * z2;
    mtx.x.Set(yz - zy, zx - xz, xy - yx);
    Normalize(mtx.x, mtx.x);
    Cross(mtx.x, mtx.y, mtx.z);
}

void MakeRotMatrix(const Hmx::Quat &q, Hmx::Matrix3 &mtx) {
    float qx = q.x;
    float two = 2.0f;
    float qy = q.y;
    float qz = q.z;
    float x2 = two * qx;
    float y2 = two * qy;
    float one = 1.0f;
    float z2 = two * qz;
    float qw = q.w;
    float qxx = x2 * qx;
    float qyy = y2 * qy;
    float qzz = z2 * qz;
    float zz = (one - qxx) - qyy;
    float qzw = z2 * qw;
    float yy = (one - qzz) - qxx;
    mtx.z.z = zz;
    float xx = (one - qyy) - qzz;
    float qxy = x2 * qy;
    mtx.y.y = yy;
    float qxz = x2 * qz;
    float qyw = y2 * qw;
    mtx.x.x = xx;
    mtx.x.y = qxy + qzw;
    mtx.x.z = qxz - qyw;
    float qxw = x2 * qw;
    float qyz = y2 * qz;
    mtx.y.x = qxy - qzw;
    mtx.z.x = qxz + qyw;
    mtx.y.z = qyz + qxw;
    mtx.z.y = qyz - qxw;
}

void RotateAboutX(const Hmx::Matrix3 &min, float f, Hmx::Matrix3 &mout) {
    float fcos = Cosine(f);
    float fsin = Sine(f);
    float yx = min.y.x;
    float zx = min.z.x;
    float zz = min.z.z;
    float yz = min.y.z;
    float zy = min.z.y;
    float yy = min.y.y;
    mout.x.x = min.x.x;
    mout.x.y = min.x.y * fcos - min.x.z * fsin;
    mout.x.z = min.x.y * fsin + min.x.z * fcos;
    mout.y.x = yx;
    mout.y.y = yy * fcos - yz * fsin;
    mout.y.z = yy * fsin + yz * fcos;
    mout.z.x = zx;
    mout.z.y = zy * fcos - zz * fsin;
    mout.z.z = zy * fsin + zz * fcos;
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
    float v1x = v1.x;
    float v2x = v2.x;
    float v1z = v1.z;
    float v2z = v2.z;
    float v2y = v2.y;
    float v1y = v1.y;
    float cy = v1z * v2x - v1x * v2z;
    float cz = v1x * v2y - v1y * v2x;
    float cx = v1y * v2z - v1z * v2y;
    float lensq1 = v1x * v1x + v1y * v1y + v1z * v1z;
    float lensq2 = v2x * v2x + v2y * v2y + v2z * v2z;
    float sq = std::sqrt(lensq1 * lensq2);
    float sq2 = std::sqrt(1.0f + (v1x * v2x + v1y * v2y + v1z * v2z) / sq);
    if (sq2 > 1e-7f) {
        q.w = sq2;
        float scale = 1.0f / (sq * sq2);
        q.x = cx * scale;
        q.y = cy * scale;
        q.z = cz * scale;
    } else {
        q.x = 0.0f;
        q.y = 0.0f;
        q.z = 1.0f;
        q.w = 0.0f;
    }
}

void MakeRotQuatUnitX(const Vector3 &vec, Hmx::Quat &q) {
    float sq = std::sqrt(0.5f + vec.x * 0.5f);
    if (sq > 1e-7f) {
        float inv2sq = 0.5f / sq;
        q.Set(0, vec.z * inv2sq, -vec.y * inv2sq, sq);
    } else {
        q.Set(0, 0, 1, 0);
    }
}

void Multiply(const Vector3 &vin, const Hmx::Quat &q, Vector3 &vout) {
    float qx = q.x;
    float qz = q.z;
    float qy = q.y;
    float qw = q.w;
    float neg_qx = -qx;
    float neg_qz = -qz;
    float neg_qy = -qy;
    float viny = vin.y;
    float neg_qxqx = neg_qx * qx;
    float vinx = vin.x;
    float neg_qzqz = neg_qz * qz;
    float vinz = vin.z;
    float qxqw = qw * qx;
    float qyqz = qy * qz;
    float qzqw = qw * qz;
    float qxqy = qx * qy;
    float neg_qyqy = neg_qy * qy;

    float sum_z_viny = qxqw + qyqz;
    float qxqz = qx * qz;
    float sum_x_viny = qxqy - qzqw;
    float qyqw = qw * qy;
    float sum_y_viny = neg_qxqx + neg_qzqz;
    float sum_y_vinx = qzqw + qxqy;
    float sum_x_vinx = neg_qyqy + neg_qzqz;
    float sum_x_vinz = qyqw + qxqz;
    float sum_y_vinz = qyqz - qxqw;
    float sum_z_vinx = qxqz - qyqw;
    float sum_z_vinz = neg_qxqx + neg_qyqy;

    vout.y = (sum_y_vinz * vinz + sum_y_vinx * vinx + sum_y_viny * viny) * 2.0f + viny;
    vout.x = (sum_x_vinz * vinz + sum_x_vinx * vinx + sum_x_viny * viny) * 2.0f + vinx;
    vout.z = (sum_z_vinz * vinz + sum_z_vinx * vinx + sum_z_viny * viny) * 2.0f + vinz;
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
    float xy = min.x.y;
    float yy = min.y.y;
    float zy = min.z.y;
    float xx = min.x.x;
    float yx = min.y.x;
    float xz = min.x.z;
    float zx = min.z.x;
    float yz = min.y.z;
    float zz = min.z.z;
    float xdot = 1.0f / (xx * xx + xy * xy + xz * xz);
    float ydot = 1.0f / (yx * yx + yy * yy + yz * yz);
    float zdot = 1.0f / (zx * zx + zy * zy + zz * zz);
    mout.Set(
        xx * xdot, xy * xdot, xz * xdot,
        yx * ydot, yy * ydot, yz * ydot,
        zx * zdot, zy * zdot, zz * zdot
    );
}

void Invert(const Hmx::Matrix3 &min, Hmx::Matrix3 &mout) {
    float myz = min.y.z;
    float mzx = min.z.x;
    float mzz = min.z.z;
    float myx = min.y.x;
    float f2 = mzx * myz;
    float mzy = min.z.y;
    float f4 = myx * mzz;
    float myy = min.y.y;
    float f0 = mzy * myz;
    float mxy = min.x.y;
    float f1 = myy * mzz;
    float mxx = min.x.x;
    float f8 = f4 - f2;
    float mxz = min.x.z;
    float f9 = f1 - f0;
    float f3 = myx * mzy;
    float f2b = mzx * myy;
    float f1b = mxy * f8;
    float f10 = f3 - f2b;
    float det = mxx * f9 - f1b + mxz * f10;
    float mult = 0.0f;
    if (det != 0.0f) {
        mult = 1.0f / det;
    }
    mout.x.x = mult * f9;
    float r_zy = mzx * mxy - mxx * mzy;
    float r_xy = mzy * mxz - mxy * mzz;
    mout.z.y = mult * r_zy;
    float r02 = mxy * myz - myy * mxz;
    mout.x.y = mult * r_xy;
    mout.x.z = mult * r02;
    mout.y.x = mult * (-f8);
    mout.y.y = mult * (mxx * mzz - mzx * mxz);
    mout.y.z = mult * (myx * mxz - mxx * myz);
    mout.z.x = mult * f10;
    mout.z.z = mult * (mxx * myy - mxy * myx);
}

Hmx::Quat::Quat(const Vector3 &v, float f) { Set(v, f); }
