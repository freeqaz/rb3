#ifndef MATH_MTX_H
#define MATH_MTX_H
#include "math/Vec.h"
#include "math/Trig.h"
#include "obj/Data.h"
#include "utl/BinStream.h"
#include "decomp.h"

#define PSQ_MOVE(dst, src) *(__vec2x32float__ *)&dst = *(__vec2x32float__ *)&src

namespace Hmx {
    class Matrix3 {
    public:
        Vector3 x;
        Vector3 y;
        Vector3 z;

        // all of these are weak
        Matrix3() {}

        Matrix3(const Matrix3 &mtx) {
            x = mtx.x;
            y = mtx.y;
            z = mtx.z;
        }

        Matrix3(const Vector3 &v1, const Vector3 &v2, const Vector3 &v3)
            : x(v1), y(v2), z(v3) {}

        // clang-format off
        Matrix3(
            float f1, float f2, float f3,
            float f4, float f5, float f6,
            float f7, float f8, float f9
        )
            : x(f1, f2, f3), y(f4, f5, f6), z(f7, f8, f9) {}

        void Set(
            float f1, float f2, float f3,
            float f4, float f5, float f6,
            float f7, float f8, float f9
        ) {
            x.Set(f1, f2, f3);
            y.Set(f4, f5, f6);
            z.Set(f7, f8, f9);
        }
        // clang-format on
        void Set(const Vector3 &v1, const Vector3 &v2, const Vector3 &v3) {
            x = v1;
            y = v2;
            z = v3;
        }
        void Zero() {
            x.Zero();
            y.Zero();
            z.Zero();
        }
        void RotateAboutZ(float angle) {
            float c = Cosine(angle);
            float s = Sine(angle);
            Set(c, -s, 0.0f, s, c, 0.0f, 0.0f, 0.0f, 1.0f);
        }
        void RotateAboutY(float angle) {
            float c = Cosine(angle);
            float s = Sine(angle);
            Set(c, 0.0f, -s, 0.0f, 1.0f, 0.0f, s, 0.0f, c);
        }
        void RotateAboutX(float angle) {
            float c = Cosine(angle);
            float s = Sine(angle);
            Set(1.0f, 0.0f, 0.0f, 0.0f, c, s, 0.0f, -s, c);
        }
        void Identity() {
            x.Set(1.0f, 0.0f, 0.0f);
            y.Set(0.0f, 1.0f, 0.0f);
            z.Set(0.0f, 0.0f, 1.0f);
        }
        Matrix3 &operator=(const Matrix3 &mtx) {
            PSQ_MOVE(x.x, mtx.x.x);
            x.z = mtx.x.z;

            PSQ_MOVE(y.x, mtx.y.x);
            y.z = mtx.y.z;

            PSQ_MOVE(z.x, mtx.z.x);
            z.z = mtx.z.z;
            return *this;
        }
        Vector3 &operator[](int i) { return *(&x + i); }
        const Vector3 &operator[](int i) const { return *(&x + i); }

        bool operator==(const Matrix3 &mtx) const {
            return x == mtx.x && y == mtx.y && z == mtx.z;
        }

        bool operator!=(const Matrix3 &mtx) const {
            return x != mtx.x || y != mtx.y || z != mtx.z;
        }
    };

    class Quat {
    public:
        Quat() {}
        Quat(float f1, float f2, float f3, float f4) : x(f1), y(f2), z(f3), w(f4) {}
        Quat(const Matrix3 &m) { Set(m); }
        Quat(const Vector3 &v) { Set(v); }
        Quat(const Vector3 &, float);

        void Reset() {
            x = y = z = 0.0f;
            w = 1.0f;
        }
        void Zero() { w = x = y = z = 0.0f; }
        void Set(const Matrix3 &);
        void Set(const Vector3 &);
        void Set(const Vector3 &, float);
        void Set(float f1, float f2, float f3, float f4) {
            x = f1;
            y = f2;
            z = f3;
            w = f4;
        }

        bool operator!=(const Quat &q) const {
            return x != q.x || y != q.y || z != q.z || w != q.w;
        }
        float operator*(const Quat &q) const { return x * q.x + y * q.y + z * q.z + w * q.w; }
        const float &operator[](int i) const { return *(&x + i); }
        float &operator[](int i) { return *(&x + i); }

        float x;
        float y;
        float z;
        float w;
    };
}

