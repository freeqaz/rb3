// A material rect writes the bloom mask the way retail's does (dc3 GPU flavor).
//
// RB3's retail post chain blooms the scene weighted by the frame's alpha, the
// pseudo-HDR mask. Retail draws a rect (Rnd::DrawRect, RndFlare::DrawShowing)
// through RndShaderDrawRect (rb3-xenon rndobj/Shader.cpp): CalcShaderOpts sets
// pseudo-HDR when !Offscreen() && mat->AllowHDR(), and SetColorWriteMask writes
// alpha only under pseudo-HDR, Offscreen() or mat->mAlphaWrite. So in the main
// frame an additive material writes alpha = luma(rgb) * c7, and a SrcAlphaAdd
// one (not AllowHDR) leaves alpha alone. The engine's DrawRect2D used to write
// texel alpha times colour alpha for every rect, so each flare put a solid
// square of mask under itself (milo-native-engine dc3-backend-for-rb3-wii.md
// section 29).
//
// The test draws a grey world whose material leaves alpha at the clear value
// 0, then one rect in the middle, ends the world (which runs the bloom), and
// reads a pixel a little outside the rect. Bloom can only reach it from mask
// written inside the rect.
//   - white additive rect: luma 1, so the mask is 1 and the pixel brightens
//     (the control that shows the bloom can be seen at all);
//   - black additive rect: luma 0, so no mask and no bloom;
//   - white SrcAlphaAdd rect: not AllowHDR, so no mask and no bloom.
//
// Retail also combines a blended draw's alpha with the destination's by MAX
// (DxRnd::SetDefaultRenderStates: BlendOpAlpha 3; milo-native-engine
// dc3-backend-for-rb3-wii.md section 30), where the
// engine used to blend alpha by the colour equation, so an additive draw
// summed its mask with the one under it. With an additive grey world (mask
// 0.25), an additive 0.25 rect must leave the mask at max(0.25, 0.25), the
// same as a SrcAlphaAdd 0.25 rect, which adds the same colour and no mask.
// Measured: both read 189 with MAX; with the colour equation the additive
// rect reads 194.
#include "test_helpers.h"

#include "rb3_rnd_backend.h"
#include "gfx/GpuDevice.h"
#include "platform/Rnd_Wgpu.h"
#include "rndobj/Cam.h"
#include "rndobj/Mat.h"
#include "rndobj/Mesh.h"
#include "rndobj/PostProc.h"
#include "rndobj/Rnd.h"
#include "obj/Dir.h"

#include <string>
#include <vector>

namespace {

std::string PopErrors(GpuDevice &gpu) {
    std::string errors;
    gpu.Instance().WaitAny(
        gpu.Device().PopErrorScope(wgpu::CallbackMode::WaitAnyOnly,
                                   [&](wgpu::PopErrorScopeStatus, wgpu::ErrorType type,
                                       wgpu::StringView msg) {
                                       if (type != wgpu::ErrorType::NoError)
                                           errors.assign(msg.data, msg.length);
                                   }),
        UINT64_MAX);
    return errors;
}

constexpr int kSize = 64;

// The renderer draws an unnamed mesh as text, without depth, so the world gets
// a name, which needs a dir. Kept for the process: the engine is a singleton.
ObjectDir *MaskNameDir() {
    static ObjectDir *dir = nullptr;
    if (!dir) {
        dir = new ObjectDir();
        dir->Reserve(16, 256);
    }
    return dir;
}

class RectBloomMaskTest : public ::testing::Test {
protected:
    void SetUp() override {
        EnsureSymbolInit();
        if (!RB3RndBackend::InitGpu(kSize, kSize, /*headless=*/true))
            GTEST_SKIP() << "headless GPU device unavailable on this host";
        if (RB3RndBackend::Gpu().IsNullBackend())
            GTEST_SKIP() << "null Dawn backend: nothing is rasterized";
        mW = RB3RndBackend::Gpu().WindowWidth();
        mH = RB3RndBackend::Gpu().WindowHeight();

        // Camera at the origin looking down +Y (Milo's forward).
        mCam = new RndCam();
        mCam->SetFrustum(1.0f, 100.0f, 0.6024178f, 1.0f);

        // The world: an unlit grey wall over the whole view. SrcAlpha is not
        // AllowHDR, so it leaves the frame's alpha (the mask) at 0.
        mWorldMat = new RndMat();
        mWorldMat->mCull = 0;
        mWorldMat->SetUseEnv(false);
        mWorldMat->SetPreLit(false);
        mWorldMat->SetColor(0.25f, 0.25f, 0.25f);
        mWorldMat->SetBlend(RndMat::kBlendSrcAlpha);
        mWorld = new RndMesh();
        mWorld->Verts().resize(4, false);
        const float corners[4][2] = {{-100, -100}, {100, -100}, {-100, 100}, {100, 100}};
        for (int i = 0; i < 4; i++) {
            RndMesh::Vert &v = mWorld->Verts(i);
            v.pos.Set(corners[i][0], 20.0f, corners[i][1]);
            v.norm.Set(0, -1, 0);
            v.uv.Set(0, 0);
        }
        std::vector<RndMesh::Face> faces(2);
        faces[0].Set(0, 1, 2);
        faces[1].Set(2, 1, 3);
        mWorld->Faces() = faces;
        mWorld->SetMat(mWorldMat);
        mWorld->SetName("bloom_mask_world", MaskNameDir());

        mRectMat = new RndMat();
        mRectMat->SetUseEnv(false);
        mRectMat->SetPreLit(false);
        mRectMat->SetZMode(RndMat::kZModeDisable);

        // A postproc with a strong bloom, so the retail chain runs and blooms
        // by the mask (threshold 1: c7 is plain luma).
        mPost = new RndPostProc();
        mPost->mBloomIntensity = 4.0f;
        mPost->mBloomThreshold = 1.0f;
        mPost->Select();
    }
    void TearDown() override {
        if (mPost) mPost->Unselect();
        delete mPost;
        delete mWorld;
        delete mWorldMat;
        delete mRectMat;
        delete mCam;
    }

