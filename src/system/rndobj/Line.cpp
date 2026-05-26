#include "rndobj/Line.h"
#include "math/Color.h"
#include "math/Geo.h"
#include "math/Mtx.h"
#include "math/Vec.h"
#include "obj/Data.h"
#include "obj/ObjMacros.h"
#include "obj/Object.h"
#include "os/Debug.h"
#include "rndobj/Cam.h"
#include "rndobj/Draw.h"
#include "rndobj/Mesh.h"
#include "rndobj/Mat.h"
#include "rndobj/Trans.h"
#include "utl/Symbols.h"
#include "types.h"
#include <math.h>

// The target copies vector<RndLine::Point> with an 8x-unrolled word copy
// (lwz/stw for ALL 13 words of Point, not typed lfs/stfs). Confirmed by the
// debug ELF's DWARF decompilation. The generic stlport copy paths do
// `*dst = *src` (Point assignment), which our MWCC emits as per-field typed
// copy + no unroll. To match, route the copy through a trivial POD word-struct
// assignment in a count-based loop: MWCC word-copies the POD assignment and
// inlines+unrolls the loop. The sub-object grouping below (Vector3 = 3 words,
// Color32 = 1 word, then a 3-word + 6-word split of the 9 unk floats) is what
// reproduces the target's exact load/store pairing schedule.
namespace stlpmtx_std {

struct _RndLineW3 { unsigned int a, b, c; };
struct _RndLineW6 { unsigned int a, b, c, d, e, f; };
struct _RndLinePointWords {
    _RndLineW3 v;     // Vector3 v        -> words 0,1,2
    unsigned int col; // Hmx::Color32 c   -> word 3
    _RndLineW3 unkA;  // unk0,unk1,unk2   -> words 4,5,6
    _RndLineW6 unkB;  // unk3..unk8       -> words 7..12
};

// Used by _Vector_impl<Point>::operator= in-place copy path.
template <>
inline RndLine::Point*
__copy_ptrs<const RndLine::Point*, RndLine::Point*>(
    const RndLine::Point* __first, const RndLine::Point* __last,
    RndLine::Point* __result, const __false_type& /*IsOKToMemCpy*/
) {
    _RndLinePointWords* __d = (_RndLinePointWords*)__result;
    const _RndLinePointWords* __s = (const _RndLinePointWords*)__first;
    for (ptrdiff_t __n = __last - __first; __n > 0; --__n) {
        *__d = *__s;
        ++__s;
        ++__d;
    }
    return (RndLine::Point*)__d;
}

// Non-const source overload (erase's element shift in operator>>'s resize).
template <>
inline RndLine::Point*
__copy_ptrs<RndLine::Point*, RndLine::Point*>(
    RndLine::Point* __first, RndLine::Point* __last,
    RndLine::Point* __result, const __false_type& /*IsOKToMemCpy*/
) {
    _RndLinePointWords* __d = (_RndLinePointWords*)__result;
    const _RndLinePointWords* __s = (const _RndLinePointWords*)__first;
    for (ptrdiff_t __n = __last - __first; __n > 0; --__n) {
        *__d = *__s;
        ++__s;
        ++__d;
    }
    return (RndLine::Point*)__d;
}

// Used by _M_fill_insert_aux to shift existing elements backward to open a gap.
// (op= does NOT use this — its realloc path goes through __uninitialized_copy
// which stays generic/typed, matching the target.)
template <>
inline RndLine::Point*
__copy_backward_ptrs<RndLine::Point*, RndLine::Point*>(
    RndLine::Point* __first, RndLine::Point* __last,
    RndLine::Point* __result, const __false_type& /*TrivialAssignment*/
) {
    _RndLinePointWords* __d = (_RndLinePointWords*)__result;
    _RndLinePointWords* __l = (_RndLinePointWords*)__last;
    for (ptrdiff_t __n = __last - __first; __n > 0; --__n) {
        *--__d = *--__l;
    }
    return (RndLine::Point*)__d;
}

} // namespace stlpmtx_std

// Defined in Geo.cpp but not declared in Geo.h.
void Intersect(const Hmx::Ray &r1, const Hmx::Ray &r2, Vector2 &out);

// File-static helpers for offsetting a Vector3 by a Vector2 (x, z) pair.
static void Add(const Vector3 &v3, const Vector2 &v2, Vector3 &out) {
    float z = v3.z + v2.y;
    float x = v3.x + v2.x;
    out.y = v3.y;
    out.x = x;
    out.z = z;
}

