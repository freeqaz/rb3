// rb3 Native Port — Wave-17 R1-DOLPHIN Lane D3 native-side BONE PROBE.
//
// Purpose: emit the NATIVE half of the Wii-vs-native inter-bone delta table the
// R5 hands-endgame decision is gated on. The Wii ground-truth half already exists
// (Lane D2: Bank-8 debug DOL booted on a retail disc via a patched apploader; see
// execution/R1-DOLPHIN/evidence/D2_wii_bones.json + D2_interbone_table.md). This
// TU dumps the SAME JSON record shape from a live fixed-clock native session so a
// join tool (scripts/analysis/interbone_diff.py — Lane D3) can compute, per
// adjacent hand-chain bone pair, D=inv(W_parent)*W_child on both sides and the
// matrix-relative angle between them (PLAN §3.7).
//
// PATTERN: modeled on rb3_replay_capture.cpp — an additive, env-gated TU that
// includes decomp headers (so it rides the target-wide MWCC compat flags, NOT the
// MS-compat-OFF override) and exposes ONE extern "C" entry point reached over the
// existing /api/call endpoint (RB3_REPLAY_API=1). It performs read-only live-state
// reads on the MAIN thread (the /api/call contract), touches no engine source, and
// is completely inert unless RB3_BONE_PROBE_OUT is set.
//
// ANTI-MAGNET (the campaign's own/bound trap — HANDS-ADJUDICATION/VERDICT.md): the
// shared static "bound" magnet skeleton (char/main/skeleton.milo) has the SAME bone
// pointer across every member and does NOT animate; the per-member "own" animated
// skeleton has DISTINCT pointers. This probe walks EACH member's OWN BandCharacter
// dir (TheCharCache->GetCharacter(slot)) and records every bone's pointer identity;
// the join asserts same-named bones have DISTINCT addresses across members (a
// collision would mean we accidentally dumped the magnet). Bilateral L/R symmetry
// of the resulting relRot angles is the same independent real-pose check D2 used.

#ifdef HX_NATIVE

