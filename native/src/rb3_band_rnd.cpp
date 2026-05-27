// BandRnd implementation — RB3-Wii native GPU rendering backend (Strategy B).
// See rb3_band_rnd.h.

#include "rb3_band_rnd.h"

#include "rndobj/Cam.h"
#include "rndobj/Mesh.h"
#include "rndobj/Mat.h"
#include "rndobj/Tex.h"
#include "rndobj/Text.h"
#include "rndobj/Trans.h"
#include "rndobj/Dir.h"
#include "rndobj/Group.h"
#include "rndobj/Env.h"
#include "rndobj/Lit.h"
#include "rndobj/MultiMesh.h"
#include "math/Mtx.h"
#include "math/Vec.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

// The single global renderer. TheRnd (declared extern in rndobj/Rnd.h) is a
// WEAK no-op alias in rndobj_synth_link_stubs.s; this strong definition wins.
BandRnd gBandRnd;
Rnd* TheRnd = &gBandRnd;

// Registers the legacy short-name rndobj class aliases (Tex/Text/Dir). Defined
// below; also called from the real game boot in main_native.cpp (RunGame).
void RB3RegisterLegacyRndAliases();

// ---------------------------------------------------------------------------
// VertexFormats::StaticLayout()/SkinnedLayout() — the engine's PipelineManager
// (gfx/PipelineManager.cpp) calls these, but their definitions live in
// gfx/VertexFormats.cpp, which RB3 EXCLUDES (it includes DC3's rndobj/Mesh.h for
// the Unpack* helpers we don't use). The layout fns are pure wgpu (no RndMesh),
// so we provide them here. They MUST match GpuVertexRB3 / the engine GpuVertex
// byte layout exactly (pos/norm/color/uv/tangent + skinned bone data).
// ---------------------------------------------------------------------------
namespace VertexFormats {
static wgpu::VertexAttribute MkA(wgpu::VertexFormat f, uint64_t off, uint32_t loc) {
    wgpu::VertexAttribute a{}; a.format = f; a.offset = off; a.shaderLocation = loc; return a;
}
const wgpu::VertexBufferLayout& StaticLayout() {
    static wgpu::VertexAttribute attrs[5];
    static wgpu::VertexBufferLayout layout;
    static bool inited = false;
    if (!inited) {
        attrs[0] = MkA(wgpu::VertexFormat::Float32x3, 0,  0);
        attrs[1] = MkA(wgpu::VertexFormat::Float32x3, 12, 1);
        attrs[2] = MkA(wgpu::VertexFormat::Float32x4, 24, 2);
        attrs[3] = MkA(wgpu::VertexFormat::Float32x2, 40, 3);
        attrs[4] = MkA(wgpu::VertexFormat::Float32x4, 48, 4);
        layout.arrayStride = sizeof(GpuVertexRB3);
        layout.stepMode = wgpu::VertexStepMode::Vertex;
        layout.attributeCount = 5; layout.attributes = attrs;
        inited = true;
    }
    return layout;
}
const wgpu::VertexBufferLayout& SkinnedLayout() {
    // 88-byte skinned vertex layout (matches engine GpuVertexSkinned).
    static wgpu::VertexAttribute attrs[7];
    static wgpu::VertexBufferLayout layout;
    static bool inited = false;
    if (!inited) {
        attrs[0] = MkA(wgpu::VertexFormat::Float32x3, 0,  0);
        attrs[1] = MkA(wgpu::VertexFormat::Float32x3, 12, 1);
        attrs[2] = MkA(wgpu::VertexFormat::Float32x4, 24, 2);
        attrs[3] = MkA(wgpu::VertexFormat::Float32x2, 40, 3);
        attrs[4] = MkA(wgpu::VertexFormat::Float32x4, 48, 4);
        attrs[5] = MkA(wgpu::VertexFormat::Uint8x4,   64, 5);
        attrs[6] = MkA(wgpu::VertexFormat::Float32x4, 72, 6);
        layout.arrayStride = 88;
        layout.stepMode = wgpu::VertexStepMode::Vertex;
        layout.attributeCount = 7; layout.attributes = attrs;
        inited = true;
    }
    return layout;
}
} // namespace VertexFormats

// ===========================================================================
// BandUniformRing
// ===========================================================================
void BandUniformRing::Init(wgpu::Device device, uint32_t capacity, const char* label) {
    mDevice = device;
    mLabel = label;
    wgpu::BufferDescriptor d{};
    d.label = label;
    d.size = capacity;
    d.usage = wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst;
    mBuffer = device.CreateBuffer(&d);
    mCapacity = capacity;
    mOffset = 0;
}

