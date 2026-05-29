#include "rndobj/Draw.h"
#include "rndobj/Cam.h"
#include "rndobj/Utl.h"
#include "math/Geo.h"
#include "obj/PropSync_p.h"
#include "utl/Symbols.h"
#ifdef HX_NATIVE
#include "rndobj/Mesh.h"
#include "rndobj/Mat.h"
#include "rndobj/Tex.h"
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <unordered_map>
#include <string>
// === MENU/HUB "black void" fix (N3 re-investigation, 2026-05-29) ===========
//
// Root cause: in the rooftop main-hub shell vignette (world/vignette/shell/gen/
// sv8_a.milo_xbox — the ONLY milo that contains it), `worldcenter.mesh` is a
// small (24-vert) skybox-sized box at the world origin using `worldcenter.mat`
// (color=(0,0,0,1), NO diffuse texture, blend=kBlendSrc, zmode=kZModeNormal so
// it WRITES depth). It draws opaque-black and depth-occludes the actual sky
// backdrop layers that sit behind/around it (`difference_clouds`, `skynight`,
// the BAND3/RB3 `logo`, and the `sky_dome*` cloud domes). Retail layers the
// painted cloud render-target (`clouds_rnd.tex`, a kRenderedNoZ render target)
// over this; on native that RT is never painted (RndTex::MakeDrawTarget is a
// no-op) so its pixels are NULL → sky_dome contributes nothing → the black
// `worldcenter` backdrop is all that fills the upper ~45% of the frame = the
// void. (Confirmed by RB3_CLEAR_COLOR A/B: with worldcenter drawn the upper
// region stays black under a green clear = opaque geometry; skipping just
// `worldcenter.mesh` reveals the cloud sky + RB3 logo.)
//
// Fix (gated, default ON; RB3_MENU_VOID_FIX_OFF=1 restores baseline): skip
// drawing the opaque-black, untextured, depth-writing `worldcenter` backdrop
// box so the textured sky layers behind it show through. Tightly scoped by
// name + opaque-black-untextured-depth-write material signature, so it touches
// ONLY this single cosmetic backdrop mesh (absent from every gameplay venue /
// other vignette) and cannot affect the in-song venue, highway, HUD, or
// song-load path.
static bool MenuVoidIsWorldcenterOccluder(RndMesh *m, const char *nm) {
    if (!strstr(nm, "worldcenter"))
        return false;
    RndMat *mat = m->Mat();
    if (!mat)
        return false;
    // Opaque, pure-black, untextured, depth-writing backdrop box only.
    const Hmx::Color &c = mat->GetColor();
    bool black = c.red < 0.02f && c.green < 0.02f && c.blue < 0.02f && c.alpha > 0.98f;
    bool untextured = mat->GetDiffuseTex() == nullptr;
    bool writesDepth = mat->GetZMode() == RndMat::kZModeNormal;
    return black && untextured && writesDepth;
}