    // One frame: the world, the rect (unless `blend` < 0), then the world end,
    // which grades and blooms the world. Returns the luma just right of the
    // rect, read back from the headless frame.
    int Frame(int blend, float c) {
        RB3RndBackend::BeginFrame(mCam);
        EXPECT_TRUE(RB3RndBackend::InPass());
        RB3RndBackend::DrawMesh(mWorld);
        if (blend >= 0) {
            mRectMat->SetBlend((RndMat::Blend)blend);
            mRectMat->SetColor(c, c, c);
            const float s = TheRnd->Width() / 4.0f;
            const Hmx::Rect rect(TheRnd->Width() / 2.0f - s / 2, TheRnd->Height() / 2.0f - s / 2,
                                 s, s);
            TheRnd->DrawRect(rect, Hmx::Color(1, 1, 1, 1), mRectMat, nullptr, nullptr);
        }
        TheRnd->unkef = true;  // the world camera counts as copied (see test_point_test)
        TheRnd->DoWorldEnd();
        RB3RndBackend::EndFrame();

        std::vector<uint8_t> px((size_t)mW * mH * 4);
        EXPECT_TRUE(RB3RndBackend::Gpu().ReadbackHeadlessFrame(px.data(), px.size()));
        // Three sixteenths right of the centre: 1/16 outside the rect's edge.
        const int x = mW / 2 + 3 * mW / 16, y = mH / 2;
        const uint8_t *p = &px[((size_t)y * mW + x) * 4];
        return (299 * p[0] + 587 * p[1] + 114 * p[2]) / 1000;
    }

    int mW = kSize, mH = kSize;
    RndCam *mCam = nullptr;
    RndMat *mWorldMat = nullptr, *mRectMat = nullptr;
    RndMesh *mWorld = nullptr;
    RndPostProc *mPost = nullptr;
};

} // namespace

TEST_F(RectBloomMaskTest, AdditiveRectMasksByItsLuma) {
    GpuDevice &gpu = RB3RndBackend::Gpu();
    gpu.Device().PushErrorScope(wgpu::ErrorFilter::Validation);

    const int none = Frame(/*blend=*/-1, 0.0f);
    const int white = Frame(RndMat::kBlendAdd, 1.0f);
    const int black = Frame(RndMat::kBlendAdd, 0.0f);
    EXPECT_EQ(PopErrors(gpu), "");

    EXPECT_GT(white, none + 10) << "control: a luma-1 additive rect blooms past its edge";
    EXPECT_LE(std::abs(black - none), 3)
        << "a black additive rect writes mask 0, so nothing blooms (was a solid square of mask)";
}

TEST_F(RectBloomMaskTest, SrcAlphaAddRectLeavesTheMaskAlone) {
    GpuDevice &gpu = RB3RndBackend::Gpu();
    gpu.Device().PushErrorScope(wgpu::ErrorFilter::Validation);

    const int none = Frame(/*blend=*/-1, 0.0f);
    const int white = Frame(RndMat::kBlendSrcAlphaAdd, 1.0f);
    EXPECT_EQ(PopErrors(gpu), "");

    EXPECT_LE(std::abs(white - none), 3)
        << "SrcAlphaAdd is not AllowHDR: the rect leaves alpha alone, so nothing blooms";
}

TEST_F(RectBloomMaskTest, AdditiveRectKeepsTheBrighterMaskNotTheSum) {
    GpuDevice &gpu = RB3RndBackend::Gpu();
    gpu.Device().PushErrorScope(wgpu::ErrorFilter::Validation);

    // An additive world writes its own luma, 0.25, to the mask. A milder
    // bloom than the other tests', so the whole-frame mask does not saturate.
    mWorldMat->SetBlend(RndMat::kBlendAdd);
    mPost->mBloomIntensity = 1.0f;
    const int none = Frame(/*blend=*/-1, 0.0f);
    const int add = Frame(RndMat::kBlendAdd, 0.25f);
    const int srcAlphaAdd = Frame(RndMat::kBlendSrcAlphaAdd, 0.25f);
    const int white = Frame(RndMat::kBlendAdd, 1.0f);
    EXPECT_EQ(PopErrors(gpu), "");

    EXPECT_GT(white, add + 10)
        << "control: a brighter additive rect raises the mask and blooms more";
    EXPECT_GE(add, none) << "the rect adds colour, so the bloom cannot fall";
    EXPECT_LE(std::abs(add - srcAlphaAdd), 2)
        << "an additive 0.25 rect over mask 0.25 keeps mask 0.25 (MAX), as a rect "
           "that writes no mask does; the sum 0.5 would bloom brighter";
}
