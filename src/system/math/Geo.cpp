#include "Geo.h"
#include "math/Bsp.h"
#include "math/Rot.h"
#include "obj/DataFunc.h"
#include "os/Debug.h"
#include "utl/MakeString.h"
#include "decomp.h"
#include <cmath>

float gBSPPosTol = 0.01f;
float gBSPDirTol = 0.985f;
int gBSPMaxDepth = 20;
int gBSPMaxCandidates = 40;
float gBSPCheckScale = 1.1f;

static DataNode SetBSPParams(DataArray *da) {
    SetBSPParams(da->Float(1), da->Float(2), da->Int(3), da->Int(4), da->Float(5));
    return DataNode();
}

void GeoInit() {
    DataArray *cfg = SystemConfig("math");
    SetBSPParams(cfg->FindArray("bsp_pos_tol")->Float(1), cfg->FindArray("bsp_dir_tol")->Float(1), cfg->FindArray("bsp_max_depth")->Int(1), cfg->FindArray("bsp_max_candidates")->Int(1), cfg->FindArray("bsp_check_scale")->Float(1));
    DataRegisterFunc("set_bsp_params", SetBSPParams);
}

DECOMP_FORCEACTIVE(Geo, "points:");

TextStream &deadstrippedVec2Read(TextStream &ts, const std::vector<Vector2> &vec) {
    return ts << vec;
}

BinStream &operator>>(BinStream &bs, BSPNode *&bsp) {
    bool exists;
    bs >> exists;
    if (exists) {
        bsp = new BSPNode();
        bs >> bsp->plane >> bsp->left >> bsp->right;
    } else
        bsp = 0;
    return bs;
}

void Box::GrowToContain(const Vector3 &vec, bool b) {
    if (b) {
        mMin = mMax = vec;
    } else
        for (int i = 0; i < 3; i++) {
            if (vec[i] < mMin[i]) {
                mMin[i] = vec[i];
            } else if (vec[i] > mMax[i]) {
                mMax[i] = vec[i];
            }
        }
}

bool Box::Clamp(Vector3 &vec) {
    return ClampEq(vec.x, mMin.x, mMax.x) | ClampEq(vec.y, mMin.y, mMax.y) | ClampEq(vec.z, mMin.z, mMax.z);
}

void Multiply(const Box &box, float f, Box &out) {
    float miny = box.mMin.y, maxy = box.mMax.y;
    float minz = box.mMin.z, maxz = box.mMax.z;
    float minx = box.mMin.x, maxx = box.mMax.x;
    float cy = miny + (maxy - miny) * 0.5f;
    float cz = minz + (maxz - minz) * 0.5f;
    float cx = minx + (maxx - minx) * 0.5f;
    float py = (maxy - cy) * f;
    float pz = (maxz - cz) * f;
    float ny = (miny - cy) * f;
    float nz = (minz - cz) * f;
    float nx = (minx - cx) * f;
    float px = (maxx - cx) * f;
    out.mMax.z = cz + pz;
    out.mMax.y = cy + py;
    out.mMin.x = cx + nx;
    out.mMax.x = cx + px;
    out.mMin.y = cy + ny;
    out.mMin.z = cz + nz;
}

void Multiply(const Plane &p, const Transform &t, Plane &out) {
    Hmx::Matrix3 invM;
    FastInvert(t.m, invM);
    float b = p.b;
    float negD = -p.d;
    float a = p.a;
    float c = p.c;
    float nx = invM.x.x * a + invM.x.y * b + invM.x.z * c;
    float ny = invM.y.x * a + invM.y.y * b + invM.y.z * c;
    float nz = invM.z.x * a + invM.z.y * b + invM.z.z * c;
    float scalar = negD / (a * a + b * b + c * c);
    Vector3 on(a * scalar, b * scalar, c * scalar);
    Vector3 pOut;
    Multiply(on, t, pOut);
    out.Set(nx, ny, nz, -(pOut.y * ny + (pOut.z * nz + pOut.x * nx)));
}