uint32_t BandUniformRing::Write(wgpu::Queue queue, const void* data, uint32_t size) {
    uint32_t aligned = (size + kAlign - 1) & ~(kAlign - 1);
    if (mOffset + aligned > mCapacity) mOffset = 0; // wrap (defensive)
    uint32_t off = mOffset;
    queue.WriteBuffer(mBuffer, off, data, size);
    mOffset += aligned;
    return off;
}

// ===========================================================================
// Matrix helpers — output is 16 floats COLUMN-MAJOR (WGSL mat4x4f reads cols),
// so the WGSL expression `M * v` applies M with the math we set up here.
// ===========================================================================

// A Milo Transform is row-vector: world = v * [m | v], i.e. for a point p,
//   p_world = p.x*m.x + p.y*m.y + p.z*m.z + t  (m.x/m.y/m.z are basis ROWS).
// To use it as a column-major mat4 M with `M * p` semantics (column-vector),
// the matrix columns are the basis rows of m plus translation:
//   col0 = (m.x.x, m.x.y, m.x.z, 0)? — NO. v*M means M's ROWS are the basis.
// For M*p to equal p*MiloXfm, M must be the transpose: M's COLUMNS are m.x,m.y,m.z.
// Column-major storage of M means storing M's columns contiguously, which are
// exactly m.x, m.y, m.z, then translation as the 4th column with the 4th row
// being (0,0,0,1).
static void MiloXfmToColMajor(const Transform& x, float* out) {
    // M (column-major). Column j = basis vector along axis j of the Milo xfm.
    out[0]  = x.m.x.x; out[1]  = x.m.x.y; out[2]  = x.m.x.z; out[3]  = 0.0f;
    out[4]  = x.m.y.x; out[5]  = x.m.y.y; out[6]  = x.m.y.z; out[7]  = 0.0f;
    out[8]  = x.m.z.x; out[9]  = x.m.z.y; out[10] = x.m.z.z; out[11] = 0.0f;
    out[12] = x.v.x;   out[13] = x.v.y;   out[14] = x.v.z;   out[15] = 1.0f;
}

static void MatMul4(const float* A, const float* B, float* out) {
    // out = A * B (all column-major). out_col_j = A * B_col_j.
    for (int j = 0; j < 4; j++) {
        for (int i = 0; i < 4; i++) {
            float s = 0;
            for (int k = 0; k < 4; k++) s += A[k * 4 + i] * B[j * 4 + k];
            out[j * 4 + i] = s;
        }
    }
}

// ===========================================================================
// BandRnd
// ===========================================================================

void BandRnd::PreInitRender() {
    if (mPreInited) return;
    mPreInited = true;
    // Mirror the rndobj factory registration block in Rnd::PreInit (Rnd.cpp),
    // minus the GPU / overlay / console / TheRnd-singleton parts. These register
    // under the correct OBJ_CLASSNAME and read gSystemConfig — which the boot
    // path (SystemPreInit/SystemInit) has populated by the time we get here.
    RndTransformable::Init();
    RndCam::Init();
    RndMesh::Init();
    RndEnviron::Init();
    RndMat::Init();
    RndTex::Init();
    RndLight::Init();
    RndMultiMesh::Init();
    RndTransformable::Register();
    RndGroup::Init();
    RndDir::Init();

    RB3RegisterLegacyRndAliases();
    printf("BandRnd: rndobj factories registered (Trans/Cam/Mesh/Env/Mat/Tex/Light/MultiMesh/Group/Dir + Tex/Text/Dir aliases)\n");
}

// Name aliases for the legacy milo class names. RndTex/RndText/RndDir register
// under OBJ_CLASSNAME "RndTex"/"RndText"/"RndDir", but RB3's 2010-era on-disc
// milos store the old short names "Tex"/"Text"/"Dir" (the other rndobj classes —
// Mat/Mesh/Group/Cam/Trans/MultiMesh — already use bare OBJ_CLASSNAMEs, so no
// alias is needed there). Register the short Symbols to the same factory so
// NewObject("Tex"/"Text"/"Dir") resolves (otherwise "Can't make Tex"/"Text").
// Called from BOTH the synthetic render harness (PreInitRender) and the real
// game boot (RunGame, where the real Rnd::PreInit registers the RndXxx names but
// not the short aliases). RndXxx::NewObject is a valid static factory regardless
// of registration order, so calling this any time before a milo loads is safe.
void RB3RegisterLegacyRndAliases() {
    Hmx::Object::RegisterFactory(Symbol("Tex"),  RndTex::NewObject);
    Hmx::Object::RegisterFactory(Symbol("Text"), RndText::NewObject);
    Hmx::Object::RegisterFactory(Symbol("Dir"),  RndDir::NewObject);
}

