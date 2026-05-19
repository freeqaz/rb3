#include "bandobj/BandFaceDeform.h"
#include "decomp.h"
#include "utl/Symbols.h"

INIT_REVS(BandFaceDeform);

BandFaceDeform::DeltaArray::DeltaArray() : mSize(0), mData(0) {}
BandFaceDeform::DeltaArray::DeltaArray(const BandFaceDeform::DeltaArray &da)
    : mSize(0), mData(0) {
    *this = da;
}

BandFaceDeform::DeltaArray &
BandFaceDeform::DeltaArray::operator=(const BandFaceDeform::DeltaArray &da) {
    SetSize(da.mSize);
    memcpy(mData, da.mData, mSize);
    return *this;
}

BandFaceDeform::DeltaArray::~DeltaArray() { _MemFree(mData); }
void BandFaceDeform::DeltaArray::Clear() { SetSize(0); }

int BandFaceDeform::DeltaArray::NumVerts() {
    void *p = begin();
    int num = 0;
    void *itend = end();
    while (p < itend) {
        num += ((Delta *)p)->num;
        p = ((Delta *)p)->next();
    }
    return num;
}

DECOMP_FORCEACTIVE(BandFaceDeform, "")

extern void *
MemResizeElem(void *&, int &, void *, int, int, const char *);

void BandFaceDeform::DeltaArray::AppendDeltas(
    const std::vector<Vector3> &pos, const std::vector<Vector3> &base
) {
    if (pos.size() != base.size()) {
        MILO_FAIL(
            "AppendDeltas pos has %d points, base has %d", pos.size(), base.size()
        );
    }

    static int total;
    static int totalRuns;
    static int totalLength;
    static float maxDelta;

    int *const pTotalRuns = &totalRuns;
    int *const pTotalLength = &totalLength;

    float minClamp = -2.0f;
    float maxClamp = 2.0f;

    int start = 0;
    int end = 0;

    while ((unsigned short)end < pos.size()) {
        // Skip leading zero-delta vertices
        int byteOff = start * 0xC;
        while ((unsigned short)start < pos.size()) {
            const Vector3 &pv = pos[start];
            const Vector3 &bv = base[start];
            float deltax = pv.x - bv.x;
            float deltaz = pv.z - bv.z;
            float deltay = pv.y - bv.y;

            float dx = (float)deltax;
            if (dx > maxClamp)
                dx = maxClamp;
            else if (dx < minClamp)
                dx = minClamp;
            signed char qx = (int)(63.5 * (double)dx + 0.5);

            float dy = deltay;
            if (dy > maxClamp)
                dy = maxClamp;
            else if (dy < minClamp)
                dy = minClamp;
            signed char qy = (int)(63.5 * (double)dy + 0.5);

            float dz = deltaz;
            if (dz > maxClamp)
                dz = maxClamp;
            else if (dz < minClamp)
                dz = minClamp;
            signed char qz = (int)(63.5 * (double)dz + 0.5);

            int nonZero = 0;
            if ((signed char)qx != 0 || (signed char)qy != 0 || (signed char)qz != 0) {
                nonZero = 1;
            }
            if (nonZero == 0) {
                start++;
                byteOff += 0xC;
                continue;
            }
            break;
        }

        // Extend run while deltas are non-zero
        end = start + 1;
        int byteOff2 = end * 0xC;
        while ((unsigned short)end < pos.size()) {
            const Vector3 &pv = pos[end];
            const Vector3 &bv = base[end];
            float deltax = pv.x - bv.x;
            float deltaz = pv.z - bv.z;
            float deltay = pv.y - bv.y;

            float dx = (float)deltax;
            if (dx > maxClamp)
                dx = maxClamp;
            else if (dx < minClamp)
                dx = minClamp;
            signed char qx = (int)(63.5 * (double)dx + 0.5);

            float dy = deltay;
            if (dy > maxClamp)
                dy = maxClamp;
            else if (dy < minClamp)
                dy = minClamp;
            signed char qy = (int)(63.5 * (double)dy + 0.5);

            float dz = deltaz;
            if (dz > maxClamp)
                dz = maxClamp;
            else if (dz < minClamp)
                dz = minClamp;
            signed char qz = (int)(63.5 * (double)dz + 0.5);

            int nonZero = 0;
            if ((signed char)qx != 0 || (signed char)qy != 0 || (signed char)qz != 0) {
                nonZero = 1;
            }
            if (nonZero != 0) {
                end++;
                byteOff2 += 0xC;
                continue;
            }
            break;
        }

        if (start < pos.size()) {
            int count = end - start;
            char *rec = (char *)MemResizeElem(
                mData, mSize, (char *)mData + mSize, 0, count * 3 + 4, "BandFaceDeform"
            );

            *(unsigned short *)(rec + 0) = start;
            int vi = start;
            float md = maxDelta;
            *(unsigned short *)(rec + 2) = count;
            int off = start * 0xC;
            if ((int)start < (int)end) {
                int ctr = count;
                do {
                    const Vector3 &pv = pos[vi];
                    const Vector3 &bv = base[vi];
                    int recOff = (vi - start) * 3;
                    float deltax = pv.x - bv.x;
                    float deltaz = pv.z - bv.z;
                    float deltay = pv.y - bv.y;

                    float dx = (float)deltax;
                    if (dx > maxClamp)
                        dx = maxClamp;
                    else if (dx < minClamp)
                        dx = minClamp;
                    rec[recOff + 4] = (signed char)(int)(63.5 * (double)dx + 0.5);

                    float dy = deltay;
                    if (dy > maxClamp)
                        dy = maxClamp;
                    else if (dy < minClamp)
                        dy = minClamp;
                    rec[recOff + 5] = (signed char)(int)(63.5 * (double)dy + 0.5);

                    float dz = deltaz;
                    if (dz > maxClamp)
                        dz = maxClamp;
                    else if (dz < minClamp)
                        dz = minClamp;
                    rec[recOff + 6] = (signed char)(int)(63.5 * (double)dz + 0.5);

                    // Track max delta per component
                    const Vector3 &pv2 = pos[vi];
                    const Vector3 &bv2 = base[vi];
                    float ddx = pv2.x - bv2.x;
                    float ddz = pv2.z - bv2.z;
                    float ddy = pv2.y - bv2.y;
                    float absx = std::fabs(ddx);
                    if (md < absx) {
                        md = absx;
                        maxDelta = absx;
                    }
                    float absy = std::fabs(ddy);
                    if (md < absy) {
                        md = absy;
                        maxDelta = absy;
                    }
                    float absz = std::fabs(ddz);
                    if (md < absz) {
                        maxDelta = absz;
                    }

                    off += 0xC;
                    vi++;
                } while (--ctr);
            }

            unsigned short recCount = *(unsigned short *)(rec + 2);
            TheDebug << MakeString(
                "   run from %d to %d waste %g \n",
                (int)start,
                (int)end,
                4.0f / (float)(recCount * 3 + 4)
            );

            (*pTotalRuns)++;
            *pTotalLength += count;
        }

        start = end;
    }

    int sz = mSize;
    total += sz;
    TheDebug << MakeString(
        "   is size %d total %d av runlength %g totalWaste %d md %g\n",
        sz,
        total,
        (float)*pTotalLength / (float)*pTotalRuns,
        *pTotalRuns * 4,
        maxDelta
    );
}

