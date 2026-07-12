// rb3 Native Port — Wave-31 Lane D (W31-HUBWALKER-SHARDS) READ-ONLY SHARD PROBE.
//
// Purpose: the STEP-0 discriminator instrument for the hub street-walker shard
// family (forehead flesh cone, waist stick-fans, crumpled boots). It answers the
// three checkpointed discriminators WITHOUT any fix code (three-supersessions rule):
//   (i)   NAME the shard meshes + their bound bones — matrix-relative + pointer-
//         verified (lint 1), per-walker (lint 2). Walks each player%d BandCharacter
//         dir for skinned RndMesh; per mesh emits name / NumVerts / NumBones and, per
//         bound bone, {name, BoneTransAt pointer, WorldXfm, BoneOffsetAt (inverse
//         bind)} plus a vertex-weight histogram (how many verts have their PRIMARY
//         weight on each bone) so the fan-apex bone is identifiable.
//   (ii)  are those bones driven by the walk clips? — enumerates each CharDriver's
//         playing CharClip and calls CharClip::ListBones() to dump the driven
//         bone-track name set; the join checks whether a shard-mesh bone's channel
//         is present.
//   (iii) SKEL 87-deg seed-R family vs undriven-track gap — per bound bone we emit
//         d_angle_deg = rotation angle of (WorldXfm.basis * BoneOffset.basis), which
//         is 0 at bind and grows as the bone animates AWAY from bind. A shard-apex
//         bone frozen at d~=0 across frames whose track is ABSENT from ListBones =>
//         undriven-track; a coherent ~87 deg on a DRIVEN bone => SKEL-family.
//
// PATTERN (identical to rb3_bonedump_native.cpp): additive, env-gated TU including
// decomp headers (rides the target-wide MWCC compat flags), ONE extern "C" entry
// reached over /api/call (RB3_REPLAY_API=1), read-only live-state reads on the MAIN
// thread, touches NO engine source, inert unless RB3_SHARD_PROBE_OUT is set. It is a
// DISTINCT TU (NOT BandCharacter.cpp / BandCamShot.cpp — Lane A exclusive-write, A2).
//
// POINTER-VERIFICATION (anti-magnet, HANDS-ADJUDICATION/VERDICT.md): the shared
// "bound" magnet skeleton has the SAME bone pointer across members and does not
// animate; the per-member "own" skeleton has DISTINCT pointers. We record char_addr
// per walker and BoneTransAt pointer per mesh bone so the analysis keys by pointer,
// stating the name-key caveat (E7).

#ifdef HX_NATIVE

#include "obj/Object.h"
#include "obj/Dir.h"
#include "math/Mtx.h"              // Transform / Matrix3 / Vector3
#include "rndobj/Trans.h"         // RndTransformable::WorldXfm()/TransParent()
#include "rndobj/Mesh.h"          // RndMesh: NumBones/BoneTransAt/BoneOffsetAt/Verts
#include "bandobj/BandCharacter.h"
#include "bandobj/BandCharDesc.h" // Gender()
#include "bandobj/BandDirector.h"
#include "meta_band/CharCache.h"  // TheCharCache->GetCharacter(slot) (hub walkers)
#include "char/CharDriver.h"
#include "char/CharClip.h"
#include "char/CharBones.h"       // CharBones::Bone (ListBones)

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <list>
#include <string>

