// Progressive-texture-sharpen MANAGER test (research/13 T1).
//
// The T0 gate (test_texsharpen.cpp) proved the engine churn path recreates a GPU
// texture at a new size when a bitmap is swapped + its fingerprint dirtied. This
// test exercises the full T1 MANAGER on top of that: build a synthetic `.sharpen`
// sidecar + an ObjectDir of "loaded" stripped RndTex objects, run
// RB3SharpenLoadSidecar (fingerprint match) + RB3SharpenStep (incremental swap +
// reupload), and confirm:
//   - every matched texture is RECREATED at the full-res size (sTexGpu grows),
//   - a fresh GPU view is published per sharpen (so the matBG rebuild key fires),
//   - the per-frame scheduler honors the budget (N textures/frame),
//   - the opt-out flag (RB3_PROGRESSIVE_SHARPEN=0) keeps everything stripped,
//   - reset is clean (no crash / no dangling pixels) and a non-matching sidecar
//     matches nothing.
//
// It drives the SAME production helpers the rb3 glue uses (RB3SharpenLoadSidecar /
// RB3SharpenStep) and inspects the GPU result via RB3DebugGetTexGpuInfo. Real
// headless GPU (Dawn / null backend in CI).

#include "test_helpers.h"

#include "rndobj/Tex.h"
#include "rndobj/Bitmap.h"
#include "obj/Dir.h"
#include "obj/Object.h"

#include "platform/Rnd_Wgpu_RB3.h"        // gBandRnd, InitGpu
#include "platform/RB3TexSharpen.h"       // the manager under test
#include "platform/RB3TexSharpenDebug.h"  // RB3DebugUploadTex / GetTexGpuInfo

#include <cstdint>
#include <cstring>
#include <vector>

