// TexBlendPass (milo-native-engine gfx/TexBlendPass.cpp), dc3 GPU flavor only.
//
// The pass ports retail RndTexBlender::DrawShowing's draws: the base map as a
// full-target rect with blending off, then each controller mesh "unwrapped" by
// the unwrapuv shader (vertex at its UV, pixel = vec4(tex.rgb, alpha), blend
// SrcAlpha / InvSrcAlpha). Band heads compose head_wrinkle_output.tex with it.
// These cases run the real pass on the real device inside a validation error
// scope and compare its output with a CPU model: a gradient base and two
// layers, a quad over UV [0.25, 0.75]^2 in the static vertex layout and a
// triangle in the skinned one, so both vertex strides, the UV-to-texel
// orientation (v = 0 is row 0) and the blend order are checked.
#include "test_helpers.h"

#include "rb3_rnd_backend.h"
#include "gfx/GpuDevice.h"
#include "gfx/TexBlendPass.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>
#include <vector>

namespace {

constexpr int kW = 64, kH = 64;

struct Rgba {
    uint8_t r, g, b, a;
};

class TexBlendPassTest : public ::testing::Test {
protected:
    void SetUp() override {
        if (!RB3RndBackend::InitGpu(64, 64, /*headless=*/true))
            GTEST_SKIP() << "headless GPU device unavailable on this host";
        if (RB3RndBackend::Gpu().IsNullBackend())
            GTEST_SKIP() << "null Dawn backend: no rendering to read back";
    }
    void TearDown() override { mPass.Terminate(); }

    wgpu::Texture MakeTex(int w, int h, const std::vector<Rgba> &px, bool target) {
        GpuDevice &gpu = RB3RndBackend::Gpu();
        wgpu::TextureDescriptor d{};
        d.size = {(uint32_t)w, (uint32_t)h, 1};
        d.format = wgpu::TextureFormat::RGBA8Unorm;
        d.usage = wgpu::TextureUsage::TextureBinding | wgpu::TextureUsage::CopyDst;
        if (target)
            d.usage |= wgpu::TextureUsage::RenderAttachment | wgpu::TextureUsage::CopySrc;
        wgpu::Texture t = gpu.Device().CreateTexture(&d);
        wgpu::TexelCopyTextureInfo dst{};
        dst.texture = t;
        wgpu::TexelCopyBufferLayout layout{};
        layout.bytesPerRow = w * 4;
        layout.rowsPerImage = h;
        gpu.Queue().WriteTexture(&dst, px.data(), px.size() * 4, &layout, &d.size);
        return t;
    }

    // A vertex stream of `stride`-byte vertices whose UV (byte 40) is `uv`;
    // every other byte is garbage the pass must ignore.
    TexBlendPass::Layer MakeLayer(uint32_t stride, const std::vector<float> &uv,
                                  const std::vector<uint16_t> &idx, wgpu::TextureView tex,
                                  float alpha) {
        GpuDevice &gpu = RB3RndBackend::Gpu();
        const size_t n = uv.size() / 2;
        std::vector<uint8_t> vb(n * stride, 0xCD);
        for (size_t i = 0; i < n; i++)
            memcpy(&vb[i * stride + TexBlendPass::kUVOffset], &uv[i * 2], 8);
        wgpu::BufferDescriptor bd{};
        bd.usage = wgpu::BufferUsage::Vertex | wgpu::BufferUsage::CopyDst;
        bd.size = vb.size();
        TexBlendPass::Layer l;
        l.vertexBuffer = gpu.Device().CreateBuffer(&bd);
        gpu.Queue().WriteBuffer(l.vertexBuffer, 0, vb.data(), vb.size());
        l.vertexStride = stride;
        l.vertexBytes = vb.size();
        std::vector<uint16_t> ib = idx;
        if (ib.size() & 1)
            ib.push_back(0);  // WriteBuffer sizes are 4-byte multiples
        bd.usage = wgpu::BufferUsage::Index | wgpu::BufferUsage::CopyDst;
        bd.size = ib.size() * 2;
        l.indexBuffer = gpu.Device().CreateBuffer(&bd);
        gpu.Queue().WriteBuffer(l.indexBuffer, 0, ib.data(), ib.size() * 2);
        l.indexCount = (uint32_t)idx.size();
        l.tex = tex;
        l.alpha = alpha;
        return l;
    }