static void Subtract(const Vector3 &v3, const Vector2 &v2, Vector3 &out) {
    float z = v3.z - v2.y;
    float x = v3.x - v2.x;
    out.y = v3.y;
    out.x = x;
    out.z = z;
}

static int LINE_REV = 4;

RndDrawable *RndLine::CollideShowing(const Segment &s, float &f, Plane &p) {
    return mMesh->Collide(s, f, p) ? this : nullptr;
}

int RndLine::CollidePlane(const Plane &p) { return mMesh->CollidePlane(p); }

void RndLine::DrawShowing() {
    if (mPoints.size() >= 2) {
        if (mLineUpdate) {
            RndCam *cam = RndCam::sCurrent;
            UpdateLine(cam->WorldXfm(), cam->NearPlane());
            mMesh->SetWorldXfm(cam->WorldXfm());
        }
        mMesh->DrawShowing();
    }
}

void RndLine::UpdateSphere() {
    Sphere s;
    MakeWorldSphere(s, true);
    Transform xfm;
    FastInvert(WorldXfm(), xfm);
    Multiply(s, xfm, s);
    SetSphere(s);
}

float RndLine::GetDistanceToPlane(const Plane &p, Vector3 &v3) {
    if (mPoints.empty())
        return 0;
    else {
        WorldXfm();
        float ret = 0;
        bool b1 = true;
        FOREACH (it, mPoints) {
            float dot = p.Dot(it->v);
            if (b1 || std::fabs(dot) < std::fabs(ret)) {
                ret = dot;
                b1 = false;
                v3 = it->v;
            }
        }
        return ret;
    }
}

bool RndLine::MakeWorldSphere(Sphere &s, bool b2) {
    if (b2) {
        s.Zero();
        FOREACH (it, mPoints) {
            s.GrowToContain(Sphere(it->v, mWidth));
        }
        return true;
    } else {
        if (mSphere.GetRadius()) {
            Multiply(mSphere, WorldXfm(), s);
            return true;
        } else
            return false;
    }
}

void RndLine::MapVerts(int i1, VertsMap &vmap) {
    if (mLineHasCaps) {
        if (mLinePairs) {
            if (i1 & 1) {
                vmap.t = 2;
            } else
                vmap.t = 1;
            vmap.v = &mMesh->Verts(i1 * 4);
        } else {
            if (i1 == 0) {
                vmap.t = 1;
                vmap.v = &mMesh->Verts(0);
            } else if (i1 != 0) {
                if (i1 + 1 == mPoints.size()) {
                    vmap.t = 2;
                    vmap.v = mMesh->Verts().back() - 3;
                } else {
                    vmap.t = 0;
                    vmap.v = &mMesh->Verts((i1 + 1) * 2);
                }
            } else {
                vmap.t = 1;
                vmap.v = &mMesh->Verts(0);
            }
        }
    } else {
        vmap.t = 0;
        vmap.v = &mMesh->Verts(i1 * 2);
    }
}

void RndLine::SetMat(RndMat *mat) {
    mMat = mat;
    mMesh->SetMat(mat);
}

void RndLine::SetNumPoints(int num) {
    mPoints.resize(num);
    if (num >= 1) {
        if (mLineHasCaps) {
            if (mLinePairs) {
                num = (num & 0x7ffffffeU) * 2;
            } else {
                num = num + 2;
            }
        }
        mMesh->Verts().resize(num * 2, true);
        for (int i = 0; i < mPoints.size(); i++) {
            VertsMap vmap;
            MapVerts(i, vmap);
            Hmx::Color32 &ptColor = mPoints[i].c;
            if (vmap.t == 1) {
                vmap.v->uv.Set(0, 1);
                vmap.v++->color = ptColor;
                vmap.v->uv.Set(0, 0);
                vmap.v++->color = ptColor;
            }
            vmap.v->uv.Set(0.5f, 1.0f);
            vmap.v++->color = ptColor;
            vmap.v->uv.Set(0.5f, 0.0f);
            vmap.v++->color = ptColor;
            if (vmap.t == 2) {
                vmap.v->uv.Set(1.0f, 1.0f);
                vmap.v++->color = ptColor;
                vmap.v->uv.Set(1.0f, 0.0f);
                vmap.v++->color = ptColor;
            }
        }

        if (mLinePairs) {
            if (mLineHasCaps)
                num = num * 3 >> 1;
        } else
            num = (num - 1) * 2;
        mMesh->Faces().resize(num);
        num -= 2;
        while (num >= 0) {
            int i7 = num;
            if (mLinePairs) {
                if (mLineHasCaps) {
                    i7 = num % 6 + (num / 6) * 8;
                } else
                    i7 = num * 2;
            }
            mMesh->Faces(num).Set(i7, i7 + 2, i7 + 1);
            mMesh->Faces(num + 1).Set(i7 + 1, i7 + 2, i7 + 3);
            num -= 2;
        }
        mMesh->Sync(0x13F);
    }
}

