#include "rndobj/TexBlender.h"
#include "rndobj/Utl.h"
#include "utl/Symbols.h"

INIT_REVS(RndTexBlender);

RndTexBlender::RndTexBlender()
    : mBaseMap(this, 0), mNearMap(this, 0), mFarMap(this, 0), mOutputTextures(this, 0),
      mControllerList(this, kObjListNoNull), mOwner(this, 0), mControllerInfluence(1.0f),
      unk70(0) {
    unk9p6 = 1;
}

BEGIN_COPYS(RndTexBlender)
    COPY_SUPERCLASS(Hmx::Object)
    COPY_SUPERCLASS(RndDrawable)
    CREATE_COPY(RndTexBlender)
    BEGIN_COPYING_MEMBERS
        COPY_MEMBER(mOutputTextures)
        COPY_MEMBER(mBaseMap)
        COPY_MEMBER(mNearMap)
        COPY_MEMBER(mFarMap)
        COPY_MEMBER(mControllerList)
        COPY_MEMBER(mOwner)
        COPY_MEMBER(mControllerInfluence)
    END_COPYING_MEMBERS
    unk70 = 0;
END_COPYS

SAVE_OBJ(RndTexBlender, 0x52);

void RndTexBlender::Load(BinStream &bs) {
    LOAD_REVS(bs);
    ASSERT_REVS(2, 0);
    Hmx::Object::Load(bs);
    RndDrawable::Load(bs);
    bs >> mOutputTextures;
    bs >> mBaseMap;
    bs >> mNearMap;
    bs >> mFarMap;
    bs >> mControllerList;
    bs >> mOwner;
    if (gRev > 1)
        bs >> mControllerInfluence;
    else
        mControllerInfluence = 0.7071068f;
    unk70 = 0;
}

float RndTexBlender::GetDistanceToPlane(const Plane &plane, Vector3 &vec) {
    if (mOwner) {
        return mOwner->GetDistanceToPlane(plane, vec);
    } else
        return 0.0f;
}

bool RndTexBlender::MakeWorldSphere(Sphere &sphere, bool b) {
    if (mOwner) {
        return mOwner->MakeWorldSphere(sphere, b);
    } else
        return 0;
}

#ifndef HX_NATIVE
void RndTexBlender::DrawShowing() {}
#else
// The Wii build's DrawShowing is empty: the Wii had no normal maps, and the
// blenders it loads (the band head's wrinkle.texblend and eyes.cfg's feature
// blender) only ever paint normal maps. Natively the band draws the Xbox data,
// where retail composes them, so this is retail Xbox
// RndTexBlender::DrawShowing (rb3-xenon rndobj/TexBlender.cpp): it decides
// what draws, in retail's order, and hands the draws to the GPU backend
// through platform/TexBlendHook.h. Retail's camera dance
// (TheRnd.GetDefaultCam()->SetTargetTex + Select, then the saved camera's
// Select) and its work material only exist to bind the target and the
// unwrapuv shader, which the backend's pass does itself.
// unk9p6 is the Wii slot of retail's re-render flag ("unkc0" in rb3-xenon)
// and unk70 is retail's mRenderedStates. One deviation: retail clears the
// flag before drawing; here it is cleared only once the backend has recorded
// the draws, since the backend declines when no frame is open.
// Opt-out: RB3_NO_WRINKLE_BLEND=1 (the blender draws nothing, as before).
// Probe: RB3_TEXBLEND_PROBE=1 prints a [TEXBLEND] line per blender change.
#include "platform/TexBlendHook.h"
#include "rb3_session_trace.h" // gRB3TraceFrame (RB3_TEXBLEND_AB_FRAME)
#include "rndobj/Mesh.h"
#include "rndobj/PostProc.h"
#include "rndobj/Trans.h"
#include "rndobj/Rnd.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <vector>

