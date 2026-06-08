export const meta = {
  name: 'samemix-ratedrift',
  description: 'Tighten the audio verifier: build a SAME-MIX reference (apply the game limiter to the downmix so fidelity ceiling rises) and a SAMPLE-ACCURATE rate/drift proof (bound native rate error < 0.1%), validate, adversarially verify, document',
  phases: [
    { title: 'Build', detail: 'same-mix reference (decode_reference --same-mix) + rate/drift tool (audio_drift.py) in parallel' },
    { title: 'Adversarial verify', detail: 'refute the fidelity gain + the rate bound, default to refuted' },
    { title: 'Synthesize', detail: 'findings doc + commit-ready tools' },
  ],
}

const REPO = '/home/free/code/milohax/rb3'
const ENGINE = '/home/free/code/milohax/milo-native-engine'
const DOC = `${REPO}/docs/native/audio-perf-loop/wave-09-samemix-rate.md`

const COMMON = `You are HARDENING the RB3 audio verifier (scripts/native/audio_verify.py,
committed 7d02e80e, with the /audio-verify skill). Repo: ${REPO}. Do NOT rebuild
native (the binary native/build-native/rb3-native already exists); reuse the existing
native captures where possible:
  /tmp/verify_preview.wav      (~70s, song-select preview of the default song 20thcenturyboy)
  /tmp/verify_gameplay45.wav   (~45s, in-game gameplay of 20thcenturyboy)
Tools you build on: scripts/native/{decode_reference,decrypt_mogg,audio_verify,capture_gameplay_audio,song-preview-audio-test}.py.

CONTEXT — why this work: audio_verify's identity ceiling is only MODERATE (true song
chroma ~0.55, fp_ber ~0.31) because the ground-truth reference (decode_reference.py)
is an independent NO-CLAMP stem downmix, NOT the game's actual post-limiter mix. And
'rate is correct' currently rests on a FLAT speed-curve argument (no chipmunk peak),
not a tight numeric bound. This run fixes BOTH.

A concurrent run is EXTRACTING ~80 more song moggs (symlinks). Use 20thcenturyboy /
25or6to4 / antibodies (guaranteed present) for validation; if more songs appear under
${REPO}/orig-assets/extracted/songs/<id>/<id>.mogg you may use them too.`

const VERDICT = {
  type: 'object',
  properties: {
    task: { type: 'string' },
    status: { type: 'string', enum: ['done', 'partial', 'blocked'] },
    toolPath: { type: 'string', description: 'the script/flag that was added' },
    numbers: { type: 'string', description: 'the decisive before/after measurements' },
    improvement: { type: 'string', description: 'quantified change vs the prior baseline' },
    evidence: { type: 'string' },
    notes: { type: 'string' },
  },
  required: ['task', 'status', 'numbers', 'evidence'],
}

const REFUTE = {
  type: 'object',
  properties: { refuted: { type: 'boolean' }, why: { type: 'string' }, numbers: { type: 'string' } },
  required: ['refuted', 'why'],
}

// ---- Build phase: two independent tools in parallel ----
phase('Build')