void RndLine::SetPointPos(int i, const Vector3 &pos) {
    MILO_ASSERT((i >= 0) && (i < mPoints.size()), 0x1D0);
    mPoints[i].v = pos;
}

void RndLine::SetPointColor(int i, const Hmx::Color32 &color, bool sync) {
    MILO_ASSERT((i >= 0) && (i < mPoints.size()), 0x1D7);
    mPoints[i].c = color;
    VertsMap vmap;
    MapVerts(i, vmap);
    vmap.v++->color = color;
    vmap.v++->color = color;
    if (vmap.t != 0) {
        vmap.v++->color = color;
        vmap.v++->color = color;
    }
    if (sync)
        mMesh->Sync(0x1F);
}

void RndLine::SetPointsColor(int start, int end, const Hmx::Color32 &color) {
    MILO_ASSERT((start >= 0) && (start < mPoints.size()) && (end >= 0) && (end < mPoints.size()), 0x1EF);
    if (end < start) {
        int tmp = start;
        start = end;
        end = tmp;
    }
    for (int i = start; i <= end; i++) {
        mPoints[i].c = color;
        VertsMap vmap;
        MapVerts(i, vmap);
        vmap.v++->color = color;
        vmap.v++->color = color;
        if (vmap.t != 0) {
            vmap.v++->color = color;
            vmap.v++->color = color;
        }
    }
    mMesh->Sync(0x1F);
}

SAVE_OBJ(RndLine, 535)

void RndLine::SetUpdate(bool b1) {
    mLineUpdate = b1;
    if (!mLineUpdate) {
        Transform xfm;
        xfm = WorldXfm();
        static Vector3 offset(0, -1, 0);
        Multiply(offset, xfm, xfm.v);
        UpdateLine(xfm, 0);
        mMesh->SetLocalPos(offset);
    }
}

