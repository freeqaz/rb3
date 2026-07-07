# W0.3d-b — Lane A, Stage A-S1 (attribution-first) — STATUS

Checkpoint `/tmp/wave12-checkpoints/A-S1.json`. Mode: DIAGNOSIS + attribution instrument.
NO seam implemented (design only). Build: own dir `native/build-agent-W0.3d-b` (clang).
Commits: rb3 `653ba4a4` (task0 --tol), rb3 `<this>` (probe+harness+STATUS), engine
`bd9522d` (classjson RB3_LOADDET_PROBE, no pin bump — coordinator regens gen.inc).
Six defaults ON, refuted flags UNSET, probe getenv-gated (`RB3_LOADDET_PROBE`) default-OFF,
gate-OFF verified byte-inert (0 `[LOADDET]` lines on a normal boot). Left the uncommitted
engine `FxSendNative.cpp` audio edit untouched.

## TASK 0 — DONE: tightened wash/BOOTRNG capture `--tol` 2000/250 -> 150 ms
`scripts/native/wash-measure.py`, `scripts/native/wash_matrix.py`,
`execution/BOOTRNG/bootrng_probe.py`. The ±2000 ms bootrng window admitted the
bimodal ~20,900 / ~21,150 ms songMs split that drove the dominant BOOTRNG confound
(`pearson(songMs,mid_sat)=0.77` was cluster separation, not within-cluster structure).
All later spread gates now compare same-tol arms vs a fresh OFF-arm baseline. Harness-only;
no binary change. Committed `653ba4a4`.

## TASK 1 — DONE: RB3_LOADDET_PROBE attribution instrument (built + run)

Two co-registered stderr streams, getenv-gated, HX_NATIVE-only:
- `[LOADDET] frame=<N> gdraw=<cum RB3GRandDrawCount at frame N start>` — per-frame
  global-stream position. Tap in `native/src/rb3_session_trace.cpp` `RB3TraceSetFrame`
  (runs once/RunOneFrame at frame start under RB3_FIXED_CLOCK).
- `[LOADDET] complete frame=<N> gdraw=<cum> kind=<dir|data> name=<file>` — the frame a
  loader reached DoneLoading. `dir` tap at `src/system/obj/DirLoader.cpp:736`
  (LoadObjs->Done). `data` tap at `src/system/obj/DataFile.cpp` `DataLoader::ThreadDone`
  (the async ThreadCall-worker-completion point, MainThread-asserted — the H-TIMING hook).

Probe TU: `native/src/rb3_loaddet_probe.cpp` (+CMake wiring). Harness:
`execution/W0.3d-b/loaddet_probe.py` boots N headless (`MILO_MAX_FRAMES`, `setarch -R`,
RB3_FIXED_CLOCK), aligns per-frame gdraw, finds the FIRST divergent frame, and cross-tabs
completion frames.

### Measured (N=8 concurrent + N=4 quiescent, 120 frames, headless boot)

| metric | N=8 concurrent | N=4 quiescent (sequential) |
|---|---|---|
| final gdraw per boot | 17157/16767/17183/16715/17150/17158/17180/17083 | 16300/16501/16321/16290 |
| final gdraw spread | **468** | **211** |
| completions / boot | 511 (identical) | 511 (identical) |
| completion-frame variance | **NONE** (byte-identical stream, md5 match boot0==boot3) | NONE |
| all completions land by frame | **2** | 2 |
| FIRST divergent gdraw frame | **4** | (per-frame md5 all distinct) |

Per-frame gdraw table at the divergence (N=8), start point identical, frame 3->4 delta varies:

```
frame | boot0  boot1  boot2  boot3  boot4  boot5  boot6  boot7
  3   |11151  11151  11151  11151  11151  11151  11151  11151   (IDENTICAL)
  4   |11153  11180  11285  11233  11153  11312  11286  11154 <-- DIVERGES (delta 2..161)
  5   |11981  11543  11877  11517  11866  11668  11668  11877
```

Per-boot rhythm is STABLE within a boot (e.g. boot0 draws +2 every even frame; boot3 draws
+82 every even frame) but DIFFERS between boots — a per-boot-fixed consumption pattern set at
load, not per-frame noise.