// Per-drawable native hook. Returns true if the caller should SKIP drawing.
//   - Applies the menu-void fix (skip the worldcenter black backdrop occluder,
//     default ON; RB3_MENU_VOID_FIX_OFF=1 disables).
//   - MENU_VOID_DBG2[=2]: one-shot-per-name material dump for hub-backdrop
//     drawables (=2 = every mesh). Confirms which meshes reach Draw()/DrawBudget
//     and their material state (color / prelit / blend / zmode / diffuse-tex /
//     render-target / vert-extent). Render-inert.
//   - MENU_VOID_SKIP=<comma-substrings>: skip-draw any mesh whose name matches
//     (diagnostic A/B of which drawable paints a given screen region).
static bool MenuVoidDrawHook(RndDrawable *d) {
    static int sEnabled = -1;
    static const char *sSkip = nullptr;
    static int sFixOff = -1;
    if (sEnabled < 0) {
        sEnabled = getenv("MENU_VOID_DBG2") ? 1 : 0;
        sSkip = getenv("MENU_VOID_SKIP");
        sFixOff = getenv("RB3_MENU_VOID_FIX_OFF") ? 1 : 0;
    }
    RndMesh *m = dynamic_cast<RndMesh *>(d);
    if (!m)
        return false;
    const char *nm = m->Name();
    if (!nm)
        return false;
    // --- The fix: skip the worldcenter black backdrop occluder (default ON). ---
    if (!sFixOff && MenuVoidIsWorldcenterOccluder(m, nm))
        return true;
    if (!sEnabled && !sSkip)
        return false;
    if (sSkip) {
        // Comma/space-separated substrings; skip-draw any matching mesh.
        const char *s = sSkip;
        char tok[64];
        while (*s) {
            while (*s == ',' || *s == ' ') s++;
            int i = 0;
            while (*s && *s != ',' && *s != ' ' && i < 63) tok[i++] = *s++;
            tok[i] = 0;
            if (i > 0 && strstr(nm, tok))
                return true;
        }
    }
    if (!sEnabled)
        return false;
    // MENU_VOID_DBG2=2 dumps EVERY mesh (find large dark backdrop panels), =1
    // dumps only sky-keyed meshes.
    static int sAll = -1;
    if (sAll < 0) { const char *e = getenv("MENU_VOID_DBG2"); sAll = (e && e[0]=='2') ? 1 : 0; }
    bool key = sAll || strstr(nm, "sky") || strstr(nm, "Sky") || strstr(nm, "dome")
        || strstr(nm, "moon") || strstr(nm, "cloud") || strstr(nm, "star")
        || strstr(nm, "night") || strstr(nm, "bgbuilding") || strstr(nm, "fog");
    if (!key)
        return false;
    static std::unordered_map<std::string, int> sSeen;
    if (sSeen[nm]++ != 0)
        return false;
    RndMat *mat = m->Mat();
    const Transform &wx = m->WorldXfm();
    if (mat) {
        const Hmx::Color &c = mat->GetColor();
        RndTex *dt = mat->GetDiffuseTex();
        fprintf(stderr,
            "[MENU_VOID_DBG2] mesh='%s' showing=%d mat='%s' color=(%.3f,%.3f,%.3f,%.3f) "
            "prelit=%d intensify=%d blend=%d zmode=%d alphaCut=%d useEnviron=%d "
            "diffuse=%s tex(%dx%d bpp=%d type=0x%x) pos=(%.1f,%.1f,%.1f)\n",
            nm, (int)m->mShowing, mat->Name() ? mat->Name() : "?",
            c.red, c.green, c.blue, c.alpha,
            (int)mat->mPreLit, (int)mat->mIntensify, (int)mat->GetBlend(),
            (int)mat->GetZMode(), (int)mat->mAlphaCut, (int)mat->mUseEnviron,
            dt ? (dt->Name() ? dt->Name() : "(unnamed)") : "(null)",
            dt ? dt->Width() : 0, dt ? dt->Height() : 0,
            dt ? dt->Bpp() : 0, dt ? (int)dt->GetType() : -1,
            wx.v.x, wx.v.y, wx.v.z);
        if (dt)
            fprintf(stderr,
                "[MENU_VOID_DBG2]   diffuse '%s' isRenderTarget=%d pixels=%s bmp(%dx%d bpp=%d)\n",
                dt->Name() ? dt->Name() : "?", (int)dt->IsRenderTarget(),
                dt->mBitmap.Pixels() ? "PRESENT" : "NULL",
                dt->mBitmap.Width(), dt->mBitmap.Height(), dt->mBitmap.Bpp());
        // Local-space vertex extent (uncompressed verts only) for size triage.
        RndMesh *owner = m->GeomOwner() ? m->GeomOwner() : m;
        int nv = owner->mVerts.size();
        if (nv > 0) {
            float lo[3] = {1e9f, 1e9f, 1e9f}, hi[3] = {-1e9f, -1e9f, -1e9f};
            for (int i = 0; i < nv; i++) {
                const RndMesh::Vert &v = owner->mVerts[i];
                float p[3] = {v.pos.x, v.pos.y, v.pos.z};
                for (int k = 0; k < 3; k++) { if (p[k] < lo[k]) lo[k] = p[k]; if (p[k] > hi[k]) hi[k] = p[k]; }
            }
            fprintf(stderr,
                "[MENU_VOID_DBG2]   verts=%d localBox min(%.1f,%.1f,%.1f) max(%.1f,%.1f,%.1f)\n",
                nv, lo[0], lo[1], lo[2], hi[0], hi[1], hi[2]);
        } else {
            fprintf(stderr, "[MENU_VOID_DBG2]   verts=%d (compressed=%u)\n",
                nv, owner->mNumCompressedVerts);
        }
    } else {
        fprintf(stderr, "[MENU_VOID_DBG2] mesh='%s' showing=%d mat=NULL pos=(%.1f,%.1f,%.1f)\n",
                 nm, (int)m->mShowing, wx.v.x, wx.v.y, wx.v.z);
    }
    return false;
}
#endif

