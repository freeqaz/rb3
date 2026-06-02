#include "char/CharBones.h"
#include "char/CharClip.h"
#include "decomp.h"
#include "math/Mtx.h"
#include "math/Rot.h"
#include "math/Vec.h"
#include "utl/MakeString.h"
#include "utl/Symbols.h"

void TestDstComplain(Symbol s) {
    MILO_NOTIFY_ONCE("src %s not in dst, punting animation", s);
}

CharBones *gPropBones;

CharBones::CharBones() : mCompression(kCompressNone), mStart(0), mTotalSize(0) {
    for (int i = 0; i < NUM_TYPES; i++) {
        mCounts[i] = 0;
        mOffsets[i] = 0;
    }
}

CharBones::Type CharBones::TypeOf(Symbol s) {
    for (const char *p = s.Str(); *p != 0; p++) {
        if (*p == '.') {
            switch (*++p) {
            case 'p':
                return TYPE_POS;
            case 's':
                return TYPE_SCALE;
            case 'q':
                return TYPE_QUAT;
            case 'r': {
                // check if rot is x, y, or z
                unsigned char next = p[3];
                if ((unsigned char)(next - 'x') <= 2)
                    return (Type)((char)next - 'u');
            }
            default:
                break;
            }
        }
    }
    MILO_FAIL("Unknown bone suffix in %s", s);
    return NUM_TYPES;
}

const char *CharBones::SuffixOf(CharBones::Type t) {
    static const char *suffixes[NUM_TYPES] = { "pos",  "scale", "quat",
                                               "rotx", "roty",  "rotz" };
    MILO_ASSERT(t < TYPE_END, 0x66);
    return suffixes[t];
}

DECOMP_FORCEACTIVE(
    CharBones, " (added:", " ", ", ", ")", " (removed:", " (weights:", ":", "->"
)

Symbol CharBones::ChannelName(const char *cc, CharBones::Type t) {
    MILO_ASSERT(t < TYPE_END, 0x6F);
    char buf[256];
    strcpy(buf, cc);
    char *chr = strchr(buf, '.');
    if (!chr) {
        chr = buf + strlen(buf);
        *chr = '.';
    }
    strcpy(chr + 1, SuffixOf(t));
    return Symbol(buf);
}

void CharBones::ClearBones() {
    mBones.clear();
    for (int i = 0; i < NUM_TYPES; i++) {
        mCounts[i] = 0;
        mOffsets[i] = 0;
    }
    mTotalSize = 0;
    mCompression = kCompressNone;
    ReallocateInternal();
}

void CharBones::ReallocateInternal() {}

void CharBones::SetWeights(float f) { SetWeights(f, mBones); }

void CharBones::AddBoneInternal(const Bone &bone) {
    int type = TypeOf(bone.name);
    int pos = mCounts[type];
    int end = mCounts[type + 1];
    while (pos < end) {
        if (mBones[pos].name == bone.name)
            return;
        if (strcmp(mBones[pos].name.Str(), bone.name.Str()) >= 0)
            break;
        pos++;
    }
#ifdef HX_NATIVE
    mBones.insert(mBones.begin() + pos, bone);
#else
    mBones.insert(mBones.data() + pos, bone);
#endif
    int size = TypeSize(type);
    type++;
    while (type < NUM_TYPES) {
        mCounts[type]++;
        mOffsets[type] += size;
        type++;
    }
    mTotalSize = (mOffsets[TYPE_END] + 0xFU) & 0xFFFFFFF0;
}

void CharBones::AddBones(const std::vector<Bone> &vec) {
    for (std::vector<Bone>::const_iterator it = vec.begin(); it != vec.end(); ++it) {
        AddBoneInternal(*it);
    }
    ReallocateInternal();
}

void CharBones::AddBones(const std::list<Bone> &bones) {
    for (std::list<Bone>::const_iterator it = bones.begin(); it != bones.end(); ++it) {
        AddBoneInternal(*it);
    }
    ReallocateInternal();
}

void CharBones::ListBones(std::list<Bone> &bones) const {
    for (int i = 0; i < mBones.size(); i++) {
        bones.push_back(mBones[i]);
    }
}

void CharBones::Zero() { memset(mStart, 0, mTotalSize); }

int CharBones::TypeSize(int i) const {
    switch (i) {
    case TYPE_POS:
    case TYPE_SCALE:
        if (mCompression >= kCompressVects)
            return 6;
        else
            return 12;
    case TYPE_QUAT:
        if (mCompression >= kCompressQuats)
            return 4;
        else if (mCompression != kCompressNone)
            return 8;
        else
            return 16;

    default:
        if (mCompression != kCompressNone)
            return 2;
        else
            return 4;
    }
}

int CharBones::FindOffset(Symbol s) const {
    Type ty = TypeOf(s);
    int nextcount = mCounts[ty + 1];
    int size = TypeSize(ty);
    int count = mCounts[ty];
    int offset = mOffsets[ty];
    for (int i = count; i < nextcount; i++, offset += size) {
        if (mBones[i].name == s)
            return offset;
    }
    return -1;
}

void CharBones::SetWeights(float wt, std::vector<Bone> &bones) {
    for (int i = 0; i < bones.size(); i++) {
        bones[i].weight = wt;
    }
}

void *CharBones::FindPtr(Symbol s) const {
    int offset = FindOffset(s);
    if (offset == -1)
        return 0;
    else
        return (void *)&mStart[offset];
}

