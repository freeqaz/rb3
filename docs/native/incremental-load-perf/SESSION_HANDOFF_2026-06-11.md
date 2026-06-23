# Incremental-Load Perf — Session Handoff (Fable → Opus)

**Authoring context:** Waves 0–5 of the web incremental-load-perf effort were
planned and orchestrated by **Fable** across one long session (2026-06-10 →
2026-06-11). This doc was written by **Opus** picking the thread back up on
2026-06-23, when Fable was unavailable. Its job is continuity: capture the
*operating method* and the *cross-wave narrative* so any model can continue, and
reconcile the Wave-6 queue against the 12 days of unrelated work that landed in
between.

Canonical detail lives elsewhere — this doc points at it, it does not duplicate it:
- **`PLAN.md`** — ranked levers, invariants, per-wave results tables (Waves 3/4/5).
- **`research/01..09`** — the measured investigations (census, async arch, web-IO
  redesign, frame-budget, waterfall, native-vs-web, **07 network matrix**,
  **08 bytes-reduction plan**, **09 game_screen first-frame plan**).
- Memory: `~/.claude/.../memory/project_incremental_load_perf.md` — the commit ledger.

---

## 1. The operating method (this is the reusable part)

Every wave ran as one **ultracode Workflow** (multi-agent, deterministic
orchestration) with a fixed shape. This shape caught **two blocking
implementation bugs and ten plan-level errors** across the waves that solo work
would have shipped — it is the main process learning.