namespace {
typedef std::pair<RndTexBlendController *, float> TexBlendEntry;

struct TexBlendSorter {
    bool operator()(const TexBlendEntry &a, const TexBlendEntry &b) const {
        return a.second < b.second;
    }
};

enum {
    kTexBase = 1,
    kTexNear = 2,
    kTexFar = 4,
    kTexCustom = 8,
};

bool TexBlendOptOut() {
    static int s = -1;
    if (s < 0)
        s = getenv("RB3_NO_WRINKLE_BLEND") ? 1 : 0;
    return s != 0;
}

bool TexBlendProbe() {
    static int s = -1;
    if (s < 0)
        s = getenv("RB3_TEXBLEND_PROBE") ? 1 : 0;
    return s != 0;
}

// In-run A/B (probe): RB3_TEXBLEND_AB_FRAME="a-b[,c-d...]" (or a bare "a",
// meaning a to the end). Inside a window every blender draws the first base
// map of its chain alone: wrinkle.texblend's base is norm_output.tex, whose
// blender's base is <gender>_head00_norm.tex, so head_wrinkle_output.tex then
// holds head00, the head normal the band drew before the blenders were
// composed. With the game paused (msg:beatmatch:set_paused:1:0:0) the scene
// holds still, so frames either side of a window edge differ only in the head
// normal. Frames are game-input/trace frames (gRB3TraceFrame).
bool TexBlendABActive(int frame) {
    static std::vector<std::pair<int, int> > sWin;
    static bool sParsed = false;
    if (!sParsed) {
        sParsed = true;
        const char *e = getenv("RB3_TEXBLEND_AB_FRAME");
        while (e && *e) {
            char *end;
            long a = strtol(e, &end, 10);
            if (end == e)
                break;
            long b = 0x7fffffff;
            if (*end == '-')
                b = strtol(end + 1, &end, 10);
            sWin.push_back(std::make_pair((int)a, (int)b));
            e = (*end == ',') ? end + 1 : 0;
        }
    }
    for (size_t i = 0; i < sWin.size(); i++)
        if (frame >= sWin[i].first && frame < sWin[i].second)
            return true;
    return false;
}

// The first base map of a blender chain: while `base` is another blender's
// output, step to that blender's base.
RndTex *TexBlendChainBase(RndTexBlender *self, RndTex *base) {
    ObjectDir *dir = self->Dir();
    for (int depth = 0; base && dir && depth < 4; depth++) {
        RndTexBlender *producer = 0;
        for (ObjDirItr<RndTexBlender> it(dir, true); it != 0; ++it) {
            if (it->mOutputTextures == base && &*it != self) {
                producer = &*it;
                break;
            }
        }
        if (!producer || !producer->mBaseMap)
            break;
        base = producer->mBaseMap;
    }
    return base;
}

const char *TexName(RndTex *t) { return t && t->Name() ? t->Name() : "-"; }

// Probe: one line per blender whenever what it draws changes (states and
// layer count), capped.
void TexBlendProbeLine(
    RndTexBlender *b, RndTex *out, RndTex *base, int states, int nNear, int nFar,
    int nCustom, bool ok
) {
    static std::map<RndTexBlender *, int> sLast;
    static int sLines = 0;
    const int key = states | (nNear << 4) | (nFar << 12) | (nCustom << 20) | (ok ? 1 << 30 : 0);
    std::map<RndTexBlender *, int>::iterator it = sLast.find(b);
    if (it != sLast.end() && it->second == key)
        return;
    sLast[b] = key;
    if (sLines++ >= 400)
        return;
    fprintf(
        stderr,
        "[TEXBLEND] draw '%s' out='%s' %dx%d base='%s' states=0x%x near=%d far=%d "
        "custom=%d ok=%d\n",
        PathName(b), TexName(out), out->Width(), out->Height(), TexName(base), states,
        nNear, nFar, nCustom, (int)ok
    );
}
} // namespace