const sameMixPrompt = `${COMMON}

TASK A — SAME-MIX REFERENCE. Make decode_reference.py able to emit the game's ACTUAL
post-limiter mix, so audio_verify's chroma/fingerprint ceiling rises toward a STRONG
match (not just a moderate rank).

1. Read the EXACT master-bus DSP in ${ENGINE}/src/audio/AudioDevice.cpp (the
   PumpAudio / mix loop around lines 399-445, plus the constants near lines 33-51):
     - sPreGain = 1.0 (default; DC3_AUDIO_GAIN override)
     - one-pole STEREO-LINKED peak limiter: kLimThreshold = 0.90, kLimReleaseMs = 80,
       a fast attack (find kLimAttack* — the comment notes a few-ms attack), envelope
       member mLimiterEnv (reset to 1.0 on resume)
     - soft-knee saturator: kSoftKnee = 0.95 (the safety net above the knee)
   Port this chain FAITHFULLY to Python (per-sample: linked peak -> target gain
   reduction -> one-pole attack/release smoothing -> apply -> soft-knee saturate).
2. Add a \`--same-mix\` flag to ${REPO}/scripts/native/decode_reference.py that, AFTER
   the existing pan/vol downmix, applies sPreGain + the limiter + soft-knee, and
   writes the result as the reference WAV (keep the no-clamp path as default; add a
   clearly-named same-mix output). Match the native sample rate.
3. VALIDATE the gain: build BOTH references for 20thcenturyboy (preview + gameplay)
   and run audio_verify against the existing captures:
     no-clamp:  python3 scripts/native/audio_verify.py /tmp/verify_preview.wav --ref <no-clamp preview ref>
     same-mix:  python3 scripts/native/audio_verify.py /tmp/verify_preview.wav --ref <same-mix preview ref>
   (and the gameplay capture /tmp/verify_gameplay45.wav similarly). Report chroma +
   fp_ber for BOTH references. EXPECT the same-mix reference to RAISE chroma and LOWER
   fp_ber (esp. preview, which has no crowd/SFX). Confirm audio_verify still verdicts
   MATCH and the selftest is still 6/6.
APPEND a \`### same-mix reference\` section (the DSP you ported + before/after numbers)
to ${DOC}, then return the schema (task="same-mix"; numbers = chroma/fp_ber no-clamp
vs same-mix for preview AND gameplay; improvement = the delta).`

const driftPrompt = `${COMMON}

TASK B — SAMPLE-ACCURATE RATE / DRIFT PROOF. Build a tool that bounds the native
playback rate error to < 0.1% (far tighter than audio_verify's flat-speed-curve
argument), by measuring TIME DRIFT between the start and end of a capture.

1. Create ${REPO}/scripts/native/audio_drift.py. Given a capture WAV + a reference
   (a song id -> build via decode_reference, OR a --ref WAV):
     - decode/trim both; pick an EARLY window and a LATE window of the capture (each a
       few seconds), well separated in time.
     - For each window, find its precise offset within the reference via cross-
       correlation (use a robust feature: onset-strength envelope or chroma, like
       audio_verify; reuse audio_verify's functions by importing them if convenient).
     - speed_ratio = 1 + (offset_late - offset_early) / (capture_time_late - capture_time_early).
       A correct rate gives drift ~0 -> ratio 1.000; report the ratio, the per-window
       offsets, the elapsed span, and a +/- bound (precision = feature frame period /
       elapsed span). Aim to RESOLVE rate to < 0.1%.
     - Include a --selftest with synthetic cases: exact-rate (ratio 1.000) and a known
       1.05x stretch (ratio ~1.05) — PROVE the tool recovers the injected rate before
       trusting it on real audio.
2. RUN it on a native capture. /tmp/verify_gameplay45.wav (45s) may suffice; if a
   longer baseline tightens the bound, capture one:
   \`python3 scripts/native/capture_gameplay_audio.py /tmp/drift_gameplay.wav --secs 110\`
   (headless; fine to run concurrently). Report the measured native rate ratio + bound.
   EXPECT ~1.000 within < 0.1% (native miniaudio resamples 44100 correctly; no chipmunk).
APPEND a \`### rate/drift proof\` section (tool design + selftest + measured native
bound) to ${DOC}, then return the schema (task="rate-drift"; numbers = measured ratio
+/- bound + the selftest result).`

const builds = await parallel([
  () => agent(sameMixPrompt, { schema: VERDICT, phase: 'Build', label: 'build:same-mix' }),
  () => agent(driftPrompt, { schema: VERDICT, phase: 'Build', label: 'build:rate-drift' }),
])
const [sameMix, drift] = builds
log(`built: same-mix=${sameMix?.status} (${sameMix?.improvement || '?'}) | rate-drift=${drift?.status} (${drift?.numbers || '?'})`)

