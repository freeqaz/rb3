// rb3_rnd_backend_rb3.cpp — RB3RndBackend over the `rb3` flavor (BandRnd).
// Pure forwarders: every harness behaves exactly as it did when it called
// gBandRnd directly.
#include "rb3_rnd_backend.h"

#include "platform/Rnd_Wgpu_RB3.h"

namespace RB3RndBackend {

const char *FlavorName() { return "rb3"; }
void PreInitRender() { gBandRnd.PreInitRender(); }
bool InitGpu(int width, int height, bool headless) {
    return gBandRnd.InitGpu(width, height, headless);
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
