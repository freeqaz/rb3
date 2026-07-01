// BandPatchMesh char-mesh corruption regression test.
//
// ROOT CAUSE this guards: BandPatchMesh::WorkVerts builds a per-vertex arena of
// MeshVert records, each followed by a trailing `unsigned short faceList[]`. The
// face-list start offset, the twin-flag byte, and the per-slot stride are all
// hardcoded as Wii literals (0x32 / 0x27 / 0x38). Those literals assume the Wii
// MeshVert layout, where the leading `const RndMesh::Vert *mVert` is 4 bytes. On
// a native LP64 build that pointer is 8 bytes, shifting every field after it by
// +4. With the Wii literals, the face-list write at +0x32 lands in the HIGH
// halfword of MeshVert::unk2c (the twin-list "next" cursor, host offset
// 0x30..0x33), scribbling a face index over the intact 0xFFFF low half of its
// -1 sentinel -> the twin walk in SetMeshVertAndTwins later reads e.g.
// 0x0752FFFF and subscripts mMeshVerts[] out of bounds -> heap corruption / the
// intermittent (~1/19) char-preview composite crash.
//
// The fix derives the offsets from the actual MeshVert layout under HX_NATIVE
// (Wii path keeps the literals, byte-identical match). This test reproduces the
// arena/twin-list construction directly and asserts the twin-list stays
// well-formed: every MeshVert::unk2c cursor is either -1 (end) or a valid
// in-range vertex index. On the buggy code an unk2c holds 0x<faceidx>FFFF
// (out of range) and the test fails; on the fixed code it passes.

#include "test_helpers.h"

#include "bandobj/BandPatchMesh.h"
#include "rndobj/Mesh.h"
#include "obj/Object.h"
#include "math/Vec.h"

#include <set>
#include <vector>

class BandPatchMeshTest : public EngineTestFixture {};

namespace {

// Build a small RndMesh that exercises the twin-list machinery: several pairs of
// vertices share an identical position (so SetMeshVerts links them into a twin
// list via unk28/unk2c), and a fan of faces references them. This drives the
// exact valence-count, slot-allocation, face-list-write and twin-link code paths
// that the offset bug corrupts — without needing the full asset/closet flow.
RndMesh *BuildTwinnedMesh(int twinGroups, int facesPerVert) {
    // Construct directly (not via the object factory) — the headless test boot
    // does not register the render-object factories, and a self-owning RndMesh
    // (mGeomOwner = this) is all SetMeshVerts needs.
    RndMesh *mesh = new RndMesh();
    // 3 verts per twin group, all sharing the group's position so they twin up.
    const int vertsPerGroup = 3;
    const int vertCount = twinGroups * vertsPerGroup;
    mesh->Verts().resize(vertCount, false);
    for (int g = 0; g < twinGroups; g++) {
        for (int k = 0; k < vertsPerGroup; k++) {
            int vi = g * vertsPerGroup + k;
            RndMesh::Vert &v = mesh->Verts(vi);
            // Identical position within the group -> twins; distinct Z per group.
            v.pos.Set((float)g, 0.0f, (float)g);
            v.norm.Set(0.0f, 0.0f, 1.0f);
            v.uv.Set((float)g * 0.01f, (float)k * 0.01f);
        }
    }
    // Faces: a fan that touches every vertex `facesPerVert` times so each
    // MeshVert accumulates a non-trivial valence (>= 1) and gets face-list
    // entries written — this is the write that the bug misplaces into unk2c.
    std::vector<RndMesh::Face> faces;
    for (int rep = 0; rep < facesPerVert; rep++) {
        for (int vi = 0; vi < vertCount; vi++) {
            RndMesh::Face f;
            f.Set(vi, (vi + 1) % vertCount, (vi + 2) % vertCount);
            faces.push_back(f);
        }
    }
    mesh->Faces() = faces;
    return mesh;
}

// Verify the twin list reachable from every MeshVert is well-formed: each
// unk2c cursor is -1 or a valid vertex index, and the walk terminates within
// vertCount steps (no cycle / no garbage).
::testing::AssertionResult TwinListWellFormed(
    BandPatchMesh::WorkVerts &wv, int vertCount
) {
    for (int i = 0; i < (int)wv.mMeshVerts.size(); i++) {
        BandPatchMesh::MeshVert *mv = wv.mMeshVerts[i];
        if (!mv)
            continue;
        int steps = 0;
        for (int num = mv->unk28; num != -1; num = wv.mMeshVerts[num]->unk2c) {
            if (num < 0 || num >= vertCount) {
                return ::testing::AssertionFailure()
                    << "MeshVert[" << i << "] twin cursor num=" << num
                    << " (0x" << std::hex << (unsigned)num << std::dec
                    << ") out of range [0," << vertCount << ")"
                    << " — corrupt unk2c (offset-bug signature 0x<faceidx>FFFF)";
            }
            if (++steps > vertCount + 1) {
                return ::testing::AssertionFailure()
                    << "MeshVert[" << i << "] twin walk did not terminate "
                    << "within " << vertCount << " steps (corrupt twin list)";
            }
        }
    }
    return ::testing::AssertionSuccess();
}

} // namespace

