# Track B — LoadMgr per-frame budget overspill (finding #3) — FIXED

**Date:** 2026-06-20
**Worktree:** rb3 `wt-fstall2-postload`
  (`.claude/worktrees/fstall2-postload`), engine wt `wt-fstall2-postload` (NO engine change — clean).
**File changed:** `src/system/utl/Loader.cpp` (only) — both edits inside `#ifdef HX_WEB`
  and `#ifdef HX_NATIVE`; the Wii `#else` path is untouched → **Wii image byte-identical**.
**Engine pin:** NOT bumped — Track B is entirely in rb3 shared-decomp loader code, guarded
  native/web. No `MILO_ENGINE_PIN` change, no push.

## The finding (recap)

`LoadMgr::Poll()` is the per-frame background loader, called once per `RunOneFrame` from
`SystemPoll`. It is *already* budget-capped (`RB3_LOADER_BUDGET_MS`, default 8 ms). But the
budget was checked the WRONG way: it bounded the *number of `PollFrontLoader` passes*, while
each pass was armed with a **fresh full `sBudgetMs`** slice and the cumulative break was only
tested AFTER the pass returned. So a single `PollFrontLoader` → `DirLoader::LoadObjs` /
`LoadStream` / `CreateObjects` loop could drain a whole 8 ms of objects **on top of** work
already done earlier in the same `Poll()` — the per-object PostLoad overspill.

`CheckSplit()` (the inter-object yield) compares the per-pass `mTimer` against `unk1c`;
`Poll()` set `unk1c = sBudgetMs` every iteration → each pass got the full budget regardless of
time already spent → `lp` overshot to ~2× budget.

## Root-cause mechanism (verified in source)

```
LoadMgr::Poll (HX_NATIVE / HX_WEB arm):
  budgetTimer.Restart()
  while (!mLoading.empty()):
      unk1c = sBudgetMs          <-- BUG: fresh full slice every pass
      mTimer.Restart()
      PollFrontLoader()          <-- LoadObjs drains objects until CheckSplit():
                                     mTimer(this pass) > unk1c (== sBudgetMs)
      ...
      budgetTimer.Split()
      if budgetTimer > sBudgetMs: break   <-- checked only AFTER the pass
```
If `budgetTimer` is at ~6 ms and one pass runs ~8 ms, `lp` reaches ~14 ms before the break.
A single heavy object's PreLoad+PostLoad inside `LoadObjs` is itself uninterruptible
(`CheckSplit` only fires *between* objects), so one fat object (venue mesh, normal-map,
CharClip on web after the JSPI multiplier) adds its whole cost on top.

## The fix

Arm each `PollFrontLoader` with the budget **remaining this frame**, not a fresh full slice:

```c
float spentMs  = drainToEmpty ? 0.0f : Timer::CyclesToMs(budgetTimer.mCycles);
float remainMs = sBudgetMs - spentMs;
const float kSliceFloorMs = 1.5f;            // min forward-progress slice
if (remainMs < kSliceFloorMs) remainMs = kSliceFloorMs;
unk1c = drainToEmpty ? sBudgetMs : remainMs; // CheckSplit now trips on REMAINING budget
```

- Bounds the whole `Poll()` to ~`sBudgetMs` (kills the ≈2× overshoot).
- As the frame fills, each later object-loop's slice shrinks toward the floor, so after a
  heavy object completes the next `CheckSplit` trips immediately → we don't pile a second
  heavy object onto an already-over-budget frame.
- `kSliceFloorMs` (1.5 ms) guarantees a freshly-front loader always gets a non-zero slice, so
  CheckSplit isn't already-tripped on entry — that's the documented front-loader stall
  (`DirLoader::PollLoading` comment: `while(!CheckSplit && ...)` body never runs).
- `drainToEmpty` (the `PollUntilEmpty` unbudgeted-contract path, `mPeriod >= 1e29f`) is left
  on the full slice — it must empty the queue and yields via `sYieldMs` / the frame return,
  not the budget break. Track B deliberately does NOT budget the synchronous-contract drains
  (`PollUntilLoaded` / `PollUntilEmpty`) — that is stall #1's lane (texprewarm).

Applied identically to the HX_WEB and HX_NATIVE arms of `Poll()`.