void RndLine::UpdateLinePair(RndLine::Point *pt1, RndLine::Point *pt2) {
    VertsMap vmap;
    MapVerts((pt1 - &mPoints[0]), vmap);

    if (pt1 == pt2) {
        Vector3 *vp;
        if (mLineHasCaps) {
            vp = (Vector3 *)&vmap.v->pos;
            vmap.v++;
            vp->x = pt1->unk0;
            vp->y = pt1->unk1;
            vp->z = pt1->unk2;
            vp = (Vector3 *)&vmap.v->pos;
            vmap.v++;
            vp->x = pt1->unk0;
            vp->y = pt1->unk1;
            vp->z = pt1->unk2;
        }
        vp = (Vector3 *)&vmap.v->pos;
        vmap.v++;
        vp->x = pt1->unk0;
        vp->y = pt1->unk1;
        vp->z = pt1->unk2;
        vp = (Vector3 *)&vmap.v->pos;
        vmap.v++;
        vp->x = pt1->unk0;
        vp->y = pt1->unk1;
        vp->z = pt1->unk2;
        vp = (Vector3 *)&vmap.v->pos;
        vmap.v++;
        vp->x = pt2->unk0;
        vp->y = pt2->unk1;
        vp->z = pt2->unk2;
        vp = (Vector3 *)&vmap.v->pos;
        vmap.v++;
        vp->x = pt2->unk0;
        vp->y = pt2->unk1;
        vp->z = pt2->unk2;
        if (mLineHasCaps) {
            vp = (Vector3 *)&vmap.v->pos;
            vmap.v++;
            vp->x = pt2->unk0;
            vp->y = pt2->unk1;
            vp->z = pt2->unk2;
            vp = (Vector3 *)&vmap.v->pos;
            vmap.v++;
            vp->x = pt2->unk0;
            vp->y = pt2->unk1;
            vp->z = pt2->unk2;
        }
    } else {
        Vector2 perp;

        float epsilon = 1e-4f;
        float invY1 = 1.0f / pt1->unk1;
        pt1->unk3 = pt1->unk0 * invY1;
        pt1->unk4 = pt1->unk2 * invY1;
        float invY2 = 1.0f / pt2->unk1;
        pt2->unk3 = pt2->unk0 * invY2;
        pt2->unk4 = pt2->unk2 * invY2;

        float dirX = pt2->unk3 - pt1->unk3;
        float dirZ = pt2->unk4 - pt1->unk4;
        pt1->unk5 = dirX;
        pt1->unk6 = dirZ;
        if (!(fabsf(dirX) < epsilon) || !(fabsf(dirZ) < epsilon)) {
            float zz = pt1->unk6;
            float inv = 1.0f / std::sqrt(dirX * dirX + zz * zz);
            pt1->unk5 = dirX * inv;
            pt1->unk6 = zz * inv;
        }
        float dx2 = pt1->unk5;
        float ndy = -pt1->unk6;
        pt1->unk8 = dx2;
        pt1->unk7 = ndy;
        float width = mWidth;
        float sx = ndy * width;
        pt1->unk8 = dx2 * width;
        pt1->unk7 = sx;
        pt2->unk7 = sx;
        pt2->unk8 = pt1->unk8;
        float py = pt1->unk7;
        float px = pt1->unk8;
        perp.y = py;
        perp.x = -px;

        if (mLineHasCaps) {
            Subtract(*(Vector3 *)&pt1->unk0, *(Vector2 *)&pt1->unk7, *(Vector3 *)&vmap.v->pos);
            Add(*(Vector3 *)&vmap.v->pos, perp, *(Vector3 *)&vmap.v->pos);
            vmap.v++;
            Add(*(Vector3 *)&pt1->unk0, *(Vector2 *)&pt1->unk7, *(Vector3 *)&vmap.v->pos);
            Add(*(Vector3 *)&vmap.v->pos, perp, *(Vector3 *)&vmap.v->pos);
            vmap.v++;
        }

        Subtract(*(Vector3 *)&pt1->unk0, *(Vector2 *)&pt1->unk7, *(Vector3 *)&vmap.v->pos);
        vmap.v++;
        Add(*(Vector3 *)&pt1->unk0, *(Vector2 *)&pt1->unk7, *(Vector3 *)&vmap.v->pos);
        vmap.v++;
        Subtract(*(Vector3 *)&pt2->unk0, *(Vector2 *)&pt2->unk7, *(Vector3 *)&vmap.v->pos);
        vmap.v++;
        Add(*(Vector3 *)&pt2->unk0, *(Vector2 *)&pt2->unk7, *(Vector3 *)&vmap.v->pos);
        vmap.v++;

        if (mLineHasCaps) {
            float ny = pt2->unk7;
            float nx = pt2->unk8;
            perp.x = nx;
            perp.y = -ny;
            Subtract(*(Vector3 *)&pt2->unk0, *(Vector2 *)&pt2->unk7, *(Vector3 *)&vmap.v->pos);
            Add(*(Vector3 *)&vmap.v->pos, perp, *(Vector3 *)&vmap.v->pos);
            vmap.v++;
            Add(*(Vector3 *)&pt2->unk0, *(Vector2 *)&pt2->unk7, *(Vector3 *)&vmap.v->pos);
            Add(*(Vector3 *)&vmap.v->pos, perp, *(Vector3 *)&vmap.v->pos);
        }
    }
}

template <class _T>
__declspec(noinline) typename _T::reference _outline_back(_T *_obj) {
    return _obj->back();
}

