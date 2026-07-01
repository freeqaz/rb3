// Progressive-texture-sharpen recreate-at-new-size GATE test (research/13 T0).
//
// THE GATE for the whole progressive-sharpen engine wave. The design (load a
// half-res stripped venue to reach gameplay fast, then background-fetch the
// full-res top-mips and sharpen each texture live in-session) rests on ONE
// engine assumption:
//
//   Swap a stripped (half-res) RndBitmap up to full resolution + dirty the
//   churn key, and UploadRndTexIfNeeded (Rnd_Wgpu_RB3.cpp) RECREATES the GPU
//   texture at the NEW (larger) size and publishes a NEW view — with no
//   same-size assert and no fixed-size assumption.
//
// This test proves that against the REAL engine GPU device (headless Dawn on
// this host's RTX 3090; the null backend in CI exercises the same code path).
// It drives UploadRndTexIfNeeded through the exact production helper the sharpen
// manager will use, then inspects the resulting sTexGpu entry via the native
// diagnostic accessor. It also documents the matBG-staleness consequence for T1:
// because UploadRndTexIfNeeded replaces e.view with a NEW handle, the cached
// DrawMesh path's existing rebuild key (slot.matDiffuseView != diffuse.Get())
// already detects the change — T1's only requirement is to RE-INVOKE
// UploadRndTexIfNeeded on the swapped texture (GetRB3TexView alone returns the
// stale view; it does not re-run the churn check).

#include "test_helpers.h"

#include "rndobj/Tex.h"
#include "rndobj/Bitmap.h"
#include "obj/Object.h"

#include "platform/Rnd_Wgpu_RB3.h"        // gBandRnd, BandRnd::InitGpu
#include "platform/RB3TexSharpenDebug.h"  // RB3DebugUploadTex / RB3DebugGetTexGpuInfo

#include <cstdint>
#include <vector>

namespace {

// One-time headless GPU bring-up shared across the cases in this file. The
// engine GpuDevice is a process-global; bring it up once (InitGpu is the same
// call main_native makes) and leave it up. Returns false if no device could be
// created (then the cases SKIP rather than fail — e.g. a host with no Vulkan).
bool EnsureGpu() {
    static int sState = -1; // -1 untried, 0 failed, 1 ready
    if (sState >= 0) return sState == 1;
    // Small target — we never present; the texture sizes under test are
    // independent of the swapchain size.
    bool ok = gBandRnd.InitGpu(/*width=*/64, /*height=*/64, /*headless=*/true);
    sState = ok ? 1 : 0;
    return ok;
}

// Fill a DXT1 (BC1) bitmap of WxH into `tex->mBitmap`, owning a fresh pixel
// buffer (so the pixel POINTER changes across a swap — one of the two churn
// signals). DXT1: bpp=4, order=0x08 (kDXT1 format bit), rowBytes = W*4/8 = W/2,
// pixelBytes = rowBytes*H. `seed` varies the bytes so the fingerprint differs
// too (the other churn signal). The buffer is heap-owned by the test and freed
// in the fixture teardown — RndBitmap::mPixels just points at it.
struct OwnedBitmap {
    std::vector<uint8_t> pixels;
};

void SetDxt1(RndTex* tex, OwnedBitmap& store, int w, int h, uint8_t seed) {
    const int bpp = 4;
    const int rowBytes = (w * bpp) / 8;        // == w/2 for DXT1
    const size_t pixBytes = (size_t)rowBytes * h;
    store.pixels.assign(pixBytes, 0);
    // Deterministic, seed-dependent content. The first 16 bytes matter most:
    // TexFingerprint samples 8 evenly-spaced bytes, so vary across the buffer.
    for (size_t i = 0; i < pixBytes; i++)
        store.pixels[i] = (uint8_t)((i * 7u + seed * 131u) & 0xFF);

    RndBitmap& bmp = tex->mBitmap;
    bmp.mWidth = (uint16_t)w;
    bmp.mHeight = (uint16_t)h;
    bmp.mRowBytes = (uint16_t)rowBytes;
    bmp.mBpp = (uint8_t)bpp;
    bmp.mOrder = 0x08;                          // DXT1 / BC1
    bmp.mPixels = store.pixels.data();          // base level pointer
    bmp.mPalette = nullptr;
    // NOTE: do NOT set mBuffer — the test owns `store.pixels`; RndBitmap::Reset/
    // dtor frees mBuffer if set, which would double-free the std::vector storage.
    bmp.mBuffer = nullptr;
    bmp.mMip = nullptr;
    // Keep RndTex's own mirror fields consistent (harmless; the upload path reads
    // mBitmap, but a future caller might read these).
    tex->mWidth = w;
    tex->mHeight = h;
    tex->mBpp = bpp;
}

} // namespace

class TexSharpenTest : public EngineTestFixture {
protected:
    void SetUp() override {
        if (!EnsureGpu())
            GTEST_SKIP() << "headless GPU device unavailable on this host";
    }
};

