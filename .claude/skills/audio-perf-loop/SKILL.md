---
name: audio-perf-loop
description: Orchestrate a multi-wave, doc-synced investigation loop into (1) AUDIO CORRECTNESS — record the game's output and measure how far it diverges from the expected/ground-truth source audio (the "clipped noise" problem), and (2) ASSET-LOAD PERFORMANCE — profile and attribute the load-time stutters. You act as orchestrator/coordinator: each wave plans+reviews, fans out subagents (via the Workflow tool / ultracode) over independent hypotheses, converges, builds, re-measures, then plans the next wave. Agents hand off through on-disk docs. Use when iterating on web/native audio fidelity or load-stutter perf.
argument-hint: "[audio|perf|both]  (default: both)  — optional: native|web target hint"
allowed-tools: Workflow, Agent, Task, TaskCreate, TaskUpdate, TaskList, TaskGet, Bash, Read, Write, Edit, Grep, Glob
---

# audio-perf-loop — orchestrated audio-fidelity + load-stutter investigation

You are the **orchestrator/coordinator**. You do *not* personally do the deep
investigation — you scope it, fan it out to subagents, converge their findings,
land the fix, re-measure, and plan the next wave. Keep looping until the exit
criteria are met or the user stops you.

This skill encodes two concrete, currently-open problems:

1. **Audio correctness.** Song/preview audio plays but sounds like **clipped
   noise**. We need to *record the engine's output and measure how far it
   diverges from the expected (ground-truth) source audio* — a real
   reference-vs-output divergence metric, not just "is it silent / is it
   reproducible". Quantify the divergence and trace it to a root cause
   (clipping/overflow, sample format, channel mix/downmix, decode corruption,
   resample, gain).

2. **Asset-load stutter.** Loading assets makes the game stutter. We need to
   *profile the application, attribute each long frame to the specific asset/work
   that caused it*, and drive the stutters down.

`$ARGUMENTS` selects focus: `audio` | `perf` | `both` (default `both`). A second
word may hint target (`native` / `web`); default to **native-first** (≈3s
rebuilds, headless, identical engine) and confirm web-specific fixes once in the
browser afterward.

---

## Why these problems aren't already solved by existing tools

The repo already has audibility + perf-tail tooling, but each leaves the exact
gap this loop must close — read these first so you don't reinvent them:

- `scripts/native/analyze_preview_audio.py` proves a capture is "real music, not
  noise" by comparing **two captures of the same preview** (self-reproducibility)
  + tonality. It does **NOT** compare against the *expected source audio*. The
  "clipped noise" symptom can be perfectly reproducible and tonal yet still wrong.
  **Gap to build:** a reference-vs-output divergence comparator.
- `scripts/native/loadperf-frametail.py` reports the long-`RunOneFrame` tail with
  per-frame `poll`/`pollUntil` ms + screen, split loader vs non-loader. It tells
  you *that* frames are long, not *which file/asset/object* each long frame was
  loading. **Gap to build:** per-asset stutter attribution.

The ground truth for audio divergence is available: **145 source `.mogg` files**
under `orig-assets/extracted/songs/<song>/<song>.mogg`, and **`ffmpeg`/`ffprobe`
are installed**. A MOGG is an OGG-Vorbis stream behind a small header (first
4 bytes little-endian = version; next 4 = byte offset to the `OggS` start —
`dd bs=1 skip=<offset>` or seek past the header, then `ffmpeg -i - -f s16le`).
Decoding that to PCM gives the ground truth to align the captured output against.

---

## The handoff-doc convention (how waves stay in sync)

All cross-wave state lives under `docs/native/audio-perf-loop/` (create it on
first run). This is the single source of truth every subagent reads before acting
and writes findings into — that is what lets waves hand off cleanly.

