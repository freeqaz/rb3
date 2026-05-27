#include "world/Spotlight.h"
#include "char/Character.h"
#include "decomp.h"
#include "math/Mtx.h"
#include "obj/Object.h"
#include "obj/Task.h"
#include "os/Debug.h"
#include "os/System.h"
#include "os/Timer.h"
#include "rndobj/Cam.h"
#include "rndobj/Env.h"
#include "rndobj/Mat.h"
#include "rndobj/Flare.h"
#include "rndobj/Lit.h"
#include "rndobj/Mesh.h"
#include "rndobj/Group.h"
#include "rndobj/Rnd.h"
#include "rndobj/Trans.h"
#include "utl/Loader.h"
#include "world/SpotlightDrawer.h"
#include "world/LightPreset.h"
#include "math/Rot.h"
#include "utl/Symbols.h"

RndEnviron *Spotlight::sEnviron;
RndMesh *Spotlight::sDiskMesh;

INIT_REVS(Spotlight)

Vector2 Spotlight::BeamDef::NGRadii() const {
    float exp = mExpand;
    float f1 = mTopRadius * exp;
    float f2 = mBottomRadius * exp;
    if (!mIsCone) {
        f2 = f2 * -(mBottomSideBorder * 0.7f - 1.0f);
        f1 = f1 * -(mTopSideBorder * 0.7f - 1.0f);
    }
    return Vector2(f1, f2);
}

void Spotlight::Init() {
    Register();
    sEnviron = Hmx::Object::New<RndEnviron>();
    BuildBoard();
}

Spotlight::Spotlight()
    : mDiscMat(this), mFlare(Hmx::Object::New<RndFlare>()), mFlareOffset(0.0f),
      mSpotScale(30.0f), mSpotHeight(0.25f), mColor(-1), mIntensity(1.0f),
      mColorOwner(this, this), mLensSize(0.0f), mLensOffset(0.0f), mLensMaterial(this),
      mBeam(this), mSlaves(this), mLightCanOffset(0.0f), mLightCanMesh(this),
      mTarget(this), mSpotTarget(this), unk22c(-1e+33f), mDampingConstant(1.0f),
      mAdditionalObjects(this), mFlareEnabled(1), mFlareVisibilityTest(1), unk286(1),
      mTargetShadow(0), mLightCanSort(0), unk289(1), mAnimateColorFromPreset(1),
      mAnimateOrientationFromPreset(1), unk28c(0) {
    mFlare->SetTransParent(this, false);
    mFloorSpotXfm.Reset();
    mLensXfm.Reset();
    mLightCanXfm.Reset();
    unk230.Identity();
    unk268.Zero();
    unk274.Reset();
    SetOrder(-1000.0f);
}

Spotlight::~Spotlight() {
    CloseSlaves();
    SpotlightDrawer::RemoveFromLists(this);
    RELEASE(mFlare);
}

void Spotlight::SetFlareEnabled(bool b) {
    mFlareEnabled = b;
    UpdateFlare();
}

void Spotlight::SetFlareIsBillboard(bool b) {
    mFlareVisibilityTest = b;
    UpdateFlare();
}

void Spotlight::UpdateFlare() {
    if (!mFlareEnabled) {
        mFlare->SetVisible(false);
        mFlare->SetPointTest(false);
    } else if (mFlareVisibilityTest) {
        mFlare->SetVisible(true);
        mFlare->SetPointTest(false);
    } else
        mFlare->SetPointTest(true);
}

void Spotlight::ConvertGroupToMesh(RndGroup *grp) {
    if (grp) {
        int count = 0;
        std::vector<RndDrawable *>::iterator it = grp->mDraws.begin();
        std::vector<RndDrawable *>::iterator itEnd = grp->mDraws.end();
        for (; it != itEnd; it++) {
            RndMesh *cur = dynamic_cast<RndMesh *>(*it);
            if (cur) {
                if (cur) {
                count++;
                if (!mLightCanMesh)
                    mLightCanMesh = cur;
            }
            }
        }
        if (count > 1) {
            MILO_WARN(
                "Multiple meshes (%d) found converting light can group %s to mesh",
                count,
                grp->Name()
            );
        }
    }
}

BEGIN_LOADS(Spotlight)
    LOAD_REVS(bs)
    ASSERT_REVS(0x21, 0)
    if (gRev < 9) {
        MILO_FAIL("Unsupported spotlight version");
        return;
    }
    LOAD_SUPERCLASS(Hmx::Object)
    LOAD_SUPERCLASS(RndDrawable)
    LOAD_SUPERCLASS(RndTransformable)
    bs >> mSpotScale;
    bs >> mSpotHeight;
    if (gRev > 0x16)
        bs >> mBeam;
    else {
        ObjVector<BeamDef> beams(this);
        bs >> beams;
        MILO_ASSERT(beams.size() <= 1, 0xC9);
        if (beams.size() != 0)
            mBeam = beams[0];
        else
            mBeam.mLength = 0;
    }
    if (gRev > 0x15)
        bs >> mLightCanMesh;
    else {
        ObjPtr<RndGroup> grpPtr(this);
        bs >> grpPtr;
        ConvertGroupToMesh(grpPtr);
    }
    if (!mTarget.Load(bs, false, 0))
        unk286 = false;
    if (gRev > 0x1C)
        bs >> mSpotTarget;
    bs >> mLightCanOffset;
    if (gRev > 0x1E)
        bs >> mLightCanSort;
    bs >> mColor;
    mColor.SetAlpha(1.0f);
    if (gRev > 9)
        bs >> mIntensity;
    bs >> mDiscMat;
    if (gRev == 0x12) {
        char buf[0x80];
        bs.ReadString(buf, 0x80);
        if (!mDiscMat && buf[0] != '\0') {
            mDiscMat = LookupOrCreateMat(buf, Dir());
        }
    }
    bs >> mDampingConstant;
    if (gRev < 0x21) {
        Symbol sym;
        bs >> sym;
    }
    if (gRev > 10) {
        ObjPtr<RndMat> matPtr(this);
        bs >> matPtr;
        mFlare->SetMat(matPtr);
        if (gRev > 0x11 && gRev < 0x13) {
            char buf[0x80];
            bs.ReadString(buf, 0x80);
            if (!matPtr && buf[0] != '\0') {
                matPtr = LookupOrCreateMat(buf, Dir());
                mFlare->SetMat(matPtr);
            }
        }
        bs >> mFlare->mSizes;
        bs >> mFlare->mRange;
        int steps;
        bs >> steps;
        mFlare->SetSteps(steps);
        bs >> mFlareOffset;
    }
    if (gRev > 0xD)
        bs >> mFlareEnabled;
    if (gRev > 0xE)
        bs >> mFlareVisibilityTest;
    UpdateFlare();
    if (gRev > 0xB) {
        bs >> mLensSize;
        bs >> mLensOffset;
        bs >> mLensMaterial;
    }
    if (gRev == 0x12) {
        char buf[0x80];
        bs.ReadString(buf, 0x80);
        if (!mLensMaterial && buf[0] != '\0') {
            mLensMaterial = LookupOrCreateMat(buf, Dir());
        }
    }
    if (gRev > 0xC)
        bs >> mAdditionalObjects;
    if (gRev > 0x1B)
        bs >> mSlaves;
    if (gRev > 0xF)
        bs >> mTargetShadow;
    if (gRev > 0x19) {
        bs >> mAnimateColorFromPreset;
        bs >> mAnimateOrientationFromPreset;
    } else if (gRev > 0x10) {
        bs >> mAnimateColorFromPreset;
        mAnimateOrientationFromPreset = mAnimateColorFromPreset;
    }
    if (gRev > 0x1D) {
        bs >> mColorOwner;
        if (!mColorOwner)
            mColorOwner = this;
    }
    Generate();