void CharBones::ScaleDown(CharBones &dst, float f) const {
    if (mBones.size() == 0)
        return;
    const Bone *src = mBones.data();

    if (f == 0.0f) {
        if (mCounts[TYPE_QUAT] > mCounts[TYPE_POS]) {
            Bone *db_begin = dst.mBones.data();
            Vector3 *data = (Vector3 *)dst.mStart;
            Bone *db = db_begin + dst.mCounts[TYPE_POS];
            Bone *db_end = db_begin + dst.mCounts[TYPE_QUAT];
            const Bone *src_end = src + mCounts[TYPE_QUAT];
            while (true) {
                while (db->name != src->name) {
                    db++;
                    if (db >= db_end) {
                        TestDstComplain(src->name);
                        return;
                    }
                    data++;
                }
                src++;
                data->z = 0.0f;
                data->y = 0.0f;
                data->x = 0.0f;
                db->weight = 0.0f;
                if (src == src_end)
                    goto zero_quat;
                db++;
                if (db >= db_end) {
                    TestDstComplain(src->name);
                    return;
                }
                data++;
            }
        }
    zero_quat:
        if (mCounts[TYPE_ROTX] > mCounts[TYPE_QUAT]) {
            Bone *db_begin = dst.mBones.data();
            Bone *db = db_begin + dst.mCounts[TYPE_QUAT];
            Hmx::Quat *qdata = (Hmx::Quat *)(dst.mStart + dst.mOffsets[TYPE_QUAT]);
            const Bone *src_end = mBones.data() + mCounts[TYPE_ROTX];
            Bone *db_end = db_begin + dst.mCounts[TYPE_ROTX];
            while (true) {
                while (db->name != src->name) {
                    db++;
                    if (db >= db_end) {
                        TestDstComplain(src->name);
                        return;
                    }
                    qdata++;
                }
                src++;
                qdata->x = 0.0f;
                qdata->y = 0.0f;
                qdata->z = 0.0f;
                qdata->w = 0.0f;
                db->weight = 0.0f;
                if (src == src_end)
                    goto zero_rot;
                db++;
                if (db >= db_end) {
                    TestDstComplain(src->name);
                    return;
                }
                qdata++;
            }
        }
    zero_rot:
        if (mCounts[TYPE_END] > mCounts[TYPE_ROTX]) {
            Bone *db_begin = dst.mBones.data();
            float *fdata = (float *)(dst.mStart + dst.mOffsets[TYPE_ROTX]);
            Bone *db = db_begin + dst.mCounts[TYPE_ROTX];
            Bone *db_end = db_begin + dst.mCounts[TYPE_END];
            const Bone *src_end = mBones.data() + mCounts[TYPE_END];
            while (true) {
                while (db->name != src->name) {
                    db++;
                    if (db >= db_end) {
                        TestDstComplain(src->name);
                        return;
                    }
                    fdata++;
                }
                src++;
                *fdata = 0.0f;
                db->weight = 0.0f;
                if (src == src_end)
                    return;
                db++;
                if (db >= db_end) {
                    TestDstComplain(src->name);
                    return;
                }
                fdata++;
            }
        }
    } else {
        if (mCounts[TYPE_QUAT] > mCounts[TYPE_POS]) {
            Bone *db_begin = dst.mBones.data();
            Vector3 *data = (Vector3 *)dst.mStart;
            Bone *db = db_begin + dst.mCounts[TYPE_POS];
            Bone *db_end = db_begin + dst.mCounts[TYPE_QUAT];
            const Bone *src_end = src + mCounts[TYPE_QUAT];
            while (true) {
                while (db->name != src->name) {
                    db++;
                    if (db >= db_end) {
                        TestDstComplain(src->name);
                        return;
                    }
                    data++;
                }
                src++;
                data->x *= f;
                data->y *= f;
                data->z *= f;
                if (src == src_end)
                    goto scale_quat;
                db++;
                if (db >= db_end) {
                    TestDstComplain(src->name);
                    return;
                }
                data++;
            }
        }
    scale_quat:
        if (mCounts[TYPE_ROTX] > mCounts[TYPE_QUAT]) {
            Bone *db_begin = dst.mBones.data();
            Bone *db = db_begin + dst.mCounts[TYPE_QUAT];
            Hmx::Quat *qdata = (Hmx::Quat *)(dst.mStart + dst.mOffsets[TYPE_QUAT]);
            const Bone *src_end = mBones.data() + mCounts[TYPE_ROTX];
            Bone *db_end = db_begin + dst.mCounts[TYPE_ROTX];
            while (true) {
                while (db->name != src->name) {
                    db++;
                    if (db >= db_end) {
                        TestDstComplain(src->name);
                        return;
                    }
                    qdata++;
                }
                src++;
                qdata->x *= f;
                qdata->y *= f;
                qdata->z *= f;
                qdata->w *= f;
                if (src == src_end)
                    goto scale_rot;
                db++;
                if (db >= db_end) {
                    TestDstComplain(src->name);
                    return;
                }
                qdata++;
            }
        }
    scale_rot:
        if (mCounts[TYPE_END] > mCounts[TYPE_ROTX]) {
            Bone *db_begin = dst.mBones.data();
            float *fdata = (float *)(dst.mStart + dst.mOffsets[TYPE_ROTX]);
            Bone *db = db_begin + dst.mCounts[TYPE_ROTX];
            Bone *db_end = db_begin + dst.mCounts[TYPE_END];
            const Bone *src_end = mBones.data() + mCounts[TYPE_END];
            while (true) {
                while (db->name != src->name) {
                    db++;
                    if (db >= db_end) {
                        TestDstComplain(src->name);
                        return;
                    }
                    fdata++;
                }
                src++;
                *fdata *= f;
                if (src == src_end)
                    return;
                db++;
                if (db >= db_end) {
                    TestDstComplain(src->name);
                    return;
                }
                fdata++;
            }
        }
    }
}