    // Records and submits the pass into a kW x kH target that starts as
    // `initial`, collecting validation errors; reads the target back.
    bool Run(const std::vector<Rgba> &initial, wgpu::TextureView base,
             const std::vector<TexBlendPass::Layer> &layers, std::vector<uint8_t> &out,
             std::string &errors) {
        GpuDevice &gpu = RB3RndBackend::Gpu();
        wgpu::Device dev = gpu.Device();
        dev.PushErrorScope(wgpu::ErrorFilter::Validation);
        wgpu::Texture target = MakeTex(kW, kH, initial, true);
        wgpu::CommandEncoder enc = dev.CreateCommandEncoder();
        bool ran = mPass.Record(enc, target.CreateView(), wgpu::TextureFormat::RGBA8Unorm, kW,
                                kH, base, layers.data(), layers.size(), gpu);
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
        out.assign((size_t)kW * kH * 4, 0);
        EXPECT_TRUE(gpu.ReadbackTexture(target, kW, kH, out.data(), out.size()));
        return ran;
    }

    TexBlendPass mPass;
};

std::vector<Rgba> Gradient() {
    std::vector<Rgba> px(kW * kH);
    for (int y = 0; y < kH; y++)
        for (int x = 0; x < kW; x++)
            px[y * kW + x] = {(uint8_t)(x * 4), (uint8_t)(y * 4), 128, 255};
    return px;
}

std::vector<Rgba> Solid(int n, Rgba c) { return std::vector<Rgba>(n, c); }

float Blend(float dst, float src, float a) { return src * a + dst * (1.0f - a); }

int Diff(const std::vector<uint8_t> &gpu, const std::vector<float> &model) {
    int worst = 0;
    for (size_t i = 0; i < gpu.size(); i++)
        worst = std::max(worst, std::abs((int)gpu[i] - (int)std::lround(model[i])));
    return worst;
}

} // namespace

TEST_F(TexBlendPassTest, BaseThenUnwrappedLayersMatchRetailModel) {
    const std::vector<Rgba> base = Gradient();
    wgpu::Texture baseTex = MakeTex(kW, kH, base, false);
    // Layer textures: 8x8 solid colours, so the bilinear sample is exact.
    wgpu::Texture redTex = MakeTex(8, 8, Solid(64, {255, 0, 0, 0}), false);
    wgpu::Texture blueTex = MakeTex(8, 8, Solid(64, {0, 0, 255, 255}), false);

    // Static layout (stride 64): a quad over UV [0.25, 0.75]^2, alpha 0.5.
    // The texel alpha (0) must not matter: retail writes the material alpha.
    std::vector<TexBlendPass::Layer> layers;
    layers.push_back(MakeLayer(64, {0.25f, 0.25f, 0.75f, 0.25f, 0.75f, 0.75f, 0.25f, 0.75f},
                               {0, 1, 2, 0, 2, 3}, redTex.CreateView(), 0.5f));
    // Skinned layout (stride 88): the triangle u in [0, 0.5], v in [0, 0.25]
    // under the line through (0,0) and (0.5,0.25), drawn second, alpha 0.25.
    // Its winding is the opposite of the quad's: no culling.
    layers.push_back(MakeLayer(88, {0.0f, 0.0f, 0.5f, 0.25f, 0.5f, 0.0f}, {0, 1, 2},
                               blueTex.CreateView(), 0.25f));

    std::vector<uint8_t> out;
    std::string err;
    ASSERT_TRUE(Run(Solid(kW * kH, {9, 9, 9, 9}), baseTex.CreateView(), layers, out, err));
    EXPECT_EQ(err, "");

    std::vector<float> model((size_t)kW * kH * 4);
    int quadPx = 0, triPx = 0;
    for (int y = 0; y < kH; y++)
        for (int x = 0; x < kW; x++) {
            float c[4] = {(float)base[y * kW + x].r, (float)base[y * kW + x].g,
                          (float)base[y * kW + x].b, 255.0f};
            const float u = (x + 0.5f) / kW, v = (y + 0.5f) / kH;
            if (u > 0.25f && u < 0.75f && v > 0.25f && v < 0.75f) {
                const float src[4] = {255, 0, 0, 255 * 0.5f};
                for (int k = 0; k < 4; k++)
                    c[k] = Blend(c[k], src[k], 0.5f);
                quadPx++;
            }
            if (u < 0.5f && v < 0.5f * u) {
                const float src[4] = {0, 0, 255, 255 * 0.25f};
                for (int k = 0; k < 4; k++)
                    c[k] = Blend(c[k], src[k], 0.25f);
                triPx++;
            }
            for (int k = 0; k < 4; k++)
                model[((size_t)y * kW + x) * 4 + k] = c[k];
        }
    ASSERT_GT(quadPx, 0);
    ASSERT_GT(triPx, 0);
    // Pixels on a triangle edge may fall either side of the rasteriser's
    // tie-break; compare everything off the triangle's diagonal exactly.
    int worst = 0, edge = 0;
    for (int y = 0; y < kH; y++)
        for (int x = 0; x < kW; x++) {
            const float u = (x + 0.5f) / kW, v = (y + 0.5f) / kH;
            if (u < 0.5f && std::fabs(v - 0.5f * u) < 1.5f / kH) {
                edge++;
                continue;
            }
            for (int k = 0; k < 4; k++) {
                size_t i = ((size_t)y * kW + x) * 4 + k;
                worst = std::max(worst, std::abs((int)out[i] - (int)std::lround(model[i])));
            }
        }
    printf("[TexBlendPass] quad px %d, triangle px %d, edge px skipped %d, max |gpu - model| "
           "= %d / 255\n",
           quadPx, triPx, edge, worst);
    EXPECT_LE(worst, 2);
    // The quad's centre texel, exactly.
    const size_t c = ((size_t)32 * kW + 32) * 4;
    EXPECT_NEAR(out[c + 0], Blend(128, 255, 0.5f), 1.0);
    EXPECT_NEAR(out[c + 1], Blend(128, 0, 0.5f), 1.0);
}

