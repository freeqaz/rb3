// rb3_rnd_backend_rb3.cpp — RB3RndBackend over the `rb3` flavor (BandRnd).
// Forwarders: every harness behaves as it did when it called gBandRnd
// directly, plus an exit-time GPU teardown registered by InitGpu.
#include "rb3_rnd_backend.h"

#include "platform/Rnd_Wgpu_RB3.h"

#include <cstdlib>

namespace {
void BandRndAtExit() { gBandRnd.Shutdown(); }
} // namespace

namespace RB3RndBackend {

const char *FlavorName() { return "rb3"; }
void PreInitRender() { gBandRnd.PreInitRender(); }
bool InitGpu(int width, int height, bool headless) {
    bool ok = gBandRnd.InitGpu(width, height, headless);
    // A process that never reaches Debug::Exit (the gtest binary returns from
    // main) would otherwise release BandRnd's handles from gBandRnd's static
    // destructor, after Dawn's Vulkan backend is gone, and segfault at exit.
    // atexit handlers registered now run before that destructor; Shutdown is a
    // no-op once it has run (it latches mGpuReady off).
    static bool sAtExit = false;
    if (ok && !sAtExit) {
        sAtExit = true;
        std::atexit(BandRndAtExit);
    }
    return ok;
}
bool StartGpuInit(int width, int height, bool headless) {
    return gBandRnd.StartGpuInit(width, height, headless);
}
void InitGpuResources() { gBandRnd.InitGpuResources(); }
bool GpuReady() { return gBandRnd.mGpuReady; }
GpuDevice &Gpu() { return gBandRnd.Gpu(); }
void SetClearColor(const Hmx::Color &c) { gBandRnd.SetClearColor(c); }
void InitScreenshots() { gBandRnd.InitScreenshots(); }
void BeginFrame(RndCam *cam) { gBandRnd.BeginFrame(cam); }
void DrawMesh(RndMesh *mesh) { gBandRnd.DrawMesh(mesh); }
void EndFrame() { gBandRnd.EndFrame(); }
bool InPass() { return gBandRnd.InPass(); }
int FrameCount() { return gBandRnd.mFrameCount; }
int WarmGpuForDir(ObjectDir *root, float budgetMs) {
    return gBandRnd.WarmGpuForDir(root, budgetMs);
}

} // namespace RB3RndBackend