### The FIRST divergent frame, named
**Frame 4. What happened on it: NOTHING loaded.** Zero DirLoader/DataLoader completions on
frames 3, 4, or 5 (all 511 completions finished by frame 2, byte-identical across boots). The
loaded object set and every completion frame are IDENTICAL; only the number of gRand draws the
frame's polling consumes differs. So the divergence is **NOT a load completing on a different
frame, and NOT an order swap of loads** — it is a per-frame gRand **COUNT** divergence at
provably-identical loaded state.

## TASK 2 — pre-registered hypotheses, adjudicated on the evidence

### H-TIMING (completion-FRAME timing) — **REFUTED (this regime)**
Prediction: async ThreadCall / DataLoader completions land on different sim frames -> live
Polling set differs per frame -> per-frame count diverges. **Evidence against:** all 511
completions are byte-identical across boots and all land by frame 2; the divergence starts at
frame 4 with ZERO completions on the diverging frames. Completion-frame variance = NONE. The
async-completion timing IS deterministic here; it is not the axis. (Caveat: this is a headless
boot; the venue/song async loads that stream DURING gameplay were not exercised — a gameplay-nav
re-run is the residual check, but the mechanism below already diverges with ALL loads
completing identically, so completion timing is not NEEDED to explain the spread.)

### H-ORDER (unsorted consumer walk -> gRand) — **SUPPORTED as the mechanism class**
The binding correction A2 held "the spread is a COUNT axis a permutation cannot produce" — that
is true only for **fixed-count** consumers. The per-frame consumer set contains **variable-count
(rejection-sampling) gRand consumers**, confirmed in-tree:
- `Rand::Gaussian()` (`Rand.cpp:56-76`) — a `do{...}while(f5>=1.0 || f5==0)` rejection loop
  drawing 2,4,6,... values; called from `Wind::` (`rndobj/Wind.cpp`).
- `CameraShot.cpp:265-267` — `if (RandomFloat()<freq){ RandomFloat(); RandomFloat(); }`
  conditional draws (0 or 3 per shot).
- `Crowd.cpp:1234` — `RandomInt()%(i+1)` Fisher-Yates shuffle (per-element draw).

With a rejection sampler in the set, **a consumer-ORDER permutation DOES change the total COUNT**:
each rejection sampler starts at a different stream offset depending on what ran before it, so it
rejects a different number of times. The order variance itself comes from **unsorted,
address-ordered iteration**: `mAnims` is built by an `ObjDirItr<RndAnimatable>` walk
(`Dir.cpp:53`) and, unlike `mPolls` (SortPolls) and `mDraws` (SortDraws), is **never sorted**;
`RndDir::SetFrame` iterates it (`Dir.cpp:276`). Object addresses shuffle boot-to-boot because the
native ThreadCall worker pthread (`ThreadCall_Native.cpp`) parses milo DTA on its own glibc arena,
racing the main thread's allocations — a race present **even in a single quiescent boot** (the
quiescent N=4 still diverges, spread 211, per-frame md5s all distinct; `setarch -R` pins ASLR base
but not pthread scheduling). So: worker↔main alloc race -> per-boot-fixed shuffled object order ->
unsorted per-frame walk visits rejection samplers at different offsets -> per-frame gRand COUNT
diverges -> cumulative stream position diverges. The stable per-boot rhythm + differing TOTALS
(a pure permutation would preserve the total; totals differ 16290..16501) confirm the
order->count coupling through rejection samplers, not a pure reorder.

The exact single leak site (which unsorted walk feeds which rejection sampler) is one instrument
deeper (per-consumer tagging) — a follow-up, out of A-S1's attribution scope. The CLASS is named
and proven.

### H-RESEED (canonical mid-boot reseed under fixed clock) — **RECOMMENDED SEAM**
Reseed `gRand` to a canonical constant at a deterministic, boot-count-INDEPENDENT anchor under
RB3_FIXED_CLOCK. Because the trace shows the divergence is entirely in per-frame COUNT at
identical loaded state, reseeding at anchor A makes `gdraw[>=A]` a deterministic function of the
reseed constant -> **all post-A draws byte-identical across boots -> the pinned capture (post-A)
collapses to one stream position**, regardless of how the pre-A order/count diverged. Residual,
priced from this trace + BOOTRNG S1: the only surviving divergence is STATE set by pre-A draws that
persists to capture WITHOUT being overwritten post-A. BOOTRNG already proved render SELECTION
(preset/grade/light color/type/showing) is invariant -> NOT in the residual. The residual is
animated-content PHASE (crowd idle pose, FX/swept-light emitter phase), most of which is re-driven
each post-A frame and converges quickly; the genuinely-persistent sliver is exactly the FX/
swept-light-phase axis BOOTRNG S2 already filed as a separate finding. So H-RESEED collapses the
MEASURED stream-position/mid_sat spread and does NOT falsely claim the WHITE FX residual.