void Intersect(const Hmx::Ray &r1, const Hmx::Ray &r2, Vector2 &out) {
    float r1bx = r1.base.x;
    float r1dx = r1.dir.x;
    float r2bx = r2.base.x;
    float r1by = r1.base.y;
    float r2dx = r2.dir.x;
    float r1dy = r1.dir.y;
    float r2by = r2.base.y;
    float r2dy = r2.dir.y;
    float dot = r2dx * r1dy - r1dx * r2dy;
    if (dot) {
        float s = (r1dy * (r1bx - r2bx) + r1dx * (r2by - r1by)) / dot;
        out.Set(s * r2dx + r2bx, s * r2dy + r2by);
    } else {
        out = r1.base;
    }
}

void Intersect(const Transform &trans, const Plane &plane, Hmx::Ray &ray) {
    float pa = plane.a;
    float pd = plane.d;
    float pb = plane.b;
    float pc = plane.c;
    float mzy = trans.m.z.y;
    float scalar = -pd / (pa * pa + pb * pb + pc * pc);
    float onX = pa * scalar;
    float onY = pb * scalar;
    float onZ = pc * scalar;
    float tvy = trans.v.y;
    float tvx = trans.v.x;
    float tvz = trans.v.z;
    float tmpX = onX - tvx;
    float tmpY = onY - tvy;
    float tmpZ = onZ - tvz;
    float dotY = trans.m.y.x * pa + trans.m.y.y * pb + trans.m.y.z * pc;
    float dotX = trans.m.x.x * pa + trans.m.x.y * pb + trans.m.x.z * pc;
    float dotZ = trans.m.z.x * pa + mzy * pb + trans.m.z.z * pc;
    float pointY = trans.m.y.x * tmpX + trans.m.y.y * tmpY + trans.m.y.z * tmpZ;
    float pointX = trans.m.x.x * tmpX + trans.m.x.y * tmpY + trans.m.x.z * tmpZ;
    float pointZ = trans.m.z.x * tmpX + mzy * tmpY + trans.m.z.z * tmpZ;
    ray.dir.Set(dotX, dotY);
    if (fabsf(dotX) > fabsf(dotY)) {
        ray.base.Set(pointY, (dotZ * pointZ) / dotX + pointX);
    }
    else {
        ray.base.Set((dotZ * pointZ) / dotY + pointY, pointX);
    }
}

bool Intersect(const Segment &seg, const Triangle &tri, bool b, float &out) {
    float segDirY = seg.end.y - seg.start.y;
    float segDirX = seg.end.x - seg.start.x;
    float segDirZ = seg.end.z - seg.start.z;

    float segDirDot = tri.frame.z.x * segDirX + tri.frame.z.y * segDirY + tri.frame.z.z * segDirZ;

    if (fabsf(segDirDot) < 0.0001f || (b && segDirDot > 0.0f)) {
        return false;
    }

    float vec3AY = seg.start.y - tri.origin.y;
    float vec3AX = seg.start.x - tri.origin.x;
    float vec3AZ = seg.start.z - tri.origin.z;

    float tempDot = tri.frame.z.x * vec3AX + tri.frame.z.y * vec3AY + tri.frame.z.z * vec3AZ;
    float t = -tempDot / segDirDot;
    out = t;

    if (t < 0.0f || t > 1.0f) {
        return false;
    }

    float mulX = segDirX * t;
    float mulY = segDirY * t;
    float mulZ = segDirZ * t;
    float vec3BX = seg.start.x + mulX - tri.origin.x;
    float vec3BY = seg.start.y + mulY - tri.origin.y;
    float vec3BZ = seg.start.z + mulZ - tri.origin.z;

    float dotXX = tri.frame.x.x * tri.frame.x.x + tri.frame.x.y * tri.frame.x.y + tri.frame.x.z * tri.frame.x.z;
    float dotYY = tri.frame.y.x * tri.frame.y.x + tri.frame.y.y * tri.frame.y.y + tri.frame.y.z * tri.frame.y.z;
    float dotXY = tri.frame.x.x * tri.frame.y.x + tri.frame.x.y * tri.frame.y.y + tri.frame.x.z * tri.frame.y.z;
    float dotX3B = tri.frame.x.x * vec3BX + tri.frame.x.y * vec3BY + tri.frame.x.z * vec3BZ;
    float dotY3B = tri.frame.y.x * vec3BX + tri.frame.y.y * vec3BY + tri.frame.y.z * vec3BZ;

    float denom = dotXY * dotXY - dotYY * dotXX;
    float k = (dotY3B * dotXY - dotX3B * dotYY) / denom;
    if (k < 0.0 || k > 1.0) {
        return false;
    }
    float j = (dotX3B * dotXY - dotY3B * dotXX) / denom;
    if (j < 0.0 || k + j > 1.0) {
        return false;
    }
    return true;
}

