// SpotBeamPass (milo-native-engine gfx/SpotBeamPass.cpp), dc3 GPU flavor only.
//
// The pass ports retail NgSpotlightDrawer's volumetric beams: the depthvolume
// shader on each beam's NG shaft mesh into a half-size target (blend ONE/ONE,
// the cone culling the shaft's front faces), then a 5-tap blur along x and
// then y. These cases check the constants RenderConeDefs / SetupFogDensityMap
// / BlurRT set against hand-worked values, and run the real pass on the real
// device inside a validation error scope against a CPU model of the cone
// shader: a quad far behind a cone stands in for the shaft (so every pixel
// runs the shader and the model needs no rasteriser), the scene depth is
// cleared to the far plane, and the model blurs its own result the same way.
#include "test_helpers.h"

#include "rb3_rnd_backend.h"
#include "gfx/GpuDevice.h"
#include "gfx/SpotBeamPass.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace {

constexpr int kSceneW = 128, kSceneH = 128;  // the beam target is half that
constexpr float kNear = 1.0f, kFar = 10000.0f;
constexpr float kQuadZ = 500.0f;

// A camera at the origin looking down +z, x right, y up; row-major, p * VP.
SpotBeamPass::Camera MakeCamera() {
    SpotBeamPass::Camera c{};
    c.fwd[2] = 1.0f;
    c.nearPlane = kNear;
    c.farPlane = kFar;
    c.zRange[0] = 0.0f;
    c.zRange[1] = 1.0f;
    const float s = 1.0f / std::tan(0.5f * 60.0f * 3.14159265f / 180.0f);
    const float q = kFar / (kFar - kNear);
    float *m = c.viewProj;
    std::memset(m, 0, sizeof(c.viewProj));
    m[0] = s;
    m[5] = s;
    m[10] = q;
    m[11] = 1.0f;
    m[14] = -kNear * q;
    return c;
}

// A cone across the view: from (-60, 0, 300) along +x, radii 10 -> 50.
NativeSpotBeam MakeBeam() {
    NativeSpotBeam b;
    std::memset(&b, 0, sizeof(b));
    b.shape = 0;
    b.lightPos[0] = -60.0f;
    b.lightPos[2] = 300.0f;
    b.axis[0] = 1.0f;
    b.sheetDir[2] = 1.0f;
    b.ngRadii[0] = 10.0f;
    b.ngRadii[1] = 50.0f;
    b.topRadius = 10.0f;
    b.length = 160.0f;
    b.brighten = 1.0f;
    b.color[0] = 1.0f;
    b.color[1] = 0.5f;
    b.color[2] = 0.25f;
    b.color[3] = 1.0f;
    b.intensity = 0.25f;
    for (int i = 0; i < 4; i++)
        b.matColor[i] = 1.0f;
    // identity mesh transform
    b.meshXfm[0] = b.meshXfm[4] = b.meshXfm[8] = 1.0f;
    return b;
}

float Dot(const float *a, const float *b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }
float Sat(float x) { return std::min(1.0f, std::max(0.0f, x)); }