END_LOADS

void Spotlight::Replace(Hmx::Object *from, Hmx::Object *to) {
    RndTransformable::Replace(from, to);
    if (mColorOwner == from) {
        mColorOwner = dynamic_cast<Spotlight *>(to);
    }
    if (!mColorOwner) {
        mColorOwner = this;
    }
}

SAVE_OBJ(Spotlight, 0x16D)

void Spotlight::BeamDef::Load(BinStream &bs) {
    bs >> mIsCone;
    bs >> mLength;
    bs >> mBottomRadius;
    bs >> mTopRadius;
    bs >> mTopSideBorder;
    bs >> mBottomSideBorder;
    bs >> mBottomBorder;
    bs >> mMat;
    if (gRev == 0x12) {
        char buf[0x80];
        bs.ReadString(buf, 0x80);
    }
    bs >> mOffset;
    if (gRev < 10) {
        Hmx::Color col;
        bs >> col;
    }
    bs >> mTargetOffset;
    if (gRev > 0x14) {
        bs >> mBrighten;
        bs >> mXSection;
    }
    if (gRev > 0x17)
        bs >> mExpand;
    if (gRev > 0x1A)
        bs >> mShape;
    if (gRev > 0x18)
        bs >> mCutouts;
    if (gRev > 0x1F) {
        bs >> mNumSections;
        bs >> mNumSegments;
    }
}

BEGIN_COPYS(Spotlight)
    COPY_SUPERCLASS(Hmx::Object)
    COPY_SUPERCLASS(RndTransformable)
    COPY_SUPERCLASS(RndDrawable)
    CREATE_COPY(Spotlight)
    BEGIN_COPYING_MEMBERS
        if (ty != kCopyFromMax) {
            mFlare->Copy(c->mFlare, kCopyDeep);
            COPY_MEMBER(mFlareOffset)
            COPY_MEMBER(mLightCanMesh)
            COPY_MEMBER(mTarget)
            COPY_MEMBER(mSpotTarget)
            COPY_MEMBER(mSpotScale)
            COPY_MEMBER(mSpotHeight)
            SetColorIntensity(c->Color(), c->Intensity());
            COPY_MEMBER(mDiscMat)
            COPY_MEMBER(mDampingConstant)
            COPY_MEMBER(mLensSize)
            COPY_MEMBER(mLensOffset)
            COPY_MEMBER(mLensMaterial)
            COPY_MEMBER(mLightCanOffset)
            COPY_MEMBER(mLightCanSort)
            COPY_MEMBER(mFlareEnabled)
            COPY_MEMBER(mFlareVisibilityTest)
            UpdateFlare();
            COPY_MEMBER(mTargetShadow)
            COPY_MEMBER(mAnimateColorFromPreset)
            COPY_MEMBER(mAnimateOrientationFromPreset)
            COPY_MEMBER(mAdditionalObjects)
            COPY_MEMBER(mSlaves)
            COPY_MEMBER(mBeam.mIsCone)
            COPY_MEMBER(mBeam.mLength)
            COPY_MEMBER(mBeam.mBottomRadius)
            COPY_MEMBER(mBeam.mTopRadius)
            COPY_MEMBER(mBeam.mTopSideBorder)
            COPY_MEMBER(mBeam.mBottomSideBorder)
            COPY_MEMBER(mBeam.mBottomBorder)
            COPY_MEMBER(mBeam.mMat)
            COPY_MEMBER(mBeam.mTargetOffset)
            COPY_MEMBER(mBeam.mBrighten)
            COPY_MEMBER(mBeam.mExpand)
            COPY_MEMBER(mBeam.mShape)
            COPY_MEMBER(mBeam.mXSection)
            COPY_MEMBER(mBeam.mCutouts)
            COPY_MEMBER(mBeam.mOffset)
            COPY_MEMBER(mBeam.mNumSections)
            COPY_MEMBER(mBeam.mNumSegments)
            if (c->mBeam.mBeam) {
                mBeam.mBeam = Hmx::Object::New<RndMesh>();
                mBeam.mBeam->Copy(c->mBeam.mBeam, kCopyDeep);
            }
        }
    END_COPYING_MEMBERS
END_COPYS

void Spotlight::ListDrawChildren(std::list<RndDrawable *> &draws) {
    RndMesh *lightCanMesh = mLightCanMesh;
    if (lightCanMesh)
        draws.push_back(lightCanMesh);
    FOREACH (it, mAdditionalObjects) {
        draws.push_back(*it);
    }
}

RndDrawable *Spotlight::CollideShowing(const Segment &s, float &f, Plane &pl) {
    if (mLightCanMesh) {
        mLightCanMesh->SetWorldXfm(mLightCanXfm);
        bool oldshowing = mLightCanMesh->Showing();
        mLightCanMesh->SetShowing(true);
        bool coll = mLightCanMesh->Collide(s, f, pl);
        mLightCanMesh->SetShowing(oldshowing);
        if (coll)
            return this;
    }
    return 0;
}

int Spotlight::CollidePlane(const Plane &pl) {
    if (mLightCanMesh) {
        mLightCanMesh->SetWorldXfm(mLightCanXfm);
        bool oldshowing = mLightCanMesh->Showing();
        mLightCanMesh->SetShowing(true);
        int coll = mLightCanMesh->CollidePlane(pl);
        mLightCanMesh->SetShowing(oldshowing);
        if (coll)
            return coll;
    }
    return -1;
}

void Spotlight::Poll() {
    if (!LOADMGR_EDITMODE && (!Showing() || mIntensity == 0))
        return;
    Hmx::Matrix3 m38;
    if (!unk28c) {
        RndTransformable *target = ResolveTarget();
        if (!target || (!LOADMGR_EDITMODE && !unk289 && target->WorldXfm().v == unk268)) {
            if (!target && !mAnimateOrientationFromPreset && !DoFloorSpot()) {
                UpdateTransforms();
                return;
            }
            CheckFloorSpotTransform();
            unk230 = WorldXfm().m;
            UpdateSlaves();
            return;
        }
        unk268 = target->WorldXfm().v;
        CalculateDirection(target, m38);
        if (!unk289 && mDampingConstant != 1) {
            Interp(unk230, m38, mDampingConstant * TheTaskMgr.DeltaSeconds(), m38);
        } else
            unk289 = false;
    } else
        MakeRotMatrix(unk274, m38);
    SetLocalRot(m38);
    unk230 = m38;
    UpdateTransforms();
    unk28c = false;
}

