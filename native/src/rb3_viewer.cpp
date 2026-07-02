// rb3-viewer — standalone .milo asset renderer (native, headless).
//
// A CLI wrapper around the proto-viewer render spine in rb3_render_mesh.cpp:
// boots the real curated config, stands up BandRnd (WGPU, headless), registers
// the rndobj + game + CHAR object factories, loads one .milo (plus optional
// dependency milos as subdirs), optionally settles CharHair physics with a
// manually driven TheTaskMgr clock, frames a camera over the scene bounds, and
// writes a PNG.
//
// Purpose: debug rendering bugs on individual assets (first customer: the
// "white wig / long hair" CharHair artifact) WITHOUT booting the whole game.
//
// Triggered by `--viewer` in argv or RB3_VIEWER=1 (see main_native.cpp). This
// mode reuses the LoadMiloAndWalk / RenderFrame / RenderToPng exports from
// rb3_render_mesh.h and adds: char-class factories, --subdir dep loading,
// --sim hair settling, --hide filtering, richer camera CLI, and --list census.
//
// CLI:
//   rb3-native --viewer <milo-rel-path>
//       [--out out.png] [--frames N] [--sim N]
//       [--subdir <milo>]... [--hide substr]... [--only-showing]
//       [--azimuth d --elevation d --distance u | --cam-dir x,y,z]
//       [--width W --height H] [--list] [--verbose]
//
// The milo path is relative to RB3_DATA (default
// /home/free/code/milohax/rb3/orig-assets/extracted) or absolute.

#include "obj/Dir.h"
#include "obj/DirLoader.h"
#include "obj/DataFile.h"
#include "obj/Object.h"
#include "obj/Task.h"
#include "utl/Loader.h"
#include "utl/FilePath.h"
#include "utl/Symbol.h"
#include "os/System.h"

#include "rndobj/Dir.h"
#include "rndobj/Cam.h"
#include "rndobj/Mesh.h"
#include "rndobj/Trans.h"
#include "math/Vec.h"
#include "math/Mtx.h"
#include "math/Rot.h"

// --- char / bandobj factories the char & hair milos instantiate ---
#include "rndobj/Mesh.h"
#include "rndobj/MeshAnim.h"
#include "rndobj/MeshDeform.h"
#include "rndobj/Trans.h"
#include "rndobj/Tex.h"
#include "rndobj/TexBlender.h"
#include "rndobj/TexBlendController.h"
#include "rndobj/Mat.h"
#include "rndobj/Group.h"
#include "rndobj/MatAnim.h"
#include "rndobj/TransAnim.h"
#include "rndobj/PropAnim.h"
#include "rndobj/EventTrigger.h"
#include "rndobj/AmbientOcclusion.h"
#include "char/CharClip.h"
#include "char/CharClipSet.h"
#include "char/CharCollide.h"
#include "char/CharLipSync.h"
#include "char/CharInterest.h"
#include "char/CharFaceServo.h"
#include "char/CharWeightSetter.h"
#include "char/CharHair.h"
#include "char/CharServoBone.h"
#include "bandobj/BandFaceDeform.h"
#include "bandobj/BandCharDesc.h"
#include "bandobj/OutfitConfig.h"

#include "math/Color.h"
#include "gfx/GpuDevice.h"
#include "gfx/Screenshot.h"
#include "platform/Rnd_Wgpu_RB3.h"

#include "rb3_render_mesh.h"

#include <unistd.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <unordered_map>

extern DataArray *gSystemConfig;

// Defined in rb3_render_mesh.cpp / rb3_game_object_factories.cpp.
extern void RB3RegisterGameObjectFactories();