bool Intersect(const Vector3 &v, const BSPNode *n) {
    MILO_ASSERT(n, 0x49e);
    if (n->plane.Dot(v) > 0) {
        if (!n->left) return false;
        return Intersect(v, n->left);
    } else {
        if (!n->right) return true;
        return Intersect(v, n->right);
    }
}

bool Intersect(const Segment &seg, const BSPNode *n, float &t, Plane &p) {
    MILO_ASSERT(n, 0x4ba);

    float startDot = n->plane.Dot(seg.start);
    float endDot = n->plane.Dot(seg.end);

    if (startDot >= 0.0f && endDot >= 0.0f) {
        if (!n->left)
            return false;
        return Intersect(seg, n->left, t, p);
    }

    if (startDot <= 0.0f && endDot <= 0.0f) {
        if (!n->right) {
            t = 0.0f;
            return true;
        }
        return Intersect(seg, n->right, t, p);
    }

    float t2 = 0.0f;
    float denom = startDot - endDot;
    if (denom == 0.0f)
        return false;

    float frac = startDot / denom;

    Segment seg1; // first half: [start, mid]
    Segment seg2; // second half: [mid, end]

    Interp(seg.start, seg.end, frac, seg1.end); // mid → seg1.end
    seg1.start = seg.start;
    seg2.start = seg1.end;
    seg2.end = seg.end;

    if (startDot > endDot) {
        // start on positive (left) side: first half → left, second half → right
        if (n->left && Intersect(seg1, n->left, t2, p)) {
            t = frac * t2;
        } else if (!n->right) {
            t = frac;
        } else {
            if (Intersect(seg2, n->right, t2, p)) {
                t = t2 * (1.0f - frac) + frac;
            } else {
                return false;
            }
        }
        if (t2 == 0.0f && t != 0.0f) {
            p.a = n->plane.a;
            p.b = n->plane.b;
            p.c = n->plane.c;
            p.d = n->plane.d;
        }
    } else {
        // start on negative (right) side: first half → right, second half → left
        if (!n->right) {
            t = 0.0f;
            goto done_neg;
        }
        if (Intersect(seg1, n->right, t2, p)) {
            t = frac * t2;
        } else {
            if (n->left && Intersect(seg2, n->left, t2, p)) {
                t = t2 * (1.0f - frac) + frac;
            } else {
                return false;
            }
        }
    done_neg:
        if (t2 == 0.0f && t != 0.0f) {
            float nd = n->plane.d;
            float nc = n->plane.c;
            float nb = n->plane.b;
            float na = n->plane.a;
            p.d = -nd;
            p.c = -nc;
            p.b = -nb;
            p.a = -na;
        }
    }
    return true;
}

bool Intersect(const Transform &tf, const Hmx::Polygon &poly, const BSPNode *node) {
    if (!node)
        return true;

    bool front = false;
    bool back = false;
    for (const Vector2 *i = poly.mPoints.begin(); i != poly.mPoints.end(); i++) {
        Vector3 v(i->x, i->y, 0.0f);
        Multiply(v, tf, v);
        float dot = node->plane.Dot(v);
        if (dot >= 0.0f)
            front = true;
        if (dot < 0.0f)
            back = true;
    }

    if (front && !back) {
        return Intersect(tf, poly, node->left);
    }
    if (!front && back) {
        return Intersect(tf, poly, node->right);
    }

    Hmx::Ray r;
    Intersect(tf, node->plane, r);

    Hmx::Polygon splitPoly;
    Clip(poly, r, splitPoly);
    if (!splitPoly.mPoints.empty() && !Intersect(tf, splitPoly, node->left))
        return false;

    Hmx::Ray negRay;
    negRay.base = r.base;
    negRay.dir.Set(-r.dir.x, -r.dir.y);
    Hmx::Polygon splitPoly2;
    Clip(poly, negRay, splitPoly2);
    if (!splitPoly2.mPoints.empty() && !Intersect(tf, splitPoly2, node->right))
        return false;

    return true;
}

