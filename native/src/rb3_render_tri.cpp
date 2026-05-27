// RB3-Wii native GPU rendering — milestone (ii): the triangle.
//
// Proves the milo-native-engine WebGPU gfx CORE (GpuDevice + PipelineManager +
// the standard shader) works under RB3's link, with NO rndobj involved. We
// build a real render pipeline (kStandardShader, static vertex layout), a
// hand-built 3-vertex buffer, identity-ish uniforms for all 4 bind groups
// (scene/material/object/bone), draw one triangle into the headless offscreen
// target, read it back, and write a PNG.
//
// Gated by RB3_RENDER_TRI=1 in main_native.cpp. The bind-group / default-texture
// scaffolding here is the same shape the mesh backend (BandRnd, milestone iii)
// reuses.

#include "gfx/GpuDevice.h"
#include "gfx/PipelineManager.h"
#include "gfx/Screenshot.h"
#include "rb3_gpu_uniforms.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <webgpu/webgpu_cpp.h>

// Note: the render pipeline takes its vertex layout from the engine's
// VertexFormats::StaticLayout() (we provide that symbol in rb3_band_rnd.cpp,
// since the engine's VertexFormats.cpp is excluded for RB3). The hand-built
// triangle below just matches GpuVertexRB3's byte layout.

// Create a 1x1 RGBA texture filled with the given color.
static wgpu::Texture MakeSolidTexture(GpuDevice& gpu, wgpu::TextureFormat fmt,
                                      uint8_t r, uint8_t g, uint8_t b, uint8_t a,
                                      const char* label) {
    wgpu::TextureDescriptor desc{};
    desc.label = label;
    desc.size = {1, 1, 1};
    desc.format = fmt;
    desc.usage = wgpu::TextureUsage::TextureBinding | wgpu::TextureUsage::CopyDst;
    desc.mipLevelCount = 1;
    wgpu::Texture tex = gpu.Device().CreateTexture(&desc);

    uint8_t px[4] = {r, g, b, a};
    wgpu::TexelCopyTextureInfo dst{};
    dst.texture = tex;
    wgpu::TexelCopyBufferLayout lay{};
    lay.bytesPerRow = 4;
    lay.rowsPerImage = 1;
    wgpu::Extent3D ext = {1, 1, 1};
    gpu.Queue().WriteTexture(&dst, px, 4, &lay, &ext);
    return tex;
}

