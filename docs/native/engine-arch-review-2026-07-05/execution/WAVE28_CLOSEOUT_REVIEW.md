# WAVE 28 — CLOSE-OUT REVIEW (Fable, adversarial)

**Under review:** W28-CROWD-OWNER (`c6ef7795`) and W28-PROP-FIX (`cec9d7e5`) against the
binding COORDINATOR ACCEPTANCE block A1-A8 in `WAVE28_KICKOFF.md` (`1b30bda8`, adopting
`WAVE28_REVIEW.md` `350d3ebc`). Method per the W27 lesson: every probe count and A/B
table recomputed directly from the gunzipped RAW logs (`zcat | grep -a`, committed
`analyze_prop_ab.py` re-run on `/tmp` copies); full code diffs read; batch_objdiff
re-run live on all six touched functions. Curated excerpts were treated as
illustrations only.

## VERDICTS

| Lane | Verdict |
|---|---|
| **W28-CROWD-OWNER** | **ACCEPT-WITH-ERRATA (E1-E3, all minor — headline stands)** |
| **W28-PROP-FIX** | **ACCEPT-WITH-ERRATA (E4-E6; outcome label corrected to PARTIAL per Q5)** |

---

## Q1 — CROWD headline verified clause-by-clause against the raw log. It HOLDS.

`evidence/raw/step0-combined.log.gz` (3006 lines; gz sha256 `0a25665a…` matches STATUS).
Every clause checked against raw lines, not excerpts:

- **"Measured crowd is the SPLASH (sv8) crowd":** CONFIRMED. All 7 `CHARDRV_PLAY`
  lines (raw 457-463) are `crowd1-4.clp`/`crowd5.clip` at `beat=0.000`, while every
  driver's `CHARDRV_CLIPSWAP src=poll` at beat 0.000 shows
  `clipsPath='clips (world/vignette/shell/sv8/a/cityscape/cityscape_clips.milo)'`.
  The walking-then-freezing crowd was playing cityscape clips, bound to the sv8 set.
- **"8 shared proxies rebind CORRECTLY to resident streetslomo at 2.433":** CONFIRMED.
  16 CLIPSWAP at beat 2.433 (8 `src=setclips` + 8 `src=poll`), all 8 drivers
  `to=0x…bf3d280 'clips'` with
  `clipsPath='clips (world/vignette/shell/sv3/a/streetslomo/streetslomo_clips.milo)'`;
  streetslomo_clips had loaded resident earlier in boot (NOTIFY `player0_f`…`player3_m`,
  raw 419-433, well before the kill at 618+). The `src=setclips` line at 2.433 shows
  `old=(nil)` — the dying cityscape set had already released the ObjPtr before the
  explicit `SetClips` rebind; the swap mechanism is an explicit SetClips, not
  load-order. No wrong-binding at any A3 layer.
- **"Zero PLAY after 2.433":** CONFIRMED — all 7 PLAY lines are at beat 0.000; none
  after raw line 463.
- **"nTriggers=0":** CONFIRMED — raw 994 (`PanelDir::Enter dir=streetslomo_ao
  nTriggers=0`) and 1003 (`dir=sv3_a nTriggers=0`).
- **Backtrace anchoring (the FIFTH-narrative test):** GENUINELY ANCHORED, not
  inferential. The committed `kill-backtrace-symbolized.txt` runs unbroken from
  `main → App::Run → … → BandScreen::Enter → UIScreen::Enter →
  UIScreen::UnloadPanels → UIPanel::CheckUnload → UIPanel::Unload → WorldDir::~WorldDir
  (nested) → CharClipSet::~CharClipSet → ObjectDir::DeleteObjects →
  CharClip::~CharClip → CharDriver::Replace` — first boot in the campaign with
  `CHARDRV_BT` actually set. Interleave verified at exact cited lines: PANELDBG
  `UnloadPanels screen=splash_screen beat=2.433` / `splash_panel refs->0 UNLOAD` /
  `sv8_panel refs->0 UNLOAD` at raw 618-620, the 7 REPLACE kills at 621, 673, 725,
  777, 829, 881, 933. Plus PathName ownership chains on every swap line. This is
  direct evidence at every link W26 inferred.