- **`STATE.md`** — the canonical living brief. Sections: `## Goal`,
  `## Current phase/wave`, `## Confirmed facts`, `## Ruled out`,
  `## Open hypotheses (ranked)`, `## Measurement baselines` (latest audio
  divergence numbers + perf long-frame histogram), `## Next wave plan`,
  `## Tooling index` (scripts/env knobs that work). You (orchestrator) own
  STATE.md; you rewrite it at the end of every converge phase.
- **`wave-NN-<topic>.md`** — one per wave. Each fan-out agent appends its own
  `### <agent label>` section (hypothesis, what it ran, raw numbers, verdict,
  artifacts). Ends with your `### SYNTHESIS` block.
- **`baselines/`** — captured WAVs, ground-truth PCM, metric JSON, perf logs you
  want to A/B across waves. Keep filenames wave-stamped (`w03-native-preview.wav`).

**Every subagent prompt MUST instruct the agent to (a) read
`docs/native/audio-perf-loop/STATE.md` first, and (b) write its findings to the
current `wave-NN-*.md` before returning its structured verdict.** Agents return
*data* to you; the durable record is the doc on disk.

---

## The loop — three phases per wave, repeat

### Phase 1 — PLAN & REVIEW  (you, lightly assisted)

1. Read `STATE.md` (or bootstrap it from the memory pointers below on wave 1).
2. Pull current context: relevant memory files, `git log --oneline -15`, the
   uncommitted audio diff (`git diff -- src/system/synth src/App.cpp native/src`).
3. Form/refresh a **ranked list of falsifiable hypotheses**. For each: the
   one-sentence claim, the single measurement that would confirm/refute it, and
   the tool/script that produces that measurement.
4. (Optional, for a contested or stuck wave) fan out a tiny **review panel**:
   2–3 agents that each independently critique the current hypothesis ranking and
   propose what's missing. Cheap insurance against tunnel vision.
5. Emit a **wave task list**: 3–8 *independent* investigations sized so they can
   run in parallel without colliding (no two editing the same file; reads/measure
   only in fan-out — code edits happen in converge).

### Phase 2 — FAN-OUT WAVE  (Workflow / ultracode)

Launch **one `Workflow`** for the wave (this is the ultracode delegation the user
asked for). Structure it as the canonical pipeline: investigate → adversarially
verify. Each investigator:

- reads `STATE.md`, runs its measurement (build native if needed, capture, analyze),
- writes its `wave-NN-*.md` section,
- returns a structured verdict `{hypothesis, status: confirmed|refuted|inconclusive, evidence, numbers, proposedFix?}`.

Then **adversarially verify** every *decisive* claim — especially any
"audio is now correct" or "X is the stutter cause" — with independent skeptic
agents prompted to *refute* (default to refuted if uncertain). A plausible-but-
wrong audio verdict is the main failure mode here; require the
reference-divergence number, not vibes.

A reusable skeleton is in **`## Workflow skeleton`** below — adapt the
`INVESTIGATIONS` list each wave; don't rewrite the harness.

### Phase 3 — CONVERGE & PLAN NEXT  (you)

1. Collect verdicts; reconcile with the wave doc.
2. **Update `STATE.md`**: move confirmed→facts, refuted→ruled-out, add new
   hypotheses, refresh baselines.
3. If an actionable fix emerged: implement it (small edit yourself, or delegate a
   focused impl agent for anything non-surgical — use a worktree via
   `tools/setup-worktree.sh` for non-trivial/parallel edits, per repo git-safety
   rules). Then **build and RE-MEASURE**:
   - native engine change: `cmake --build native/build-native` (engine target is
     separate — NOT `--target rb3-native`; the app target is `rb3-native`).
   - web confirm: `scripts/web/build.sh` (slow ~minutes; only for web-specific or
     final cross-check).
   - re-run the same capture/profile script and compare the metric to the
     baseline in STATE.md. A fix that doesn't move the number isn't a fix.
4. **Decide:** exit criteria met? → stop and summarize. Otherwise → write the
   next-wave plan into STATE.md and loop to Phase 1.