void RndLine::UpdateLine(RndLine::Point *start, RndLine::Point *end) {
    // Phase 1: Project all points (divide x,z by y in view space)
    for (Point *pt = start; pt <= end; pt++) {
        float *viewPos = &pt->unk0;
        float *proj = &pt->unk3;
        float invY = 1.0f / viewPos[1];
        proj[0] = viewPos[0] * invY;
        proj[1] = viewPos[2] * invY;
    }

    // Phase 2: Compute direction and side vectors between adjacent points
    Point *pt;
    for (pt = start; pt != end; pt++) {
        float curX = pt->unk3;
        float nextX = pt[1].unk3;
        float nextZ = pt[1].unk4;
        float dirX = nextX - curX;
        float curZ = pt->unk4;
        pt->unk5 = dirX;
        pt->unk6 = nextZ - curZ;

        if (!(std::fabs(dirX) < 0.0001f) || !(std::fabs(pt->unk6) < 0.0001f)) {
            float dirZ = pt->unk6;
            float invLen = 1.0f / std::sqrt(dirX * dirX + dirZ * dirZ);
            pt->unk5 = dirX * invLen;
            pt->unk6 = dirZ * invLen;
        }

        // Side vector: perpendicular to direction, scaled by width
        pt->unk7 = -pt->unk6;
        pt->unk8 = pt->unk5;
        float width = mWidth;
        pt->unk7 *= width;
        pt->unk8 *= width;
    }

    // Copy direction/side from second-to-last point to last point
    {
        float *lastWords = &pt->unk5;
        float *prevWords = &(pt - 1)->unk5;
        lastWords[0] = prevWords[0];
        lastWords[1] = prevWords[1];
        lastWords[2] = prevWords[2];
        lastWords[3] = prevWords[3];
    }

    // Phase 3: Handle fold angles at interior points
    Point *secondPt = start + 1;
    bool flipped = 0;

    float *startProj = &start->unk3;
    float *startDir = &start->unk5;
    float *startSide = &start->unk7;

    Hmx::Ray prevRay;
    prevRay.base.Set(startProj[0] + startSide[0], startProj[1] + startSide[1]);
    prevRay.dir.Set(startDir[0], startDir[1]);

    for (pt = secondPt; pt != end; pt++) {
        Point *p = pt;
        float *dir = &p->unk5;
        float *side = &p->unk7;
        float *proj = &p->unk3;
        Point *prevP = p - 1;
        float *prevDir2 = &prevP->unk5;

        float dot = dir[0] * prevDir2[0] + dir[1] * prevDir2[1];

        if (dot < mFoldCos) {
            flipped = !flipped;
        }

        if (flipped) {
            float s7 = p->unk7;
            float s8 = p->unk8;
            p->unk7 = -s7;
            p->unk8 = -s8;
        }

        Hmx::Ray oldPrevRay = prevRay;
        prevRay.base.Set(p->unk3 + p->unk7, p->unk4 + p->unk8);
        prevRay.dir.x = p->unk5;
        prevRay.dir.y = p->unk6;

        if (dot < 0.9998499751091003f) {
            Intersect(prevRay, oldPrevRay, *(Vector2 *)&p->unk7);
            float ix = p->unk7;
            float px = p->unk3;
            float iz = p->unk8;
            float pz = p->unk4;
            p->unk7 = ix - px;
            p->unk8 = iz - pz;
        }
    }

    if (flipped) {
        float *endSide = &end->unk7;
        endSide[0] = -endSide[0];
        endSide[1] = -endSide[1];
    }

    // Phase 4: Copy side vectors for points outside the visible range
    if (start == &mPoints[0]) {
        Point *pt = end + 1;
        Point *pointsEnd = &mPoints.back();
        for (; pt <= pointsEnd; pt++) {
            pt->unk7 = end->unk7;
            pt->unk8 = end->unk8;
            pt->unk0 = end->unk0;
            pt->unk1 = end->unk1;
            pt->unk2 = end->unk2;
        }
    } else if (&mPoints[0] < start) {
        for (Point *pt = &mPoints[0]; pt < start; pt++) {
            pt->unk7 = start->unk7;
            pt->unk8 = start->unk8;
            pt->unk0 = start->unk0;
            pt->unk1 = start->unk1;
            pt->unk2 = start->unk2;
        }
    }

    // Phase 5: Write vertex positions.
    // The cap offset is (-side.y, side.x) used to extend the cap perpendicular to the line direction.
    Point *pointsBegin = &mPoints[0];
    Point *pointsEnd = &mPoints.back();
    Vector2 capOffset;
    capOffset.x = -pointsBegin->unk8;
    capOffset.y = pointsBegin->unk7;

    VertsMap vmap;
    MapVerts(0, vmap);

    if (mLineHasCaps) {
        Subtract(*(Vector3 *)&pointsBegin->unk0, *(Vector2 *)&pointsBegin->unk7, vmap.v->pos);
        Add(vmap.v->pos, capOffset, vmap.v->pos);
        vmap.v++;
        Add(*(Vector3 *)&pointsBegin->unk0, *(Vector2 *)&pointsBegin->unk7, vmap.v->pos);
        Add(vmap.v->pos, capOffset, vmap.v->pos);
        vmap.v++;
    }

    for (Point *pt = pointsBegin; pt <= pointsEnd; pt++) {
        Subtract(*(Vector3 *)&pt->unk0, *(Vector2 *)&pt->unk7, vmap.v->pos);
        vmap.v++;
        Add(*(Vector3 *)&pt->unk0, *(Vector2 *)&pt->unk7, vmap.v->pos);
        vmap.v++;
    }

    if (mLineHasCaps) {
        if (flipped) {
            capOffset.y = pointsEnd->unk7;
            capOffset.x = -pointsEnd->unk8;
        } else {
            capOffset.y = -pointsEnd->unk7;
            capOffset.x = pointsEnd->unk8;
        }
        Subtract(*(Vector3 *)&pointsEnd->unk0, *(Vector2 *)&pointsEnd->unk7, vmap.v->pos);
        Add(vmap.v->pos, capOffset, vmap.v->pos);
        vmap.v++;
        Add(*(Vector3 *)&pointsEnd->unk0, *(Vector2 *)&pointsEnd->unk7, vmap.v->pos);
        Add(vmap.v->pos, capOffset, vmap.v->pos);
    }
}