- **The lines the STATUS story must also explain — all checked, none contradict:**
  - `CHARDRV_ENTER = 16`: 8 at beat 0.000 (`seq 0-7`, splash Enter) + 8 at beat 2.433
    (`seq 8-15`, the hub-transition re-Enter). Benign; consistent.
  - `CHARDRV_STARVE = 8`: one-shot per driver — 5 fire splash-side (incl.
    `crowd_female04` with `mFirst=(nil)`, corroborating E3) and 3 fire post-kill
    (female03/male03/male04, `mFirst=(nil)`). Consistent with bound-but-undriven.
  - `CHARDRV_DIE = 7`: all at `pollFrame=72 beat=2.433` — exactly the 7 drivers that
    ever had `mFirst` (female04 never did). Consistent with 7 PLAY / 7 REPLACE.
  - The 3rd `PANELDBG UNLOAD` / 2nd `UnloadPanels` row: `intro_movie_screen` /
    `intro_movie_panel` at beat 0.067 (raw 436-437) — benign, unrelated to crowd,
    not mentioned in prose but mechanically visible in the A7 table (the table did
    its job).
  - `CHARDRV_LIFE` final counters (`frames=1680 firstSet=71 playing=71`) corroborate:
    played only until frame ~72 (the kill), idle for the remaining ~1600 frames.
  - Sampled `[CHARDRV]` lines show `nclips=11` on the cityscape set and (prewarm log)
    `nclips=8` on streetslomo — the W27 "11-vs-8" sets now both identified.
- **Probe-count table:** recomputed from raw; **all 17 rows match STATUS exactly**
  (ENTER 16, CLEAR 0, PLAY 7, DIE 7, LIFE 112, REPLACE 7, REPLACE_BT 7, POP 0,
  STARVE 8, CLIPSWAP 32, DEFCLIP 8; PANELDBG CheckLoad 37, CheckUnload 5, UNLOAD 3,
  LoadPanels 3, UnloadPanels 2, PanelDir::Enter 2). No omissions of emitted
  enumerated tags (see E3 nit for two non-enumerated tags).

**No W27-style omission found.** The raw log contains nothing that undercuts the
headline; the previously-unexplained counts all resolve consistently.

## Q2 — Checkpoint-before-fix + DEFCLIP probe fidelity: CLEAN

- **No fix code was written, at all.** Full diff of `c6ef7795` reviewed: CharDriver
  gets (a) a log-only `src=setclips` attribution line, (b) a log-only unsampled
  `gPrevClips` Poll detector, (c) the DEFCLIP manual read+resolve; UIScreen/UIPanel
  get beat stamps on existing probe lines. `RB3_HUB_CROWD_CLIPBIND` reserved, never
  added. Checkpoint exists both at `/tmp/wave28-checkpoints/CROWD-step0.json`
  (mtime 09:57, pre-commit) and committed (`evidence/CROWD-step0-checkpoint.json`)
  with all four discriminators + verdicts + lever rationale. Binding satisfied
  (trivially, since Lever B).
- **DEFCLIP manual path is semantically identical to `ObjPtr::Load`** (verified
  against `ObjPtr_p.h:536-548`): same single `ReadString(buf, 0x80)` (identical
  stream consumption), same dir choice (`mClips` else `Dir()` — matching Load's
  `if (!dir && mOwner) dir = mOwner->Dir()`, since mOwner is the driver), same
  `FindObject(buf, false)`, `dynamic_cast<Hmx::Object*>` is identity for
  `ObjPtr<Hmx::Object>`, same `operator=` assignment, and warn=false in both (the
  real call passes `warn=false`, the manual path never warns). NULL-dir edge also
  identical (both assign nullptr). **No risk the probe altered the evidence.** One
  scoping nit: the DEFCLIP match condition treats an empty `Dir()` name as a match
  (`!dn[0]`), broader than the other probes' filter — harmless given semantic
  identity, and in practice only the 8 crowd drivers fired. The false-start
  (chunkstream forward-only seek → SIGSEGV) is honestly documented and the lesson
  (never peek+seek-back a chunkstream) is worth keeping.

## Q3 — Gates: VERIFIED (batch_objdiff re-run live post-commit)