HighlightStyle RndDrawable::sHighlightStyle;
bool RndDrawable::sForceSubpartSelection;
float RndDrawable::sNormalDisplayLength = 1.0f;
int DRAW_REV = 3;

RndDrawable::RndDrawable() : mShowing(1), mSphere(), mOrder(0.0f) { mSphere.Zero(); }

void RndDrawable::Draw() {
    if (mShowing) {
#ifdef HX_NATIVE
        if (MenuVoidDrawHook(this)) return;
        // Frustum culling disabled for the native build: the WebGPU renderer's
        // camera/frustum setup does not yet match RndCam::sCurrent's, so culling
        // here would wrongly drop visible drawables. Over-draw is harmless.
        DrawShowing();
#else
        Sphere sphere;
        int worldSphere = MakeWorldSphere(sphere, false);
        if (worldSphere == 0 || !RndCam::sCurrent->CompareSphereToWorld(sphere)) {
            DrawShowing();
        }
#endif
    }
}

bool RndDrawable::DrawBudget(float f) {
    if (!mShowing)
        return true;
    else {
#ifdef HX_NATIVE
        if (MenuVoidDrawHook(this)) return true;
        // SMASHER_DRAW_FIX: frustum culling disabled for the native build, exactly
        // as in RndDrawable::Draw() above. The WebGPU renderer's frustum does not
        // match RndCam::sCurrent's, so CompareSphereToWorld wrongly culls visible
        // drawables. This is the budget-traversal twin of the Draw() patch: every
        // group/dir that draws via DrawShowingBudget (RndGroup::DrawShowingBudget,
        // the smasher plate's before_gems/after_gems chain) recurses through
        // DrawBudget on its members — without this, the gem_smasher / strike-plate
        // meshes reached DrawBudget but were frustum-culled before DrawShowing, so
        // they never hit BandRnd::DrawMesh. Over-draw is harmless.
        return DrawShowingBudget(f);
#else
        Sphere sphere;
        int worldSphere = MakeWorldSphere(sphere, false);
        if (worldSphere != 0 && RndCam::sCurrent->CompareSphereToWorld(sphere)) {
            return true;
        } else
            return DrawShowingBudget(f);
#endif
    }
}

bool RndDrawable::DrawShowingBudget(float f) {
    DrawShowing();
    return true;
}

void RndDrawable::Highlight() {
    if (sHighlightStyle != kHighlightNone) {
        Sphere s;
        if (!MakeWorldSphere(s, false) || !RndCam::sCurrent->CompareSphereToWorld(s)) {
            bool showing = mShowing;
            mShowing = true;
#ifdef VERSION_SZBE69_B8
            UtilDrawSphere(s.center, s.radius, Hmx::Color(1.0f, 1.0f, 0.0f));
#else
            UtilDrawSphere(s.center, s.GetRadius(), Hmx::Color(1.0f, 1.0f, 0.0f));
#endif
            mShowing = showing;
        }
    }
}

BEGIN_COPYS(RndDrawable)
    CREATE_COPY(RndDrawable)
    BEGIN_COPYING_MEMBERS
        if (ty != kCopyFromMax) {
            COPY_MEMBER(mShowing)
            COPY_MEMBER(mOrder)
            COPY_MEMBER(mSphere)
        } else {
#ifdef VERSION_SZBE69_B8
            float zero = 0.0f;
            float rad = mSphere.GetRadius();
            if (rad != zero) {
                rad = c->mSphere.GetRadius();
                if (rad != zero) {
                    COPY_MEMBER(mSphere)
                }
            }
#else
            if (mSphere.GetRadius() && c->mSphere.GetRadius()) {
                COPY_MEMBER(mSphere);
            }
#endif
        }
    END_COPYING_MEMBERS
END_COPYS

SAVE_OBJ(RndDrawable, 0xAE)

void RndDrawable::Load(BinStream &bs) {
    int rev;
    bs >> rev;
    ASSERT_GLOBAL_REV(rev, DRAW_REV);
    if (gLoadingProxyFromDisk) {
        bool dummy;
        bs >> dummy;
    } else {
        bool bs_showing;
        bs >> bs_showing;
        mShowing = bs_showing;
    }
    if (rev < 2) {
        int count;
        bs >> count;
        RndGroup *grp = dynamic_cast<RndGroup *>(this);
        if (count != 0) {
            for (; count != 0; count--) {
                char buf[0x80];
                bs.ReadString(buf, 0x80);
                if (grp) {
                    Hmx::Object *found = Dir()->Find<Hmx::Object>(buf, true);
                    RndEnviron *env = dynamic_cast<RndEnviron *>(found);
                    if (env) {
                        if (grp->GetEnv())
                            MILO_WARN("%s won't set %s", grp->Name(), buf);
                        else
                            grp->SetEnv(env);
                    } else {
                        RndCam *cam = dynamic_cast<RndCam *>(found);
                        if (!cam) {
                            grp->RemoveObject(found);
                            grp->AddObject(found, 0);
                        }
                    }
                } else
                    MILO_WARN("%s not in group", buf);
            }
        }
    }
    if (rev > 0)
        bs >> mSphere;
    if (rev > 2) {
        if (gLoadingProxyFromDisk) {
            float dummy;
            bs >> dummy;
        } else
            bs >> mOrder;
    }
}