static wgpu::Texture MakeSolid(GpuDevice& gpu, wgpu::TextureFormat fmt,
                               uint8_t r, uint8_t g, uint8_t b, uint8_t a, const char* label) {
    wgpu::TextureDescriptor d{};
    d.label = label; d.size = {1, 1, 1}; d.format = fmt;
    d.usage = wgpu::TextureUsage::TextureBinding | wgpu::TextureUsage::CopyDst;
    d.mipLevelCount = 1;
    wgpu::Texture t = gpu.Device().CreateTexture(&d);
    uint8_t px[4] = {r, g, b, a};
    wgpu::TexelCopyTextureInfo dst{}; dst.texture = t;
    wgpu::TexelCopyBufferLayout lay{}; lay.bytesPerRow = 4; lay.rowsPerImage = 1;
    wgpu::Extent3D ext = {1, 1, 1};
    gpu.Queue().WriteTexture(&dst, px, 4, &lay, &ext);
    return t;
}

void BandRnd::CreateDefaultTextures() {
    mWhiteTex = MakeSolid(mGpu, mTargetFmt, 255, 255, 255, 255, "White");
    mBlackTex = MakeSolid(mGpu, wgpu::TextureFormat::RGBA8Unorm, 0, 0, 0, 255, "Black");
    mFlatNormalTex = MakeSolid(mGpu, wgpu::TextureFormat::RGBA8Unorm, 128, 128, 255, 255, "FlatNormal");
    mWhiteView = mWhiteTex.CreateView();
    mBlackView = mBlackTex.CreateView();
    mFlatNormalView = mFlatNormalTex.CreateView();

    {
        wgpu::TextureDescriptor cd{};
        cd.label = "BlackCube"; cd.size = {1, 1, 6};
        cd.format = wgpu::TextureFormat::RGBA8Unorm;
        cd.usage = wgpu::TextureUsage::TextureBinding | wgpu::TextureUsage::CopyDst;
        cd.mipLevelCount = 1;
        mBlackCubeTex = mGpu.Device().CreateTexture(&cd);
        uint8_t px[4] = {0, 0, 0, 255};
        for (uint32_t f = 0; f < 6; f++) {
            wgpu::TexelCopyTextureInfo dst{}; dst.texture = mBlackCubeTex; dst.origin = {0, 0, f};
            wgpu::TexelCopyBufferLayout lay{}; lay.bytesPerRow = 4; lay.rowsPerImage = 1;
            wgpu::Extent3D ext = {1, 1, 1};
            mGpu.Queue().WriteTexture(&dst, px, 4, &lay, &ext);
        }
        wgpu::TextureViewDescriptor vd{}; vd.dimension = wgpu::TextureViewDimension::Cube; vd.arrayLayerCount = 6;
        mBlackCubeView = mBlackCubeTex.CreateView(&vd);
    }

    wgpu::SamplerDescriptor sd{};
    sd.magFilter = wgpu::FilterMode::Linear; sd.minFilter = wgpu::FilterMode::Linear;
    sd.addressModeU = wgpu::AddressMode::Repeat; sd.addressModeV = wgpu::AddressMode::Repeat;
    mSampler = mGpu.Device().CreateSampler(&sd);

    {
        wgpu::TextureDescriptor td{};
        td.label = "ShadowDummy"; td.size = {1, 1, 1};
        td.format = wgpu::TextureFormat::Depth24Plus;
        td.usage = wgpu::TextureUsage::TextureBinding | wgpu::TextureUsage::RenderAttachment;
        td.mipLevelCount = 1;
        mShadowTex = mGpu.Device().CreateTexture(&td);
        mShadowView = mShadowTex.CreateView();
    }
    wgpu::SamplerDescriptor cs{}; cs.compare = wgpu::CompareFunction::LessEqual;
    mShadowSampler = mGpu.Device().CreateSampler(&cs);
}

bool BandRnd::InitGpu(int width, int height, bool headless) {
    if (mGpuReady) return true;
    GpuDeviceDesc desc{};
    desc.headless = headless;
    desc.width = width;
    desc.height = height;
    desc.title = "rb3-native BandRnd";
    if (!mGpu.Init(desc) || !mGpu.IsReady()) {
        fprintf(stderr, "BandRnd: GpuDevice init FAILED\n");
        return false;
    }
    mPipelines.Init(&mGpu);

    const int W = mGpu.WindowWidth(), H = mGpu.WindowHeight();
    {
        wgpu::TextureDescriptor dd{};
        dd.label = "BandDepth"; dd.size = {(uint32_t)W, (uint32_t)H, 1};
        dd.format = wgpu::TextureFormat::Depth24PlusStencil8;
        dd.usage = wgpu::TextureUsage::RenderAttachment;
        dd.mipLevelCount = 1;
        mDepthTex = mGpu.Device().CreateTexture(&dd);
        mDepthView = mDepthTex.CreateView();
    }

    mSceneRing.Init(mGpu.Device(), 16 * 1024, "SceneUBO");
    mMaterialRing.Init(mGpu.Device(), 256 * 1024, "MaterialUBO");
    mObjectRing.Init(mGpu.Device(), 256 * 1024, "ObjectUBO");
    mBoneRing.Init(mGpu.Device(), 256 * 1024, "BoneUBO");

    CreateDefaultTextures();

    mGpuReady = true;
    printf("BandRnd: GPU ready (%dx%d, %s)\n", W, H, headless ? "headless" : "windowed");
    return true;
}