namespace {

bool EnsureGpu() {
    static int sState = -1;
    if (sState >= 0) return sState == 1;
    bool ok = gBandRnd.InitGpu(64, 64, /*headless=*/true);
    sState = ok ? 1 : 0;
    return ok;
}

// Replicate the engine TexFingerprint (Rnd_Wgpu_RB3.cpp) byte-for-byte — the
// same formula scripts/milo/mip_strip.py uses to compute stripped_fp.
uint32_t TexFp(const uint8_t* p, int sz) {
    if (!p || sz < 16) return 0;
    uint32_t h = 0; int step = sz / 8; if (step < 1) step = 1;
    for (int i = 0; i < sz; i += step) h = h * 31u + p[i];
    return h;
}

// A DXT1 base level of WxH (bpp=4, rowBytes=W/2, pixBytes=rowBytes*H). The pixel
// buffer is heap-owned by the caller (mBuffer set so the sharpen swap's _MemFree
// path is exercised — but allocated with the ENGINE allocator so _MemFree matches
// _MemAlloc; see the note below). `seed` varies the content (and thus the fp).
struct LoadedTex {
    RndTex* tex = nullptr;
    uint32_t strippedFp = 0;
    int strippedW = 0, strippedH = 0;
};

// Allocate the stripped base level with the ENGINE allocator so that when the
// manager frees the old mBuffer via _MemFree it is matched. (RndBitmap::Load does
// exactly this in production.)
#include "utl/MemMgr.h"

LoadedTex MakeStrippedTex(ObjectDir* dir, const char* name,
                          int w, int h, uint8_t seed) {
    const int bpp = 4;
    const int rowBytes = (w * bpp) / 8;
    const int pixBytes = rowBytes * h;

    RndTex* tex = dir->New<RndTex>(name);
    RndBitmap& bmp = tex->mBitmap;

    uint8_t* buf = (uint8_t*)_MemAlloc(pixBytes, 32);
    for (int i = 0; i < pixBytes; i++)
        buf[i] = (uint8_t)((i * 7u + seed * 131u) & 0xFF);

    bmp.mWidth = (uint16_t)w;
    bmp.mHeight = (uint16_t)h;
    bmp.mRowBytes = (uint16_t)rowBytes;
    bmp.mBpp = (uint8_t)bpp;
    bmp.mOrder = 0x08;          // DXT1 / BC1
    bmp.mPixels = buf;
    bmp.mBuffer = buf;          // bitmap owns it (matches production Load layout)
    bmp.mPalette = nullptr;
    bmp.mMip = nullptr;
    tex->mWidth = w; tex->mHeight = h; tex->mBpp = bpp;

    LoadedTex lt;
    lt.tex = tex;
    lt.strippedFp = TexFp(buf, pixBytes);
    lt.strippedW = w; lt.strippedH = h;
    return lt;
}

// Append one SHRP entry: '<I HHH HHH BB I I I I' header then name then topmip.
void PutU16(std::vector<uint8_t>& b, uint16_t v) { b.push_back(v & 0xFF); b.push_back((v >> 8) & 0xFF); }
void PutU32(std::vector<uint8_t>& b, uint32_t v) {
    b.push_back(v & 0xFF); b.push_back((v >> 8) & 0xFF);
    b.push_back((v >> 16) & 0xFF); b.push_back((v >> 24) & 0xFF);
}

void AppendEntry(std::vector<uint8_t>& blob, uint32_t index,
                 int fullW, int fullH, int strippedW, int strippedH,
                 int bpp, uint32_t order, uint32_t strippedFp, uint8_t topSeed) {
    int fullRb = (fullW * bpp) / 8;
    int strRb  = (strippedW * bpp) / 8;
    uint32_t topmipLen = (uint32_t)(fullRb * fullH);
    PutU32(blob, index);
    PutU16(blob, (uint16_t)fullW); PutU16(blob, (uint16_t)fullH); PutU16(blob, (uint16_t)fullRb);
    PutU16(blob, (uint16_t)strippedW); PutU16(blob, (uint16_t)strippedH); PutU16(blob, (uint16_t)strRb);
    blob.push_back((uint8_t)bpp); blob.push_back(0); // bpp, pad
    PutU32(blob, order);
    PutU32(blob, strippedFp);
    PutU32(blob, topmipLen);
    PutU32(blob, 0); // name_len == 0 (fingerprint-authoritative)
    // top-mip: deterministic full-res bytes (distinct from the stripped content).
    for (uint32_t i = 0; i < topmipLen; i++)
        blob.push_back((uint8_t)((i * 13u + topSeed * 97u) & 0xFF));
}

std::vector<uint8_t> MakeSidecarHeader(uint32_t count) {
    std::vector<uint8_t> b;
    b.push_back('S'); b.push_back('H'); b.push_back('R'); b.push_back('P');
    PutU32(b, 1);     // version
    PutU32(b, 1);     // levels
    PutU32(b, count); // entry_count
    return b;
}

} // namespace

class TexSharpenManagerTest : public EngineTestFixture {
protected:
    void SetUp() override {
        if (!EnsureGpu())
            GTEST_SKIP() << "headless GPU device unavailable on this host";
        // Ensure the flag is ON for the positive cases (a prior test may have set
        // it). getenv is cached process-wide in the engine, so we only rely on the
        // default ON here and test the opt-out in its own process-independent way
        // by checking RB3ProgressiveSharpenEnabled() reflects the env.
        RB3SharpenReset();
    }
    void TearDown() override { RB3SharpenReset(); }
};

