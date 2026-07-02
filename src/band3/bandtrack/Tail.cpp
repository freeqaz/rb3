#include "bandtrack/Tail.h"
#include "bandtrack/GraphicsUtl.h"
#include "math/Mtx.h"
#include "math/Utl.h"
#include "obj/Object.h"
#include "obj/Task.h"
#include "os/Debug.h"
#include "rndobj/Group.h"
#include "rndobj/Mesh.h"
#include <math.h>

Tail::Tail(GemRepTemplate &tmp)
    : mGroup(Hmx::Object::New<RndGroup>()), mTail1(Hmx::Object::New<RndMesh>()),
      mTail2(Hmx::Object::New<RndMesh>()), mTailBegin(0), mType("normal"), mSlot(-1),
      mTemplate(tmp), mTailGeomOwner(0), mUpdateGeometry(0), mLastWhammyVal(0), mLastWhammyAlpha(0), mTailLength(0),
      mLastTailLength(0), mLastActive(0) {
    mGroup->AddObject(mTail1);
    mTail1->SetTransParent(mGroup, false);
    mGroup->AddObject(mTail2);
    mTail2->SetTransParent(mGroup, false);
}

Tail::~Tail() {
    UnhookAllParents(mGroup);
    ReleaseMeshes();
    RELEASE(mGroup);
    RELEASE(mTail1);
    RELEASE(mTail2);
}

void Tail::Init(
    int i1,
    const Transform &tf,
    bool b3,
    Symbol s,
    RndGroup *grp,
    const Tail::SlideInfo &info,
    Tail *tail
) {
    mState = 0;
    mSlot = i1;
    mSlideInfo = info;
    if (mSlideInfo.mSliding) {
        static float severity = 3.5f;
        mInterpolator.Reset(
            mSlideInfo.mStartOffset, mSlideInfo.mEndOffset, 0, mSlideInfo.mLength, severity
        );
    }
    ConfigureMeshes(tail);
    SetType(s, b3);
    mGroup->SetLocalXfm(tf);
    grp->AddObject(mGroup);
    mWhammy.Clear();
    mLastWhammyVal = 0;
    mLastWhammyAlpha = 0;
    mWaveTime = 0;
    mTailLength = 0;
    mLastTailLength = 0;
    mLastActive = false;
    UpdateVerts(mTemplate.kTailMinAlpha, false);
}

void Tail::MoveSlot(const Transform &tf) {
    if (mGroup) {
        Transform tf38(tf);
        tf38.v.y = mGroup->mLocalXfm.v.y;
        mGroup->SetLocalXfm(tf38);
    }
}

void Tail::SetType(Symbol s, bool b) {
    mType = s;
    if (mType == "unison") {
        mType = "star";
    }
    bool isStar = mType == "star";
    RndMat *tailMat;
    if (mState == 1) {
        tailMat = mTemplate.GetTailMiss();
    } else if (isStar) {
        tailMat = mTemplate.GetTailBonus();
    } else if (b) {
        tailMat = mTemplate.GetTailChord();
    } else {
        tailMat = mTemplate.GetSlotMat(0, mSlot);
    }
    MILO_ASSERT(tailMat, 0x8A);
    mTail1->SetMat(tailMat);
    mTail2->SetMat(tailMat);
    mTail1->SetShowing(mType != "invisible");
    mTail2->SetShowing(false);
    if (mState == 2)
        Hit();
}

void Tail::ConfigureMeshes(Tail *tail) {
    if (tail) {
        mTailGeomOwner = tail->mTailGeomOwner;
        mUpdateGeometry = false;
    } else {
        mTailGeomOwner = mTemplate.GetTail();
        mUpdateGeometry = true;
    }
    MILO_ASSERT(mTailGeomOwner, 0xAC);
    mTail1->SetGeomOwner(mTailGeomOwner);
    mTail2->SetGeomOwner(mTailGeomOwner);
    Hmx::Matrix3 m38;
    m38.Identity();
    m38.x.x = -1.0f;
    mTail2->SetLocalRot(m38);
}