## Measurement (native, `scripts/native/frame_profiler.py --into-song`, RB3_FRAME_TRACE)

Native is the un-multiplied floor; web multiplies each `objMs`/`lpu` by the JSPI per-read
suspension (50-194 ms web longtasks ← these native ms). Track B targets the **`lp`
(LoadMgr.Poll background)** bucket. Three fix runs vs baseline:

### `lp` (per-frame budgeted background loader) — busy frames only

| build | lp p90 | lp p99 | lp max | frames >12 ms | frames >16 ms |
|---|---|---|---|---|---|
| **BASELINE** | 14.1 | 19.5 | **19.5** (2.4× budget) | 11 | **1** |
| **FIX** run1 | 10.9 | 14.9 | **14.9** | 4 | **0** |
| **FIX** run2 | 10.7 | 14.5 | 14.5 | 4 | 0 |
| **FIX** run3 | 10.5 | 14.6 | 14.6 | 6 | 0 |

The background-loader peak drops **19.5 → ~14.6 ms** (consistent across runs), and the
worst-case overspill (`lp > 16 ms`) is **eliminated** (1 → 0 in every run).

### per-object PostLoad sum (`objMs`) — frames over budget

| build | objMs frames >8 ms | >12 ms |
|---|---|---|
| BASELINE | 12 | 4 |
| FIX | **3** | 1 |

Confirms a single PollFrontLoader no longer drains a fresh full budget of objects on top of a
part-spent frame.

### Does it stretch total load time? — NO

`game_screen` load-in window (wall-time from first game_screen frame to load-settle, plus the
total load-work = `lp + lpu + objMs` summed over the window):

| build | load-settle wall | total load-work |
|---|---|---|
| BASELINE | 1561 ms | 437 ms |
| FIX | 1413 ms | 441 ms |

Total load-work is identical (437 vs 441 ms — noise); settle wall did **not** stretch (the
delta is run-variance). The fix only removes the *overshoot above* the existing 8 ms budget —
the same objects load over the same frames, packed to ~8 ms/frame instead of spiking to
14-19 ms. **Peak reduced, duration unchanged** (the ideal Track B outcome).

## What is NOT addressed by Track B (by design)

- The `dt = 97.8 ms` worst frame is **`lpu = 51 ms` (`PollUntilLoaded` sync texture drain) +
  `texMs`** = the venue-normal-map reveal (`RndTex:floor_wood02_NORM.tex`). That is **stall
  #1**, owned by the texprewarm prototype (`wt-framestall-texprewarm`). Track B leaves the
  synchronous-contract drains on their full slice on purpose. The fix's `objMs`/`dt` max are
  unchanged because that frame is `lpu`-dominated, not `lp`.
- A single object whose PostLoad alone exceeds the remaining budget still runs to completion
  in one pass (PostLoad is one uninterruptible virtual call). On native no single object is
  huge (worst `objWMs` ≈ 3-7 ms); on web the JSPI-multiplied heavy object is the residual.
  Splitting one PostLoad mid-call would require re-entrant per-object state machinery
  (invasive in shared decomp) — out of scope; the remaining-budget cap is the high-ROI,
  low-risk lever and it makes the *next* object not compound the overrun.

## Correctness / safety

- Both fix runs reached `game_screen` and ran the full ~30 s of gameplay with no
  asserts/aborts/errors; loading is correct (3499 / 3978 game_screen frames).
- `kSliceFloorMs` preserves forward progress for freshly-front loaders (no new
  front-loader-stall risk).
- Env knob unchanged: `RB3_LOADER_BUDGET_MS` (default 8) still tunes `sBudgetMs`; set it huge
  to restore drain-to-completion. A/B against the old overshoot is `RB3_LOADER_BUDGET_MS=100000`
  (effectively un-budgeted) vs default.

## Reproduce

```bash
cd .claude/worktrees/fstall2-postload   # (engine-linked; clang)
cmake --build native/build-native --target rb3-native -j"$(nproc)"
python3 scripts/native/frame_profiler.py --into-song --run-secs 25 \
        --trace /tmp/ft.jsonl --worst 25 --port 887X
# parse the JSONL `lp` bucket (busy frames lp>0.5) for p90/p99/max + >12/>16ms counts.
```
