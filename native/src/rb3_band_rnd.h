// RB3-Wii native GPU rendering backend — BandRnd (Strategy B, milestone iii).
//
// BandRnd : Rnd (RB3's rndobj/Rnd.h, NOT NgRnd — RB3 has no NgRnd). It owns the
// milo-native-engine WebGPU gfx CORE (GpuDevice + PipelineManager + per-draw
// uniform buffers + default textures) and provides the real, out-of-line bodies
// for the RndMesh/RndTex methods that the matched fork declares
// `#ifdef HX_NATIVE virtual ...` and that rndobj_synth_link_stubs.s currently
// satisfies with WEAK no-ops. A strong definition here displaces the weak alias.
//
// Structurally modeled on milo-native-engine src/platform/Rnd_Wgpu.cpp (WgpuRnd)
// + Mesh_Wgpu.cpp, adapted to RB3's older rndobj shapes (Cam has no
// GetViewProjectXfms; RndMesh::Vert uses packed Color32; RndMat shape differs).

#pragma once

#include "gfx/GpuDevice.h"
#include "gfx/PipelineManager.h"
#include "gfx/Screenshot.h"
#include "rndobj/Rnd.h"
#include "rb3_gpu_uniforms.h"

#include <webgpu/webgpu_cpp.h>
#include <string>
#include <vector>

class RndCam;
class RndMesh;
class RndMat;

// Simple bump-allocated uniform ring (mirrors Rnd_Wgpu.h UniformRingBuffer).
class BandUniformRing {
public:
    void Init(wgpu::Device device, uint32_t capacity, const char* label);
    void Reset() { mOffset = 0; }
    void Release() { mBuffer = nullptr; mDevice = nullptr; }
    uint32_t Write(wgpu::Queue queue, const void* data, uint32_t size);
    wgpu::Buffer& Buffer() { return mBuffer; }
private:
    static constexpr uint32_t kAlign = 256;
    wgpu::Device mDevice;
    wgpu::Buffer mBuffer;
    uint32_t mCapacity = 0;
    uint32_t mOffset = 0;
    const char* mLabel = "BandRing";
};

class BandRnd : public Rnd {
public:
    BandRnd() {}
    virtual ~BandRnd() {}

    // Curated PreInit: register the rndobj factories (mirrors Rnd::PreInit) and
    // create defaults, WITHOUT the GPU/TheRnd/overlay/console parts. Call before
    // loading a milo. Idempotent.
    void PreInitRender();

    // Stand up the engine GpuDevice + pipelines + uniform buffers + default
    // textures. Returns false on failure.
    bool InitGpu(int width, int height, bool headless);

    // Per-frame: acquire target, begin pass with clear, reset rings, write scene
    // uniforms from the given camera.
    void BeginFrame(RndCam* cam);
    // End pass + submit. Does NOT present (headless readback comes from GpuDevice).
    void EndFrame();

    // Draw one RndMesh (called from the engine RndMesh::DrawShowing body).
    void DrawMesh(RndMesh* mesh);

    GpuDevice& Gpu() { return mGpu; }
    wgpu::RenderPassEncoder& Pass() { return mPass; }
    bool InPass() const { return mInPass; }

    // Initialise auto-screenshot from env vars (call after InitGpu).
    // Reads MILO_SCREENSHOT_DIR and MILO_SCREENSHOT_FRAMES (comma-separated
    // frame numbers); if set, captures a PNG at each listed frame number.
    // Frame names (optional) come from MILO_SCREENSHOT_NAMES (comma-separated,
    // same count as MILO_SCREENSHOT_FRAMES).
    void InitScreenshots();

    // --- Rnd virtual overrides ---
    // BeginDrawing: acquire GPU target + start render pass (using current cam).
    void BeginDrawing() override;
    // EndDrawing: end pass + submit + optionally capture screenshot.
    void EndDrawing() override;

private:
    void WriteSceneUniforms(RndCam* cam);
    void CreateDefaultTextures();
    wgpu::BindGroup MakeMaterialBindGroup(uint32_t off, RndMat* mat);
    wgpu::BindGroup MakeMaterialBindGroupRaw(wgpu::Buffer buf, uint32_t off);

public:
    GpuDevice mGpu;
    PipelineManager mPipelines;

    BandUniformRing mSceneRing;
    BandUniformRing mMaterialRing;
    BandUniformRing mObjectRing;
    BandUniformRing mBoneRing;

    wgpu::TextureFormat mTargetFmt = wgpu::TextureFormat::RGBA8Unorm;
    wgpu::CommandEncoder mEncoder;
    wgpu::RenderPassEncoder mPass;
    wgpu::TextureView mFrameView;
    bool mInPass = false;

    wgpu::Texture mDepthTex;
    wgpu::TextureView mDepthView;

    // Scene bind group (group 0)
    wgpu::BindGroup mSceneBindGroup;
    uint32_t mSceneOffset = 0;

    // Default textures
    wgpu::Texture mWhiteTex, mBlackTex, mFlatNormalTex, mBlackCubeTex, mShadowTex;
    wgpu::TextureView mWhiteView, mBlackView, mFlatNormalView, mBlackCubeView, mShadowView;
    wgpu::Sampler mSampler, mShadowSampler;

    bool mGpuReady = false;
    bool mPreInited = false;
    int mDrawnMeshes = 0;
    int mDrawnTris = 0;

    // Frame counter (incremented each EndDrawing call — the base Rnd::EndDrawing
    // is bypassed so mFrameID never advances; we keep our own).
    int mFrameCount = 0;

    // Auto-screenshot state (MILO_SCREENSHOT_DIR / MILO_SCREENSHOT_FRAMES).
    std::string mShotDir;
    std::vector<int> mShotFrames;
    std::vector<std::string> mShotNames;
    int mShotIndex = 0;
};

extern BandRnd gBandRnd;