bool Intersect(const Segment &seg, const Sphere &sphere) {
    float cdiff_y = sphere.center.y - seg.start.y;
    float start_x = seg.start.x;
    float start_y = seg.start.y;
    float end_y = seg.end.y;
    float start_z = seg.start.z;
    float end_z = seg.end.z;
    float dir_z = end_z - start_z;
    float dir_y = end_y - start_y;
    float end_x = seg.end.x;
    float center_x = sphere.center.x;
    float dir_x = end_x - start_x;
    float cdiff_x = center_x - start_x;
    float cdiff_z = sphere.center.z - start_z;
    float a = dir_x * dir_x + dir_y * dir_y + dir_z * dir_z;
    if (a == 0.0f)
        return false;
    float t = (cdiff_x * dir_x + cdiff_y * dir_y + cdiff_z * dir_z) / a;
    if (t > 1.0f)
        t = 1.0f;
    else if (t < 0.0f)
        t = 0.0f;
    float cx, cy, cz;
    if (t == 0.0f) {
        cx = seg.start.x;
        cy = seg.start.y;
        cz = seg.start.z;
    } else if (t == 1.0f) {
        cx = seg.end.x;
        cy = seg.end.y;
        cz = seg.end.z;
    } else {
        cx = start_x + t * (end_x - start_x);
        cy = start_y + t * (end_y - start_y);
        cz = start_z + t * (end_z - start_z);
    }
    float dy = cy - sphere.center.y;
    float dx = cx - sphere.center.x;
    float dz = cz - sphere.center.z;
    float r = sphere.radius;
    float r2 = r * r;
    float dist2 = dy * dy + dx * dx + dz * dz;
    if (dist2 > r2)
        return false;
    return true;
}

void SetBSPParams(float f1, float f2, int r3, int r4, float f3) {
    gBSPPosTol = f1;
    gBSPDirTol = f2;
    gBSPMaxDepth = r3;
    gBSPMaxCandidates = r4;
    gBSPCheckScale = f3;
}