void Spotlight::CloseSlaves() {
    FOREACH (it, mSlaves) {
        RndLight *lit = *it;
        if (lit)
            lit->SetShadowOverride(0);
    }
}

void Spotlight::UpdateSlaves() {
    bool isEmpty = mSlaves.empty();
    if (isEmpty)
        return;
    ObjPtrList<RndLight>::iterator it = mSlaves.begin();
    ObjPtrList<RndLight>::iterator itEnd = mSlaves.end();
    for (; it != itEnd; ++it) {
        RndLight *lit = *it;
        Transform tf40(WorldXfm());
        if (lit->TransParent()) {
            Transform tf70;
            Invert(lit->TransParent()->WorldXfm(), tf70);
            Multiply(WorldXfm(), tf70, tf40);
        }
        lit->SetLocalXfm(tf40);
        lit->SetShadowOverride(&mBeam.mCutouts);
        lit->SetShowing(Showing());
    }
}

#pragma fp_contract off
void Spotlight::CalculateDirection(RndTransformable *target, Hmx::Matrix3 &mtx) {
    MILO_ASSERT(target, 0x2C3);
    Vector3 v20;
    Subtract(target->WorldXfm().v, WorldXfm().v, v20);
    Vector3 v2c;
    Cross(v20, Vector3(1.0f, 0.0f, 0.0f), v2c);
    Normalize(v2c, v2c);
    MakeRotMatrix(v20, v2c, mtx);
}
#pragma fp_contract on

void Spotlight::UpdateSphere() {
    Sphere s48;
    MakeWorldSphere(s48, true);
    Transform tf38;
    FastInvert(WorldXfm(), tf38);
    Multiply(s48, tf38, s48);
    SetSphere(s48);
}

bool Spotlight::MakeWorldSphere(Sphere &s, bool b) {
    if (b) {
        s.Zero();
        if (mBeam.mBeam) {
            Sphere s28;
            if (mBeam.mBeam->MakeWorldSphere(s28, true)) {
                s.GrowToContain(s28);
            }
        }
        if (DoFloorSpot()) {
            MILO_ASSERT(sDiskMesh, 0x2F2);
            Sphere s38;
            sDiskMesh->SetWorldXfm(mFloorSpotXfm);
            if (sDiskMesh->MakeWorldSphere(s38, true)) {
                s.GrowToContain(s38);
            }
        }
        if (mFlare) {
            Sphere s48;
            if (mFlare->MakeWorldSphere(s48, true)) {
                s.GrowToContain(s48);
            }
        }
        if (mLightCanMesh) {
            Sphere s58;
            mLightCanMesh->SetWorldXfm(mLightCanXfm);
            if (mLightCanMesh->MakeWorldSphere(s58, true)) {
                s.GrowToContain(s58);
            }
        }
        return true;
    } else if (mSphere.GetRadius()) {
        Multiply(mSphere, WorldXfm(), s);
        return true;
    } else
        return false;
}

void Spotlight::SetColorIntensity(const Hmx::Color &col, float f) {
    Hmx::Color c20(col);
    Multiply(c20, f, c20);
    Color();
    Intensity();
    mColorOwner->mColor = Hmx::Color32(col.Pack());
    mColorOwner->mIntensity = f;
}

void Spotlight::SetColor(int packed) {
    Hmx::Color color;
    color.Unpack(packed);
    color.alpha = 1.0f;
    SetColorIntensity(color, Intensity());
}

void Spotlight::SetIntensity(float f) { SetColorIntensity(Hmx::Color(Color()), f); }

void Spotlight::UpdateBounds() {
    UpdateTransforms();
    UpdateSphere();
}

void Spotlight::UpdateTransforms() {
    START_AUTO_TIMER("spotlight_xfm");
    Transform &thetf = WorldXfm();
    mLightCanXfm = thetf;
    Vector3 vcc(mLightCanXfm.m.y);
    vcc *= mLightCanOffset;
    ::Add(mLightCanXfm.v, vcc, mLightCanXfm.v);
    static Hmx::Matrix3 ident(
        Vector3(1.0f, 0.0f, 0.0f), Vector3(0.0f, 1.0f, 0.0f), Vector3(0.0f, 0.0f, 1.0f)
    );
    static Hmx::Matrix3 rot(
        Vector3(1.0f, 0.0f, 0.0f), Vector3(0.0f, 0.0f, 1.0f), Vector3(0.0f, -1.0f, 0.0f)
    );
    if (mLensMaterial) {
        Vector3 vd8(0.0f, mLensOffset, 0.0f);
        Multiply(vd8, thetf.m, vd8);
        ::Add(vd8, thetf.v, vd8);
        Hmx::Matrix3 m48;
        m48.Set(
            Vector3(-mLensSize, 0.0f, 0.0f),
            Vector3(0.0f, 0.0f, mLensSize),
            Vector3(0.0f, mLensSize, 0.0f)
        );
        Multiply(m48, thetf.m, m48);
        mLensXfm = Transform(m48, vd8);
    }
    if (mBeam.mBeam) {
        Vector3 ve4(0.0f, mBeam.mOffset, 0.0f);
        mBeam.mBeam->SetLocalPos(ve4);
        Hmx::Matrix3 m6c(mBeam.mIsCone ? rot : ident);
        Hmx::Matrix3 m90;
        MakeRotMatrix(
            Vector3(
                mBeam.mTargetOffset.x * DEG2RAD, 0.0f, mBeam.mTargetOffset.y * DEG2RAD
            ),
            m90,
            true
        );
        Multiply(m6c, m90, m6c);
        mBeam.mBeam->SetLocalRot(m6c);
    }
    if (mFlare && mFlare->GetMat()) {
        Vector3 vf0(0.0f, mFlareOffset, 0.0f);
        mFlare->SetLocalPos(vf0);
        mFlare->SetLocalRot(ident);
    }
    UpdateFloorSpotTransform(thetf);
    UpdateSlaves();
}

void Spotlight::CheckFloorSpotTransform() {
    if (DoFloorSpot()) {
        if (GetFloorSpotTarget()->WorldXfm().v.z != unk22c) {
            UpdateFloorSpotTransform(WorldXfm());
        }
    }
}

