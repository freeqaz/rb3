// World refraction reads the pre-process buffer (dc3 GPU flavor only).
//
// Retail DxRnd::DoWorldEnd ends with SavePreBuffer, which resolves the world's
// colour into mPreProcessBuffer (rb3-xenon rnddx9/Rnd_Xbox.cpp). A material
// with RndMat::GetRefractEnabled draws with standard.ps option bit 46: it
// fetches that buffer (GetCurrentFrameTex(false), retail sampler tf6) at its
// own screen position, pushed by the refract normal map times
// kPS_RefractStrength, and multiplies it into the texel. The buffer is the one
// the last world end saved, so a refracting surface shows the previous frame's
// world. Native: WgpuRnd::SavePreBuffer and standard_wgsl.inc refractScreen
// (milo-native-engine dc3-backend-for-rb3-wii.md section 22).
//
// The test draws a red world and ends it, then recolours the world green and
// draws a white refracting quad over the right half: the quad must show red
// (the saved world), not green (the world under it now) and not white. A
// control quad with refraction off shows its own white. The strength is tiny,
// so the normal map's content cannot move the sample.
#include "test_helpers.h"

#include "rb3_rnd_backend.h"
#include "gfx/GpuDevice.h"
#include "platform/Rnd_Wgpu.h"
#include "rndobj/Cam.h"
#include "rndobj/Mat.h"
#include "rndobj/Mesh.h"
#include "rndobj/Rnd.h"
#include "rndobj/Tex.h"
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

// The renderer draws an unnamed mesh as text, without depth, so the quads get
// names, which need a dir. Kept for the process: the engine is a singleton.
ObjectDir *RefractNameDir() {
    static ObjectDir *dir = nullptr;
    if (!dir) {
        dir = new ObjectDir();
        dir->Reserve(16, 256);
    }
    return dir;
}

struct Pixel {
    int r, g, b;
};

class RefractTest : public ::testing::Test {
protected:
    void SetUp() override {
        EnsureSymbolInit();
        if (!RB3RndBackend::InitGpu(kSize, kSize, /*headless=*/true))
            GTEST_SKIP() << "headless GPU device unavailable on this host";
        if (RB3RndBackend::Gpu().IsNullBackend())
            GTEST_SKIP() << "null Dawn backend: nothing is rasterized";

        // Camera at the origin looking down +Y (Milo's forward).
        mCam = new RndCam();
        mCam->SetFrustum(1.0f, 100.0f, 0.6024178f, 1.0f);

        // The world: an unlit wall at y = 20 over the whole view.
        mWorldMat = MakeUnlitMat(1.0f, 0.0f, 0.0f);
        mWorld = MakeQuad(-100.0f, 100.0f, 20.0f, mWorldMat, "refract_world");

        // The glass: an unlit white quad at y = 10 over the right half (x > 0).
        mGlassMat = MakeUnlitMat(1.0f, 1.0f, 1.0f);
        mGlass = MakeQuad(0.0f, 50.0f, 10.0f, mGlassMat, "refract_glass");

        // Any refract normal map: at this strength it cannot move the sample.
        mNormal = new RndTex();
        mNormalPixels.assign(8, 0x80);  // one 4x4 DXT1 block
        RndBitmap &bmp = mNormal->mBitmap;
        bmp.mWidth = 4;
        bmp.mHeight = 4;
        bmp.mRowBytes = 2;
        bmp.mBpp = 4;
        bmp.mOrder = 0x08;  // DXT1
        bmp.mPixels = mNormalPixels.data();
        bmp.mPalette = nullptr;
        bmp.mBuffer = nullptr;  // the test owns the pixels
        bmp.mMip = nullptr;
        mNormal->mWidth = 4;
        mNormal->mHeight = 4;
        mNormal->mBpp = 4;
    }
    void TearDown() override {
        if (mGlassMat) mGlassMat->mRefractNormalMap = nullptr;
        delete mGlass;
        delete mWorld;
        delete mGlassMat;
        delete mWorldMat;
        delete mNormal;
        delete mCam;
    }

    static RndMat *MakeUnlitMat(float r, float g, float b) {
        RndMat *m = new RndMat();
        m->mCull = 0;
        m->SetUseEnv(false);  // unlit: the register colour as drawn
        m->SetPreLit(false);
        m->SetColor(r, g, b);
        return m;
    }