#pragma push
#pragma dont_inline on
bool CheckBSPTree(const BSPNode *node, const Box &box) {
    if (!gBSPCheckScale)
        return true;
    Box box68;
    Multiply(box, gBSPCheckScale, box68);
    Hmx::Polygon polygon70;
    polygon70.mPoints.resize(4);
    Transform tf50;
    polygon70.mPoints[0] = Vector2(box68.mMin.x, box68.mMin.y);
    polygon70.mPoints[1] = Vector2(box68.mMax.x, box68.mMin.y);
    polygon70.mPoints[2] = Vector2(box68.mMax.x, box68.mMax.y);
    polygon70.mPoints[3] = Vector2(box68.mMin.x, box68.mMax.y);
    tf50.m.Identity();
    tf50.v.Set(0, 0, box68.mMin.z);
    if (Intersect(tf50, polygon70, node))
        return false;
    // first intersect check

    polygon70.mPoints.clear();
    polygon70.mPoints.resize(4);
    polygon70.mPoints[0] = Vector2(box68.mMin.x, -box68.mMax.y);
    polygon70.mPoints[1] = Vector2(box68.mMax.x, -box68.mMax.y);
    polygon70.mPoints[2] = Vector2(box68.mMax.x, -box68.mMin.y);
    polygon70.mPoints[3] = Vector2(box68.mMin.x, -box68.mMin.y);
    float negone = -1.0f;
    tf50.m.Set(1.0f, 0.0f, 0.0f, 0.0f, negone, 0.0f, 0.0f, 0.0f, 0.0f);
    tf50.v.Set(0, 0, box68.mMax.z);
    if (Intersect(tf50, polygon70, node))
        return false;
    // second intersect check

    polygon70.mPoints.clear();
    polygon70.mPoints.resize(4);
    polygon70.mPoints[0] = Vector2(box68.mMin.y, box68.mMin.z);
    polygon70.mPoints[1] = Vector2(box68.mMax.y, box68.mMin.z);
    polygon70.mPoints[2] = Vector2(box68.mMax.y, box68.mMax.z);
    polygon70.mPoints[3] = Vector2(box68.mMin.y, box68.mMax.z);
    tf50.m.Set(1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f);
    tf50.v.Set(box68.mMin.x, 0, 0);
    if (Intersect(tf50, polygon70, node))
        return false;
    // third intersect check

    polygon70.mPoints.clear();
    polygon70.mPoints.resize(4);
    polygon70.mPoints[0] = Vector2(-box68.mMax.y, box68.mMin.z);
    polygon70.mPoints[1] = Vector2(-box68.mMin.y, box68.mMin.z);
    polygon70.mPoints[2] = Vector2(-box68.mMin.y, box68.mMax.z);
    polygon70.mPoints[3] = Vector2(-box68.mMax.y, box68.mMax.z);
    tf50.m.Set(1.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f);
    tf50.v.Set(box68.mMax.x, 0, 0);
    if (Intersect(tf50, polygon70, node))
        return false;
    // fourth intersect check

    polygon70.mPoints.clear();
    polygon70.mPoints.resize(4);
    polygon70.mPoints[0] = Vector2(box68.mMin.x, box68.mMin.z);
    polygon70.mPoints[1] = Vector2(box68.mMax.x, box68.mMin.z);
    polygon70.mPoints[2] = Vector2(box68.mMax.x, box68.mMax.z);
    polygon70.mPoints[3] = Vector2(box68.mMin.x, box68.mMax.z);
    tf50.m.Set(1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f);
    tf50.v.Set(0, box68.mMax.y, 0);
    if (Intersect(tf50, polygon70, node))
        return false;
    // fifth intersect check

    polygon70.mPoints.clear();
    polygon70.mPoints.resize(4);
    polygon70.mPoints[0] = Vector2(-box68.mMax.x, box68.mMin.z);
    polygon70.mPoints[1] = Vector2(-box68.mMin.x, box68.mMin.z);
    polygon70.mPoints[2] = Vector2(-box68.mMin.x, box68.mMax.z);
    polygon70.mPoints[3] = Vector2(-box68.mMax.x, box68.mMax.z);
    tf50.m.Set(-1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f);
    tf50.v.Set(0, box68.mMin.y, 0);
    if (Intersect(tf50, polygon70, node))
        return false;
    return true;
    // sixth and final intersect check
}
#pragma pop

void NumNodes(const BSPNode *node, int &num, int &maxDepth) {
    static int depth = 0;
    if (node) {
        depth++;
        if (depth == 1) {
            num = 0;
            maxDepth = 1;
        } else if (depth > maxDepth) {
            maxDepth = depth;
        }
        NumNodes(node->left, num, maxDepth);
        NumNodes(node->right, num, maxDepth);
        num++;
        depth--;
    }
}

void MultiplyEq(BSPNode *n, const Transform &t) {
    if (!n) return;
    Multiply(n->plane, t, n->plane);
    Normalize(n->plane, n->plane);
    MultiplyEq(n->left, t);
    MultiplyEq(n->right, t);
}

inline void Normalize(const Plane &in, Plane &out) {
#ifdef MATCHING
    register float a_save = in.a;
    if (a_save == 0.0f && in.b == 0.0f && in.c == 0.0f && in.d == 0.0f) return;

    register Plane *_p = &out;
    register __vec2x32float__ ab;
    register float c;
    register float sum;
    register float zero;
    register float half = 0.5f;
    register float inv;
    register float inv_sq;
    register float mag;
    register float d_val;
    register float b_val;
    register float c_val;
    register float a_val;
    register float one;
    register float inv_mag;

    asm {
        psq_l ab, Plane.a(_p), 0, 0
        lfs c, Plane.c(_p)
        ps_mul ab, ab, ab
        ps_madd sum, c, c, ab
        ps_sum0 sum, sum, ab, ab
        fsubs zero, half, half
        fcmpu sum, zero
        beq _done
    }

    register float three = 3.0f;

    asm {
        frsqrte inv, sum
        fmuls inv_sq, inv, inv
        fmuls inv, inv, half
        fnmsubs inv_sq, inv_sq, sum, three
        fmuls inv, inv_sq, inv
        fmuls mag, sum, inv
        lfs d_val, Plane.d(_p)
        lfs c_val, Plane.c(_p)
    }

    one = 1.0f;

    asm {
        lfs b_val, Plane.b(_p)
        fdivs inv_mag, one, mag
        fmuls b_val, b_val, inv_mag
        fmuls d_val, d_val, inv_mag
        fmuls c_val, c_val, inv_mag
        stfs b_val, Plane.b(_p)
        fmuls a_val, a_save, inv_mag
        stfs d_val, Plane.d(_p)
        stfs a_val, Plane.a(_p)
        stfs c_val, Plane.c(_p)
        _done:
    }
#else
    float mult = 0;
    float len = std::sqrt(in.a * in.a + in.b * in.b + in.c * in.c);
    if (len != 0) {
        mult = 1 / len;
    }
    out.Set(in.a * mult, in.b * mult, in.c * mult, in.d * mult);
#endif
}