// fs_cone (depthvolume.ps option 0) for the ray through screen uv, the shaft
// surface being the quad at z = kQuadZ, the scene depth the far plane.
void ConeModel(const SpotBeamPass::Constants &k, const SpotBeamPass::Camera &cam, float u,
               float v, float out[3]) {
    const float s = cam.viewProj[0];
    const float ndc[2] = {u * 2.0f - 1.0f, 1.0f - v * 2.0f};
    const float world[3] = {ndc[0] / s * kQuadZ, ndc[1] / s * kQuadZ, kQuadZ};
    const float *eye = k.c10, *ax = k.c26, *rc = k.c27;
    const float dir[3] = {world[0] - eye[0], world[1] - eye[1], world[2] - eye[2]};
    const float dist = std::sqrt(Dot(dir, dir));
    const float nd[3] = {dir[0] / dist, dir[1] / dist, dir[2] / dist};
    const float cos2 = k.c28[3];
    const float axRc = Dot(ax, rc), ndAx = Dot(nd, ax);
    const float a = ndAx * ndAx - cos2;
    const float b = ndAx * axRc - cos2 * Dot(nd, rc);
    const float c = axRc * axRc - cos2 * Dot(rc, rc);
    const float maxT = std::min(kFar, dist);  // the scene depth is the far plane
    const float disc = b * b - c * a;
    float enter[3] = {eye[0], eye[1], eye[2]}, exit[3] = {eye[0], eye[1], eye[2]};
    if (disc > 0.0f) {
        const float r = std::sqrt(disc);
        const float t1 = (-b - r) / a, t2 = (-b + r) / a;
        float p1[3], p2[3];
        for (int i = 0; i < 3; i++) {
            p1[i] = rc[i] + t1 * nd[i];
            p2[i] = rc[i] + t2 * nd[i];
        }
        const float h1 = Dot(p1, ax), h2 = Dot(p2, ax);
        float x = t1, y = t2;
        if (h1 > 0.0f && h2 <= 0.0f) {
            y = 0.0f;
        } else {
            if (!(h2 > 0.0f && h1 > 0.0f)) x = t2;
            if (h2 > 0.0f && h1 <= 0.0f) x = maxT;
        }
        x = std::min(maxT, std::max(x, 0.0f));
        y = std::min(maxT, std::max(y, 0.0f));
        for (int i = 0; i < 3; i++) {
            enter[i] = eye[i] + nd[i] * x;
            exit[i] = eye[i] + nd[i] * y;
        }
    }
    const float xsf = 1.0f;  // no cross-section texture: white, whatever c86.x is
    const float de[3] = {enter[0] - k.c25[0], enter[1] - k.c25[1], enter[2] - k.c25[2]};
    const float dx[3] = {exit[0] - k.c25[0], exit[1] - k.c25[1], exit[2] - k.c25[2]};
    const float sE = Sat(Dot(de, ax) * k.c25[3]), sX = Sat(Dot(dx, ax) * k.c25[3]);
    const float oE = 1.0f - sE, oX = 1.0f - sX, ds = sX - sE;
    float fall = 0.004f * oE * oE;
    if (std::fabs(ds) > 1e-6f)
        fall = 0.004f * ((oE * oE * oE - oX * oX * oX) / 3.0f) / ds;
    const float vdE = Dot(enter, k.c30) + k.c30[3], vdX = Dot(exit, k.c30) + k.c30[3];
    const float kk = fall * std::fabs(vdX - vdE) * xsf;  // fog keeps 1: c127.zw = 0
    for (int i = 0; i < 3; i++)
        out[i] = k.c90[i] * kk;
}

class SpotBeamPassTest : public ::testing::Test {
protected:
    void SetUp() override {
        if (!RB3RndBackend::InitGpu(64, 64, /*headless=*/true))
            GTEST_SKIP() << "headless GPU device unavailable on this host";
        if (RB3RndBackend::Gpu().IsNullBackend())
            GTEST_SKIP() << "null Dawn backend: no rendering to read back";
    }
    void TearDown() override { mPass.Terminate(); }

    // A 4x-multisampled depth buffer cleared to `depth`, and its depth view.
    wgpu::TextureView MakeDepth(float depth, wgpu::CommandEncoder &enc) {
        GpuDevice &gpu = RB3RndBackend::Gpu();
        wgpu::TextureDescriptor d{};
        d.size = {(uint32_t)kSceneW, (uint32_t)kSceneH, 1};
        d.format = wgpu::TextureFormat::Depth32Float;
        d.sampleCount = 4;
        d.usage = wgpu::TextureUsage::RenderAttachment | wgpu::TextureUsage::TextureBinding;
        mDepth = gpu.Device().CreateTexture(&d);
        wgpu::TextureView view = mDepth.CreateView();
        wgpu::RenderPassDepthStencilAttachment ds{};
        ds.view = view;
        ds.depthLoadOp = wgpu::LoadOp::Clear;
        ds.depthStoreOp = wgpu::StoreOp::Store;
        ds.depthClearValue = depth;
        wgpu::RenderPassDescriptor rp{};
        rp.depthStencilAttachment = &ds;
        enc.BeginRenderPass(&rp).End();
        return view;
    }

