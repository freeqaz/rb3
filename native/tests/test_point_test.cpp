// Flare point tests on occlusion queries (dc3 GPU flavor only).
//
// Retail DxRnd::DoPointTests (rb3-xenon rnddx9/Rnd_Xbox.cpp) answers each flare
// Rnd::TestPoint queued with two occlusion queries drawn against the world's
// depth: a one-pixel point (visible = any sample passed) and the flare's area
// rect (the visible pixel count, which RndFlare::DrawShowing divides by the
// rect's area). It reads the answers back one frame later. Native, the engine's
// gfx/PointTestPass.cpp draws the queries and WgpuRnd hands the answers to
// Rnd::TestPoint's handler through platform/PointTestHook.h.
//
// PointTestPassTest runs the pass over a depth target the test fills itself, so
// every expected count is exact. RndTestPoint drives the real path: WgpuRnd's
// frame, a depth-writing mesh, Rnd::TestPoint on real RndFlares, world end.
// Both fail if occlusion stops working: a flare behind geometry must read 0.
#include "test_helpers.h"

#include "rb3_rnd_backend.h"
#include "gfx/GpuDevice.h"
#include "gfx/PointTestPass.h"
#include "platform/PointTestHook.h"
#include "platform/Rnd_Wgpu.h"
#include "rndobj/Cam.h"
#include "rndobj/Flare.h"
#include "rndobj/Mat.h"
#include "rndobj/Mesh.h"
#include "rndobj/Rnd.h"
#include "obj/Dir.h"

#include <cmath>
#include <map>
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

bool GpuUp() { return RB3RndBackend::InitGpu(64, 64, /*headless=*/true); }

// ---------------------------------------------------------------------------
// The pass alone
// ---------------------------------------------------------------------------

constexpr uint32_t kW = 128, kH = 64;
constexpr float kOccluderZ = 0.5f;  // written over the left half, x < 64

// Clears a 4x MSAA depth target (WgpuRnd's format) to 1 and writes
// kOccluderZ over its left half.
void FillDepth(GpuDevice &gpu, const wgpu::TextureView &view) {
    wgpu::Device dev = gpu.Device();
    static const char *kFill = R"WGSL(
@vertex fn vs(@builtin(vertex_index) i: u32) -> @builtin(position) vec4f {
    var p = array<vec2f, 6>(vec2f(-1, -1), vec2f(0, -1), vec2f(-1, 1),
                            vec2f(-1, 1), vec2f(0, -1), vec2f(0, 1));
    return vec4f(p[i], 0.5, 1.0);
}
)WGSL";
    wgpu::ShaderSourceWGSL src;
    src.code = kFill;
    wgpu::ShaderModuleDescriptor sm{};
    sm.nextInChain = &src;
    wgpu::ShaderModule mod = dev.CreateShaderModule(&sm);
    wgpu::DepthStencilState ds{};
    ds.format = wgpu::TextureFormat::Depth24PlusStencil8;
    ds.depthWriteEnabled = wgpu::OptionalBool::True;
    ds.depthCompare = wgpu::CompareFunction::Always;
    wgpu::RenderPipelineDescriptor pd{};
    pd.vertex.module = mod;
    pd.vertex.entryPoint = "vs";
    pd.depthStencil = &ds;
    pd.multisample.count = 4;
    wgpu::RenderPipeline pipe = dev.CreateRenderPipeline(&pd);

    wgpu::CommandEncoder enc = dev.CreateCommandEncoder();
    wgpu::RenderPassDepthStencilAttachment da{};
    da.view = view;
    da.depthLoadOp = wgpu::LoadOp::Clear;
    da.depthStoreOp = wgpu::StoreOp::Store;
    da.depthClearValue = 1.0f;
    da.stencilLoadOp = wgpu::LoadOp::Clear;
    da.stencilStoreOp = wgpu::StoreOp::Store;
    wgpu::RenderPassDescriptor rp{};
    rp.depthStencilAttachment = &da;
    wgpu::RenderPassEncoder pass = enc.BeginRenderPass(&rp);
    pass.SetPipeline(pipe);
    pass.Draw(6);
    pass.End();
    wgpu::CommandBuffer cmd = enc.Finish();
    gpu.Queue().Submit(1, &cmd);
}

