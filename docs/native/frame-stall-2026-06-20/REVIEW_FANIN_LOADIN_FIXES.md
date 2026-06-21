# Adversarial fan-in review — load-in frame-stall fixes (Tracks A/B/C) — 2026-06-21

Independent reviewer (not the implementer). Reproduced each track's before/after
measurement from scratch (or, for the GPU track, re-analyzed the raw trace
artifacts) and stress-tested the two failure modes the brief named: **stall
removed vs merely RELOCATED**, and **match-neutrality / correctness regression**.

**Overall verdict: CONFIRM all three.** Each fix genuinely reduces its stall, is
match-neutral (Wii image byte-identical), and preserves correctness. Independently
reproduced, not taken on the implementers' word.

---

## Track A — skinned-mesh collection cache (`wt-fstall2-skincache` @ `9a53ae49`) — CONFIRM

**Change:** `src/system/bandobj/BandCharacter.{cpp,h}`, all `#ifdef HX_NATIVE`.
`NativeCollectSkinnedMeshes` re-walked each band member's ObjectDir (RTTI
`dynamic_cast` per entry) + the whole draw tree + O(N²) `std::find` dedup **every
Poll for ~10 s** of song-start. Now: walk once into a per-member cache
(`mNativeSkinnedMeshCache`), O(1) `std::set` dedup, invalidate only at
StartLoad/SyncObjects (the dir-restuff points that already re-arm the rebind latches).

**Reproduced (native, `char-burst-capture.py`, A/B in one binary via `RB3_SKIN_NOCACHE`):**
| | full RTTI walks | Σ walk time |
|---|---|---|
| nocache (pre-change) | 11,850+ | 3259+ ms |
| cached (default) | 50 | 16.8 ms |

Matches the doc (≈253× fewer walks, −99.5% walk time). Frame-burst saving follows.

**Removed, not relocated:** the walk is *eliminated*, not deferred — it runs once
per (re)load. The 50 residual rebuilds == the legitimate StartLoad/SyncObjects
reload churn. No later frame inherits the cost.

**Correctness (the skin-explosion / rebind-miss risk):** verified the band still
rebinds identically. Both cached and nocache reached the same final latch pattern
(one member actively rebinding 17–23 meshes / 187–201 bones, the other three
latched with `pending=1–5` = the known permanently-unresolvable residual mesh).
**Zero** SHARD_GUARD / skin-explosion fires in either run. Pending residual
distribution equivalent (cached: pending=1 ×5, pending=5 ×1; nocache: pending=1
×4, pending=2 ×1, pending=6 ×1 — run variance, not a systematic cache loss).
Screenshots: venue close-up of the band is coherent (solid guitarist/drummer/
vocalist, no shards/fling); gameplay highway+gems render correctly.

The one theoretical hole — a mesh that streams in mid-merge **without** a
StartLoad/SyncObjects (the original per-Poll walk was partly there to catch this,
per the `RebindHeadHandsAtRest` 30-quiet-latch comment) — did **not** manifest:
target counts stay in the documented 16–23 band and no member latched with an
inflated residual. The invalidation hooks sit exactly at the dir-restuff points,
and the empirical A/B converges to the same end-state. Accept.

**Match-neutral:** all edits `#ifdef HX_NATIVE`; the two new `.h` members are
appended inside the HX_NATIVE block *after* the matched layout (verified) → Wii
struct layout byte-identical. Engine pin not bumped (rb3 shared-decomp file).

---

## Track B — LoadMgr::Poll per-frame budget overspill (`wt-fstall2-postload` @ `cbc249a2`) — CONFIRM

**Change:** `src/system/utl/Loader.cpp` only, both edits inside the `#ifdef HX_WEB`
and `#ifdef HX_NATIVE` arms of `LoadMgr::Poll`. The Wii `#else` path (plain
`while(!mLoading.empty()) PollFrontLoader();`, no budget logic at all) is untouched.
Each `PollFrontLoader` pass was armed with a **fresh full `sBudgetMs`** slice and
the cumulative break was only checked *after* the pass → a single object loop could
drain a whole 8 ms on top of work already done → `lp` overshot to ~2× budget. Fix:
arm each pass with `sBudgetMs − spentThisFrame`, floored at 1.5 ms for forward
progress; `drainToEmpty` (PollUntilEmpty synchronous contract) keeps the full slice.

**Reproduced (native, `frame_profiler.py --into-song`, true in-binary A/B — I
added a temporary `RB3_TRACKB_OFF` gate, measured both, then hand-reverted to the
exact committed source; worktree confirmed clean):**
| `lp` (bg loader, busy frames) | p90 | p99 | max | >12 ms | >16 ms |
|---|---|---|---|---|---|
| baseline (fresh full slice) | 14.5 | 19.8 | **19.8** | 9 | **2** |
| fix (remaining budget) | 10.0 | 14.0 | **14.0** | 1 | **0** |

Matches the doc (19.5 → ~14.6, >16 ms 1→0).