    // The quad z = kQuadZ, wider than the view, in the 64-byte vertex layout
    // (position at 0, colour at 24, UV at 40); `ccw` picks its screen winding.
    SpotBeamPass::Draw MakeQuadDraw(const SpotBeamPass::Constants &k, bool ccw) {
        GpuDevice &gpu = RB3RndBackend::Gpu();
        const float e = 400.0f;
        const float corners[4][2] = {{-e, -e}, {e, -e}, {e, e}, {-e, e}};
        std::vector<uint8_t> vb(4 * 64, 0);
        for (int i = 0; i < 4; i++) {
            float *p = (float *)&vb[i * 64];
            p[0] = corners[i][0];
            p[1] = corners[i][1];
            p[2] = kQuadZ;
            float *col = (float *)&vb[i * 64 + 24];
            col[0] = col[1] = col[2] = col[3] = 1.0f;
        }
        // (0,1,2) runs counter-clockwise on screen (y up).
        const uint16_t idxCcw[6] = {0, 1, 2, 0, 2, 3};
        const uint16_t idxCw[6] = {0, 2, 1, 0, 3, 2};
        SpotBeamPass::Draw d;
        d.k = k;
        std::memset(d.meshXfm, 0, sizeof(d.meshXfm));
        d.meshXfm[0] = d.meshXfm[4] = d.meshXfm[8] = 1.0f;
        wgpu::BufferDescriptor bd{};
        bd.usage = wgpu::BufferUsage::Vertex | wgpu::BufferUsage::CopyDst;
        bd.size = vb.size();
        d.vertexBuffer = gpu.Device().CreateBuffer(&bd);
        gpu.Queue().WriteBuffer(d.vertexBuffer, 0, vb.data(), vb.size());
        d.vertexStride = 64;
        d.vertexBytes = vb.size();
        bd.usage = wgpu::BufferUsage::Index | wgpu::BufferUsage::CopyDst;
        bd.size = sizeof(idxCw);
        d.indexBuffer = gpu.Device().CreateBuffer(&bd);
        gpu.Queue().WriteBuffer(d.indexBuffer, 0, ccw ? idxCcw : idxCw, sizeof(idxCw));
        d.indexCount = 6;
        return d;
    }

    // Runs the pass over `draws` with the scene depth cleared to `depth`;
    // reads back the blurred target (kSceneW/2 x kSceneH/2, RGBA8).
    bool Run(const SpotBeamPass::Camera &cam, const std::vector<SpotBeamPass::Draw> &draws,
             float depth, std::vector<uint8_t> &out, std::string &errors) {
        GpuDevice &gpu = RB3RndBackend::Gpu();
        wgpu::Device dev = gpu.Device();
        dev.PushErrorScope(wgpu::ErrorFilter::Validation);
        wgpu::CommandEncoder enc = dev.CreateCommandEncoder();
        wgpu::TextureView depthView = MakeDepth(depth, enc);
        const bool ran = mPass.Run(enc, cam, draws.data(), draws.size(), depthView, 4, kSceneW,
                                   kSceneH, gpu);
        wgpu::CommandBuffer cmd = enc.Finish();
        gpu.Queue().Submit(1, &cmd);
        errors.clear();
        gpu.Instance().WaitAny(
            dev.PopErrorScope(wgpu::CallbackMode::WaitAnyOnly,
                              [&](wgpu::PopErrorScopeStatus, wgpu::ErrorType type,
                                  wgpu::StringView msg) {
                                  if (type != wgpu::ErrorType::NoError)
                                      errors.assign(msg.data, msg.length);
                              }),
            UINT64_MAX);
        if (!ran)
            return false;
        const int w = kSceneW / 2, h = kSceneH / 2;
        out.assign((size_t)w * h * 4, 0);
        EXPECT_TRUE(gpu.ReadbackTexture(mPass.OutputTexture(), w, h, out.data(), out.size()));
        return true;
    }

    SpotBeamPass mPass;
    wgpu::Texture mDepth;
};

} // namespace

// RenderConeDefs, worked by hand for the test beam: the apex sits where the
// radii meet, 10 * 160 / (50 - 10) = 40 behind the light; total length 200.
TEST(SpotBeamConstants, ConeMatchesRenderConeDefs) {
    const SpotBeamPass::Camera cam = MakeCamera();
    NativeSpotBeam b = MakeBeam();
    SpotBeamPass::Constants k;
    ASSERT_TRUE(SpotBeamPass::BeamConstants(b, cam, k));
    EXPECT_EQ(k.shape, 0);
    EXPECT_FLOAT_EQ(k.c25[0], -100.0f);
    EXPECT_FLOAT_EQ(k.c25[2], 300.0f);
    EXPECT_FLOAT_EQ(k.c25[3], 1.0f / 200.0f);
    EXPECT_FLOAT_EQ(k.c26[0], 1.0f);
    EXPECT_FLOAT_EQ(k.c26[3], 200.0f);
    EXPECT_FLOAT_EQ(k.c27[0], 100.0f);  // eye - apex
    EXPECT_FLOAT_EQ(k.c27[2], -300.0f);
    const float c = std::cos(std::atan(50.0f / 200.0f));
    EXPECT_NEAR(k.c28[3], c * c, 1e-6);
    // colour * intensity * sBeamIntensity (8) * brighten, alpha 1
    EXPECT_FLOAT_EQ(k.c90[0], 2.0f);
    EXPECT_FLOAT_EQ(k.c90[1], 1.0f);
    EXPECT_FLOAT_EQ(k.c90[2], 0.5f);
    EXPECT_FLOAT_EQ(k.c90[3], 1.0f);
    // view plane, depth range, fog state
    EXPECT_FLOAT_EQ(k.c30[2], 1.0f);
    EXPECT_FLOAT_EQ(k.c30[3], 0.0f);
    EXPECT_FLOAT_EQ(k.c89[0], kNear);
    EXPECT_FLOAT_EQ(k.c89[1], kFar);
    EXPECT_FLOAT_EQ(k.c127[1], 1.0f / kFar);
    EXPECT_FLOAT_EQ(k.c127[2], 0.0f);
    EXPECT_FLOAT_EQ(k.c127[3], 0.0f);
    EXPECT_FLOAT_EQ(k.c9[0], 0.125f);  // retail sFogScale, 0x82C711CC

    // RenderBeams skips a beam without length.
    b.length = 0.0f;
    EXPECT_FALSE(SpotBeamPass::BeamConstants(b, cam, k));
}

