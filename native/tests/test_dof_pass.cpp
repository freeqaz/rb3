// DofPass (milo-native-engine gfx/DofPass.cpp), dc3 GPU flavor only.
//
// The pass ports retail NgDOFProc::DoPost: downsample_4x into a quarter-size
// target, two 8-tap blurs with SetVHBlurWeights' tap tables, then the
// postprocess shader's depth-of-field mix, f = sat(min(max(|z * range -
// scale * range|, minBlur), maxBlur)), out = sat(mix(scene, blur, f)).
//
// Until 2026-10-06 nothing could reach it: RB3's DOFProc never enabled, and the
// first frame that did (W16-RO) failed validation (an UnfilterableFloat depth
// sampled through a filtering sampler, and a depth+stencil view bound as a
// texture), which invalidated the whole frame's command buffer. These cases
// run the real pass on the real device inside a validation error scope, and
// check its output against a CPU model of the same chain at three depths:
// in focus (f = 0: the scene), near (f = 1: the blur) and half way (f = 0.5).
#include "test_helpers.h"

#include "rb3_rnd_backend.h"
#include "gfx/DofPass.h"
#include "gfx/GpuDevice.h"
#include "platform/rndshape/RndShape.h"
#include "rndobj/Cam.h"
#include "rndobj/DOFProc.h"
#include "rndobj/PostProc.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace {

constexpr int kW = 1280, kH = 720;  // retail's back buffer; blur target 320x180
constexpr int kQW = kW / 4, kQH = kH / 4;

class TestDOFProc : public DOFProc {
public:
    bool mOn = true;
    float mFocal = 40.0f, mDepth = 0.5f, mMax = 1.0f, mMin = 0.0f;
    bool Enabled() const override { return mOn; }
    float FocalPlane() override { return mFocal; }
    float BlurDepth() override { return mDepth; }
    float MaxBlur() override { return mMax; }
    float MinBlur() override { return mMin; }
};

struct Img {
    int w = 0, h = 0;
    std::vector<float> px;  // RGBA, 0..1
    float &at(int x, int y, int c) { return px[((size_t)y * w + x) * 4 + c]; }
    float at(int x, int y, int c) const { return px[((size_t)y * w + x) * 4 + c]; }
};

float Q8(float v) { return std::round(std::clamp(v, 0.0f, 1.0f) * 255.0f) / 255.0f; }

// Linear filter, clamp-to-edge addressing, level 0.
void Bilinear(const Img &t, float u, float v, float out[4]) {
    float px = u * t.w - 0.5f, py = v * t.h - 0.5f;
    int x0 = (int)std::floor(px), y0 = (int)std::floor(py);
    float fx = px - x0, fy = py - y0;
    auto cx = [&](int x) { return std::clamp(x, 0, t.w - 1); };
    auto cy = [&](int y) { return std::clamp(y, 0, t.h - 1); };
    for (int c = 0; c < 4; c++) {
        float a = t.at(cx(x0), cy(y0), c), b = t.at(cx(x0 + 1), cy(y0), c);
        float d = t.at(cx(x0), cy(y0 + 1), c), e = t.at(cx(x0 + 1), cy(y0 + 1), c);
        out[c] = (a * (1 - fx) + b * fx) * (1 - fy) + (d * (1 - fx) + e * fx) * fy;
    }
}

Img MakeScene() {
    Img s;
    s.w = kW;
    s.h = kH;
    s.px.resize((size_t)kW * kH * 4);
    for (int y = 0; y < kH; y++)
        for (int x = 0; x < kW; x++) {
            s.at(x, y, 0) = ((x / 5 + y / 7) % 2) ? 1.0f : 0.0f;
            s.at(x, y, 1) = (float)((x * 7 + y * 3) & 255) / 255.0f;
            s.at(x, y, 2) = ((x ^ y) & 32) ? 1.0f : 0.0f;
            s.at(x, y, 3) = 1.0f;
        }
    return s;
}

// The blur image the composite samples: 4x4 box, then the horizontal and the
// vertical 8-tap passes, each written to an 8-bit target.
Img ModelBlur(const Img &scene, float widthScale) {
    Img a;
    a.w = kQW;
    a.h = kQH;
    a.px.resize((size_t)kQW * kQH * 4);
    for (int y = 0; y < kQH; y++)
        for (int x = 0; x < kQW; x++)
            for (int c = 0; c < 4; c++) {
                float acc = 0;
                for (int j = 0; j < 4; j++)
                    for (int i = 0; i < 4; i++)
                        acc += scene.at(x * 4 + i, y * 4 + j, c);
                a.at(x, y, c) = Q8(acc / 16.0f);
            }
    auto blur = [&](const Img &src, bool vertical) {
        float off[8][2];
        DofPass::RetailBlurOffsets(vertical, kQW, kQH, widthScale, off);
        Img d = src;
        for (int y = 0; y < kQH; y++)
            for (int x = 0; x < kQW; x++) {
                float acc[4] = {0, 0, 0, 0};
                for (int i = 0; i < 8; i++) {
                    float s[4];
                    Bilinear(src, (x + 0.5f) / kQW + off[i][0], (y + 0.5f) / kQH + off[i][1], s);
                    for (int c = 0; c < 4; c++)
                        acc[c] += s[c];
                }
                for (int c = 0; c < 4; c++)
                    d.at(x, y, c) = Q8(acc[c] * 0.125f);
            }
        return d;
    };
    return blur(blur(a, false), true);
}

