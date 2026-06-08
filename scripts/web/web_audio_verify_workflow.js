export const meta = {
  name: 'web-audio-verify',
  description: 'Verify WEB build audio with audio_verify.py: establish a working boot+capture, capture preview+song, rank vs source moggs, prove rate, adversarially verify, compare to native baseline',
  phases: [
    { title: 'Establish boot', detail: 'get ONE working in-browser capture (release long-timeout, else debug build, else diagnose the boot hang)' },
    { title: 'Capture', detail: 'sequential preview+song+rate captures + audio_verify (one browser at a time)' },
    { title: 'Adversarial verify', detail: 'skeptics re-analyze artifacts, default to refuted' },
    { title: 'Synthesize', detail: 'web-vs-native verdict + findings doc' },
  ],
}

// The web server is ALREADY RUNNING at http://localhost:8421 (owned by the main
// loop, ThreadingHTTPServer; do NOT build or start it). The web build was just
// rebuilt from current source. Browser captures MUST run one-at-a-time (WebGPU/
// Chromium contention).
const REPO = '/home/free/code/milohax/rb3'
const SONGS = '20thcenturyboy,25or6to4,antibodies'   // the only songs with extracted moggs
const DOC = `${REPO}/docs/native/audio-perf-loop/wave-08-web-audio.md`

const CONTEXT = `You are verifying the RB3 WEB build's audio using our tools.
Server: ALREADY RUNNING at http://localhost:8421 (do NOT build/start it). Repo: ${REPO}.

WHAT THE MAIN LOOP ALREADY FOUND (start here, don't rediscover):
- Native audio is VERIFIED CORRECT and is the BAR web must meet (committed 7d02e80e):
    preview  vs 20thcenturyboy: chroma 0.55 fp_ber 0.31 VERDICT MATCH clip 0.00% rank CONFIDENT(+0.22)
    gameplay vs 20thcenturyboy: chroma 0.54 fp_ber 0.33 VERDICT MATCH clip 0.00% rank CONFIDENT(+0.13)
    rate: not a chipmunk (flat speed curve); not clipped; not noise.
- A first web preview capture HUNG in boot: it reached on-demand asset loading
  (config/gen/metamaterials.milo_xbox fetched on a WORKER thread) and spammed
  "FAIL: .../math/Rand.cpp Line 111 Error: MainThread()" (Rand called off the main
  thread during async milo parse) but did NOT reach song_select within 300s, so NO
  WAV was produced. The on-demand HTTP asset fetch makes web boot much slower than
  native. The uncommitted Loader.cpp change is BENIGN (HX_NATIVE frame-trace
  counters) — NOT the cause.

TOOLS:
- Web captures live in ${REPO}/scripts/web and import ./lib/core.mjs + need
  playwright from scripts/web/node_modules, so run them as:
  \`cd ${REPO}/scripts/web && node <script> ...\`. Each browser capture is SLOW
  (web boot via on-demand fetch can take minutes). Use generous timeouts.
- The DEBUG build (smaller/faster milo parse, no-store) loads at
  http://localhost:8421/?debug=true. lib/core.mjs launchBrowser(port, {query})
  takes a query string, and most capture scripts accept the default release; to
  force debug you may pass the query via the script if supported, or test boot with
  a small custom node snippet using launchBrowser(8421, {query:'debug=true'}).
- Verifier (run from ${REPO}): \`python3 ${REPO}/scripts/native/audio_verify.py\`
  (decrypts+decodes the source moggs itself; --rank ${SONGS}; --selftest = 6/6).
- audio_coherence.py = reference-free "is it real audio / clipped" on a WAV.`

const ESTABLISH = {
  type: 'object',
  properties: {
    bootWorks: { type: 'boolean', description: 'did boot reach song_select and a preview WAV get produced' },
    recipe: { type: 'string', description: 'the exact command/recipe that worked (or "NONE")' },
    wavPath: { type: 'string', description: 'path to a produced preview WAV, or ""' },
    furthestScreen: { type: 'string' },
    hangSignature: { type: 'string', description: 'where/why boot stalls if it does' },
    bootSeconds: { type: 'number' },
    notes: { type: 'string' },
  },
  required: ['bootWorks', 'recipe', 'furthestScreen'],
}

const CAP = {
  type: 'object',
  properties: {
    step: { type: 'string' },
    captured: { type: 'boolean' },
    wavPath: { type: 'string' },
    verdict: { type: 'string' },
    rankWinner: { type: 'string' },
    rankMargin: { type: 'number' },
    chroma: { type: 'number' },
    fpBer: { type: 'number' },
    speedRatio: { type: 'number' },
    clipPct: { type: 'number' },
    ctxSampleRate: { type: 'number' },
    toneHz: { type: 'number' },
    numbers: { type: 'string' },
    notes: { type: 'string' },
  },
  required: ['step', 'captured', 'verdict', 'numbers'],
}

const REFUTE = {
  type: 'object',
  properties: { refuted: { type: 'boolean' }, why: { type: 'string' }, numbers: { type: 'string' } },
  required: ['refuted', 'why'],
}