**Removed, not relocated — the key refutation:** same binary, same nav, game_screen window:
| | total load-work (lp+lpu+objMs) | settle wall | first-150-frame Σdt |
|---|---|---|---|
| baseline | 409 ms | 597 ms | 2318 ms |
| fix | 417 ms | 610 ms | 2082 ms |

Total load-work and settle-wall are unchanged (within noise); the burst is if
anything slightly *faster*. The same objects load over the same total time, packed
to ~8 ms/frame instead of spiking to 14–20 ms. This directly refutes "loader-spread
just moved the cost to later budget-blowing frames" — there is no stretch and no
later spike. Peak reduced, duration unchanged.

**Honest scope (not a deficiency):** the worst single song-start frame (~94 ms) is
`lpu`-dominated (the venue normal-map sync texture drain), which Track B explicitly
does NOT touch — that's the texprewarm lane. Track B's own claim (cap the `lp`
overspill) is fully met.

**Match-neutral:** Wii `#else` path byte-identical (the budget construct only ever
existed in the native/web arms). Engine pin not bumped.

---

## Track C — chunk pipeline pre-warm to kill the 550 ms GPUTask (engine `wt-fstall2-gputask` @ `7ee6850`, rb3 harness `eef0da1`) — CONFIRM

**Change:** engine `src/gfx/PipelineManager.{cpp,h}` + `src/platform/Rnd_Wgpu_RB3.cpp`
— native/web WebGPU backend only (no Wii match surface). `BandRnd::BeginFrame` ran
`PipelineManager::PreWarm()` one-shot, creating all 240 enumerated pipelines in one
Dawn wire flush → on web, ONE ~550 ms GPU-process compile. Fix: `PreWarmStep()`
creates a bounded **count** (default 12, `RB3_PIPELINE_PREWARM_PER_FRAME`) per
frame from a persistent cursor; `BuildPreWarmKeys()` enumerates the identical
240-key superset shared by both the synchronous `PreWarm` and the chunked path.
Latches `mPipelinesPrewarmed` only when the cursor reports 0 remaining.

**Attribution is evidence-backed (re-analyzed the raw `gputrace.json`, not the
in-script summary):** the single 550.6 ms GPUTask window contains **exactly n=240
`APICreateRenderPipeline` slices** (543.6 ms) + 323.7 ms `ShaderModuleVk::
GetHandleAndSpirv` — precisely `PreWarm`'s 3×8×5×2 = 240-key sweep. Next-largest
GPUTask in the whole session is 18 ms; the 550 ms is a clean outlier. Attribution
confirmed.

**A/B reproduced from the raw traces (web debug, real Vulkan):**
| variant | n GPUTasks | top GPUTasks (ms) | >100 ms |
|---|---|---|---|
| baseline (whole-session) | 1673 | **550.6**, 18.2, 15.0… | 1 |
| one-shot (`NOCHUNK=1`) | 3125 | **464.9**, 26.2… | 1 |
| **chunked (default)** | 1486 | **79.9**, 59.8, 49.9… | **0** |

**Removed, not relocated — verified by counting creates per flush:** both chunked
and one-shot create the **same 252** `APICreateRenderPipeline` total (240 prewarm +
~12 on-demand) → the chunked path warms the *identical* pipeline set, not fewer.
- one-shot: 1 flush of **240** + trivial rest.
- chunked: **25 flushes**, max **14**, mostly exactly **12**/flush.

So the 550 ms burst is genuinely split into ~12-at-a-time chunks, not relocated
whole. The chunked creates span the boot→splash→menu→song-select dwell (first→last
over ~10 s wall; the largest 79.9 ms task lands at GPU-session start = boot
first-render, NOT gameplay), where there is frame slack and 79.9 ms < the
~90–140 ms audio ring depth → no under-run. The doc honestly flags this ~40–80 ms
residual.

**Correctness / visual no-op:** game_screen renders correctly with the fix (band
+ venue coherent; a missing/unwarmed pipeline would show as black/missing geometry
— none). Same pipeline set, only *when* created. Engine compiles + links clean into
`rb3-native` (rebuilt here). `CachedPipelineCount()` already existed in the parent
(no build break).

**Match-neutral:** WebGPU-backend-only engine files; Wii match surface untouched.
Pin NOT bumped (per workflow rules).

---

## Reviewer notes / residuals (none blocking)

- Track A: cache-miss-on-mid-merge-without-SyncObjects is a *theoretical* hole that
  did not manifest in the A/B; if a future asset path streams skinned meshes outside
  StartLoad/SyncObjects it would need an extra invalidation hook. Low risk today.
- Track B: leaves the `lpu` sync-texture-drain worst frame untouched **by design**
  (texprewarm lane). Not a Track B regression.
- Track C: residual ~40–80 ms per-flush GPUTask is under the ring depth and lands
  in non-gameplay dwell; driving it lower means prewarming only the observed common
  key subset (deferred, riskier).

All three are independent, additive, and land in different layers (char Poll / bg
loader budget / GPU pipeline compile). Recommend all three for landing.