void BandRnd::WriteSceneUniforms(RndCam* cam) {
    SceneUniforms s{};

    float viewProj[16];
    float camPos[3] = {0, 0, 0};

    if (getenv("RB3_RENDER_DBG")) fprintf(stderr, "[dbg] WriteSceneUniforms cam=%p\n", (void*)cam);
    if (cam) {
        cam->UpdateLocal();          // refresh mLocalProjectXfm + mWorldProjectXfm
        const Transform& world = cam->WorldXfm();
        camPos[0] = world.v.x; camPos[1] = world.v.y; camPos[2] = world.v.z;

        // View matrix: world -> camera-local. Build it DIRECTLY from the camera's
        // world-space basis (rows of WorldXfm.m: x=right, y=forward/depth, z=up)
        // and eye (WorldXfm.v), so camera-local coords are
        //   x' = dot(p-eye, right), y' = dot(p-eye, fwd), z' = dot(p-eye, up).
        // y' (depth) is then POSITIVE for points in front of the camera. (Going
        // through RB3's mInvWorldXfm + a transpose interpretation flipped the
        // depth sign, projecting everything behind the camera — clip.w < 0.)
        const Vector3& right = world.m.x;
        const Vector3& fwd   = world.m.y;
        const Vector3& up    = world.m.z;
        const Vector3& eye   = world.v;
        float view[16] = {0};
        // Column-major: column j holds the world-axis component for output row.
        // out.x' = right·p - right·eye, etc.
        view[0]  = right.x; view[4]  = right.y; view[8]  = right.z; view[12] = -(right.x*eye.x + right.y*eye.y + right.z*eye.z);
        view[1]  = fwd.x;   view[5]  = fwd.y;   view[9]  = fwd.z;   view[13] = -(fwd.x*eye.x   + fwd.y*eye.y   + fwd.z*eye.z);
        view[2]  = up.x;    view[6]  = up.y;    view[10] = up.z;    view[14] = -(up.x*eye.x    + up.y*eye.y    + up.z*eye.z);
        view[3]  = 0;       view[7]  = 0;       view[11] = 0;       view[15] = 1.0f;

        // Perspective: Milo camera-local axes are X=right, Y=forward(depth),
        // Z=up. Build a column-major clip matrix mapping that to WebGPU clip
        // (x right, y up, z in [0,1]) from YFov / aspect / near / far.
        float yfov = cam->YFov();
        if (yfov <= 0.0001f) yfov = 0.9f;
        float n = cam->NearPlane() > 0 ? cam->NearPlane() : 0.1f;
        float f = cam->FarPlane()  > n ? cam->FarPlane()  : (n + 1000.0f);
        float aspect = (float)mGpu.WindowWidth() / (float)mGpu.WindowHeight();
        float tanHalf = tanf(yfov * 0.5f);
        float sy = 1.0f / tanHalf;        // vertical scale
        float sx = sy / aspect;           // horizontal scale

        // Column-major perspective P. Camera-local p=(x,y,z): x=right, y=depth, z=up.
        //   clip.x =  sx * x
        //   clip.y =  sy * z          (Milo Z-up -> clip Y-up)
        //   clip.z =  f/(f-n) * (y - n)   (so z in [0,1], y=n -> 0, y=f -> 1)
        //   clip.w =  y               (perspective divide by depth)
        float P[16] = {0};
        // column 0 (multiplies x): contributes to clip.x
        P[0] = sx;
        // column 1 (multiplies y): contributes to clip.z and clip.w
        P[1 * 4 + 2] = f / (f - n);
        P[1 * 4 + 3] = 1.0f;
        // column 2 (multiplies z): contributes to clip.y
        P[2 * 4 + 1] = sy;
        // column 3 (constant / w=1): clip.z offset
        P[3 * 4 + 2] = -(f * n) / (f - n);

        MatMul4(P, view, viewProj);
        std::memcpy(s.view, view, sizeof(view));
    } else {
        for (int i = 0; i < 16; i++) viewProj[i] = (i % 5 == 0) ? 1.f : 0.f;
        std::memcpy(s.view, viewProj, sizeof(viewProj));
    }

    std::memcpy(s.viewProj, viewProj, sizeof(viewProj));
    s.cameraPos[0] = camPos[0]; s.cameraPos[1] = camPos[1]; s.cameraPos[2] = camPos[2];

    // One directional key light + bright ambient so unlit materials still read.
    s.numLights = 1;
    s.lightDirs[0][0] = -0.4f; s.lightDirs[0][1] = -0.5f; s.lightDirs[0][2] = -0.75f; s.lightDirs[0][3] = 0;
    s.lightColors[0][0] = 1.0f; s.lightColors[0][1] = 1.0f; s.lightColors[0][2] = 1.0f; s.lightColors[0][3] = 1.0f;
    s.ambientColor[0] = s.ambientColor[1] = s.ambientColor[2] = 0.45f; s.ambientColor[3] = 1.0f;
    s.numPointLights = 0;
    s.fogEnabled = 0;
    s.shadowEnabled = 0;
    s.numProjLights = 0;

    mSceneOffset = mSceneRing.Write(mGpu.Queue(), &s, sizeof(s));

    wgpu::BindGroupEntry e[5] = {};
    e[0].binding = 0; e[0].buffer = mSceneRing.Buffer(); e[0].offset = mSceneOffset; e[0].size = sizeof(SceneUniforms);
    e[1].binding = 1; e[1].textureView = mShadowView;
    e[2].binding = 2; e[2].sampler = mShadowSampler;
    e[3].binding = 3; e[3].textureView = mWhiteView;
    e[4].binding = 4; e[4].sampler = mSampler;
    wgpu::BindGroupDescriptor bd{};
    bd.layout = mPipelines.SceneLayout();
    bd.entryCount = 5; bd.entries = e;
    mSceneBindGroup = mGpu.Device().CreateBindGroup(&bd);
}