void RndLine::UpdateLine(const Transform &camXfm, float nearPlane) {
    int numPts = (int)mPoints.size();
    if ((unsigned int)numPts < 2)
        return;

    Transform viewXfm;
    Transpose(camXfm, viewXfm);
    Multiply(WorldXfm(), viewXfm, viewXfm);

    int firstClipped = -1;
    int lastClipped = -1;
    int i = 0;
    float clipDist = nearPlane + 0.01f;

    numPts = (int)mPoints.size();
    for (i = 0; i < numPts; i++) {
        Point *pt = &mPoints[i];
        float *viewPos = &pt->unk0;
        Multiply(pt->v, viewXfm, *(Vector3 *)viewPos);
        if (viewPos[1] < clipDist) {
            lastClipped = i;
            if (firstClipped == -1) {
                firstClipped = i;
            }
        }
    }
    if (firstClipped == 0 && lastClipped == numPts - 1)
        return;

    if (!mLinePairs) {
        int startIdx;
        int endIdx;
        if (lastClipped != -1) {
            if (firstClipped > (numPts - 1) - lastClipped) {
                Point *prevPt = &mPoints[firstClipped - 1];
                Point *pt = &mPoints[firstClipped];
                float *prevView = &prevPt->unk0;
                float *curView = &pt->unk0;
                Interp(*(Vector3 *)prevView, *(Vector3 *)curView,
                       (clipDist - prevView[1]) / (curView[1] - prevView[1]),
                       *(Vector3 *)curView);
                endIdx = firstClipped;
                startIdx = 0;
            } else {
                Point *pt = &mPoints[lastClipped];
                Point *nextPt = &mPoints[lastClipped + 1];
                float *curView = &pt->unk0;
                float *nextView = &nextPt->unk0;
                Interp(*(Vector3 *)curView, *(Vector3 *)nextView,
                       (clipDist - curView[1]) / (nextView[1] - curView[1]),
                       *(Vector3 *)curView);
                endIdx = numPts - 1;
                startIdx = lastClipped;
            }
        } else {
            startIdx = 0;
            endIdx = numPts - 1;
        }
        UpdateLine(&mPoints[startIdx], &mPoints[endIdx]);
    } else {
        i = 0;
        while (i < numPts - 1) {
            Point *pt1 = &mPoints[i];
            Point *pt2 = &mPoints[i + 1];
            float dist1 = (&pt1->unk0)[1];
            if (dist1 < clipDist) {
                float dist2 = (&pt2->unk0)[1];
                if (dist2 < clipDist) {
                    pt2 = pt1;
                } else {
                    Interp(*(Vector3 *)&pt1->unk0, *(Vector3 *)&pt2->unk0,
                           (clipDist - dist1) / (dist2 - dist1),
                           *(Vector3 *)&pt1->unk0);
                }
            } else {
                float dist2 = (&pt2->unk0)[1];
                if (dist2 < clipDist) {
                    Interp(*(Vector3 *)&pt2->unk0, *(Vector3 *)&pt1->unk0,
                           (clipDist - dist2) / (dist1 - dist2),
                           *(Vector3 *)&pt2->unk0);
                }
            }
            UpdateLinePair(pt1, pt2);
            i += 2;
        }
    }

    mMesh->Sync(0x1F);
}