// ---- Adversarial verify ----
phase('Adversarial verify')
const claims = [
  sameMix && sameMix.status !== 'blocked' && {
    k: 'same-mix', text: 'the SAME-MIX reference materially raises the fidelity confidence (higher chroma / lower fp_ber) and the port faithfully matches the C++ limiter',
    v: sameMix,
    how: `Re-build BOTH references for 20thcenturyboy and re-run audio_verify on /tmp/verify_preview.wav and /tmp/verify_gameplay45.wav for each; confirm same-mix chroma > no-clamp chroma and fp_ber lower. Read ${ENGINE}/src/audio/AudioDevice.cpp and check the Python port matches the DSP (threshold 0.90, release 80ms, attack, soft-knee 0.95, stereo-linked, pre-gain). audio_verify --selftest MUST still be 6/6.`,
  },
  drift && drift.status !== 'blocked' && {
    k: 'rate-drift', text: 'audio_drift.py correctly bounds the native playback rate to ~1.000 within < 0.1% (and provably recovers an injected rate)',
    v: drift,
    how: `Run audio_drift.py --selftest yourself: it MUST recover an injected 1.05x stretch as ~1.05 and an exact-rate case as ~1.000 — if it cannot recover a KNOWN rate, the native measurement is meaningless (REFUTE). Then re-run it on the native capture and confirm the ratio + bound.`,
  },
].filter(Boolean)

const refutePrompt = (c) => `${COMMON}

You are an adversarial SKEPTIC. Try to REFUTE this claim. Default refuted=true if you
cannot independently confirm it with NUMBERS. Reproducible-but-wrong still counts as
refuted. Do NOT rebuild native.

CLAIM: ${c.text}
Reported: ${c.v.numbers} | improvement: ${c.v.improvement || 'n/a'} | tool: ${c.v.toolPath || '?'}
HOW TO CHECK INDEPENDENTLY: ${c.how}

Report refuted true/false with the numbers you measured.`

const refutes = claims.length ? await parallel(claims.map(c => () =>
  agent(refutePrompt(c), { schema: REFUTE, phase: 'Adversarial verify', label: `refute:${c.k}` })
    .then(r => ({ k: c.k, r })))) : []
const survived = refutes.filter(Boolean).filter(x => x.r && !x.r.refuted).map(x => x.k)
log(`survived: ${survived.join(', ') || '(none)'}`)

// ---- Synthesize ----
phase('Synthesize')
const summary = await agent(`${COMMON}

Synthesize this verifier-hardening sweep into ${REPO}/docs/native/audio-perf-loop/SAMEMIX_RATEDRIFT_2026-06-08.md. Inputs:
SAME-MIX: ${JSON.stringify(sameMix)}
RATE-DRIFT: ${JSON.stringify(drift)}
ADVERSARIAL: ${JSON.stringify(refutes)}  SURVIVED: ${JSON.stringify(survived)}

Cover, number-driven and tight:
- Same-mix reference: the ported DSP, the before/after chroma/fp_ber for preview +
  gameplay, and whether the verdict strengthens (LIKELY -> STRONG). New usage:
  \`decode_reference.py --same-mix\` + \`audio_verify --ref <same-mix>\`.
- Rate/drift proof: audio_drift.py design, the selftest recovery of a known rate, and
  the measured native rate ratio +/- bound (is it < 0.1%? no chipmunk confirmed?).
- Which claims SURVIVED adversarial verify (${survived.join(', ') || 'none'}) vs refuted, with deciding numbers.
- Whether decode_reference.py (--same-mix) and audio_drift.py are clean + commit-ready
  (note any follow-ups). Update the /audio-verify skill reference if usage changed.
Then return a 4-6 line summary verdict.`, { phase: 'Synthesize', label: 'synthesize' })

return { sameMix, drift, adversarial: refutes, survived, summary }
