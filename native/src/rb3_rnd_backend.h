// rb3_rnd_backend.h — the harness-facing seam over the engine GPU backend.
//
// RB3's native harnesses (the --viewer, RB3_RENDER_MESH, RB3_GAME boot, the web
// boot machine, the HTTP debug server) drive one renderer. The engine offers two
// (RB3_GPU_BACKEND in native/CMakeLists.txt):
//
//   rb3  BandRnd : Rnd   (milo-native-engine src/platform/Rnd_Wgpu_RB3.cpp)
//   dc3  WgpuRnd         (src/platform/Rnd_Wgpu.cpp, shared with rb3-xenon and
//                         dc3-decomp; reads RB3-Wii rndobj via src/platform/rndshape)
//
// Harness code calls RB3RndBackend:: and never names gBandRnd / gWgpuRnd, so
// every harness builds under either flavor. Exactly one of
// rb3_rnd_backend_rb3.cpp / rb3_rnd_backend_dc3.cpp is linked.
#pragma once

#include "gfx/GpuDevice.h"  // Gpu() callers read the device directly

class RndCam;
class RndMesh;
class ObjectDir;
namespace Hmx {
    class Color;
}

namespace RB3RndBackend {

// "rb3" or "dc3".
const char *FlavorName();

// Register the rndobj factories (+ the legacy Tex/Text/Dir short names) the way
// the harnesses need them: no GPU, no overlay/console. Idempotent.
void PreInitRender();

// Bring up the GpuDevice + the backend's GPU resources, synchronously.
bool InitGpu(int width, int height, bool headless);
// The web boot's split form: start the (async) device request, then, once
// Gpu().IsReady(), create the GPU resources. InitGpu == both back to back.
bool StartGpuInit(int width, int height, bool headless);
void InitGpuResources();
bool GpuReady();
GpuDevice &Gpu();

void SetClearColor(const Hmx::Color &c);
// Arm the MILO_SCREENSHOT_DIR / _FRAMES auto-capture, where the flavor has one.
void InitScreenshots();

// One frame: open the frame pass under `cam` (may be null), draw, close it.
void BeginFrame(RndCam *cam);
// Draw a mesh whatever its showing flag says (hidden template geometry).
void DrawMesh(RndMesh *mesh);
void EndFrame();

bool InPass();
int FrameCount();

// Upload up to budgetMs of a dir's textures/meshes ahead of first draw. Returns
// the number of objects uploaded; a flavor without a warm sweep returns 0.
int WarmGpuForDir(ObjectDir *root, float budgetMs);

} // namespace RB3RndBackend
