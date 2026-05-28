# rb3-native GPU Capture Workflow

**Date:** 2026-05-28
**Status:** Ready. GFXReconstruct layer + tools are built at `../gpu/gfxreconstruct/build/`.

Scripts: `scripts/gpu/capture.sh`, `scripts/gpu/inspect.sh`

---

## Overview

rb3-native renders via Dawn → Vulkan. GFXReconstruct captures Vulkan API calls at
the layer level — app-agnostic, works headless, no engine changes required. A capture
run produces a `.gfxr` binary trace that can be converted to JSON Lines for grepping,
replayed with per-frame screenshots, or analyzed for pipeline/shader/descriptor state.

This is the primary tool for the draw/shader/pipeline bug class:
- "mesh invisible" — verify the draw happened, check pipeline culling + depth state
- "colors wrong" — dump render targets at specific draw calls, check bound textures
- "skinned mesh broken" — inspect vertex buffer layout, bone palette descriptor bindings
- "what draws happen in a frame?" — convert to JSON, grep `vkCmdDraw`

---

## Quick start

```bash
# Short headless capture (submit-trimmed, ~30s, first gameplay window)
cd /home/free/code/milohax/rb3
scripts/gpu/capture.sh -s 100-300 -t 20

# Inspect the capture
CAPFILE=$(ls -t /tmp/gpu_captures/rb3_capture*.gfxr | head -1)
scripts/gpu/inspect.sh summary "$CAPFILE"
scripts/gpu/inspect.sh draws "$CAPFILE"
```

---

## capture.sh reference

```
scripts/gpu/capture.sh [options]
```

| Option | Description | Default |
|--------|-------------|---------|
| `-o <path>` | Output `.gfxr` file | `/tmp/gpu_captures/rb3_capture.gfxr` |
| `-s <range>` | Queue submit range, headless (e.g. `100-300`) | all |
| `-f <range>` | Frame range (e.g. `3200-3250`) — needs display | all |
| `-q` | Quit after captured frames (requires `-f`) | off |
| `-t <seconds>` | Kill rb3-native after N seconds | 35 |
| `-n <frames>` | `MILO_MAX_FRAMES` for rb3-native | 6000 |
| `-g <input>` | Override `RB3_GAME_INPUT` string | V12 guitar reproducer |
| `-x` | Use virtual display (auto-enabled when `-f` + no `$DISPLAY`) | off |
| `-c <type>` | Compression: LZ4, ZSTD, ZLIB, NONE | ZSTD |
| `-l <level>` | Log level: debug, info, warning, error | warning |

The script sets `RB3_GAME=1 MILO_HEADLESS=1 MILO_AUDIO=1` automatically.
`RB3_DATA` defaults to `orig-assets/extracted` (relative to the repo root).
Pass extra env vars before the script: `GEM_DBG=1 scripts/gpu/capture.sh ...`

### Submit-range vs frame-range

rb3-native is headless — Dawn does not present to a swapchain, so GFXReconstruct
has no frame boundary signal. Use `-s` (queue submit trimming) instead of `-f`:

- Each `vkQueueSubmit` is one submit unit. A gameplay frame typically generates
  several submits (shadow pass, main draw, post-process, blit).
- For the gem/smasher window at frames 3200+, roughly submits 80–500 cover the
  early gameplay window. Start with `-s 100-300` for a first capture; widen as needed.
- `-f` (frame counting) requires a swapchain → use `-x` (xvfb virtual display) to
  give rb3-native a window so Dawn creates a swapchain. Slower and heavier than `-s`.

### Sizing

With ZSTD compression, expect ~8 MB/s of capture data. A `-s 100-300` trim
produces a small (<50 MB) capture. A full `-t 35` unconstrained run can be 250 MB+.

---

## inspect.sh reference

```
scripts/gpu/inspect.sh <command> <capture.gfxr> [options]
```