class DofPassTest : public ::testing::Test {
protected:
    void SetUp() override {
        if (!RB3RndBackend::InitGpu(64, 64, /*headless=*/true))
            GTEST_SKIP() << "headless GPU device unavailable on this host";
        if (RB3RndBackend::Gpu().IsNullBackend())
            GTEST_SKIP() << "null Dawn backend: no rendering to read back";
        mSaved = TheDOFProc;
        mProc = new TestDOFProc;
        TheDOFProc = mProc;
    }
    void TearDown() override {
        if (mProc) {
            TheDOFProc = mSaved;
            delete mProc;
        }
        mPass.Terminate();
    }

    // The constants DofPass derives, from the same camera it reads.
    void Constants(float c24[4]) {
        RndCam *cam = RndCam::Current();
        float n = cam ? cam->NearPlane() : 1.0f, f = cam ? cam->FarPlane() : 1000.0f;
        Vector2 zr = cam ? rndshape::CamZRange(cam) : Vector2(0.0f, 1.0f);
        DofPass::RetailConstants(mProc->mFocal, mProc->mDepth, mProc->mMax, mProc->mMin, n, f,
                                 zr.x, zr.y, c24);
    }

    // Runs the pass over `scene` with every depth sample at `z`. Returns false
    // if the pass did not run; `errors` collects every validation error raised
    // while creating, recording and submitting it.
    bool RunAt(const Img &scene, float z, std::vector<uint8_t> &out, std::string &errors) {
        GpuDevice &gpu = RB3RndBackend::Gpu();
        wgpu::Device dev = gpu.Device();
        dev.PushErrorScope(wgpu::ErrorFilter::Validation);

        wgpu::TextureDescriptor sd{};
        sd.size = {(uint32_t)kW, (uint32_t)kH, 1};
        sd.format = wgpu::TextureFormat::RGBA8Unorm;
        sd.usage = wgpu::TextureUsage::TextureBinding | wgpu::TextureUsage::CopyDst;
        wgpu::Texture sceneTex = dev.CreateTexture(&sd);
        std::vector<uint8_t> bytes(scene.px.size());
        for (size_t i = 0; i < bytes.size(); i++)
            bytes[i] = (uint8_t)std::lround(scene.px[i] * 255.0f);
        wgpu::TexelCopyTextureInfo dst{};
        dst.texture = sceneTex;
        wgpu::TexelCopyBufferLayout layout{};
        layout.bytesPerRow = kW * 4;
        layout.rowsPerImage = kH;
        gpu.Queue().WriteTexture(&dst, bytes.data(), bytes.size(), &layout, &sd.size);

        // The scene depth as WgpuRnd::CreateDepthTexture makes it.
        wgpu::TextureDescriptor dd{};
        dd.size = {(uint32_t)kW, (uint32_t)kH, 1};
        dd.format = wgpu::TextureFormat::Depth24PlusStencil8;
        dd.usage = wgpu::TextureUsage::RenderAttachment | wgpu::TextureUsage::TextureBinding;
        dd.sampleCount = 4;
        wgpu::Texture depthTex = dev.CreateTexture(&dd);
        wgpu::TextureViewDescriptor dv{};
        dv.aspect = wgpu::TextureAspect::DepthOnly;
        wgpu::TextureView depthSample = depthTex.CreateView(&dv);

        wgpu::CommandEncoder enc = dev.CreateCommandEncoder();
        {
            wgpu::RenderPassDepthStencilAttachment da{};
            da.view = depthTex.CreateView();
            da.depthLoadOp = wgpu::LoadOp::Clear;
            da.depthStoreOp = wgpu::StoreOp::Store;
            da.depthClearValue = z;
            da.stencilLoadOp = wgpu::LoadOp::Clear;
            da.stencilStoreOp = wgpu::StoreOp::Store;
            wgpu::RenderPassDescriptor rp{};
            rp.depthStencilAttachment = &da;
            enc.BeginRenderPass(&rp).End();
        }
        bool ran = mPass.Run(enc, sceneTex.CreateView(), sd.format, depthSample, 4, kW, kH, gpu);
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
        out.assign((size_t)kW * kH * 4, 0);
        EXPECT_TRUE(gpu.ReadbackTexture(mPass.OutputTexture(), kW, kH, out.data(), out.size()));
        return true;
    }

    DofPass mPass;
    TestDOFProc *mProc = nullptr;
    DOFProc *mSaved = nullptr;
};

