#include "bandobj/BandHeadShaper.h"
#include "bandobj/BandFaceDeform.h"
#include "char/CharUtl.h"
#include "os/Debug.h"
#include "utl/Symbols.h"
#include <list>

int BandHeadShaper::sChinNum;
int BandHeadShaper::sEyeNum;
int BandHeadShaper::sMouthNum;
int BandHeadShaper::sNoseNum;
int BandHeadShaper::sShapeNum;
ObjectDir *gHeadMale;
std::vector<int> gHeadMaleMapping;
ObjectDir *gHeadFemale;
std::vector<int> gHeadFemaleMapping;
ObjDirPtr<ObjectDir> gVisemes[4] = { ObjDirPtr<ObjectDir>(0),
                                     ObjDirPtr<ObjectDir>(0),
                                     ObjDirPtr<ObjectDir>(0),
                                     ObjDirPtr<ObjectDir>(0) };

void SetMeshAnim(ObjectDir *dir, std::vector<int> &vec) {
    RndMeshAnim *manim = dir->Find<RndMeshAnim>("base.msnm", false);
    if (!manim)
        MILO_WARN("%s does not contain base.msnm, no head shaping", PathName(dir));
    else {
        if (manim->VertPointsKeys().size() == 0) {
            MILO_WARN("%s has no point verts", PathName(manim));
        } else {
            std::vector<Vector3> &vertkeys = manim->VertPointsKeys()[0].value;
            vec.resize(vertkeys.size());
            ObjectDir *headdir = DirLoader::LoadObjects(
                FilePath(MakeString(
                    "char/main/head/%s/head.milo",
                    strstr(dir->GetPathName(), "female") ? "female" : "male"
                )),
                0,
                0
            );
            RndMesh *headmesh = headdir->Find<RndMesh>("head.mesh", false);
            if (headmesh) {
                if (vertkeys.size() == headmesh->Verts().size()) {
                    for (int i = 0; i < vec.size(); i++) {
                        vec[i] = i;
                    }
                    for (int i = 0; i < vec.size(); i++) {
                        float f19 = 1.0E+30f;
                        int i16 = -1;
                        const Vector3 &v = vertkeys[i];
                        for (int j = i; j < vec.size(); j++) {
                            float distsq =
                                DistanceSquared(v, headmesh->Verts()[vec[j]].pos);
                            if (distsq < f19) {
                                f19 = distsq;
                                i16 = j;
                            }
                        }
                        std::swap(vec[i], vec[i16]);
                    }
                } else {
                    MILO_WARN(
                        "%s has %d verts, head %s has %d, need to match\n",
                        PathName(manim),
                        vec.size(),
                        PathName(headmesh),
                        headmesh->Verts().size()
                    );
                    vec.clear();
                }
            } else {
                MILO_WARN("%s must contain head.mesh", PathName(headdir));
                vec.clear();
            }
            delete headdir;
        }
    }
}

int GetNum(const char *cc, int i1, ObjectDir *dir, int i2) {
    BandFaceDeform *df = dir->Find<BandFaceDeform>(MakeString("%s.fdm", cc), false);
    int num = 0;
    if (df) {
        num = df->mFrames.size();
        if ((num - 1) % i1) {
            MILO_LOG(
                "NOTIFY: %s has %d frames, must be multiple of %d + 1\n",
                PathName(df),
                num,
                i1
            );
        }
    }
    int ret = (num - 1) / i1;
    if (i2 >= 0 && i2 != ret) {
        MILO_LOG(
            "NOTIFY: %s must have %d frames, has %d\n", PathName(df), i1 * i2 + 1, num
        );
    }
    return ret;
}

ObjectDir *BandHeadShaper::GetViseme(Symbol s, bool b) {
    return gVisemes[b + 2 * (s != female)];
}

ObjectDir *FindSubdir(ObjectDir *dir, const char *cc) {
    ObjectDir *subdir = dir->Find<ObjectDir>(cc, false);
    if (subdir)
        return subdir->mSubDirs[0].Ptr();
    else
        return 0;
}