namespace {

const char* ProbeOut() {
    const char* e = getenv("RB3_SHARD_PROBE_OUT");
    return (e && e[0]) ? e : nullptr;
}

std::string JEsc(const char* s) {
    std::string o;
    if (!s) return o;
    for (const char* p = s; *p; ++p) {
        char c = *p;
        if (c == '"' || c == '\\') { o += '\\'; o += c; }
        else if (c == '\n') o += "\\n";
        else o += c;
    }
    return o;
}

void EmitRows(FILE* f, const Transform& t) {
    fprintf(f,
        "\"rows\":[[%.6g,%.6g,%.6g],[%.6g,%.6g,%.6g],[%.6g,%.6g,%.6g]],"
        "\"trans\":[%.6g,%.6g,%.6g]",
        t.m.x.x, t.m.x.y, t.m.x.z,
        t.m.y.x, t.m.y.y, t.m.y.z,
        t.m.z.x, t.m.z.y, t.m.z.z,
        t.v.x, t.v.y, t.v.z);
}

// Rotation angle (deg) of the 3x3 product P = A * B (row-vector basis, matching the
// x/y/z Vector3 rows used across the codebase). At bind, WorldXfm.basis is the
// inverse of BoneOffset.basis so P == Identity => 0 deg. Uses acos((trace-1)/2),
// clamped; scale ~1 for band skeletons so this reads as a pure bone-deviation angle.
float DeviationAngleDeg(const Hmx::Matrix3& a, const Hmx::Matrix3& b) {
    // P = a * b   (P.row_i = sum_k a.row_i[k] * b.row_k)
    const Vector3* ar[3] = { &a.x, &a.y, &a.z };
    const Vector3* br[3] = { &b.x, &b.y, &b.z };
    float p[3][3];
    for (int i = 0; i < 3; i++) {
        const float ai[3] = { ar[i]->x, ar[i]->y, ar[i]->z };
        for (int j = 0; j < 3; j++) {
            p[i][j] = ai[0]*(&br[0]->x)[j] + ai[1]*(&br[1]->x)[j] + ai[2]*(&br[2]->x)[j];
        }
    }
    float trace = p[0][0] + p[1][1] + p[2][2];
    float c = (trace - 1.0f) * 0.5f;
    if (c > 1.0f) c = 1.0f;
    if (c < -1.0f) c = -1.0f;
    return acosf(c) * (180.0f / 3.14159265358979f);
}

// (ii): the driven bone-track name set for the member's body clip(s). We list bones
// for the FIRST-PLAYING clip of every CharDriver in the dir; ListBones is const-ish
// (fills a std::list, no mutation of engine state). Emits a capped, de-duplicated
// name array + the total count. Read-only.
void DumpDrivenTracks(FILE* f, BandCharacter* bc) {
    fprintf(f, "\"drivers\":[");
    int nd = 0;
    for (ObjDirItr<CharDriver> it(bc, true); it != 0; ++it) {
        CharDriver* dr = it;
        CharClipDriver* fp = dr->FirstPlaying();
        CharClip* clip = fp ? fp->GetClip() : nullptr;
        const char* drName = dr->Name() ? dr->Name() : "";
        const char* clipName = (clip && clip->Name()) ? clip->Name() : "";
        if (nd > 0) fprintf(f, ",");
        fprintf(f, "{\"driver\":\"%s\",\"clip\":\"%s\",\"driver_addr\":\"%p\",\"tracks\":[",
                JEsc(drName).c_str(), JEsc(clipName).c_str(), (void*)dr);
        int nt = 0;
        if (clip) {
            std::list<CharBones::Bone> bones;
            clip->ListBones(bones);
            for (std::list<CharBones::Bone>::iterator bi = bones.begin();
                 bi != bones.end() && nt < 400; ++bi) {
                const char* tn = bi->name.Str() ? bi->name.Str() : "";
                if (nt > 0) fprintf(f, ",");
                fprintf(f, "\"%s\"", JEsc(tn).c_str());
                nt++;
            }
        }
        fprintf(f, "],\"track_count\":%d}", nt);
        nd++;
    }
    fprintf(f, "]");
}

bool NameHasAny(const char* nm, const char* const* toks) {
    if (!nm) return false;
    char low[160];
    size_t i = 0;
    for (; nm[i] && i < sizeof(low) - 1; i++) {
        char c = nm[i];
        low[i] = (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : c;
    }
    low[i] = 0;
    for (const char* const* t = toks; *t; ++t)
        if (strstr(low, *t)) return true;
    return false;
}

// Suspect shard-mesh name tokens: head/scalp/hair/face (forehead cone), waist/belt/
// hip/pelvis/prop/mic/stand (waist fans), boot/shoe/foot/leg (crumpled boots).
const char* const kSuspectTokens[] = {
    "head", "scalp", "hair", "face", "brow", "wig", "horn",
    "waist", "belt", "hip", "pelvis", "prop", "mic", "stand", "spine",
    "boot", "shoe", "foot", "leg", "knee", "ankle", "toe",
    nullptr
};

// Dump one walker's skinned meshes. focusOnly => emit full per-bone detail only for
// suspect-named meshes; all other skinned meshes get a compact summary row
// (name, verts, bones, max d_angle, apex bone). Returns skinned-mesh count.
int DumpWalker(FILE* f, int slot, const char* source, BandCharacter* bc, bool& first) {
    const char* nm = bc->Name() ? bc->Name() : "";
    const char* gender = bc->Gender().Str() ? bc->Gender().Str() : "";
    if (!first) fprintf(f, ",\n");
    first = false;

    int totalMeshes = 0, skinnedMeshes = 0;
    for (ObjDirItr<RndMesh> it(bc, true); it != 0; ++it) {
        totalMeshes++;
        RndMesh* o = it->GeomOwner() ? it->GeomOwner() : (RndMesh*)it;
        if (o->IsSkinned()) skinnedMeshes++;   // skinning lives on the GEOM OWNER
    }

    fprintf(f,
        "  {\"slot\":%d,\"source\":\"%s\",\"name\":\"%s\",\"gender\":\"%s\","
        "\"char_addr\":\"%p\",\"loading\":%d,\"total_meshes\":%d,\"skinned_meshes\":%d,",
        slot, source, JEsc(nm).c_str(), JEsc(gender).c_str(),
        (void*)bc, bc->IsLoading() ? 1 : 0, totalMeshes, skinnedMeshes);
    DumpDrivenTracks(f, bc);
    fprintf(f, ",\"meshes\":[\n");

    int nm_emitted = 0;
    for (ObjDirItr<RndMesh> it(bc, true); it != 0; ++it) {
        RndMesh* mesh = it;                       // drawn node (name + mesh WorldXfm)
        RndMesh* owner = mesh->GeomOwner() ? mesh->GeomOwner() : mesh; // skin source
        if (!owner->IsSkinned()) continue;
        const char* mname = mesh->Name() ? mesh->Name() : "";
        int nbones = owner->NumBones();
        int nverts = owner->NumVerts();
        bool suspect = NameHasAny(mname, kSuspectTokens);

        // mesh-local skin transform per bone: skin = BoneOffset(b) * boneWorld, then
        // localExtent = |(skin * inverse(meshWorld)).v| — the EXACT engine SKIN_CLAMP
        // metric (Rnd_Wgpu_RB3.cpp:3456-3459, >12u => shard). d_angle = rotation of
        // (boneWorld.basis * BoneOffset.basis) from identity (0 at bind).
        Transform meshWorld = mesh->WorldXfm();
        Transform invMeshWorld; Invert(meshWorld, invMeshWorld);

        // vertex-weight histogram: primary-weight vert count per bone index.
        std::string hist;
        {
            static const int kMaxB = 128;
            int cnt[kMaxB];
            for (int i = 0; i < kMaxB; i++) cnt[i] = 0;
            RndMesh::VertVector& vv = owner->Verts();
            int vcount = vv.size();  // 0 if compressed verts were cleared (safe)
            for (int v = 0; v < vcount; v++) {
                RndMesh::Vert& vt = vv[v];
                float w[4] = { vt.boneWeights.FloatAt(0), vt.boneWeights.FloatAt(1),
                               vt.boneWeights.FloatAt(2), vt.boneWeights.FloatAt(3) };
                int best = 0; float bw = w[0];
                for (int k = 1; k < 4; k++) if (w[k] > bw) { bw = w[k]; best = k; }
                int bi = vt.boneIndices[best];
                if (bi >= 0 && bi < nbones && bi < kMaxB) cnt[bi]++;
            }
            char tmp[48];
            for (int b = 0; b < nbones && b < kMaxB; b++) {
                if (cnt[b] == 0) continue;
                snprintf(tmp, sizeof(tmp), "%s%d:%d", hist.empty() ? "" : ",", b, cnt[b]);
                hist += tmp;
            }
        }

        // apex = the bone with the largest mesh-local skin extent (the shard driver).
        float maxExt = -1.0f, maxDev = -1.0f; int apex = -1;
        for (int b = 0; b < nbones; b++) {
            RndTransformable* bt = owner->BoneTransAt(b);
            if (!bt) continue;
            const Transform& bw = bt->WorldXfm();
            if (!(std::fabs(bw.v.x) < 1e5f && std::fabs(bw.v.y) < 1e5f &&
                  std::fabs(bw.v.z) < 1e5f)) continue;   // runaway -> skip metric
            Transform sk;  Multiply(owner->BoneOffsetAt(b), bw, sk);
            Transform loc; Multiply(sk, invMeshWorld, loc);
            float ext = sqrtf(loc.v.x*loc.v.x + loc.v.y*loc.v.y + loc.v.z*loc.v.z);
            float d = DeviationAngleDeg(bw.m, owner->BoneOffsetAt(b).m);
            if (d > maxDev) maxDev = d;
            if (ext > maxExt) { maxExt = ext; apex = b; }
        }

        if (nm_emitted > 0) fprintf(f, ",\n");
        fprintf(f,
            "    {\"mesh\":\"%s\",\"owner\":\"%s\",\"suspect\":%d,\"verts\":%d,\"bones\":%d,"
            "\"max_ext\":%.3f,\"max_dev_deg\":%.3f,\"apex_bone\":%d,\"vhist\":\"%s\"",
            JEsc(mname).c_str(), owner->Name() ? JEsc(owner->Name()).c_str() : "",
            suspect ? 1 : 0, nverts, nbones, maxExt, maxDev, apex, hist.c_str());

        // Emit per-bone detail for suspect meshes OR any mesh with a >12u shard bone.
        if (suspect || maxExt > 12.0f) {
            fprintf(f, ",\"bone_detail\":[");
            int emitted = 0;
            for (int b = 0; b < nbones; b++) {
                RndTransformable* bt = owner->BoneTransAt(b);
                if (emitted > 0) fprintf(f, ",");
                if (!bt) { fprintf(f, "{\"idx\":%d,\"bone\":null}", b); emitted++; continue; }
                const char* bnm = bt->Name() ? bt->Name() : "";
                const Transform& bw = bt->WorldXfm();
                bool runaway = !(std::fabs(bw.v.x) < 1e5f && std::fabs(bw.v.y) < 1e5f &&
                                 std::fabs(bw.v.z) < 1e5f);
                float ext = -1.0f, d = -1.0f;
                if (!runaway) {
                    Transform sk;  Multiply(owner->BoneOffsetAt(b), bw, sk);
                    Transform loc; Multiply(sk, invMeshWorld, loc);
                    ext = sqrtf(loc.v.x*loc.v.x + loc.v.y*loc.v.y + loc.v.z*loc.v.z);
                    d = DeviationAngleDeg(bw.m, owner->BoneOffsetAt(b).m);
                }
                const char* pnm = (bt->TransParent() && bt->TransParent()->Name())
                                    ? bt->TransParent()->Name() : "";
                fprintf(f,
                    "{\"idx\":%d,\"bone\":\"%s\",\"parent\":\"%s\",\"bone_addr\":\"%p\","
                    "\"runaway\":%d,\"ext\":%.3f,\"d_angle_deg\":%.3f}",
                    b, JEsc(bnm).c_str(), JEsc(pnm).c_str(), (void*)bt,
                    runaway ? 1 : 0, ext, d);
                emitted++;
            }
            fprintf(f, "]");
        }
        fprintf(f, "}");
        nm_emitted++;
    }
    fprintf(f, "\n  ]}");
    return skinnedMeshes;
}

}  // namespace

// One exported entry point. /api/call {symbol: rb3sp_dump_shards} (RB3_REPLAY_API=1).
// Returns the total skinned-mesh count written (0 => no walkers live yet).
extern "C" int rb3sp_dump_shards() {
    const char* out = ProbeOut();
    if (!out) return 0;
    FILE* f = fopen(out, "w");
    if (!f) return 0;

    const char* scene = getenv("RB3_SHARD_PROBE_SCENE");
    fprintf(f,
        "{\n\"side\":\"native\",\"probe\":\"rb3sp_dump_shards\",\"scene\":\"%s\",\n"
        "\"walkers\":[\n", scene ? JEsc(scene).c_str() : "");

    int total = 0;
    bool first = true;
    void* seen[8] = {0}; int nseen = 0;
    // Hub walkers = TheCharCache player0-3 (chars.milo proxies). Also cover the live
    // director band as a cross-check if present. Dedup by char pointer.
    for (int slot = 0; slot < 4; ++slot) {
        BandCharacter* bc = nullptr;
        const char* source = "charcache";
        if (TheCharCache) bc = TheCharCache->GetCharacter(slot);
        if (!bc && TheBandDirector) { bc = TheBandDirector->GetCharacter(slot); source = "director"; }
        if (!bc) continue;
        bool dup = false;
        for (int i = 0; i < nseen; i++) if (seen[i] == (void*)bc) dup = true;
        if (dup) continue;
        if (nseen < 8) seen[nseen++] = (void*)bc;
        total += DumpWalker(f, slot, source, bc, first);
    }
    if (TheBandDirector) {
        for (int slot = 0; slot < 4; ++slot) {
            BandCharacter* bc = TheBandDirector->GetCharacter(slot);
            if (!bc) continue;
            bool dup = false;
            for (int i = 0; i < nseen; i++) if (seen[i] == (void*)bc) dup = true;
            if (dup) continue;
            if (nseen < 8) seen[nseen++] = (void*)bc;
            total += DumpWalker(f, slot, "director", bc, first);
        }
    }

    fprintf(f, "\n],\n\"total_skinned_meshes\":%d\n}\n", total);
    fclose(f);
    return total;
}

#endif  // HX_NATIVE