// ---- Phase 1: establish a working boot+capture (gates the expensive captures) ----
phase('Establish boot')

const establish = await agent(`${CONTEXT}

TASK — get ONE working web SONG-SELECT PREVIEW capture by ANY means, or prove boot
is blocked. Steps (stop as soon as one yields a valid /tmp/rb3_web_preview.wav):
1. RELEASE build, generous timeout (web boot via on-demand fetch is slow):
   \`cd ${REPO}/scripts/web && timeout 600 node web-song-preview-audio.mjs --phase preview --port 8421\`
   Watch the screens it logs. If it reaches song_select and fires the preview, it
   writes /tmp/rb3_web_preview.wav. Confirm with \`python3 ${REPO}/scripts/native/audio_coherence.py /tmp/rb3_web_preview.wav\`.
2. If release never reaches song_select, try the DEBUG build (faster milo parse):
   write a tiny node probe in ${REPO}/scripts/web using \`import { launchBrowser, waitForBoot, engineState } from './lib/core.mjs'\`
   with \`launchBrowser(8421, { query: 'debug=true' })\`, wait for boot, log the
   screen reached. If debug boots, adapt the capture (the capture scripts may take a
   query or you can drive nav with pressKey from core.mjs).
3. If neither boots to song_select, DIAGNOSE the hang: is it just slow (how far does
   it get, how many assets, is frame count advancing) or truly deadlocked (Rand.cpp
   MainThread asserts blocking)? Capture the furthest screen + the stall point.
Be efficient — this gates the rest of the run. APPEND a \`### establish boot\`
section (what you tried, screens, timings, the hang point) to ${DOC}. Return the
schema: set bootWorks=true ONLY if a real preview WAV was produced and
audio_coherence says it is non-empty audio.`, { schema: ESTABLISH, phase: 'Establish boot', label: 'establish' })

log(`establish: bootWorks=${establish?.bootWorks} recipe=${establish?.recipe} furthest=${establish?.furthestScreen}`)

// ---- Phase 2: capture + verify (only if boot works) ----
let C = {}
if (establish && establish.bootWorks) {
  phase('Capture')
  const previewPrompt = `${CONTEXT}

A working boot recipe exists: ${establish.recipe}. A preview WAV may already be at
${establish.wavPath || '/tmp/rb3_web_preview.wav'}.
TASK — verify the WEB PREVIEW audio:
1. Ensure /tmp/rb3_web_preview.wav exists (re-capture with the working recipe if not).
2. Rank: \`python3 ${REPO}/scripts/native/audio_verify.py /tmp/rb3_web_preview.wav --rank ${SONGS} --section preview\`
3. Full single-ref report vs the winner (--section preview).
Compare to the native preview baseline (MATCH 20thcenturyboy, chroma~0.55, ~0% clip).
APPEND \`### web preview\` (commands + raw numbers + verdict) to ${DOC}, return schema. step="preview".`

  const songPrompt = (p) => `${CONTEXT}

Working boot recipe: ${establish.recipe}.
TASK — verify the WEB IN-GAME SONG audio:
1. Capture the streamed song: adapt the working recipe to \`--phase song\` (e.g.
   \`cd ${REPO}/scripts/web && timeout 600 node web-song-preview-audio.mjs --phase song --port 8421\`) -> /tmp/rb3_web_song.wav. Retry once if needed.
2. Rank: \`python3 ${REPO}/scripts/native/audio_verify.py /tmp/rb3_web_song.wav --rank ${SONGS} --section gameplay\`
3. Full single-ref report vs winner (--section gameplay).
Compare to native gameplay baseline (MATCH 20thcenturyboy, ~0% clip). APPEND
\`### web song\` to ${DOC}, return schema. step="song". (preview winner=${p?.preview?.rankWinner || '?'}.)`

  const ratePrompt = (p) => `${CONTEXT}

Working boot recipe: ${establish.recipe}.
TASK — prove the WEB PLAYBACK RATE is correct (wave-05 'chipmunk': browser may clamp
ctx to 48000 while engine pushes 44100; the SAB->worklet resampler must fix it). Run
SEQUENTIALLY (one browser at a time):
1. Deterministic resampler proof — known 440Hz tone, forced ctx=48000, measure output Hz:
   \`cd ${REPO}/scripts/web && timeout 480 node web-audio-tone-verify.mjs --force 48000 --tone 440 --out /tmp/rb3_web_tone48k.wav\`
   PASS = captured tone stays ~440 Hz (chipmunk would shift to ~479 Hz = 440*48000/44100). Parse the printed measured Hz.
2. ctx rate readback: \`cd ${REPO}/scripts/web && timeout 300 node web-audio-rate-probe.mjs --port 8421\`.
3. Cross-check: does \`audio_verify.py /tmp/rb3_web_preview.wav --rank ${SONGS} --section preview\` report speed near 1.0 / not a chipmunk?
Set toneHz, ctxSampleRate, verdict (PASS only if tone ~440 AND no chipmunk). APPEND
\`### web rate\` to ${DOC}, return schema. step="rate".`

  const captured = await pipeline([0],
    () => agent(previewPrompt, { schema: CAP, phase: 'Capture', label: 'cap:preview' }).then(v => ({ preview: v })),
    (p) => agent(songPrompt(p), { schema: CAP, phase: 'Capture', label: 'cap:song' }).then(v => ({ ...p, song: v })),
    (p) => agent(ratePrompt(p), { schema: CAP, phase: 'Capture', label: 'cap:rate' }).then(v => ({ ...p, rate: v })),
  )
  C = (captured[0]) || {}
  log(`captured: preview=${C.preview?.verdict} song=${C.song?.verdict} rate=${C.rate?.verdict}`)
} else {
  log(`boot did not work (${establish?.hangSignature || 'unknown'}); skipping captures, synthesizing the blocker report`)
}