void Spotlight::UpdateFloorSpotTransform(const Transform &tf) {
    mFloorSpotXfm.Reset();
    if (DoFloorSpot()) {
        float f1 = GetFloorSpotTarget()->WorldXfm().v.z;
        Vector3 vac(tf.m.y);
        if (vac.z != 0) {
            float absed = std::fabs(((f1 - tf.v.z) / vac.z) / (f1 - tf.v.z));
            vac = tf.m.y;
            float curz = vac.z;
            vac.z = 0;
            Hmx::Matrix3 m70;
            if (curz > -0.9999999f && curz < 0.9999999f)
                MakeRotMatrix(vac, Vector3(0.0f, 0.0f, 1.0f), m70);
            else
                m70.Identity();
            vac.Set(mSpotScale, mSpotScale * absed, 1.0f);
            Scale(vac, m70, m70);
            float scalar = (f1 + mSpotHeight - tf.v.z) / curz;
            vac = tf.m.y;
            vac *= scalar;
            ::Add(vac, tf.v, vac);
            mFloorSpotXfm = Transform(m70, vac);
        }
        unk22c = f1;
    }
}

#pragma push
#pragma auto_inline on
void Spotlight::DrawShowing() {
    START_AUTO_TIMER("spotlight");
    ObjPtr<RndMesh> &_ref0 = mLightCanMesh;
    if (LightCanSort() && _ref0) {
        _ref0->SetWorldXfm(mLightCanXfm);
        Sphere s(_ref0->mSphere);
        if (s.GetRadius() >= 1) {
            Multiply(s, mLightCanXfm, s);
            if (!RndCam::sCurrent->CompareSphereToWorld(s)) {
                _ref0->DrawShowing();
            }
        }
    }
    if (TheRnd->DrawMode() == 0) {
        SpotlightDrawer::DrawLight(this);
        return;
    }
    if (!unk286) return;
    {
        UpdateTransforms();
        Hmx::Color c48(Color());
        Multiply(c48, Intensity(), c48);
        sEnviron->SetAmbientColor(c48);
        RndEnvironTracker tracker(sEnviron, nullptr);
        FOREACH (it, mAdditionalObjects) {
            MILO_ASSERT(*it != this, 0x3D8);
            if (*it != this)
                (*it)->DrawShowing();
        }
        if (mLensMaterial) {
            MILO_ASSERT(sDiskMesh, 0x3E2);
            sDiskMesh->SetWorldXfm(mLensXfm);
            sDiskMesh->SetMat(mLensMaterial);
            sDiskMesh->DrawShowing();
        }
        if (mBeam.mBeam && TheRnd->DrawMode() != 4) {
            mBeam.mBeam->DrawShowing();
        }
        if (mFlare && mFlare->GetMat()) {
            mFlare->Draw();
        }
        if (mTarget) {
            if (mTargetShadow) {
                Character *theChar = dynamic_cast<Character *>(mTarget.Ptr());
                if (theChar) {
                    Vector3 v58 = theChar->WorldXfm().v;
                    v58.z += 3.0f;
                    Plane p68(v58, Vector3(0.0f, 0, 1));
                    theChar->DrawShadow(WorldXfm(), p68);
                }
            }
            if (DoFloorSpot()) {
                MILO_ASSERT(sDiskMesh, 0x408);
                sDiskMesh->SetWorldXfm(mFloorSpotXfm);
                sDiskMesh->SetMat(mDiscMat);
                sDiskMesh->DrawShowing();
            }
        }
    }
}
#pragma pop

void Spotlight::Generate() {
    if (!mBeam.mBeam || LOADMGR_EDITMODE) {
        RELEASE(mBeam.mBeam);
        if (mBeam.mLength > 0) {
            if (SpotlightDrawer::DrawNGSpotlights()) {
                BuildNGShaft(mBeam);
            } else {
                BuildShaft(mBeam);
            }
        }
        UpdateBounds();
        UpdateSphere();
    }
}

void Spotlight::BuildBoard() {
    MILO_ASSERT(!sDiskMesh, 0x427);
    sDiskMesh = Hmx::Object::New<RndMesh>();
    RndMesh::VertVector &verts = sDiskMesh->Verts();
    std::vector<RndMesh::Face> &faces = sDiskMesh->Faces();
    verts.resize(4, true);
    faces.resize(2);

    verts[0].pos.Set(-0.5, -0.5, 0);
    verts[0].color.Clear();
    verts[0].uv.Set(0, 0);

    verts[1].pos.Set(0.5, -0.5, 0);
    verts[1].color.Clear();
    verts[1].uv.Set(1, 0);

    verts[2].pos.Set(-0.5, 0.5, 0);
    verts[2].color.Clear();
    verts[2].uv.Set(0, 1);

    verts[3].pos.Set(0.5, 0.5, 0);
    verts[3].color.Clear();
    verts[3].uv.Set(1, 1);

    faces[0].Set(0, 1, 2);
    faces[1].Set(1, 3, 2);
    sDiskMesh->Sync(0x13F);
    sDiskMesh->UpdateSphere();
}

DECOMP_FORCEACTIVE(
    Spotlight,
    "iVert == kNumVerts",
    "iFace == kNumFaces"
)

void Spotlight::BuildNGShaft(Spotlight::BeamDef &def) {
    switch (def.mShape) {
    case 1:
        BuildNGCone(def, 4);
        return;
    case 2:
        BuildNGSheet(def);
        return;
    case 3:
        BuildNGQuad(def, RndTransformable::kBillboardXYZ);
        return;
    case 4:
        BuildNGQuad(def, RndTransformable::kBillboardZ);
        return;
    default:
        int num = 10;
        if (def.mNumSegments > 3) {
            num = def.mNumSegments;
        }
        BuildNGCone(def, num);
        return;
    }
}

void Spotlight::BuildShaft(Spotlight::BeamDef &def) {
    if (def.mIsCone)
        BuildCone(def);
    else
        BuildBeam(def);
}