void BandHeadShaper::Init() {
    FilePathTracker tracker(FileRoot());
    const char *genderpath = "";
    DataArray *cfg = SystemConfig("objects", "BandCharDesc");
    if (cfg->FindData("head_male_path", genderpath, false) && genderpath[0] != 0) {
        static int _x = MemFindHeap("char");
        MemPushHeap(_x);
        gHeadMale = DirLoader::LoadObjects(FilePath(genderpath), 0, 0);
        SetMeshAnim(gHeadMale, gHeadMaleMapping);
        sChinNum = GetNum("chin", 5, gHeadMale, -1);
        sEyeNum = GetNum("eye", 7, gHeadMale, -1);
        sMouthNum = GetNum("mouth", 5, gHeadMale, -1);
        sNoseNum = GetNum("nose", 5, gHeadMale, -1);
        sShapeNum = GetNum("shape", 1, gHeadMale, -1);
        GetNum("jaw", 5, gHeadMale, 1);
        gVisemes[2] = FindSubdir(gHeadMale, "visemes");
        gVisemes[3] = FindSubdir(gHeadMale, "vignette_visemes");
        MemPopHeap();
    }
    if (cfg->FindData("head_female_path", genderpath, false) && genderpath[0] != 0) {
        static int _x = MemFindHeap("char");
        MemPushHeap(_x);
        gHeadMale = DirLoader::LoadObjects(FilePath(genderpath), 0, 0);
        SetMeshAnim(gHeadMale, gHeadMaleMapping);
        GetNum("chin", 5, gHeadFemale, sChinNum);
        GetNum("eye", 7, gHeadFemale, sEyeNum);
        GetNum("mouth", 5, gHeadFemale, sMouthNum);
        GetNum("nose", 5, gHeadFemale, sNoseNum);
        GetNum("shape", 1, gHeadFemale, sShapeNum);
        GetNum("jaw", 5, gHeadFemale, 1);
        gVisemes[0] = FindSubdir(gHeadFemale, "visemes");
        gVisemes[1] = FindSubdir(gHeadFemale, "vignette_visemes");
        MemPopHeap();
    }
    for (int i = 0; i < 4; i++) {
        gVisemes[i]->SetName("", 0);
    }
}

int BandHeadShaper::GetCount(Symbol s) {
    if (s == shape)
        return sShapeNum;
    if (s == chin)
        return sChinNum;
    if (s == eye)
        return sEyeNum;
    if (s == nose)
        return sNoseNum;
    return s == mouth ? sMouthNum : 0;
}

void BandHeadShaper::Terminate() {
    RELEASE(gHeadFemale);
    RELEASE(gHeadMale);
    for (int i = 0; i < 4; i++)
        gVisemes[i] = 0;
}

BandHeadShaper::BandHeadShaper() : mBones(0) {}

BandHeadShaper::~BandHeadShaper() { MILO_ASSERT(!mBones, 0xD6); }

bool BandHeadShaper::Start(
    ObjectDir *dir, Symbol s, RndMesh *mesh, SyncMeshCB *cb, bool b
) {
    if (mesh->Verts().size() == 0)
        return false;
    else {
        ObjectDir *visemedir = gVisemes[0 + 2 * (s != female)];
        if (visemedir) {
            CharClip *clip = visemedir->Find<CharClip>("Base", false);
            if (clip) {
                clip->PoseMeshes(dir, clip->StartBeat());
            }
        }
        cb->SyncMesh(mesh, 0x1F);
        mDst = mesh;
        mBonesOnly = b;
        mMapping = s == female ? &gHeadFemaleMapping : &gHeadMaleMapping;
        mHeadDir = s == female ? gHeadFemale : gHeadMale;
        if (mMapping->size() == 0)
            return false;
        else {
            if (mMapping->size() != mDst->Verts().size()) {
                MILO_WARN(
                    "%s claims to be %s but has wrong vert number %d, should be %d",
                    PathName(mDst),
                    s,
                    mDst->Verts().size(),
                    mMapping->size()
                );
                return false;
            } else {
                mAnim = mHeadDir->Find<RndMeshAnim>("base.msnm", true);
                const std::vector<Vector3> &vec = mAnim->VertPointsKeys()[0].value;
                for (int i = 0; i < vec.size(); i++) {
                    mDst->Verts()[(*mMapping)[i]].pos = vec[i];
                }
                mBones = new CharBonesMeshes();
                mBones->SetName("head_morph", dir);
                mBase = mHeadDir->Find<CharClip>("base", true);
                mBase->StuffBones(*mBones);
                mBase->ScaleDown(*mBones, 0);
                return true;
            }
        }
    }
}

void TestMesh(RndTransformable *start, RndTransformable *top) {
    Transform ident;
    ident.Reset();
    for (RndTransformable *cur = start; cur != top; cur = cur->TransParent()) {
        if (!cur->TransParent()) {
            MILO_WARN(
                "%s needs to have eventual parent of %s, does not, stops at %s",
                PathName(start),
                PathName(top),
                PathName(cur)
            );
            return;
        }
        if (!(cur->LocalXfm() == ident)) {
            MILO_WARN(
                "%s needs to be all zero'd xfms all the way up, but %s is not",
                PathName(start),
                PathName(cur)
            );
            return;
        }
    }
}

