// Subsystems-online tests — assert the headless engine actually boots on native
// and the core object/config subsystems are LIVE (not stubbed/null). Mirrors
// dc3-decomp/native/tests/test_subsystems.cpp, scoped to RB3's RunBoot spine.
//
// These guard the whole "refuse-to-init a subsystem" hack class: if someone
// re-defers a load or an LP64 fault regresses SystemInit, a boot test goes red.

#include "test_helpers.h"

#include "obj/Object.h"
#include "obj/Data.h"
#include "utl/Symbol.h"
#include "os/System.h"
#include "bandobj/BandFaceDeform.h"

extern DataArray *gSystemConfig;

class NativeSubsystems : public EngineTestFixture {};

TEST_F(NativeSubsystems, SystemConfigPopulated) {
    ASSERT_NE(gSystemConfig, nullptr) << "SystemInit must populate gSystemConfig";
    DataArray *objCfg = gSystemConfig->FindArray(Symbol("objects"), false);
    ASSERT_NE(objCfg, nullptr) << "SystemConfig(\"objects\") must resolve after boot";
    EXPECT_GT(objCfg->Size(), 1) << "objects config should have per-class type-defs";
}

TEST_F(NativeSubsystems, ObjectFactoryRegistered) {
    // RegisterCommonFactories registers ObjectDir; the loader constructs by name.
    Hmx::Object *o = Hmx::Object::NewObject(Symbol("ObjectDir"));
    ASSERT_NE(o, nullptr) << "ObjectDir factory must be registered (loader needs it)";
    EXPECT_STREQ(o->ClassName().Str(), "ObjectDir");
    delete o;
}

TEST_F(NativeSubsystems, SymbolInterningStable) {
    Symbol a("convergence_test_token");
    Symbol b("convergence_test_token");
    EXPECT_EQ(a, b) << "equal strings must intern to the same Symbol";
    EXPECT_STREQ(a.Str(), "convergence_test_token");
}

// BandFaceDeform::DeltaArray::Load reads a big-endian (.milo_xbox) Delta stream.
// The 2026-06-08 blocker-validation sweep proved the *current* shared Load is
// ALREADY byte-correct on the LE host (38/38 records of head_male.fdm frame[1]
// decode identically), because the two `>>` reads are NON-overlapping: the
// "char unk0" field is the HIGH byte of a 2-byte start-vertex index (offset 0-1),
// and `num` is a separate 2-byte read (offset 2-3). ReadEndian swaps each to
// host order, so num/thisoffset() never corrupt and the stream stays in sync.
//
// This locks that invariant: if anyone "fixes" Load to read unk0 as a single
// byte (the original audit's mistaken suggestion), the start-low byte stays in
// the stream and rec0 reads num=0xE800 -> total desync -> this test goes red.
// The expected values mirror the real chin/head_male asset's first records
// (start=0x09F9 num=1 body{00 f9 00}; start=0x0A15 num=1 body{00 f9 00}).
TEST_F(NativeSubsystems, BandFaceDeformDeltaArrayLoadBE) {
    std::vector<uint8_t> buf;
    PutBE32(buf, 14); // int size (total Delta bytes)
    // rec0: start=0x09F9, num=1, body = 00 f9 00  (num*3+4 = 7 bytes)
    PutBE16(buf, 0x09F9);
    PutBE16(buf, 1);
    buf.insert(buf.end(), {0x00, 0xf9, 0x00});
    // rec1: start=0x0A15, num=1, body = 00 f9 00  (7 bytes) -> 14 total
    PutBE16(buf, 0x0A15);
    PutBE16(buf, 1);
    buf.insert(buf.end(), {0x00, 0xf9, 0x00});

    MemBinStream bs(buf.data(), (int)buf.size(), /*littleEndian=*/false);
    BandFaceDeform::DeltaArray da;
    da.Load(bs);

    EXPECT_EQ(da.mSize, 14);
    EXPECT_EQ(da.NumVerts(), 2) << "sum of d->num across records (proves no desync)";

    Delta *d0 = (Delta *)da.begin();
    EXPECT_EQ(*(unsigned short *)d0, 0x09F9u) << "start index, as AddFrame reads it host-natively";
    EXPECT_EQ(d0->num, 1);
    EXPECT_EQ(d0->thisoffset(), 7u) << "num*3+4";
    const unsigned char *b0 = (const unsigned char *)d0;
    EXPECT_EQ(b0[4], 0x00);
    EXPECT_EQ(b0[5], 0xf9);
    EXPECT_EQ(b0[6], 0x00);

    Delta *d1 = (Delta *)d0->next();
    EXPECT_EQ(*(unsigned short *)d1, 0x0A15u) << "second record landed at the right offset";
    EXPECT_EQ(d1->num, 1);
}
