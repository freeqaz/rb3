# W25-CROWD — PLAN

Lane: CROWD (hub crowd-proxy streetslomo clip-trigger fix). Parent charters:
`WAVE25_KICKOFF.md`, `WAVE25_REVIEW.md` (A1–A9 binding), `W24-RECON/REPORT.md`.

## A8 FILE-OWNERSHIP DECLARATION

This lane owns and will modify:
- `src/system/char/CharDriver.cpp` — probe extensions (STEP 0) + the branch-scoped,
  flag-gated, `clipType=='crowd'`-scoped fix (A7-compliant: byte-identical `#else`,
  no un-scoped shared behavior change). CROWD owns this file per A8.
- `scripts/native/_w25_crowd_trace.py` — new STEP-0 trace harness (native-src/scripts).
- `docs/native/engine-arch-review-2026-07-05/execution/W25-CROWD/**` — PLAN/STATUS/evidence.

This lane will NOT touch: `src/system/char/CharIKHand.cpp` / `bandobj/` (FOREARM),
`src/system/rndobj/Mesh.cpp` (proven-correct loader), `src/system/world/Crowd.cpp`
(protected WorldCrowd/RndMultiMesh oracle). Never stages `rb3_session_trace.cpp` or
engine `FxSendNative.cpp`.

## STEP 0 — DISCRIMINATOR (verdict checkpointed BEFORE fix code)

Extended `CHARDRV_PROBE` (CharDriver.cpp) to print `mDefaultClip`/`mClips`/`nclips`,
added Enter/Play/Starve/Pop/Clear/Replace/Die probes (all `#ifdef HX_NATIVE` +
env-gated, inert by default). Harness `_w25_crowd_trace.py` boots to `main_hub` with
`CHARDRV_PROBE=crowd`.

### Verdict: async load-completion `Replace(clip, NULL)` destroys the live walk clip

Mechanism (all evidence in `evidence/`):
1. Load-time `CharDriver::Enter` on 8 crowd drivers: `defClip=(nil)` → no auto-play.
2. Vignette `vignette_start.trig` sends `play_clip` ONCE at beat 0 → 7/8 drivers
   `Play crowdN.clp` with flags `0x222` = `kPlayNoBlend|kPlayLoop(0x20)|kPlayRealTime(0x200)`
   (a self-looping real-time walk clip). `crowd_female04` never gets the message.
3. At **beat 2.433** (pollFrame 72), an async load-merge **destroys** the playing
   `crowdN.clp` objects → `Hmx::Object::~Object` fires `Replace(this, NULL)` to every
   referencer → `CharDriver::Replace` runs `mFirst = mFirst->DeleteClip(from)` →
   `Exit(false)` pops the clip → `mFirst` becomes NULL. Confirmed: `[CHARDRV_REPLACE]
   from='crowdN.clp' to='?'(null) beat=2.433` on all 7, `[CHARDRV_DIE] pollFrame=72
   beat=2.433` on all 7. (NOT PreEvaluate-pop: `[CHARDRV_POP]` never fires. NOT the
   second Enter's Clear: `mFirstAtEntry=(nil)` — the clip was already gone.)
4. No re-trigger exists: `defClip=(nil)` (Enter no-op), `mStarvedHandler` Null,
   and `kPlayLoop(0x20)` is NOT one of the starved-replay branches (only `0x30`/`0x40`).
   `Starved()` returns true forever, `mFirst` NULL forever → census `animating=0` →
   undriven skin palette scrambles.

The `mClips` bank (`male_base`/`female_base`, `nclips=8/11`, incl. the `realtime_idle`
CharClipGroup) SURVIVES the merge — only the playing clip instance is destroyed. So a
re-play from the surviving bank is viable.

Scope discriminator: in the hub, `clipType=='crowd'` is UNIQUE to the 8 crowd proxies
(band `player0-3` + `*_extras*` use `clipType=='vignette'`). Gameplay WorldCrowd renders
via `RndMultiMesh` with NO CharDriver → the fix cannot reach the protected oracle (A7).

NOT a hand-off (A5): the whole vignette Enter/flow machinery IS present natively
(`Enter` fires, `play_clip` fires) — only the post-merge re-establishment is missing.

## THE FIX (flag-gated default-OFF, `clipType=='crowd'`-scoped, byte-identical `#else`)

In `CharDriver::Poll`, behind `RB3_CROWD_CLIP_KEEP` (default OFF) and scoped to
`ClipType()==Symbol("crowd")`: when the crowd driver is `Starved()` with `mFirst==NULL`
and a non-empty `mClips` bank, re-`Play` the ambient `realtime_idle` group (the
canonical crowd idle loop) with `kPlayLoop|kPlayRealTime` flags — re-establishing the
walk/idle loop that the async merge destroyed. Guarded so it re-arms at most once per
starvation gap (re-fires only when `mFirst` is NULL). A7: no un-scoped autoplay; the
`#else` compiles the exact pre-existing Poll body byte-for-byte for the Wii target.

Rationale for re-play (vs. prevent-loss): the clip object is genuinely destroyed by the
engine merge; holding a ref would be invasive shared-engine surgery. Re-establishing an
equivalent ambient loop from the surviving bank is minimal, robust to *why* the clip
died, and matches the intended "crowd keeps walking" behavior.

## GATES (recon acceptance)

- `{rb3_crowd_census}` `animating > 0` (flag ON).
- `RB3_ISOLATE_MESH=crowd_body` shows LIT standing figures; max-pixel vs recon 17/255
  (A6 material-deferral discriminator).
- MANDATORY WorldCrowd A/B **flag-ON vs baseline** — gameplay crowd draw counts +
  screenshot SSIM UNCHANGED (protected oracle).
- Flag-OFF `drawlog-golden.py --fixed-clock --canonical-order` = 792 byte-identical.
- `batch_objdiff == baseline` on `char/CharDriver` (native-only fix → exact equality).
- `rb3-tests` 116/0 (A9).

## DELIVERABLES

PLAN.md (this) · STATUS.md (verdict headline) · evidence/ (probe dumps, trace,
before/after isolate + max-pixel, hub walkers, WorldCrowd A/B) · code default-OFF ·
checkpoint (discriminator verdict early).