void RndDrawable::DumpLoad(BinStream &bs) {
    unsigned char dummy;
    int y, x, w;
    int rev;
    int i, j;
    int z;
    bs >> rev;
    MILO_ASSERT(rev < 4, 0xFD);
    bs >> dummy;
    if (rev < 2) {
        char buf[0x80];
        bs >> i;
        for (; i != 0; i--) {
            bs.ReadString(buf, 0x80);
        }
    }
    if (rev > 0) {
#ifdef VERSION_SZBE69_B8
        bs >> w >> x >> y >> z;
#else
        Sphere s;
        bs >> s;
#endif
    }
    if (rev > 2) {
        bs >> j;
    }
    if (rev > 3) {
        ObjPtr<RndDrawable> ptr(nullptr);
        bs >> ptr;
    }
}

bool RndDrawable::CollideSphere(const Segment &seg) {
    if (!mShowing)
        return false;
    else {
        Sphere sphere;
        if (MakeWorldSphere(sphere, false) && !Intersect(seg, sphere))
            return false;
        else
            return true;
    }
}

RndDrawable *RndDrawable::Collide(const Segment &seg, float &f, Plane &plane) {
    START_AUTO_TIMER("collide");
    if (!CollideSphere(seg))
        return nullptr;
    else
        return CollideShowing(seg, f, plane);
}

// retail: https://decomp.me/scratch/X3MyB
// debug: https://decomp.me/scratch/rLOfM
int RndDrawable::CollidePlane(const Plane &plane) {
    if (!mShowing)
        return -1;
    else {
        Sphere sphere;
        if (MakeWorldSphere(sphere, false)) {
            if (sphere >= plane)
                return 1;
            else
                return -(sphere < plane);
        } else
            return -1;
    }
}

void RndDrawable::CollideList(
    const Segment &seg, std::list<RndDrawable::Collision> &collisions
) {
    float f;
    Plane pl;
    RndDrawable *draw = Collide(seg, f, pl);
    if (draw) {
        RndDrawable::Collision coll;
        coll.object = draw;
        coll.distance = f;
        coll.plane = pl;
        collisions.push_back(coll);
    }
}

BEGIN_HANDLERS(RndDrawable)
    HANDLE(set_showing, OnSetShowing)
    HANDLE(showing, OnShowing)
    HANDLE(zero_sphere, OnZeroSphere)
    HANDLE_ACTION(update_sphere, UpdateSphere())
    HANDLE(get_sphere, OnGetSphere)
    HANDLE(copy_sphere, OnCopySphere)
    HANDLE_CHECK(0x168)
END_HANDLERS

DataNode RndDrawable::OnCopySphere(const DataArray *da) {
    RndDrawable *draw = da->Obj<RndDrawable>(2);
    if (draw)
        mSphere = draw->mSphere;
    return 0;
}

DataNode RndDrawable::OnGetSphere(const DataArray *da) {
    *da->Var(2) = mSphere.center.X();
    *da->Var(3) = mSphere.center.Y();
    *da->Var(4) = mSphere.center.Z();
    *da->Var(5) = mSphere.GetRadius();
    return 0;
}

DataNode RndDrawable::OnSetShowing(const DataArray *da) {
    SetShowing(da->Int(2));
    return 0;
}

DataNode RndDrawable::OnShowing(const DataArray *) { return mShowing; }

DataNode RndDrawable::OnZeroSphere(const DataArray *) {
    mSphere.Zero();
    return 0;
}

BEGIN_PROPSYNCS(RndDrawable)
    SYNC_PROP(draw_order, mOrder)
    static Symbol _s("showing");
    if (sym == _s) {
        if (_op == kPropSet) {
            mShowing = _val.Int();
        } else {
            _val = mShowing;
        }
        return true;
    }
    SYNC_PROP(sphere, mSphere);
END_PROPSYNCS