inline BinStream &operator>>(BinStream &bs, Hmx::Matrix3 &mtx) {
    bs >> mtx.x >> mtx.y >> mtx.z;
    return bs;
}

inline BinStream &operator<<(BinStream &bs, const Hmx::Quat &q) {
    bs << q.x << q.y << q.z << q.w;
    return bs;
}

inline BinStream &operator>>(BinStream &bs, Hmx::Quat &q) {
    bs >> q.x >> q.y >> q.z >> q.w;
    return bs;
}

class Transform {
public:
    class Hmx::Matrix3 m;
    class Vector3 v;

    // all of these are weak
    Transform() {}

    Transform(const Hmx::Matrix3 &mtx, const Vector3 &vec) : m(mtx), v(vec) {}

    // both of these use powerpc asm magic
    Transform(const register Transform &tf) {
        // m.x.x = tf.m.x.x;
        // m.x.y = tf.m.x.y;
        PSQ_MOVE(m.x.x, tf.m.x.x);
        m.x.z = tf.m.x.z;

        // m.y.x = tf.m.y.x;
        // m.y.y = tf.m.y.y;
        PSQ_MOVE(m.y.x, tf.m.y.x);
        m.y.z = tf.m.y.z;

        // m.z.x = tf.m.z.x;
        // m.z.y = tf.m.z.y;
        PSQ_MOVE(m.z.x, tf.m.z.x);
        m.z.z = tf.m.z.z;

        // v.x = tf.v.x;
        // v.y = tf.v.y;
        PSQ_MOVE(v.x, tf.v.x);
        v.z = tf.v.z;
    }
    Transform &operator=(const Transform &tf) {
        // m.x.x = tf.m.x.x;
        // m.x.y = tf.m.x.y;
        PSQ_MOVE(m.x.x, tf.m.x.x);
        m.x.z = tf.m.x.z;

        // m.y.x = tf.m.y.x;
        // m.y.y = tf.m.y.y;
        PSQ_MOVE(m.y.x, tf.m.y.x);
        m.y.z = tf.m.y.z;

        // m.z.x = tf.m.z.x;
        // m.z.y = tf.m.z.y;
        PSQ_MOVE(m.z.x, tf.m.z.x);
        m.z.z = tf.m.z.z;

        // v.x = tf.v.x;
        // v.y = tf.v.y;
        PSQ_MOVE(v.x, tf.v.x);
        v.z = tf.v.z;
        return *this;
    }

    void Reset() {
        m.Identity();
        v.Zero();
    }

    void Set(const Hmx::Matrix3 &mtx, const Vector3 &vec) {
        m = mtx;
        v = vec;
    }

    void LookAt(const Vector3 &, const Vector3 &);
    void Zero() {
        m.Zero();
        v.Zero();
    }

    bool operator==(const Transform &tf) const { return m == tf.m && v == tf.v; }
};

inline BinStream &operator>>(BinStream &bs, Transform &tf) {
    bs >> tf.m >> tf.v;
    return bs;
}

class QuatXfm {
public:
    Vector3 v; // 0x0
    Hmx::Quat q; // 0xc
};

class ShortQuat {
public:
    short x, y, z, w;
    void Reset() {
        x = y = z = 0;
        w = 32767;
    }
    void Set(const Hmx::Quat &q) {
        x = (short)floor(Clamp(-32767.0f, 32767.0f, 0.5f + q.x * 32767.0f));
        y = (short)floor(Clamp(-32767.0f, 32767.0f, 0.5f + q.y * 32767.0f));
        z = (short)floor(Clamp(-32767.0f, 32767.0f, 0.5f + q.z * 32767.0f));
        w = (short)floor(Clamp(-32767.0f, 32767.0f, 0.5f + q.w * 32767.0f));
    }
    void ToQuat(Hmx::Quat& q) const {
        q.Set(
            (float)x * 3.051851e-05f,
            (float)y * 3.051851e-05f,
            (float)z * 3.051851e-05f,
            (float)w * 3.051851e-05f
        );
    }
};