## RECOMMENDED SEAM DESIGN (design only — NOT implemented this stage)

**Primary: H-RESEED, fixed-clock-scoped, own opt-in flag `RB3_LOAD_DETERMINISM` (acceptance A3).**
- Shape: an idempotent `RB3ReseedGRandAtAnchor()` that calls `gRand.Seed(<canonical, 0x5EED-derived>)`
  exactly once per boot, the first time a **deterministic boot-count-independent anchor** is reached
  (candidate: `game is_playing` 0->1, or a fixed songMs threshold — NOT a fixed frame index, which
  is boot-count-dependent). Gated `RB3FixedClockActive() && RB3LoadDeterminism()`.
- Files: `src/system/math/Rand.{h,cpp}` (the reseed helper, beside the existing
  `RB3GRandDrawCount`) + one anchor call site (BandDirector song-start or App::RunOneFrame keyed on
  is_playing). Flag registered in engine classjson (append-only, default-OFF during wave;
  coordinator flips opt-in->fixed-clock-default at wave end with the drawlog re-golden provision).
- Risks: (a) the anchor MUST be boot-count-independent or it re-imports divergence — pick an
  engine-state edge (is_playing), not a frame counter; (b) reseeding changes the exact gRand stream,
  so any gRand-sensitive exact-order golden (drawlog world-xfms) must be re-captured under seam-ON —
  but the seam is fixed-clock-scoped opt-in, so flag-OFF goldens (the 792 canonical) stay
  byte-identical (regression gate reads "flag-OFF 792 byte-identical; flag-ON re-golden");
  (c) the FX/swept-light-phase residual persists — the seam's gate must NOT claim to fix the WHITE
  spikes (that is BOOTRNG S2's separate axis).

**Complementary (upstream, partial): serialize the ThreadCall worker under fixed clock.** Running
`DataLoader`'s parse inline (`unk38->ThreadDone(unk38->ThreadStart())`) under RB3_FIXED_CLOCK removes
the worker↔main alloc race that shuffles addresses — W0.3d Experiment A already showed this drops
order-variants 4->2. It REDUCES but does not ELIMINATE the pre-anchor divergence (other allocations,
e.g. Cache_Wii, still shuffle), so it is a hardening lever, not a standalone fix. H-RESEED is the
robust closer; worker-serialize narrows the pre-anchor residual under it. NOT recommended as the sole
seam (multi-site whack-a-mole; the rejection-sampler coupling leaks count on any residual order
variance).

**Rejected: H-ORDER root-fix (sort every per-frame gRand-consumer walk).** Deterministic-ordering the
unsorted `mAnims`/ObjDirItr walks is multi-site and, because ANY residual order variance leaks count
through the rejection samplers, low-confidence of full closure — same failure shape W0.3d Experiment A
hit. Not the recommended seam.

## Fail-red / gate note for S2 (per acceptance A4)
The A4 fail-red proof (seam-ON 10/10 identical stream position under induced contention + worker-latency
jitter; seam-OFF reproduces the spread under the same jitter) is directly buildable on this harness:
`loaddet_probe.py --n>=10` already reproduces the OFF spread under concurrent contention (468) AND
quiescent (211 — the W0.3c quiescent-machine trap is defeated, the intrinsic worker race suffices).
S2 adds the seam flag and re-runs: ON must collapse final-gdraw spread to 0 at the anchor.

## Artifacts
`PLAN.md`, `loaddet_probe.py`, `/tmp/loaddet/as1.json` + `as1-table.json`, raw boot logs
`/tmp/loaddet/as1-boot{0..7}.log` + `/tmp/loaddet-quiescent/q-boot{0..3}.log`. Probe:
`native/src/rb3_loaddet_probe.cpp`, taps in `rb3_session_trace.cpp` / `DirLoader.cpp` /
`DataFile.cpp`. Checkpoint `/tmp/wave12-checkpoints/A-S1.json`.