void BSPFace::OnSide(const Plane &plane, bool &front, bool &back) {
    front = false;
    back = false;
    Vector3 pt;
    const Vector2 *it = p.mPoints.begin();
    while (it != p.mPoints.end()) {
        pt.x = it->x;
        pt.y = it->y;
        pt.z = 0.0f;
        Multiply(pt, t, pt);
        float dot = plane.a * pt.x + plane.b * pt.y + plane.c * pt.z + plane.d;
        if (dot > gBSPPosTol) front = true;
        if (dot < -gBSPPosTol) back = true;
        ++it;
    }
}

void BSPFace::Update() {
    MILO_ASSERT(p.mPoints.size() > 2, 0x696);

    const Vector2 *anchor = p.mPoints.begin();
    const Vector2 *v1 = anchor + 1;
    const Vector2 *v2 = anchor + 2;
    area = 0.0f;
    while (v2 != p.mPoints.end()) {
        area += (v1->y * anchor->x - v1->x * anchor->y +
                 v2->x * anchor->y - v2->y * anchor->x +
                 v2->y * v1->x - v2->x * v1->y) * 0.5f;
        v1 = v2;
        v2++;
    }

    planes.clear();

    Plane facePlane;
    facePlane.a = t.m.z.x;
    facePlane.b = t.m.z.y;
    facePlane.c = t.m.z.z;
    facePlane.d = -(t.m.z.x * t.v.x + t.m.z.y * t.v.y + t.m.z.z * t.v.z);
    planes.insert(planes.end(), facePlane);

    Vector3 prevPt(p.mPoints.back().x, p.mPoints.back().y, 0.0f);
    Multiply(prevPt, t, prevPt);

    for (const Vector2 *it = p.mPoints.begin(); it != p.mPoints.end(); it++) {
        Vector3 curPt(it->x, it->y, 0.0f);
        Multiply(curPt, t, curPt);

        float dx = curPt.x - prevPt.x;
        float dy = curPt.y - prevPt.y;
        float dz = curPt.z - prevPt.z;

        if (dx != 0.0f || dy != 0.0f || dz != 0.0f) {
            Vector3 normal;
            normal.x = t.m.z.z * dy - t.m.z.y * dz;
            normal.y = t.m.z.x * dz - t.m.z.z * dx;
            normal.z = t.m.z.y * dx - t.m.z.x * dy;
            Normalize(normal, normal);

            Plane edgePlane;
            edgePlane.a = normal.x;
            edgePlane.b = normal.y;
            edgePlane.c = normal.z;
            edgePlane.d = -(normal.x * curPt.x + (normal.y * curPt.y + normal.z * curPt.z));
            planes.insert(planes.end(), edgePlane);

            prevPt = curPt;
        }
    }
}

void BSPFace::Set(const Vector3 &p1, const Vector3 &p2, const Vector3 &p3) {
    Subtract(p2, p1, t.m.x);
    Normalize(t.m.x, t.m.x);

    Subtract(p3, p1, t.m.y);
    Cross(t.m.x, t.m.y, t.m.z);
    Normalize(t.m.z, t.m.z);
    Cross(t.m.z, t.m.x, t.m.y);

    t.v = p1;

    p.mPoints.clear();
    Vector3 v;
    Vector2 pt;

    MultiplyTranspose(p1, t, v);
    pt.Set(v.x, v.y);
    p.mPoints.push_back(pt);

    MultiplyTranspose(p2, t, v);
    pt.Set(v.x, v.y);
    p.mPoints.push_back(pt);

    MultiplyTranspose(p3, t, v);
    pt.Set(v.x, v.y);
    p.mPoints.push_back(pt);

    Update();
}