class ByteQuat {
public:
    char x, y, z, w;
    void Set(const Hmx::Quat &q) {
        x = (char)(float)floor(Clamp(-127.0f, 127.0f, 0.5f + q.x * 127.0f));
        y = (char)(float)floor(Clamp(-127.0f, 127.0f, 0.5f + q.y * 127.0f));
        z = (char)(float)floor(Clamp(-127.0f, 127.0f, 0.5f + q.z * 127.0f));
        w = (char)(float)floor(Clamp(-127.0f, 127.0f, 0.5f + q.w * 127.0f));
    }
    void ToQuat(Hmx::Quat& q) const {
        q.Set(
            (float)x * 0.0078740157f,
            (float)y * 0.0078740157f,
            (float)z * 0.0078740157f,
            (float)w * 0.0078740157f
        );
    }
};

class TransformNoScale {
public:
    TransformNoScale() {}
    TransformNoScale(const TransformNoScale &t) { Set(t); }
    void Set(const Transform &);
    void Set(const TransformNoScale &);
    void SetRot(const Hmx::Matrix3 &);
    void SetRot(const Hmx::Quat &);
    Hmx::Quat &GetRot(Hmx::Quat &) const;
    void Reset();
    Transform &ToTransform(Transform &) const;
    TransformNoScale &operator=(const TransformNoScale &t) { Set(t); }

    ShortQuat q; // 0x0/2/4/6
    class Vector3 v; // 0x8
};

BinStream &operator>>(BinStream &, TransformNoScale &);

class Plane {
public:
    Plane() {}
    Plane(const Vector3 &v1, const Vector3 &v2) { Set(v1, v2); }

    void Set(const Vector3 &pt, const Vector3 &normal) {
        a = normal.x;
        b = normal.y;
        c = normal.z;
        float dot = a * pt.x + b * pt.y + c * pt.z;
        d = -dot;
    }
    void Set(float _a, float _b, float _c, float _d) { a = _a; b = _b; c = _c; d = _d; }
    float Dot(const Vector3 &vec) const { return a * vec.x + b * vec.y + c * vec.z + d; }
    Vector3 On() const;

    float a, b, c, d;
};

inline BinStream &operator>>(BinStream &bs, Plane &pl) {
    bs >> pl.a >> pl.b >> pl.c >> pl.d;
    return bs;
}

class Frustum {
    // total size: 0x60
public:
    void Set(float, float, float, float);

    class Plane front; // offset 0x0, size 0x10
    class Plane back; // offset 0x10, size 0x10
    class Plane left; // offset 0x20, size 0x10
    class Plane right; // offset 0x30, size 0x10
    class Plane top; // offset 0x40, size 0x10
    class Plane bottom; // offset 0x50, size 0x10
};

class Triangle {
public:
    Triangle() {}
    Triangle(const Vector3 &v1, const Vector3 &v2, const Vector3 &v3) { Set(v1, v2, v3); }
    void Set(const Vector3 &, const Vector3 &, const Vector3 &);

    Vector3 origin; // 0x0
    Hmx::Matrix3 frame; // 0xc
};

void Scale(const Hmx::Matrix3 &mtx, const Vector3 &vec, Hmx::Matrix3 &res);

// https://decomp.me/scratch/kGwAB
// lol, regswaps galore
inline void Scale(const Vector3 &vec, const Hmx::Matrix3 &mtx, Hmx::Matrix3 &res) {
    Scale(mtx.x, vec.x, res.x);
    Scale(mtx.y, vec.y, res.y);
    Scale(mtx.z, vec.z, res.z);
}

inline void Negate(const Hmx::Quat &q, Hmx::Quat &qres) {
    qres.Set(-q.x, -q.y, -q.z, q.w);
}