// RenderSheet and RenderSphere: shape 2 and 3/4, and their colour scales.
TEST(SpotBeamConstants, SheetAndSphere) {
    const SpotBeamPass::Camera cam = MakeCamera();
    NativeSpotBeam b = MakeBeam();
    SpotBeamPass::Constants k;
    b.shape = 2;
    ASSERT_TRUE(SpotBeamPass::BeamConstants(b, cam, k));
    EXPECT_EQ(k.shape, 1);
    EXPECT_FLOAT_EQ(k.c91[2], 1.0f);  // the spotlight's m.z
    EXPECT_FLOAT_EQ(k.c91[3], 0.5f);  // retail sSheetW, 0x82C71198
    EXPECT_FLOAT_EQ(k.c90[0], 2.0f);  // intensity * sSheetIntensity (8) * brighten
    b.shape = 4;
    ASSERT_TRUE(SpotBeamPass::BeamConstants(b, cam, k));
    EXPECT_EQ(k.shape, 2);
    EXPECT_FLOAT_EQ(k.c91[2], 0.625f);
    EXPECT_FLOAT_EQ(k.c91[3], 10.0f);  // top radius * sSphereScale
    EXPECT_FLOAT_EQ(k.c90[0], 0.025f); // intensity * brighten * sBeamBrighten (0.1)
}

// SetupForPostProcess / SetupFogDensityMap (percent / 100), and BlurRT's taps.
TEST(SpotBeamConstants, CompositeAndBlur) {
    NativeSpotBeamFrame f{};
    f.intensity = 1.0f;
    f.baseIntensity = 1.0f;
    f.smokeIntensity = 50.0f;
    float c[3];
    SpotBeamPass::CompositeConstants(f, c);
    EXPECT_FLOAT_EQ(c[0], 32.0f);
    EXPECT_FLOAT_EQ(c[1], 0.01f);
    EXPECT_FLOAT_EQ(c[2], 0.5f * 0.99f);
    float off[5][2];
    SpotBeamPass::BlurOffsets(1.0f, 0.0f, 64, 32, off);
    EXPECT_FLOAT_EQ(off[0][0], -2.0f / 64.0f);
    EXPECT_FLOAT_EQ(off[4][0], 2.0f / 64.0f);
    EXPECT_FLOAT_EQ(off[4][1], 0.0f);
    float sum = 0.0f;
    for (float w : SpotBeamPass::kBlurWeights)
        sum += w;
    EXPECT_FLOAT_EQ(sum, 1.0f);
}

