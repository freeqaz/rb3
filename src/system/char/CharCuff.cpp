#include "char/CharCuff.h"
#include "char/FileMerger.h"
#include "decomp.h"
#include "math/Rot.h"
#include "math/Trig.h"
#include "math/Vec.h"
#include "os/Debug.h"
#include "rndobj/Mesh.h"
#include "rndobj/Rnd.h"
#include "rndobj/Trans.h"
#include "utl/Symbols.h"

INIT_REVS(CharCuff)

CharCuff::CharCuff()
    : mOpenEnd(0), mIgnore(this, kObjListNoNull), mBone(this, 0), mEccentricity(1.0f),
      mCategory() {
    mShape[0].offset = -2.9f;
    mShape[0].radius = 1.9f;

    mShape[1].offset = 0.0f;
    mShape[1].radius = 2.6f;

    mShape[2].offset = 2.0f;
    mShape[2].radius = 3.5f;

    mOuterRadius = mShape[1].radius + 0.5f;
}

CharCuff::~CharCuff() {}

float CharCuff::Eccentricity(const Vector2 &v) const {
    float inv_ecc_sq = 1.0f / (mEccentricity * mEccentricity);
    float vy = v.y;
    float vx = v.x;
    float vy2 = vy * vy;
    float vx2 = vx * vx;
    return std::sqrt((vy2 + vx2) / (vy2 * inv_ecc_sq + vx2));
}

// fn_804C3D90 - highlight
void CharCuff::Highlight() {
    Hmx::Color white(1, 1, 1, 1);
    for (int i = 0.0f; i < 2; i++) {
        for (int j = 0; j < 32; j++) {
            float toSine = j * 6.2831855f / 32.0f;
            Vector3 va8(Sine(toSine), Cosine(toSine), mShape[i].offset);
            Vector3 vb4(Sine(toSine), Cosine(toSine), mShape[i + 1].offset);
            (Vector2 &)va8 *= mShape[i].radius * Eccentricity((Vector2 &)va8);
            (Vector2 &)vb4 *= mShape[i + 1].radius * Eccentricity((Vector2 &)vb4);
            Vector3 vc0;
            Multiply(va8, WorldXfm(), vc0);
            Vector3 vcc;
            Multiply(vb4, WorldXfm(), vcc);
            TheRnd->DrawLine(vc0, vcc, white, false);
            if (i < 2) {
                float toSinePlus1 = (j + 1) * 6.2831855f / 32.0f;
                va8.Set(Sine(toSinePlus1), Cosine(toSinePlus1), mShape[i].offset);
                (Vector2 &)va8 *= mShape[i].radius * Eccentricity((Vector2 &)va8);
                Multiply(va8, WorldXfm(), vcc);
                TheRnd->DrawLine(vc0, vcc, white, false);
            }
            if (i == 1) {
                Vector3 vd8(Sine(toSine), Cosine(toSine), mShape[i].offset);
                (Vector2 &)vd8 *= mOuterRadius;
                Multiply(vd8, WorldXfm(), vc0);
                float toSinePlus1 = (j + 1) * 6.2831855f / 32.0f;
                vb4.Set(Sine(toSinePlus1), Cosine(toSinePlus1), mShape[i].offset);
                (Vector2 &)vb4 *= mOuterRadius;
                Multiply(vb4, WorldXfm(), vcc);
                TheRnd->DrawLine(vc0, vcc, white, false);
            }
        }
    }
}

unsigned int BoneMask(std::list<RndTransformable *> &tlist, RndMesh *mesh) {
    unsigned int mask = 0;
    for (int i = 0; i < mesh->NumBones(); i++) {
        if (std::find(tlist.begin(), tlist.end(), mesh->BoneTransAt(i)) != tlist.end()) {
            mask |= 1 << i;
        }
    }
    return mask;
}