// THE GATE: stripped (64x64) -> full-res (128x128) recreate.
TEST_F(TexSharpenTest, ChurnRecreatesAtNewLargerSize) {
    RndTex* tex = new RndTex();

    OwnedBitmap stripped;
    SetDxt1(tex, stripped, /*w=*/64, /*h=*/64, /*seed=*/1);

    // ---- Step 1: upload the STRIPPED (half-res) texture ----
    ASSERT_TRUE(RB3DebugUploadTex(tex)) << "stripped upload failed";
    RB3TexGpuInfo before = RB3DebugGetTexGpuInfo(tex);
    ASSERT_TRUE(before.present);
    ASSERT_TRUE(before.uploaded);
    EXPECT_EQ(before.texW, 64);
    EXPECT_EQ(before.texH, 64);
    ASSERT_NE(before.viewPtr, nullptr);

    // A redundant re-upload with the SAME bitmap must be a cache HIT (no
    // recreate) — establishes the churn key actually gates recreation.
    ASSERT_TRUE(RB3DebugUploadTex(tex));
    RB3TexGpuInfo cacheHit = RB3DebugGetTexGpuInfo(tex);
    EXPECT_EQ(cacheHit.globalRecreateCount, before.globalRecreateCount)
        << "re-upload of an unchanged bitmap should NOT recreate the texture";
    EXPECT_EQ(cacheHit.viewPtr, before.viewPtr)
        << "cache-hit must keep the same view handle";

    // ---- Step 2: swap to FULL-RES (128x128) + dirty the churn key ----
    // This is exactly what the sharpen manager does: reconstruct the full base
    // level (here a fresh, larger, different-content DXT1) into the SAME RndTex's
    // mBitmap. The pixel pointer AND the fingerprint both change.
    OwnedBitmap full;
    SetDxt1(tex, full, /*w=*/128, /*h=*/128, /*seed=*/2);
    ASSERT_NE(full.pixels.data(), stripped.pixels.data());

    // ---- Step 3: re-drive the upload/churn path ----
    ASSERT_TRUE(RB3DebugUploadTex(tex)) << "full-res re-upload failed";
    RB3TexGpuInfo after = RB3DebugGetTexGpuInfo(tex);
    ASSERT_TRUE(after.present);
    ASSERT_TRUE(after.uploaded);

    // GATE ASSERTIONS ---------------------------------------------------------
    // (1) Recreated at the NEW, LARGER size — no same-size assumption/assert.
    EXPECT_EQ(after.texW, 128)
        << "texture must be RECREATED at the new full-res width, not reuse 64";
    EXPECT_EQ(after.texH, 128)
        << "texture must be RECREATED at the new full-res height, not reuse 64";
    // (2) A NEW view handle was published (this is what propagates to the matBG
    //     rebuild key slot.matDiffuseView != diffuse.Get()).
    EXPECT_NE(after.viewPtr, before.viewPtr)
        << "sTexGpu[tex].view must become a NEW view after recreate";
    EXPECT_NE(after.texPtr, before.texPtr)
        << "sTexGpu[tex].tex must become a NEW texture after recreate";
    // (3) Exactly ONE recreate happened on the swap (the cache-hit step did
    //     none) — proves the churn path took the recreate branch.
    EXPECT_EQ(after.globalRecreateCount, before.globalRecreateCount + 1)
        << "the full-res swap must trigger exactly one CreateTexture";

    delete tex;
}

// Documents the matBG-staleness consequence for T1: GetRB3TexView returns the
// CURRENTLY-cached view; after a swap-without-re-upload it is STALE (old size).
// Only re-invoking the upload (RB3DebugUploadTex) recreates and refreshes it.
// This is the precise reason T1's sharpen manager must call the upload path on
// each swapped texture — the existing matBG view-handle compare then does the
// rest. (Goes the OTHER way too: shrink works identically — no size assumption.)
TEST_F(TexSharpenTest, RecreateAlsoHandlesShrinkAndPublishesFreshView) {
    RndTex* tex = new RndTex();

    OwnedBitmap big;
    SetDxt1(tex, big, /*w=*/256, /*h=*/256, /*seed=*/3);
    ASSERT_TRUE(RB3DebugUploadTex(tex));
    RB3TexGpuInfo a = RB3DebugGetTexGpuInfo(tex);
    EXPECT_EQ(a.texW, 256);
    EXPECT_EQ(a.texH, 256);

    OwnedBitmap small;
    SetDxt1(tex, small, /*w=*/64, /*h=*/64, /*seed=*/4);
    ASSERT_TRUE(RB3DebugUploadTex(tex));
    RB3TexGpuInfo b = RB3DebugGetTexGpuInfo(tex);
    EXPECT_EQ(b.texW, 64) << "recreate honors a smaller size too (no assert)";
    EXPECT_EQ(b.texH, 64);
    EXPECT_NE(b.viewPtr, a.viewPtr);
    EXPECT_EQ(b.globalRecreateCount, a.globalRecreateCount + 1);

    delete tex;
}