PointTestPass::Query MakeQuery(int key, float x, float y, float size, float z) {
    PointTestPass::Query q;
    q.key = (const void *)(intptr_t)key;
    q.px = std::floor(x + size / 2) + 0.5f;
    q.py = std::floor(y + size / 2) + 0.5f;
    q.rx = x;
    q.ry = y;
    q.rw = size;
    q.rh = size;
    q.z = z;
    q.point = true;
    q.area = true;
    return q;
}

std::map<intptr_t, PointTestPass::Answer> gAnswers;
void Collect(const PointTestPass::Answer &a, void *) { gAnswers[(intptr_t)a.key] = a; }

class PointTestPassTest : public ::testing::Test {
protected:
    void SetUp() override {
        if (!GpuUp())
            GTEST_SKIP() << "headless GPU device unavailable on this host";
        if (RB3RndBackend::Gpu().IsNullBackend())
            GTEST_SKIP() << "null Dawn backend: queries never pass";
        GpuDevice &gpu = RB3RndBackend::Gpu();
        wgpu::TextureDescriptor dd{};
        dd.size = {kW, kH, 1};
        dd.format = wgpu::TextureFormat::Depth24PlusStencil8;
        dd.usage = wgpu::TextureUsage::RenderAttachment;
        dd.sampleCount = 4;
        mDepth = gpu.Device().CreateTexture(&dd);
        mDepthView = mDepth.CreateView();
        gAnswers.clear();
    }
    void TearDown() override { mPass.Terminate(&RB3RndBackend::Gpu()); }

    // Records `qs`, submits, and returns whatever Collect() delivers before
    // (`early`) and after the readback is started.
    void Run(const std::vector<PointTestPass::Query> &qs, int &early, int &late,
             std::string &errors) {
        GpuDevice &gpu = RB3RndBackend::Gpu();
        gpu.Device().PushErrorScope(wgpu::ErrorFilter::Validation);
        FillDepth(gpu, mDepthView);
        wgpu::CommandEncoder enc = gpu.Device().CreateCommandEncoder();
        ASSERT_TRUE(mPass.Record(enc, mDepthView, wgpu::TextureFormat::Depth24PlusStencil8, 4,
                                 kW, kH, qs.data(), qs.size(), gpu));
        early = mPass.Collect(true, Collect, nullptr, gpu);  // recorded, not submitted
        wgpu::CommandBuffer cmd = enc.Finish();
        gpu.Queue().Submit(1, &cmd);
        mPass.Submitted();
        late = mPass.Collect(true, Collect, nullptr, gpu);
        errors = PopErrors(gpu);
    }

    PointTestPass mPass;
    wgpu::Texture mDepth;
    wgpu::TextureView mDepthView;
};

} // namespace

TEST_F(PointTestPassTest, CountsVisiblePixelsAgainstDepth) {
    std::vector<PointTestPass::Query> qs = {
        MakeQuery(1, 80, 16, 16, 0.75f),   // right half, nothing in front
        MakeQuery(2, 16, 16, 16, 0.75f),   // left half, behind the occluder
        MakeQuery(3, 16, 16, 16, 0.25f),   // left half, in front of it
        MakeQuery(4, 56, 16, 16, 0.75f),   // straddles x = 64: right half visible
        MakeQuery(5, 120, 40, 16, 0.75f),  // half off the right edge
        MakeQuery(6, 16, 40, 16, kOccluderZ),  // coplanar: LESS fails, as retail
    };
    qs[3].px = 66.5f;  // the straddling rect's point on its visible side
    qs[4].px = 124.5f;  // the half-off rect's point on screen
    qs[4].areaScale = 0.25f;
    int early = -1, late = -1;
    std::string errors;
    Run(qs, early, late, errors);
    EXPECT_EQ(errors, "");
    EXPECT_EQ(early, 0) << "answers before the queries were submitted";
    ASSERT_EQ(late, 6);

    struct Want {
        bool visible;
        float area;
    } want[] = {{true, 256}, {false, 0}, {true, 256}, {true, 128}, {true, 128}, {false, 0}};
    for (int k = 1; k <= 6; k++) {
        SCOPED_TRACE(k);
        ASSERT_TRUE(gAnswers.count(k));
        const PointTestPass::Answer &a = gAnswers[k];
        EXPECT_TRUE(a.pointDone);
        EXPECT_TRUE(a.areaDone);
        EXPECT_EQ(a.visible, want[k - 1].visible);
        // Samples / 4 for a 4x target; areaScale applied on top (query 5).
        const float area = want[k - 1].area * (k == 5 ? 0.25f : 1.0f);
        EXPECT_FLOAT_EQ(a.area, area);
    }
    EXPECT_EQ(mPass.InFlight(), 0);
}

