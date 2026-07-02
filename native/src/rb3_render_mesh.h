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

// ---------------------------------------------------------------------------
// Reusable scene-bounds + framing-camera helpers (used by rb3_viewer.cpp so it
// can drive the camera from az/el/distance CLI params without duplicating the
// robust median/percentile bounds walk). RB3_RENDER_MESH keeps its own
// SynthesizeCamera path unchanged; these are additive exports.
// ---------------------------------------------------------------------------
struct SceneBounds {
    float cx = 0, cy = 0, cz = 0;   // robust (median) center
    float radius = 1.0f;            // 90th-percentile radius
    bool valid = false;
    int totalObjects = 0;
    int meshCount = 0;              // meshes with drawable geometry
    int camCount = 0;
    RndCam* firstCam = nullptr;     // first scene camera found (may be null)
};

// Walk `dir` (recursive; includes AppendSubDir'd subdirs) and compute robust
// scene bounds + object/mesh/cam counts.
SceneBounds ViewerComputeBounds(ObjectDir* dir);

// Build a framing RndCam looking at the given bounds. `dir3` (nullable) is a
// view direction in Milo axes (X=right, Y=forward/depth, Z=up); when null a
// default 3/4 diagonal is used. `distanceOverride` <= 0 means auto-frame from
// the bounds radius. The returned cam is Select()'d by the caller.
RndCam* ViewerMakeCamera(const SceneBounds& b, const float* dir3, float distanceOverride);