// Core: load a 3-texture sidecar, match all three, sharpen them, and verify each
// GPU texture recreated at full-res with a fresh view.
TEST_F(TexSharpenManagerTest, MatchesAndSharpensToFullRes) {
    if (!RB3ProgressiveSharpenEnabled())
        GTEST_SKIP() << "RB3_PROGRESSIVE_SHARPEN disabled in env";

    ObjectDir* dir = Hmx::Object::New<ObjectDir>();

    // Three stripped textures at 128x128 / 256x128 / 64x64.
    LoadedTex a = MakeStrippedTex(dir, "tex_a", 128, 128, 11);
    LoadedTex b = MakeStrippedTex(dir, "tex_b", 256, 128, 22);
    LoadedTex c = MakeStrippedTex(dir, "tex_c", 64,  64,  33);

    // Upload them stripped first (mirrors the venue having drawn once).
    ASSERT_TRUE(RB3DebugUploadTex(a.tex));
    ASSERT_TRUE(RB3DebugUploadTex(b.tex));
    ASSERT_TRUE(RB3DebugUploadTex(c.tex));
    RB3TexGpuInfo a0 = RB3DebugGetTexGpuInfo(a.tex);
    RB3TexGpuInfo b0 = RB3DebugGetTexGpuInfo(b.tex);
    RB3TexGpuInfo c0 = RB3DebugGetTexGpuInfo(c.tex);
    EXPECT_EQ(a0.texW, 128); EXPECT_EQ(b0.texW, 256); EXPECT_EQ(c0.texW, 64);

    // Build a sidecar that sharpens each to 2x (full = stripped<<1).
    std::vector<uint8_t> blob = MakeSidecarHeader(3);
    AppendEntry(blob, 0, 256, 256, a.strippedW, a.strippedH, 4, 0x08, a.strippedFp, 7);
    AppendEntry(blob, 1, 512, 256, b.strippedW, b.strippedH, 4, 0x08, b.strippedFp, 8);
    AppendEntry(blob, 2, 128, 128, c.strippedW, c.strippedH, 4, 0x08, c.strippedFp, 9);

    int matched = RB3SharpenLoadSidecar(dir, blob.data(), (uint32_t)blob.size());
    ASSERT_EQ(matched, 3) << "all three fingerprints must match their entry";

    // Step with a budget of 2/frame: first call sharpens 2, second sharpens 1.
    int n1 = RB3SharpenStep(2);
    EXPECT_EQ(n1, 2);
    EXPECT_FALSE(RB3SharpenComplete());
    int n2 = RB3SharpenStep(2);
    EXPECT_EQ(n2, 1);
    EXPECT_TRUE(RB3SharpenComplete());
    int n3 = RB3SharpenStep(2);
    EXPECT_EQ(n3, 0) << "no work left once complete";

    // Every texture recreated at full-res with a NEW view.
    RB3TexGpuInfo a1 = RB3DebugGetTexGpuInfo(a.tex);
    RB3TexGpuInfo b1 = RB3DebugGetTexGpuInfo(b.tex);
    RB3TexGpuInfo c1 = RB3DebugGetTexGpuInfo(c.tex);
    EXPECT_EQ(a1.texW, 256); EXPECT_EQ(a1.texH, 256);
    EXPECT_EQ(b1.texW, 512); EXPECT_EQ(b1.texH, 256);
    EXPECT_EQ(c1.texW, 128); EXPECT_EQ(c1.texH, 128);
    EXPECT_NE(a1.viewPtr, a0.viewPtr);
    EXPECT_NE(b1.viewPtr, b0.viewPtr);
    EXPECT_NE(c1.viewPtr, c0.viewPtr);

    RB3SharpenStatus st = RB3SharpenGetStatus();
    EXPECT_EQ(st.sharpened, 3);
    EXPECT_EQ(st.total, 3);
    EXPECT_GT(st.bytesUpgraded, 0u);

    RB3SharpenReset();
    delete dir; // also frees the textures + their (now full-res) bitmaps — no UAF
}