void AddBoneChildren(std::list<RndTransformable *> &tlist, RndTransformable *trans) {
    if (trans) {
        if (strncmp(trans->Name(), "bone_", 5) == 0) {
            tlist.push_back(trans);
            for (std::vector<RndTransformable *>::const_iterator it =
                     trans->TransChildren().begin();
                 it != trans->TransChildren().end();
                 ++it) {
                AddBoneChildren(tlist, *it);
            }
        }
    }
}

void CharCuff::Deform(SyncMeshCB *cb, FileMerger *fm) {
    if (mBone) {
        std::list<RndMesh *> meshes;
        for (ObjDirItr<CharCuff> it(Dir(), false); it != nullptr; ++it) {
            if (it != this) {
                if (it->mBone == mBone) {
                    float itRadius = it->mOuterRadius;
                    float thisRadius = mOuterRadius;
                    if (itRadius > thisRadius)
                        return;
                    if (thisRadius == itRadius) {
                        if (strcmp(it->Name(), Name()) > 0)
                            return;
                    }
                    for (ObjPtrList<RndMesh>::iterator iter = it->mIgnore.begin();
                         iter != it->mIgnore.end();
                         ++iter) {
                        meshes.push_back(*iter);
                    }
                }
            }
        }
        FileMerger::Merger *merger = nullptr;
        if (fm) {
            for (int i = 0; i < fm->mMergers.size(); i++) {
                FileMerger::Merger *cur = &fm->mMergers[i];
                if (strstr(cur->mName.mStr, mCategory.mStr)
                    && cur->mLoadedObjects.size() != 0
                    && cur->mLoadedObjects.front()->Dir() == Dir()) {
                    merger = cur;
                    break;
                }
            }
        }
        if (!merger)
            return;
        else {
            std::list<RndTransformable *> transes;
            AddBoneChildren(transes, mBone);
            for (ObjPtrList<Hmx::Object>::iterator it = merger->mLoadedObjects.begin();
                 it != merger->mLoadedObjects.end();
                 ++it) {
                RndMesh *curMesh = dynamic_cast<RndMesh *>(*it);
                if (curMesh) {
                    if (std::find(meshes.begin(), meshes.end(), curMesh)
                        == meshes.end()) {
                        unsigned int mask = BoneMask(transes, curMesh);
                        if (mask != 0) {
                            meshes.push_back(curMesh);
                            DeformMesh(curMesh, mask, cb);
                        }
                    }
                }
            }
        }
    }
}

