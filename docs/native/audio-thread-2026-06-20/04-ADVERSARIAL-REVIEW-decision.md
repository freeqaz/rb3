# ADVERSARIAL REVIEW — off-main web-audio architecture decision

**Date:** 2026-06-20
**Reviewer:** adversarial review pass (tried to REFUTE the recommendation)
**Subject:** `03-DECISION-offmain-web-audio.md` (recommends **path A**: pure-JS
AudioWorklet mixes per-stem SABs on the audio thread)
**Verdict:** **CONFIRM with concerns.** The MVP is evidence-backed and genuinely
stall-proof *for the music stems* (the thing that matters). Two scoping gaps in
the headline/effort framing are real but do not change the decision.

---

## What I tried to refute, and what the code says

I verified every load-bearing claim against the actual source, not the prose.

### 1. "A ~9 s decode-ahead ring already exists and is SPSC" — **TRUE, verified.**
- `src/system/synth/StreamReceiver.h:55-69`: under `HX_NATIVE`, `mBuffer[0xC0000]`
  = 16 chunks × `0xC000`, int16 mono ≈ **9.1 s**. Confirmed, not asserted.
- `native/src/rb3_stream_receiver_native.cpp:314,395,454-455`: `mAudioReadPos` is a
  real `std::atomic<int>`, consumer-owned, written release / read acquire; producer
  cursors (`mRingWritePos`/`mRingWrittenSpace`/`mRingReadPos`) are plain ints read
  racy-benign on the consumer side (stale-low under-reports availability for one
  callback, never reads unwritten memory). This is a **genuine SPSC handshake that
  already runs cross-thread on native** (miniaudio audio thread vs main). Moving the
  drain to a web audio thread reuses the exact same protocol. Not hope — shipping.

### 2. "Decode and mix are separable; only the mix must move off-main" — **TRUE.**
- `rb3/src/App.cpp:561-569`: on web, **both** `TheSynth->Poll()` (decode → `WriteData`
  → fills the 9 s ring) **and** `AudioDevice::PumpAudio()` (mix → drains the ring) run
  back-to-back in the **same rAF tick**. During a 400 ms main-thread stall BOTH freeze.
- Path A moves only the drain (`MixSources`/`RenderAudio`) into the worklet. Decode
  stays rAF-gated. **The honest consequence:** during a stall the worklet keeps
  draining the 9 s ring while decode is also frozen — i.e. the ring is *drained, not
  refilled*. This is fine **because the ring is 9 s deep**. The starvation is NOT
  "moved to decode"; decode simply pauses and the deep cushion covers it. The model
  is sound and matches exactly why native is stall-immune.

### 3. The spike's "0% under-run" is not a measurement cheat — **verified honest.**
- `worklet-mix/scripts/web/worklet-mix-spike/index.html:182-220`: the stall busy-spin
  (`while (Date.now() < until)`, no yield) sits in the **same rAF callback** as the
  producer, so a long stall delays the *next* rAF — the producer skips ticks during the
  freeze, exactly like `App::RunOneFrame` under a real longtask. Both A/B modes are
  subject to the identical stall; the ONLY difference is where the mix runs. The spike's
  0% is a faithful model, and it falsifiably **breaks at 3500 ms** = precisely the 3 s
  prototype ring depth (`SPIKE_WORKLET_SIDE_MIX.md` sweep 2). A model that breaks
  exactly where its own theory predicts is the opposite of hope.

### 4. "Zero build risk — no shared wasm memory, no new flags" — **TRUE, already proven in prod.**
- `engine/src/platform/web/assets/audio-worklet.js:75-82`: the **shipping** worklet
  already receives a standalone `SharedArrayBuffer` (`e.data.sab`) as the output ring and
  reads it with `Atomics` — in the current single-threaded, non-shared-wasm-memory,
  `-sJSPI` build. So "pure-JS worklet + SAB + Atomics, no `-pthread`, no shared heap"
  is **not a hypothesis; it is the architecture already in production.** Path A's only
  delta is N per-stem SABs + mixing in the worklet instead of one pre-mixed SAB. It
  cannot collide with `-sJSPI` because it never touches the wasm threading model.