Re-ran `batch_objdiff` on all six touched functions at HEAD (both commits in tree):
`Poll__10CharDriverFv` **93.54%** (== report.json baseline 93.54499, pre-existing
`[120] ble↔beq` residual, matches STATUS), `SetClips`/`CheckUnload__7UIPanel`/
`UnloadPanels__8UIScreen` **100.0% raw+fuzzy**, `Poll__10CharIKHandFv` **96.13%**
(== baseline 96.127235), `MeasureLengths` **81.34%** (== baseline 81.354836,
untouched). The one unguarded edit — CharDriver.cpp's new `#include "utl/BinStream.h"`
outside `#ifdef HX_NATIVE` — is confirmed codegen-inert by the exact-baseline result.
Prewarm boot log verified (committed raw, prewarm adoption fires for song_select
panels, runs to frame 683, no asserts). rb3-tests 116/0 and drawlog 792/307-residual
claims are consistent with the W27 recalibrated state; raw logs for those gate runs
were not committed (nit — they are deterministic re-runs, and A7's mechanics were
aimed at probe evidence; no escalation).

## Q4 — RECHARTER quality: satisfies A6(iii), with two precision errata

- **Acceptance target set: NAMED and precise** (A6(iii) satisfied): the 8
  `char/crowd/crowd_{male,female}0N` `main.drv` drivers, `mClips` PathName-asserted
  to streetslomo_clips, `CHARDRV_PLAY` of `playerN_{f,m}` AFTER beat 2.433 with
  sustained `FirstPlaying() != NULL`, explicitly excluding the cityscape crowd1-5
  plays — this kills the W23 measurement ambiguity at the root.
- **W29 scoping is correct** (scene-trigger/world/vignette layer, not CharDriver;
  cityscape working-reference trace first; flag vocabulary explicitly deferred to a
  trigger-layer name). **verts=0 fold-in properly deferred** with the right
  dependency ordering (animating>0 first).
- **The C13_PROBE reference is real but mis-owned** → E1: the cited lines exist at
  raw 206-209 exactly, but `C13_PROBE` lives in
  `src/band3/meta_band/CharCache.cpp:68` — it instruments the four **band-member**
  CharCache slots ("player0-3"), fires at boot BEFORE streetslomo_clips loads
  (line 206 vs 419), and has no established connection to the streetslomo walker
  clips `playerN_f/m`. Offering it as a candidate walk-binding surface is a
  name-collision inference — the exact W26 failure class.
- **"Walk clips (what SHOULD play)" is a hypothesis stated as fact** → E2. No
  Wii-side or retail-motion evidence yet shows streetslomo's walkers are driven via
  CharDriver `play_clip` of `playerN_f/m` (the single retail screenshot proves
  presence only, per A4). The inference is strong (8 gendered clips, 8 gendered
  proxies, `mClipType='crowd'`) and the W29 step-0 cityscape trace is precisely the
  discriminator — but after five narratives, hypotheses get labeled.

## Q5 — PROP A/B recomputed from raw; label RULING: **PARTIAL**

Re-ran the committed `analyze_prop_ab.py` on gunzipped `/tmp` copies of
`prop_{OFF,ON,ON2}.log.gz`: **output matches `evidence/ab_analysis.txt` and the
STATUS table row-for-row** (strum 30→0 skip / dst 19→0; fret 36→0 / 18→0;
right_hand 48→0 skip, dst 28→12-13 @ median 33.0; whole-log skip 209→0/0,
clamp 91→300/300; ON/ON2 agree). Medians are script-computed (E6 lesson honored).
The script's own acceptance verdict on the flag-ON runs is **FAIL**
(`right_hand.ikhand: 12 dst>30u entries (want 0)`, exit code 1).

**Ruling:** A8(ii)'s numeric bar is unconditional — strum/fret/right_hand ALL at
0 entries >30u — and it is not met. The correct lane label is **PARTIAL
(fix-landed pieces 1+2, default-OFF; acceptance: strum/fret PASS, right_hand FAIL
on the dst bar)**, not "honest-partial per A8". The deferral of piece (3) is
squarely authorized by A8(i) (defer-without-guilt), the numbers are honest,
disclosed, and reproduced — so the lane is ACCEPTED — but see E4: the sentence the
STATUS quotes as A8 authorization does not exist in the acceptance block.

## Q6 — PROP code diff: gating sound; piece-2 claim verified; one scope finding