// Largest |GPU - model| over all pixels and channels, in 8-bit steps.
int MaxDiff(const std::vector<uint8_t> &gpu, const Img &model) {
    int worst = 0;
    for (size_t i = 0; i < gpu.size(); i++)
        worst = std::max(worst, std::abs((int)gpu[i] - (int)std::lround(model.px[i] * 255.0f)));
    return worst;
}

} // namespace

TEST_F(DofPassTest, DisabledRecordsNothing) {
    mProc->mOn = false;
    std::vector<uint8_t> out;
    std::string err;
    EXPECT_FALSE(RunAt(MakeScene(), 0.5f, out, err));
    EXPECT_EQ(err, "");
}

TEST_F(DofPassTest, MatchesRetailChainAtThreeDepths) {
    const Img scene = MakeScene();
    const Img blur = ModelBlur(scene, RndPostProc::DOFOverrides().mBlurWidthScale);
    float c24[4];
    Constants(c24);
    const float scale = -c24[1] / c24[0];       // projected focal depth
    const float bias = scale - 1.0f / c24[0];   // projected near edge of the ramp
    ASSERT_GT(scale, bias);
    ASSERT_GE(bias, 0.0f);

    // The full-res composite: blur sampled bilinearly at the pixel centre.
    auto model = [&](float f) {
        Img m = scene;
        for (int y = 0; y < kH; y++)
            for (int x = 0; x < kW; x++) {
                float b[4];
                Bilinear(blur, (x + 0.5f) / kW, (y + 0.5f) / kH, b);
                for (int c = 0; c < 4; c++)
                    m.at(x, y, c) = Q8(scene.at(x, y, c) + (b[c] - scene.at(x, y, c)) * f);
            }
        return m;
    };
    const Img sharp = model(0.0f), blurred = model(1.0f), half = model(0.5f);
    // The blur must actually move pixels, or the near case below proves nothing.
    ASSERT_GT(MaxDiff(std::vector<uint8_t>([&] {
                  std::vector<uint8_t> v(scene.px.size());
                  for (size_t i = 0; i < v.size(); i++)
                      v[i] = (uint8_t)std::lround(scene.px[i] * 255.0f);
                  return v;
              }()), blurred),
              64);

    struct Case {
        const char *name;
        float z;
        const Img *expect;
    } cases[] = {
        {"in focus (z = scale, f = 0)", scale, &sharp},
        {"near (z = 0, f = maxBlur = 1)", 0.0f, &blurred},
        {"half way (z = (scale + bias) / 2, f = 0.5)", 0.5f * (scale + bias), &half},
    };
    for (const Case &c : cases) {
        std::vector<uint8_t> out;
        std::string err;
        ASSERT_TRUE(RunAt(scene, c.z, out, err)) << c.name;
        EXPECT_EQ(err, "") << c.name;
        int d = MaxDiff(out, *c.expect);
        printf("[DofPass] %s: max |gpu - model| = %d / 255\n", c.name, d);
        EXPECT_LE(d, 3) << c.name;
    }
}

// The harness can fail: feeding the pass the depth+stencil view it used to
// receive must raise a validation error and be caught here.
TEST_F(DofPassTest, ErrorScopeCatchesBadDepthView) {
    GpuDevice &gpu = RB3RndBackend::Gpu();
    wgpu::Device dev = gpu.Device();
    dev.PushErrorScope(wgpu::ErrorFilter::Validation);
    wgpu::TextureDescriptor dd{};
    dd.size = {64, 64, 1};
    dd.format = wgpu::TextureFormat::Depth24PlusStencil8;
    dd.usage = wgpu::TextureUsage::RenderAttachment | wgpu::TextureUsage::TextureBinding;
    dd.sampleCount = 4;
    wgpu::Texture depthTex = dev.CreateTexture(&dd);
    wgpu::TextureDescriptor sd{};
    sd.size = {64, 64, 1};
    sd.format = wgpu::TextureFormat::RGBA8Unorm;
    sd.usage = wgpu::TextureUsage::TextureBinding;
    wgpu::Texture sceneTex = dev.CreateTexture(&sd);
    wgpu::CommandEncoder enc = dev.CreateCommandEncoder();
    mPass.Run(enc, sceneTex.CreateView(), sd.format, depthTex.CreateView(), 4, 64, 64, gpu);
    wgpu::CommandBuffer cmd = enc.Finish();
    gpu.Queue().Submit(1, &cmd);
    std::string err;
    gpu.Instance().WaitAny(
        dev.PopErrorScope(wgpu::CallbackMode::WaitAnyOnly,
                          [&](wgpu::PopErrorScopeStatus, wgpu::ErrorType type,
                              wgpu::StringView msg) {
                              if (type != wgpu::ErrorType::NoError)
                                  err.assign(msg.data, msg.length);
                          }),
        UINT64_MAX);
    EXPECT_NE(err.find("aspect"), std::string::npos) << "got: '" << err << "'";
}