void CharCuff::DeformMesh(RndMesh *mesh, int boneMask, SyncMeshCB *cb) {
    float ecc_inv_sq = 1.0f / (mEccentricity * mEccentricity);
    int called = 0;
    MILO_ASSERT(mesh->mGeomOwner, 0xAB);
    RndMesh *geomOwner = mesh->mGeomOwner;
    Transform xfm;
    if (TransParent() && TransParent()->Name() != Dir()->Name()) {
        MILO_ASSERT(mesh->NumBones(), 0xF5);
        if (!mesh->NumBones()) return;
        Transform sp50;
        FastInvert(mesh->BoneTransAt(0)->WorldXfm(), sp50);
        Multiply(WorldXfm(), sp50, sp50);
        FastInvert(mesh->BoneOffsetAt(0), xfm);
        Multiply(sp50, xfm, xfm);
    } else {
        xfm = mLocalXfm;
    }
    float axisY = xfm.m.z.y;
    float axisZ = xfm.m.z.z;
    float planeDist = axisZ * xfm.v.z + (xfm.m.z.x * xfm.v.x + axisY * xfm.v.y);
    float planef = (float)(-planeDist);

    RndMesh::VertVector &verts = geomOwner->mVerts;
    int numVerts = verts.size();
    for (int i = 0; i < numVerts; i++) {
        RndMesh::Vert &vert = verts[i];
        const Vector4_16_01 &bw = vert.boneWeights;
        int m0 = (bw.GetW() > 0.0f) ? (1 << vert.boneIndices[0]) : 0;
        int m1 = (bw.GetX() > 0.0f) ? (1 << vert.boneIndices[1]) : 0;
        int m01 = m0 | m1;
        int m2 = (bw.GetY() > 0.0f) ? (1 << vert.boneIndices[2]) : 0;
        int m012 = m01 | m2;
        int m3 = (bw.GetZ() > 0.0f) ? (1 << vert.boneIndices[3]) : 0;
        if (!((m012 | m3) & boneMask))
            continue;
        float axisX = xfm.m.z.x;
        float axisCoord = planef + ((axisZ * vert.pos.z) + ((axisX * vert.pos.x) + (axisY * vert.pos.y)));
        if (axisCoord < mShape[2].offset) {
        float projY = axisY * axisCoord + xfm.v.y;
        float projX = xfm.m.z.x * axisCoord + xfm.v.x;
        float dy = vert.pos.y - projY;
        float projZ = axisZ * axisCoord + xfm.v.z;
        float dx = vert.pos.x - projX;
        float dz = vert.pos.z - projZ;
        float f6 = (dz * xfm.m.x.z) + ((dx * xfm.m.x.x) + (dy * xfm.m.x.y));
        float f3 = (dz * xfm.m.y.z) + ((dx * xfm.m.y.x) + (dy * xfm.m.y.y));
        float f6sq = f6 * f6;
        float f3sq = f3 * f3;
        float distSq = (dx * dx + dy * dy + dz * dz) * ((f3sq * ecc_inv_sq + f6sq) / (f6sq + f3sq));
        if (axisCoord < mShape[0].offset) {
            if (!mOpenEnd) {
                if (called == 0) {
                    cb->SyncMesh(mesh, 0xBF);
                    called = 1;
                }
                float t = mShape[0].offset;
                vert.pos.x = xfm.m.z.x * t + xfm.v.x;
                vert.pos.y = xfm.m.z.y * t + xfm.v.y;
                vert.pos.z = xfm.m.z.z * t + xfm.v.z;
                float scale = mShape[0].radius / std::sqrt(distSq);
                vert.pos.x += dx * scale;
                vert.pos.y += dy * scale;
                vert.pos.z += dz * scale;
            }
        } else {
            float var_f19;
            if (axisCoord < mShape[1].offset) {
                float t = (axisCoord - mShape[1].offset) / (mShape[0].offset - mShape[1].offset);
                var_f19 = (t * (mShape[0].radius - mShape[1].radius)) + mShape[1].radius;
            } else {
                float t = (axisCoord - mShape[2].offset) / (mShape[1].offset - mShape[2].offset);
                var_f19 = (t * (mShape[1].radius - mShape[2].radius)) + mShape[2].radius;
            }
            if (var_f19 * var_f19 < distSq) {
                if (called == 0) {
                    called = 1;
                    cb->SyncMesh(mesh, 0xBF);
                }
                float scale = var_f19 / std::sqrt(distSq);
                vert.pos.x = dx * scale + projX;
                vert.pos.y = dy * scale + projY;
                vert.pos.z = dz * scale + projZ;
            }
        }
        } // end if (axisCoord < mShape[2].offset)
    }
    if (!mOpenEnd) {
        MILO_ASSERT(mesh->mGeomOwner, 0xAB);
        RndMesh *go = mesh->mGeomOwner;
        const float kTolerance = 0.01f;
        std::vector<RndMesh::Face> &faces = go->mFaces;
        int faceCount = faces.size() - 1;
        float faceAxisX = xfm.m.z.x;
        for (int fi = 0; fi <= faceCount; fi++) {
            int pass = 0;
            RndMesh::Face *facep = &faces[fi];
            u16 *faceVerts = &facep->v1;
            for (int vi = 0; vi < 3; vi++) {
                RndMesh::Vert &v0 = verts[faceVerts[vi]];
                int bm32 = (1 << v0.boneIndices[3]) | (1 << v0.boneIndices[2]);
                int bm01 = (1 << v0.boneIndices[0]) | (1 << v0.boneIndices[1]);
                int bm = bm32 | bm01;
                if (!(bm & boneMask)) break;
                float ac2 = axisY * v0.pos.y;
                ac2 = faceAxisX * v0.pos.x + ac2;
                ac2 = axisZ * v0.pos.z + ac2;
                ac2 = planef + ac2;
                if (ac2 > kTolerance + mShape[0].offset) break;
                pass++;
            }
            if (pass == 3) {
                if (called == 0) {
                    cb->SyncMesh(mesh, 0xBF);
                    called = 1;
                }
                *facep = faces[faceCount];
                faceCount--;
                fi--;
            }
        }
        // resize faces
        RndMesh::Face zero;
        zero.v1 = 0; zero.v2 = 0; zero.v3 = 0;
        int newSize = faceCount + 1;
        if ((unsigned)newSize < faces.size()) {
            faces.erase(faces.begin() + newSize, faces.end());
        } else {
            faces.insert(faces.end(), newSize - (int)faces.size(), zero);
        }
    }
}

