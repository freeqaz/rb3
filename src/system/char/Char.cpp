
#include "CharBonesBlender.h"
#include "CharGuitarString.h"
#include "CharLookAt.h"
#include "CharMeshHide.h"
#include "CharMirror.h"
#include "CharNeckTwist.h"
#include "CharPosConstraint.h"
#include "CharSleeve.h"
#include "CharTransCopy.h"
#include "CharTransDraw.h"
#include "CharUpperTwist.h"
#include "CharWeightSetter.h"
#include "ClipCollide.h"
#include "FileMergerOrganizer.h"
#include "Waypoint.h"
#include "char/CharClipGroup.h"
#include "char/CharClipSet.h"
#include "char/CharCollide.h"
#include "char/CharCuff.h"
#include "char/CharDriver.h"
#include "char/CharDriverMidi.h"
#include "char/CharEyeDartRuleset.h"
#include "char/CharEyes.h"
#include "char/CharFaceServo.h"
#include "char/CharForeTwist.h"
#include "char/CharHair.h"
#include "char/CharIKFingers.h"
#include "char/CharIKFoot.h"
#include "char/CharIKHand.h"
#include "char/CharIKHead.h"
#include "char/CharIKMidi.h"
#include "char/CharIKRod.h"
#include "char/CharIKScale.h"
#include "char/CharIKSliderMidi.h"
#include "char/CharInterest.h"
#include "char/CharLipSync.h"
#include "char/CharLipSyncDriver.h"
#include "char/CharPollGroup.h"
#include "char/CharServoBone.h"
#include "char/CharTaskMgr.h"
#include "char/CharWeightable.h"
#include "char/FileMerger.h"
#include "obj/Dir.h"
#include "os/Debug.h"
#include "char/Char.h"
#include "char/Character.h"
#include "char/CharBoneDir.h"
#include "char/CharBoneOffset.h"
#include "char/CharBones.h"
#include "char/CharBlendBone.h"
#include "char/CharBoneTwist.h"
#include "char/CharClip.h"
#include "char/CharUtl.h"
#include "obj/DataFunc.h"
#include "rndobj/Cam.h"
#include "rndobj/Highlightable.h"
#include "rndobj/Mat.h"
#include "rndobj/Mesh.h"
#include "rndobj/Tex.h"
#include "world/Dir.h"

float gCharHighlightY = -1;

CharDebug TheCharDebug;

inline void CharDebug::AddObject(Hmx::Object *o, bool once) {
    if (o) {
        ObjPtrList<Hmx::Object> &which = once ? mOnce : mObjects;
        ObjPtrList<Hmx::Object>::iterator it = which.find(o);
        if (!it) {
            which.push_back(o);
        } else {
            which.erase(it);
        }
    }
}

inline void CharDebug::SetObjects(DataArray *msg) {
    int i = 1;
    bool clear = false;
    bool once = false;
    bool cmp = msg->Size() > 1;
    if (cmp) {
        cmp = msg->Type(1) == kDataSymbol;
    }
    if (cmp) {
        cmp = msg->Sym(1) == "clear";
        if (!cmp) {
            cmp = msg->Sym(1) == "once";
        }
    }
    if (cmp) {
        i = 2;
        clear = msg->Sym(1) == "clear";
        once = msg->Sym(1) == "once";
    }
    if (clear) {
        mObjects.clear();
    }
    for (; i < msg->Size(); i++) {
        AddObject(msg->GetObj(i), once);
    }
    mOverlay->SetShowing(!mObjects.empty() || !mOnce.empty());
}

DataNode CharDebug::OnSetObjects(DataArray *a) {
    TheCharDebug.SetObjects(a);
    return 0;
}

inline void CharDebug::Once(Hmx::Object *obj) {
    AddObject(obj, true);
    mOverlay->SetShowing(!mObjects.empty() || !mOnce.empty());
}

void CharDeferHighlight(Hmx::Object *obj) { TheCharDebug.Once(obj); }

inline void CharDebug::Init() {
    DataRegisterFunc("char_debug", OnSetObjects);
    mOverlay = RndOverlay::Find("char_debug", true);
    mOverlay->SetCallback(this);
}