void CharBones::ScaleAdd(CharBones &dst, float f) const {
    if (!mBones.size())
        return;
    const Bone *src = mBones.data();

    if (mCounts[TYPE_QUAT] > mCounts[TYPE_POS]) {
        Vector3 *ddata = (Vector3 *)dst.mStart;
        Bone *db = dst.mBones.data() + dst.mCounts[TYPE_POS];
        Bone *db_end = dst.mBones.data() + dst.mCounts[TYPE_QUAT];
        const Bone *src_end = src + mCounts[TYPE_QUAT];
        if (mCompression >= kCompressVects) {
            short *sdata = (short *)mStart;
            while (true) {
                float fz = (float)sdata[2] * 0.039674062;
                float fy = (float)sdata[1] * 0.039674062f;
                float fx = (float)sdata[0] * 0.039674062f;
                while (db->name != src->name) {
                    db++;
                    if (db >= db_end) {
                        TestDstComplain(src->name);
                        return;
                    }
                    ddata++;
                }
                ddata->x += fx * f;
                ddata->y += fy * f;
                ddata->z += fz * f;
                db->weight += src->weight * f;
                src++;
                if (src == src_end)
                    goto add_quat;
                db++;
                if (db >= db_end) {
                    TestDstComplain(src->name);
                    return;
                }
                ddata++;
                sdata += 3;
            }
        } else {
            Vector3 *sdata = (Vector3 *)mStart;
            while (true) {
                while (db->name != src->name) {
                    db++;
                    if (db >= db_end) {
                        TestDstComplain(src->name);
                        return;
                    }
                    ddata++;
                }
                ddata->x += sdata->x * f;
                ddata->y += sdata->y * f;
                ddata->z += sdata->z * f;
                db->weight += src->weight * f;
                src++;
                if (src == src_end)
                    goto add_quat;
                db++;
                if (db >= db_end) {
                    TestDstComplain(src->name);
                    return;
                }
                ddata++;
                sdata++;
            }
        }
    }
add_quat:
    if (mCounts[TYPE_ROTX] > mCounts[TYPE_QUAT]) {
        Bone *db_end = dst.mBones.data() + dst.mCounts[TYPE_ROTX];
        const Bone *src_end = mBones.data() + mCounts[TYPE_ROTX];
        Bone *db = dst.mBones.data() + dst.mCounts[TYPE_QUAT];
        Hmx::Quat *dquat = (Hmx::Quat *)(dst.mStart + dst.mOffsets[TYPE_QUAT]);
        float abs_f = fabs(f);
        if (mCompression >= kCompressQuats) {
            char *sdata = (char *)(mStart + mOffsets[TYPE_QUAT]);
            float scale = abs_f * 0.0078740157f;
            float swscale = f * 0.0078740157f;
            while (true) {
                while (db->name != src->name) {
                    db++;
                    if (db >= db_end) {
                        TestDstComplain(src->name);
                        return;
                    }
                    dquat++;
                }
                float sw = (float)sdata[3] * swscale;
                float sy = (float)sdata[1] * scale;
                float sz = (float)sdata[2] * scale;
                float sx = (float)sdata[0] * scale;
                if ((sy * dquat->y + (sw * dquat->w + (sz * dquat->z + sx * dquat->x))) < 0.0f) {
                    dquat->x -= sx;
                    dquat->y -= sy;
                    dquat->z -= sz;
                    dquat->w -= sw;
                } else {
                    dquat->x += sx;
                    dquat->y += sy;
                    dquat->z += sz;
                    dquat->w += sw;
                }
                db->weight += src->weight * f;
                src++;
                if (src == src_end)
                    goto add_rot;
                db++;
                if (db >= db_end) {
                    TestDstComplain(src->name);
                    return;
                }
                dquat++;
                sdata += 4;
            }
        } else if (mCompression != kCompressNone) {
            short *sdata = (short *)(mStart + mOffsets[TYPE_QUAT]);
            float scale = abs_f * 3.051851e-05f;
            float swscale = f * 3.051851e-05f;
            while (true) {
                while (db->name != src->name) {
                    db++;
                    if (db >= db_end) {
                        TestDstComplain(src->name);
                        return;
                    }
                    dquat++;
                }
                float sw = (float)sdata[3] * swscale;
                float sy = (float)sdata[1] * scale;
                float sz = (float)sdata[2] * scale;
                float sx = (float)sdata[0] * scale;
                if (sw * dquat->w + sz * dquat->z + sx * dquat->x + sy * dquat->y < 0.0f) {
                    dquat->x -= sx;
                    dquat->y -= sy;
                    dquat->z -= sz;
                    dquat->w -= sw;
                } else {
                    dquat->x += sx;
                    dquat->y += sy;
                    dquat->z += sz;
                    dquat->w += sw;
                }
                db->weight += src->weight * f;
                src++;
                if (src == src_end)
                    goto add_rot;
                db++;
                if (db >= db_end) {
                    TestDstComplain(src->name);
                    return;
                }
                dquat++;
                sdata += 4;
            }
        } else {
            Hmx::Quat *squat = (Hmx::Quat *)(mStart + mOffsets[TYPE_QUAT]);
            while (true) {
                while (db->name != src->name) {
                    db++;
                    if (db >= db_end) {
                        TestDstComplain(src->name);
                        return;
                    }
                    dquat++;
                }
                float sx = squat->x * abs_f;
                float sy = squat->y * abs_f;
                float sz = squat->z * abs_f;
                float sw = squat->w * f;
                if (sy * dquat->y + sx * dquat->x + sz * dquat->z + sw * dquat->w < 0.0f) {
                    dquat->x -= sx;
                    dquat->y -= sy;
                    dquat->z -= sz;
                    dquat->w -= sw;
                } else {
                    dquat->x += sx;
                    dquat->y += sy;
                    dquat->z += sz;
                    dquat->w += sw;
                }
                db->weight += src->weight * f;
                src++;
                if (src == src_end)
                    goto add_rot;
                db++;
                if (db >= db_end) {
                    TestDstComplain(src->name);
                    return;
                }
                dquat++;
                squat++;
            }
        }
    }
add_rot:
    if (mCounts[TYPE_END] > mCounts[TYPE_ROTX]) {
        const Bone *src_end = mBones.data() + mCounts[TYPE_END];
        Bone *db = dst.mBones.data() + dst.mCounts[TYPE_ROTX];
        Bone *db_end = dst.mBones.data() + dst.mCounts[TYPE_END];
        float *dfdata = (float *)(dst.mStart + dst.mOffsets[TYPE_ROTX]);
        if (mCompression != kCompressNone) {
            float *sfdata = (float *)(mStart + mOffsets[TYPE_ROTX]);
            float sf = 0.0006103515625f * f;
            while (true) {
                while (db->name != src->name) {
                    db++;
                    if (db >= db_end) {
                        TestDstComplain(src->name);
                        return;
                    }
                    dfdata++;
                }
                *dfdata += (float)*(short *)sfdata * sf;
                db->weight += src->weight * f;
                src++;
                if (src == src_end)
                    return;
                db++;
                if (db >= db_end) {
                    TestDstComplain(src->name);
                    return;
                }
                dfdata++;
                sfdata = (float *)((char *)sfdata + 2);
            }
        } else {
            float *sfdata = (float *)(mStart + mOffsets[TYPE_ROTX]);
            while (true) {
                while (db->name != src->name) {
                    db++;
                    if (db >= db_end) {
                        TestDstComplain(src->name);
                        return;
                    }
                    dfdata++;
                }
                *dfdata += *sfdata * f;
                db->weight += src->weight * f;
                src++;
                if (src == src_end)
                    return;
                db++;
                if (db >= db_end) {
                    TestDstComplain(src->name);
                    return;
                }
                dfdata++;
                sfdata++;
            }
        }
    }
}