void BandRnd::BeginFrame(RndCam* cam) {
    if (!mGpuReady) return;
    mDrawnMeshes = 0;
    mDrawnTris = 0;
    mSceneRing.Reset();
    mMaterialRing.Reset();
    mObjectRing.Reset();
    mBoneRing.Reset();

    mFrameView = mGpu.IsHeadless() ? mGpu.AcquireHeadlessFrame() : mGpu.AcquireNextFrame();
    if (!mFrameView) { fprintf(stderr, "BandRnd: frame acquire failed\n"); return; }

    WriteSceneUniforms(cam);

    mEncoder = mGpu.Device().CreateCommandEncoder();

    wgpu::RenderPassColorAttachment colorAtt{};
    colorAtt.view = mFrameView;
    colorAtt.loadOp = wgpu::LoadOp::Clear;
    colorAtt.storeOp = wgpu::StoreOp::Store;
    colorAtt.clearValue = {(double)mClearColor.red, (double)mClearColor.green,
                           (double)mClearColor.blue, 1.0};

    wgpu::RenderPassDepthStencilAttachment depthAtt{};
    depthAtt.view = mDepthView;
    depthAtt.depthLoadOp = wgpu::LoadOp::Clear; depthAtt.depthStoreOp = wgpu::StoreOp::Store;
    depthAtt.depthClearValue = 1.0f;
    depthAtt.stencilLoadOp = wgpu::LoadOp::Clear; depthAtt.stencilStoreOp = wgpu::StoreOp::Store;
    depthAtt.stencilClearValue = 0;

    wgpu::RenderPassDescriptor rp{};
    rp.label = "BandMainPass";
    rp.colorAttachmentCount = 1; rp.colorAttachments = &colorAtt;
    rp.depthStencilAttachment = &depthAtt;

    mPass = mEncoder.BeginRenderPass(&rp);
    mInPass = true;
    mPass.SetBindGroup(0, mSceneBindGroup, 0, nullptr);
}

void BandRnd::EndFrame() {
    if (!mGpuReady) return;
    if (mInPass) { mPass.End(); mInPass = false; }
    wgpu::CommandBuffer cmd = mEncoder.Finish();
    mGpu.Queue().Submit(1, &cmd);
    mFrameView = nullptr;
    printf("BandRnd: frame drawn — %d meshes, %d tris\n", mDrawnMeshes, mDrawnTris);
}

