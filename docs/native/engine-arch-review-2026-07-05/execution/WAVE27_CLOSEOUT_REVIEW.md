# WAVE 27 — Close-Out Review (Fable, adversarial post-wave audit)

**Scope:** W27-CROWD (`a1cf22f3`) and W27-PROP-PROBE (`6a07cc42`) against
`WAVE27_KICKOFF.md` (COORDINATOR ACCEPTANCE A1-A10), `WAVE27_REVIEW.md` (`cf94e168`),
W26-CROWD/STATUS.md, the committed evidence, and — decisively — the **raw probe logs
still on disk** (`/tmp/w27-crowd/*.log`, byte-checked against the committed excerpts).
Read-only probes + git archaeology only; no source edits, no builds.

## Verdicts

| Lane | Verdict |
|---|---|
| **W27-CROWD** | **ACCEPT-WITH-ERRATA** — the chartered STEP-0 answer (sv3_panel RESIDENT, refcount handshake correct, no in-grant ui lever) is TRUE and well-evidenced, and all process gates are honest. But the substituted root-cause narrative ("zero teardown / walk clip ends naturally / char-layer replay gap") is **FALSIFIED BY THE LANE'S OWN RAW LOGS** (E1): the W26 Replace-kill mechanism reproduces. Errata E1-E5 are headline-reversing on the mechanism (not on the residency result). |
| **W27-PROP-PROBE** | **ACCEPT-WITH-ERRATA (minor)** — (a) mFinger-bypass A/B verified end-to-end from committed logs (skip/clamp counts reproduce EXACTLY; conclusion stands); (b) NEGATIVE enumeration valid with one methodology softening. Errata E6-E7 are cosmetic. |

---

## Q1 — THE CONTRADICTION, adjudicated

**Ruling: BOTH W26 and W27 observed real events; W26's MECHANISM stands and is
REINSTATED; W26 mis-attributed the OWNER; W27's "natural end / zero teardown" reading
is an evidence-reading error. Confidence: HIGH.**