- `native/web/server.py:411-415`: `COOP: same-origin` + `COEP: require-corp`
  (+`CORP: cross-origin`) are live; `crossOriginIsolated` already holds (the shipping
  worklet proves SAB works under it). **The COOP/COEP/SAB+JSPI combination works in the
  deploy server today.** This was the question I most expected to break the plan; it does
  not.

### 5. The BLOCKED-spike question (does the design respect evidence?) — **respected.**
- The JSPI×shared-memory spike (`02-spike-offmain-jspi-RESULT.md`) came back **VIABLE,
  not BLOCKED** — so there's no blocked result being papered over. Crucially, the
  decision does **not** depend on that spike: path A needs none of it (`-sAUDIO_WORKLET`/
  `-sWASM_WORKERS`/shared heap are path B). The risky spike is correctly demoted to a
  parked fallback (MVP-2), and the design rests on the *zero-flag* path. That is the
  conservative, evidence-respecting choice.

---

## Concerns (real, but do not flip the verdict)

### C1 — SFX is a SECOND source class the synthetic spike never modeled. (the biggest gap)
`MixSources` (`AudioDevice_Web.cpp:513`) mixes a **heterogeneous** `mSources` list:
- `RB3StreamReceiverNative` — the 9 s-ring music stems (deep SPSC, easy to publish).
- `RB3SampleInstNative` — one-shot SFX (`rb3_sampleinst_native.cpp:85,131`), which
  renders from a **bank-resident `mPCMData` pointer with a fractional resample cursor**,
  NOT a deep ring.

The spike modeled 6 sine **stems** only. To mix SFX in the worklet you must either
expose every SFX instance's bank PCM + cursor + lifecycle to the worklet (far more than
the "~150 lines port `MixSources`" the doc estimates), OR keep SFX on a **second
main-thread mix pass** — which means **SFX is NOT stall-immune.** The decision lists this
only as open-question #6, but it materially undercuts two headline framings:
- the flat **"0% under-run"** claim (true for stems, not for SFX during a stall), and
- the **medium / ~150-line** effort estimate (stems-only; SFX adds real work).

**Why it doesn't flip the verdict:** SFX is triggered by main-thread input events, so a
stalled main thread already delays the *trigger* — a brief SFX hiccup during a 400 ms
freeze is acceptable for a rhythm game where the **music must not drop**. The thing the
user actually cares about (continuous music through jank) is exactly what path A protects.
But the MVP must explicitly scope SFX (recommend: keep SFX on a main-thread second pass
for v1, document it as out-of-scope-for-stall-immunity), and the effort/metric language
should stop saying a bare "0%".