// Build a material bind group against an explicit buffer (used for pre-warm).
wgpu::BindGroup BandRnd::MakeMaterialBindGroupRaw(wgpu::Buffer buf, uint32_t off) {
    wgpu::BindGroupEntry e[11] = {};
    e[0].binding = 0;  e[0].buffer = buf; e[0].offset = off; e[0].size = sizeof(MaterialUniforms);
    e[1].binding = 1;  e[1].textureView = mWhiteView;
    e[2].binding = 2;  e[2].sampler = mSampler;
    e[3].binding = 3;  e[3].textureView = mFlatNormalView;
    e[4].binding = 4;  e[4].textureView = mBlackView;
    e[5].binding = 5;  e[5].textureView = mBlackView;
    e[6].binding = 6;  e[6].textureView = mBlackView;
    e[7].binding = 7;  e[7].sampler = mSampler;
    e[8].binding = 8;  e[8].textureView = mBlackCubeView;
    e[9].binding = 9;  e[9].sampler = mSampler;
    e[10].binding = 10; e[10].textureView = mFlatNormalView;
    wgpu::BindGroupDescriptor bd{};
    bd.layout = mPipelines.MaterialLayout();
    bd.entryCount = 11; bd.entries = e;
    return mGpu.Device().CreateBindGroup(&bd);
}

wgpu::BindGroup BandRnd::MakeMaterialBindGroup(uint32_t off, RndMat* mat) {
    wgpu::BindGroupEntry e[11] = {};
    e[0].binding = 0;  e[0].buffer = mMaterialRing.Buffer(); e[0].offset = off; e[0].size = sizeof(MaterialUniforms);
    e[1].binding = 1;  e[1].textureView = mWhiteView;       // diffuse (default white)
    e[2].binding = 2;  e[2].sampler = mSampler;
    e[3].binding = 3;  e[3].textureView = mFlatNormalView;
    e[4].binding = 4;  e[4].textureView = mBlackView;
    e[5].binding = 5;  e[5].textureView = mBlackView;
    e[6].binding = 6;  e[6].textureView = mBlackView;
    e[7].binding = 7;  e[7].sampler = mSampler;
    e[8].binding = 8;  e[8].textureView = mBlackCubeView;
    e[9].binding = 9;  e[9].sampler = mSampler;
    e[10].binding = 10; e[10].textureView = mFlatNormalView;
    wgpu::BindGroupDescriptor bd{};
    bd.layout = mPipelines.MaterialLayout();
    bd.entryCount = 11; bd.entries = e;
    return mGpu.Device().CreateBindGroup(&bd);
}

// --- Xbox 360 compressed vertex unpacking (36 bytes/vert, big-endian) ---
// Mirrors milo-native-engine gfx/VertexFormats.cpp UnpackCompressedVertices
// (which lives in the rndobj-coupled TU RB3 excludes). The D3D vertex decl is:
//   pos   = FLOAT3   POSITION  (3 BE floats, off 0)
//   color = D3DCOLOR COLOR     (packed, off 12)
//   uv    = FLOAT16_2 TEXCOORD (off 16)
//   norm  = DEC4N    NORMAL    (10-10-10-2, off 20)
//   tan   = DEC4N    TANGENT   (off 24); bone data off 28/32.
struct XboxCVert { int pos[3]; int color; int uv; int norm; int tan; int b0; int b1; };
static_assert(sizeof(XboxCVert) == 36, "XboxCVert must be 36 bytes");

static float BeFloat(int bits) {
    unsigned v = __builtin_bswap32((unsigned)bits); float f; std::memcpy(&f, &v, 4); return f;
}
static float Half2Float(unsigned short h) {
    unsigned sign = (h >> 15) & 1, exp = (h >> 10) & 0x1F, mant = h & 0x3FF;
    unsigned f;
    if (exp == 0) { if (mant == 0) f = sign << 31; else { float val = (float)mant / 1024.0f * (1.0f/16384.0f); return sign ? -val : val; } }
    else if (exp == 0x1F) f = (sign << 31) | 0x7F800000 | (mant << 13);
    else f = (sign << 31) | ((exp - 15 + 127) << 23) | (mant << 13);
    float r; std::memcpy(&r, &f, 4); return r;
}
static void BeUV(int packed, float out[2]) {
    unsigned v = __builtin_bswap32((unsigned)packed);
    out[0] = Half2Float((v >> 16) & 0xFFFF); out[1] = Half2Float(v & 0xFFFF);
}
static void BeColor(int packed, float out[4]) {
    unsigned v = __builtin_bswap32((unsigned)packed);
    out[0] = ((v >> 0) & 0xFF) / 255.0f; out[1] = ((v >> 8) & 0xFF) / 255.0f;
    out[2] = ((v >> 16) & 0xFF) / 255.0f; out[3] = ((v >> 24) & 0xFF) / 255.0f;
}
static void BeDec4n(int packed, float out[3]) {
    unsigned v = __builtin_bswap32((unsigned)packed);
    int ix = (int)(v << 22) >> 22, iy = (int)(v << 12) >> 22, iz = (int)(v << 2) >> 22;
    out[0] = ix / 511.0f; out[1] = iy / 511.0f; out[2] = iz / 511.0f;
}