namespace {

struct ViewerArgs {
    const char* miloPath = nullptr;
    const char* outPath = nullptr;              // default /tmp/rb3_viewer.png
    int frames = 1;                             // GPU warmup frames before readback
    int sim = 0;                                // CharHair settle steps (30fps)
    std::vector<std::string> subdirs;           // dependency milos (loaded first)
    std::vector<std::string> hides;             // hide meshes whose name contains substr
    bool onlyShowing = false;
    bool list = false;
    bool verbose = false;
    // camera
    bool hasAzEl = false;
    float azimuth = 20.0f, elevation = 15.0f;   // degrees
    float distance = -1.0f;                     // <=0 = auto
    bool hasCamDir = false;
    float camDir[3] = {0, 0, 0};
    int width = 640, height = 480;
    // v2 inspection
    bool drawDir = false;                       // draw via RndDir::DrawShowing()
    const char* poseDump = nullptr;             // JSON pose dump path (after sim)
    std::vector<std::string> poseDumpBones;     // name-substring filter for the dump
    bool hasTestBone = false;
    std::string testBoneName;
    float testBoneDeg = 0.0f;
    char testBoneAxis = 'z';
    bool noHairParent = false;                  // debug: skip synthetic-parent hookup
};

void Usage() {
    fprintf(stderr,
        "usage: rb3-native --viewer <milo-rel-path> [options]\n"
        "  --out <path.png>       output PNG (default /tmp/rb3_viewer.png)\n"
        "  --frames N             GPU warmup frames before readback (default 1)\n"
        "  --sim N                CharHair settle steps at 30fps before render\n"
        "  --subdir <milo>        pre-load a dependency milo (repeatable)\n"
        "  --hide <substr>        skip meshes whose name contains substr (repeatable)\n"
        "  --only-showing         draw only meshes with Showing()==true\n"
        "  --azimuth d            camera azimuth in degrees (with --elevation)\n"
        "  --elevation d          camera elevation in degrees\n"
        "  --distance u           camera stand-off distance (default auto-frame)\n"
        "  --cam-dir x,y,z        explicit Milo-axis view direction (overrides az/el)\n"
        "  --width W --height H   render size (default 640x480)\n"
        "  --list                 print class/name census and exit (no GPU)\n"
        "  --verbose              extra per-mesh logging\n"
        "  --draw-dir             draw via the dir's RndDir::DrawShowing() (draw-order parity)\n"
        "  --pose-dump <file>     dump every Trans local+world xfm as JSON (AFTER --sim)\n"
        "  --pose-dump-bones csv  comma-separated name substrings to filter the pose dump\n"
        "  --test-bone <name> <deg> [x|y|z]  rotate one Trans from rest before draw\n"
        "  --no-hair-parent       debug: skip the standalone posing shims (parent + rebind)\n");
}

// Parse argv. Returns false on a parse error (usage already printed).
bool ParseArgs(int argc, char** argv, ViewerArgs& a) {
    for (int i = 1; i < argc; i++) {
        const char* s = argv[i];
        if (!strcmp(s, "--viewer")) continue;   // the mode selector itself
        auto next = [&](const char* name) -> const char* {
            if (i + 1 >= argc) { fprintf(stderr, "rb3-viewer: %s needs a value\n", name); return nullptr; }
            return argv[++i];
        };
        if (!strcmp(s, "--out")) { const char* v = next(s); if (!v) return false; a.outPath = v; }
        else if (!strcmp(s, "--frames")) { const char* v = next(s); if (!v) return false; a.frames = atoi(v); }
        else if (!strcmp(s, "--sim")) { const char* v = next(s); if (!v) return false; a.sim = atoi(v); }
        else if (!strcmp(s, "--subdir")) { const char* v = next(s); if (!v) return false; a.subdirs.push_back(v); }
        else if (!strcmp(s, "--hide")) { const char* v = next(s); if (!v) return false; a.hides.push_back(v); }
        else if (!strcmp(s, "--only-showing")) { a.onlyShowing = true; }
        else if (!strcmp(s, "--azimuth")) { const char* v = next(s); if (!v) return false; a.azimuth = (float)atof(v); a.hasAzEl = true; }
        else if (!strcmp(s, "--elevation")) { const char* v = next(s); if (!v) return false; a.elevation = (float)atof(v); a.hasAzEl = true; }
        else if (!strcmp(s, "--distance")) { const char* v = next(s); if (!v) return false; a.distance = (float)atof(v); }
        else if (!strcmp(s, "--cam-dir")) { const char* v = next(s); if (!v) return false;
            if (sscanf(v, "%f,%f,%f", &a.camDir[0], &a.camDir[1], &a.camDir[2]) == 3) a.hasCamDir = true; }
        else if (!strcmp(s, "--width")) { const char* v = next(s); if (!v) return false; a.width = atoi(v); }
        else if (!strcmp(s, "--height")) { const char* v = next(s); if (!v) return false; a.height = atoi(v); }
        else if (!strcmp(s, "--list")) { a.list = true; }
        else if (!strcmp(s, "--verbose")) { a.verbose = true; }
        else if (!strcmp(s, "--draw-dir")) { a.drawDir = true; }
        else if (!strcmp(s, "--no-hair-parent")) { a.noHairParent = true; }
        else if (!strcmp(s, "--pose-dump")) { const char* v = next(s); if (!v) return false; a.poseDump = v; }
        else if (!strcmp(s, "--pose-dump-bones")) { const char* v = next(s); if (!v) return false;
            // split the CSV on commas into substring filters
            const char* p = v;
            while (*p) {
                const char* comma = strchr(p, ',');
                size_t len = comma ? (size_t)(comma - p) : strlen(p);
                if (len > 0) a.poseDumpBones.emplace_back(p, len);
                if (!comma) break;
                p = comma + 1;
            }
        }
        else if (!strcmp(s, "--test-bone")) {
            const char* nm = next(s); if (!nm) return false;
            const char* dg = next(s); if (!dg) return false;
            a.testBoneName = nm; a.testBoneDeg = (float)atof(dg); a.hasTestBone = true;
            // optional trailing axis token (single char x/y/z)
            if (i + 1 < argc && argv[i + 1][1] == '\0' &&
                (argv[i + 1][0] == 'x' || argv[i + 1][0] == 'y' || argv[i + 1][0] == 'z'))
                a.testBoneAxis = argv[++i][0];
        }
        else if (!strcmp(s, "--help") || !strcmp(s, "-h")) { Usage(); return false; }
        else if (s[0] == '-') { fprintf(stderr, "rb3-viewer: unknown option '%s'\n", s); Usage(); return false; }
        else if (!a.miloPath) { a.miloPath = s; }
        else { fprintf(stderr, "rb3-viewer: unexpected extra arg '%s'\n", s); return false; }
    }
    if (!a.miloPath) { fprintf(stderr, "rb3-viewer: missing <milo-rel-path>\n"); Usage(); return false; }
    return true;
}

// Register the char/bandobj factory set a char/hair milo needs. Mirrors
// native/tests/test_charload5b.cpp:RegisterCharLoadFactories plus the three
// viewer-only Inits (CharHair/OutfitConfig/RndAmbientOcclusion) called out in
// scout-rb3-infra.md §3.4. Deliberately does NOT register Character (its ctor
// news CharacterTest -> char_test overlay MILO_FAIL) and does NOT call
// CharInit()/BandInit() wholesale (they carry overlay/preload traps). Idempotent.
void RegisterViewerCharFactories() {
    // rndobj leaves/containers referenced by char & hair milos
    RndMeshAnim::Init();
    RndMeshDeform::Init();
    RndTexBlender::Init();
    RndTexBlendController::Init();
    RndMatAnim::Init();
    RndTransAnim::Init();
    RndPropAnim::Init();
    EventTrigger::Init();
    RndAmbientOcclusion::Init();   // in-game done by Rnd::PreInit; not by the synth harness

    // char objects (the head/skeleton/clip + hair set)
    CharClipSet::Init();
    CharClip::Init();
    CharCollide::Init();
    CharLipSync::Init();
    CharInterest::Init();
    CharFaceServo::Init();
    CharWeightSetter::Init();
    CharServoBone::Init();
    CharHair::Init();
    BandFaceDeform::Init();

    // bandobj outfit tint config — Init() also news the static sMat/sCam/
    // sBandCharDesc used by MatSwap::Compose (call Init, not a bare Register).
    // OutfitConfig::Init does Hmx::Object::New<BandCharDesc>(), so BandCharDesc's
    // factory must be registered first — but only the lightweight Register() (the
    // full BandCharDesc::Init() also ReloadPrefabs() + loads deform.milo, which a
    // static asset render doesn't need). Without this, New<BandCharDesc>() hits
    // "Unknown class BandCharDesc" -> MILO_FAIL -> null sBandCharDesc -> crash.
    BandCharDesc::Register();
    OutfitConfig::Init();
}

// Resolve a possibly-relative milo path to an absolute one anchored at the data
// dir (mirrors RunRenderMesh). Uses the current cwd first if it exists there.
void ResolveMiloPath(const char* in, const char* dataDir, char* out, size_t outSz) {
    if (in[0] == '/') { snprintf(out, outSz, "%s", in); return; }
    if (access(in, F_OK) == 0) {
        char cwd[2048]; getcwd(cwd, sizeof(cwd));
        snprintf(out, outSz, "%s/%s", cwd, in);
    } else {
        snprintf(out, outSz, "%s/%s", dataDir, in);
    }
}

// Print a class/name census of the loaded dir (recursive, includes subdirs).
void PrintCensus(ObjectDir* dir) {
    if (!dir) return;
    printf("=== census: dir '%s' [%s] ===\n",
           dir->Name() ? dir->Name() : "(unnamed)", dir->ClassName().Str());
    int n = 0;
    for (ObjDirItr<Hmx::Object> it(dir, true); it; ++it) {
        Hmx::Object* o = it;
        printf("  [%s] %s\n", o->ClassName().Str(), o->Name() ? o->Name() : "(unnamed)");
        n++;
    }
    printf("=== end census (%d objects) ===\n", n);
}

// Draw one frame walking every drawable RndMesh, applying --hide / --only-showing.
void ViewerDrawFrame(ObjectDir* dir, RndCam* cam, const ViewerArgs& a) {
    if (cam) { RndCam::sCurrent = cam; cam->Select(); }
    gBandRnd.BeginFrame(cam);
    for (ObjDirItr<Hmx::Object> it(dir, true); it; ++it) {
        RndMesh* mesh = dynamic_cast<RndMesh*>((Hmx::Object*)it);
        if (!mesh) continue;
        RndMesh* owner = mesh->GeomOwner(); if (!owner) owner = mesh;
        bool hasGeom = owner->mFaces.size() > 0 &&
                       (owner->mVerts.size() > 0 || owner->mNumCompressedVerts > 0);
        if (!hasGeom) continue;
        if (a.onlyShowing && !mesh->Showing()) continue;
        const char* nm = mesh->Name();
        bool hidden = false;
        if (nm) for (const std::string& h : a.hides) if (strstr(nm, h.c_str())) { hidden = true; break; }
        if (hidden) continue;
        if (mesh->Showing()) mesh->DrawShowing();
        else                 gBandRnd.DrawMesh(mesh);
    }
    gBandRnd.EndFrame();
}

// Drive N CharHair settle steps at 30fps with a manually advanced TaskMgr clock.
// This is what makes hair physics run without a Character (see scout §3.4 /
// scout-dc3-viewer §5): Poll() each CharHair after SetSecondsAndBeat.
void SimulateHair(ObjectDir* dir, int steps, bool verbose) {
    const float dt = 1.0f / 30.0f;
    const float bpm = 120.0f;               // arbitrary; hair sim uses seconds
    int hairCount = 0;
    for (ObjDirItr<CharHair> ci(dir, true); ci; ++ci) hairCount++;
    printf("rb3-viewer: --sim %d steps over %d CharHair object(s)\n", steps, hairCount);
    for (int i = 0; i < steps; i++) {
        float t = (float)(i + 1) * dt;
        float beat = t * (bpm / 60.0f);
        TheTaskMgr.SetSecondsAndBeat(t, beat, false);
        int polled = 0;
        for (ObjDirItr<CharHair> ci(dir, true); ci; ++ci) { ci->Poll(); polled++; }
        if (verbose && (i == 0 || i == steps - 1))
            printf("  sim step %d/%d t=%.3f polled %d hair\n", i + 1, steps, t, polled);
    }
}

// S1 part 3 — bake the simulated WORLD pose into LOCAL xfms. CharHair advances
// the strand by calling RndTransformable::SetWorldXfm(t100) per point, which sets
// mWorldXfm but LEAVES mLocalXfm at its rest value. In-game the mesh draws in the
// same frame with the bone clean, so it reads the moved world. In the viewer the
// draw path re-dirties the bone chain (a downstream WorldXfm_Force), and a dirty
// bone recomputes world = local * parentWorld — from the STALE rest local — so the
// draw silently reverts every strand to rest (MEASURED: pose-dump right after sim
// shows tips moved 10u, but BONE_PROBE at draw reads rest). --test-bone doesn't
// hit this because it sets LOCAL directly.
//
// Fix: after the sim, snapshot every Trans's post-sim world and rewrite each local
// so parent*local reproduces that world (local = world * inverse(parentWorld),
// matching WorldXfm_Force's `world = local * parentWorld`). Any later recompute
// then reproduces the settled pose. Snapshot-first makes it order-independent.
void BakeSimPoseToLocal(ObjectDir* dir) {
    std::vector<RndTransformable*> ts;
    std::unordered_map<RndTransformable*, Transform> snap;
    for (ObjDirItr<RndTransformable> it(dir, true); it; ++it) {
        RndTransformable* t = it;
        ts.push_back(t);
        snap[t] = t->WorldXfm();   // clean cached post-sim world
    }
    for (RndTransformable* t : ts) {
        RndTransformable* p = t->TransParent();
        Transform pw;
        if (p && snap.count(p)) pw = snap[p]; else pw.Reset();
        Transform invpw; Invert(pw, invpw);
        Transform local; Multiply(snap[t], invpw, local);   // world * inv(parentWorld)
        t->SetLocalXfm(local);
    }
}

// S1 — the load-bearing fix. A standalone hair *_resource.milo carries its own
// `bone_hair_*` strand Trans chain, but each strand ROOT is authored parented to
// a head/skeleton bone that lives in the CHARACTER milo, not the resource. Loaded
// alone, that parent ObjPtr resolves to null, so CharHair::SimulateInternal
// (src/system/char/CharHair.cpp:534) and SimulateZeroTime (:706) both early-out
// on `Root() && Root()->TransParent()` — the ENTIRE strand sim is skipped and
// `--sim` is a visual no-op (bones never move; the skinned mesh, which the engine
// DOES draw skinned via owner->IsSkinned(), renders the rest pose every frame).
//
// In-game the head bone supplies the strand's world frame; standalone we
// synthesize an identity-world parent Trans per rootless strand root. With
// recalcLocal=false and an identity parent, the root's world xfm is preserved
// EXACTLY (world = parent.world * local = I * local = old world), so the static
// render is byte-unchanged — only the sim gate now passes. The sim then reads
// t100.m = RootMat * parentWorld.m = RootMat (identity parent = head-upright
// approximation, correct for a standing rest pose) and gravity droops the strands
// while stiffness/length constraints hold the authored fan.
int SetupHairForSim(ObjectDir* dir, bool verbose) {
    int fixed = 0, hairs = 0;
    for (ObjDirItr<CharHair> ci(dir, true); ci; ++ci) {
        CharHair* h = ci;
        hairs++;
        for (int s = 0; s < (int)h->mStrands.size(); s++) {
            CharHair::Strand& strand = h->mStrands[s];
            RndTransformable* root = strand.Root();
            if (!root || root->TransParent()) continue;   // real skeleton present -> leave it
            RndTransformable* parent = Hmx::Object::New<RndTransformable>();
            Transform id; id.Reset();
            parent->SetWorldXfm(id);                       // pin the frame to identity
            root->SetTransParent(parent, false);           // preserve root world xfm
            fixed++;
            if (verbose)
                printf("  hair '%s' strand %d root '%s': synthesized identity parent\n",
                       h->Name() ? h->Name() : "?", s, root->Name() ? root->Name() : "?");
        }
    }
    printf("rb3-viewer: hair-sim setup — %d rootless strand root(s) over %d CharHair "
           "given synthetic parents\n", fixed, hairs);
    return fixed;
}

// S1 part 2 — make the standalone skinned mesh actually POSEABLE. A hair mesh's
// inverse-bind offsets (BoneOffsetAt) were authored against the bones' IN-GAME
// bind world (strand roots parented to the head at a specific character-space
// pose). Loaded alone, the bones sit at their local xfms (no head), so
// skin = offset * boneWorld != identity even at rest: the mesh would fling, and
// the engine's per-bone SKIN_CLAMP (active because mNativeBonesRebound is unset
// with no BandCharacter) freezes the >12u bones back to bind — so the mesh draws
// its stored bind geometry and bone MOTION never reaches the pixels (measured:
// rotating a bound bone 60deg = 0 pixels changed).
//
// Fix, using the engine's own primitive: recompute each bone's inverse-bind
// offset against the CURRENT (rest) pose — SetBone(b, bone, calcOffset=true) sets
// mOffset = meshWorld * inverse(boneWorld), so skin == meshWorld (identity in
// mesh space) at rest. The stored bind verts are unchanged (the rest render is
// the same authored shape), but now any bone displacement from rest deforms the
// mesh correctly. Setting mNativeBonesRebound tells the engine these are
// correctly bound, so the clamp/rebake heuristics skip them. Call at REST, after
// the hair-parent hookup, before --sim / --test-bone.
int RebindSkinnedMeshesToRest(ObjectDir* dir, bool verbose) {
    int meshes = 0, bones = 0;
    for (ObjDirItr<RndMesh> it(dir, true); it; ++it) {
        RndMesh* m = it;
        RndMesh* owner = m->GeomOwner(); if (!owner) owner = m;
        if (!owner->IsSkinned()) continue;
        int nb = owner->NumBones();
        for (int b = 0; b < nb; b++) {
            RndTransformable* bt = owner->BoneTransAt(b);
            if (bt) { owner->SetBone(b, bt, true); bones++; }
        }
        owner->mNativeBonesRebound = true;
        m->mNativeBonesRebound = true;
        meshes++;
        if (verbose)
            printf("  rebound skinned mesh '%s' (%d bones) to rest pose\n",
                   m->Name() ? m->Name() : "?", nb);
    }
    printf("rb3-viewer: skinned rebind — %d mesh(es), %d bone offset(s) recomputed "
           "to rest (skin=identity at rest, motion now deforms)\n", meshes, bones);
    return meshes;
}

// --test-bone: rotate one Trans `deg` degrees about a local axis from its rest
// pose, before the draw. Matches by exact name first, then substring. Applied in
// the bone's local frame (rot * localRotation). If --sim also runs and the bone
// is a hair bone, the sim overwrites it (test-bone is the manual-pose alternative).
void ApplyTestBone(ObjectDir* dir, const std::string& name, float deg, char axis) {
    RndTransformable* target = nullptr;
    for (ObjDirItr<RndTransformable> it(dir, true); it; ++it) {
        const char* nm = it->Name();
        if (nm && !strcmp(nm, name.c_str())) { target = it; break; }
    }
    if (!target) {
        for (ObjDirItr<RndTransformable> it(dir, true); it; ++it) {
            const char* nm = it->Name();
            if (nm && strstr(nm, name.c_str())) { target = it; break; }
        }
    }
    if (!target) {
        fprintf(stderr, "rb3-viewer: --test-bone: no Trans matching '%s'\n", name.c_str());
        return;
    }
    float rad = deg * (float)M_PI / 180.0f, c = cosf(rad), s = sinf(rad);
    Hmx::Matrix3 rot;
    switch (axis) {
    case 'x': rot.x.Set(1, 0, 0); rot.y.Set(0, c, s); rot.z.Set(0, -s, c); break;
    case 'y': rot.x.Set(c, 0, -s); rot.y.Set(0, 1, 0); rot.z.Set(s, 0, c); break;
    default:  rot.x.Set(c, s, 0); rot.y.Set(-s, c, 0); rot.z.Set(0, 0, 1); break; // z
    }
    Transform local = target->LocalXfm();
    Hmx::Matrix3 newm;
    Multiply(rot, local.m, newm);
    local.m = newm;
    target->SetLocalXfm(local);
    printf("rb3-viewer: --test-bone '%s' rotated %.1f deg about %c\n",
           target->Name() ? target->Name() : "?", deg, axis);
}

// --pose-dump: write every RndTransformable's local + world transform as JSON.
// Called AFTER --sim so it captures the simulated pose (the numeric A/B tool the
// wig saga lacked). `filter` (from --pose-dump-bones) keeps only names containing
// one of the given substrings; empty filter dumps everything.
void DumpPose(ObjectDir* dir, const char* path, const std::vector<std::string>& filter) {
    FILE* f = fopen(path, "w");
    if (!f) { fprintf(stderr, "rb3-viewer: pose-dump: can't open %s\n", path); return; }
    fprintf(f, "{\n  \"bones\": [\n");
    int n = 0;
    for (ObjDirItr<RndTransformable> it(dir, true); it; ++it) {
        const char* nm = it->Name() ? it->Name() : "";
        if (!filter.empty()) {
            bool match = false;
            for (const std::string& fs : filter) if (strstr(nm, fs.c_str())) { match = true; break; }
            if (!match) continue;
        }
        const Transform& lx = it->LocalXfm();
        Transform& wx = it->WorldXfm();
        const char* cls = it->ClassName().Str();
        const char* pnm = it->TransParent() && it->TransParent()->Name()
                              ? it->TransParent()->Name() : "";
        if (n > 0) fprintf(f, ",\n");
        fprintf(f,
            "    {\"name\":\"%s\",\"class\":\"%s\",\"parent\":\"%s\","
            "\"local\":{\"m\":[[%.6f,%.6f,%.6f],[%.6f,%.6f,%.6f],[%.6f,%.6f,%.6f]],"
            "\"v\":[%.6f,%.6f,%.6f]},"
            "\"world\":{\"m\":[[%.6f,%.6f,%.6f],[%.6f,%.6f,%.6f],[%.6f,%.6f,%.6f]],"
            "\"v\":[%.6f,%.6f,%.6f]}}",
            nm, cls, pnm,
            lx.m.x.x, lx.m.x.y, lx.m.x.z, lx.m.y.x, lx.m.y.y, lx.m.y.z,
            lx.m.z.x, lx.m.z.y, lx.m.z.z, lx.v.x, lx.v.y, lx.v.z,
            wx.m.x.x, wx.m.x.y, wx.m.x.z, wx.m.y.x, wx.m.y.y, wx.m.y.z,
            wx.m.z.x, wx.m.z.y, wx.m.z.z, wx.v.x, wx.v.y, wx.v.z);
        n++;
    }
    fprintf(f, "\n  ],\n  \"count\": %d\n}\n", n);
    fclose(f);
    printf("rb3-viewer: pose-dump wrote %d transform(s) to %s\n", n, path);
}

// Draw one frame via the dir's own RndDir::DrawShowing() (draw-order / transparency
// parity with in-game), instead of the filtered mesh walk. --hide/--only-showing
// do not apply on this path (the dir controls its own draw order).
void ViewerDrawFrameDir(ObjectDir* dir, RndCam* cam) {
    if (cam) { RndCam::sCurrent = cam; cam->Select(); }
    gBandRnd.BeginFrame(cam);
    if (RndDir* rd = dynamic_cast<RndDir*>(dir)) rd->DrawShowing();
    gBandRnd.EndFrame();
}

}  // namespace