    static RndMesh *MakeQuad(float x0, float x1, float y, RndMat *mat, const char *name) {
        RndMesh *mesh = new RndMesh();
        mesh->Verts().resize(4, false);
        const float corners[4][2] = {{x0, -50}, {x1, -50}, {x0, 50}, {x1, 50}};
        for (int i = 0; i < 4; i++) {
            RndMesh::Vert &v = mesh->Verts(i);
            v.pos.Set(corners[i][0], y, corners[i][1]);
            v.norm.Set(0, -1, 0);
            v.uv.Set(0, 0);
        }
        std::vector<RndMesh::Face> faces(2);
        faces[0].Set(0, 1, 2);
        faces[1].Set(2, 1, 3);
        mesh->Faces() = faces;
        mesh->SetMat(mat);
        mesh->SetName(name, RefractNameDir());
        return mesh;
    }

    void SetRefract(bool on) {
        mGlassMat->mRefractEnabled = on;
        mGlassMat->mRefractStrength = on ? 0.001f : 0.0f;
        mGlassMat->mRefractNormalMap = on ? mNormal : nullptr;
    }

    // One frame: the world, the glass if asked, then the world end.
    void Frame(bool glass) {
        RB3RndBackend::BeginFrame(mCam);
        ASSERT_TRUE(RB3RndBackend::InPass());
        RB3RndBackend::DrawMesh(mWorld);
        if (glass) RB3RndBackend::DrawMesh(mGlass);
        TheRnd->unkef = true;  // the world camera counts as copied (see test_point_test)
        TheRnd->DoWorldEnd();
        RB3RndBackend::EndFrame();
    }

    // The frame's pixel at (x, y), read back from the headless target.
    Pixel At(int x, int y) {
        std::vector<uint8_t> px(kSize * kSize * 4);
        EXPECT_TRUE(RB3RndBackend::Gpu().ReadbackHeadlessFrame(px.data(), px.size()));
        const uint8_t *p = &px[(y * kSize + x) * 4];
        return {p[0], p[1], p[2]};
    }

    RndCam *mCam = nullptr;
    RndMat *mWorldMat = nullptr, *mGlassMat = nullptr;
    RndMesh *mWorld = nullptr, *mGlass = nullptr;
    RndTex *mNormal = nullptr;
    std::vector<uint8_t> mNormalPixels;
};

} // namespace

TEST_F(RefractTest, RefractingSurfaceShowsTheSavedWorld) {
    GpuDevice &gpu = RB3RndBackend::Gpu();
    gpu.Device().PushErrorScope(wgpu::ErrorFilter::Validation);

    // Frame 1: a red world, saved at its world end.
    mWorldMat->SetColor(1.0f, 0.0f, 0.0f);
    SetRefract(true);
    Frame(/*glass=*/false);

    // Frame 2: the world is green now; the glass refracts the saved red world.
    mWorldMat->SetColor(0.0f, 1.0f, 0.0f);
    Frame(/*glass=*/true);
    const Pixel left = At(kSize / 4, kSize / 2);
    const Pixel right = At(3 * kSize / 4, kSize / 2);
    EXPECT_EQ(PopErrors(gpu), "");

    EXPECT_GT(left.g, 200) << "the world itself is green this frame";
    EXPECT_LT(left.r, 50);
    EXPECT_GT(right.r, 200) << "the glass shows the world saved at the last world end (red)";
    EXPECT_LT(right.g, 50) << "not the world drawn beneath it this frame (green)";
    EXPECT_LT(right.b, 50) << "and not its own white";
}

TEST_F(RefractTest, NonRefractingSurfaceKeepsItsColour) {
    GpuDevice &gpu = RB3RndBackend::Gpu();
    gpu.Device().PushErrorScope(wgpu::ErrorFilter::Validation);

    mWorldMat->SetColor(1.0f, 0.0f, 0.0f);
    SetRefract(false);
    Frame(/*glass=*/false);
    mWorldMat->SetColor(0.0f, 1.0f, 0.0f);
    Frame(/*glass=*/true);
    const Pixel right = At(3 * kSize / 4, kSize / 2);
    EXPECT_EQ(PopErrors(gpu), "");

    EXPECT_GT(right.r, 200) << "refraction off: the glass is white";
    EXPECT_GT(right.g, 200);
    EXPECT_GT(right.b, 200);
}
