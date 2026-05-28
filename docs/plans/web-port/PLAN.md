# RB3 Web Port — Browser via WebAssembly + WebGPU

**Status:** planning. No code yet.

This directory holds the RB3 web-port plan, split into per-phase docs so each
can be delegated to a focused subagent. The plan mirrors DC3's web port
(`dc3-decomp/docs/plans/web-port/`, Phases 0–7b complete) and lands Phase 6 of
the native roadmap.

## Goal

A browser tab that boots RB3, renders RB3 geometry, and ultimately plays one
song end-to-end via WASM + WebGPU. v1 acceptance is one song end-to-end —
same v1 bar as the native build (see
[`rb3/docs/sessions/native/V1_ONE_SONG.md`](../../sessions/native/V1_ONE_SONG.md)).

## Non-goals (v1)

- Online multiplayer (mirrors the native non-goal — `src/network/` is out of
  scope).
- Touch / mobile UX. Desktop browsers (Chrome 137+ for JSPI) only.
- Match DC3's web port byte-for-byte. Shared web infra is lifted into the
  engine; RB3-specific glue is its own code.

## Architectural anchors

These cut across every phase and must be respected:

| Anchor | What it means for the web port |
|---|---|
| **RB3 uses GFX core only.** `MILO_ENGINE_BUILD_GFX=ON`, `MILO_ENGINE_BUILD_GPU_BACKENDS=OFF` (`rb3/native/CMakeLists.txt:182-183`). | The web target keeps this. DC3's `WgpuRnd : NgRnd` and the rndobj-coupled gfx tier do **not** compile against RB3's older 2010-era rndobj. RB3 web uses its own `BandRnd : Rnd` (`native/src/rb3_band_rnd.cpp:47`) on top of the engine's `GpuDevice`, exactly as native does. |
| **DC3 was the model, but isn't a drop-in source set.** DC3 web lives in `dc3-decomp/native/`, not the engine. | Phase W0 lifts the engine-portable bits into `milo-native-engine` under `MILO_BUILD_WEB`. Both decomps then consume the shared layer. |
| **rb3-native is currently headless** (PNG output). | The web target adds a windowed/canvas path. W1 reuses the clear-frame smoke; W2 wires `BandRnd` to present per-frame. |
| **MOGG decryption already wasm-clean.** `aes.c/crypt.c/ctr.c` are pure C, no SDK deps (`rb3/native/CMakeLists.txt:430-432`). | Roll into the web target source set when audio comes online in W3. |
| **`HX_WEB` and `__EMSCRIPTEN__` are the gates.** Match DC3's convention. | Use `HX_WEB=1` as the project define for `#ifdef HX_WEB`-style RB3 gating; reserve `__EMSCRIPTEN__` for compiler-set checks. |

## Phases

| Phase | Doc | Acceptance | Depends on |
|---|---|---|---|
| **W0** | [`W0_ENGINE_EXTRACTION.md`](W0_ENGINE_EXTRACTION.md) | DC3's `dc3-web` build is functionally unchanged after web infra is lifted into the engine under `MILO_BUILD_WEB`. | — |
| **W1** | [`W1_CLEAR_FRAME.md`](W1_CLEAR_FRAME.md) | `rb3-web` builds, boots, runs `SystemPreInit`/`SystemInit` (config DTAs), clears canvas, reaches `BOOT_RUNNING`, ticks ≥5 frames; Playwright screenshot proves non-default canvas. | W0 |
| **W2** | [`W2_RENDER_MILO.md`](W2_RENDER_MILO.md) | `tracksystem.milo_xbox`, gem-smasher, and a UI panel render in-browser visibly matching the native `RB3_RENDER_MESH` PNGs. | W1 |
| **W3** | [`W3_BOOT_TO_SONG.md`](W3_BOOT_TO_SONG.md) | One song plays end-to-end (audio + gem track + scoring) in the browser. Same v1 bar as native. | W2 + native v1 |
| **W4** | [`W4_POLISH.md`](W4_POLISH.md) | WASM size <15MB compressed; loading screen; IndexedDB cache; memory tuning. | W3 |

## Delegation model

Each phase doc is self-contained: it links upstream context, lists the exact
files to touch, captures the acceptance test, and calls out the DC3 file to
mirror for each line item. A subagent dispatched on a phase doc should not
need to read the others.

Where a phase touches both repos (DC3 + RB3, e.g. W0), the doc lists each
repo's task list separately.

## Reference

- DC3 web port plan: [`dc3-decomp/docs/plans/web-port/PLAN.md`](../../../../dc3-decomp/docs/plans/web-port/PLAN.md)
- DC3 App/web unification: [`dc3-decomp/docs/plans/web-port/UNIFY_WITH_APP.md`](../../../../dc3-decomp/docs/plans/web-port/UNIFY_WITH_APP.md)
- DC3 audio plan: [`dc3-decomp/docs/plans/web-port/AUDIO.md`](../../../../dc3-decomp/docs/plans/web-port/AUDIO.md)
- RB3 native roadmap: [`rb3/docs/native/NATIVE_PORT_ROADMAP.md`](../../native/NATIVE_PORT_ROADMAP.md)
- RB3 native inventory: [`rb3/docs/native/NATIVE_PORT_INVENTORY.md`](../../native/NATIVE_PORT_INVENTORY.md)
- Engine `MILO_BUILD_WEB` option (placeholder today): `milo-native-engine/CMakeLists.txt:71`
- RB3 GFX-off rationale (K3): `rb3/docs/native/NATIVE_PORT_ROADMAP.md` Known-issues / K3 entry