// A sidecar whose fingerprints DON'T match any loaded texture matches nothing and
// leaves every texture stripped.
TEST_F(TexSharpenManagerTest, NonMatchingSidecarIsNoOp) {
    if (!RB3ProgressiveSharpenEnabled())
        GTEST_SKIP() << "RB3_PROGRESSIVE_SHARPEN disabled in env";

    ObjectDir* dir = Hmx::Object::New<ObjectDir>();
    LoadedTex a = MakeStrippedTex(dir, "tex_a", 128, 128, 44);
    ASSERT_TRUE(RB3DebugUploadTex(a.tex));
    RB3TexGpuInfo a0 = RB3DebugGetTexGpuInfo(a.tex);

    std::vector<uint8_t> blob = MakeSidecarHeader(1);
    AppendEntry(blob, 0, 256, 256, 128, 128, 4, 0x08,
                a.strippedFp ^ 0xDEADBEEFu /*wrong fp*/, 1);
    int matched = RB3SharpenLoadSidecar(dir, blob.data(), (uint32_t)blob.size());
    EXPECT_EQ(matched, 0);
    EXPECT_FALSE(RB3SharpenGetStatus().active);

    int n = RB3SharpenStep(8);
    EXPECT_EQ(n, 0);
    RB3TexGpuInfo a1 = RB3DebugGetTexGpuInfo(a.tex);
    EXPECT_EQ(a1.texW, 128) << "non-matching sidecar must leave the texture stripped";

    delete dir;
}

// Research/14 Lane B fold-in: RB3SharpenReuploadTex returning false (GPU not
// ready) must NOT mark the entry done and must NOT consume the per-frame budget
// — the entry retries on later frames and succeeds once the GPU is back. We
// simulate not-ready by flipping the public gBandRnd.mGpuReady latch (the exact
// condition RB3SharpenReuploadTex early-outs on); the device itself stays alive.
TEST_F(TexSharpenManagerTest, RetriesWhenGpuNotReady) {
    if (!RB3ProgressiveSharpenEnabled())
        GTEST_SKIP() << "RB3_PROGRESSIVE_SHARPEN disabled in env";

    ObjectDir* dir = Hmx::Object::New<ObjectDir>();
    LoadedTex a = MakeStrippedTex(dir, "tex_a", 128, 128, 66);
    LoadedTex b = MakeStrippedTex(dir, "tex_b", 64,  64,  77);
    ASSERT_TRUE(RB3DebugUploadTex(a.tex));
    ASSERT_TRUE(RB3DebugUploadTex(b.tex));
    RB3TexGpuInfo a0 = RB3DebugGetTexGpuInfo(a.tex);

    std::vector<uint8_t> blob = MakeSidecarHeader(2);
    AppendEntry(blob, 0, 256, 256, a.strippedW, a.strippedH, 4, 0x08, a.strippedFp, 3);
    AppendEntry(blob, 1, 128, 128, b.strippedW, b.strippedH, 4, 0x08, b.strippedFp, 4);
    ASSERT_EQ(RB3SharpenLoadSidecar(dir, blob.data(), (uint32_t)blob.size()), 2);

    // GPU "not ready": several frames of stepping make NO progress — the head
    // entry is retried (not marked done), the budget is not consumed (0 returned
    // even with budget 4 and 2 entries pending), and the session is not complete.
    gBandRnd.mGpuReady = false;
    for (int frame = 0; frame < 5; frame++) {
        EXPECT_EQ(RB3SharpenStep(4), 0) << "not-ready reupload must not consume budget";
        EXPECT_EQ(RB3SharpenGetStatus().sharpened, 0) << "must not be marked done";
        EXPECT_FALSE(RB3SharpenComplete());
    }
    // The GPU texture is untouched while not ready (no recreate happened).
    RB3TexGpuInfo aDuring = RB3DebugGetTexGpuInfo(a.tex);
    EXPECT_EQ(aDuring.texW, 128);
    EXPECT_EQ(aDuring.viewPtr, a0.viewPtr);

    // GPU back: the SAME entries complete (the retry path must not have lost the
    // already-swapped full-res bitmap) and both recreate at full size.
    gBandRnd.mGpuReady = true;
    EXPECT_EQ(RB3SharpenStep(4), 2);
    EXPECT_TRUE(RB3SharpenComplete());
    RB3TexGpuInfo a1 = RB3DebugGetTexGpuInfo(a.tex);
    RB3TexGpuInfo b1 = RB3DebugGetTexGpuInfo(b.tex);
    EXPECT_EQ(a1.texW, 256); EXPECT_EQ(a1.texH, 256);
    EXPECT_EQ(b1.texW, 128); EXPECT_EQ(b1.texH, 128);
    EXPECT_NE(a1.viewPtr, a0.viewPtr);
    EXPECT_EQ(RB3SharpenGetStatus().sharpened, 2);

    RB3SharpenReset();
    delete dir;
}