void CharBones::Blend(CharBones &bones) const {
    MILO_ASSERT(!mCompression && !bones.mCompression, 0x311);
    if (mBones.empty())
        return;
    const Bone *src = mBones.data();

    if (mCounts[TYPE_QUAT] > mCounts[TYPE_POS]) {
        Vector3 *sdata = (Vector3 *)mStart;
        Vector3 *ddata = (Vector3 *)bones.mStart;
        Bone *db = bones.mBones.data() + bones.mCounts[TYPE_POS];
        Bone *db_end = bones.mBones.data() + bones.mCounts[TYPE_QUAT];
        const Bone *src_end = src + mCounts[TYPE_QUAT];
        while (true) {
            while (db->name != src->name) {
                db++;
                if (db >= db_end) {
                    TestDstComplain(src->name);
                    return;
                }
                ddata++;
            }
            float wt = 1.0f - src->weight;
            ddata->x *= wt;
            ddata->y *= wt;
            ddata->z *= wt;
            ddata->x += sdata->x;
            ddata->y += sdata->y;
            ddata->z += sdata->z;
            src++;
            if (src == src_end)
                goto blend_quat;
            db++;
            if (db >= db_end) {
                TestDstComplain(src->name);
                return;
            }
            ddata++;
            sdata++;
        }
    }
blend_quat:
    if (mCounts[TYPE_ROTX] > mCounts[TYPE_QUAT]) {
        Bone *db = bones.mBones.data() + bones.mCounts[TYPE_QUAT];
        Bone *db_end = bones.mBones.data() + bones.mCounts[TYPE_ROTX];
        const Bone *src_end = mBones.data() + mCounts[TYPE_ROTX];
        Hmx::Quat *dquat = (Hmx::Quat *)(bones.mStart + bones.mOffsets[TYPE_QUAT]);
        Hmx::Quat *squat = (Hmx::Quat *)(mStart + mOffsets[TYPE_QUAT]);
        while (true) {
            while (db->name != src->name) {
                db++;
                if (db >= db_end) {
                    TestDstComplain(src->name);
                    return;
                }
                dquat++;
            }
            float wt = 1.0f - src->weight;
            dquat->w *= wt;
            dquat->x *= wt;
            dquat->y *= wt;
            dquat->z *= wt;
            float abs_wt = fabsf(src->weight);
            float sy = squat->y * abs_wt;
            float sx = squat->x * abs_wt;
            float sz = squat->z * abs_wt;
            float sw = src->weight * squat->w;
            if (((dquat->x * sx + (dquat->y * sy + (dquat->w * sw + dquat->z * sz))))
                < 0.0f) {
                dquat->x -= sx;
                dquat->y -= sy;
                dquat->z -= sz;
                dquat->w -= sw;
            } else {
                dquat->x += sx;
                dquat->y += sy;
                dquat->z += sz;
                dquat->w += sw;
            }
            src++;
            if (src == src_end)
                goto blend_rot;
            db++;
            if (db >= db_end) {
                TestDstComplain(src->name);
                return;
            }
            dquat++;
            squat++;
        }
    }
blend_rot:
    if (mCounts[TYPE_END] > mCounts[TYPE_ROTX]) {
        Bone *db = bones.mBones.data() + bones.mCounts[TYPE_ROTX];
        Bone *db_end = bones.mBones.data() + bones.mCounts[TYPE_END];
        float *dfdata = (float *)(bones.mStart + bones.mOffsets[TYPE_ROTX]);
        float *sfdata = (float *)(mStart + mOffsets[TYPE_ROTX]);
        const Bone *src_end = mBones.data() + mCounts[TYPE_END];
        while (true) {
            while (db->name != src->name) {
                db++;
                if (db >= db_end) {
                    TestDstComplain(src->name);
                    return;
                }
                dfdata++;
            }
            *dfdata *= (1.0f - src->weight);
            float wt = src->weight;
            *dfdata += wt * *sfdata;
            src++;
            if (src == src_end)
                return;
            db++;
            if (db >= db_end) {
                TestDstComplain(src->name);
                return;
            }
            dfdata++;
            sfdata++;
        }
    }
}