void BandFaceDeform::DeltaArray::SetSize(int i) {
    if (mSize != i) {
        mSize = i;
        _MemFree(mData);
        mData = _MemAlloc(mSize, 0);
    }
}

BandFaceDeform::BandFaceDeform() {}

BandFaceDeform::~BandFaceDeform() {}

void BandFaceDeform::SetFromMeshAnim(RndMeshAnim *a1, RndMeshAnim *a2, int i1, int i2) {
    if (i2 == -1) {
        i2 = a1->VertPointsKeys().size();
    }
    mFrames.resize(i2);
    for (int i = 0; i < i2; i++) {
        mFrames[i].Clear();
        mFrames[i].AppendDeltas(
            a1->VertPointsKeys()[i + i1].value, a2->VertPointsKeys()[0].value
        );
    }
}

int BandFaceDeform::TotalSize() {
    int size = 0;
    for (int i = 0; i < mFrames.size(); i++) {
        size += mFrames[i].mSize;
    }
    return size;
}

BEGIN_COPYS(BandFaceDeform)
    COPY_SUPERCLASS(Hmx::Object)
    CREATE_COPY(BandFaceDeform)
    BEGIN_COPYING_MEMBERS
        COPY_MEMBER(mFrames)
    END_COPYING_MEMBERS
END_COPYS

BinStream &operator>>(BinStream &bs, BandFaceDeform::DeltaArray &da) {
    da.Load(bs);
    return bs;
}

void BandFaceDeform::DeltaArray::Load(BinStream &bs) {
    int size;
    bs >> size;
    SetSize(size);
    Delta *d = (Delta *)mData;
    short *sptr = (short *)mData;
    while (size > 0) {
        bs >> (short &)d->unk0;
        bs >> d->num;
        bs.Read(d + 1, d->thisoffset() - 4);
        size -= d->thisoffset();
        d = (Delta *)d->next();
    }
}

SAVE_OBJ(BandFaceDeform, 0x129)

BEGIN_LOADS(BandFaceDeform)
    LOAD_REVS(bs)
    ASSERT_REVS(0, 0)
    LOAD_SUPERCLASS(Hmx::Object)
    bs >> mFrames;
END_LOADS

BEGIN_HANDLERS(BandFaceDeform)
    HANDLE_ACTION(
        set_from_meshanim,
        SetFromMeshAnim(_msg->Obj<RndMeshAnim>(2), _msg->Obj<RndMeshAnim>(3), 0, -1)
    )
    HANDLE_SUPERCLASS(Hmx::Object)
    HANDLE_CHECK(0x145)
END_HANDLERS

BEGIN_CUSTOM_PROPSYNC(BandFaceDeform::DeltaArray)
    SYNC_PROP_SET(verts, o.NumVerts(), )
END_CUSTOM_PROPSYNC

BEGIN_PROPSYNCS(BandFaceDeform)
    SYNC_PROP(frames, mFrames)
    SYNC_PROP_SET(size, TotalSize(), )
END_PROPSYNCS