**Model division of labour (the user's standing rule):**
- **Fable** — planning, deep questions, synthesis, adversarial judgment. *Plans
  run FIRST and write their plan to a doc on disk* (`research/NN-*.md`), so the
  implementation agents inherit grounded `file:line` facts, not a prose brief.
- **Opus** — all implementation, all measurement, all adversarial review, the fix
  pass, and the single integration pass.
- **Sonnet** — purely mechanical edits (counter wire-ins, etc.). Rarely used.

**Phase pipeline (per wave):**
1. **Plan (Fable, first).** One planner per workstream. Each *scouts hands-on*
   (builds, measures, reads code) then commits a plan doc containing: measured
   diagnosis, chosen design, **rejected alternatives with why**, and ≤3
   implementation tasks each carrying an *exhaustive owned-file list*, flags,
   and concrete pass/fail verification. The owned-file lists become the
   conflict map.
2. **Implement (Opus, parallel).** One agent per task, strict file ownership from
   the plan. Each reads its plan-doc section before touching code. Commits its
   own files only.
3. **Review (Opus, parallel, adversarial).** One reviewer per landed task,
   prompted to **refute** — re-run the cheapest decisive check, hunt the
   recurring bug classes (UAF/lifetime on async paths, Wii-match leaks outside
   `#ifdef HX_NATIVE`, `HX_WEB`-instead-of-`__EMSCRIPTEN__` in engine code, flag
   defaults, commit contamination). Structured verdict: `pass | blocking` with
   `file:line` evidence.
4. **Fix (Opus, conditional).** Spawned only if a reviewer returned `blocking`.
   Confirms each finding is real (may refute the reviewer), applies the minimal
   fix in the originating repo, re-runs the decisive check.
5. **Integrate (Opus, single agent).** The *only* agent allowed to bump
   `MILO_ENGINE_PIN` and run the full gate suite (Wii byte-identical → native
   build + tests + smoke → full dual web build → web gates → docs append). One
   pin bump, one build, one gate run.

**Why it works / discipline that paid off:**
- **Plan-as-doc handoff** — agents return condensed structured summaries; the
  durable artifact is on disk. Survives context loss and agent death.
- **Adversarial review with a refute mandate** is non-negotiable. It caught a
  Range-fetch use-after-free reachable only by *preview-cancel-on-song-change*
  (the implementer's single-song test couldn't hit it) and a CMakeLists
  contamination that referenced a concurrent agent's untracked file.
- **Single integrator** prevents pin-bump races and double builds.
- **Gate at the conditions that can SEE the bug.** The 20 Mbps/40 ms gate was
  *provably blind* to the serial-fetch class — Wave 4 only found it by gating at
  **8 Mbps/80 ms + 4 Mbps/150 ms**. This is now the standing rule for any
  load-perf gate. (Network throttling via CDP `Network.emulateNetworkConditions`;
  harness `scripts/web/_netmatrix.mjs`.)

**Recovery patterns (learned the hard way):**
- Background **investigation agents that park themselves "waiting on a monitor"
  die.** Drive long polls with *foreground* `until`-loops, not by ending the turn
  on a monitor. (Two matrix agents and three Wave-5 warm-task agents died this
  way; the work survived only because of the next point.)
- **Workflow `stop + resumeFromRunId` replays the cached agent prefix instantly**
  and re-runs only the dead/edited task. The Wave-5 GPU-warm task needed *three*
  attempts; resume replayed 7 cached agents each time and the runtime respawned
  the dead key. Before resuming, archive the dead agent's partial tree edits to
  `/tmp` so you can hand-finish if it dies again.
- **`miloSerialΣ` inflates under concurrency** — once fetches overlap, the sum of
  fetch durations is no longer a wall-clock proxy. Use `peakConcurrent` /
  `overlappingMilos` / `chunkReDownloads` (see `analyze_net.py`).

**Standing gotchas baked into every agent prompt:**
- Engine TUs compile `HX_NATIVE`-only → web-only engine code is `#ifdef
  __EMSCRIPTEN__`, **never** `HX_WEB` (that symbol only exists on the `rb3-web`
  target). rb3 sources may use either.
- Any edit to a matched Wii TU (`src/system/**`, `src/band3/**`) sits entirely
  inside `#ifdef HX_NATIVE` → Wii build stays byte-identical (a hard gate:
  31951/41254 funcs @ 62.884%).
- Flags: default-ON, `RB3_*_OFF` opt-out, `getenv`-once static; A/B-able in
  browser via the `?env=RB3_X=1` URL→ENV bridge.
- Build locks: `flock /tmp/rb3-{native,web}-build.lock`, `rb3-native-run.lock`;
  Wii via `tools/ninja-locked` only. `server.py` does not hot-reload (restart it).
- Always rebuild+redeploy before measuring (stale-deploy false bugs cost real
  time); pre-js edits need a forced relink (cmake doesn't track them);
  gameplay screenshots before `songMs>0` show the ~25 s intro cinematic — wait
  for track slide-in before judging gameplay.

---

## 2. Cross-wave narrative (what shipped, what it taught)

The whole effort rests on **one diagnosis**: Milo's loader is *already async* above
the `File` class (FileLoader/DirLoader state machines, ReadDone/TempEof,
kLoadFront dependency queue). The native port collapsed that asynchrony at **one
seam** — `NativeStdioFile` did a blocking sync XHR on a MEMFS miss and `ReadAsync`
read inline, so the engine's async machinery was dormant dead code. Native
`lpu=0.0ms` everywhere proved the 17 `PollUntilLoaded` sites were free with
resident bytes. **Fix the seam, not the call sites.**

- **Waves 0–2** (pin `d71aadc3` → engine `fb23b5e`): async pending-File open
  (manifest size/404 oracle + async ensure-resident), HTTP-206 Range moggs (~4 MB
  vs 36 MB whole-file), BC-native texture upload, preview prefetch. Headline:
  cold preview hover **7.5–12 s blocking → frozen 0 ms** at 20 Mbps; boot 3.62 s.
- **Wave 3** (master `1d7ae200`, pin → engine `a0848b1`): per-screen dependency
  bundles (hub→select fetch reqs 20→1), prewarm default-ON for web, pipeline
  pre-warm (venue-build frame 165→82 ms). **M1 measurement killed Q11 sliced
  prime** (primeMs ≈ 0 at song start — never pursue it) and identified the
  **game_screen first frame ~600 ms** as the real remaining hitch.
- **Network matrix** (`research/07`): the canvas *never freezes* at any
  bandwidth/RTT — the user's remote "hangs" are **wall-clock serial-fetch
  stalls**. ~85 MB of milos download 100% serially (0/38 overlap), 45 s for a
  single venue milo at 4 Mbps, DNF at 1.5 Mbps. Server HOL blocking and sync-XHR
  fallback both **refuted**. This reframed everything downstream.
- **Wave 4** (master `a7f929ee`): loader-queue read-ahead (concurrency 2→6),
  mogg 2-slot read-ahead + cross-open chunk cache (re-downloads 1→0), Resync
  zero-yield-spin fix (a latent web tab-hang). **Honest partial:** parallelism
  kills the per-file RTT tax but *can't beat throughput* — at 4–8 Mbps the pipe
  is saturated, wall ≈ totalBytes/bandwidth. → next lever is **bytes**.
- **Wave 5** (master `81972d4c`, pin → engine `8fb669d`): the bytes wave.
  **4 Mbps cold journey 181 → 115.5 MB (−36%)**, driven by **vorbis SFX sidecars
  (59 MB raw PCM → 8.5 MB ogg, a 10× lever)** + q11 brotli on milos
  (Content-Encoding, client-transparent). L1 vertex-unpack cache (one-time) +
  `WarmGpuForDir` engine API landed. **Honest miss:** game_screen first frame
  stayed ~600 ms — the L2 venue warm-sweep shipped default-OFF (it was an
  Enter-state-dependent no-op) and L3 in-frame-drain removal never landed.

---

## 3. Wave-6 queue — reconciled against the work that landed since 2026-06-11

The repo moved on for 12 days (input fixes, web-audio, a **render-polish /
framestall** thread). Engine pin advanced `8fb669d` → `20dba552`. Status of each
queued item *as of 2026-06-23*:

| Item | Status | Notes |
|---|---|---|
| **First-frame L2/L3** (600→120 ms reveal) | **MOSTLY ABSORBED** by the framestall thread | `234e3c57`/`86312dcf`/`e5c854e5`: the venue-milo reveal-drain prewarm. The Wave-5 miss is explained: the prewarm targeted `MetaPerformer::GetVenue()` but `BandDirector::EnterVenue` (BandDirector.cpp:631-665) *ignores* `mVenue` and force-loads `small_club_01` via the World `venue` prop. `86312dcf` makes `ComputeVenueMiloPath()` mirror EnterVenue's resolution. **Re-measure on current master to confirm the reveal frame actually dropped — that close-out wasn't fed back into PLAN.md.** |
| **sticky-q11 cache durability** | **LIKELY A PHANTOM** (Opus analysis) | `_encoded_cache_valid` (server.py:577) keys only on size+mtime, ignores level → on-demand *serves* a valid q11 artifact as-is, it does not re-encode/clobber it. The "last-writer-wins" the Wave-5 integrator saw is the **prewarm-vs-on-demand race during prewarm** (before q11 exists), which a "skip if higher exists" guard cannot fix. Real fix if needed: have prewarm atomically *claim* files so on-demand skips them; otherwise the documented "run prewarm to completion before measuring" is sufficient. **Do not patch without an empirical repro.** |
| **A4 content downscale** | **OPEN, needs planning** | The *only* remaining 1.5 Mbps lever (that arm is throughput-bound; bytes already saturate the link). Recompress/downscale textures + strip unused channels/mips for web delivery. `research/08 §A4` has the feasibility sketch. **Needs a Fable plan** (rejected-alternatives + quality-gate design) before implementation — do not freelance it. |
| **L1 steady-state win** (web gameplay p50 18.99 → <17 ms) | **UNVERIFIED** | The vertex-unpack cache was claimed to also shave steady-state draw cost. Pure measurement task (Opus lane): frame-trace web gameplay on current master with `RB3_UNPACK_CACHE_OFF` A/B. |
| **Q9 async venue** (Enter() split) | **OPEN, low priority** | Shipped as a sync-default no-op in Wave 4. Needs an `Enter()` split to become a real lever; small expected upside. |

**Recommended next actions (in order):**
1. **Re-baseline incload gates on current master** (engine moved a lot; framestall
   landed). Confirm the framestall reveal-drain fix cut the first frame, and that
   Wave-5 byte numbers hold. Append a "Wave 6 / re-baseline" section to PLAN.md.
   *(Measurement — Opus can do this directly.)*
2. **A4 content downscale** — the highest-value remaining lever for genuinely-bad
   networks, but **gate it behind a Fable plan** when a planner is available.
3. Verify the L1 steady-state gameplay win (cheap, Opus-direct).

**Operational reminder for any live redeploy:** a deploy needs more than the wasm
— the new `server.py` (compression negotiation), a `prewarm_encode_cache.py` run
(q11 artifacts), and regenerated SFX `.ogg` sidecars (the `xma_convert` pass).