void CharBones::RotateBy(CharBones &dst) const {
    int _tmp1 = mBones.size();
    if (_tmp1 == 0)
        return;
    const Bone *src = mBones.data();

    if (mCounts[TYPE_QUAT] > mCounts[TYPE_POS]) {
        const Bone *src_end = src + mCounts[TYPE_QUAT];
        Bone *db = dst.mBones.data() + dst.mCounts[TYPE_POS];
        Bone *db_end = dst.mBones.data() + dst.mCounts[TYPE_QUAT];
        Vector3 *ddata = (Vector3 *)dst.mStart;
        if (mCompression >= kCompressVects) {
            short *sdata = (short *)mStart;
            while (true) {
                float fz = (float)sdata[2] * 0.000030518509f * 1300.0f;
                float fy = (float)sdata[1] * 0.000030518509f * 1300.0f;
                float fx = (float)sdata[0] * 0.000030518509f * 1300.0f;
                while (db->name != src->name) {
                    db++;
                    if (db >= db_end) {
                        TestDstComplain(src->name);
                        return;
                    }
                    ddata++;
                }
                src++;
                ddata->x += fx;
                ddata->y += fy;
                ddata->z += fz;
                if (src_end == src)
                    goto rotate_quat;
                db++;
                if (db >= db_end) {
                    TestDstComplain(src->name);
                    return;
                }
                ddata++;
                sdata += 3;
            }
        } else {
            Vector3 *sdata = (Vector3 *)mStart;
            while (true) {
                while (db->name != src->name) {
                    db++;
                    if (db >= db_end) {
                        TestDstComplain(src->name);
                        return;
                    }
                    ddata++;
                }
                src++;
                ddata->x += sdata->x;
                ddata->y += sdata->y;
                ddata->z += sdata->z;
                if (src == src_end)
                    goto rotate_quat;
                db++;
                if (db >= db_end) {
                    TestDstComplain(src->name);
                    return;
                }
                ddata++;
                sdata++;
            }
        }
    }
rotate_quat:
    if (mCounts[TYPE_ROTX] > mCounts[TYPE_QUAT]) {
        const Bone *src_end = mBones.data() + mCounts[TYPE_ROTX];
        Bone *db = dst.mBones.data() + dst.mCounts[TYPE_QUAT];
        Bone *db_end = dst.mBones.data() + dst.mCounts[TYPE_ROTX];
        Hmx::Quat *dquat = (Hmx::Quat *)(dst.mStart + dst.mOffsets[TYPE_QUAT]);
        if (mCompression >= kCompressQuats) {
            ByteQuat *sqdata = (ByteQuat *)(mStart + mOffsets[TYPE_QUAT]);
            while (true) {
                while (db->name != src->name) {
                    db++;
                    if (db >= db_end) {
                        TestDstComplain(src->name);
                        return;
                    }
                    dquat++;
                }
                src++;
                float dw = dquat->w;
                float sqw = (float)sqdata->w * 0.0078740157f;
                float sqz = (float)sqdata->z * 0.0078740157f;
                float dx = dquat->x;
                float dy = dquat->y;
                float dz = dquat->z;
                float sqx = (float)sqdata->x * 0.0078740157f;
                float sqy = (float)sqdata->y * 0.0078740157f;
                dquat->x = -(sqz * dy - (sqy * dz + (sqw * dx + sqx * dw)));
                dquat->y = -(sqx * dz - (sqz * dx + (sqw * dy + sqy * dw)));
                dquat->z = -(sqy * dx - (sqx * dy + (sqw * dz + sqz * dw)));
                dquat->w = -(sqz * dz - -(sqy * dy - (sqw * dw - sqx * dx)));
                if (src == src_end)
                    goto rotate_rot;
                db++;
                if (db >= db_end) {
                    TestDstComplain(src->name);
                    return;
                }
                dquat++;
                sqdata++;
            }
        } else if (mCompression != kCompressNone) {
            ShortQuat *sqdata = (ShortQuat *)(mStart + mOffsets[TYPE_QUAT]);
            while (true) {
                while (db->name != src->name) {
                    db++;
                    if (db >= db_end) {
                        TestDstComplain(src->name);
                        return;
                    }
                    dquat++;
                }
                src++;
                float dw = dquat->w;
                float sqz = (float)sqdata->z * 3.051851e-05f;
                float sqw = (float)sqdata->w * 3.051851e-05f;
                float dx = dquat->x;
                float sqx = (float)sqdata->x * 3.051851e-05f;
                float dy = dquat->y;
                float dz = dquat->z;
                float sqy = (float)sqdata->y * 3.051851e-05f;
                dquat->x = -(sqz * dy - (sqy * dz + (sqw * dx + sqx * dw)));
                dquat->y = -(sqx * dz - (sqz * dx + (sqw * dy + sqy * dw)));
                dquat->z = -(sqy * dx - (sqx * dy + (sqw * dz + sqz * dw)));
                dquat->w = -(sqz * dz - -(sqy * dy - (sqw * dw - sqx * dx)));
                if (src == src_end)
                    goto rotate_rot;
                db++;
                if (db >= db_end) {
                    TestDstComplain(src->name);
                    return;
                }
                dquat++;
                sqdata++;
            }
        } else {
            Hmx::Quat *squat = (Hmx::Quat *)(mStart + mOffsets[TYPE_QUAT]);
            while (true) {
                while (db->name != src->name) {
                    db++;
                    if (db >= db_end) {
                        TestDstComplain(src->name);
                        return;
                    }
                    dquat++;
                }
                float sx = squat->x;
                src++;
                float dw = dquat->w;
                float sy = squat->y;
                float dx = dquat->x;
                float sw = squat->w;
                float dy = dquat->y;
                float sz = squat->z;
                float dz = dquat->z;
                dquat->x = -(sz * dy - (sy * dz + (sw * dx + sx * dw)));
                dquat->y = -(sx * dz - (sz * dx + (sw * dy + sy * dw)));
                dquat->z = -(sy * dx - (sx * dy + (sw * dz + sz * dw)));
                dquat->w = -(sz * dz - -(sy * dy - (sw * dw - sx * dx)));
                if (src == src_end)
                    goto rotate_rot;
                db++;
                if (db >= db_end) {
                    TestDstComplain(src->name);
                    return;
                }
                dquat++;
                squat++;
            }
        }
    }
rotate_rot:
    if (mCounts[TYPE_END] > mCounts[TYPE_ROTX]) {
        const Bone *src_end = mBones.data() + mCounts[TYPE_END];
        Bone *db = dst.mBones.data() + dst.mCounts[TYPE_ROTX];
        Bone *db_end = dst.mBones.data() + dst.mCounts[TYPE_END];
        float *dfdata = (float *)(dst.mStart + dst.mOffsets[TYPE_ROTX]);
        if (mCompression != kCompressNone) {
            float *sfdata = (float *)(mStart + mOffsets[TYPE_ROTX]);
            while (true) {
                while (db->name != src->name) {
                    db++;
                    if (db >= db_end) {
                        TestDstComplain(src->name);
                        return;
                    }
                    dfdata++;
                }
                src++;
                *dfdata += (float)*(short *)sfdata * 0.00061035156f;
                if (src == src_end)
                    return;
                db++;
                if (db >= db_end) {
                    TestDstComplain(src->name);
                    return;
                }
                dfdata++;
                sfdata = (float *)((char *)sfdata + 2);
            }
        } else {
            float *sfdata = (float *)(mStart + mOffsets[TYPE_ROTX]);
            while (true) {
                while (db->name != src->name) {
                    db++;
                    if (db >= db_end) {
                        TestDstComplain(src->name);
                        return;
                    }
                    dfdata++;
                }
                src++;
                *dfdata += *sfdata;
                if (src == src_end)
                    return;
                db++;
                if (db >= db_end) {
                    TestDstComplain(src->name);
                    return;
                }
                dfdata++;
                sfdata++;
            }
        }
    }
}