| Command | What it does |
|---------|-------------|
| `info` | GPU info, pipeline counts, memory stats |
| `summary` | Count each Vulkan API call by frequency |
| `calls <pattern>` | Filter calls by name (e.g. `vkCmdDraw`) |
| `draws` | All draw calls with bound pipeline + render area |
| `pipelines` | Every `vkCreateGraphicsPipelines`: topology, cull, depth, blend, vertex layout |
| `extract [-d dir]` | Extract SPIR-V shader binaries |
| `shaders` | Extract + disassemble all shaders (requires `spirv-tools`) |
| `convert [-o out]` | Export full trace to JSON Lines |
| `query <file> [opts]` | Stream-query the JSON trace (see `query_trace.py --help`) |
| `labels [-g pat]` | Vulkan debug labels / object names |

The `draws`, `pipelines`, `labels`, and `query` commands auto-convert the `.gfxr`
to a `.jsonl` cache on first use (slow, a few seconds). Subsequent calls are fast.

---

## Tool paths

```bash
GPU_DIR=/home/free/code/milohax/gpu

# GFXReconstruct binaries (already built)
$GPU_DIR/gfxreconstruct/build/layer/libVkLayer_gfxreconstruct.so   # capture layer
$GPU_DIR/gfxreconstruct/build/tools/info/gfxrecon-info
$GPU_DIR/gfxreconstruct/build/tools/convert/gfxrecon-convert
$GPU_DIR/gfxreconstruct/build/tools/extract/gfxrecon-extract
$GPU_DIR/gfxreconstruct/build/tools/replay/gfxrecon-replay

# Optional aliases
alias gfxrecon-info="$GPU_DIR/gfxreconstruct/build/tools/info/gfxrecon-info"
alias gfxrecon-convert="$GPU_DIR/gfxreconstruct/build/tools/convert/gfxrecon-convert"
alias gfxrecon-replay="$GPU_DIR/gfxreconstruct/build/tools/replay/gfxrecon-replay"
```

---

## One-time build (if the layer is missing)

The `../gpu/gfxreconstruct/` build is already present as of 2026-05-28. If it ever
needs rebuilding (clean checkout, new machine):

```bash
cd /home/free/code/milohax/gpu/gfxreconstruct
git submodule update --init
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DGFXRECON_ENABLE_OPENXR=OFF
cmake --build build -j$(nproc)   # ~10 minutes
```

---

## Debugging scenarios

### "Why is this mesh invisible?" (e.g. smasher not drawing)

```bash
# 1. Capture the gameplay window
scripts/gpu/capture.sh -s 100-400 -t 25
CAPFILE=/tmp/gpu_captures/rb3_capture.gfxr

# 2. Verify the draw happened
scripts/gpu/inspect.sh draws "$CAPFILE"
# Look for the smasher draw — if absent, the geometry never reached DrawMesh.

# 3. Check pipeline state (culling, depth test, blend)
scripts/gpu/inspect.sh pipelines "$CAPFILE"

# 4. Extract vertex/fragment shaders
scripts/gpu/inspect.sh extract "$CAPFILE" -d /tmp/rb3_shaders
spirv-dis /tmp/rb3_shaders/sh<id>   # requires spirv-tools: pacman -S spirv-tools

# 5. Dump resources at a specific draw call index N (from step 2)
cat > /tmp/dump_config.json << 'EOF'
{
  "draw_call_indices": [N],
  "dump_resources_before": false,
  "dump_depth": true,
  "dump_vertex_index_buffers": true
}
EOF
GPU=/home/free/code/milohax/gpu
$GPU/gfxreconstruct/build/tools/replay/gfxrecon-replay \
  --dump-resources /tmp/dump_config.json \
  --dump-resources-dir /tmp/rb3_dump \
  "$CAPFILE"
```

### "What Vulkan calls does a gameplay frame make?"

```bash
scripts/gpu/inspect.sh summary /tmp/gpu_captures/rb3_capture.gfxr
```

### "Are the bone palette descriptors bound?" (skinned mesh bug)