void Spotlight::BuildBeam(BeamDef &def) {
    MILO_ASSERT(!SpotlightDrawer::DrawNGSpotlights(), 0x5F3);
    def.mIsCone = false;
    def.mBeam = Hmx::Object::New<RndMesh>();
    RndMesh::VertVector &verts = def.mBeam->Verts();
    std::vector<RndMesh::Face> &faces = def.mBeam->Faces();
    float bottomBorderLen = def.mBottomBorder * def.mLength;
    float topSideBorderVal = def.mTopSideBorder * def.mTopRadius;
    float bottomSideBorderVal = def.mBottomSideBorder * def.mBottomRadius;

    int numSectionsTop = 4;
    {
        int rawTop = (int)((def.mLength - bottomBorderLen) / 15.0f);
        if (rawTop > 4) numSectionsTop = rawTop;
    }

    int numSectionsBottom = 1;
    {
        int rawBot = (int)(bottomBorderLen / 15.0f);
        if (rawBot > 1) numSectionsBottom = rawBot;
    }

    int totalSections = numSectionsBottom + numSectionsTop;

    verts.resize(totalSections * 4, true);
    faces.resize(totalSections * 6);

    float topLen = def.mLength - bottomBorderLen;
    float topRadius = def.mTopRadius;
    float borderTopRadius = (topLen / def.mLength) * (def.mBottomRadius - topRadius) + topRadius;
    float radiusStepTopVal = (borderTopRadius - topRadius) / (float)numSectionsTop;
    float radiusStepBotVal = (def.mBottomRadius - borderTopRadius) / (float)numSectionsBottom;
    float halfWidth = topRadius;
    float sideBorderDiff = bottomSideBorderVal - topSideBorderVal;
    int fi = 0;
    int c0 = 0;
    for (unsigned int i = 0; i < (unsigned int)totalSections; i++) {
        int c1 = c0 + 1;
        int c2 = c0 + 2;
        int c3 = c0 + 3;
        int n0 = c0 + 4;
        int n1 = n0 + 1;
        int n2 = n0 + 2;
        int n3 = n0 + 3;
        float y;
        float alpha;
        if (i == (unsigned int)totalSections - 1) {
            y = def.mLength;
            alpha = 0.0f;
        } else if (i >= (unsigned int)numSectionsTop) {
            int lVar31 = i - numSectionsTop;
            y = (float)lVar31 * (bottomBorderLen / (float)numSectionsBottom) + topLen;
            alpha = 1.0f - (float)lVar31 / (float)numSectionsBottom;
        } else {
            y = (float)i * (topLen / (float)numSectionsTop);
            alpha = 1.0f;
        }

        float yFrac = y / def.mLength;
        float negY = -y;
        float sideBorder = sideBorderDiff * yFrac + topSideBorderVal;
        float borderRatio = sideBorder / (halfWidth * 2.0f);

        // Column 0: left edge
        verts[c0].pos.x = -halfWidth;
        verts[c0].pos.y = 0.0f;
        verts[c0].pos.z = negY;
        verts[c0].color.color = 0;
        verts[c0].uv.Set(0.0f, yFrac);

        float leftInner = sideBorder - halfWidth;

        // Column 1: left inner
        if (-leftInner < 0.0f) leftInner = 0.0f;
        verts[c1].pos.x = leftInner;
        verts[c1].pos.y = 0.0f;
        verts[c1].pos.z = negY;
        {
            int iAlpha = (int)(alpha * 255.0f);
            int alphaColor = ((iAlpha & 0xFF) << 24) | ((iAlpha & 0xFF) << 16) | ((iAlpha & 0xFF) << 8) | (iAlpha & 0xFF);
            verts[c1].color.color = alphaColor;
            verts[c2].color.color = alphaColor;
        }
        verts[c1].uv.Set(borderRatio, yFrac);

        // Column 2: right inner
        float rightInner = halfWidth - sideBorder;
        if (-rightInner < 0.0f) rightInner = 0.0f;
        verts[c2].pos.x = rightInner;
        verts[c2].pos.y = 0.0f;
        verts[c2].pos.z = negY;
        verts[c2].uv.Set(1.0f - borderRatio, yFrac);

        // Column 3: right edge
        verts[c3].pos.x = halfWidth;
        verts[c3].pos.y = 0.0f;
        verts[c3].pos.z = negY;
        verts[c3].color.color = 0;
        verts[c3].uv.Set(1.0f, yFrac);

        if (i != totalSections - 1) {
            if ((i & 1) == 0) {
                faces[fi].Set(c0, n0, c1);
                faces[fi + 1].Set(c1, n0, n1);
                faces[fi + 2].Set(c1, n2, c2);
                faces[fi + 3].Set(c1, n1, n2);
                faces[fi + 4].Set(c2, n2, c3);
                faces[fi + 5].v1 = c3;
            } else {
                faces[fi].Set(c0, n0, n1);
                faces[fi + 1].Set(c0, n1, c1);
                faces[fi + 2].Set(c1, n1, c2);
                faces[fi + 3].Set(c2, n1, n2);
                faces[fi + 4].Set(c2, n3, c3);
                faces[fi + 5].v1 = c2;
            }
            faces[fi + 5].v2 = n2;
            faces[fi + 5].v3 = n3;

            if (i == totalSections - 2) {
                faces[fi].Set(c0, n0, c1);
                faces[fi + 1].Set(c1, n0, n1);
                faces[fi + 4].Set(c2, n2, n3);
                faces[fi + 5].Set(c3, c2, n3);
            }
        }

        if (i >= numSectionsTop) {
            halfWidth = radiusStepBotVal + halfWidth;
        } else {
            halfWidth = radiusStepTopVal + halfWidth;
        }

        fi += 6;
        c0 += 4;
    }

    def.mBeam->Sync(0x13F);
    def.mBeam->SetMat(def.mMat);
    def.mBeam->SetTransConstraint(kBillboardZ, nullptr, false);
    def.mBeam->SetTransParent(this, false);
}

void Spotlight::BuildNGCone(BeamDef &def, int numSegments) {
    Hmx::Matrix3 identMtx;
    identMtx.x.Set(1.0f, 0.0f, 0.0f);
    identMtx.y.Set(0.0f, 1.0f, 0.0f);
    identMtx.z.Set(0.0f, 0.0f, 1.0f);
    Hmx::Matrix3 orientMtx;
    Hmx::Matrix3 *pMtx;
    Hmx::Matrix3 rotMtx;
    if (def.mIsCone) {
        pMtx = &identMtx;
    } else {
        rotMtx.Set(
            Vector3(1.0f, 0.0f, 0.0f),
            Vector3(0.0f, 0.0f, -1.0f),
            Vector3(0.0f, 1.0f, 0.0f)
        );
        pMtx = &rotMtx;
    }
    orientMtx.x = pMtx->x;
    orientMtx.y = pMtx->y;
    orientMtx.z = pMtx->z;

    def.mBeam = Hmx::Object::New<RndMesh>();
    int numVerts = numSegments * 3;
    RndMesh::VertVector &verts = def.mBeam->Verts();
    std::vector<RndMesh::Face> &faces = def.mBeam->Faces();

    verts.resize(numVerts + 2, true);
    faces.resize(numSegments * 6);

    float length = def.mLength;
    Vector2 radii = def.NGRadii();
    float numSegsF = (float)numSegments;
    float angleStep = 6.2831855f / numSegsF;
    float halfAngle = angleStep * 0.5f;
    float invCosHalf = 1.0f / (float)std::cos((double)halfAngle);
    float topRadius = radii.x * invCosHalf;
    float bottomRadius = radii.y * invCosHalf;

    int flip = 0;
    int iVert = 0;
    float csAngle = 0.0f;
    float xsAngle = 0.7853982f;
    short baseIdx = 2;
    int iFace = 0;
    for (int seg = 0; seg != numSegments; seg++) {
        float halfStep = 2.0f;
        float cosH = (float)std::cos((double)halfAngle);
        float sinH = (float)std::sin((double)halfAngle);
        float segU = (float)seg / numSegsF;

        for (unsigned int v = 0; v < 3; v++) {
            float uvV = (float)v / halfStep;
            if (v <= 1) {
                float t = (float)v;
                float radius = (bottomRadius - topRadius) * t + topRadius;
                verts[iVert].pos.Set(radius * cosH, t * length, radius * sinH);
                Multiply(verts[iVert].pos, orientMtx, verts[iVert].pos);
            } else {
                float cosCs = (float)std::cos((double)csAngle);
                float sinCs = (float)std::sin((double)csAngle);
                csAngle = csAngle + xsAngle;
                verts[iVert].pos.y = sinCs * bottomRadius + length;
                verts[iVert].pos.z = (cosCs * (sinH * bottomRadius));
                verts[iVert].pos.x = cosCs * cosH * bottomRadius;
                Multiply(verts[iVert].pos, orientMtx, verts[iVert].pos);
            }
            verts[iVert].color.color = -1;
            verts[iVert].uv.Set(uvV, segU);
            iVert++;
        }

        short sideWidth;
        if (seg >= numSegments - 1) {
            sideWidth = 3 - (short)numVerts;
        } else {
            sideWidth = 3;
        }

        short cur = baseIdx - 1;
        int curFlip = flip;
        int fCount = 2;
        do {
            flip = curFlip + 1;
            short nextRow = cur - 1 + sideWidth;
            if (curFlip & 1) {
                faces[iFace].Set(nextRow, cur - 1, nextRow + 1);
                faces[iFace + 1].Set(nextRow + 1, cur - 1, cur);
            } else {
                faces[iFace].Set(cur - 1, cur, nextRow);
                faces[iFace + 1].Set(nextRow, cur, nextRow + 1);
            }
            cur = cur + 1;
            iFace += 2;
            curFlip = flip;
        } while (--fCount);

        halfAngle = halfAngle + angleStep;
        faces[iFace].Set(baseIdx - 2, baseIdx + sideWidth - 2, numVerts);
        faces[iFace + 1].Set(baseIdx + sideWidth, baseIdx, numVerts + 1);
        baseIdx = baseIdx + 3;
        iFace += 2;
    }

    verts[numVerts].pos.Set(0.0f, 0.0f, 0.0f);
    verts[numVerts].color.color = -1;
    verts[numVerts].uv.Set(0.0f, 0.0f);

    int baseVertIdx = numVerts + 1;
    verts[baseVertIdx].pos.Set(0.0f, length, 0.0f);
    Multiply(verts[baseVertIdx].pos, orientMtx, verts[baseVertIdx].pos);
    verts[baseVertIdx].color.color = -1;
    verts[baseVertIdx].uv.Set(0.0f, 1.0f);

    def.mBeam->Sync(0x13F);
    def.mBeam->SetMat(def.mMat);
    def.mBeam->SetTransParent(this, false);
}