inline void ScaleAddEq(Hmx::Quat &q1, const Hmx::Quat &q2, float f) {
    float abs_f = std::fabs(f);
    float sx = q2.x * abs_f;
    float sy = q2.y * abs_f;
    float sz = q2.z * abs_f;
    float sw = q2.w * f;
    if (q1.x * sx + q1.y * sy + q1.z * sz + q1.w * sw < 0.0f) {
        q1.x -= sx;
        q1.y -= sy;
        q1.z -= sz;
        q1.w -= sw;
    } else {
        q1.x += sx;
        q1.y += sy;
        q1.z += sz;
        q1.w += sw;
    }
}
void Normalize(const Hmx::Quat &, Hmx::Quat &);
inline void Multiply(const Hmx::Quat &q1, const Hmx::Quat &q2, Hmx::Quat &qres) {
    qres.Set(
        -(q1.z * q2.y - (q1.y * q2.z + (q1.w * q2.x + q1.x * q2.w))),
        -(q1.x * q2.z - (q1.z * q2.x + (q1.w * q2.y + q1.y * q2.w))),
        -(q1.y * q2.x - (q1.x * q2.y + (q1.w * q2.z + q1.z * q2.w))),
        -(q1.z * q2.z - -(q1.y * q2.y - (q1.w * q2.w - q1.x * q2.x)))
    );
}
void FastInterp(const Hmx::Quat &, const Hmx::Quat &, float, Hmx::Quat &);
void Invert(const Hmx::Matrix3 &, Hmx::Matrix3 &);
void FastInvert(const Hmx::Matrix3 &, Hmx::Matrix3 &);
inline void Multiply(const Hmx::Matrix3 &m, const Vector3 &v, Vector3 &out) {
    float vy = v.y;
    float accz = m.z.y * vy;
    float accy = m.y.y * vy;
    float accx = m.x.y * vy;
    float vx = v.x;
    accz += m.z.x * vx;
    accy += m.y.x * vx;
    accx += m.x.x * vx;
    float vz = v.z;
    out.z = m.z.z * vz + accz;
    out.y = m.y.z * vz + accy;
    out.x = m.x.z * vz + accx;
}
void Multiply(const Vector3 &, const Hmx::Matrix3 &, Vector3 &);
void Multiply(const Transform &, const Transform &, Transform &);
void Multiply(const Transform &, const Vector3 &, Vector3 &);
void Multiply(const Vector3 &, const Hmx::Quat &, Vector3 &);
void Multiply(const Vector3 &, const Transform &, Vector3 &);
inline void Multiply(const Vector3 &v, const Transform &t, Vector3 &out) {
#ifdef MATCHING
    register __vec2x32float__ i1, i2, m1, m2, o1, o2;
    register const Vector3 *_v = &v;
    register Vector3 *_out = &out;
    register const Hmx::Matrix3 *_m = &t.m;
    typedef Hmx::Matrix3 Matrix3;
    ASM_BLOCK(
        psq_l o1, 0x24(_m), 0, 0
        psq_l o2, 0x2c(_m), 1, 0
        psq_l i2, Vector3.y(_v), 0, 0
        psq_l m1, Matrix3.z.x(_m), 0, 0
        psq_l m2, Matrix3.z.z(_m), 1, 0
        ps_madds1 o1, m1, i2, o1
        ps_madds1 o2, m2, i2, o2
        psq_l m1, Matrix3.y.x(_m), 0, 0
        psq_l m2, Matrix3.y.z(_m), 1, 0
        ps_madds0 o1, m1, i2, o1
        ps_madds0 o2, m2, i2, o2
        psq_l i1, Vector3.x(_v), 0, 0
        psq_l m1, Matrix3.x.x(_m), 0, 0
        psq_l m2, Matrix3.x.z(_m), 1, 0
        ps_madds0 o1, m1, i1, o1
        ps_madds0 o2, m2, i1, o2
        psq_st o1, Vector3.x(_out), 0, 0
        psq_st o2, Vector3.z(_out), 1, 0
    )
#else
    out.Set(
        t.m.x.x * v.x + t.m.y.x * v.y + t.m.z.x * v.z + t.v.x,
        t.m.x.y * v.x + t.m.y.y * v.y + t.m.z.y * v.z + t.v.y,
        t.m.x.z * v.x + t.m.y.z * v.y + t.m.z.z * v.z + t.v.z
    );
#endif
}
inline void MultiplyTranspose(const Vector3 &v, const Transform &t, Vector3 &out) {
    Subtract(v, t.v, out);
    out.Set(Dot(out, t.m.x), Dot(out, t.m.y), Dot(out, t.m.z));
}
void Multiply(const Plane &, const Transform &, Plane &);
#if defined(__MWERKS__) && !defined(CHARHAIR_LOCAL_MULTIPLY)
inline void Multiply(const Hmx::Matrix3 &a, const Hmx::Matrix3 &b, Hmx::Matrix3 &out) {
    typedef __vec2x32float__ psq;
    register const Hmx::Matrix3 *_a = &a;
    register const Hmx::Matrix3 *_b = &b;
    register Hmx::Matrix3 *_out = &out;
    float row0[3], row1[3], row2[3];
    register float *_row0 = row0;
    register float *_row1 = row1;
    register float *_row2 = row2;
    register psq _f0, _f1, _f2, _f3, _f4, _f5, _f6, _f7, _f8, _f9, _f10, _f11, _f12;
    asm { cmplw cr1, _b, _out }
    asm volatile {
        beq cr1, alias_path
        // non-alias path
        psq_l  _f4, 0x4(_a),  0, 0
        psq_l  _f3, 0x18(_b), 0, 0
        psq_l  _f2, 0x20(_b), 1, 0
        ps_muls1 _f1, _f3, _f4
        psq_l  _f3, 0xc(_b),  0, 0
        ps_muls1 _f0, _f2, _f4
        psq_l  _f2, 0x14(_b), 1, 0
        psq_l  _f9, 0x10(_a), 0, 0
        psq_l  _f8, 0x18(_b), 0, 0
        psq_l  _f7, 0x20(_b), 1, 0
        ps_madds0 _f1, _f3, _f4, _f1
        ps_madds0 _f0, _f2, _f4, _f0
        psq_l  _f4, 0x0(_a),  0, 0
        ps_muls1 _f6, _f8, _f9
        psq_l  _f3, 0x0(_b),  0, 0
        ps_muls1 _f5, _f7, _f9
        ps_madds0 _f1, _f3, _f4, _f1
        psq_l  _f2, 0x8(_b),  1, 0
        psq_l  _f8, 0xc(_b),  0, 0
        ps_madds0 _f0, _f2, _f4, _f0
        psq_l  _f7, 0x14(_b), 1, 0
        ps_madds0 _f6, _f8, _f9, _f6
        psq_l  _f2, 0xc(_a),  0, 0
        ps_madds0 _f5, _f7, _f9, _f5
        psq_l  _f4, 0x1c(_a), 0, 0
        psq_l  _f7, 0x1c(_b), 0, 0
        psq_l  _f3, 0x18(_b), 0, 0
        ps_madds0 _f6, _f1, _f2, _f6
        ps_madds0 _f5, _f0, _f2, _f5
        psq_l  _f8, 0x20(_b), 1, 0
        ps_muls1 _f3, _f3, _f7
        psq_l  _f9, 0x18(_a), 0, 0
        ps_muls1 _f2, _f8, _f7
        psq_st _f1, 0x0(_out), 0, 0
        ps_madds0 _f6, _f3, _f9, _f6
        ps_madds0 _f5, _f2, _f9, _f5
        psq_st _f0, 0x8(_out), 1, 0
        ps_madds0 _f3, _f1, _f4, _f3
        psq_st _f6, 0xc(_out), 0, 0
        ps_madds0 _f2, _f0, _f4, _f2
        psq_st _f5, 0x14(_out), 1, 0
        psq_st _f3, 0x18(_out), 0, 0
        psq_st _f2, 0x20(_out), 1, 0
        b mult_end
    alias_path:
        psq_l  _f4, 0x4(_a),  0, 0
        psq_l  _f3, 0x18(_out), 0, 0
        psq_l  _f2, 0x20(_out), 1, 0
        ps_muls1 _f1, _f3, _f4
        psq_l  _f3, 0xc(_out), 0, 0
        ps_muls1 _f0, _f2, _f4
        psq_l  _f2, 0x14(_out), 1, 0
        psq_l  _f9, 0x10(_a),  0, 0
        psq_l  _f8, 0x18(_out), 0, 0
        psq_l  _f7, 0x20(_out), 1, 0
        ps_madds0 _f1, _f3, _f4, _f1
        ps_muls1 _f6, _f8, _f9
        psq_l  _f12, 0x1c(_a), 0, 0
        ps_mr  _f8, _f3
        psq_l  _f3, 0x18(_out), 0, 0
        ps_muls1 _f5, _f7, _f9
        ps_muls1 _f11, _f3, _f12
        ps_mr  _f7, _f2
        psq_l  _f3, 0x0(_out), 0, 0
        ps_madds0 _f0, _f2, _f4, _f0
        psq_l  _f2, 0x20(_out), 1, 0
        psq_l  _f4, 0x0(_a),  0, 0
        ps_muls1 _f10, _f2, _f12
        psq_l  _f2, 0x8(_out), 1, 0
        ps_madds0 _f1, _f3, _f4, _f1
        ps_madds0 _f6, _f8, _f9, _f6
        ps_madds0 _f0, _f2, _f4, _f0
        psq_l  _f4, 0x18(_a), 0, 0
        ps_madds0 _f5, _f7, _f9, _f5
        psq_l  _f9, 0xc(_a),  0, 0
        ps_madds0 _f11, _f8, _f12, _f11
        ps_madds0 _f10, _f7, _f12, _f10
        psq_st _f1, 0x0(_row0), 0, 0
        ps_madds0 _f6, _f3, _f9, _f6
        ps_madds0 _f5, _f2, _f9, _f5
        ps_madds0 _f11, _f3, _f4, _f11
        lfs    _f8, 0x0(_row0)
        ps_madds0 _f10, _f2, _f4, _f10
        psq_st _f6, 0x0(_row1), 0, 0
        lfs    _f7, 0x4(_row0)
        psq_st _f11, 0x0(_row2), 0, 0
        lfs    _f4, 0x4(_row1)
        psq_st _f5, 0x8(_row1), 1, 0
        lfs    _f5, 0x0(_row1)
        psq_st _f0, 0x8(_row0), 1, 0
        lfs    _f3, 0x8(_row1)
        psq_st _f10, 0x8(_row2), 1, 0
        lfs    _f6, 0x8(_row0)
        lfs    _f2, 0x0(_row2)
        lfs    _f1, 0x4(_row2)
        lfs    _f0, 0x8(_row2)
        stfs   _f8, 0x0(_out)
        stfs   _f7, 0x4(_out)
        stfs   _f6, 0x8(_out)
        stfs   _f5, 0xc(_out)
        stfs   _f4, 0x10(_out)
        stfs   _f3, 0x14(_out)
        stfs   _f2, 0x18(_out)
        stfs   _f1, 0x1c(_out)
        stfs   _f0, 0x20(_out)
    mult_end:
    }
}
#elif !defined(CHARHAIR_LOCAL_MULTIPLY)
void Multiply(const Hmx::Matrix3 &, const Hmx::Matrix3 &, Hmx::Matrix3 &);
#endif // __MWERKS__ && !CHARHAIR_LOCAL_MULTIPLY
void IdentityInterp(const Hmx::Quat &, float, Hmx::Quat &);
void Multiply(const Transform &, const Hmx::Matrix3 &, Transform &);