// Without a base the target keeps what it held and only the layers draw.
TEST_F(TexBlendPassTest, NullBaseLoadsTarget) {
    wgpu::Texture redTex = MakeTex(8, 8, Solid(64, {255, 0, 0, 255}), false);
    std::vector<TexBlendPass::Layer> layers;
    layers.push_back(MakeLayer(64, {0.0f, 0.0f, 0.5f, 0.0f, 0.5f, 1.0f, 0.0f, 1.0f},
                               {0, 1, 2, 0, 2, 3}, redTex.CreateView(), 1.0f));
    std::vector<uint8_t> out;
    std::string err;
    ASSERT_TRUE(Run(Solid(kW * kH, {10, 20, 30, 40}), wgpu::TextureView(), layers, out, err));
    EXPECT_EQ(err, "");
    std::vector<float> model((size_t)kW * kH * 4);
    for (int y = 0; y < kH; y++)
        for (int x = 0; x < kW; x++) {
            float *m = &model[((size_t)y * kW + x) * 4];
            if (x < kW / 2) {
                m[0] = 255; m[1] = 0; m[2] = 0; m[3] = 255;
            } else {
                m[0] = 10; m[1] = 20; m[2] = 30; m[3] = 40;
            }
        }
    EXPECT_LE(Diff(out, model), 1);
}

// The harness can fail: a target without RenderAttachment usage must raise a
// validation error inside the scope.
TEST_F(TexBlendPassTest, ErrorScopeCatchesUnrenderableTarget) {
    GpuDevice &gpu = RB3RndBackend::Gpu();
    wgpu::Device dev = gpu.Device();
    wgpu::Texture notTarget = MakeTex(kW, kH, Gradient(), false);
    dev.PushErrorScope(wgpu::ErrorFilter::Validation);
    wgpu::CommandEncoder enc = dev.CreateCommandEncoder();
    mPass.Record(enc, notTarget.CreateView(), wgpu::TextureFormat::RGBA8Unorm, kW, kH,
                 notTarget.CreateView(), nullptr, 0, gpu);
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
    EXPECT_NE(err, "");
}