void Spotlight::BuildNGSheet(BeamDef &def) {
    Hmx::Matrix3 identMtx;
    identMtx.x.Set(1.0f, 0.0f, 0.0f);
    identMtx.y.Set(0.0f, 1.0f, 0.0f);
    identMtx.z.Set(0.0f, 0.0f, 1.0f);

    Hmx::Matrix3 orientMtx;
    Hmx::Matrix3 rotMtx;
    Hmx::Matrix3 *pMtx;
    if (def.mIsCone) {
        pMtx = &identMtx;
    } else {
        rotMtx.Set(
            Vector3(1.0f, 0.0f, 0.0f),
            Vector3(0.0f, 0.0f, -1.0f),
            Vector3(0.0f, 1.0f, 0.0f)
        );
        pMtx = &rotMtx;
    }
    orientMtx.x = pMtx->x;
    orientMtx.y = pMtx->y;
    orientMtx.z = pMtx->z;

    def.mBeam = Hmx::Object::New<RndMesh>();
    RndMesh::VertVector &verts = def.mBeam->Verts();
    std::vector<RndMesh::Face> &faces = def.mBeam->Faces();

    int numSections = 5;
    if (def.mNumSections > 1) numSections = def.mNumSections;
    int numSegments = 10;
    if (def.mNumSegments > 2) numSegments = def.mNumSegments;

    int numRows = numSections + 1;
    int numCols = numSegments + 1;
    int kNumVerts = numRows * numCols;
    int kNumFaces = (numSegments * (numSections * 2));

    verts.resize(kNumVerts, true);
    faces.resize(kNumFaces);

    Vector2 radii = def.NGRadii();
    float topRadius = radii.x;
    float bottomRadius = radii.y;

    static float midBow = 1.0f;

    int iVert = 0;
    for (int row = 0; row < numRows; row++) {
        float t = (float)row / (float)numSections;
        float oneMinusT = 1.0f - t;
        for (int col = 0; col < numCols; col++) {
            float segFrac = (float)col / (float)numSegments * 2.0f - 1.0f;
            float xTop = segFrac * topRadius;
            float xBot = segFrac * bottomRadius;
            float absSegFrac = std::fabs(segFrac);

            verts[iVert].pos.Set(
                (xBot - xTop) * t + xTop,
                def.mLength * t,
                (1.0f - absSegFrac) * midBow
            );

            Multiply(verts[iVert].pos, orientMtx, verts[iVert].pos);

            verts[iVert].norm.Set(0.0f, 0.0f, 1.0f);
            Multiply(verts[iVert].norm, orientMtx, verts[iVert].norm);

            {
                int b = (int)(oneMinusT * 255.0f) & 0xFF;
                verts[iVert].color.color = b | (b << 8) | (b << 16) | (b << 24);
            }
            verts[iVert].uv.Set(absSegFrac, t);
            iVert++;
        }
    }
    MILO_ASSERT(iVert == kNumVerts, 0x51B);

    int iFace = 0;
    int rowStart = 0;
    for (int row = 0; row < numSections; row++) {
        for (int col = 0; col < numSegments; col++) {
            int base = rowStart + col;
            int next = base + 1;
            int baseNext = base + numCols;
            int nextNext = baseNext + 1;
            if ((iFace & 2) == 0) {
                faces[iFace].Set(base, next, baseNext);
                faces[iFace + 1].Set(baseNext, next, nextNext);
            } else {
                faces[iFace].Set(baseNext, base, nextNext);
                faces[iFace + 1].Set(nextNext, base, next);
            }
            iFace += 2;
        }
        rowStart += numCols;
    }
    MILO_ASSERT(iFace == kNumFaces, 0x534);

    def.mBeam->Sync(0x13F);
    def.mBeam->SetMat(def.mMat);
    def.mBeam->SetTransParent(this, false);
}