void RndTexBlender::DrawShowing() {
    NativeTexBlendComposer *composer = GetNativeTexBlendComposer();
    if (!composer || TexBlendOptOut())
        return;
    if (TheRnd->DrawMode() != kDrawNormal)
        return;
    if (!(TheRnd->ProcCmds() & kProcessWorld) && TheRnd->ProcCmds() != kProcessNone)
        return;
    if (!mOutputTextures)
        return;
    if ((mOutputTextures->GetType() & RndTex::kRenderedNoZ) != RndTex::kRenderedNoZ) {
        MILO_NOTIFY_ONCE(
            "%s: \"%s\" must be renderable with no z-buffer", PathName(this),
            mOutputTextures->Name()
        );
        return;
    }
    if (mOutputTextures->Height() * mOutputTextures->Width() > 0x40000) {
        MILO_NOTIFY_ONCE(
            "%s: \"%s\" is %d x %d, must be no larger than 512 x 512", PathName(this),
            mOutputTextures->Name(), mOutputTextures->Height(), mOutputTextures->Width()
        );
    }

    std::vector<TexBlendEntry> nearList;
    std::vector<TexBlendEntry> farList;
    std::vector<TexBlendEntry> customList;
    float influence = mControllerInfluence;
    for (ObjPtrList<RndTexBlendController, ObjectDir>::iterator it = mControllerList.begin();
         it != mControllerList.end();
         ++it) {
        RndTexBlendController *ctrl = *it;
        float blendAmount;
        switch (ctrl->GetBlendState(blendAmount, influence)) {
        case RndTexBlendController::kBlendNear:
            nearList.push_back(TexBlendEntry(ctrl, blendAmount));
            break;
        case RndTexBlendController::kBlendFar:
            farList.push_back(TexBlendEntry(ctrl, blendAmount));
            break;
        case RndTexBlendController::kBlendCustom:
            customList.push_back(TexBlendEntry(ctrl, blendAmount));
            break;
        default:
            break;
        }
    }

    // RB3_TEXBLEND_AB_FRAME (probe, see TexBlendABActive): re-render on every
    // window edge; inside a window, the chain's first base map alone.
    static std::map<RndTexBlender *, bool> sAB;
    const bool ab = TexBlendABActive(gRB3TraceFrame);
    if (ab != sAB[this]) {
        sAB[this] = ab;
        unk9p6 = true;
        fprintf(stderr, "[TEXBLEND] A/B: '%s' %s at trace frame %d\n", PathName(this),
                ab ? "chain base map only" : "composed", gRB3TraceFrame);
    }
    if (ab) {
        nearList.clear();
        farList.clear();
        customList.clear();
    }

    // Nothing moved since the base alone was drawn: the output still holds it.
    if (!unk9p6 && nearList.empty() && farList.empty() && customList.empty()
        && unk70 == kTexBase)
        return;

    std::vector<NativeTexBlendLayer> layers;
    int states = unk70;
    RndTex *base = mBaseMap;
    if (ab)
        base = TexBlendChainBase(this, base);
    if (base)
        states = kTexBase;

    std::sort(nearList.begin(), nearList.end(), TexBlendSorter());
    std::sort(farList.begin(), farList.end(), TexBlendSorter());

    // Near and far: the blender's near or far map through each controller's
    // mesh, alpha = the controller's blend amount, in ascending alpha.
    RndTex *maps[2] = { mNearMap, mFarMap };
    std::vector<TexBlendEntry> *lists[2] = { &nearList, &farList };
    const int bits[2] = { kTexNear, kTexFar };
    for (int k = 0; k < 2; k++) {
        if (!maps[k] || lists[k]->empty())
            continue;
        states |= bits[k];
        for (size_t i = 0; i < lists[k]->size(); i++) {
            RndMesh *mesh = (*lists[k])[i].first->mMesh;
            if (mesh->IsSkinned())
                MILO_NOTIFY_ONCE(
                    "%s: \"%s\" should not be a skinned mesh", PathName(this), mesh->Name()
                );
            NativeTexBlendLayer l = { mesh, maps[k], (*lists[k])[i].second };
            layers.push_back(l);
        }
    }
    // Custom (retail DrawBlendList(customList, kTexCustom)): each controller's
    // own override map, alpha 1 times the influence, in controller-list order.
    if (!customList.empty()) {
        states |= kTexCustom;
        for (size_t i = 0; i < customList.size(); i++) {
            RndTexBlendController *ctrl = customList[i].first;
            RndMesh *mesh = ctrl->mMesh;
            if (mesh->IsSkinned())
                MILO_NOTIFY_ONCE(
                    "%s: \"%s\" should not be a skinned mesh", PathName(this), mesh->Name()
                );
            NativeTexBlendLayer l = { mesh, ctrl->mTex, customList[i].second };
            layers.push_back(l);
        }
    }

    const bool ok = composer->ComposeTexBlend(
        mOutputTextures, base, layers.empty() ? 0 : &layers[0], (int)layers.size()
    );
    if (TexBlendProbe()) {
        TexBlendProbeLine(
            this, mOutputTextures, base, states, (int)nearList.size(), (int)farList.size(),
            (int)customList.size(), ok
        );
        // Once per blender: every controller's decision and inputs.
        static std::map<RndTexBlender *, bool> sListed;
        if (!sListed[this]) {
            sListed[this] = true;
            fprintf(stderr, "[TEXBLEND]  '%s' near='%s' far='%s' influence=%g ctls=%d\n",
                    PathName(this), TexName(mNearMap), TexName(mFarMap), influence,
                    (int)mControllerList.size());
            for (ObjPtrList<RndTexBlendController, ObjectDir>::iterator it =
                     mControllerList.begin();
                 it != mControllerList.end();
                 ++it) {
                RndTexBlendController *c = *it;
                float a;
                int st = c->GetBlendState(a, influence);
                float d = 0;
                c->GetCurrentDistance(d);
                RndMesh *m = c->mMesh;
                fprintf(stderr,
                        "[TEXBLEND]   ctl '%s' state=%d alpha=%.3f dist=%.3f ref=%.3f "
                        "min=%.3f max=%.3f mesh='%s' faces=%d skinned=%d tex='%s'\n",
                        c->Name(), st, a, d, c->mReferenceDistance, c->mMinDistance,
                        c->mMaxDistance, m ? m->Name() : "-",
                        m && m->GeomOwner() ? (int)m->GeomOwner()->Faces().size() : -1,
                        m ? (int)m->IsSkinned() : -1, TexName(c->mTex));
                RndTransformable *o[2] = { c->mObject1, c->mObject2 };
                for (int k = 0; k < 2; k++) {
                    if (!o[k])
                        continue;
                    const Vector3 &v = o[k]->WorldXfm().v;
                    RndTransformable *p = o[k]->TransParent();
                    fprintf(stderr,
                            "[TEXBLEND]     obj%d '%s' %p dir='%s' parent='%s' "
                            "world=(%.3f %.3f %.3f)\n",
                            k + 1, o[k]->Name(), (void *)o[k],
                            o[k]->Dir() ? PathName(o[k]->Dir()) : "-",
                            p ? p->Name() : "-", v.x, v.y, v.z);
                }
            }
        }
    }
    // A frame that could not record (no frame open) keeps the blender dirty so
    // the next one composes it.
    if (!ok)
        return;
    unk9p6 = false;
    unk70 = states;
}
#endif