// ---- Phase 3: adversarial verify (parallel, CPU-only — re-analyze artifacts, NO browser) ----
let refutes = []
const claims = [
  C.preview && C.preview.captured && { k: 'preview', text: 'the web PREVIEW audio is the right song (20thcenturyboy) and is NOT clipped', v: C.preview, wav: '/tmp/rb3_web_preview.wav', section: 'preview' },
  C.song && C.song.captured && { k: 'song', text: 'the web IN-GAME SONG audio is the right song (20thcenturyboy) and is NOT clipped', v: C.song, wav: '/tmp/rb3_web_song.wav', section: 'gameplay' },
  C.rate && C.rate.captured && { k: 'rate', text: 'the web PLAYBACK RATE is correct — no chipmunk (forced-48k tone stays ~440Hz; audio_verify speed not off)', v: C.rate, wav: '/tmp/rb3_web_tone48k.wav', section: null },
].filter(Boolean)

if (claims.length) {
  phase('Adversarial verify')
  const refutePrompt = (c) => `${CONTEXT}

You are an adversarial SKEPTIC. Try to REFUTE this WEB claim. Default to refuted=true
if you cannot independently confirm it. Do NOT launch a browser / re-capture —
re-ANALYZE the EXISTING on-disk artifact only (CPU), so several of you run at once.

CLAIM: ${c.text}
Reported: ${c.v.numbers} (winner=${c.v.rankWinner}, chroma=${c.v.chroma}, fp_ber=${c.v.fpBer}, speed=${c.v.speedRatio}, clip%=${c.v.clipPct}, verdict=${c.v.verdict})
Artifact: ${c.wav}

INDEPENDENT CHECK:
- \`python3 ${REPO}/scripts/native/audio_verify.py --selftest\` MUST be 6/6 (tool sanity).
- Re-run \`python3 ${REPO}/scripts/native/audio_verify.py ${c.wav}${c.section ? ` --rank ${SONGS} --section ${c.section}` : ' --ref /tmp/rb3_ref_20thcenturyboy_preview.wav'}\` and confirm the verdict/winner.
- \`python3 ${REPO}/scripts/native/audio_coherence.py ${c.wav}\` — real audio, not empty/garbage?
- Does it MATCH the native baseline? A web-only regression (clipping, wrong song,
  chipmunk, silence) is a REAL refutation.
Report refuted true/false with YOUR measured numbers. Reproducible-but-wrong = refuted.`

  refutes = await parallel(claims.map(c => () =>
    agent(refutePrompt(c), { schema: REFUTE, phase: 'Adversarial verify', label: `refute:${c.k}` })
      .then(r => ({ k: c.k, claim: c.text, r }))))
}
const survived = refutes.filter(Boolean).filter(x => x.r && !x.r.refuted).map(x => x.k)
log(`survived adversarial verify: ${survived.join(', ') || '(none)'}`)

// ---- Phase 4: synthesize ----
phase('Synthesize')
const summary = await agent(`${CONTEXT}

Synthesize the WEB audio verification into a findings doc. Inputs:
ESTABLISH: ${JSON.stringify(establish)}
CAPTURE RESULTS: ${JSON.stringify(C)}
ADVERSARIAL: ${JSON.stringify(refutes)}
SURVIVED: ${JSON.stringify(survived)}

Write ${REPO}/docs/native/audio-perf-loop/WEB_AUDIO_VERIFY_2026-06-08.md with:
- HEADLINE verdict: is the WEB build audio CORRECT (right song, not clipped, right
  rate) and does it MATCH native? If boot blocked capture, say so plainly and make
  the boot blocker the headline.
- web-vs-native comparison table (preview + song: chroma, fp_ber, clip%, verdict,
  rank winner+margin; rate: tone Hz, ctx sample rate). Mark UNKNOWN where blocked.
- which claims SURVIVED adversarial verify (${survived.join(', ') || 'none'}) vs refuted, with deciding numbers.
- the web boot status (slow on-demand-fetch / Rand.cpp MainThread asserts / furthest
  screen) and concrete NEXT STEPS to make web audio fully verifiable + correct.
- 'how to re-run' (the /audio-verify skill + the web capture commands that worked).
Number-driven, tight. Then return a 4-6 line verdict summary.`, { phase: 'Synthesize', label: 'synthesize' })

return { establish, capture: C, adversarial: refutes, survived, summary }