TEST_F(PointTestPassTest, CancelDropsInFlightAnswers) {
    std::vector<PointTestPass::Query> qs = {MakeQuery(1, 80, 16, 16, 0.75f),
                                            MakeQuery(2, 96, 16, 16, 0.75f)};
    GpuDevice &gpu = RB3RndBackend::Gpu();
    gpu.Device().PushErrorScope(wgpu::ErrorFilter::Validation);
    FillDepth(gpu, mDepthView);
    wgpu::CommandEncoder enc = gpu.Device().CreateCommandEncoder();
    ASSERT_TRUE(mPass.Record(enc, mDepthView, wgpu::TextureFormat::Depth24PlusStencil8, 4, kW,
                             kH, qs.data(), qs.size(), gpu));
    wgpu::CommandBuffer cmd = enc.Finish();
    gpu.Queue().Submit(1, &cmd);
    mPass.Submitted();
    mPass.Cancel((const void *)(intptr_t)1);
    EXPECT_EQ(mPass.Collect(true, Collect, nullptr, gpu), 1);
    EXPECT_EQ(PopErrors(gpu), "");
    EXPECT_FALSE(gAnswers.count(1));
    ASSERT_TRUE(gAnswers.count(2));
    EXPECT_FLOAT_EQ(gAnswers[2].area, 256.0f);
}

// ---------------------------------------------------------------------------
// The real path: Rnd::TestPoint on RndFlares, inside WgpuRnd's frame
// ---------------------------------------------------------------------------

namespace {

// The renderer draws an unnamed mesh as text, without depth, so the wall gets
// a name, which needs a dir. Kept for the process: the engine is a singleton.
ObjectDir *NameDir() {
    static ObjectDir *dir = nullptr;
    if (!dir) {
        dir = new ObjectDir();
        dir->Reserve(16, 256);
    }
    return dir;
}

std::vector<const void *> gDelivered;
NativePointTestResultFn gConsumerFn = nullptr;
// Wraps Rnd::TestPoint's handler to see which flares were answered.
void Spy(const NativePointTestResult &r) {
    gDelivered.push_back(r.key);
    if (gConsumerFn)
        gConsumerFn(r);
}

class RndTestPoint : public ::testing::Test {
protected:
    void SetUp() override {
        EnsureSymbolInit();  // object names, and MakeString for ~RndFlare
        if (!GpuUp())
            GTEST_SKIP() << "headless GPU device unavailable on this host";
        if (RB3RndBackend::Gpu().IsNullBackend())
            GTEST_SKIP() << "null Dawn backend: queries never pass";
        ASSERT_NE(GetNativePointTester(), nullptr) << "WgpuRnd registered no point tester";

        // Camera at the origin looking down +Y (Milo's forward).
        mCam = new RndCam();
        mCam->SetFrustum(1.0f, 100.0f, 0.6024178f, 1.0f);

        // A wall at y = 10 over the left half of the view (x < 0).
        mMat = new RndMat();
        mMat->mCull = 0;
        mWall = new RndMesh();
        mWall->Verts().resize(4, false);
        const float corners[4][2] = {{-50, -50}, {0, -50}, {-50, 50}, {0, 50}};
        for (int i = 0; i < 4; i++) {
            RndMesh::Vert &v = mWall->Verts(i);
            v.pos.Set(corners[i][0], 10.0f, corners[i][1]);
            v.norm.Set(0, -1, 0);
            v.uv.Set(0, 0);
        }
        std::vector<RndMesh::Face> faces(2);
        faces[0].Set(0, 1, 2);
        faces[1].Set(2, 1, 3);
        mWall->Faces() = faces;
        mWall->SetMat(mMat);
        mWall->SetName("point_test_wall", NameDir());

        mBehind = MakeFlare(Vector3(-3, 20, 0));   // left, behind the wall
        mOpen = MakeFlare(Vector3(3, 20, 0));      // right, nothing in front
        mFront = MakeFlare(Vector3(-1, 5, 0));     // left, in front of the wall
        gDelivered.clear();
    }
    void TearDown() override {
        for (RndFlare *f : {mBehind, mOpen, mFront})
            DeleteFlare(f);
        delete mWall;
        delete mMat;
        delete mCam;
    }