void BandRnd::DrawMesh(RndMesh* mesh) {
    if (!mGpuReady || !mInPass || !mesh) return;
    RndMesh* owner = mesh->GeomOwner();
    if (!owner) owner = mesh;
    if (getenv("RB3_RENDER_DBG")) fprintf(stderr, "[dbg] DrawMesh '%s' owner=%p\n",
        mesh->Name() ? mesh->Name() : "?", (void*)owner);

    RndMesh::VertVector& verts = owner->mVerts;
    std::vector<RndMesh::Face>& faces = owner->mFaces;
    int nv = verts.size();
    int nf = (int)faces.size();
    if (nf <= 0) return;

    // --- Unpack vertices into engine GpuVertexRB3 layout ---
    std::vector<GpuVertexRB3> gpuVerts;
    if (nv > 0) {
        // Uncompressed RB3 Vert (Color32 packed).
        gpuVerts.resize(nv);
        for (int i = 0; i < nv; i++) {
            const RndMesh::Vert& v = verts[i];
            GpuVertexRB3& g = gpuVerts[i];
            g.pos[0] = v.pos.x; g.pos[1] = v.pos.y; g.pos[2] = v.pos.z;
            g.norm[0] = v.norm.x; g.norm[1] = v.norm.y; g.norm[2] = v.norm.z;
            g.color[0] = v.color.fr(); g.color[1] = v.color.fg();
            g.color[2] = v.color.fb(); g.color[3] = v.color.fa();
            g.uv[0] = v.uv.x; g.uv[1] = v.uv.y;
            g.tangent[0] = 1.0f; g.tangent[1] = 0; g.tangent[2] = 0; g.tangent[3] = 1.0f;
        }
    } else if (owner->mCompressedVerts && owner->mNumCompressedVerts > 0) {
        // Xbox-compressed verts (read verbatim by our HX_NATIVE Mesh.cpp branch).
        nv = (int)owner->mNumCompressedVerts;
        const XboxCVert* cv = (const XboxCVert*)owner->mCompressedVerts;
        gpuVerts.resize(nv);
        for (int i = 0; i < nv; i++) {
            GpuVertexRB3& g = gpuVerts[i];
            g.pos[0] = BeFloat(cv[i].pos[0]); g.pos[1] = BeFloat(cv[i].pos[1]); g.pos[2] = BeFloat(cv[i].pos[2]);
            BeColor(cv[i].color, g.color);
            BeUV(cv[i].uv, g.uv);
            BeDec4n(cv[i].norm, g.norm);
            float t3[3]; BeDec4n(cv[i].tan, t3);
            g.tangent[0] = t3[0]; g.tangent[1] = t3[1]; g.tangent[2] = t3[2]; g.tangent[3] = 1.0f;
        }
    } else {
        return; // no geometry
    }
    nv = (int)gpuVerts.size();
    bool dbg = getenv("RB3_RENDER_DBG") != nullptr;
    std::vector<uint16_t> indices;
    indices.reserve(nf * 3);
    for (int i = 0; i < nf; i++) {
        indices.push_back(faces[i].v1);
        indices.push_back(faces[i].v2);
        indices.push_back(faces[i].v3);
    }

    wgpu::Buffer vbuf, ibuf;
    {
        wgpu::BufferDescriptor bd{};
        bd.label = "MeshVB"; bd.size = (uint64_t)nv * sizeof(GpuVertexRB3);
        bd.usage = wgpu::BufferUsage::Vertex | wgpu::BufferUsage::CopyDst;
        vbuf = mGpu.Device().CreateBuffer(&bd);
        mGpu.Queue().WriteBuffer(vbuf, 0, gpuVerts.data(), bd.size);
    }
    {
        uint64_t isz = indices.size() * sizeof(uint16_t);
        // index buffer size must be a multiple of 4
        uint64_t padded = (isz + 3) & ~3ull;
        wgpu::BufferDescriptor bd{};
        bd.label = "MeshIB"; bd.size = padded;
        bd.usage = wgpu::BufferUsage::Index | wgpu::BufferUsage::CopyDst;
        ibuf = mGpu.Device().CreateBuffer(&bd);
        mGpu.Queue().WriteBuffer(ibuf, 0, indices.data(), isz);
    }

    // --- Object uniforms: world transform of the mesh ---
    ObjectUniforms obj{};
    MiloXfmToColMajor(mesh->WorldXfm(), obj.world);
    // worldInvTranspose: for unscaled rigid xfm, the rotation part suffices.
    // Use the world rotation as-is (good enough for normals on rigid meshes).
    std::memcpy(obj.worldInvTranspose, obj.world, sizeof(obj.world));
    uint32_t objOff = mObjectRing.Write(mGpu.Queue(), &obj, sizeof(obj));
    wgpu::BindGroup objBG;
    {
        wgpu::BindGroupEntry e{};
        e.binding = 0; e.buffer = mObjectRing.Buffer(); e.offset = objOff; e.size = sizeof(ObjectUniforms);
        wgpu::BindGroupDescriptor bd{};
        bd.layout = mPipelines.ObjectLayout(); bd.entryCount = 1; bd.entries = &e;
        objBG = mGpu.Device().CreateBindGroup(&bd);
    }

    // --- Bone uniforms: identity palette (static draw; skinning later) ---
    BoneUniforms bones{};
    for (int b = 0; b < kMaxBones; b++)
        for (int i = 0; i < 16; i++) bones.bones[b][i] = (i % 5 == 0) ? 1.f : 0.f;
    uint32_t boneOff = mBoneRing.Write(mGpu.Queue(), &bones, sizeof(bones));
    wgpu::BindGroup boneBG;
    {
        wgpu::BindGroupEntry e{};
        e.binding = 0; e.buffer = mBoneRing.Buffer(); e.offset = boneOff; e.size = sizeof(BoneUniforms);
        wgpu::BindGroupDescriptor bd{};
        bd.layout = mPipelines.BoneLayout(); bd.entryCount = 1; bd.entries = &e;
        boneBG = mGpu.Device().CreateBindGroup(&bd);
    }

    // --- Material uniforms ---
    RndMat* mat = mesh->Mat();
    MaterialUniforms mu{};
    if (mat) {
        const Hmx::Color& c = mat->GetColor();
        mu.color[0] = c.red; mu.color[1] = c.green; mu.color[2] = c.blue; mu.color[3] = c.alpha;
        mu.alphaThreshold = mat->mAlphaCut ? (mat->mAlphaThresh / 255.0f) : 0.0f;
        mu.useTexture = 0.0f;   // textures not bound yet — use vertex/material color
        mu.intensify = mat->mIntensify ? 2.0f : 1.0f;
        mu.prelit = mat->mPreLit ? 1.0f : 0.0f;
    } else {
        mu.color[0] = mu.color[1] = mu.color[2] = mu.color[3] = 1.0f;
        mu.useTexture = 0.0f; mu.intensify = 1.0f; mu.prelit = 0.0f;
    }
    uint32_t matOff = mMaterialRing.Write(mGpu.Queue(), &mu, sizeof(mu));
    wgpu::BindGroup matBG = MakeMaterialBindGroup(matOff, mat);

    // --- Pipeline state from material (default to opaque, double-sided) ---
    PipelineKey key{};
    key.shaderType = 0;
    key.blend = WgpuBlend::Src;
    key.zMode = WgpuZMode::Normal;
    key.cull = WgpuCull::None;        // draw both sides (RB3 winding varies)
    key.stencil = WgpuStencil::Ignore;
    key.layout = VertexLayoutType::Static;
    key.targetFormat = mTargetFmt;
    key.sampleCount = 1;
    key.hasDepth = true;
    key.alphaCut = mat ? mat->mAlphaCut : false;
    key.alphaWrite = true;
    wgpu::RenderPipeline pipe = mPipelines.GetPipeline(key);
    if (!pipe) return;

    mPass.SetPipeline(pipe);
    mPass.SetBindGroup(0, mSceneBindGroup, 0, nullptr);
    mPass.SetBindGroup(1, matBG, 0, nullptr);
    mPass.SetBindGroup(2, objBG, 0, nullptr);
    mPass.SetBindGroup(3, boneBG, 0, nullptr);
    mPass.SetVertexBuffer(0, vbuf, 0, WGPU_WHOLE_SIZE);
    mPass.SetIndexBuffer(ibuf, wgpu::IndexFormat::Uint16, 0, WGPU_WHOLE_SIZE);
    mPass.DrawIndexed((uint32_t)indices.size(), 1, 0, 0, 0);

    mDrawnMeshes++;
    mDrawnTris += nf;
}

// ===========================================================================
// Real out-of-line bodies for the matched-fork HX_NATIVE virtuals that the
// link stubs currently weak-alias. A strong def here displaces the weak alias.
// ===========================================================================

void RndMesh::DrawShowing() {
    gBandRnd.DrawMesh(this);
}

void RndMesh::OnSync(int) {
    // Geometry changed; nothing GPU-cached to invalidate in this simple backend
    // (we re-upload every draw). No-op.
}

// RndTex render-target / bitmap-sync entry points: not needed for the mesh
// render path (no render-to-texture). Provide real no-op bodies so the engine
// methods (declared HX_NATIVE virtual) link without the weak stubs.
void RndTex::MakeDrawTarget() {}
void RndTex::FinishDrawTarget() {}
void RndTex::SyncBitmap() {}
void RndTex::PresyncBitmap() {}