bool MakeBSPTree(BSPNode *&node, std::list<BSPFace> &faces, int depth) {
    if (faces.empty()) {
        node = nullptr;
        return true;
    }
    int nextDepth = depth + 1;
    if (nextDepth > gBSPMaxDepth) {
        TheDebug.Notify(MakeString("Bsp too deep"));
        return false;
    }
    node = new BSPNode();
    faces.sort();

    int totalFaces = 0;
    for (std::list<BSPFace>::iterator it = faces.begin(); it != faces.end(); ++it)
        totalFaces++;

    int candidateIdx = 0;
    float bestScore = -1.0f;
    float zero = 0.0f;
    float powExp = 0.6f;
    for (std::list<BSPFace>::iterator faceIt = faces.begin(); faceIt != faces.end(); ++faceIt) {
        if (candidateIdx >= gBSPMaxCandidates) break;
        for (std::list<Plane>::iterator planeIt = faceIt->planes.begin(); planeIt != faceIt->planes.end(); ++planeIt) {
            if (totalFaces == 1) {
                node->plane = *planeIt;
                bestScore = zero;
                break;
            }
            int frontCount = 0, backCount = 0, spanCount = 0;
            float frontArea = zero, backArea = zero;
            std::list<BSPFace>::iterator jt;
            for (jt = faces.begin(); jt != faces.end(); ++jt) {
                bool front, back;
                jt->OnSide(*planeIt, front, back);
                if (!front && !back) {
                    if (fabsf(planeIt->a * jt->t.m.z.x + planeIt->b * jt->t.m.z.y + planeIt->c * jt->t.m.z.z) < gBSPDirTol)
                        break;
                } else {
                    if (back) {
                        backArea += jt->area;
                        backCount++;
                        if (!front) continue;
                        spanCount++;
                    }
                    frontArea += jt->area;
                    frontCount++;
                }
            }
            if (jt != faces.end()) {
                candidateIdx--;
                continue;
            }
            float powBack = (float)pow((float)(spanCount + backCount), powExp);
            float score = (float)pow((float)(spanCount + frontCount), powExp) * frontArea
                        + powBack * backArea;
            if (frontCount < totalFaces && backCount < totalFaces && (bestScore < zero || score < bestScore)) {
                node->plane = *planeIt;
                bestScore = score;
            }
        }
        candidateIdx++;
    }

    if (bestScore < zero) {
        TheDebug.Notify(MakeString("Couldn't find candidate plane"));
        return false;
    }

    std::list<BSPFace> backFaces, frontFaces;
    std::list<BSPFace>::iterator it = faces.begin();
    while (it != faces.end()) {
        std::list<BSPFace>::iterator cur = it++;
        bool front, back;
        cur->OnSide(node->plane, front, back);
        if (!front && !back) {
            faces.erase(cur);
        } else if (!back) {
            frontFaces.splice(frontFaces.begin(), faces, cur);
        } else if (!front) {
            backFaces.splice(backFaces.begin(), faces, cur);
        } else {
            Hmx::Ray ray;
            Intersect(cur->t, node->plane, ray);
            BSPFace frontFace;
            frontFace.t = cur->t;
            Clip(cur->p, ray, frontFace.p);
            if (frontFace.p.mPoints.size() > 2) {
                frontFace.Update();
                frontFaces.insert(frontFaces.begin(), frontFace);
            }
            ray.dir.Set(-ray.dir.x, -ray.dir.y);
            Clip(cur->p, ray, cur->p);
            if (cur->p.mPoints.size() > 2) {
                cur->Update();
                backFaces.splice(backFaces.begin(), faces, cur);
            }
        }
    }

    bool ok = MakeBSPTree(node->left, frontFaces, nextDepth);
    if (!ok)
        return false;
    ok = MakeBSPTree(node->right, backFaces, nextDepth);
    backFaces.clear();
    frontFaces.clear();
    return ok;
}

void Sphere::GrowToContain(const Sphere &s) {
    GrowToContain(s.center, s.radius);
}

