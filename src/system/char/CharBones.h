#pragma once
#include "obj/Object.h"
#include "utl/BinStream.h"
#include "os/Debug.h"
#include "math/Vec.h"
#include <math.h>
#include <vector>

class CharClip;

#line 94
inline short MakeShortAng(float f) {
    f = 0.5f + f * 1638.4f;
    MILO_ASSERT(f < 32768 && f > -32767, 0x60);
    return (short)(float)floor(f);
}
#line 102

class ShortVector3 {
public:
    ShortVector3() {}
    ShortVector3(short *) {}

    static short ToShort(float f) {
        float scaled = f * (1.0f / 1300.0f) * 32767.0f + 0.5f;
        return (short)(float)floor(Clamp(-32767.0f, 32767.0f, scaled));
    }
    void Set(const Vector3 &vec) {
        x = ToShort(vec.x);
        y = ToShort(vec.y);
        z = ToShort(vec.z);
    }

    short x;
    short y;
    short z;
};

class CharBones {
public:
    enum Type {
        TYPE_POS = 0,
        TYPE_SCALE = 1,
        TYPE_QUAT = 2,
        TYPE_ROTX = 3,
        TYPE_ROTY = 4,
        TYPE_ROTZ = 5,
        TYPE_END = 6,
        NUM_TYPES = 7,
    };

    enum CompressionType {
        kCompressNone,
        kCompressRots,
        kCompressVects,
        kCompressQuats,
        kCompressAll
    };

    struct Bone {
        Bone() : name(), weight(1.0f) {}
        Bone(Symbol s, float w) : name(s), weight(w) {}

        /** "Bone to blend into" */
        Symbol name;
        /** "Weight to blend with" */
        float weight;
    };

    CharBones();
    virtual ~CharBones() {}
    virtual void ScaleAdd(CharClip *, float, float, float);
    virtual void Print();
    virtual void ReallocateInternal();

    void ClearBones();
    void Zero();
    void SetWeights(float);
    void AddBoneInternal(const Bone &);
    void AddBones(const std::vector<Bone> &);
    void AddBones(const std::list<Bone> &);
    void ListBones(std::list<Bone> &) const;
    int TypeSize(int) const;
    int FindOffset(Symbol) const;
    void *FindPtr(Symbol) const;
    void RecomputeSizes();
    void SetCompression(CompressionType);
    const char *StringVal(Symbol);
    void ScaleAddIdentity();
    void Blend(CharBones &) const;
    void RotateBy(CharBones &) const;
    void RotateTo(CharBones &, float) const;
    void ScaleAdd(CharBones &, float) const;
    void Enter() {
        Zero();
        SetWeights(0);
    }
    int TotalSize() const { return mTotalSize; }
    void ScaleDown(CharBones &, float) const;
    char *Start() const { return mStart; }
    char *ScaleOffset() const { return mStart + mScaleOffset; }
    char *QuatOffset() const { return mStart + mQuatOffset; }
    char *RotXOffset() const { return mStart + mRotXOffset; }
    char *RotYOffset() const { return mStart + mRotYOffset; }
    char *RotZOffset() const { return mStart + mRotZOffset; }
    char *EndOffset() const { return mStart + mEndOffset; }
#ifdef HX_NATIVE
    // Matched-fork declares a char* return but has no return statement; MWCC let
    // the assigned value linger in the return reg, but clang LP64 emits ud2 (SIGILL)
    // on the non-void fall-through. The return value is never used by callers
    // (SetStartFromRawData ignores it). Add the explicit return. (Mirrors the
    // ObjDirPtr::operator= missing-return HX_NATIVE fix.)
    char* SetStart(char* ptr){ mStart = ptr; return ptr; }
#else
    char* SetStart(char* ptr){ mStart = ptr; }
#endif

    static Type TypeOf(Symbol);
    static const char *SuffixOf(Type);
    static Symbol ChannelName(const char *, Type);
    static void SetWeights(float, std::vector<Bone> &);

    CompressionType mCompression; // 0x4
    std::vector<Bone> mBones; // 0x8
    char *mStart; // 0x10
    union {
        struct {
            int mPosCount; // 0x14
            int mScaleCount; // 0x18
            int mQuatCount; // 0x1c
            int mRotXCount; // 0x20
            int mRotYCount; // 0x24
            int mRotZCount; // 0x28
            int mEndCount; // 0x2c
        };
        int mCounts[7]; // 0x14 - 0x2c
    };
    struct {
        union {
            struct {
                int mPosOffset; // 0x30
                int mScaleOffset; // 0x34
                int mQuatOffset; // 0x38
                int mRotXOffset; // 0x3c
                int mRotYOffset; // 0x40
                int mRotZOffset; // 0x44
                int mEndOffset; // 0x48
            };
            int mOffsets[7]; // 0x30 - 0x48
        };
        int mTotalSize; // 0x4c
    };
};

/** "Holds state for a set of bones" */
class CharBonesObject : public CharBones, public virtual Hmx::Object {
public:
#ifdef HX_NATIVE
    // Clear any stale "freed" mark if this address is being reused by a new
    // CharBonesObject (see ~CharBonesObject / obj/Object.cpp HxNoteReusedAddr).
    CharBonesObject();
#else
    CharBonesObject() {}
#endif
#ifdef HX_NATIVE
    // Record this CharBonesObject's pointer (the representation an
    // ObjPtr<CharBonesObject> stores as mPtr) as freed, so a dangling
    // ObjPtr<CharBonesObject>::~ObjPtr (e.g. CharDriver::mBones during venue/char
    // teardown) skips its Release instead of reading the vbase offset out of this
    // freed object's vtable and SIGSEGVing. See obj/Object.cpp HxNoteFreedAddr.
    virtual ~CharBonesObject();
#else
    virtual ~CharBonesObject() {}
#endif
    OBJ_CLASSNAME(CharBonesObject);
    OBJ_SET_TYPE(CharBonesObject);
    virtual bool SyncProperty(DataNode &, DataArray *, int, PropOp);

    NEW_OBJ(CharBonesObject)
    static void Init() { REGISTER_OBJ_FACTORY(CharBonesObject) }
};

/** "Holds state for a set of bones, and allocates own space" */
class CharBonesAlloc : public CharBonesObject {
public:
    CharBonesAlloc() {}
    virtual ~CharBonesAlloc();
    virtual void ReallocateInternal();
};

extern CharBones *gPropBones;
bool PropSync(CharBones::Bone &, DataNode &, DataArray *, int, PropOp);
BinStream &operator>>(BinStream &bs, CharBones::Bone &bone);