inline void Multiply(const Frustum &fin, const Transform &tf, Frustum &fout) {
    Multiply(fin.front, tf, fout.front);
    Multiply(fin.back, tf, fout.back);
    Multiply(fin.left, tf, fout.left);
    Multiply(fin.right, tf, fout.right);
    Multiply(fin.top, tf, fout.top);
    Multiply(fin.bottom, tf, fout.bottom);
}

void Interp(const Hmx::Matrix3 &, const Hmx::Matrix3 &, float, Hmx::Matrix3 &);
void NormalizeTo(const Hmx::Quat &, Hmx::Quat &);
bool operator<=(const Vector3 &, const Plane &);
Vector3 TransformNormal(const Vector3 &, const Hmx::Matrix3 &);
inline void ScaleAddEq(Hmx::Matrix3 &m1, const Hmx::Matrix3 &m2, float f) {
    ScaleAddEq(m1.x, m2.x, f);
    ScaleAddEq(m1.y, m2.y, f);
    ScaleAddEq(m1.z, m2.z, f);
}
inline void ScaleAddEq(Transform &tf1, const Transform &tf2, float f) {
    ScaleAddEq(tf1.m, tf2.m, f);
    ScaleAddEq(tf1.v, tf2.v, f);
}

float AngleBetween(const Hmx::Quat &q1, const Hmx::Quat &q2);