DataNode RndTexBlender::OnGetRenderTextures(DataArray *arr) {
    return GetRenderTexturesNoZ(Dir());
}

BEGIN_HANDLERS(RndTexBlender)
    HANDLE(get_render_textures, OnGetRenderTextures)
    HANDLE_SUPERCLASS(RndDrawable)
    HANDLE_SUPERCLASS(Hmx::Object)
    HANDLE_CHECK(0x1A5)
END_HANDLERS

BEGIN_PROPSYNCS(RndTexBlender)
    SYNC_PROP(base_map, mBaseMap)
    SYNC_PROP(near_map, mNearMap)
    SYNC_PROP(far_map, mFarMap)
    SYNC_PROP(output_texture, mOutputTextures)
    SYNC_PROP(controller_list, mControllerList)
    SYNC_PROP(owner, mOwner)
    SYNC_PROP(controller_influence, mControllerInfluence)
    SYNC_SUPERCLASS(RndDrawable)
END_PROPSYNCS

DECOMP_FORCEFUNC(TexBlender, RndTexBlender, SetType)
DECOMP_FORCEFUNC_TEMPL(
    TexBlender, ObjPtrList, Replace(0, 0), RndTexBlendController, ObjectDir
)
DECOMP_FORCEFUNC_TEMPL(
    TexBlender, ObjPtrList, RefOwner(), RndTexBlendController, ObjectDir
)
DECOMP_FORCEDTOR(TexBlender, RndTexBlender)