void Sphere::GrowToContain(const Vector3 &v, float r) {
    float zero = 0.0f;
    if (r == zero)
        return;
    if (radius == zero) {
        center = v;
        radius = r;
        return;
    }
    float vy = v.y;
    float cy = center.y;
    float half = 0.5f;
    float vx = v.x;
    float cx = center.x;
    float dy = vy - cy;
    float vz = v.z;
    float cz = center.z;
    float dx = vx - cx;
    float dz = vz - cz;
#ifdef __MWERKS__
    float dist = dx * dx + dy * dy + dz * dz;
    if (dist != (half - half)) {
        float inv = __frsqrte(dist);
        dist *= -((inv * inv * dist) - 3.0f) * (inv * half);
    }
#else
    float dist = std::sqrt(dx * dx + dy * dy + dz * dz);
#endif
    float oldRadius = radius;
    if (r + dist > oldRadius) {
        float totalDist = dist + oldRadius;
        if (totalDist < r) {
            center = v;
            radius = r;
            return;
        }
        if (dist != zero) {
            float invDist = 1.0f / dist;
            float ndz = dz * invDist;
            float ndy = dy * invDist;
            float ndx = dx * invDist;
            dx = ndx;
            dy = ndy;
            dz = ndz;
            float az = cz - ndz * oldRadius;
            radius = half * (r + totalDist);
            float ay = cy - ndy * oldRadius;
            float ax = cx - ndx * oldRadius;
            center.z = half * ((vz + ndz * r) - az) + az;
            center.y = half * ((vy + ndy * r) - ay) + ay;
            center.x = half * ((vx + ndx * r) - ax) + ax;
        }
    }
}

void Frustum::Set(float near, float far, float fovY, float ratio) {
    front.Set(0, 1, 0, -near);
    back.Set(0, -1, 0, far);
    float halfY = fovY * 0.5f;
    float sy = std::sin(halfY);
    float cy = std::cos(halfY);
    float sx = sy / ratio;
    top.Set(0, sy, -cy, 0);
    bottom.Set(0, sy, cy, 0);
    static const float kEpsilon = 1e-4f;
    if (!(std::fabs(cy) < kEpsilon && std::fabs(sx) < kEpsilon)) {
        float len = std::sqrt(cy * cy + sx * sx);
        len = 1.0f / len;
        cy = cy * len;
        sx = sx * len;
    }
    left.Set(cy, sx, 0, 0);
    right.Set(-cy, sx, 0, 0);
    if (fovY == 0.0f) {
        right.d = 1.0f;
        left.d = 1.0f;
        top.d = ratio;
        bottom.d = ratio;
    }
}

bool operator>(const Sphere &s, const Frustum &f) {
    return s < f.front || s < f.back || s < f.left || s < f.right || s < f.top || s < f.bottom;
}

void Clip(const Hmx::Polygon &poly, const Hmx::Ray &ray, Hmx::Polygon &out) {
    std::vector<Vector2> *newPoints = &out.mPoints;

    float lastDot = ray.dir.x * (poly.mPoints.back().y - ray.base.y)
                  - ray.dir.y * (poly.mPoints.back().x - ray.base.x);
    const Vector2 *lastPoint = &poly.mPoints.back();

    for (const Vector2 *i = poly.mPoints.begin(); i != poly.mPoints.end(); i++) {
        float dot = ray.dir.x * (i->y - ray.base.y) - ray.dir.y * (i->x - ray.base.x);

        if (dot >= 0.0f) {
            if (lastDot < 0.0f) {
                float t = lastDot / (lastDot - dot);
                Vector2 v;
                v.Set(lastPoint->x + t * (i->x - lastPoint->x),
                      lastPoint->y + t * (i->y - lastPoint->y));
                newPoints->push_back(v);
            }
            newPoints->push_back(*i);
        } else {
            if (lastDot >= 0.0f) {
                float t = lastDot / (lastDot - dot);
                Vector2 v;
                v.Set(lastPoint->x + t * (i->x - lastPoint->x),
                      lastPoint->y + t * (i->y - lastPoint->y));
                newPoints->push_back(v);
            }
        }

        lastDot = dot;
        lastPoint = i;
    }
}