#include "obj/Object.h"
#include "obj/Dir.h"
#include "math/Mtx.h"          // Transform / Matrix3 / Vector3
#include "rndobj/Trans.h"      // RndTransformable::WorldXfm()/LocalXfm()/TransParent()
#include "bandobj/BandCharacter.h"
#include "bandobj/BandCharDesc.h"   // Gender()
#include "bandobj/BandDirector.h"   // TheBandDirector->GetCharacter(slot) (live in-game band)
#include "meta_band/CharCache.h"    // TheCharCache->GetCharacter(slot) (shell/preview band)
#include "char/CharDriver.h"        // D4: CharClipDriver clip name + frame join key
#include "char/CharClip.h"          // CharClip::Name()

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace {

const char* ProbeOut() {
    const char* e = getenv("RB3_BONE_PROBE_OUT");
    return (e && e[0]) ? e : nullptr;
}

// Hand-chain name tokens (both hands). D2's pair list (M-2): forearm->hand anchor
// + middle/ring/thumb 01->02->03 cascades. We dump every bone whose name contains
// one of these substrings; the join builds the pairs. Names on the native side are
// the CharBone object names ("bone_L-hand.cb"); D2's records use the identical
// ".cb" names — the join normalizes both by stripping "bone_"/".cb".
bool IsHandChainBone(const char* nm) {
    if (!nm) return false;
    // Case-insensitive (the native rig names the forearm "bone_L-foreArm.mesh").
    // Skip the exoskeleton deform drivers ("exo_*") — those are not the animating
    // skeleton D2 captured.
    if (strncmp(nm, "bone_exo_", 9) == 0 || strncmp(nm, "exo_", 4) == 0) return false;
    char low[128];
    size_t i = 0;
    for (; nm[i] && i < sizeof(low) - 1; i++) {
        char c = nm[i];
        low[i] = (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : c;
    }
    low[i] = 0;
    static const char* toks[] = {
        "forearm", "hand", "middlefinger", "ringfinger", "thumb",
        // kept for completeness / provenance if present in the rig:
        "indexfinger", "pinky", "wrist",
        // D4: pelvis + spine are the convention-pin reference bones (known-shared
        // structure; pelvis≈identity per D3 unblock). Captured so the join can null
        // the per-bone local-frame conjugation on a non-hand bone before reading the
        // finger deltas.
        "pelvis", "spine",
    };
    for (const char* t : toks)
        if (strstr(low, t)) return true;
    return false;
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
    // world_rows = 3x3 basis (row-major, matching D2's world_rows), world_trans = v.
    fprintf(f,
        "\"rows\":[[%.9g,%.9g,%.9g],[%.9g,%.9g,%.9g],[%.9g,%.9g,%.9g]],"
        "\"trans\":[%.9g,%.9g,%.9g]",
        t.m.x.x, t.m.x.y, t.m.x.z,
        t.m.y.x, t.m.y.y, t.m.y.z,
        t.m.z.x, t.m.z.y, t.m.z.z,
        t.v.x, t.v.y, t.v.z);
}

// D4 (Wave-C, the frame-matched join): emit the member's CharClipDriver clip name +
// frame so the join has the (clip, frame) key PLAN §3.6/§3.7 require — the exact thing
// D3's static-shell table lacked. The hand bones are posed by a CharDriver (or the
// CharDriverMidi subclass) living in the member's OWN dir; each driver holds a
// CharClipDriver STACK (mFirst..Last), the top of which carries the live beat.
//   - clip   = Last()->GetClip()->Name()   (the top/deepest clip TopClipFrame uses)
//   - frame  = CharDriver::TopClipFrame()   (beat->frame via the clip's fps)
//   - beat   = Last()->mBeat                (raw driver beat, the un-fps'd clock)
// We emit EVERY CharDriver in the member dir (body/face/etc), tagged by driver name +
// clip_type Symbol, so the join can pick the body/skeleton driver that moves the hand
// chain. Read-only: FirstPlaying()/Last()/TopClipFrame()/GetClip()/Name() are all const
// or non-mutating accessors. Returns the number of drivers emitted.
int DumpDrivers(FILE* f, BandCharacter* bc) {
    int n = 0;
    fprintf(f, "\"drivers\":[");
    for (ObjDirItr<CharDriver> it(bc, true); it != 0; ++it) {
        CharDriver* dr = it;
        // Top (deepest) clip driver = what TopClipFrame() reads.
        CharClipDriver* last = dr->Last();
        CharClip* topClip = last ? last->GetClip() : nullptr;
        // First fully-blended-in playing clip (may differ from top during a blend).
        CharClipDriver* fp = dr->FirstPlaying();
        CharClip* playClip = fp ? fp->GetClip() : nullptr;
        float frame = dr->TopClipFrame();
        float beat = last ? last->mBeat : 0.0f;
        // count the stack depth for provenance
        int depth = 0;
        for (CharClipDriver* d = fp; d; d = d->Next()) depth++;
        const char* drName = dr->Name() ? dr->Name() : "";
        const char* clipType = dr->ClipType().Str() ? dr->ClipType().Str() : "";
        const char* topName = (topClip && topClip->Name()) ? topClip->Name() : "";
        const char* playName = (playClip && playClip->Name()) ? playClip->Name() : "";
        if (n > 0) fprintf(f, ",");
        fprintf(f,
            "{\"driver\":\"%s\",\"clip_type\":\"%s\",\"clip\":\"%s\","
            "\"playing_clip\":\"%s\",\"frame\":%.6f,\"beat\":%.6f,\"depth\":%d,"
            "\"driver_addr\":\"%p\"}",
            JEsc(drName).c_str(), JEsc(clipType).c_str(), JEsc(topName).c_str(),
            JEsc(playName).c_str(), frame, beat, depth, (void*)dr);
        n++;
    }
    fprintf(f, "]");
    return n;
}

// Dump one member's OWN hand-chain bones. The animating per-member skeleton bones
// are RndTransformable "bone_X" nodes in the member's skeleton_unshared subdir
// (found via Find<RndTransformable> — BandCharacter.cpp:514); the CharBone drivers
// that would carry the ".cb" names live in the SHARED magnet skeleton (the own/bound
// trap). We walk the member's OWN dir (recurse=true) for the Trans bones and read
// WorldXfm() directly — the same node D2's CharBone->mTrans resolves to, and the
// same nodes RebindOutfitBonesToOwnSkeleton repoints outfit meshes to.
// Returns number of hand-chain bones emitted.
int DumpMember(FILE* f, int slot, const char* source, BandCharacter* bc,
               bool& firstMember) {
    const char* nm = bc->Name() ? bc->Name() : "";
    // BandCharacter : public BandCharDesc -> Gender() is a Symbol (male/female).
    const char* gender = bc->Gender().Str() ? bc->Gender().Str() : "";

    if (!firstMember) fprintf(f, ",\n");
    firstMember = false;

    // First pass: census (total transforms + a name sample) for diagnostics — this
    // is what tells us whether the animated skeleton is present in this dir at all.
    int totalTrans = 0, handCount = 0;
    bool diag = getenv("RB3_BONE_PROBE_DIAG") != nullptr;
    std::string sample;
    for (ObjDirItr<RndTransformable> it(bc, true); it != 0; ++it) {
        totalTrans++;
        const char* bnm = it->Name();
        if (IsHandChainBone(bnm)) handCount++;
        if (diag && sample.size() < 1200 && bnm && strstr(bnm, "bone")) {
            sample += JEsc(bnm); sample += " ";
        }
    }

    fprintf(f,
        "  {\"slot\":%d,\"source\":\"%s\",\"name\":\"%s\",\"gender\":\"%s\","
        "\"char_addr\":\"%p\",\"loading\":%d,\"total_trans\":%d,"
        "\"name_sample\":\"%s\",",
        slot, source, JEsc(nm).c_str(), JEsc(gender).c_str(),
        (void*)bc, bc->IsLoading() ? 1 : 0, totalTrans, sample.c_str());
    // D4: the (clip, frame) join key — emitted per member, before the bones.
    DumpDrivers(f, bc);
    fprintf(f, ",\"bones\":[\n");

    int n = 0;
    for (ObjDirItr<RndTransformable> it(bc, true); it != 0; ++it) {
        const char* bnm = it->Name();
        if (!IsHandChainBone(bnm)) continue;
        RndTransformable* tr = (RndTransformable*)it;
        const char* pnm = (tr->TransParent() && tr->TransParent()->Name())
                              ? tr->TransParent()->Name() : "";
        Transform& wx = tr->WorldXfm();       // forces recompute if dirty (Trans.h:104)
        const Transform& lx = tr->LocalXfm();
        if (n > 0) fprintf(f, ",\n");
        // trans_addr = the pointer-identity provenance the join asserts
        // distinct-per-member (anti-magnet).
        fprintf(f,
            "    {\"name\":\"%s\",\"parent\":\"%s\",\"class\":\"%s\","
            "\"trans_addr\":\"%p\",\"world\":{",
            JEsc(bnm).c_str(), JEsc(pnm).c_str(),
            tr->ClassName().Str() ? tr->ClassName().Str() : "", (void*)tr);
        EmitRows(f, wx);
        fprintf(f, "},\"local\":{");
        EmitRows(f, lx);
        fprintf(f, "}}");
        n++;
    }
    fprintf(f, "\n  ]}");
    return n;
}

}  // namespace

// One exported entry point. /api/call {symbol: rb3bp_dump_bones} (RB3_REPLAY_API=1)
// invokes it on the main thread. Returns the total hand-chain bone count written
// this call (0 => no posed band members live yet — the harness's "not-ready" signal).
extern "C" int rb3bp_dump_bones() {
    const char* out = ProbeOut();
    if (!out) return 0;

    FILE* f = fopen(out, "w");
    if (!f) return 0;

    const char* scene = getenv("RB3_BONE_PROBE_SCENE");
    fprintf(f,
        "{\n\"side\":\"native\",\"build\":\"rb3-native\","
        "\"scene\":\"%s\",\"probe\":\"rb3bp_dump_bones\",\n\"members\":[\n",
        scene ? JEsc(scene).c_str() : "");

    int total = 0;
    bool firstMember = true;
    // Two sources, in order of fidelity for the LIVE animated band:
    //   - TheBandDirector->GetCharacter(slot): the on-stage, clip-driven band (the
    //     gameplay/venue context — bones animate faithfully). Preferred.
    //   - TheCharCache->GetCharacter(slot): the shell/preview band (may be posed at
    //     the rest/bind pose only). Used when the director has no character in a slot.
    // We dedupe by char_addr so a slot present in both is dumped once (director wins).
    void* seen[8] = {0};
    int nseen = 0;
    for (int slot = 0; slot < 4; ++slot) {
        BandCharacter* bc = nullptr;
        const char* source = "director";
        if (TheBandDirector) bc = TheBandDirector->GetCharacter(slot);
        if (!bc && TheCharCache) { bc = TheCharCache->GetCharacter(slot); source = "charcache"; }
        if (!bc) continue;
        bool dup = false;
        for (int i = 0; i < nseen; i++) if (seen[i] == (void*)bc) { dup = true; break; }
        if (dup) continue;
        if (nseen < 8) seen[nseen++] = (void*)bc;
        total += DumpMember(f, slot, source, bc, firstMember);
    }
    // Also dump any charcache slots not already covered (shell context / A-B check).
    if (TheCharCache) {
        for (int slot = 0; slot < 4; ++slot) {
            BandCharacter* bc = TheCharCache->GetCharacter(slot);
            if (!bc) continue;
            bool dup = false;
            for (int i = 0; i < nseen; i++) if (seen[i] == (void*)bc) { dup = true; break; }
            if (dup) continue;
            if (nseen < 8) seen[nseen++] = (void*)bc;
            total += DumpMember(f, slot, "charcache", bc, firstMember);
        }
    }

    fprintf(f, "\n],\n\"total_hand_bones\":%d\n}\n", total);
    fclose(f);
    return total;
}

#endif  // HX_NATIVE