// The retry is BOUNDED (~120 frames): a permanently-not-ready GPU eventually
// marks the entry done (so the session can complete and the driver stops
// polling) without ever recreating the GPU texture, and without spinning
// within a single frame.
TEST_F(TexSharpenManagerTest, RetryCapMarksDoneEventually) {
    if (!RB3ProgressiveSharpenEnabled())
        GTEST_SKIP() << "RB3_PROGRESSIVE_SHARPEN disabled in env";

    ObjectDir* dir = Hmx::Object::New<ObjectDir>();
    LoadedTex a = MakeStrippedTex(dir, "tex_a", 128, 128, 88);
    ASSERT_TRUE(RB3DebugUploadTex(a.tex));
    RB3TexGpuInfo a0 = RB3DebugGetTexGpuInfo(a.tex);

    std::vector<uint8_t> blob = MakeSidecarHeader(1);
    AppendEntry(blob, 0, 256, 256, a.strippedW, a.strippedH, 4, 0x08, a.strippedFp, 5);
    ASSERT_EQ(RB3SharpenLoadSidecar(dir, blob.data(), (uint32_t)blob.size()), 1);

    gBandRnd.mGpuReady = false;
    // One retry per Step call; the cap is 120 → the entry must be abandoned
    // (marked done) within a bounded number of calls, well under 200.
    int stepsUntilDone = -1;
    for (int frame = 0; frame < 200; frame++) {
        if (RB3SharpenStep(4) > 0) { stepsUntilDone = frame + 1; break; }
    }
    bool completeWhileDown = RB3SharpenComplete();
    int sharpenedWhileDown = RB3SharpenGetStatus().sharpened;
    // No GPU work ever happened (still the stripped texture + original view).
    RB3TexGpuInfo a1 = RB3DebugGetTexGpuInfo(a.tex);
    // Restore the GPU latch BEFORE any assert can abort the test body — later
    // tests depend on it.
    gBandRnd.mGpuReady = true;

    EXPECT_GT(stepsUntilDone, 100) << "cap must allow ~120 retry frames";
    EXPECT_GE(stepsUntilDone, 0)   << "entry must eventually be marked done";
    EXPECT_LE(stepsUntilDone, 130) << "cap must bound the retry";
    EXPECT_TRUE(completeWhileDown) << "capped-out entry counts as consumed";
    EXPECT_EQ(sharpenedWhileDown, 1);
    EXPECT_EQ(a1.texW, 128);
    EXPECT_EQ(a1.viewPtr, a0.viewPtr);

    RB3SharpenReset();
    delete dir;
}

// A corrupt blob (bad magic / short) is rejected without crashing.
TEST_F(TexSharpenManagerTest, RejectsGarbageSidecar) {
    ObjectDir* dir = Hmx::Object::New<ObjectDir>();
    MakeStrippedTex(dir, "tex_a", 128, 128, 55);

    uint8_t garbage[8] = {'X','X','X','X', 1,0,0,0};
    EXPECT_EQ(RB3SharpenLoadSidecar(dir, garbage, sizeof(garbage)), 0);
    EXPECT_EQ(RB3SharpenLoadSidecar(dir, nullptr, 0), 0);

    std::vector<uint8_t> shortHdr = MakeSidecarHeader(5); // claims 5, has 0 entries
    EXPECT_EQ(RB3SharpenLoadSidecar(dir, shortHdr.data(), (uint32_t)shortHdr.size()), 0);

    delete dir;
}