### C2 — synthetic→real-engine gap is acknowledged but unmeasured.
The spike is standalone JS (no wasm, no engine, f32-mono sines at ctx rate). The real path
adds: int16→float, per-source vol/pan, the stereo-linked one-pole limiter (`mLimiterEnv`
state must move to the worklet — open-Q #4), and the 44100→ctx **resampler with carry
state** (`mResamplePos`/`mResampleCarry*`, `AudioDevice_Web.cpp:820-872`) — the same
resampler whose single-sample-drop bug already caused a "static/clipping" regression
(MEMORY: audio-verify wave-08). Moving that carry state across the producer/consumer split
(open-Q #1) is the **highest-risk correctness item**, not a formality. The decision does
flag all of these, and the bench (`audio-stall-bench.mjs`, real `20thcenturyboy` →
`game_screen`, real worklet `underrun` instrumentation) plus `audio_verify.py` are the
right gates to *measure* the prediction rather than trust it. So the gap is honestly
disclosed and gated — but the "PROVEN end-to-end" tone in the ranking table overstates a
result that is proven *for the data-flow*, not yet for the full DSP. Treat MVP-1's bench
as a real go/no-go, not a victory lap.

### C3 — prime-gate / start-up burst interaction.
~85% of historical under-runs are the **song-start burst** before the ring is primed
(`audio-worklet.js:62-71`, `AudioDevice_Web.cpp:603-605`). Path A shrinks the output floor
to 60–80 ms; the start-up cushion now has to come from the **stem** rings being pre-filled
before the worklet starts mixing. The MVP must port the prime gate to the new model or the
floor-shrink reintroduces the start glitch. Listed implicitly (open-Q #5) but worth calling
out as a named must-do, since it's the dominant historical failure mode.

---

## Were any viable options dismissed too quickly? — No.
- **D (deeper ring)** is correctly rejected: it *is* the user's complaint (stall budget ==
  output latency), and the baseline already rides the adaptive law to its 500 ms ceiling and
  still breaks at 800 ms (`STALL_BENCH_BASELINE.md`).
- **C (pthread/Worker producer)** is correctly out: a Worker isn't the audio clock, so it
  races the scheduler and still needs a worklet drainer — strictly additive to A/B.
- **B (wasm audio-worklet)** is correctly *parked, not killed*: its toolchain risk is
  retired by the spike, but it buys nothing over A unless exact C++ byte-reuse becomes a
  hard requirement. Keeping it as a de-risked fallback (MVP-2) is right.
The ranking is honest; A genuinely dominates on the two axes the user cares about
(dropout + latency) at the lowest risk.

---

## Is the latency/effort honest?
- **Latency:** honest and the key insight — decoupling stall-budget (stem-ring depth, ~9 s)
  from output latency (60–80 ms floor) is exactly why A wins on dropout AND latency where D
  could only trade one for the other. Verified against the baseline's documented 500 ms
  ceiling behavior.
- **Effort:** **understated** by the SFX gap (C1) and the resampler-carry/limiter-state
  move (C2). "Medium / ~150 lines" is the stems-only happy path; budget for SFX handling +
  the carry/limiter/prime-gate correctness items, which are where bugs will actually land.

---

## Bottom line
The recommendation is grounded in **code-verified evidence**, not hope: the deep SPSC ring,
the separability of decode from mix, the honest stall harness, and — decisively — the fact
that the shipping build **already** runs a pure-JS SAB worklet under COOP/COEP+JSPI. Path A
truly keeps the **music** fed through a 400 ms (and far longer) main-thread stall, and does
NOT merely relocate the starvation. The COOP/COEP/SAB+JSPI combination works in `server.py`
today. The concerns are scoping/effort honesty (SFX is a second source class; the resampler-
carry + limiter-state + prime-gate moves are the real risk), not a flaw in the architecture.

**CONFIRM with concerns. Build MVP-1 — but scope SFX explicitly, treat the resampler-carry
hand-off as the top correctness risk, and let `audio-stall-bench.mjs` + `audio_verify.py` be
a real gate rather than a formality.**

### Evidence index (all read this pass)
- `src/system/synth/StreamReceiver.h:55-85` — 9 s ring, `HX_NATIVE`
- `native/src/rb3_stream_receiver_native.cpp:60-72,283-399,454-455` — SPSC, RenderAudio drain
- `native/src/rb3_sampleinst_native.cpp:85-200` — SFX is NOT a deep ring (C1)
- `rb3/src/App.cpp:561-569` — decode + mix both rAF-gated, separable
- `engine/src/audio/AudioDevice_Web.cpp:499-556,560-573,769-888` — MixSources + limiter + resampler-carry
- `engine/src/platform/web/assets/audio-worklet.js:62-87,100-176` — prod already pure-JS SAB+Atomics
- `native/web/server.py:411-415` — COOP/COEP/CORP live
- `worklet-mix/.../index.html:182-220` — honest in-tick stall injection
- `scripts/web/audio-stall-bench.mjs` — real-song real-worklet gate exists
