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
    Type type = TypeOf(bone.name);
    int end = mCounts[type + 1];
    int start = mCounts[type];
    int pos = start;
    if (start < end) {
        const char *name = bone.name.Str();
        do {
            const char *existing = mBones[pos].name.Str();
            if (existing == name)
                return;
            if (strcmp(existing, name) >= 0)
                break;
            pos++;
        } while (pos < end);
    }
    mBones.insert(mBones.begin() + pos, bone);
    int size = TypeSize(type);
    for (int i = type + 1; i < NUM_TYPES; i++) {
        mCounts[i]++;
        mOffsets[i] += size;
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
    const Bone *src = mBones.begin();
    if (src == mBones.end())
        return;

    if (f == 0.0f) {
        if (mCounts[TYPE_QUAT] > mCounts[TYPE_POS]) {
            Bone *db_begin = dst.mBones.begin();
            Vector3 *data = (Vector3 *)dst.mStart;
            Bone *db = db_begin + dst.mCounts[TYPE_POS];
            Bone *db_end = db_begin + dst.mCounts[TYPE_QUAT];
            const Bone *src_end = src + mCounts[TYPE_QUAT];
            while (true) {
                while (db->name != src->name) {
                    db++;
                    if (db >= db_end)
                        goto complain;
                    data++;
                }
                src++;
                data->z = 0.0f;
                data->y = 0.0f;
                data->x = 0.0f;
                db->weight = 0.0f;
                if (src >= src_end)
                    goto zero_quat;
                db++;
                if (db >= db_end)
                    goto complain;
                data++;
            }
        }
    zero_quat:
        if (mCounts[TYPE_ROTX] > mCounts[TYPE_QUAT]) {
            Bone *db_begin = dst.mBones.begin();
            Hmx::Quat *qdata = (Hmx::Quat *)(dst.mStart + dst.mOffsets[TYPE_QUAT]);
            Bone *db = db_begin + dst.mCounts[TYPE_QUAT];
            Bone *db_end = db_begin + dst.mCounts[TYPE_ROTX];
            const Bone *src_end = mBones.begin() + mCounts[TYPE_ROTX];
            while (true) {
                while (db->name != src->name) {
                    db++;
                    if (db >= db_end)
                        goto complain;
                    qdata++;
                }
                src++;
                qdata->x = 0.0f;
                qdata->y = 0.0f;
                qdata->z = 0.0f;
                qdata->w = 0.0f;
                db->weight = 0.0f;
                if (src >= src_end)
                    goto zero_rot;
                db++;
                if (db >= db_end)
                    goto complain;
                qdata++;
            }
        }
    zero_rot:
        if (mCounts[TYPE_END] > mCounts[TYPE_ROTX]) {
            Bone *db_begin = dst.mBones.begin();
            float *fdata = (float *)(dst.mStart + dst.mOffsets[TYPE_ROTX]);
            Bone *db = db_begin + dst.mCounts[TYPE_ROTX];
            Bone *db_end = db_begin + dst.mCounts[TYPE_END];
            const Bone *src_end = mBones.begin() + mCounts[TYPE_END];
            while (true) {
                while (db->name != src->name) {
                    db++;
                    if (db >= db_end)
                        goto complain;
                    fdata++;
                }
                src++;
                *fdata = 0.0f;
                db->weight = 0.0f;
                if (src >= src_end)
                    return;
                db++;
                if (db >= db_end)
                    goto complain;
                fdata++;
            }
        }
    } else {
        if (mCounts[TYPE_QUAT] > mCounts[TYPE_POS]) {
            Bone *db_begin = dst.mBones.begin();
            Vector3 *data = (Vector3 *)dst.mStart;
            Bone *db = db_begin + dst.mCounts[TYPE_POS];
            Bone *db_end = db_begin + dst.mCounts[TYPE_QUAT];
            const Bone *src_end = src + mCounts[TYPE_QUAT];
            while (true) {
                while (db->name != src->name) {
                    db++;
                    if (db >= db_end)
                        goto complain;
                    data++;
                }
                src++;
                data->x *= f;
                data->y *= f;
                data->z *= f;
                if (src >= src_end)
                    goto scale_quat;
                db++;
                if (db >= db_end)
                    goto complain;
                data++;
            }
        }
    scale_quat:
        if (mCounts[TYPE_ROTX] > mCounts[TYPE_QUAT]) {
            Bone *db_begin = dst.mBones.begin();
            Hmx::Quat *qdata = (Hmx::Quat *)(dst.mStart + dst.mOffsets[TYPE_QUAT]);
            Bone *db = db_begin + dst.mCounts[TYPE_QUAT];
            Bone *db_end = db_begin + dst.mCounts[TYPE_ROTX];
            const Bone *src_end = mBones.begin() + mCounts[TYPE_ROTX];
            while (true) {
                while (db->name != src->name) {
                    db++;
                    if (db >= db_end)
                        goto complain;
                    qdata++;
                }
                src++;
                qdata->x *= f;
                qdata->y *= f;
                qdata->z *= f;
                qdata->w *= f;
                if (src >= src_end)
                    goto scale_rot;
                db++;
                if (db >= db_end)
                    goto complain;
                qdata++;
            }
        }
    scale_rot:
        if (mCounts[TYPE_END] > mCounts[TYPE_ROTX]) {
            Bone *db_begin = dst.mBones.begin();
            float *fdata = (float *)(dst.mStart + dst.mOffsets[TYPE_ROTX]);
            Bone *db = db_begin + dst.mCounts[TYPE_ROTX];
            Bone *db_end = db_begin + dst.mCounts[TYPE_END];
            const Bone *src_end = mBones.begin() + mCounts[TYPE_END];
            while (true) {
                while (db->name != src->name) {
                    db++;
                    if (db >= db_end)
                        goto complain;
                    fdata++;
                }
                src++;
                *fdata *= f;
                if (src >= src_end)
                    return;
                db++;
                if (db >= db_end)
                    goto complain;
                fdata++;
            }
        }
    }
    return;

complain:
    TestDstComplain(src->name);
}