void CharBones::RotateTo(CharBones &dst, float f) const {
    auto _tmp0 = mBones.size();
    if (_tmp0 == 0)
        return;
    const Bone *src = mBones.data();

    if (mCounts[TYPE_QUAT] > mCounts[TYPE_POS]) {
        const Bone *src_end = src + mCounts[TYPE_QUAT];
        Bone *db = dst.mBones.data() + dst.mCounts[TYPE_POS];
        Bone *db_end = dst.mBones.data() + dst.mCounts[TYPE_QUAT];
        Vector3 *ddata = (Vector3 *)dst.mStart;
        if (mCompression >= kCompressVects) {
            short *sdata = (short *)mStart;
            while (true) {
                float fsz = (float)sdata[2] * 3.051851e-05f * 1300.0f;
                float fsy = (float)sdata[1] * 3.051851e-05f * 1300.0f;
                float fsx = (float)sdata[0] * 3.051851e-05f * 1300.0f;
                while (db->name != src->name) {
                    db++;
                    if (db >= db_end) {
                        TestDstComplain(src->name);
                        return;
                    }
                    ddata++;
                }
                src++;
                ddata->x += fsx * f;
                ddata->y += fsy * f;
                ddata->z += fsz * f;
                if (src == src_end)
                    goto rotateto_quat;
                db++;
                if (db >= db_end) {
                    TestDstComplain(src->name);
                    return;
                }
                ddata++;
                sdata += 3;
            }
        } else {
            Vector3 *sdata = (Vector3 *)mStart;
            while (true) {
                while (db->name != src->name) {
                    db++;
                    if (db >= db_end) {
                        TestDstComplain(src->name);
                        return;
                    }
                    ddata++;
                }
                src++;
                ddata->x += sdata->x * f;
                ddata->y += sdata->y * f;
                ddata->z += sdata->z * f;
                if (src == src_end)
                    goto rotateto_quat;
                db++;
                if (db >= db_end) {
                    TestDstComplain(src->name);
                    return;
                }
                ddata++;
                sdata++;
            }
        }
    }
rotateto_quat:
    if (mCounts[TYPE_ROTX] > mCounts[TYPE_QUAT]) {
        const Bone *src_end = mBones.data() + mCounts[TYPE_ROTX];
        Bone *db = dst.mBones.data() + dst.mCounts[TYPE_QUAT];
        Bone *db_end = dst.mBones.data() + dst.mCounts[TYPE_ROTX];
        Hmx::Quat *dquat = (Hmx::Quat *)(dst.mStart + dst.mOffsets[TYPE_QUAT]);
        if (mCompression >= kCompressQuats) {
            ByteQuat *sqdata = (ByteQuat *)(mStart + mOffsets[TYPE_QUAT]);
            float one_minus_f = 1.0f - f;
            while (true) {
                while (db->name != src->name) {
                    db++;
                    if (db >= db_end) {
                        TestDstComplain(src->name);
                        return;
                    }
                    dquat++;
                }
                float sqw = (float)sqdata->w * 0.0078740157f;
                float sqx = (float)sqdata->x * 0.0078740157f;
                float sqy = (float)sqdata->y * 0.0078740157f;
                float sqz = (float)sqdata->z * 0.0078740157f;
                float sx = sqx * f;
                float sy = sqy * f;
                float sz = sqz * f;
                float sw;
                if (sqw < 0.0f) {
                    float tmp = sqw * f;
                    sw = tmp - one_minus_f;
                } else {
                    float tmp = sqw * f;
                    sw = tmp + one_minus_f;
                }
                float dx = dquat->x;
                src++;
                float dy = dquat->y;
                float dw = dquat->w;
                float dz = dquat->z;
                dquat->x = -(dz * sy - (dw * sx + dy * sz + dx * sw));
                dquat->y = -(dx * sz - ((dz * sx + (dy * sw + dw * sy))));
                dquat->z = -(dy * sx - (dw * sz + dz * sw + dx * sy));
                dquat->w = -(dz * sz - -(dy * sy - (dw * sw - dx * sx)));
                if (src == src_end)
                    goto rotateto_rot;
                db++;
                if (db >= db_end) {
                    TestDstComplain(src->name);
                    return;
                }
                dquat++;
                sqdata++;
            }
        } else if (mCompression != kCompressNone) {
            ShortQuat *sqdata = (ShortQuat *)(mStart + mOffsets[TYPE_QUAT]);
            float one_minus_f = 1.0f - f;
            while (true) {
                while (db->name != src->name) {
                    db++;
                    if (db >= db_end) {
                        TestDstComplain(src->name);
                        return;
                    }
                    dquat++;
                }
                float sqw = (float)sqdata->w * 3.051851e-05f;
                float sqx = (float)sqdata->x * 3.051851e-05f;
                float sqy = (float)sqdata->y * 3.051851e-05f;
                float sqz = (float)sqdata->z * 3.051851e-05f;
                float sx = sqx * f;
                float sy = sqy * f;
                float sz = sqz * f;
                float sw;
                if (sqw < 0.0f) {
                    float tmp = sqw * f;
                    sw = tmp - one_minus_f;
                } else {
                    float tmp = sqw * f;
                    sw = tmp + one_minus_f;
                }
                float dx = dquat->x;
                src++;
                float dy = dquat->y;
                float dw = dquat->w;
                float dz = dquat->z;
                dquat->x = -(dz * sy - (dw * sx + dy * sz + dx * sw));
                dquat->y = -(dx * sz - ((dz * sx + (dy * sw + dw * sy))));
                dquat->z = -(dy * sx - (dw * sz + dz * sw + dx * sy));
                dquat->w = -(dz * sz - -(dy * sy - (dw * sw - dx * sx)));
                if (src == src_end)
                    goto rotateto_rot;
                db++;
                if (db >= db_end) {
                    TestDstComplain(src->name);
                    return;
                }
                dquat++;
                sqdata++;
            }
        } else {
            Hmx::Quat *squat = (Hmx::Quat *)(mStart + mOffsets[TYPE_QUAT]);
            float one_minus_f = 1.0f - f;
            while (true) {
                while (db->name != src->name) {
                    db++;
                    if (db >= db_end) {
                        TestDstComplain(src->name);
                        return;
                    }
                    dquat++;
                }
                float sw = squat->w * f;
                float sz = f * squat->z;
                float sy = squat->y * f;
                float sx = f * squat->x;
                if (squat->w < 0.0f) {
                    sw = sw - one_minus_f;
                } else {
                    sw = one_minus_f + sw;
                }
                float dx = dquat->x;
                src++;
                float dy = dquat->y;
                float dw = dquat->w;
                float dz = dquat->z;
                dquat->x = -(sy * dz - (sw * dx + sz * dy + sx * dw));
                dquat->y = -(sz * dx - (sw * dy + sx * dz + sy * dw));
                dquat->z = -(sx * dy - (sw * dz + sy * dx + sz * dw));
                dquat->w = -(sz * dz - -(sy * dy - (sw * dw - sx * dx)));
                if (src == src_end)
                    goto rotateto_rot;
                db++;
                if (db >= db_end) {
                    TestDstComplain(src->name);
                    return;
                }
                dquat++;
                squat++;
            }
        }
    }
rotateto_rot:
    if (mCounts[TYPE_END] > mCounts[TYPE_ROTX]) {
        const Bone *src_end = mBones.data() + mCounts[TYPE_END];
        Bone *db = dst.mBones.data() + dst.mCounts[TYPE_ROTX];
        Bone *db_end = dst.mBones.data() + dst.mCounts[TYPE_END];
        float *dfdata = (float *)(dst.mStart + dst.mOffsets[TYPE_ROTX]);
        if (mCompression != kCompressNone) {
            float sf = f * 0.0006103515625f;
            short *sfdata = (short *)(mStart + mOffsets[TYPE_ROTX]);
            while (true) {
                while (db->name != src->name) {
                    db++;
                    if (db >= db_end) {
                        TestDstComplain(src->name);
                        return;
                    }
                    dfdata++;
                }
                src++;
                *dfdata += (float)*sfdata * sf;
                if (src == src_end)
                    return;
                db++;
                if (db >= db_end) {
                    TestDstComplain(src->name);
                    return;
                }
                dfdata++;
                sfdata++;
            }
        } else {
            float *sfdata = (float *)(mStart + mOffsets[TYPE_ROTX]);
            while (true) {
                while (db->name != src->name) {
                    db++;
                    if (db >= db_end) {
                        TestDstComplain(src->name);
                        return;
                    }
                    dfdata++;
                }
                src++;
                *dfdata += *sfdata * f;
                if (src == src_end)
                    return;
                db++;
                if (db >= db_end) {
                    TestDstComplain(src->name);
                    return;
                }
                dfdata++;
                sfdata++;
            }
        }
    }
}