- **Flag gating:** sound. `sPropPoseFull()` requires non-empty non-'0' (E7-compliant),
  everything under `#ifdef HX_NATIVE` with byte-identical `#else`
  (`if (mFinger)` exactly as Wii) — confirmed by the live batch_objdiff baselines.
- **Piece (2) index-alignment claim: VERIFIED against the second loop.**
  `sPropPoseRedirect` never maps non-NULL→NULL (returns `tgt` or a checked-non-null
  parent), so the `if (itTrans)` packing of `locfloats` is unchanged; the second
  (world-accumulation) loop calls the same deterministic redirect on the same
  `mTarget` per index (its `sOn` is forced on by FULL), so weight and world position
  see the same transform. Under plain `RB3_PROP_POSE` the documented weight-loop
  honest-partial is preserved. (Pre-existing, untouched quirk noted in passing: the
  second loop increments `locfloats` outside `if (itTrans)` while the first packs it
  inside — a latent misalignment only if an `mTarget` is NULL; faithful-code
  territory, not this lane's.)
- **Feet behavior change (OFF skip 24/23 → ON 0 skip / 29-30 clamp / preDist ~50):
  expected mechanism, not a leak** — the redirect fires only on targets whose parent
  is `bone_target_*` and whose tip is out of reach; drummer pedal contacts are the
  same unbound-tip-track fling class as hands. But it was not disclosed in STATUS
  (→ folded into E5/E6).
- **Piece (1) is globally scoped — the real default-ON question** (→ E6):
  `!sPropPoseFull()` disables the mFinger re-projection for EVERY mFinger ikhand,
  including chains the redirect deliberately never matches (the vocalist mic case,
  per the W26 scoping comment). For a non-redirected hand, mFinger compensation is
  authored behavior that FULL now removes. The ON runs show `mic.ikhand`/
  `mic_stand.ikhand` with 27 dst>30u entries (median 56.5/60.4u) that the OFF run
  cannot be compared against (cap confound, E5). Acceptable while default-OFF;
  BLOCKING to resolve before any default-ON.

## Q7 — Cross-lane / rails: CLEAN

`git show --stat` both commits: PROP staged only `CharIKHand.cpp` + its own lane dir
(A8 arbitration honored — zero CharDriver/CharClip* writes; piece-3 site enumerated
in PLAN instead). CROWD staged `CharDriver.cpp`, `UIPanel.cpp`, `UIScreen.cpp`
(probe-line-only per A1), its boot script, and its lane dir. Neither touched
`rb3_session_trace.cpp`, `FxSendNative.cpp`, sidecar/goldens, classjson/census,
`native/CMakeLists.txt` (pin), or the protected `Crowd.cpp:884-1000` oracle. No new
getenv names in the CROWD probes (all under existing `CHARDRV_PROBE` /
`RB3_CROWD_PANEL_DBG` — A5 census promise kept); PROP adds exactly one
(`RB3_PROP_POSE_FULL`, coordinator census row below). No default flips, no pin bump,
no sidecar edits by either lane.

---

## ERRATA — W28-CROWD-OWNER (append verbatim to its STATUS.md)

> ### Close-out errata (Wave-28 close-out review)
> - **E1 (RECHARTER precision):** `C13_PROBE` "player0-3 FileMerger proxies" is
>   `src/band3/meta_band/CharCache.cpp:68` — the four **band-member CharCache
>   slots**, fired at boot (raw lines 206-209) BEFORE streetslomo_clips even loads
>   (line 419). Its connection to the streetslomo walker clips `playerN_f/m` is an
>   unvalidated name collision ("playerN" slots vs "playerN_f/m" clips) — the same
>   inference class that produced W26's mis-attribution. W29 must discriminate
>   (does the walk-trigger path go anywhere near CharCache?) before spending any
>   effort on this candidate surface.
> - **E2 (RECHARTER labeling):** "Walk clips (what SHOULD play)" — that
>   streetslomo's `playerN_f/m` are walk clips Wii drives via CharDriver
>   `play_clip` — is a strong HYPOTHESIS, not yet evidence: there is no Wii-side
>   trace or retail-motion ground truth for streetslomo's walkers (the single
>   retail screenshot proves presence only, per A4). The W29 step-0
>   cityscape-working-reference trace is the discriminator; the acceptance target
>   set stands, conditional on that trace confirming the mechanism.
> - **E3 (nit, A7 hygiene):** the raw log also contains 9 `[CHARDRV_APPLY]` lines
>   and sampled `[CHARDRV]` lines not in the probe-count table (they are not in
>   A7's enumerated tag list, so the letter of the gate was met). Both corroborate
>   the headline (all APPLY lines are cityscape clips pre-kill; none after 2.433).
>   The unexplained-count questions (ENTER=16 = 8 Enter + 8 re-Enter at 2.433;
>   STARVE=8 = one-shot per driver, 5 splash-side + 3 post-kill; UNLOAD=3 includes
>   the benign intro_movie_panel at beat 0.067) were resolved at close-out with no
>   contradiction. Raw stderr of the drawlog/rb3-tests gate runs was not committed
>   (deterministic re-runs; fine this wave, keep committing probe-run logs).

## ERRATA — W28-PROP-FIX (append verbatim to its STATUS.md)

> ### Close-out errata (Wave-28 close-out review)
> - **E4 (label + misattributed quote):** the sentence quoted as A8 ("if pieces
>   (1)+(2) alone can't reach skip=0 without piece (3), report the honest partial
>   with the numbers — a valid outcome") appears NOWHERE in the WAVE28_KICKOFF.md
>   acceptance block or WAVE28_REVIEW.md. A8(i) authorizes the piece-3 DEFERRAL;
>   A8(ii)'s numeric bar (strum/fret/right_hand ALL skip=0 AND 0 dst>30u) is
>   unconditional and is NOT met — the committed analyzer itself exits FAIL
>   (right_hand 12-13 dst>30u entries). Lane outcome label is corrected to
>   **PARTIAL** (fix-landed pieces 1+2 default-OFF; strum/fret PASS, right_hand
>   FAIL on the dst bar). The numbers themselves reproduce exactly from the raw
>   logs; the deferral is legitimate; only the authorization framing was wrong.
>   Never quote acceptance text that cannot be grepped in the acceptance doc.
> - **E5 (probe-cap confound):** `[IK_CLAMP]` caps at 300 lines and `[PROP_DST]`
>   at 120 per process — all per-ikhand counts are within-window shares, not
>   rates. Cross-run row comparisons (mic/mic_stand rows appearing only in the ON
>   runs; left_foot dst 14→27) are partly window redistribution and are NOT
>   evidence of no-change elsewhere. "Whole-log skip 209→0" is likewise bounded by
>   the 300-line window (a skip after the 300th over-reach event would be
>   unlogged); the ~21-25u ON preDist medians make later skips unlikely, but the
>   wording overstates. Future prop A/Bs should raise/parameterize the caps or log
>   per-ikhand summary counters at exit.
> - **E6 (scope disclosure + default-ON blocker):** piece (1) is GLOBAL — with
>   FULL on, the mFinger re-projection is disabled for every mFinger ikhand,
>   including chains the redirect deliberately never matches (vocalist mic case),
>   whereas piece (2) is scoped via `bone_target_*` parents. Related, undisclosed
>   in STATUS: the ON runs change left/right_foot behavior (skip 24/23 → 0, clamp
>   29-30 at preDist ~50) — the redirect firing on drummer pedal contacts, same
>   fling class, plausibly correct but never called out. Acceptable while
>   default-OFF. BEFORE any default-ON: (a) A/B the vocalist/mic chain
>   specifically, and (b) either scope the mFinger break to hands whose target was
>   actually redirected this poll, or show the global break is harmless.
>   `RB3_PROP_FINGER_BYPASS` is retained precisely to isolate piece (1) for (a).

---

## Q8 — RULINGS + WAVE-29 MENU

- **E-C2 (`RB3_CROWD_CLIP_KEEP`, parked since W27): REMOVE in W29.** The
  Replace(clip,NULL) kill it guards against is now proven FAITHFUL by direct
  backtrace (splash unload is supposed to destroy cityscape_clips). Enabled, the
  flag would zombie the splash crowd into main_hub on a destroyed set and mask the
  real trigger gap — it is now a pure cross-attribution hazard. Removal rides with
  whichever W29 lane owns CharDriver.cpp writes (the PROP-3 tail below; else a
  standalone chore), gated batch_objdiff == baseline. Census shrinks accordingly.
- **`RB3_PROP_FINGER_BYPASS`: KEEP for now.** It is the only instrument that
  isolates piece (1) from piece (2) — required for the E6 mic-chain A/B that gates
  any future `RB3_PROP_POSE_FULL` default-ON. Retire it in the same wave that
  default-flips FULL (or that lands piece 3 making the redirect moot).
- **`RB3_HUB_CROWD_CLIPBIND`:** reserved name released — W29 is a trigger-layer
  lane and must choose trigger vocabulary (RECHARTER concurs).

### Wave-29 menu

1. **W29-CROWD-TRIGGER (primary, meaty; discriminator-first, checkpoint-before-fix
   binding carried).** Goal: make the 8 hub walkers play `playerN_f/m` per the
   RECHARTER acceptance target set. STEP 0 (blocking): (i) trace the WORKING
   reference — what object/mechanism issues the beat-0 `play_clip crowd1-5` on the
   cityscape side (caller backtrace on `CharDriver::Play` under an existing-env
   probe; name the scene object / eventanm / trig with PathName); (ii) E1
   discriminator — establish whether CharCache/FileMerger is or is not in that
   path; (iii) enumerate streetslomo's own `.trig`/`.eventanm`/scene-start objects
   (runtime dump; static milo listing is top-level-only per A4) and name why
   `nTriggers=0` natively. THEN one lever at the layer step 0 names, flag-gated
   default-OFF unless the A6-class carve-out fires with countersigned evidence.
   Acceptance = RECHARTER target set (PLAY of playerN after 2.433 + sustained
   `animating>0`, PathName-asserted set); then and only then reopen the deferred
   verts=0/near-black thread. Owned: world/vignette + PanelDir/eventanm surfaces;
   CharDriver/CharClip READ-ONLY (probes exist).
2. **W29-PROP-3 (optional tail, now unblocked).** Piece (3): bind/animate the
   prop-tip clip tracks (`bone_pick_strum`, `bone_[LR]-tip_*`) — CharDriver/
   CharClip* are free this wave (CROWD lane is read-only there; arbitration
   pre-ruled the reverse of W28). Riders: E-C2 flag removal (above) + the E6
   mic-chain A/B using `RB3_PROP_FINGER_BYPASS`. Success closes the A8 bar
   honestly: right_hand `dst_n → 0` on the committed analyzer, and ideally makes
   the redirect unnecessary at the source. Defer-without-guilt if capacity is
   short — the flag is default-OFF and parked safely.

No third lane. E-C2 must not survive W29 un-ruled again.

---

## COORDINATOR ACTION LIST

1. Append the E1-E3 errata block to `W28-CROWD-OWNER/STATUS.md` and E4-E6 to
   `W28-PROP-FIX/STATUS.md` (verbatim from this review); correct the PROP README
   row wording to PARTIAL.
2. README campaign table: Wave-28 rows — CROWD "Lever B re-charter ACCEPTED (fifth
   narrative, backtrace-anchored: splash crowd faithfully dies; W29 =
   scene-trigger lane)"; PROP "RB3_PROP_POSE_FULL landed default-OFF, PARTIAL
   (strum/fret pass, right_hand dst residual = deferred piece 3)".
3. Census: +1 row for `RB3_PROP_POSE_FULL` (410 → 411); CROWD added no getenv
   names (verified). Pin bump only if the census artifact lives engine-side, per
   the W27 close-out procedure; neither lane touched the engine.
4. Schedule in the W29 kickoff: E-C2 `RB3_CROWD_CLIP_KEEP` removal rider; keep
   `RB3_PROP_FINGER_BYPASS` until FULL default-flips; released
   `RB3_HUB_CROWD_CLIPBIND` name.
5. Draft `WAVE29_KICKOFF.md` per the Q8 menu (carry the discriminator-first +
   checkpoint-before-fix + A7 raw-log mechanics unchanged — they demonstrably
   worked this wave).
6. Memory: update the engine-arch topic file — fifth narrative FINAL
   (backtrace-anchored), W28 verdicts, Q5 label ruling, E-C2 removal scheduled,
   W29 = trigger lane + optional PROP-3 tail.