void CharBones::ScaleAdd(CharBones &dst, float f) const {
    const Bone *src = mBones.begin();
    if (src == mBones.end())
        return;

    if (mCounts[TYPE_QUAT] > mCounts[TYPE_POS]) {
        Vector3 *ddata = (Vector3 *)dst.mStart;
        Bone *db_end = dst.mBones.begin() + dst.mCounts[TYPE_QUAT];
        Bone *db = dst.mBones.begin() + dst.mCounts[TYPE_POS];
        const Bone *src_end = src + mCounts[TYPE_QUAT];
        if (mCompression >= kCompressVects) {
            short *sdata = (short *)mStart;
            while (true) {
                short sz = sdata[2];
                short sy = sdata[1];
                while (db->name != src->name) {
                    db++;
                    if (db >= db_end)
                        goto complain;
                    ddata++;
                }
                ddata->x += (float)sdata[0] * 0.039674062f * f;
                ddata->z += (float)sz * 0.039674062f * f;
                ddata->y += (float)sy * 0.039674062f * f;
                db->weight += src->weight * f;
                src++;
                if (src >= src_end)
                    goto add_quat;
                db++;
                if (db >= db_end)
                    goto complain;
                ddata++;
                sdata += 3;
            }
        } else {
            Vector3 *sdata = (Vector3 *)mStart;
            while (true) {
                while (db->name != src->name) {
                    db++;
                    if (db >= db_end)
                        goto complain;
                    ddata++;
                }
                ddata->x += sdata->x * f;
                ddata->y += sdata->y * f;
                ddata->z += sdata->z * f;
                db->weight += src->weight * f;
                src++;
                if (src >= src_end)
                    goto add_quat;
                db++;
                if (db >= db_end)
                    goto complain;
                ddata++;
                sdata++;
            }
        }
    }
add_quat:
    if (mCounts[TYPE_ROTX] > mCounts[TYPE_QUAT]) {
        Bone *db_end = dst.mBones.begin() + dst.mCounts[TYPE_ROTX];
        const Bone *src_end = mBones.begin() + mCounts[TYPE_ROTX];
        Bone *db = dst.mBones.begin() + dst.mCounts[TYPE_QUAT];
        Hmx::Quat *dquat = (Hmx::Quat *)(dst.mStart + dst.mOffsets[TYPE_QUAT]);
        float abs_f = fabs(f);
        if (mCompression >= kCompressQuats) {
            char *sdata = (char *)(mStart + mOffsets[TYPE_QUAT]);
            float scale = abs_f * 0.0078740157f;
            while (true) {
                while (db->name != src->name) {
                    db++;
                    if (db >= db_end)
                        goto complain;
                    dquat++;
                }
                float dy = dquat->y;
                float dx = dquat->x;
                float dz = dquat->z;
                float dw = dquat->w;
                float sy = (float)sdata[1] * scale;
                float sx = (float)sdata[0] * scale;
                float sz = (float)sdata[2] * scale;
                float sw = (float)sdata[3] * (f * 0.0078740157f);
                if (dw * sw + dz * sz + dx * sx + dy * sy < 0.0f) {
                    dquat->y = dy - sy;
                    dquat->z = dz - sz;
                    dquat->x = dx - sx;
                    dquat->w = dw - sw;
                } else {
                    dquat->y = dy + sy;
                    dquat->z = dz + sz;
                    dquat->x = dx + sx;
                    dquat->w = dw + sw;
                }
                db->weight += src->weight * f;
                src++;
                if (src >= src_end)
                    goto add_rot;
                db++;
                if (db >= db_end)
                    goto complain;
                dquat++;
                sdata += 4;
            }
        } else if (mCompression != kCompressNone) {
            short *sdata = (short *)(mStart + mOffsets[TYPE_QUAT]);
            float scale = abs_f * 3.051851e-05f;
            while (true) {
                while (db->name != src->name) {
                    db++;
                    if (db >= db_end)
                        goto complain;
                    dquat++;
                }
                float dz = dquat->z;
                float dy = dquat->y;
                float dw = dquat->w;
                float dx = dquat->x;
                float sx = (float)sdata[0] * scale;
                float sz = (float)sdata[2] * scale;
                float sy = (float)sdata[1] * scale;
                float sw = (float)sdata[3] * (f * 3.051851e-05f);
                if (dx * sx + dy * sy + dz * sz + dw * sw < 0.0f) {
                    dquat->z = dz - sz;
                    dquat->x = dx - sx;
                    dquat->y = dy - sy;
                    dquat->w = dw - sw;
                } else {
                    dquat->z = sz + dz;
                    dquat->x = dx + sx;
                    dquat->y = sy + dy;
                    dquat->w = sw + dw;
                }
                db->weight += src->weight * f;
                src++;
                if (src >= src_end)
                    goto add_rot;
                db++;
                if (db >= db_end)
                    goto complain;
                dquat++;
                sdata += 4;
            }
        } else {
            Hmx::Quat *squat = (Hmx::Quat *)(mStart + mOffsets[TYPE_QUAT]);
            while (true) {
                while (db->name != src->name) {
                    db++;
                    if (db >= db_end)
                        goto complain;
                    dquat++;
                }
                float sy = squat->y * abs_f;
                float dy = dquat->y;
                float sx = squat->x * abs_f;
                float dx = dquat->x;
                float sz = squat->z * abs_f;
                float dz = dquat->z;
                float sw = squat->w * f;
                float dw = dquat->w;
                if (sx * dx + sy * dy + sz * dz + sw * dw < 0.0f) {
                    dquat->y = dy - sy;
                    dquat->z = dz - sz;
                    dquat->x = dx - sx;
                    dquat->w = dw - sw;
                } else {
                    dquat->y = sy + dy;
                    dquat->z = sz + dz;
                    dquat->x = sx + dx;
                    dquat->w = sw + dw;
                }
                db->weight += src->weight * f;
                src++;
                if (src >= src_end)
                    goto add_rot;
                db++;
                if (db >= db_end)
                    goto complain;
                dquat++;
                squat++;
            }
        }
    }
add_rot:
    if (mCounts[TYPE_END] > mCounts[TYPE_ROTX]) {
        Bone *db_end = dst.mBones.begin() + dst.mCounts[TYPE_END];
        float *dfdata = (float *)(dst.mStart + dst.mOffsets[TYPE_ROTX]);
        Bone *db = dst.mBones.begin() + dst.mCounts[TYPE_ROTX];
        const Bone *src_end = mBones.begin() + mCounts[TYPE_END];
        float *sfdata = (float *)(mStart + mOffsets[TYPE_ROTX]);
        if (mCompression != kCompressNone) {
            while (true) {
                while (db->name != src->name) {
                    db++;
                    if (db >= db_end)
                        goto complain;
                    dfdata++;
                }
                *dfdata += (float)*(short *)sfdata * (f * 0.0006103515625f);
                db->weight += src->weight * f;
                src++;
                if (src >= src_end)
                    return;
                db++;
                if (db >= db_end)
                    goto complain;
                dfdata++;
                sfdata = (float *)((char *)sfdata + 2);
            }
        } else {
            while (true) {
                while (db->name != src->name) {
                    db++;
                    if (db >= db_end)
                        goto complain;
                    dfdata++;
                }
                *dfdata += *sfdata * f;
                db->weight += src->weight * f;
                src++;
                if (src >= src_end)
                    return;
                db++;
                if (db >= db_end)
                    goto complain;
                dfdata++;
                sfdata++;
            }
        }
    }
    return;

complain:
    TestDstComplain(src->name);
}

