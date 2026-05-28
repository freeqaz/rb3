// rb3_render_mesh.h — reusable milo-load + walk + render pieces shared between
// the native PNG harness (RB3_RENDER_MESH=1, main_native.cpp) and the web boot
// machine (main_web.cpp). The native side keeps the PNG readback + _exit; the
// web side reuses LoadMiloAndWalk + RenderFrame to draw to the canvas per frame.
#pragma once

class ObjectDir;
class RndCam;

// Result of loading a milo and walking its object graph: the loaded dir, the
// camera to render from (a synthesized framing cam or a scene cam), and counts.
// `ok` is false if the load failed or no drawable mesh was found.
struct WalkResult {
    ObjectDir* dir = nullptr;
    RndCam* cam = nullptr;
    int totalObjects = 0;
    int meshCount = 0;
    int camCount = 0;
    bool ok = false;
};

// Load a milo via DirLoader::LoadObjects, walk it to compute scene bounds, and
// pick/synthesize a camera. Assumes the GpuDevice is up and the rndobj
// factories are registered (BandRnd::PreInitRender / config boot done by the
// caller). `miloPath` may be absolute or relative to the cwd. Does NOT render.
WalkResult LoadMiloAndWalk(const char* miloPath);

// Draw one frame of the loaded scene: BeginFrame(cam) -> walk every drawable
// RndMesh (DrawShowing for showing meshes, DrawMesh for hidden template
// geometry) -> EndFrame. Safe to call every frame. No readback, no _exit.
void RenderFrame(const WalkResult& walk);

// NATIVE-ONLY: render one frame, read it back, write a PNG, and _exit(rc).
// MUST NOT be reached from the web path (the browser loops forever; _exit would
// terminate the page). Returns only on a pre-render error.
int RenderToPng(const WalkResult& walk);