void CharBones::ScaleAddIdentity() {
    Hmx::Quat *qend = (Hmx::Quat *)(mStart + mOffsets[TYPE_ROTX]);
    Bone *bone = mBones.data() + mCounts[TYPE_QUAT];
    Hmx::Quat *qstart = (Hmx::Quat *)(mStart + mOffsets[TYPE_QUAT]);
    while (qstart != qend) {
        float identity = 1.0f - bone->weight;
        float w = qstart->w;
        if (w < 0.0f) {
            w -= identity;
            qstart->w = w;
        } else {
            w += identity;
            qstart->w = w;
        }
        qstart++;
        bone++;
    }
}

void CharBones::RecomputeSizes() {
    mPosOffset = 0;
    for (int i = 0; i < NUM_TYPES; i++) {
        int diff = mCounts[i + 1] - mCounts[i];
        mOffsets[i + 1] = mOffsets[i] + diff * TypeSize(i);
    }
    mTotalSize = mEndOffset + 0xFU & 0xFFFFFFF0; // round up to the nearest 0x10,
                                                 // alignment moment
}

void CharBones::SetCompression(CompressionType ty) {
    if (ty != mCompression) {
        mCompression = ty;
        RecomputeSizes();
    }
}

DECOMP_FORCEACTIVE(CharBones, "!mCompression && !bones.mCompression")