void CharInit() {
#ifdef MILO_DEBUG
    TheCharDebug.Init();
#endif
    Character::Init();
    CharBonesObject::Init();
    CharBoneOffset::Init();
    CharBlendBone::Init();
    CharBone::Init();
    CharBonesBlender::Init();
    CharBoneTwist::Init();
    CharClip::Init();
    CharClipSet::Init();
    CharClipGroup::Init();
    CharCollide::Init();
    CharCuff::Init();
    CharDriver::Init();
    CharDriverMidi::Init();
    CharEyes::Init();
    CharInterest::Init();
    CharEyeDartRuleset::Init();
    CharFaceServo::Init();
    CharForeTwist::Init();
    CharHair::Init();
    CharIKFingers::Init();
    CharIKFoot::Init();
    CharIKHand::Init();
    CharIKHead::Init();
    CharIKMidi::Init();
    CharIKSliderMidi::Init();
    CharIKRod::Init();
    CharIKScale::Init();
    CharLipSync::Init();
    CharLipSyncDriver::Init();
    CharLookAt::Init();
    CharMeshHide::Init();
    CharMirror::Init();
    CharNeckTwist::Init();
    CharPollGroup::Init();
    CharPosConstraint::Init();
    CharServoBone::Init();
    CharSleeve::Init();
    CharTaskMgr::Init();
    CharTransDraw::Init();
    CharTransCopy::Init();
    CharUpperTwist::Init();
    CharWeightable::Init();
    CharWeightSetter::Init();
    Waypoint::Init();
    CharGuitarString::Init();
    FileMerger::Init();
    CharBoneDir::Register();
    ClipCollide::Init();
    FileMergerOrganizer::Init();
    PreloadSharedSubdirs("char");
    CharBoneDir::Init();
    CharUtlInit();
    TheDebug.AddExitCallback(CharTerminate);
}

void CharTerminate() {
    TheDebug.RemoveExitCallback(CharTerminate);
    Character::Terminate();
    CharBoneDir::Terminate();
}

inline void CharDebug::DisplayObject(Hmx::Object *obj) {
    RndHighlightable *rh = dynamic_cast<RndHighlightable *>(obj);
    if (rh) {
        rh->Highlight();
    } else {
        RndTex *tex = dynamic_cast<RndTex *>(obj);
        if (tex) {
            static RndMesh *mesh;
            static RndMat *mat;
            if (!mesh) {
                mesh = Hmx::Object::New<RndMesh>();
                mat = Hmx::Object::New<RndMat>();
                mat->mUseEnviron = false;
                mat->mColorModFlags = RndMat::kColorModAlphaUnpackModulate;
                mesh->Verts().resize(4, true);
                mesh->Faces().resize(2);
                for (int i = 0; i < 4; i++) {
                    float v;
                    if (i < 2) {
                        v = 1.0f;
                    } else {
                        v = 0.0f;
                    }
                    bool isU = i == 1 || i == 2;
                    float u;
                    if (isU) {
                        u = 1.0f;
                    } else {
                        u = 0.0f;
                    }
                    RndMesh::Vert &vert = mesh->Verts()[i];
                    vert.uv.Set(v, u);
                    vert.pos.Set(
                        v * 20.0f + 20.0f, 0.0f * 20.0f, -u * 20.0f + 60.0f
                    );
                    vert.norm.Set(0.0f, -1.0f, 0.0f);
                    vert.boneWeights.Set(0.0f, 0.0f, 0.0f, 0.0f);
                    vert.color = Hmx::Color32(-1);
                }
                mesh->Faces()[0].Set(0, 1, 2);
                mesh->Faces()[1].Set(0, 2, 3);
                mesh->Sync(0x13F);
                mesh->SetMat(mat);
            }
            mat->SetDiffuseTex(tex);
            mesh->Highlight();
        }
    }
}

float CharDebug::UpdateOverlay(RndOverlay *ovl, float hilite_y) {
    gCharHighlightY = hilite_y;
    RndCam *cur = RndCam::Current();
    RndCam *worldCam = 0;
    if (TheWorld) {
        worldCam = TheWorld->mCam;
    }
    if (worldCam) {
        worldCam->Select();
    }
    FOREACH (it, mObjects) {
        DisplayObject(*it);
    }
    FOREACH (it, mOnce) {
        DisplayObject(*it);
    }
    mOnce.clear();
    if (mObjects.empty()) {
        ovl->mShowing = false;
        ovl->mTimer.Restart();
    }
    if (worldCam) {
        cur->Select();
    }
    float ret = gCharHighlightY;
    gCharHighlightY = -1;
    return ret;
}