inline void Transpose(const Hmx::Matrix3 &min, Hmx::Matrix3 &mout) {
    mout.Set(
        min.x.x, min.y.x, min.z.x, min.x.y, min.y.y, min.z.y, min.x.z, min.y.z, min.z.z
    );
}

inline void Transpose(const Transform &tfin, Transform &tfout) {
    Vector3 vtmp;
    Transpose(tfin.m, tfout.m);
    Negate(tfin.v, vtmp);
    Multiply(vtmp, tfout.m, tfout.v);
}

inline void Normalize(const Hmx::Matrix3 &src, Hmx::Matrix3 &dst) {
    Normalize(src.y, dst.y);
    {
        float y1 = dst.y.y;
        float z2 = src.z.z;
        float x2 = src.z.x;
        float z1 = dst.y.z;
        float y2 = src.z.y;
        float x1 = dst.y.x;
        float a = y1 * z2;
        float b = y1 * x2;
        float c = z1 * x2;
        float d = z1 * y2;
        float e = x1 * y2;
        float f = x1 * z2;
        dst.x.x = a - d;
        dst.x.y = c - f;
        dst.x.z = e - b;
    }
    Normalize(dst.x, dst.x);
    Cross(dst.x, dst.y, dst.z);
}

inline void Multiply(const Vector3 &vin, const Hmx::Matrix3 &mtx, Vector3 &vout) {
#ifdef HX_NATIVE
    // V21 — ASM_BLOCK is a no-op under clang LP64 (see decomp.h:73). The
    // matched-fork body below thus never writes to vout, returning garbage
    // and breaking every IK / skeleton / interest-cam / target-resolution
    // path that consumes Multiply(Vector3,Matrix3,Vector3). Provide the C
    // body (matches dc3-decomp/src/system/math/Mtx.h:425). Permuter never
    // touches this branch; `#else` below is byte-identical to the matched
    // fork.
    vout.Set(
        mtx.x.x * vin.x + mtx.y.x * vin.y + mtx.z.x * vin.z,
        mtx.x.y * vin.x + mtx.y.y * vin.y + mtx.z.y * vin.z,
        mtx.x.z * vin.x + mtx.y.z * vin.y + mtx.z.z * vin.z
    );
#else
    register __vec2x32float__ i1, i2, m1, m2, o1, o2;

    register const Vector3 *_vin = &vin;
    register Vector3 *_vout = &vout;
    register const Hmx::Matrix3 *_m = &mtx;

    typedef Hmx::Matrix3 Matrix3;

    ASM_BLOCK(
        psq_l i1, Vector3.x(_vin), 0, 0
        psq_l i2, Vector3.y(_vin), 0, 0

        psq_l m1, Matrix3.z.x(_m), 0, 0
        psq_l m2, Matrix3.z.z(_m), 1, 0

        ps_muls1 o1, m1, i2
        ps_muls1 o2, m2, i2

        psq_l m1, Matrix3.y.x(_m), 0, 0
        psq_l m2, Matrix3.y.z(_m), 1, 0

        ps_madds0 o1, m1, i2, o1
        ps_madds0 o2, m2, i2, o2

        psq_l m1, Matrix3.x.x(_m), 0, 0
        psq_l m2, Matrix3.x.z(_m), 1, 0

        ps_madds0 o1, m1, i1, o1
        ps_madds0 o2, m2, i1, o2

        psq_st o1, Vector3.x(_vout), 0, 0
        psq_st o2, Vector3.z(_vout), 1, 0
    )
#endif
}