const char *CharBones::StringVal(Symbol s) {
    void *ptr = FindPtr(s);
    CharBones::Type t = TypeOf(s);
    switch (t) {
    case TYPE_POS:
    case TYPE_SCALE:
        if (mCompression >= kCompressVects) {
            short *sptr = (short *)ptr;
            return MakeString(
                "%g %g %g",
                sptr[0] * 0.000030518509f * 1300.0f,
                sptr[1] * 0.000030518509f * 1300.0f,
                sptr[2] * 0.000030518509f * 1300.0f
            );
        } else {
            Vector3 *vptr = (Vector3 *)ptr;
            return MakeString("%g %g %g", vptr->x, vptr->y, vptr->z);
        }
    case TYPE_QUAT: {
        Hmx::Quat q;
        Hmx::Quat *qPtr = (Hmx::Quat *)ptr;
        if (mCompression >= kCompressQuats) {
            ByteQuat *bqPtr = (ByteQuat *)qPtr;
            bqPtr->ToQuat(q);
        } else if (mCompression != kCompressNone) {
            ShortQuat *sqPtr = (ShortQuat *)qPtr;
            sqPtr->ToQuat(q);
        } else
            q = *qPtr;
        Vector3 v40;
        MakeEuler(q, v40);
        v40 *= RAD2DEG;
        return MakeString(
            "quat(%g %g %g %g) euler(%g %g %g)", q.x, q.y, q.z, q.w, v40.x, v40.y, v40.z
        );
    }
    default: {
        float floatVal;
        if (mCompression != kCompressNone) {
            floatVal = *((short *)ptr) * 0.00061035156f;
        } else {
            floatVal = *((float *)ptr);
        }
        floatVal *= RAD2DEG;
        if (mCompression != kCompressNone) {
            return MakeString("deg %g raw %d", floatVal, *((short *)ptr));
        } else {
            return MakeString("deg %g rad %g", floatVal, *((float *)ptr));
        }
    }
    }
}

void CharBones::Print() {
    for (std::vector<Bone>::iterator it = mBones.begin(); it != mBones.end(); ++it) {
        MILO_LOG("%s %.2f: %s\n", it->name, it->weight, StringVal(it->name));
    }
}

DECOMP_FORCEACTIVE(
    CharBones, "!mCompression", "false", "newSize == 4", "oldSize == 2", "end >= start"
)

void CharBones::ScaleAdd(CharClip *clip, float f1, float f2, float f3) {
    clip->ScaleAdd(*this, f1, f2, f3);
}

#ifdef HX_NATIVE
extern void HxNoteFreedAddr(const void *);
extern void HxNoteReusedAddr(const void *);
CharBonesObject::CharBonesObject() { HxNoteReusedAddr((const void *)this); }
// Mark this CharBonesObject's address (the ObjPtr<CharBonesObject> mPtr
// representation) freed for the native use-after-free guard. Runs before the
// virtual Hmx::Object base destructs.
CharBonesObject::~CharBonesObject() { HxNoteFreedAddr((const void *)this); }
#endif

CharBonesAlloc::~CharBonesAlloc() { _MemFree(mStart); }

void CharBonesAlloc::ReallocateInternal() {
    _MemFree(mStart);
    mStart = (char *)_MemAlloc(mTotalSize, 0);
}

BinStream &operator>>(BinStream &bs, CharBones::Bone &bone) {
    bs >> bone.name;
    bs >> bone.weight;
    return bs;
}

BEGIN_CUSTOM_PROPSYNC(CharBones::Bone)
    SYNC_PROP(name, o.name)
    SYNC_PROP(weight, o.weight)
    SYNC_PROP_SET(preview_val, gPropBones->StringVal(o.name), )
END_CUSTOM_PROPSYNC

BEGIN_PROPSYNCS(CharBonesObject)
    gPropBones = this;
    if (sym == bones)
        return PropSync(mBones, _val, _prop, _i + 1, _op);
END_PROPSYNCS