The single decisive fact this review found: the raw W27 probe logs contain the kill.
In **both** `/tmp/w27-crowd/chardrv-boot.log` (lines 543-549) and
`/tmp/w27-crowd/combined-boot.log` (lines 597-603), at beat 2.433, **seven
`[CHARDRV_REPLACE] ... from='crowd1.clp'/'crowd2.clp'/'crowd3.clp'/'crowd4.clp'/'crowd5.clip' to='?'`
events fire** — `Replace(clip, NULL)` → `DeleteClip` → `mFirst=NULL`, i.e. the playing
crowd clip OBJECTS being destroyed, exactly the W26 CHARDRV_BT mechanism. These lines
appear immediately BEFORE the beat-2.433 re-Enters (`mFirstAtEntry=(nil)` — nil
*because the Replace already emptied the stack*, not because the clip had "already
ended") and the CHARDRV_DIE lines. Meanwhile **`CHARDRV_POP` — the probe that
instruments the ONLY natural-end path (`mFirst = mFirst->PreEvaluate(...)` returning
null, CharDriver.cpp:685) — fired ZERO times in every log** (`grep -c CHARDRV_POP
/tmp/w27-crowd/*.log` → all 0). The committed 60-line excerpt
(`evidence/chardriver-state.log`) omits exactly the 7 REPLACE lines while including
the surrounding ENTER/DIE lines. STATUS's "CHARDRV_CLEAR=0" is true but was silently
substituted for the load-bearing claim; `Clear()` was never the kill path — `Replace`
was, in W26 AND in W27's own runs.

**(a) Build drift — RULED OUT.** Window `5b7aabc5..a1cf22f3`: 8 docs commits + 5
micro decomp commits + 1 native-trace linkage fix. `453acfac` (LoadSubDir 99.39→100)
is a pure control-flow restructure (`else` → early `return`, semantics identical);
`f1859684`/`fdc63d29`/`bde8ef79`/`449bd134` touch CharHair.h/GemTrackDir/
BandStorePanel/TokenRedemptionPanel (1-5 lines each, no crowd/ui/char-driver surface).
Engine pin `2088c68`→`8d0e5b0` is **classjson-only** (42 lines in
`NativeCompatFlags.classification.json`, zero code). The behavior did not change
between waves; only the reading of the logs did.

**(b) Both-true reconciliation — CONFIRMED, with the owner corrected one level
further than the prompt hypothesized.** The refcount trace
(`evidence/panel-refcount-trace.log`) shows, during frame 72 (the DIE pollFrame):
line 555 `UIScreen::UnloadPanels screen=splash_screen` → line 556 `splash_panel
refs->0 UNLOAD` → line 557 `sv8_panel refs->0 UNLOAD`, while sv3_panel sits resident
at refs=1 (lines 338/455/468). So a WorldDir teardown — the `WorldDir::~WorldDir →
CharClipSet::~CharClipSet → Replace(clip,NULL)` chain W26 symbolized — has an exact
source at the exact kill instant: **the FAITHFUL unload of the splash-side panels.**
Asset cross-check: `crowd1.clp/crowd2.clp/crowd3.clp/crowd4.clp/crowd5.clip`,
`crowd_male01..04`/`crowd_female01..04(B)` proxy names, and
`crowd_left_walking.mesh`/`crowd_lot_walking.mesh` all appear as raw strings in
**`sv8_a.milo_xbox`** (the splash backdrop vignette) and NOT in `sv3_a.milo_xbox`'s
raw strings (weak negative — sv3_a's nested streetslomo payload is packed; runtime
dump required per A4). **Leading candidate: the destroyed CharClipSet belongs to
sv8_panel's WorldDir.** Canonical wording: *both observations real; W26's backtrace
and nclips drop were genuine, but W26 mis-attributed the destroyed clip set to the
sv3/streetslomo panel; W27 proved sv3_panel resident but misread its own logs into a
"natural end".* The 11-clip vs 8-clip `clips` split (both sets live simultaneously;
pre-kill samples on 0x..ec4520/11, post-kill samples on 0x..717560/8) is the probable
substrate of W26's "nclips 11→8" — whether each driver's `mClips` pointer swaps at
the kill or the 8 drivers were statically split across two sets is NOT decidable from
the %60-sampled logs (W28 STEP-0 item).

**(c) Natural-end causal coherence — REJECTED.** No vignette-done→goto check is
needed: the in-log ordering (REPLACE kills → re-Enters → DIEs, all inside frame 72,
bracketed by the splash UnloadPanels markers) shows the transition CAUSES the kill,
in the W26 direction. The beat-2.433 "coincidence" is causation — teardown, not clip
runtime. Additionally the walk clips were played with stored flags `0x222`
(CHARDRV_STARVE `firstFlags=0x222`, loop bit set) — a looping clip does not "end
naturally" at all.

**Canonical record for README:** W26 row stands with an owner correction; W27 row =
"sv3 residency PROVEN (charter premise refuted — no ui lever exists), but the
teardown-kill REPRODUCES (7 Replace kills in the lane's own logs); root cause is a
clip-set OWNERSHIP/BINDING question (splash-side sv8 copy vs resident streetslomo
copy), not a driver-replay gap."

## Q2 — W27-CROWD gate audit

- **batch_objdiff 7/7 100%** — accepted. Not rebuilt (review rail); verified instead
  that every src hunk in `a1cf22f3` is `#ifdef HX_NATIVE` + `getenv`-gated (10
  guards, 8 getenv sites), so Wii-object byte-identity is structural.
- **rb3-tests 116/0** — accepted as claimed (consistent with byte-inert probes).
- **A7 revisit** — VERIFIED from `evidence/a7-revisit-refcount.log`: 1→2→1, →0 UNLOAD
  across hub→song_select (Wii-GT expected), →1 on return; no monotonic growth. PASS.
- **A10 prewarm** — VERIFIED from `/tmp/w27-crowd/a10-prewarm.log`: adoption fires
  (`main_hub_screen -> prewarming song_select_screen (6 panels)`), run completes
  through frame 230 with no assert. PASS.
- **Overclaims caught (standing pattern holds):** (1) the E1 headline reversal —
  "ZERO teardown … does NOT occur" is contradicted by the lane's own raw logs, and
  the committed excerpt dropped the falsifying lines; (2) "CHARDRV_DIE fires for all
  8" — it fires for **7**; `crowd_female04` never receives Play at all (7
  CHARDRV_PLAY lines, LIFE `firstSet=0`), echoing W25's "one proxy never triggered";
  (3) **mDefaultClip=NULL faithfulness is NOT established.** `mDefaultClip` is
  serialized-only (`mDefaultClip.Load(bs, false, mClips)` in CharDriver LOADS,
  rev>0xB — CharDriver.cpp:923 area): if the data authors no name, Wii is ALSO NULL
  and a "mDefaultClip resolution" charter is a wasted wave; if a name is authored but
  fails native resolution (e.g. against the wrong `clips` set), it IS the divergence.
  Discriminator = log the serialized name at load. Do not charter W28 on the
  driver-replay framing without this.

## Q3 — E-C2 ruling (RB3_CROWD_CLIP_KEEP + E-C3 gCrowdKeep prune)

**PARK — do NOT remove this wave.** W27's removability rationale ("no teardown ever
occurs; CHARDRV_CLEAR=0") is falsified by E1 — the teardown occurs, which is exactly
the flag's original firing condition. The flag is still *probably* dead (W25 and W26
both tested flag-ON: zero drivers recovered, because post-kill `mClips` lacked the
crowd clips), but removing scaffolding on the strength of a lane headline this
close-out rejects would be process-inconsistent, and W28 operates in exactly this
`CharDriver::Poll` block with the ownership discriminator that settles it. Re-rule at
W28 close-out: if the surviving 8-clip set is confirmed crowd-less (or the fix lands),
remove flag + prune then. Removal remains cheap to re-add either way.