// Entry point for the --viewer / RB3_VIEWER mode. Called from main_native.cpp.
int RunViewer(int argc, char** argv) {
    ViewerArgs a;
    if (!ParseArgs(argc, argv, a)) return 2;

    const char* dataDir = getenv("RB3_DATA");
    if (!dataDir) dataDir = "/home/free/code/milohax/rb3/orig-assets/extracted";

    char absMilo[4096];
    ResolveMiloPath(a.miloPath, dataDir, absMilo, sizeof(absMilo));

    if (a.verbose) setenv("RB3_MESH_VERBOSE", "1", 1);

    // --- GPU FIRST (before chdir), unless --list (census needs no GPU). ---
    // Dawn/Vulkan adapter enumeration wants the clean original cwd.
    if (!a.list) {
        gBandRnd.SetClearColor(Hmx::Color(0.12f, 0.14f, 0.18f));
        bool headless = (getenv("MILO_HEADLESS") != nullptr) || (getenv("DISPLAY") == nullptr);
        if (!getenv("MILO_HEADLESS")) headless = true;   // viewer is always headless
        if (!gBandRnd.InitGpu(a.width, a.height, headless)) {
            fprintf(stderr, "rb3-viewer: GPU init FAILED (sandboxed? re-run with "
                            "dangerouslyDisableSandbox)\n");
            return 2;
        }
    }

    if (chdir(dataDir) != 0) {
        fprintf(stderr, "rb3-viewer: chdir('%s') failed\n", dataDir);
        return 1;
    }
    printf("rb3-viewer: data dir '%s'\n", dataDir);

    TheLoadMgr.mPlatform = kPlatformXBox;
    SetSystemArgs(argc, argv);

    printf("rb3-viewer: SystemPreInit/SystemInit...\n");
    SystemPreInit("config/band_preinit_keep.dta");
    SystemInit("config/band_keep.dta");

    // Factory bring-up: rndobj base + legacy aliases (PreInitRender), game/bandobj
    // Dir + leaf factories, then the char/hair set. PreInitRender only REGISTERS
    // rndobj factories (+ Tex/Text/Dir aliases) reading gSystemConfig — it does NOT
    // touch the GPU — so it is safe (and required) even on the --list no-GPU path:
    // skipping it leaves classes unregistered -> Unknown-class stream desync ->
    // heap corruption on load (trap #4). Only InitGpu is skipped for --list.
    gBandRnd.PreInitRender();
    RB3RegisterGameObjectFactories();
    RegisterViewerCharFactories();

    // --- Load dependency milos first (so cross-milo tex/mesh refs resolve). ---
    // Each is loaded into its own dir then AppendSubDir'd into the subject dir
    // after the subject loads (below).
    std::vector<ObjectDir*> depDirs;
    for (const std::string& sd : a.subdirs) {
        char absDep[4096];
        ResolveMiloPath(sd.c_str(), dataDir, absDep, sizeof(absDep));
        printf("rb3-viewer: loading subdir '%s'\n", absDep);
        ObjectDir* d = DirLoader::LoadObjects(FilePath(absDep), nullptr, nullptr);
        if (d) depDirs.push_back(d);
        else fprintf(stderr, "rb3-viewer: subdir load FAILED: %s\n", absDep);
    }

    // --- Load the subject milo. ---
    printf("rb3-viewer: loading '%s'\n", absMilo);
    ObjectDir* dir = DirLoader::LoadObjects(FilePath(absMilo), nullptr, nullptr);
    if (!dir) {
        fprintf(stderr, "rb3-viewer: DirLoader::LoadObjects returned null\n");
        return 1;
    }

    // Attach dependency dirs as subdirs so name lookups (hair_shared_spec.tex,
    // bone_hair.mesh, ...) resolve, then re-sync references.
    for (ObjectDir* d : depDirs) {
        ObjDirPtr<ObjectDir> ptr(d);
        dir->AppendSubDir(ptr);
    }
    if (RndDir* rd = dynamic_cast<RndDir*>(dir)) rd->SyncObjects();
    else dir->SyncObjects();

    if (a.list) {
        PrintCensus(dir);
        fflush(stdout);
        _exit(0);
    }

    // --- Bounds + camera. ---
    SceneBounds b = ViewerComputeBounds(dir);
    printf("rb3-viewer: dir '%s' [%s] — %d objects, %d drawable meshes, %d cams\n",
           dir->Name() ? dir->Name() : "(unnamed)", dir->ClassName().Str(),
           b.totalObjects, b.meshCount, b.camCount);
    if (b.meshCount == 0) {
        fprintf(stderr, "rb3-viewer: no drawable meshes; nothing to render\n");
        return 1;
    }
    if (!b.valid) {
        fprintf(stderr, "rb3-viewer: could not compute scene bounds\n");
        return 1;
    }

    // Compute the view direction. --cam-dir wins; else az/el spherical (Milo
    // axes: X=right, Y=forward/depth, Z=up). Default = the RENDER_MESH diagonal.
    float dir3[3];
    const float* dirPtr = nullptr;
    if (a.hasCamDir) { dir3[0] = a.camDir[0]; dir3[1] = a.camDir[1]; dir3[2] = a.camDir[2]; dirPtr = dir3; }
    else if (a.hasAzEl) {
        float az = a.azimuth * (float)M_PI / 180.0f;
        float el = a.elevation * (float)M_PI / 180.0f;
        // View direction FROM eye TO center. Eye sits at az/el on a sphere; the
        // look direction is the negation. Milo: y is depth (forward), z is up.
        dir3[0] = -sinf(az) * cosf(el);
        dir3[1] = -cosf(az) * cosf(el);
        dir3[2] = -sinf(el);
        dirPtr = dir3;
    }
    RndCam* cam = ViewerMakeCamera(b, dirPtr, a.distance);
    if (cam) { RndCam::sCurrent = cam; cam->Select(); }

    // --- S1 standalone-posing bring-up (only when the render depends on bone
    // motion: --sim or --test-bone). Two shims, both disabled by --no-hair-parent:
    //   (1) synthesize identity parents for rootless strand roots so the CharHair
    //       sim gate (Root()->TransParent()) passes;
    //   (2) rebake each skinned mesh's inverse-bind offsets to the current rest
    //       pose so bone displacement actually deforms the mesh (and turn off the
    //       engine clamp for those meshes via mNativeBonesRebound).
    // Both run at REST, before any bone is moved. ---
    bool posing = (a.sim > 0) || a.hasTestBone;
    // The engine's V24 SHARD_GUARD drops any skinned mesh whose world extent grows
    // past 2x its bind extent — a game-time safety net for broken band skinning
    // that also drops a legitimately spread hair pose (long strands settling under
    // gravity look "degenerate" to it). A viewer wants to SEE the real pose, so
    // turn the guard off while posing (read per-draw via getenv; leaves the static
    // path untouched so v1 renders stay byte-identical).
    if (posing && !a.noHairParent) setenv("SHARD_GUARD_OFF", "1", 1);
    if (a.sim > 0 && !a.noHairParent) SetupHairForSim(dir, a.verbose);
    if (posing && !a.noHairParent) RebindSkinnedMeshesToRest(dir, a.verbose);

    // Manual bone pose AFTER the rebind (so rest == identity and the rotation
    // deforms); sim overrides hair bones if both are given.
    if (a.hasTestBone) ApplyTestBone(dir, a.testBoneName, a.testBoneDeg, a.testBoneAxis);
    if (a.sim > 0) {
        SimulateHair(dir, a.sim, a.verbose);
        // Persist the settled world pose into local xfms so the draw's world
        // recompute doesn't revert the strands to rest (SetWorldXfm leaves local
        // stale). Skip under --no-hair-parent for A/B against the un-baked path.
        if (!a.noHairParent) BakeSimPoseToLocal(dir);
    }

    // --- Pose dump AFTER sim (captures the simulated pose). ---
    if (a.poseDump) DumpPose(dir, a.poseDump, a.poseDumpBones);

    auto drawOnce = [&]() {
        if (a.drawDir) ViewerDrawFrameDir(dir, cam);
        else           ViewerDrawFrame(dir, cam, a);
    };

    int warmup = a.frames > 1 ? a.frames : 1;
    for (int f = 0; f < warmup - 1; f++) drawOnce();

    // Final frame + PNG. Set the output path through the env var RenderToPng
    // reads, then reuse the shared readback+PNG+_exit path via a WalkResult.
    const char* outPath = a.outPath ? a.outPath : "/tmp/rb3_viewer.png";
    setenv("RB3_RENDER_MESH_PNG", outPath, 1);

    // Draw the final frame ourselves (applies --hide / --draw-dir), then read back
    // directly (so --hide stays applied on the captured frame).
    drawOnce();

    GpuDevice& gpu = gBandRnd.Gpu();
    int gw = gpu.WindowWidth(), gh = gpu.WindowHeight();
    std::vector<uint8_t> pixels((size_t)gw * gh * 4);
    if (!gpu.ReadbackHeadlessFrame(pixels.data(), pixels.size())) {
        fprintf(stderr, "rb3-viewer: readback FAILED\n");
        return 1;
    }
    int clearR = (int)(0.12f * 255), clearG = (int)(0.14f * 255), clearB = (int)(0.18f * 255);
    int nonClear = 0;
    for (size_t i = 0; i < (size_t)gw * gh; i++) {
        const uint8_t* p = &pixels[i * 4];
        if (abs((int)p[0]-clearR) > 14 || abs((int)p[1]-clearG) > 14 || abs((int)p[2]-clearB) > 14)
            nonClear++;
    }
    printf("rb3-viewer: non-clear pixels=%d / %d (%.1f%%)\n",
           nonClear, gw * gh, 100.0 * nonClear / (gw * gh));

    int rc = 0;
    if (WritePNG(outPath, pixels.data(), gw, gh))
        printf("rb3-viewer: wrote %s\n", outPath);
    else { fprintf(stderr, "rb3-viewer: WritePNG FAILED\n"); rc = 1; }

    printf("rb3-viewer: %s\n", rc == 0 ? "OK" : "FAILED");
    fflush(stdout); fflush(stderr);
    _exit(rc);   // dodge the ObjectDir-vs-Dawn static-dtor teardown race
    return rc;
}