inline void Invert(const Transform &tfin, Transform &tfout) {
#ifdef VERSION_SZBE69_B8 // DEBUG
    Vector3 vtmp;
    vtmp.z = -tfin.v.z;
    vtmp.x = -tfin.v.x;
    vtmp.y = -tfin.v.y;
#else // RETAIL
    Vector3 vtmp;
    Negate(tfin.v, vtmp);
#endif
    Invert(tfin.m, tfout.m);
    Multiply(vtmp, tfout.m, tfout.v);
}

inline void FastInvert(const Transform &tfin, Transform &tfout) {
#ifdef VERSION_SZBE69_B8 // DEBUG
    Vector3 vtmp;
    vtmp.z = -tfin.v.z;
    vtmp.x = -tfin.v.x;
    vtmp.y = -tfin.v.y;
#else // RETAIL
    Vector3 vtmp;
    Negate(tfin.v, vtmp);
#endif
    FastInvert(tfin.m, tfout.m);
    Multiply(vtmp, tfout.m, tfout.v);
}

// https://en.wikipedia.org/wiki/Gram%E2%80%93Schmidt_process
// https://gamedev.stackexchange.com/questions/139703/compute-up-and-right-from-a-direction
// Looks similar to C_MTXLookAt from the dolphin SDK.
inline void LookAt(Hmx::Matrix3 &mtx) {
    Cross(mtx.x, mtx.y, mtx.z);
    Normalize(mtx.z, mtx.z);
    Cross(mtx.z, mtx.x, mtx.y);
}

#endif