void Tail::ReleaseMeshes() {
    if (mUpdateGeometry)
        mTemplate.ReturnTail(mTailGeomOwner);
    mTailGeomOwner = nullptr;
    mTail1->SetGeomOwner(mTail1);
    mTail2->SetGeomOwner(mTail2);
    mTail1->SetShowing(false);
    mTail2->SetShowing(false);
}

void Tail::SetDuration(float f1, float f2, float f3) {
    if (mState != 4) {
        if (mState == 2) {
            mTailBegin = Max(f1, mTailBegin);
        } else {
            mTailBegin = Max(f2, mTailBegin);
        }
        mTailBegin = Min(mTailBegin, f3);
        mTailLength = f3 - mTailBegin;
    }
}

void Tail::Hit() {
    mState = 2;
    if (!mSlideInfo.mSliding) {
        mTail2->SetShowing(true);
    }
    if (mUpdateGeometry)
        mWhammy.Clear();
}

void Tail::Release() {
    if (mState != 4) {
        mState = 3;
        HandleMistake();
    }
}

void Tail::Done() {
    if (mState == 2) {
        mState = 4;
        mTailBegin = 0;
        mTailLength = 0;
        mTail1->SetShowing(false);
        mTail2->SetShowing(false);
    }
}

void Tail::HandleMistake() { mTail2->SetShowing(false); }

void Tail::Poll(float, float whammy, float) {
    if (mTailGeomOwner) {
        bool t3 = mState == 2 && !mSlideInfo.mSliding;
        float fvar1 = t3 ? mTemplate.kTailOffsetX * mTemplate.GetTailScaleX() : 0;
        mTail1->SetLocalPos(-fvar1, mTailBegin, 0);
        mTail2->SetLocalPos(fvar1, mTailBegin, 0);
        if (mUpdateGeometry) {
            float alpha;
            if (t3) {
                float delta = TheTaskMgr.DeltaSeconds();
                static float pulseRate = 1.0f / mTemplate.kTailPulseRate;
                for (float time = TheTaskMgr.Seconds(TaskMgr::kRealTime) - delta;
                     time < TheTaskMgr.Seconds(TaskMgr::kRealTime);
                     time += pulseRate) {
                    GemRepTemplate *tmp = &mTemplate;
                    mLastWhammyVal = Interp(mLastWhammyVal, whammy, tmp->kTailPulseSmoothing);
                    float negWhammy = -mLastWhammyVal;
                    float ampMin = tmp->kTailAmplitudeRange.x;
                    float f4 = Interp(
                        tmp->kTailFrequencyRange.x,
                        tmp->kTailFrequencyRange.y,
                        negWhammy
                    );
                    mWhammy.Set(
                        Interp(ampMin, tmp->kTailAmplitudeRange.y, negWhammy)
                        * std::sin(mWaveTime)
                    );
                    mWaveTime += pulseRate * f4;
                }
                mLastWhammyAlpha = Interp(mLastWhammyAlpha, whammy, mTemplate.kTailAlphaSmoothing);
                alpha = Interp(mTemplate.kTailMinAlpha, mTemplate.kTailMaxAlpha, -mLastWhammyAlpha);
            } else {
                mWaveTime = 0;
                alpha = mTemplate.kTailMinAlpha;
                mLastWhammyAlpha = 0;
            }

            if (mTailLength != mLastTailLength || t3 || mSlideInfo.mSliding || t3 != mLastActive) {
                UpdateVerts(alpha, t3);
                mLastTailLength = mTailLength;
                mLastActive = t3;
            }
        }
    }
}