## Q4 — drawlog-golden ambient-RED ruling

**Ruling: (b) — recalibrate the EXISTING per-name residual, keep everything else
strict. Do not re-baseline; do not leave RED.** Mechanics: `drawlog-golden.py`
already has exactly the right instrument — `<scene>.fixedclock-residual.json` with a
top-level `eps` + optional per-name `name_eps`, applied ONLY to single-draw
`field=world` failures (compare_fixed_clock, lines ~250-285; "a per-NAME
recalibration, never a global-eps widen"). Coordinator action: on a **clean tree**
(note: both lanes ran with a concurrent agent's uncommitted
`native/src/rb3_session_trace.cpp` in-tree — PROP STATUS flags this honestly; rule
that out first with one clean-checkout run), take N≥5 fixed-clock captures, re-derive
eps/name_eps for the crowd-pose world draws, and land the residual update. Draw
COUNT (792), structural checks, and non-world fields stay strict — so the waiver
cannot mask bind-collapses or draw-set regressions, which is the failure mode
re-baselining (`--update`, option a) invites: it bakes one arbitrary jitter sample
and the next run diverges again. Option (c) standing-RED is worst: both lanes already
had to argue around the gate, which is how gates die. Record that W26's "PASS 792"
and today's RED are the same documented W25/W26 intermittency class, wider variance
at HEAD (12-72 divergences across identical-binary reruns, both lanes concur,
clean-HEAD worktree statistically identical).

## Q5 — W27-PROP-PROBE audit

- **A/B table internal consistency: GOOD.** Recomputed from committed
  `evidence/propA.log`/`propB.log`: skip/clamp counts match EXACTLY (strum 46/0→0/64;
  fret 45/1→0/36; right_hand 46+25→0/56). Migration skip→clamp at ~21-25u vs reach
  ~20.3u is coherent (post-bypass targets land in the marginal [reach, k·reach) band
  → boundary clamp, not gross-unreachable neutralize = "effectively dormant" is a
  fair reading). B-side strum `PROP_DST` entries: zero (claimed "no entries" —
  confirmed; other ikhands' entries in B are the separate mic/vocal-chain class).
- **Median wobble (E6):** strum/fret A-column medians not reproducible from the
  committed logs — recompute gives preDist med 165.6/152.7 (claimed 199.9/184.3) and
  strum dst med 154.2 (claimed 188.5); right_hand matches exactly (118.4/21.4).
  Direction/magnitude class unaffected; likely a different median window. Cosmetic.
- **(b) methodology (E7):** "constant LocalXfm across frames ⇒ no clip track binds"
  is behaviorally sufficient for the MECHANISM (a static tip offset is what flings
  the target, whether unbound or bound-to-a-constant) but does not structurally
  exclude a constant-writing track. Soften the ontological claim; NEGATIVE checkpoint
  remains valid per A9(b).
- **Disposition:** `RB3_PROP_FINGER_BYPASS` kept default-OFF — correct; nits (env-parse
  already present, comment-only weight-loop note) verified as described. No flips, no
  pin bump — verified.

## ERRATA (append-only; add verbatim to the named STATUS docs)

**E1 (W27-CROWD/STATUS.md — MAJOR, headline-reversing).** Append:
> **ERRATUM E1 (close-out): the "ZERO teardown / ends naturally" claims are
> RETRACTED.** The raw probe logs of BOTH boot runs (`/tmp/w27-crowd/
> chardrv-boot.log:543-549`, `combined-boot.log:597-603`) contain seven
> `[CHARDRV_REPLACE] from='crowd1.clp'..'crowd5.clip' to='?'` events at beat 2.433 —
> `Replace(clip,NULL)`→`DeleteClip`, the W26 kill mechanism — immediately preceding
> the re-Enters; `mFirstAtEntry=(nil)` reflects the Replace having already emptied
> the stack. `CHARDRV_POP` (the natural-end probe on the only PreEvaluate-null path)
> fired 0 times in every log, and the played clips carried loop-class flags (0x222),
> so no natural end occurred. The committed `evidence/chardriver-state.log` excerpt
> omitted the REPLACE lines. CHARDRV_CLEAR=0 stands but was never the kill path.
> W26's teardown mechanism REPRODUCES on this build.

**E2 (W27-CROWD/STATUS.md — owner correction).** Append:
> **ERRATUM E2:** the teardown source at the kill instant is the FAITHFUL splash-side
> panel unload: `panel-refcount-trace.log:555-557` shows `UnloadPanels
> screen=splash_screen` → `splash_panel refs->0 UNLOAD` → `sv8_panel refs->0 UNLOAD`
> inside the DIE frame (72), while sv3_panel stays resident. `crowd1-4.clp`,
> `crowd5.clip`, the `crowd_male/female` proxy names and `crowd_*_walking.mesh` are
> raw-string-present in `sv8_a.milo_xbox` and absent from `sv3_a.milo_xbox`'s raw
> strings (weak negative; nested streetslomo payload is packed). Leading candidate:
> the destroyed CharClipSet is owned by sv8_panel's WorldDir; W26 mis-attributed it
> to sv3/streetslomo. Whether each driver's `mClips` swaps 11-set→8-set at the kill,
> or the drivers were statically split across the two same-named `clips` sets, is not
> decidable from the %60-sampled logs.

**E3 (W27-CROWD/STATUS.md).** Append:
> **ERRATUM E3:** "CHARDRV_DIE fires for all 8" → fires for **7**. `crowd_female04`
> never receives `CHARDRV_PLAY` at all (7 PLAY lines; `CHARDRV_LIFE firstSet=0`),
> matching W25's "one proxy never triggered" observation.

**E4 (W27-CROWD/STATUS.md — supersession scope).** Append:
> **ERRATUM E4:** "supersedes BOTH W25 AND W26" is overbroad. W25 (merge/bank-swap)
> stays refuted. W26's panel-unload-teardown MECHANISM is REINSTATED (per E1); what
> this lane genuinely supersedes is W26's OWNER attribution (sv3/streetslomo → the
> splash-side panel per E2) and the W27 charter premise (no residency lever exists —
> sv3 is already resident). The "char-layer clip-replay divergence" root cause is
> replaced by: **clip-set ownership/binding divergence** — why do the crowd drivers'
> playing clips live in (or resolve against) a clip set torn down with the splash
> panels, and what drives the equivalent walkers on Wii main_hub?

**E5 (W27-CROWD/STATUS.md — hand-off correction).** Append:
> **ERRATUM E5:** the hand-off's "mDefaultClip resolution" lever is premature.
> `mDefaultClip` is serialized-only (`mDefaultClip.Load(bs,false,mClips)`, LOADS
> rev>0xB): if the data authors no name, NULL is faithful (Wii identical) and no
> resolution fix exists. W28 STEP-0 must log the serialized default-clip name for the
> crowd drivers before any driver-replay charter.

**E6 (W27-PROP-PROBE/STATUS.md — minor).** Append:
> **ERRATUM E6:** A-column medians as committed are not reproducible from
> `evidence/propA.log` (recompute: strum preDist med 165.6, fret 152.7, strum dst med
> 154.2 vs quoted 199.9/184.3/188.5; right_hand exact; ALL skip/clamp counts and the
> B column reproduce exactly). Conclusion unaffected.

**E7 (W27-PROP-PROBE/STATUS.md — minor).** Append:
> **ERRATUM E7:** "(b) no clip track binds them" is a behavioral inference (constant
> LocalXfm); a constant-writing track is not structurally excluded. The mechanism
> conclusion (static tip offset flings the IK target) is unaffected.

## Q6 — WAVE 28 MENU (one primary lane + optional small tail)

1. **W28-CROWD-OWNER — clip-set ownership discriminator, then ONE scoped fix.
   Tractability: MEDIUM.** STEP 0 (blocking, ≤1 day, all probes already exist):
   (i) one boot with `RB3_CROWD_PANEL_DBG=1 CHARDRV_PROBE=crowd CHARDRV_BT=1` —
   interleave the beat-2.433 Replace BACKTRACE with the panel-unload markers and name
   the torn-down WorldDir owner directly; (ii) dump owner `Dir()` chains of: the five
   played clip objects, BOTH `clips` sets (11 vs 8 — and whether any driver's
   `mClips` pointer swaps at the kill), and the 8 crowd char dirs (+ `roots=2`
   identity); (iii) E5 discriminator: serialized default-clip name at CharDriver
   load; (iv) **Wii-GT identity check:** are the `crowd_*` chars part of retail
   main_hub's street scene at all, or are the hub walkers streetslomo's own
   (differently-named) chars? (retail screenshots + streetslomo runtime inventory —
   if the latter, every wave since W23 has been measuring the SPLASH crowd.) THEN one
   lever: **(A)** native cross-panel clip resolution mis-binds the resident
   streetslomo drivers to the splash-side (sv8) clip set → fix the binding so
   `play_clip` resolves the resident copies (faithful-restoration carve-out class,
   A6-style drawlog ruling applies); or **(B)** the observed crowd is splash-owned
   and faithfully dies → re-charter the acceptance around streetslomo's own walkers
   (fold in the deferred `verts=0` / near-black thread). Given THREE consecutive
   supersessions, the lane must not write fix code before the STEP-0 checkpoint.
2. *(optional tail, only if capacity)* **W28-PROP-FIX** — the real prop fix with the
   now-confirmed mechanism: bind/animate prop-tip clip tracks, redirect BEFORE the
   weight loop (per the W27 comment nit), and break the mFinger feedback
   (`RB3_PROP_FINGER_BYPASS` shows the collapse target). Flag-gated default-OFF.
   Tractability: MEDIUM (mechanism proven; binding design + protected-file surface).
   No user-visible regression risk while parked — defer without guilt.

No third lane. GLOW stays closed; E-C2 removal deferred to W28 close-out (Q3).

## Q7 — env flags needing classjson rows

Grepped both lane commits for added `getenv()` calls: **exactly two** new names —
`RB3_CROWD_PANEL_DBG` (8 sites, `a1cf22f3`: UIPanel/UIScreen/PanelDir/BandScreen)
and `RB3_PROP_FINGER_BYPASS` (1 site, `6a07cc42`: CharIKHand.cpp). Both probe-class,
default-OFF. Nothing missed; `RB3_HUB_CROWD_RESIDENT`/`RB3_HUB_CROWD_REFIRE` were
reserved but never implemented (no row), `RB3_PREWARM_DBG` is pre-existing.

## Coordinator close-out actions

1. Append errata E1-E5 to W27-CROWD/STATUS.md and E6-E7 to W27-PROP-PROBE/STATUS.md.
2. README Wave-27 row per the Q1 canonical wording (W26 mechanism reinstated, owner
   corrected, W27 = residency proven + charter-premise refuted).
3. E-C2/E-C3: **PARKED** (Q3) — re-rule at W28 close-out.
4. drawlog-golden: residual recalibration per Q4 (clean tree first, N≥5 runs,
   name_eps only; no --update re-baseline).
5. classjson: add the two Q7 rows (census 408→410), engine-repo commit + pin bump.
6. Dispatch W28 per Q6 (discriminator-first, checkpoint-before-fix binding).