    // ~RndFlare calls Rnd::RemovePointTest, which cancels the flare's tests;
    // it insists the platform's async point tests be suspended around it.
    static void DeleteFlare(RndFlare *f) {
        gSuppressPointTest++;
        delete f;
        gSuppressPointTest--;
    }

    RndFlare *MakeFlare(const Vector3 &pos) {
        RndFlare *f = new RndFlare();
        Transform t;
        t.Reset();
        t.v = pos;
        f->SetLocalXfm(t);
        f->unkec = -1.0f;  // no answer yet
        return f;
    }

    // One frame: the wall, then Rnd::TestPoint for each live flare with an
    // 8x8 rect centred on it (what RndFlare::CalcRect leaves in mArea), then
    // the world end (or not: EndDrawing ends it then).
    void Frame(bool endWorld, bool test = true) {
        RB3RndBackend::BeginFrame(mCam);
        ASSERT_TRUE(RB3RndBackend::InPass());
        RB3RndBackend::DrawMesh(mWall);
        for (RndFlare *f : {mBehind, mOpen, mFront}) {
            if (!f || !test)
                continue;
            Vector2 s;
            mCam->WorldToScreen(f->WorldXfm().v, s);
            f->mArea = Hmx::Rect(std::floor(s.x * TheRnd->mWidth) - 4.0f,
                                 std::floor(s.y * TheRnd->mHeight) - 4.0f, 8.0f, 8.0f);
            TheRnd->TestPoint(f->WorldXfm().v, f);
        }
        if (GetNativePointTestResultFn() != Spy) {
            gConsumerFn = GetNativePointTestResultFn();
            SetNativePointTestResultFn(Spy);
        }
        if (endWorld) {
            // Rnd::EndWorld's world-end step. The world camera counts as copied
            // already: its copy target is made by Rnd::Init, which this
            // in-process frame does not run.
            TheRnd->unkef = true;
            TheRnd->DoWorldEnd();
        }
        mAtWorldEnd = gDelivered.size();
        RB3RndBackend::EndFrame();
    }

    RndCam *mCam = nullptr;
    RndMat *mMat = nullptr;
    RndMesh *mWall = nullptr;
    RndFlare *mBehind = nullptr, *mOpen = nullptr, *mFront = nullptr;
    size_t mAtWorldEnd = 0;  // answers delivered by the time the world ended
};

} // namespace

TEST_F(RndTestPoint, FlareBehindGeometryReadsZero) {
    GpuDevice &gpu = RB3RndBackend::Gpu();
    for (bool endWorld : {true, false}) {
        SCOPED_TRACE(endWorld ? "answered at DoWorldEnd" : "answered at EndDrawing");
        Frame(endWorld, /*test=*/false);  // drain answers to earlier frames' tests
        for (RndFlare *f : {mBehind, mOpen, mFront})
            f->unkec = -1.0f;
        gDelivered.clear();
        gpu.Device().PushErrorScope(wgpu::ErrorFilter::Validation);
        Frame(endWorld);
        // Retail answers a frame late: nothing has been read back yet.
        EXPECT_EQ(mOpen->unkec, -1.0f);
        Frame(endWorld);
        EXPECT_EQ(PopErrors(gpu), "");
        EXPECT_EQ(gDelivered.size(), 3u);
        // Retail delivers them at world end (DxRnd::DoWorldEnd -> DoPointTests).
        EXPECT_EQ(mAtWorldEnd, endWorld ? 3u : 0u);

        EXPECT_FALSE(mBehind->mVisible);
        EXPECT_EQ(mBehind->unkec, 0.0f) << "a flare behind the wall must read no pixels";
        EXPECT_TRUE(mOpen->mVisible);
        EXPECT_NEAR(mOpen->unkec, 64.0f, 1.0f) << "an unoccluded 8x8 flare reads its area";
        EXPECT_TRUE(mFront->mVisible);
        EXPECT_NEAR(mFront->unkec, 64.0f, 1.0f);
    }
}

TEST_F(RndTestPoint, RemovedFlareIsNeverAnswered) {
    Frame(true);  // queue tests for all three
    const void *gone = mOpen;
    DeleteFlare(mOpen);  // ~RndFlare -> RemovePointTest: its in-flight tests are cancelled
    mOpen = nullptr;
    gDelivered.clear();
    Frame(true);  // reads back the first frame's queries
    EXPECT_EQ(gDelivered.size(), 2u);
    for (const void *k : gDelivered)
        EXPECT_NE(k, gone);
}