void Spotlight::BuildNGQuad(BeamDef &def, RndTransformable::Constraint constraint) {
    Hmx::Matrix3 rot;
    rot.Set(
        Vector3(1.0f, 0.0f, 0.0f),
        Vector3(0.0f, 0.0f, -1.0f),
        Vector3(0.0f, 1.0f, 0.0f)
    );
    def.mBeam = Hmx::Object::New<RndMesh>();
    RndMesh::VertVector &verts = def.mBeam->Verts();
    std::vector<RndMesh::Face> &faces = def.mBeam->Faces();
    int gridSize = def.mNumSegments;
    if (gridSize < def.mNumSections) {
        gridSize = def.mNumSections;
    }
    static int kSideVerts = (gridSize > 0) ? gridSize + 1 : 2;

    int nMinus1 = kSideVerts - 1;
    int totalVerts = kSideVerts * kSideVerts;
    int totalFaces = (nMinus1 * (nMinus1 * 2));

    verts.resize(totalVerts, true);
    faces.resize(totalFaces);

    int n = kSideVerts;
    float bottomRadius = def.mBottomRadius;
    float lengthVal = def.mLength;

    int idx = 0;
    for (int row = 0; row < n; row++) {
        float rowFrac = (float)row / (float)(n - 1);
        for (int col = 0; col < n; col++) {
            float colFrac = (float)col / (float)(n - 1);

            verts[idx].pos.Set(
                (colFrac * 2.0f - 1.0f) * bottomRadius,
                (rowFrac * 2.0f - 1.0f) * lengthVal,
                0.0f
            );
            Multiply(verts[idx].pos, rot, verts[idx].pos);

            verts[idx].norm.Set(0.0f, 0.0f, 1.0f);
            Multiply(verts[idx].norm, rot, verts[idx].norm);

            verts[idx].color.Set(1.0f, 1.0f, 1.0f, 1.0f);
            verts[idx].uv.Set(colFrac, rowFrac);
            idx++;
        }
    }

    int iFace = 0;
    for (int row = 0; row < nMinus1; row++) {
        for (int col = 0; col < nMinus1; col++) {
            int ibase = row + col * n;
            unsigned short uPrev = (unsigned short)(ibase);
            unsigned short uBase = (unsigned short)(ibase + 1);
            unsigned short uBaseN = (unsigned short)(ibase + n);
            unsigned short uBasePN = (unsigned short)(ibase + n + 1);
            if ((iFace & 2) != 0) {
                faces[iFace].v1 = uBaseN;
                faces[iFace].v2 = uPrev;
                faces[iFace].v3 = uBasePN;
                faces[iFace + 1].v1 = uBasePN;
                faces[iFace + 1].v2 = uPrev;
                faces[iFace + 1].v3 = uBase;
            } else {
                faces[iFace].v1 = uPrev;
                faces[iFace].v2 = uBase;
                faces[iFace].v3 = uBaseN;
                faces[iFace + 1].v1 = uBaseN;
                faces[iFace + 1].v2 = uBase;
                faces[iFace + 1].v3 = uBasePN;
            }
            iFace += 2;
        }
    }

    def.mBeam->Sync(0x13F);
    def.mBeam->SetMat(def.mMat);
    def.mBeam->SetTransConstraint(constraint, nullptr, false);
    def.mBeam->SetTransParent(this, false);
}

void Spotlight::BuildCone(BeamDef &def) {
    MILO_ASSERT(!SpotlightDrawer::DrawNGSpotlights(), 0x5AB);
    def.mIsCone = true;
    def.mBeam = Hmx::Object::New<RndMesh>();
    RndMesh::VertVector &verts = def.mBeam->Verts();
    std::vector<RndMesh::Face> &faces = def.mBeam->Faces();

    verts.resize(0x30, true);
    faces.resize(60);

    float len = def.mLength;
    float bottomBorderLen = def.mBottomBorder * len;
    if (len - bottomBorderLen < 0.0f) bottomBorderLen = len;
    float borderY = len - bottomBorderLen;
    float borderRadius = (borderY / len) * (def.mBottomRadius - def.mTopRadius) + def.mTopRadius;

    float angle = 0.0f;
    float uvStep = 1.0f / 15.0f;
    float angleStep = 0.4188790f;

    for (int i = 0; i != 15; i++) {
        float cosA = std::cos(angle);
        float sinA = std::sin(angle);

        float uvX = (float)i * uvStep;

        verts[i].pos.Set(def.mTopRadius * cosA, 0.0f, def.mTopRadius * sinA);
        verts[i].color.Set(1.0f, 1.0f, 1.0f, 1.0f);
        verts[i].uv.Set(uvX, 0.0f);

        verts[i + 16].pos.Set(borderRadius * cosA, borderY, borderRadius * sinA);
        verts[i + 16].color.Set(1.0f, 1.0f, 1.0f, 1.0f);
        verts[i + 16].uv.Set(uvX, borderY / len);

        verts[i + 32].pos.Set(def.mBottomRadius * cosA, len, def.mBottomRadius * sinA);
        verts[i + 32].color.Set(0.0f, 0.0f, 0.0f, 0.0f);
        verts[i + 32].uv.Set(uvX, 1.0f);

        short s = (short)(i + 17);
        int fi = i * 4;
        faces[fi].Set(s - 17, s - 1, s);
        faces[fi + 1].Set(s - 17, s, s - 16);
        faces[fi + 2].Set(s - 1, s + 15, s + 16);
        faces[fi + 3].Set(s - 1, s + 16, s);

        angle += angleStep;
    }

    verts[15].pos.Set(def.mTopRadius, 0.0f, 0.0f);
    verts[15].color.Set(1.0f, 1.0f, 1.0f, 1.0f);
    verts[15].uv.Set(1.0f, 0.0f);

    verts[31].pos.Set(borderRadius, borderY, 0.0f);
    verts[31].color.Set(1.0f, 1.0f, 1.0f, 1.0f);
    verts[31].uv.Set(1.0f, borderY / len);

    verts[47].pos.Set(def.mBottomRadius, len, 0.0f);
    verts[47].color.Set(0.0f, 0.0f, 0.0f, 0.0f);
    verts[47].uv.Set(1.0f, 1.0f);

    def.mBeam->Sync(0x13F);
    def.mBeam->SetTransParent(this, false);
    def.mBeam->SetMat(def.mMat);
}

void Spotlight::Mats(std::list<class RndMat *> &mats, bool b2) {
    if (mLensMaterial && b2) {
        mats.push_back(mLensMaterial);
        for (int i = 0; i < 2U; i++) {
            MatShaderOptions opts;
            opts.SetLast5(0xC);
            opts.mTempMat = true;
            opts.SetHasAOCalc(i);
            RndMat *mat = Hmx::Object::New<RndMat>();
            mat->Copy(mLensMaterial, kCopyDeep);
            mat->SetShaderOpts(opts);
            mats.push_back(mat);
        }
    }
    if (mDiscMat) {
        mats.push_back(mDiscMat);
    }
    if (mLightCanMesh && mLightCanMesh->Mat()) {
        MatShaderOptions opts;
        opts.SetLast5(0xC);
        RndMat *lightMat = mLightCanMesh->Mat();
        lightMat->SetShaderOpts(opts);
        mats.push_back(lightMat);
        if (b2) {
            for (int i = 0; i < 2U; i++) {
                MatShaderOptions opts;
                opts.SetLast5(0xC);
                opts.mTempMat = true;
                opts.SetHasAOCalc(i);
                RndMat *mat = Hmx::Object::New<RndMat>();
                mat->Copy(mLightCanMesh->Mat(), kCopyDeep);
                mat->SetShaderOpts(opts);
                mats.push_back(mat);
            }
        }
    }
    if (mBeam.mMat) {
        mats.push_back(mBeam.mMat);
    }
}