Surface a 2–4 line status to the user after each converge (wave #, what moved,
the key number, next wave intent). You are the coordinator — keep them oriented.

---

## Concrete tooling reference (real, runnable today)

**Headless debug harness** — `RB3_HTTP=1` exposes `/api/health` (frame, songMs,
currentScreen), `/api/screenshot` (PNG), `/api/input` (nav verbs),
`/api/dta/eval` (live engine state). Nav via `RB3_GAME_INPUT="<script>"`.
Binaries: `native/build-native/rb3-native`; data: `orig-assets/extracted`.

**Audio capture / analysis:**
- `python3 scripts/native/song-preview-audio-test.py [--secs N --out WAV --verbose]`
  — boots to song_select, fires SongPreview, dumps mixed output WAV, per-second
  RMS + audibility verdict. Env it sets: `MILO_AUDIO=1 MILO_AUDIO_BACKEND=null
  DC3_DUMP_AUDIO=<wav> DC3_DUMP_SECONDS=N RB3_STREAM_AUDIO_DBG=1`.
- `python3 scripts/native/analyze_preview_audio.py A.wav B.wav` — reproducibility
  + tonality (self-comparison). **Extend / pair with a new
  reference-divergence tool** (decode source MOGG via ffmpeg → align → spectral
  + sample divergence + clip/overflow detection). Put the new tool under
  `scripts/native/` and document it in STATE.md's tooling index.
- `node scripts/web/web-song-preview-audio.mjs [--phase preview|song]` — in-browser
  capture (`rb3CaptureAudio`/`rb3DownloadAudio`) + audibility pass/fail.
- `scripts/web/web-audio-capture.mjs`, `scripts/native/_capture_preview.sh` — more capture paths.