int RunRenderTri() {
    bool headless = (getenv("MILO_HEADLESS") != nullptr) || (getenv("DISPLAY") == nullptr);

    GpuDeviceDesc desc{};
    desc.headless = headless;
    desc.width  = getenv("MILO_WIDTH")  ? atoi(getenv("MILO_WIDTH"))  : 256;
    desc.height = getenv("MILO_HEIGHT") ? atoi(getenv("MILO_HEIGHT")) : 256;
    desc.title  = "rb3-native triangle";

    printf("rb3-native: RENDER_TRI — init GpuDevice (%dx%d, %s)\n",
           desc.width, desc.height, headless ? "headless" : "windowed");

    GpuDevice gpu;
    if (!gpu.Init(desc)) {
        fprintf(stderr, "rb3-native: GpuDevice init FAILED\n");
        return 1;
    }
    if (!gpu.IsReady()) {
        fprintf(stderr, "rb3-native: GpuDevice not ready\n");
        return 1;
    }

    PipelineManager pipelines;
    pipelines.Init(&gpu);

    // Render-target format: headless uses RGBA8Unorm (the offscreen tex format).
    // The depth-stencil format is fixed by PipelineManager::MapDepthStencil.
    wgpu::TextureFormat targetFmt = wgpu::TextureFormat::RGBA8Unorm;
    const int W = gpu.WindowWidth(), H = gpu.WindowHeight();

    // --- Depth texture (the standard pipeline expects one) ---
    wgpu::Texture depthTex;
    wgpu::TextureView depthView;
    {
        wgpu::TextureDescriptor dd{};
        dd.label = "TriDepth";
        dd.size = {(uint32_t)W, (uint32_t)H, 1};
        dd.format = wgpu::TextureFormat::Depth24PlusStencil8;
        dd.usage = wgpu::TextureUsage::RenderAttachment;
        dd.mipLevelCount = 1;
        depthTex = gpu.Device().CreateTexture(&dd);
        depthView = depthTex.CreateView();
    }

    // --- Default textures + sampler for the material/scene bind groups ---
    wgpu::Texture whiteTex = MakeSolidTexture(gpu, targetFmt, 255, 255, 255, 255, "White");
    wgpu::Texture blackTex = MakeSolidTexture(gpu, wgpu::TextureFormat::RGBA8Unorm, 0, 0, 0, 255, "Black");
    wgpu::Texture flatNormalTex = MakeSolidTexture(gpu, wgpu::TextureFormat::RGBA8Unorm, 128, 128, 255, 255, "FlatNormal");
    wgpu::TextureView whiteView = whiteTex.CreateView();
    wgpu::TextureView blackView = blackTex.CreateView();
    wgpu::TextureView flatNormalView = flatNormalTex.CreateView();

    // Cube map (black) for the environ slot.
    wgpu::Texture blackCubeTex;
    wgpu::TextureView blackCubeView;
    {
        wgpu::TextureDescriptor cd{};
        cd.label = "BlackCube";
        cd.size = {1, 1, 6};
        cd.format = wgpu::TextureFormat::RGBA8Unorm;
        cd.usage = wgpu::TextureUsage::TextureBinding | wgpu::TextureUsage::CopyDst;
        cd.mipLevelCount = 1;
        blackCubeTex = gpu.Device().CreateTexture(&cd);
        uint8_t px[4] = {0, 0, 0, 255};
        for (uint32_t f = 0; f < 6; f++) {
            wgpu::TexelCopyTextureInfo dst{};
            dst.texture = blackCubeTex;
            dst.origin = {0, 0, f};
            wgpu::TexelCopyBufferLayout lay{};
            lay.bytesPerRow = 4; lay.rowsPerImage = 1;
            wgpu::Extent3D ext = {1, 1, 1};
            gpu.Queue().WriteTexture(&dst, px, 4, &lay, &ext);
        }
        wgpu::TextureViewDescriptor vd{};
        vd.dimension = wgpu::TextureViewDimension::Cube;
        vd.arrayLayerCount = 6;
        blackCubeView = blackCubeTex.CreateView(&vd);
    }

    wgpu::SamplerDescriptor sampDesc{};
    sampDesc.magFilter = wgpu::FilterMode::Linear;
    sampDesc.minFilter = wgpu::FilterMode::Linear;
    sampDesc.addressModeU = wgpu::AddressMode::Repeat;
    sampDesc.addressModeV = wgpu::AddressMode::Repeat;
    wgpu::Sampler sampler = gpu.Device().CreateSampler(&sampDesc);

    // Depth shadow map (group 0 binding 1) + comparison sampler.
    wgpu::Texture shadowTex;
    wgpu::TextureView shadowView;
    {
        wgpu::TextureDescriptor sd{};
        sd.label = "ShadowDummy";
        sd.size = {1, 1, 1};
        sd.format = wgpu::TextureFormat::Depth24Plus;
        sd.usage = wgpu::TextureUsage::TextureBinding | wgpu::TextureUsage::RenderAttachment;
        sd.mipLevelCount = 1;
        shadowTex = gpu.Device().CreateTexture(&sd);
        shadowView = shadowTex.CreateView();
    }
    wgpu::SamplerDescriptor cmpDesc{};
    cmpDesc.compare = wgpu::CompareFunction::LessEqual;
    wgpu::Sampler shadowSampler = gpu.Device().CreateSampler(&cmpDesc);

    // --- Uniform buffers ---
    auto makeUbo = [&](uint32_t size, const char* label) {
        wgpu::BufferDescriptor bd{};
        bd.label = label;
        bd.size = size;
        bd.usage = wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst;
        return gpu.Device().CreateBuffer(&bd);
    };
    wgpu::Buffer sceneBuf = makeUbo(sizeof(SceneUniforms), "Scene");
    wgpu::Buffer matBuf   = makeUbo(sizeof(MaterialUniforms), "Material");
    wgpu::Buffer objBuf   = makeUbo(sizeof(ObjectUniforms), "Object");
    wgpu::Buffer boneBuf  = makeUbo(sizeof(BoneUniforms), "Bone");

    // Scene: identity viewProj (clip-space triangle), camera at +Z, no lights
    // -> rely on prelit vertex color path so the triangle is its own color.
    SceneUniforms scene{};
    for (int i = 0; i < 16; i++) { scene.viewProj[i] = (i % 5 == 0) ? 1.f : 0.f; scene.view[i] = scene.viewProj[i]; }
    scene.cameraPos[2] = 1.0f;
    scene.numLights = 0;
    scene.ambientColor[0] = scene.ambientColor[1] = scene.ambientColor[2] = 1.0f;
    scene.ambientColor[3] = 1.0f;
    gpu.Queue().WriteBuffer(sceneBuf, 0, &scene, sizeof(scene));

    MaterialUniforms mat{};
    mat.color[0] = mat.color[1] = mat.color[2] = mat.color[3] = 1.0f;
    mat.useTexture = 0.0f;
    mat.intensify = 1.0f;
    mat.prelit = 1.0f;           // vertex color is final color (skip lighting)
    mat.alphaThreshold = 0.0f;
    gpu.Queue().WriteBuffer(matBuf, 0, &mat, sizeof(mat));

    ObjectUniforms obj{};
    for (int i = 0; i < 16; i++) { obj.world[i] = (i % 5 == 0) ? 1.f : 0.f; obj.worldInvTranspose[i] = obj.world[i]; }
    gpu.Queue().WriteBuffer(objBuf, 0, &obj, sizeof(obj));

    BoneUniforms bones{};
    for (int b = 0; b < kMaxBones; b++)
        for (int i = 0; i < 16; i++) bones.bones[b][i] = (i % 5 == 0) ? 1.f : 0.f;
    gpu.Queue().WriteBuffer(boneBuf, 0, &bones, sizeof(bones));

    // --- Bind groups (must match PipelineManager's 4 layouts) ---
    // Group 0: scene UBO + shadow depth tex + comparison sampler + projlight tex + sampler
    wgpu::BindGroup sceneBG;
    {
        wgpu::BindGroupEntry e[5] = {};
        e[0].binding = 0; e[0].buffer = sceneBuf; e[0].size = sizeof(SceneUniforms);
        e[1].binding = 1; e[1].textureView = shadowView;
        e[2].binding = 2; e[2].sampler = shadowSampler;
        e[3].binding = 3; e[3].textureView = whiteView;
        e[4].binding = 4; e[4].sampler = sampler;
        wgpu::BindGroupDescriptor bd{};
        bd.layout = pipelines.SceneLayout();
        bd.entryCount = 5; bd.entries = e;
        sceneBG = gpu.Device().CreateBindGroup(&bd);
    }
    // Group 1: material UBO + 6 tex + 2 samplers + cube + detail
    wgpu::BindGroup matBG;
    {
        wgpu::BindGroupEntry e[11] = {};
        e[0].binding = 0;  e[0].buffer = matBuf; e[0].size = sizeof(MaterialUniforms);
        e[1].binding = 1;  e[1].textureView = whiteView;
        e[2].binding = 2;  e[2].sampler = sampler;
        e[3].binding = 3;  e[3].textureView = flatNormalView;
        e[4].binding = 4;  e[4].textureView = blackView;
        e[5].binding = 5;  e[5].textureView = blackView;
        e[6].binding = 6;  e[6].textureView = blackView;
        e[7].binding = 7;  e[7].sampler = sampler;
        e[8].binding = 8;  e[8].textureView = blackCubeView;
        e[9].binding = 9;  e[9].sampler = sampler;
        e[10].binding = 10; e[10].textureView = flatNormalView;
        wgpu::BindGroupDescriptor bd{};
        bd.layout = pipelines.MaterialLayout();
        bd.entryCount = 11; bd.entries = e;
        matBG = gpu.Device().CreateBindGroup(&bd);
    }
    // Group 2: object UBO
    wgpu::BindGroup objBG;
    {
        wgpu::BindGroupEntry e{};
        e.binding = 0; e.buffer = objBuf; e.size = sizeof(ObjectUniforms);
        wgpu::BindGroupDescriptor bd{};
        bd.layout = pipelines.ObjectLayout();
        bd.entryCount = 1; bd.entries = &e;
        objBG = gpu.Device().CreateBindGroup(&bd);
    }
    // Group 3: bone UBO
    wgpu::BindGroup boneBG;
    {
        wgpu::BindGroupEntry e{};
        e.binding = 0; e.buffer = boneBuf; e.size = sizeof(BoneUniforms);
        wgpu::BindGroupDescriptor bd{};
        bd.layout = pipelines.BoneLayout();
        bd.entryCount = 1; bd.entries = &e;
        boneBG = gpu.Device().CreateBindGroup(&bd);
    }

    // --- Vertex buffer: one triangle in clip space, bright vertex colors ---
    GpuVertexRB3 verts[3] = {};
    auto setV = [](GpuVertexRB3& v, float x, float y, float r, float g, float b) {
        v.pos[0] = x; v.pos[1] = y; v.pos[2] = 0.5f;
        v.norm[2] = 1.0f;
        v.color[0] = r; v.color[1] = g; v.color[2] = b; v.color[3] = 1.0f;
        v.uv[0] = 0; v.uv[1] = 0;
        v.tangent[0] = 1.0f; v.tangent[3] = 1.0f;
    };
    setV(verts[0],  0.0f,  0.7f, 1.0f, 0.0f, 0.0f); // top — red
    setV(verts[1], -0.7f, -0.6f, 0.0f, 1.0f, 0.0f); // bottom-left — green
    setV(verts[2],  0.7f, -0.6f, 0.0f, 0.0f, 1.0f); // bottom-right — blue

    wgpu::Buffer vtxBuf;
    {
        wgpu::BufferDescriptor bd{};
        bd.label = "TriVerts";
        bd.size = sizeof(verts);
        bd.usage = wgpu::BufferUsage::Vertex | wgpu::BufferUsage::CopyDst;
        vtxBuf = gpu.Device().CreateBuffer(&bd);
        gpu.Queue().WriteBuffer(vtxBuf, 0, verts, sizeof(verts));
    }

    // --- Pipeline (kStandardShader == shaderType 0; no MSAA; cull none) ---
    PipelineKey key{};
    key.shaderType = 0;
    key.blend = WgpuBlend::Src;
    key.zMode = WgpuZMode::Normal;
    key.cull = WgpuCull::None;
    key.stencil = WgpuStencil::Ignore;
    key.layout = VertexLayoutType::Static;
    key.targetFormat = targetFmt;
    key.sampleCount = 1;
    key.hasDepth = true;
    key.alphaCut = false;
    key.alphaWrite = true;
    wgpu::RenderPipeline pipe = pipelines.GetPipeline(key);
    if (!pipe) {
        fprintf(stderr, "rb3-native: pipeline creation FAILED\n");
        return 1;
    }

    // --- Render ---
    wgpu::TextureView frame = gpu.AcquireHeadlessFrame();
    if (!frame) {
        fprintf(stderr, "rb3-native: AcquireHeadlessFrame FAILED\n");
        return 1;
    }
    wgpu::CommandEncoder enc = gpu.Device().CreateCommandEncoder();

    wgpu::RenderPassColorAttachment colorAtt{};
    colorAtt.view = frame;
    colorAtt.loadOp = wgpu::LoadOp::Clear;
    colorAtt.storeOp = wgpu::StoreOp::Store;
    colorAtt.clearValue = {0.392, 0.584, 0.929, 1.0}; // cornflower blue

    wgpu::RenderPassDepthStencilAttachment depthAtt{};
    depthAtt.view = depthView;
    depthAtt.depthLoadOp = wgpu::LoadOp::Clear;
    depthAtt.depthStoreOp = wgpu::StoreOp::Store;
    depthAtt.depthClearValue = 1.0f;
    depthAtt.stencilLoadOp = wgpu::LoadOp::Clear;
    depthAtt.stencilStoreOp = wgpu::StoreOp::Store;
    depthAtt.stencilClearValue = 0;

    wgpu::RenderPassDescriptor rp{};
    rp.colorAttachmentCount = 1;
    rp.colorAttachments = &colorAtt;
    rp.depthStencilAttachment = &depthAtt;

    wgpu::RenderPassEncoder pass = enc.BeginRenderPass(&rp);
    pass.SetPipeline(pipe);
    pass.SetBindGroup(0, sceneBG);
    pass.SetBindGroup(1, matBG);
    pass.SetBindGroup(2, objBG);
    pass.SetBindGroup(3, boneBG);
    pass.SetVertexBuffer(0, vtxBuf);
    pass.Draw(3, 1, 0, 0);
    pass.End();

    wgpu::CommandBuffer cmd = enc.Finish();
    gpu.Queue().Submit(1, &cmd);

    // --- Readback + PNG ---
    std::vector<uint8_t> pixels((size_t)W * H * 4);
    if (!gpu.ReadbackHeadlessFrame(pixels.data(), pixels.size())) {
        fprintf(stderr, "rb3-native: readback FAILED\n");
        return 1;
    }

    // Sample a few pixels to prove non-clear-color geometry landed.
    auto px = [&](int x, int y) -> const uint8_t* { return &pixels[((size_t)y * W + x) * 4]; };
    const uint8_t* center = px(W / 2, H / 2);
    int nonClear = 0;
    for (size_t i = 0; i < (size_t)W * H; i++) {
        const uint8_t* p = &pixels[i * 4];
        // clear color ~ (100,149,237)
        if (abs((int)p[0] - 100) > 12 || abs((int)p[1] - 149) > 12 || abs((int)p[2] - 237) > 12)
            nonClear++;
    }
    printf("rb3-native: center pixel RGBA = (%u,%u,%u,%u); non-clear pixels = %d / %d\n",
           center[0], center[1], center[2], center[3], nonClear, W * H);

    const char* outPath = getenv("RB3_RENDER_TRI_PNG");
    char defPath[] = "/tmp/rb3_render_tri.png";
    if (!outPath) outPath = defPath;
    int rc = 0;
    if (WritePNG(outPath, pixels.data(), W, H)) {
        printf("rb3-native: wrote triangle frame to %s\n", outPath);
    } else {
        fprintf(stderr, "rb3-native: WritePNG FAILED\n");
        rc = 1;
    }

    pipelines.Terminate();
    gpu.Shutdown();
    printf("rb3-native: RENDER_TRI %s\n", rc == 0 ? "OK" : "FAILED");
    return rc;
}