Spotlight::BeamDef::BeamDef(Hmx::Object *obj)
    : mBeam(0), mIsCone(0), mLength(100.0f), mTopRadius(4.0f), mBottomRadius(30.0f),
      mTopSideBorder(0.1f), mBottomSideBorder(0.3f), mBottomBorder(0.5f), mOffset(0.0f),
      mTargetOffset(0.0f, 0.0f), mBrighten(1.0f), mExpand(1.0f), mShape(0),
      mNumSections(0), mNumSegments(0), mXSection(obj), mCutouts(obj), mMat(obj) {}

Spotlight::BeamDef::BeamDef(const Spotlight::BeamDef &def)
    : mBeam(0), mIsCone(def.mIsCone), mLength(def.mLength), mTopRadius(def.mTopRadius),
      mBottomRadius(def.mBottomRadius), mTopSideBorder(def.mTopSideBorder),
      mBottomSideBorder(def.mBottomSideBorder), mBottomBorder(def.mBottomBorder),
      mOffset(def.mOffset), mTargetOffset(def.mTargetOffset), mBrighten(def.mBrighten),
      mExpand(def.mExpand), mShape(def.mShape), mNumSections(def.mNumSections),
      mNumSegments(def.mNumSegments),
      mXSection(def.mXSection.Owner(), def.mXSection.Ptr()), mCutouts(def.mCutouts),
      mMat(def.mMat.Owner(), def.mMat.Ptr()) {
    if (def.mBeam) {
        mBeam = Hmx::Object::New<RndMesh>();
        mBeam->Copy(def.mBeam, kCopyDeep);
    }
}

Spotlight::BeamDef::~BeamDef() { RELEASE(mBeam); }

void Spotlight::BeamDef::OnSetMat(RndMat *mat) {
    mMat = mat;
    if (mBeam)
        mBeam->SetMat(mMat);
}

RndTransformable *Spotlight::ResolveTarget() {
    if (!unk286)
        return 0;
    if (mTarget)
        return mTarget;
    return 0;
}

void Spotlight::PropogateToPresets(int i) {
    for (ObjDirItr<LightPreset> it(Dir(), false); it != nullptr; ++it) {
        it->SetSpotlight(this, i);
    }
}

BEGIN_HANDLERS(Spotlight)
    HANDLE_ACTION(propogate_targeting_to_presets, PropogateToPresets(2))
    HANDLE_ACTION(propogate_coloring_to_presets, PropogateToPresets(1))
    HANDLE_SUPERCLASS(RndDrawable)
    HANDLE_SUPERCLASS(RndTransformable)
    HANDLE_SUPERCLASS(RndPollable)
    HANDLE_SUPERCLASS(Hmx::Object)
    HANDLE_CHECK(0x71D)
END_HANDLERS

BEGIN_PROPSYNCS(Spotlight)
    SYNC_PROP_MODIFY(length, mBeam.mLength, Generate())
    SYNC_PROP_MODIFY(top_radius, mBeam.mTopRadius, Generate())
    SYNC_PROP_MODIFY(bottom_radius, mBeam.mBottomRadius, Generate())
    SYNC_PROP_MODIFY(top_side_border, mBeam.mTopSideBorder, Generate())
    SYNC_PROP_MODIFY(bottom_side_border, mBeam.mBottomSideBorder, Generate())
    SYNC_PROP_MODIFY(bottom_border, mBeam.mBottomBorder, Generate())
    SYNC_PROP_SET(material, mBeam.mMat, mBeam.OnSetMat(_val.Obj<RndMat>()))
    SYNC_PROP_MODIFY(offset, mBeam.mOffset, Generate())
    SYNC_PROP_MODIFY_ALT(angle_offset, mBeam.mTargetOffset, Generate())
    SYNC_PROP_MODIFY(is_cone, mBeam.mIsCone, Generate())
    SYNC_PROP(brighten, mBeam.mBrighten)
    SYNC_PROP_MODIFY(expand, mBeam.mExpand, Generate())
    SYNC_PROP_MODIFY(shape, mBeam.mShape, Generate())
    SYNC_PROP(xsection, mBeam.mXSection)
    SYNC_PROP(cutouts, mBeam.mCutouts)
    SYNC_PROP_MODIFY(sections, mBeam.mNumSections, Generate())
    SYNC_PROP_MODIFY(segments, mBeam.mNumSegments, Generate())
    SYNC_PROP_MODIFY_ALT(light_can, mLightCanMesh, UpdateBounds())
    SYNC_PROP_MODIFY(light_can_offset, mLightCanOffset, UpdateBounds())
    SYNC_PROP(light_can_sort, mLightCanSort)
    SYNC_PROP_MODIFY_ALT(target, mTarget, UpdateTransforms())
    SYNC_PROP(target_shadow, mTargetShadow)
    SYNC_PROP_SET(flare_material, mFlare->GetMat(), mFlare->SetMat(_val.Obj<RndMat>()))
    SYNC_PROP(flare_size, mFlare->mSizes)
    SYNC_PROP(flare_range, mFlare->mRange)
    SYNC_PROP_SET(flare_steps, mFlare->GetSteps(), mFlare->SetSteps(_val.Int()))
    SYNC_PROP_MODIFY(flare_offset, mFlareOffset, UpdateBounds())
    SYNC_PROP_MODIFY(flare_enabled, mFlareEnabled, UpdateFlare())
    SYNC_PROP_SET(
        flare_visibility_test,
        mFlareVisibilityTest == 0,
        SetFlareIsBillboard(_val.Int() == 0)
    )
    SYNC_PROP_MODIFY_ALT(spot_target, mSpotTarget, UpdateBounds())
    SYNC_PROP_MODIFY(spot_scale, mSpotScale, UpdateBounds())
    SYNC_PROP_MODIFY(spot_height, mSpotHeight, UpdateBounds())
    SYNC_PROP_MODIFY_ALT(spot_material, mDiscMat, UpdateBounds())
    SYNC_PROP_SET(color, Color().Opaque(), SetColor(_val.Int()))
    SYNC_PROP_SET(intensity, Intensity(), SetIntensity(_val.Float()))
    SYNC_PROP(color_owner, mColorOwner)
    SYNC_PROP(damping_constant, mDampingConstant)
    SYNC_PROP_MODIFY(lens_size, mLensSize, UpdateBounds())
    SYNC_PROP_MODIFY(lens_offset, mLensOffset, UpdateBounds())
    SYNC_PROP_MODIFY_ALT(lens_material, mLensMaterial, UpdateBounds())
    SYNC_PROP(additional_objects, mAdditionalObjects)
    SYNC_PROP(slaves, mSlaves)
    SYNC_PROP(animate_orientation_from_preset, mAnimateOrientationFromPreset)
    SYNC_PROP(animate_color_from_preset, mAnimateColorFromPreset)
    SYNC_SUPERCLASS(RndDrawable)
    SYNC_SUPERCLASS(RndTransformable)
END_PROPSYNCS