void BandHeadShaper::AddChildBones(RndTransformable *t) {
    if (!t)
        MILO_WARN("Trying to add a NULL child bone.\n");
    else {
        std::vector<RndTransformable *>::iterator it =
            std::find(unk18.begin(), unk18.end(), t);
        if (it == unk18.end()) {
            unk18.push_back(t);
            for (std::vector<RndTransformable *>::const_iterator child =
                     t->TransChildren().begin();
                 child != t->TransChildren().end();
                 ++child) {
                AddChildBones(*child);
            }
        }
    }
}

void BandHeadShaper::AddFrame(const char *cc, int frame, float weight) {
    BandFaceDeform *df =
        mAnim->Dir()->Find<BandFaceDeform>(MakeString("%s.fdm", cc), false);
    if (df && !mBonesOnly) {
        int fi = frame + 1;
        if ((unsigned int)fi < (unsigned short)df->mFrames.size()) {
            BandFaceDeform::DeltaArray &da = df->mFrames[fi];
            for (Delta *d = (Delta *)da.begin(); d < da.end();
                 d = (Delta *)d->next()) {
                float dx, dy, dz;
                signed char *bytes = (signed char *)d + 4;
                for (int j = 0; j < d->num; j++) {
                    dx = 0.015748031f * (float)bytes[0];
                    dy = 0.015748031f * (float)bytes[1];
                    dz = 0.015748031f * (float)bytes[2];
                    bytes += 3;
                    int vi = (*mMapping)[j + *(unsigned short *)d];
                    RndMesh::Vert &v = mDst->Verts()[vi];
                    v.pos.x += dx * weight;
                    v.pos.y += dy * weight;
                    v.pos.z += dz * weight;
                }
            }
        }
    }
    CharClip *clip = mAnim->Dir()->Find<CharClip>(cc, false);
    if (clip) {
        clip->ScaleAdd(*mBones, weight, clip->FrameToBeat(frame + 1), 0.0f);
    }
}

void BandHeadShaper::AddFrameHelper(
    const char *cc, int i1, int i2, float f, float &fref
) {
    int frame;
    float weight;
    if (f < 0.5f) {
        frame = i1 + i2;
        weight = -(2.0f * f - 1.0f);
    } else {
        frame = i1 + i2 + 1;
        weight = 2.0f * (f - 0.5f);
    }
    AddFrame(cc, frame, weight);
    fref -= weight;
}

void BandHeadShaper::AddDegrees(const char *cc, int i1, float *degrees, int count) {
    int base = i1 * (count * 2 + 1);
    float remainder = 1.0f;
    int idx = 1;
    int i = 0;
    float *p = degrees;
    for (; i < count; p++, idx += 2, i++) {
        AddFrameHelper(cc, base, idx, *p, remainder);
    }
    AddFrame(cc, base, remainder);
}

void BandHeadShaper::Reskin() {
    if (mBonesOnly)
        return;
    RndTransformable *topTrans =
        dynamic_cast<RndTransformable *>(mBones->Dir());
    std::list<CharBones::Bone> bones;
    mBase->ListBones(bones);
    for (std::list<CharBones::Bone>::iterator it = bones.begin(); it != bones.end();
         ++it) {
        AddChildBones(CharUtlFindBoneTrans(it->name.Str(), mBones->Dir()));
    }
    for (int i = 0; i < unk18.size(); i++) {
        RndTransformable *bone = unk18[i];
        const std::vector<ObjRef *> &refs = bone->Refs();
        for (std::vector<ObjRef *>::const_iterator rit = refs.end();
             rit != refs.begin();) {
            --rit;
            RndMesh *mesh = dynamic_cast<RndMesh *>((*rit)->RefOwner());
            if (!mesh)
                continue;
            bool isHead = strcmp(mesh->Name(), "head.mesh") == 0;
            if (!isHead)
                continue;
            TestMesh(mesh, topTrans);
            for (int j = 0; j < mesh->NumBones(); j++) {
                if (mesh->BoneTransAt(j) == bone) {
                    mesh->SetBone(j, bone, true);
                }
            }
        }
        bone->SetDirty();
    }
}

void BandHeadShaper::End() {
    mBones->ScaleAddIdentity();
    mBase->RotateBy(*mBones, mBase->StartBeat());
    mBones->PoseMeshes();
    Reskin();
    RELEASE(mBones);
}