void RndLine::UpdateInternal() {
    mFoldCos = cosf(mFoldAngle);
    mMesh->SetMat(mMat);
    SetNumPoints(mPoints.size());
}

BEGIN_LOADS(RndLine)
    int rev;
    bs >> rev;
    ASSERT_GLOBAL_REV(rev, LINE_REV)
    if (rev > 3) {
        Hmx::Object::Load(bs);
    }
    RndDrawable::Load(bs);
    if (rev < 3) {
        ObjPtrList<Hmx::Object> list(this);
        int _;
        bs >> _ >> list;
    }
    RndTransformable::Load(bs);
    bs >> mMat >> mPoints >> mWidth;
    if (rev > 0) {
        bs >> mFoldAngle;
        LOAD_BITFIELD(bool, mLineHasCaps)
    }
    if (rev > 1) {
        LOAD_BITFIELD(bool, mLinePairs)
    }
    UpdateInternal();
END_LOADS

BEGIN_COPYS(RndLine)
    CREATE_COPY_AS(RndLine, d);
    MILO_ASSERT(d, 0x2C3);
    COPY_SUPERCLASS(Hmx::Object)
    COPY_SUPERCLASS(RndDrawable)
    COPY_SUPERCLASS(RndTransformable)
    COPY_MEMBER_FROM(d, mMat)
    COPY_MEMBER_FROM(d, mPoints)
    COPY_MEMBER_FROM(d, mWidth)
    COPY_MEMBER_FROM(d, mFoldAngle)
    COPY_MEMBER_FROM(d, mLineHasCaps)
    COPY_MEMBER_FROM(d, mLinePairs)
    UpdateInternal();
END_COPYS

TextStream &operator<<(TextStream &ts, const RndLine::Point &pt) {
    Hmx::Color color(pt.c);
    ts << "\n\tv:" << pt.v << "\n\tc:" << color;
    return ts;
}

void RndLine::Print() {
    TheDebug << "   points: " << mPoints << "\n";
    TheDebug << "   width: " << mWidth << "\n";
    TheDebug << "   foldAngle: " << mFoldAngle << "\n";
    TheDebug << "   hasCaps: " << mLineHasCaps << "\n";
    TheDebug << "   linePairs:" << mLinePairs << "\n";
}

RndLine::RndLine() : mWidth(1.0f), mFoldAngle(1.5707964f), mMat(this) {
    mLineHasCaps = true;
    mLinePairs = false;
    mLineUpdate = true;
    mMesh = Hmx::Object::New<RndMesh>();
    mMesh->SetMutable(0x1F);
    mMesh->SetTransParent(this, false);
    UpdateInternal();
}

RndLine::~RndLine() { RELEASE(mMesh); }

DataNode RndLine::OnSetMat(const DataArray *array) {
    RndMat *mat = array->Obj<RndMat>(2);
    SetMat(mat);
    SetShowing(mat);
    return 0;
}

BEGIN_HANDLERS(RndLine)
    HANDLE_EXPR(num_points, NumPoints())
    HANDLE_ACTION(
        set_point_pos,
        SetPointPos(_msg->Int(2), Vector3(_msg->Float(3), _msg->Float(4), _msg->Float(5)))
    )
    HANDLE_ACTION(
        set_point_color,
        SetPointColor(
            _msg->Int(2),
            Hmx::Color32(_msg->Float(3), _msg->Float(4), _msg->Float(5), _msg->Float(6)),
            true
        )
    )
    HANDLE_ACTION(
        set_points_color,
        SetPointsColor(
            _msg->Int(2),
            _msg->Int(3),
            Hmx::Color32(_msg->Float(4), _msg->Float(5), _msg->Float(6), _msg->Float(7))
        )
    )
    HANDLE_ACTION(set_update, SetUpdate(_msg->Int(2)))
    HANDLE(set_mat, OnSetMat)
    HANDLE_SUPERCLASS(RndDrawable)
    HANDLE_SUPERCLASS(RndTransformable)
    HANDLE_SUPERCLASS(Hmx::Object)
    HANDLE_CHECK(797)
END_HANDLERS

BEGIN_PROPSYNCS(RndLine)
    SYNC_PROP(width, mWidth);
    SYNC_SUPERCLASS(RndDrawable)
    SYNC_SUPERCLASS(RndTransformable)
END_PROPSYNCS