**Perf / stutter profiling:**
- `python3 scripts/native/loadperf-frametail.py [--thresh-ms 20 --run-secs 60 --label X]`
  — long-`RunOneFrame` tail + histogram, loader-attributable vs non-loader. Env
  knobs forwarded: `RB3_FRAME_INSTRUMENT=1 RB3_FRAME_INSTRUMENT_THRESH_MS`,
  `RB3_LOADER_BUDGET_MS`, `RB3_PUL_BUDGET_MS`. **Attribution gap to close:**
  correlate each LONG frame to the file/object being loaded (DirLoader/LoadMgr/
  Loader instrumentation: log the current load target per frame, join on frame#).
- `node scripts/web/loadperf-profile.mjs`, `scripts/web/analyze-cpuprofile.mjs`,
  `scripts/web/gpu-boot-probe.mjs` — web CPU-profile + boot markers.

**Prior findings to seed STATE.md (memory):** `project_web_audio_state.md`
(continuous-play ring fix `10af87ab`; VorbisReader CTR-seek EndianSwap double-swap
on LE — store byte/16 direct; preview fires via App.cpp `TheMusicLibrary->Poll()`),
`project_web_loadperf_findings.md` (boot ~13s web / ~5s native; App-ctor sync-I/O
bottleneck; `docs/native/web-loadperf-findings-2026-06-03.md`),
`docs/native/PLAN_LOADER_ASYNC.md`.

---

## Exit criteria

**Audio (per focus):** captured engine output, time-aligned to the ffmpeg-decoded
source MOGG, shows divergence below an agreed threshold (e.g. post-alignment
spectrogram correlation ≥ ~0.6 *and* no clipping/overflow signature: peak not
railed at int16 ±32767, sample-clip ratio ~0), confirmed in **native and web**.
The "clipped noise" character is gone.

**Perf (per focus):** through boot → song_select → gameplay, no loader-
attributable `RunOneFrame` exceeds the target budget (e.g. drop the long-frame
tail under ~100ms / agreed bar), each remaining long frame attributed to a named
asset, confirmed native and web.

Stop when met (or budget/user stop). Leave STATE.md as the final handoff record.

---

## Workflow skeleton (adapt `INVESTIGATIONS` per wave; keep the harness)

> Only call `Workflow` when ultracode/multi-agent is opted in (the user asked for
> it here). Edit the `INVESTIGATIONS` array each wave; reuse the verify pipeline.

```js
export const meta = {
  name: 'audio-perf-wave',
  description: 'One investigation wave: fan out hypotheses, adversarially verify, write handoff docs',
  phases: [{ title: 'Investigate' }, { title: 'Verify' }],
}

// Reset per wave. Each item = one independent, falsifiable investigation.
const WAVE = args?.wave ?? 'NN'
const DOC  = `docs/native/audio-perf-loop/wave-${WAVE}.md`
const INVESTIGATIONS = args?.investigations ?? [
  // { key, prompt } — filled in by the orchestrator each wave
]

const VERDICT = {
  type: 'object',
  properties: {
    hypothesis: { type: 'string' },
    status: { type: 'string', enum: ['confirmed', 'refuted', 'inconclusive'] },
    numbers: { type: 'string' },        // the decisive measurement(s)
    evidence: { type: 'string' },
    proposedFix: { type: 'string' },
  },
  required: ['hypothesis', 'status', 'numbers', 'evidence'],
}
const REFUTE = {
  type: 'object',
  properties: { refuted: { type: 'boolean' }, why: { type: 'string' } },
  required: ['refuted', 'why'],
}

const preamble = (p) => `You are an investigator in the RB3 audio-perf-loop.
FIRST read docs/native/audio-perf-loop/STATE.md for current facts/ruled-out.
Do your measurement (build native with \`cmake --build native/build-native\` if
needed; use the scripts in the skill's tooling reference). Then APPEND a
\`### \${'$'}{label}\` section to ${DOC} with your raw numbers and verdict before
returning. Investigation:\n${p}`

const results = await pipeline(
  INVESTIGATIONS,
  inv => agent(preamble(inv.prompt), { label: `probe:${inv.key}`, phase: 'Investigate', schema: VERDICT }),
  (verdict, inv) => {
    if (!verdict || verdict.status !== 'confirmed') return { inv, verdict, survived: verdict?.status }
    // Adversarially verify decisive 'confirmed' claims (esp. audio-correct / stutter-cause).
    return parallel([0, 1, 2].map(i => () =>
      agent(`Try to REFUTE this confirmed claim from the audio-perf-loop. Default refuted=true if uncertain.
Claim: ${verdict.hypothesis}
Numbers offered: ${verdict.numbers}
Re-run/inspect independently. A reproducible-but-wrong audio result still counts as refuted.`,
        { label: `refute:${inv.key}:${i}`, phase: 'Verify', schema: REFUTE })
    )).then(votes => {
      const refs = votes.filter(Boolean).filter(v => v.refuted).length
      return { inv, verdict, survived: refs >= 2 ? 'refuted-on-verify' : 'confirmed' }
    })
  }
)
return results.filter(Boolean)
```

After the Workflow returns, read the wave doc + the returned verdicts, then run
**Phase 3 (Converge & plan next)** yourself.

---

## Operating rules

- **You orchestrate; subagents investigate.** Don't sink into one diagnosis
  yourself — scope it, delegate, verify, converge.
- **Native-first.** Reproduce and fix in `rb3-native` (≈3s engine rebuilds,
  headless); confirm web-specific fixes in the browser once at the end.
- **Every claim needs a number.** "Audio sounds better" is not a result; the
  reference-divergence metric and the long-frame histogram are.
- **Re-measure after every fix** against the STATE.md baseline; revert fixes that
  don't move the metric.
- **Git safety (repo rule):** no `git stash/revert/checkout/restore` in the main
  repo (concurrent agents); hand-edit to undo; use `tools/setup-worktree.sh` for
  non-surgical/parallel edits.
- **Keep STATE.md current** — it is the contract between waves and the handoff if
  the session is interrupted.
- Scale wave width to remaining uncertainty: many probes early, narrow as the root
  cause localizes. Stop at the exit criteria.