```bash
# Convert to JSON and search for descriptor set binding before the smasher draw
CAPFILE=/tmp/gpu_captures/rb3_capture.gfxr
scripts/gpu/inspect.sh convert "$CAPFILE" -o /tmp/rb3_trace.jsonl

# Find bind-descriptor calls around the smasher draw index
python3 /home/free/code/milohax/dc3-decomp/scripts/gpu/query_trace.py \
  /tmp/rb3_trace.jsonl --call BindDescriptorSets --compact --limit 20

# Check vertex buffer binding layout for the skinned mesh draw
python3 /home/free/code/milohax/dc3-decomp/scripts/gpu/query_trace.py \
  /tmp/rb3_trace.jsonl --call CmdBindVertexBuffers --compact
```

### Replay with per-frame screenshots

```bash
GPU=/home/free/code/milohax/gpu
mkdir -p /tmp/rb3_replay_frames
$GPU/gfxreconstruct/build/tools/replay/gfxrecon-replay \
  --screenshots 1-10 \
  --screenshot-dir /tmp/rb3_replay_frames \
  --screenshot-format png \
  /tmp/gpu_captures/rb3_capture.gfxr
```

---

## Specific use case: skinned-mesh smasher bug

**Current status (post-V12):** `gem_smasher_*` and `gem_mash0..5` are skinned meshes.
The native `DrawMesh` path binds an identity bone palette, so the smasher/fret plate
deforms to a degenerate position instead of the correct bottom-of-highway pose. Only a
small fallback glyph appears bottom-right. Documented in
`docs/sessions/native/V8_GEM_STREAM_BLOCKERS.md` (§ "Strike line / smasher is a skinned
mesh that doesn't render").

**GPU-capture diagnostic workflow:**

1. Capture the gameplay window where the smasher should be visible:
   ```bash
   cd /home/free/code/milohax/rb3
   scripts/gpu/capture.sh -s 100-400 -t 25 -o /tmp/smasher_bug.gfxr
   ```

2. Find the smasher draw calls and their indices:
   ```bash
   scripts/gpu/inspect.sh draws /tmp/smasher_bug.gfxr
   # Look for draws with small vertex counts (degenerate smasher = 0 or few verts rendered)
   ```

3. Check pipeline vertex input layout (are bone indices/weights in the vertex attributes?):
   ```bash
   scripts/gpu/inspect.sh pipelines /tmp/smasher_bug.gfxr
   # Look for the pipeline used at the smasher draw: vertex binding stride, attribute formats
   ```

4. Dump the vertex buffer at the smasher draw (confirm bone palette is identity):
   ```bash
   # Edit draw_call_indices to the smasher draw index from step 2
   cat > /tmp/smasher_dump.json << 'EOF'
   {"draw_call_indices": [SMASHER_DRAW_INDEX], "dump_vertex_index_buffers": true, "dump_depth": true}
   EOF
   GPU=/home/free/code/milohax/gpu
   $GPU/gfxreconstruct/build/tools/replay/gfxrecon-replay \
     --dump-resources /tmp/smasher_dump.json \
     --dump-resources-dir /tmp/smasher_dump_out \
     /tmp/smasher_bug.gfxr
   ls /tmp/smasher_dump_out/
   ```

5. Extract the vertex shader to inspect how bone indices/weights are used:
   ```bash
   scripts/gpu/inspect.sh extract /tmp/smasher_bug.gfxr -d /tmp/smasher_shaders
   spirv-dis /tmp/smasher_shaders/sh<id>   # look for OpAccessChain into bone array
   ```

This confirms whether the bug is (a) the bone palette descriptor binding identity
matrices, (b) the vertex shader not sampling the palette, or (c) the vertex data
itself lacking bone index/weight attributes.

---

## Notes on DC3 script reuse

`inspect.sh` reuses DC3's `scripts/gpu/query_trace.py` for the JSON querying tier
(it is entirely app-agnostic — it parses any `gfxrecon-convert` output). The GPU
tools themselves (`gfxreconstruct/build/`) are shared at `../gpu/` across both repos.