void Tail::UpdateVerts(float alpha, bool active) {
    if (!mUpdateGeometry) return;

    int tailFlag = 0;
    if (active || mSlideInfo.mSliding) tailFlag = 1;
    GemRepTemplate::TailType tailType = (GemRepTemplate::TailType) !tailFlag;
    float scaleX = mTemplate.mTailScaleX;
    int total_sections = mTemplate.GetNumTailSections(tailType);
    float sectionLen = mTemplate.GetTailSectionLength(tailType);
    float clamped = Clamp<float>(0, mTemplate.kTailMaxLength, mTailLength);
    float capLen = Clamp<float>(0, mTemplate.mTailSectionLength[0], clamped);
    int capInc = capLen > 0;
    float midLen = clamped - capLen;
    int midSections = 0;
    float midStart = 0;
    if (midLen > 0) {
        midSections = (int) (float) ceil(midLen / sectionLen);
        midStart = -(((float) (midSections - 1) * sectionLen) - midLen);
    }
    int used_sections = capInc + midSections;
    MILO_ASSERT(used_sections <= total_sections, 0x1C3);

    GemRepTemplate &templ = mTemplate;
    int vertCount = templ.GetRequiredVertCount(used_sections);
    bool resized = false;
    RndMesh::VertVector &verts = mTailGeomOwner->Verts();
    if (vertCount != verts.size()) {
        verts.resize(vertCount, true);
        resized = true;
    }

    RndMesh::Vert *tailBegin = &templ.mTailVerts[0];
    RndMesh::Vert *out = verts.begin();
    RndMesh::Vert *tailEnd = &templ.mTailVerts[templ.mTailVerts.size()];
    float yWorld = mTailBegin;
    float curY = 0;

    float baseOfs = 0;
    if (active) {
        baseOfs = baseOfs + mWhammy[0];
    }
    if (mSlideInfo.mSliding) {
        baseOfs += mInterpolator.Eval(yWorld);
    }

    float zScale = 1.0f;
    for (RndMesh::Vert *src = tailBegin; src != tailEnd; ++src, ++out) {
        out->pos.y = 0;
        out->pos.x = scaleX * (src->pos.x + baseOfs);
        out->pos.z = src->pos.z * zScale;
        out->uv.x = src->uv.x;
        out->uv.y = src->uv.y;
    }

    curY += midStart;
    yWorld += midStart;

    for (int i = 0; i < midSections; ) {
        float ofs = 0;
        if (active) {
            int idx = (int) (0.5f * curY);
            ofs += mWhammy[idx];
        }
        if (mSlideInfo.mSliding) {
            ofs += mInterpolator.Eval(yWorld);
        }
        for (RndMesh::Vert *src = tailBegin; src != tailEnd; ++src, ++out) {
            out->pos.y = curY;
            out->pos.x = scaleX * (src->pos.x + ofs);
            out->pos.z = src->pos.z * zScale;
            out->uv.x = src->uv.x;
            out->uv.y = src->uv.y;
        }
        ++i;
        if (i == midSections) break;
        curY += sectionLen;
        yWorld += sectionLen;
    }

    if (capLen > 0) {
        curY += capLen;
        yWorld += capLen;
        float ofs = 0;
        if (active) {
            int idx = (int) (0.5f * curY);
            ofs += mWhammy[idx];
        }
        if (mSlideInfo.mSliding) {
            ofs += mInterpolator.Eval(yWorld);
        }
        RndMesh::Vert *csrc = &templ.mCapVerts[0];
        RndMesh::Vert *cend = &templ.mCapVerts[templ.mCapVerts.size()];
        for (; csrc != cend; ++csrc, ++out) {
            out->pos.y = curY;
            out->pos.x = scaleX * (csrc->pos.x + ofs);
            out->pos.z = csrc->pos.z * zScale;
            out->uv.x = csrc->uv.x;
            out->uv.y = csrc->uv.y;
        }
    }
    MILO_ASSERT(out == verts.end(), 0x219);

    int syncFlags = 0x9F;
    if (resized) {
        std::vector<RndMesh::Face> &faces = mTailGeomOwner->Faces();
        RndMesh::Face zeroFace;
        zeroFace.v1 = 0; zeroFace.v2 = 0; zeroFace.v3 = 0;
        int faceCount = mTemplate.GetRequiredFaceCount(used_sections);
        if ((unsigned int) faceCount < (unsigned int) faces.size()) {
            faces.erase(faces.begin() + faceCount, faces.end());
        } else {
            faces.insert(faces.end(), faceCount - faces.size(), zeroFace);
        }
        unsigned short v = 0;
        for (int i = 0; i < used_sections; i++) {
            faces[i*2].Set(v, v + 1, v + 2);
            faces[i*2 + 1].Set(v + 2, v + 1, v + 3);
            v += 2;
        }
        syncFlags |= 0x20;
    }
    mTailGeomOwner->Sync(syncFlags);
}