TEST_F(SpotBeamPassTest, ConeMatchesShaderModel) {
    const SpotBeamPass::Camera cam = MakeCamera();
    SpotBeamPass::Constants k;
    ASSERT_TRUE(SpotBeamPass::BeamConstants(MakeBeam(), cam, k));
    std::vector<SpotBeamPass::Draw> draws = {MakeQuadDraw(k, /*ccw=*/false)};
    std::vector<uint8_t> out;
    std::string err;
    ASSERT_TRUE(Run(cam, draws, 1.0f, out, err));
    EXPECT_EQ(err, "");

    // The model: the shader per target texel, stored as RGBA8 (clamped), then
    // the two blur passes, each stored as RGBA8. The taps land on texel
    // centres, so the bilinear fetch is a point fetch, clamped at the edges.
    const int w = kSceneW / 2, h = kSceneH / 2;
    std::vector<float> img((size_t)w * h * 3), tmp(img.size());
    auto q = [](float x) { return std::round(Sat(x) * 255.0f) / 255.0f; };
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            float c[3];
            ConeModel(k, cam, (x + 0.5f) / w, (y + 0.5f) / h, c);
            for (int i = 0; i < 3; i++)
                img[((size_t)y * w + x) * 3 + i] = q(c[i]);
        }
    for (int pass = 0; pass < 2; pass++) {
        for (int y = 0; y < h; y++)
            for (int x = 0; x < w; x++)
                for (int i = 0; i < 3; i++) {
                    float acc = 0.0f;
                    for (int t = -2; t <= 2; t++) {
                        const int sx = pass == 0 ? std::clamp(x + t, 0, w - 1) : x;
                        const int sy = pass == 1 ? std::clamp(y + t, 0, h - 1) : y;
                        acc += SpotBeamPass::kBlurWeights[t + 2] *
                               img[((size_t)sy * w + sx) * 3 + i];
                    }
                    tmp[((size_t)y * w + x) * 3 + i] = q(acc);
                }
        img.swap(tmp);
    }
    int worst = 0, lit = 0;
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            const size_t p = (size_t)y * w + x;
            if (out[p * 4 + 0] > 8)
                lit++;
            for (int i = 0; i < 3; i++)
                worst = std::max(worst, std::abs((int)out[p * 4 + i] -
                                                 (int)std::lround(img[p * 3 + i] * 255.0f)));
        }
    // The beam's narrow end is brighter than its wide end: (1 - s)^2 falloff
    // from the apex, along +x on screen.
    const auto red = [&](int x, int y) { return (int)out[((size_t)y * w + x) * 4]; };
    printf("[SpotBeamPass] lit texels %d of %d, max |gpu - model| = %d / 255, "
           "red at x 24/40 on the centre row %d/%d\n",
           lit, w * h, worst, red(24, h / 2), red(40, h / 2));
    EXPECT_GT(lit, 100);
    EXPECT_LT(lit, w * h / 2);
    EXPECT_LE(worst, 3);
    EXPECT_GT(red(24, h / 2), red(40, h / 2));
    // Off the beam: black, as the clear left it.
    EXPECT_EQ(red(2, 2), 0);
}

// The cone culls the winding retail's cull override 3 (D3DCULL_CCW) culls:
// the reverse of a material's. A quad wound the other way draws nothing.
TEST_F(SpotBeamPassTest, ConeCullsFrontFaces) {
    const SpotBeamPass::Camera cam = MakeCamera();
    SpotBeamPass::Constants k;
    ASSERT_TRUE(SpotBeamPass::BeamConstants(MakeBeam(), cam, k));
    std::vector<SpotBeamPass::Draw> draws = {MakeQuadDraw(k, /*ccw=*/true)};
    std::vector<uint8_t> out;
    std::string err;
    ASSERT_TRUE(Run(cam, draws, 1.0f, out, err));
    EXPECT_EQ(err, "");
    int lit = 0;
    for (size_t i = 0; i < out.size(); i += 4)
        lit += out[i] | out[i + 1] | out[i + 2];
    EXPECT_EQ(lit, 0);
}

// Scene depth in front of the beam clips every ray: nothing is drawn.
TEST_F(SpotBeamPassTest, SceneDepthOccludesBeam) {
    const SpotBeamPass::Camera cam = MakeCamera();
    SpotBeamPass::Constants k;
    ASSERT_TRUE(SpotBeamPass::BeamConstants(MakeBeam(), cam, k));
    std::vector<SpotBeamPass::Draw> draws = {MakeQuadDraw(k, /*ccw=*/false)};
    // the depth of a surface at view depth 100, nearer than the cone (z >= 250)
    const float q = kFar / (kFar - kNear);
    const float depth = q - kNear * q / 100.0f;
    std::vector<uint8_t> out;
    std::string err;
    ASSERT_TRUE(Run(cam, draws, depth, out, err));
    EXPECT_EQ(err, "");
    int lit = 0;
    for (size_t i = 0; i < out.size(); i += 4)
        lit += out[i] | out[i + 1] | out[i + 2];
    EXPECT_EQ(lit, 0);
}

// With nothing to draw the pass declines and records nothing.
TEST_F(SpotBeamPassTest, DeclinesWithoutDraws) {
    const SpotBeamPass::Camera cam = MakeCamera();
    GpuDevice &gpu = RB3RndBackend::Gpu();
    wgpu::CommandEncoder enc = gpu.Device().CreateCommandEncoder();
    wgpu::TextureView depthView = MakeDepth(1.0f, enc);
    EXPECT_FALSE(mPass.Run(enc, cam, nullptr, 0, depthView, 4, kSceneW, kSceneH, gpu));
    enc.Finish();
}