// Direct reproduction of the corrupting construction. On the buggy literal-
// offset code, the face-list write scribbles unk2c -> a twin cursor goes out of
// range. On the fixed (layout-derived offset) code the twin list is clean.
TEST_F(BandPatchMeshTest, SetMeshVertsTwinListNotCorrupted) {
    // Sizes chosen so valence > 1 (face list of >= 2 entries per vert) and
    // multiple twin groups exist — the regime where the offset bug fires.
    RndMesh *mesh = BuildTwinnedMesh(/*twinGroups=*/8, /*facesPerVert=*/4);
    int vertCount = mesh->Verts().size();
    ASSERT_GT(vertCount, 0);
    ASSERT_GT((int)mesh->Faces().size(), 0);

    Vector2 scale(64.0f, 64.0f);
    BandPatchMesh::WorkVerts wv(mesh, scale);
    wv.SetMeshVerts();

    ASSERT_EQ((int)wv.mMeshVerts.size(), vertCount);
    EXPECT_TRUE(TwinListWellFormed(wv, vertCount));

    // Cross-check: every MeshVert::unk2c value itself must be a sentinel or an
    // in-range index. The offset bug specifically produces 0x<faceidx>FFFF
    // (low halfword 0xFFFF intact), which fails both these conditions.
    for (int i = 0; i < (int)wv.mMeshVerts.size(); i++) {
        BandPatchMesh::MeshVert *mv = wv.mMeshVerts[i];
        if (!mv)
            continue;
        int n = mv->unk2c;
        bool ok = (n == -1) || (n >= 0 && n < vertCount);
        EXPECT_TRUE(ok) << "MeshVert[" << i << "].unk2c = " << n << " (0x"
                        << std::hex << (unsigned)n << std::dec
                        << ") is neither -1 nor a valid vertex index";
    }

    delete mesh;
}

// Defensive backstop coverage: a mesh whose faces reference an out-of-range
// vertex index must not crash / corrupt the heap. The HX_NATIVE guard skips the
// stray index; SetMeshVerts must complete and leave a well-formed twin list.
TEST_F(BandPatchMeshTest, SetMeshVertsToleratesOutOfRangeFaceIndex) {
    RndMesh *mesh = BuildTwinnedMesh(/*twinGroups=*/6, /*facesPerVert=*/3);
    int vertCount = mesh->Verts().size();
    // Poison one face with a vertex index past the end (simulates a Faces()/
    // Verts() desync). The original write would corrupt the arena/heap.
    ASSERT_GT((int)mesh->Faces().size(), 0);
    mesh->Faces()[0].Set(vertCount + 12345, 0, 1);

    Vector2 scale(64.0f, 64.0f);
    BandPatchMesh::WorkVerts wv(mesh, scale);
    wv.SetMeshVerts(); // must not crash / corrupt

    ASSERT_EQ((int)wv.mMeshVerts.size(), vertCount);
    EXPECT_TRUE(TwinListWellFormed(wv, vertCount));

    delete mesh;
}