void CharBones::Blend(CharBones &bones) const {
    MILO_ASSERT(!mCompression && !bones.mCompression, 0x311);
    if (mBones.empty())
        return;
    const Bone *src = mBones.begin();

    if (mCounts[TYPE_QUAT] > mCounts[TYPE_POS]) {
        Vector3 *sdata = (Vector3 *)mStart;
        Vector3 *ddata = (Vector3 *)bones.mStart;
        Bone *db = bones.mBones.begin() + bones.mCounts[TYPE_POS];
        Bone *db_end = bones.mBones.begin() + bones.mCounts[TYPE_QUAT];
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
            if (src >= src_end)
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
        Bone *db = bones.mBones.begin() + bones.mCounts[TYPE_QUAT];
        Bone *db_end = bones.mBones.begin() + bones.mCounts[TYPE_ROTX];
        const Bone *src_end = mBones.begin() + mCounts[TYPE_ROTX];
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
            if (src >= src_end)
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
        Bone *db = bones.mBones.begin() + bones.mCounts[TYPE_ROTX];
        Bone *db_end = bones.mBones.begin() + bones.mCounts[TYPE_END];
        float *dfdata = (float *)(bones.mStart + bones.mOffsets[TYPE_ROTX]);
        float *sfdata = (float *)(mStart + mOffsets[TYPE_ROTX]);
        const Bone *src_end = mBones.begin() + mCounts[TYPE_END];
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
            if (src >= src_end)
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
    const Bone *src = mBones.begin();
    if (src == mBones.end())
        return;

    if (mCounts[TYPE_QUAT] > mCounts[TYPE_POS]) {
        Bone *db_end = dst.mBones.begin() + dst.mCounts[TYPE_QUAT];
        Vector3 *ddata = (Vector3 *)dst.mStart;
        Bone *db = dst.mBones.begin() + dst.mCounts[TYPE_POS];
        const Bone *src_end = src + mCounts[TYPE_QUAT];
        if (db != nullptr && mCompression >= kCompressVects) {
            short *sdata = (short *)mStart;
            while (true) {
                short sz = sdata[2];
                short sy = sdata[1];
                while (db->name != src->name) {
                    db++;
                    if (db >= db_end)
                        goto complain;
                    ddata++;
                }
                src++;
                ddata->x += (float)sdata[0] * 0.039674062f;
                ddata->y += (float)sy * 0.039674062f;
                ddata->z += (float)sz * 0.039674062f;
                if (src_end == src)
                    goto rotate_quat;
                db++;
                if (db >= db_end)
                    goto complain;
                ddata++;
                sdata += 3;
            }
        } else {
            Vector3 *sdata = (Vector3 *)mStart;
            while (true) {
                while (db->name != src->name) {
                    db++;
                    if (db >= db_end)
                        goto complain;
                    ddata++;
                }
                src++;
                ddata->x += sdata->x;
                ddata->y += sdata->y;
                ddata->z += sdata->z;
                if (src >= src_end)
                    goto rotate_quat;
                db++;
                if (db >= db_end)
                    goto complain;
                ddata++;
                sdata++;
            }
        }
    }
rotate_quat:
    if (mCounts[TYPE_ROTX] > mCounts[TYPE_QUAT]) {
        Bone *db_end = dst.mBones.begin() + dst.mCounts[TYPE_ROTX];
        Bone *db = dst.mBones.begin() + dst.mCounts[TYPE_QUAT];
        Hmx::Quat *dquat = (Hmx::Quat *)(dst.mStart + dst.mOffsets[TYPE_QUAT]);
        int src_quat_off = mOffsets[TYPE_QUAT];
        const Bone *src_end = mBones.begin() + mCounts[TYPE_ROTX];
        if (mCompression >= kCompressQuats) {
            char *sqdata = (char *)(src_quat_off + mStart);
            while (true) {
                while (db->name != src->name) {
                    db++;
                    if (db >= db_end)
                        goto complain;
                    dquat++;
                }
                Hmx::Quat sq;
                ((ByteQuat *)sqdata)->ToQuat(sq);
                float dw = dquat->w;
                float dx = dquat->x;
                src++;
                float dz = dquat->z;
                float dy = dquat->y;
                dquat->w = -(-(dy * sq.y - (dw * sq.w - dx * sq.x)) - dz * sq.z);
                dquat->z = -(dx * sq.y - ((dy * sq.x + (dz * sq.w + dw * sq.z))));
                dquat->y = -(dz * sq.x - (dw * sq.y + dy * sq.w + dx * sq.z));
                dquat->x = -(dy * sq.z - (dw * sq.x + dz * sq.y + dx * sq.w));
                if (src >= src_end)
                    goto rotate_rot;
                db++;
                if (db >= db_end)
                    goto complain;
                dquat++;
                sqdata += 4;
            }
        } else if (mCompression != kCompressNone) {
            char *sqdata = (char *)(src_quat_off + mStart);
            while (true) {
                while (db->name != src->name) {
                    db++;
                    if (db >= db_end)
                        goto complain;
                    dquat++;
                }
                Hmx::Quat sq;
                ((ShortQuat *)sqdata)->ToQuat(sq);
                float dw = dquat->w;
                float dx = dquat->x;
                src++;
                float dz = dquat->z;
                float dy = dquat->y;
                dquat->w = -(-(dy * sq.y - (dw * sq.w - dx * sq.x)) - dz * sq.z);
                dquat->z = -(dx * sq.y - (dy * sq.x + dz * sq.w + dw * sq.z));
                dquat->y = -(dz * sq.x - (dw * sq.y + dy * sq.w + dx * sq.z));
                dquat->x = -(dy * sq.z - (dw * sq.x + dz * sq.y + dx * sq.w));
                if (src >= src_end)
                    goto rotate_rot;
                db++;
                if (db >= db_end)
                    goto complain;
                dquat++;
                sqdata += 8;
            }
        } else {
            Hmx::Quat *squat = (Hmx::Quat *)(src_quat_off + mStart);
            while (true) {
                while (db->name != src->name) {
                    db++;
                    if (db >= db_end)
                        goto complain;
                    dquat++;
                }
                float sy = squat->y;
                src++;
                float sz = squat->z;
                float dw = dquat->w;
                float sx = squat->x;
                float dx = dquat->x;
                float sw = squat->w;
                float dz = dquat->z;
                float dy = dquat->y;
                dquat->y = -(sx * dz - (dy * sw + dx * sz + sy * dw));
                dquat->z = (sy * dx - (dy * sx + sw * dz + dw * sz));
                dquat->z = -dquat->z;
                dquat->w = -(dz * sz - -(dy * sy - (dw * sw - sx * dx)));
                dquat->x = -(dy * sz - (sy * dz + sx * dw + dx * sw));
                if (src >= src_end)
                    goto rotate_rot;
                db++;
                if (db >= db_end)
                    goto complain;
                dquat++;
                squat++;
            }
        }
    }
rotate_rot:
    if (mCounts[TYPE_END] > mCounts[TYPE_ROTX]) {
        Bone *db_end = dst.mBones.begin() + dst.mCounts[TYPE_END];
        const Bone *src_end = mBones.begin() + mCounts[TYPE_END];
        Bone *db = dst.mBones.begin() + dst.mCounts[TYPE_ROTX];
        float *dfdata = (float *)(dst.mStart + dst.mOffsets[TYPE_ROTX]);
        float *sfdata = (float *)(mStart + mOffsets[TYPE_ROTX]);
        if (mCompression != kCompressNone) {
            while (true) {
                while (db->name != src->name) {
                    db++;
                    if (db >= db_end)
                        goto complain;
                    dfdata++;
                }
                src++;
                *dfdata += (float)(long long)*(short *)sfdata * 0.00061035156f;
                if (src >= src_end)
                    return;
                db++;
                if (db >= db_end)
                    goto complain;
                dfdata++;
                sfdata = (float *)((char *)sfdata + 2);
            }
        } else {
            while (true) {
                while (db->name != src->name) {
                    db++;
                    if (db >= db_end)
                        goto complain;
                    dfdata++;
                }
                src++;
                *dfdata += *sfdata;
                if (src >= src_end)
                    return;
                db++;
                if (db >= db_end)
                    goto complain;
                dfdata++;
                sfdata++;
            }
        }
    }
    return;

complain:
    TestDstComplain(src->name);
}

void CharBones::RotateTo(CharBones &dst, float f) const {
    const Bone *src = mBones.begin();
    if (src >= mBones.end())
        return;

    if (mCounts[TYPE_QUAT] > mCounts[TYPE_POS]) {
        const Bone *src_end = src + mCounts[TYPE_QUAT];
        Vector3 *ddata = (Vector3 *)dst.mStart;
        Bone *db_end = dst.mBones.begin() + dst.mCounts[TYPE_QUAT];
        Bone *db = dst.mBones.begin() + dst.mCounts[TYPE_POS];
        if (db != nullptr && mCompression >= kCompressVects) {
            short *sdata = (short *)mStart;
            while (true) {
                short sz = sdata[2];
                short sy = sdata[1];
                while (db->name != src->name) {
                    db++;
                    if (db >= db_end)
                        goto complain;
                    ddata++;
                }
                src++;
                ddata->x += (float)sdata[0] * 0.039674062f * f;
                ddata->y += (float)sy * 0.039674062f * f;
                ddata->z += (float)sz * 0.039674062f * f;
                if (src >= src_end)
                    goto rotateto_quat;
                db++;
                if (db >= db_end)
                    goto complain;
                ddata++;
                sdata += 3;
            }
        } else {
            Vector3 *sdata = (Vector3 *)mStart;
            while (true) {
                while (db->name != src->name) {
                    db++;
                    if (db >= db_end)
                        goto complain;
                    ddata++;
                }
                src++;
                ddata->x += sdata->x * f;
                ddata->y += sdata->y * f;
                ddata->z += sdata->z * f;
                if (src >= src_end)
                    goto rotateto_quat;
                db++;
                if (db >= db_end)
                    goto complain;
                ddata++;
                sdata++;
            }
        }
    }
rotateto_quat:
    if (mCounts[TYPE_ROTX] > mCounts[TYPE_QUAT]) {
        const Bone *src_end = mBones.begin() + mCounts[TYPE_ROTX];
        Bone *db = dst.mBones.begin() + dst.mCounts[TYPE_QUAT];
        Bone *db_end = dst.mBones.begin() + dst.mCounts[TYPE_ROTX];
        Hmx::Quat *dquat = (Hmx::Quat *)(dst.mStart + dst.mOffsets[TYPE_QUAT]);
        int src_quat_off = mOffsets[TYPE_QUAT];
        float abs_f = fabs(f);
        if (mCompression >= kCompressQuats) {
            char *sqdata = (char *)(src_quat_off + mStart);
            float scale = abs_f * 0.0078740157f;
            while (true) {
                while (db->name != src->name) {
                    db++;
                    if (db >= db_end)
                        goto complain;
                    dquat++;
                }
                Hmx::Quat sq;
                ((ByteQuat *)sqdata)->ToQuat(sq);
                float dw = dquat->w;
                float dx = dquat->x;
                src++;
                float dz = dquat->z;
                float dy = dquat->y;
                float sw = sq.w * f;
                float sx = sq.x * scale;
                float sy = sq.y * scale;
                float sz = sq.z * scale;
                if (sx * dx + sy * dy + sz * dz + sw * dw < 0.0f) {
                    dquat->w = -(-(dy * sy - (dw * sw - dx * sx)) - dz * sz);
                    dquat->z = -(dx * sy - ((dy * sx + (dz * sw + dw * sz))));
                    dquat->y = -(dz * sx - (dw * sy + dy * sw + dx * sz));
                    dquat->x = -(dy * sz - (dw * sx + dz * sy + dx * sw));
                } else {
                    dquat->w = -(-(dy * sy - (dw * sw - dx * sx)) - dz * sz);
                    dquat->z = -(dx * sy - ((dy * sx + (dz * sw + dw * sz))));
                    dquat->y = -(dz * sx - (dw * sy + dy * sw + dx * sz));
                    dquat->x = -(dy * sz - (dw * sx + dz * sy + dx * sw));
                }
                if (src >= src_end)
                    goto rotateto_rot;
                db++;
                if (db >= db_end)
                    goto complain;
                dquat++;
                sqdata += 4;
            }
        } else if (mCompression != kCompressNone) {
            char *sqdata = (char *)(src_quat_off + mStart);
            float scale = abs_f * 3.051851e-05f;
            while (true) {
                while (db->name != src->name) {
                    db++;
                    if (db >= db_end)
                        goto complain;
                    dquat++;
                }
                Hmx::Quat sq;
                ((ShortQuat *)sqdata)->ToQuat(sq);
                float dw = dquat->w;
                float dx = dquat->x;
                src++;
                float dz = dquat->z;
                float dy = dquat->y;
                float sw = sq.w * f;
                float sx = sq.x * scale;
                float sy = sq.y * scale;
                float sz = sq.z * scale;
                if (sx * dx + sy * dy + sz * dz + sw * dw < 0.0f) {
                    dquat->w = -(-(dy * sy - (dw * sw - dx * sx)) - dz * sz);
                    dquat->z = -(dx * sy - (dy * sx + dz * sw + dw * sz));
                    dquat->y = -(dz * sx - (dw * sy + dy * sw + dx * sz));
                    dquat->x = -(dy * sz - (dw * sx + dz * sy + dx * sw));
                } else {
                    dquat->w = -(-(dy * sy - (dw * sw - dx * sx)) - dz * sz);
                    dquat->z = -(dx * sy - (dy * sx + dz * sw + dw * sz));
                    dquat->y = -(dz * sx - (dw * sy + dy * sw + dx * sz));
                    dquat->x = -(dy * sz - (dw * sx + dz * sy + dx * sw));
                }
                if (src >= src_end)
                    goto rotateto_rot;
                db++;
                if (db >= db_end)
                    goto complain;
                dquat++;
                sqdata += 8;
            }
        } else {
            Hmx::Quat *squat = (Hmx::Quat *)(src_quat_off + mStart);
            while (true) {
                while (db->name != src->name) {
                    db++;
                    if (db >= db_end)
                        goto complain;
                    dquat++;
                }
                float sy = squat->y;
                src++;
                float sz = squat->z;
                float dw = dquat->w;
                float sx = squat->x;
                float dx = dquat->x;
                float sw = squat->w;
                float dz = dquat->z;
                float dy = dquat->y;
                float scaled_sw = sw * f;
                float abs_sx = sx * abs_f;
                float abs_sy = sy * abs_f;
                float abs_sz = sz * abs_f;
                if (abs_sx * dx + abs_sy * dy + abs_sz * dz + scaled_sw * dw < 0.0f) {
                    dquat->y =
                        -(abs_sx * dz - (dy * scaled_sw + dx * abs_sz + abs_sy * dw));
                    dquat->z =
                        (abs_sy * dx - (dy * abs_sx + scaled_sw * dz + dw * abs_sz));
                    dquat->z = -dquat->z;
                    dquat->w = -(dz * abs_sz
                                 - -(dy * abs_sy - (dw * scaled_sw - abs_sx * dx)));
                    dquat->x =
                        -(dy * abs_sz - (abs_sy * dz + abs_sx * dw + dx * scaled_sw));
                } else {
                    dquat->y =
                        -(abs_sx * dz - (dy * scaled_sw + dx * abs_sz + abs_sy * dw));
                    dquat->z =
                        (abs_sy * dx - (dy * abs_sx + scaled_sw * dz + dw * abs_sz));
                    dquat->z = -dquat->z;
                    dquat->w = -(dz * abs_sz
                                 - -(dy * abs_sy - (dw * scaled_sw - abs_sx * dx)));
                    dquat->x =
                        -(dy * abs_sz - (abs_sy * dz + abs_sx * dw + dx * scaled_sw));
                }
                if (src >= src_end)
                    goto rotateto_rot;
                db++;
                if (db >= db_end)
                    goto complain;
                dquat++;
                squat++;
            }
        }
    }
rotateto_rot:
    if (mCounts[TYPE_END] > mCounts[TYPE_ROTX]) {
        const Bone *src_end = mBones.begin() + mCounts[TYPE_END];
        Bone *db = dst.mBones.begin() + dst.mCounts[TYPE_ROTX];
        Bone *db_end = dst.mBones.begin() + dst.mCounts[TYPE_END];
        float *dfdata = (float *)(dst.mStart + dst.mOffsets[TYPE_ROTX]);
        float *sfdata = (float *)(mStart + mOffsets[TYPE_ROTX]);
        if (mCompression != kCompressNone) {
            while (true) {
                while (db->name != src->name) {
                    db++;
                    if (db >= db_end)
                        goto complain;
                    dfdata++;
                }
                src++;
                *dfdata += (float)*(short *)sfdata * (f * 0.0006103515625f);
                if (src >= src_end)
                    return;
                db++;
                if (db >= db_end)
                    goto complain;
                dfdata++;
                sfdata = (float *)((char *)sfdata + 2);
            }
        } else {
            while (true) {
                while (db->name != src->name) {
                    db++;
                    if (db >= db_end)
                        goto complain;
                    dfdata++;
                }
                src++;
                *dfdata += *sfdata * f;
                if (src >= src_end)
                    return;
                db++;
                if (db >= db_end)
                    goto complain;
                dfdata++;
                sfdata++;
            }
        }
    }
    return;

complain:
    TestDstComplain(src->name);
}

void CharBones::ScaleAddIdentity() {
    Hmx::Quat *qend = (Hmx::Quat *)(mStart + mOffsets[TYPE_ROTX]);
    Bone *bone = mBones.begin() + mCounts[TYPE_QUAT];
    Hmx::Quat *qstart = (Hmx::Quat *)(mStart + mOffsets[TYPE_QUAT]);
    while (qstart != qend) {
        float identity = 1.0f - bone->weight;
        float w = qstart->w;
        if (w < 0.0f) {
            w -= identity;
        } else {
            w += identity;
        }
        qstart->w = w;
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
    if (t < 2) {
        if (mCompression >= 2) {
            Vector3 vshort((short *)ptr);
            return MakeString("%g %g %g", vshort.x, vshort.y, vshort.z);
        } else {
            Vector3 *vptr = (Vector3 *)ptr;
            return MakeString("%g %g %g", vptr->x, vptr->y, vptr->z);
        }
    } else if (t == 2) {
        Hmx::Quat q;
        Hmx::Quat *qPtr = (Hmx::Quat *)ptr;
        if (mCompression >= 3) {
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
    } else {
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