SAVE_OBJ(CharCuff, 0x1A2)

DECOMP_FORCEACTIVE(CharCuff, "ObjPtr_p.h", "c.Owner()", "", "f.Owner()")

BEGIN_LOADS(CharCuff)
    LOAD_REVS(bs)
    ASSERT_REVS(8, 0)
    LOAD_SUPERCLASS(Hmx::Object)
    LOAD_SUPERCLASS(RndTransformable)
    for (int i = 0; i < 3; i++) {
        bs >> mShape[i].radius >> mShape[i].offset;
    }
    if (gRev > 1)
        bs >> mOuterRadius;
    else
        mOuterRadius = mShape[1].radius + 0.5f;
    if (gRev > 2)
        bs >> mOpenEnd;
    else
        mOpenEnd = false;
    if (gRev > 3)
        bs >> mBone;
    else
        mBone = TransParent();
    if (gRev > 4)
        bs >> mEccentricity;
    else
        mEccentricity = 1.0f;
    if (gRev > 5)
        bs >> mCategory;
    else
        mCategory = Symbol("");
    if (gRev > 7)
        bs >> mIgnore;
    if (gRev < 7)
        MILO_WARN("%s old CharCuff, must convert, see James", PathName(this));
END_LOADS

BEGIN_COPYS(CharCuff)
    COPY_SUPERCLASS(Hmx::Object)
    COPY_SUPERCLASS(RndTransformable)
    CREATE_COPY(CharCuff)
    BEGIN_COPYING_MEMBERS
        memcpy(mShape, c->mShape, 0x18);
        COPY_MEMBER(mOuterRadius)
        COPY_MEMBER(mOpenEnd)
        COPY_MEMBER(mBone)
        COPY_MEMBER(mEccentricity)
        COPY_MEMBER(mCategory)
        COPY_MEMBER(mIgnore)
    END_COPYING_MEMBERS
END_COPYS

BEGIN_HANDLERS(CharCuff)
    HANDLE_SUPERCLASS(RndTransformable)
    HANDLE_SUPERCLASS(Hmx::Object)
    HANDLE_CHECK(0x1FE)
END_HANDLERS

BEGIN_PROPSYNCS(CharCuff)
    SYNC_PROP(offset0, mShape[0].offset)
    SYNC_PROP(radius0, mShape[0].radius)
    SYNC_PROP(offset1, mShape[1].offset)
    SYNC_PROP(radius1, mShape[1].radius)
    SYNC_PROP(offset2, mShape[2].offset)
    SYNC_PROP(radius2, mShape[2].radius)
    SYNC_PROP(outer_radius, mOuterRadius)
    SYNC_PROP(open_end, mOpenEnd)
    SYNC_PROP(bone, mBone)
    SYNC_PROP(eccentricity, mEccentricity)
    SYNC_PROP(category, mCategory)
    SYNC_PROP(ignore, mIgnore)
    SYNC_SUPERCLASS(RndTransformable)
END_PROPSYNCS