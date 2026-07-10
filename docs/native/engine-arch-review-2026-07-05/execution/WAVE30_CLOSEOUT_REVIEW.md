# WAVE 30 CLOSE-OUT REVIEW — adversarial acceptance (both lanes + concurrent visual pass)

**Reviewer:** Fable close-out agent, 2026-07-10. **Inputs:** WAVE30_KICKOFF.md (CA1-CA8),
Lane 1 `W30-BAND-PERF-CLIP` (commit `3156c6b1`), Lane 2 `W30-PROP-DEFAULT-ON`
(commit `bafa0921`), concurrent `W30-VISUAL-PASS/FINDINGS.md` (commit `85143cdf`,
coordinator-dispatched, not a workflow lane), coordinator E1 raw-log countersign
(reported, spot-re-verified below). Base `fdc4d628`, engine pin `17807afd…` unchanged.

**Verdicts: Lane 1 ACCEPT-WITH-ERRATA (PARTIAL, honest). Lane 2 ACCEPT-WITH-ERRATA
(LANDED, DECISION: FLIP-SAFE stands). Visual pass ACCEPTED as Wave-31 menu input.
Coordinator executes Q(b) flip + Q(d) retirement + Q(e) census regen.**

---

## 1. Reviewer adversarial checks (run fresh, not copied from E1)

### Lane 1 — mechanism claim verified against code + committed raw gz

- **`set_play` has no C++ sender:** repo grep confirms — only the `Symbol` decl
  (`Symbols.h:202`/`Symbols.cpp:192`) and the `HANDLE(set_play, OnSetPlay)` receiver
  (`BandCharacter.cpp:4340`). Nearby hits (`set_playing_intro`, `set_play_all_tracks`,
  `set_player_*`, `shot_set_play_mode`) are different symbols. VERIFIED.
- **`OnSetPlay` is the only intensity rewriter:** `BandCharacter.cpp:4508` —
  `SetState(mGroupName, mPlayFlags & 0xFFF80FFF | da->Int(2), 3, …)` — exactly the
  masked-rewrite STATUS describes, dispatched with mask=3. VERIFIED.
- **Beat-0-only `set_play`, play_group-only in-song:** `census1_bandperf.log.gz` has
  194 `BANDPERF_STATE` rows = 190 `mask=2` (play_group) + exactly 4 `mask=3` (one per
  player0-3), all four at `beat=0.000`, `grp=''`, `flags=0x1000` (IR). VERIFIED — the
  load-bearing claim reproduces from the committed raw log.
- **Probe placement/gating:** diff review — `BANDPERF_STATE/_BT` in
  `BandCharacter::SetState` (unfiltered by char name, W29 lesson honored),
  `BANDPERF_CLIPS/CLIPFLAGS` via public `ClipDir()` per CA8 (no CharDriver.cpp edit),
  `BANDPERF_SHOT` send-side in `BandCamShot::StartAnim` per CA4 (probe-only),
  `BANDPERF_CLIP` at the selected-clip site. All `#ifdef HX_NATIVE`, getenv-gated
  (`RB3_BANDPERF_PROBE`/`_BT`/`_CLIPS`), byte-identical `#else`. Lever
  `RB3_BAND_PERF_FORCE_PLAY` is default-OFF, HX_NATIVE, naming per kickoff
  (`RB3_BAND_PERF*`, not CROWD_/CLIP_KEEP/…), and is consistently labeled
  NON-FAITHFUL / DEMONSTRATION in code comment, STATUS, checkpoint, and commit
  message. VERIFIED.
- **E4 quote rule:** STATUS quotes all four kickoff acceptance bullets verbatim
  (CA-amended text) before self-grading. VERIFIED.
- **NEW finding (reviewer):** the ON A/B log shows a **sit-group SetState retrigger
  storm** under the demo lever — `grp='sit'` `BANDPERF_STATE` rows go 16 (OFF) →
  **4373** (ON) (this is the undiscussed 87→4444 jump in the A7 table). The forced-P
  mask evidently thrashes re-selection on the seated member's group. Harmless
  (default-OFF demo lever) but must be disclosed and becomes a W31 acceptance bound
  (see Q(f)). → **E3**.

### Lane 2 — CA5 compliance + flip preconditions verified

- **Threshold-unbiased census:** diff review — `[PROP_CENSUS]` sits at the TOP of
  `CharIKHand::Poll`, before any dst computation, deduped per (root|ikhand)
  first-sighting, gated by `RB3_PROP_CENSUS_DBG`, `#ifdef HX_NATIVE`, default-OFF.
  NOT derived from the 30u-gated `[PROP_DST]`. CA5 SATISFIED (rejection clause does
  not fire).
- **No default flip by the lane:** `sPropPoseFull()` still reads
  `RB3_PROP_POSE_FULL` opt-in (`(e && e[0] && e[0]!='0')`), unchanged. VERIFIED.
- **Analyzer extended in place (CA1):** commit stats show only
  `W28-PROP-FIX/analyze_prop_ab.py` modified (+279/−39); no fork under
  `scripts/native/`. VERIFIED.
- **Raw counts:** `prop_ON_cap200k.log.gz` → 16 `[PROP_CENSUS]` rows, 11 `finger=1`;
  OFF log 53057 `[PROP_DST]` — match STATUS/A7 exactly. VERIFIED (countersigning E1).
- **Caveat A countersigned + widened:** feeding the analyzer the `.gz` paths
  directly (binary, zero rows parsed) still prints `W30 DECISION: FLIP-SAFE` exit 0
  with vacuous tables (`OFF med=-  ON med=-  -> NO-IMPROVE`). Reviewer additionally
  found: a **usage error also exits 0** (`--w30-census` with positional args prints
  usage and returns 0). The lane's decision is unaffected (it ran on gunzipped logs
  with populated tables, E1-reproduced), but both decider exits are unsound → **E5**
  with a mandatory guard fix.
- **Census identity is name-keyed, not object-keyed (reviewer):** the dedupe key is
  `(rootName|ikhand)`; per W29 the hub walkers ARE `player0-3`
  (char/main/main.milo), the same names as the gameplay band — same-named ikhands
  across scene phases collapse into one census row, so "16 CharIKHands" = 16 distinct
  name-keys over the boot+song window, not provably 16 objects. NOT load-bearing:
  the FLIP-SAFE decider runs on the un-deduped `[PROP_DST]` aggregates per
  (char,ikhand) bucket, which cover every polled row in the window, and no NON-PROP
  bucket regresses. → **E7** (documentation, binding on future censuses).

### Both lanes — process hygiene

- Checkpoints mirrored into `evidence/checkpoints/` (4 Lane 1, 3 Lane 2). VERIFIED.
- `pkill`/`killall`: zero hits in both STATUS/PLAN docs and all 7 committed raw gz.
  No bare-`ninja` mentions. VERIFIED.
- Bounds: Lane 1 used 4/6 boot runs; Lane 2 4/4 (ledger disclosed, incl. the
  superseded first census run). VERIFIED.
- Ownership: no cross-lane file touched; `rb3_session_trace.cpp`, engine
  `FxSendNative.cpp`, `scripts/web/_*.mjs` untouched by both commits. VERIFIED.

### Visual-pass F1 cross-check (see Q(f) for disposition)

`gameplay_wide_spikefans.png` (default flags, PROP OFF): white stick-fans at all
four members' HANDS + drumstick fans + crumpled cones around the kit.
`playnow_submenu_native.png`: the hub-walker fan is **waist-level prop meshes**
(drumstick-like bundles), not a hand chain. Lane 2's ON screenshots are dark/oblique
framings — they show no fans but are weak positive evidence on their own; the
hand-fan-gone claim rests properly on the W28/W29 closeups + the A/B dst collapse.

---

## 2. Errata (append-only; E1-E4 append to Lane 1 STATUS, E5-E7 to Lane 2 STATUS)

- **E1 (Lane 1, bookkeeping).** STATUS says the STEP-0 checkpoints carry
  `buildSha=fdc4d628`; the committed checkpoints actually record `85143cdf` (step-0,
  docs-only delta vs fdc4d628) and `"bafa0921 + probes + lever"` (lever A/B — a tree
  including Lane 2's default-OFF HX_NATIVE probes). Source-equivalent for every
  surface under test; no soundness impact. Record, don't rerun.
- **E2 (Lane 1, evidence durability).** The committed `BANDPERF_STATE_BT` frames are
  unsymbolized `(+0x…)` offsets; the symbolized dispatch chain in STATUS came from
  addr2line against the (uncommitted) run binary. The chain is consistent with the
  code path and the mask census corroborates it, but it is not independently
  re-derivable from the gz after a rebuild. Future BT evidence: commit one addr2line
  transcript alongside the raw log.
- **E3 (Lane 1, undisclosed lever side effect).** ON A/B shows a sit-group SetState
  retrigger storm (`grp='sit'` 16→4373) under `RB3_BAND_PERF_FORCE_PLAY` — visible
  in the A7 table (87→4444) but unexplained in STATUS. One more reason the lever is
  demo-only; W31's faithful dispatch must NOT exhibit it (bound in Q(f)).
- **E4 (Lane 1, weak screenshot).** `bandperf_ON_songMs11318.png` is a torso
  closeup; "band member mid-performance" is technically met but the pair is weak
  visual evidence. Accepted on the strength of the CHARDRV_PLAY census (0→55,
  E1-reproduced), not the screenshots.
- **E5 (Lane 2 + analyzer, = coordinator Caveat A, widened).** The
  `--w30-census`/`--w30-residual-baseline` deciders are vacuous on empty parse:
  binary/.gz input → zero rows → `FLIP-SAFE` exit 0; AND a usage error exits 0.
  RULING: coordinator applies a guard to
  `W28-PROP-FIX/analyze_prop_ab.py` in place — exit 2 with `ERROR: NO ROWS PARSED`
  when either log yields 0 census AND 0 dst rows in a w30 mode, and exit 2 on usage
  error. Decision unaffected (lane + E1 ran gunzipped logs, tables populated).
- **E6 (Lane 2, = coordinator Caveat B, residual framing).** On the UNCAPPED ON log
  the drummer right_hand shows `dst_n=4130, dst_med=39.3` (lane's own A/B table:
  med 39.0 / max 43.0 / n=3528 songMs-window) — the historical "8×31u residual" is
  the CAPPED focus-probe view (cap120, W29-continuity), not the sustained magnitude.
  Lane disclosed honestly (STATUS A/B table + bare-name note); the FLIP COMMIT must
  use the Caveat-B framing verbatim (Q(b)).
- **E7 (Lane 2, census identity).** Census rows are name-keyed (root|ikhand);
  same-named ikhands across scene phases (hub walkers = player0-3 per W29) collapse.
  "16 CharIKHands / 11 finger=1" = distinct name-keys over the window. Decision
  unaffected (decider uses un-deduped per-bucket aggregates). BINDING: future ikhand
  censuses either key by object pointer or state the name-key caveat.

---

## 3. Rulings

**Q(a) — acceptance.** Lane 1 **ACCEPT-WITH-ERRATA, outcome PARTIAL** (mechanism
named + proven, faithful fix honestly rechartered; E1-E4 appended to its STATUS).
Lane 2 **ACCEPT-WITH-ERRATA, outcome LANDED — DECISION: FLIP-SAFE stands** (E5-E7
appended to its STATUS). Errata numbering is one sequence E1-E7 across both lanes
(prior-wave convention). Visual pass: accepted as menu input; no errata process (not
a lane; retail-pair gaps were honestly declared per finding).

**Q(b) — the flip (15th default).** YES. Coordinator flips `RB3_PROP_POSE_FULL`
default-ON in `sPropPoseFull()` (CharIKHand.cpp:70-77) using the exact
`RB3_IK_REACH_CLAMP` opt-out-wins pattern (CharIKHand.cpp:526-532): new opt-out env
`RB3_PROP_POSE_FULL_OFF` wins; else `RB3_PROP_POSE_FULL=0` (leading `'0'`) disables;
else ON. Comment must note: (1) FULL default-ON also forces `sPropPoseRedirect` ON
(pre-existing documented coupling; opt-out restores the redirect to the
`RB3_PROP_POSE` opt-in), (2) still `#ifdef HX_NATIVE`, Wii `.o` byte-identical.
**Exact residual sentence for the flip commit (Caveat-B framing, binding):**
> Residual shipping with ON: the drummer right_hand still over-reaches its prop
> target — sustained across the song at med ~39u / max ~43u (n≈3.5k rows, uncapped
> songMs-matched window); the historically quoted "8×31u residual" is the capped
> focus-probe view (cap120, W29-continuity), not the sustained magnitude. Visually
> acceptable per the W28/W29 guitarist/drummer closeups; root cause is the
> zero-performance-clip idle pose (W30-BAND-PERF-CLIP finding), not piece-1, and is
> expected to shrink when W31 lands the faithful set_play dispatch.

**Q(c) — post-flip verification (coordinator, in order).**
1. `flock /tmp/rb3-native-build.lock cmake --build native/build-native --target rb3-native`,
   then `drawlog-golden --fixed-clock --canonical-order` → verdict must stay
   **792 PASS**; bound on known-residual count = the W30 observed band **307-309
   ±(couple)** — an IK pose change alters transforms, not the draw-call set, so any
   drift outside ~300-320 known-residuals or any new FAIL = STOP, do not commit.
2. Boot A/B: one `scripts/native/boot-to-song.py` run with NO env (new default) rc=0
   reaching gameplay, one with `RB3_PROP_POSE_FULL_OFF=1` rc=0 (opt-out path
   exercised once). Teardown rc=-11 tolerated as usual.
3. `rb3-tests` = 116/0/7skip.
4. `batch_objdiff` `Poll__10CharIKHandFv` == **96.13** baseline-exact (flip is
   HX_NATIVE-only; Wii `.o` byte-identical).
5. F1 retest rider (see Q(f) disposition): rerun
   `boot-to-song.py --hold 45 --interval 3` + `band-closeup-capture.py --member all
   --frames 2` and compare against the three F1 evidence frames — expect HAND
   spike-fans gone on gameplay members; cones/hub-walker fans remaining is EXPECTED
   (goes to W31), not a flip failure.

**Q(d) — probe retirement (CA3) + new-probe dispositions.** Retirement scope and
gates confirmed exactly per CA3: coordinator-executed; CharDriver.cpp
(`CHARDRV_ENTER/REPLACE/REPLACE_BT/DEFCLIP/STARVE/LIFE`), CharCache.cpp
(`C13_PROBE`), UIPanel/UIScreen/PanelDir/BandScreen (`RB3_CROWD_PANEL_DBG`); gates =
repo-wide `grep -rc <tag> src/` == 0 per retired tag, batch_objdiff baseline-exact
on EVERY touched unit, KEEP-list (`CHARDRV_PLAY`, `CHARDRV_PLAY_BT`,
`CHARDRV_CLIPSWAP`, live `BANDPERF_*`) byte-identical. Lane 1's declaration
"retire-list probes used in STEP 0: none" is verified — no exemptions. Note: the
retire-listed CHARDRV tags share the `CHARDRV_PROBE` env (rows drop from its site
count); env deletions are `CHARDRV_BT` (gates `CHARDRV_REPLACE_BT`), `C13_PROBE`,
`RB3_CROWD_PANEL_DBG`.
New probes: **`BANDPERF_*` KEEP** (all: `RB3_BANDPERF_PROBE`/`_BT`/`_CLIPS`) — they
are the W31-SET-PLAY-DISPATCH acceptance instrument (the same census rerun is the
W31 gate). **`RB3_PROP_CENSUS_DBG` KEEP** as the standing ikhand-census tool
(one-shot, threshold-unbiased, cheap; exactly what any future flip decision needs;
E7 caveat documented in its census row). **`RB3_BAND_PERF_FORCE_PLAY` KEEP
default-OFF** as the residency-vs-selection discriminator UNTIL the faithful
set_play dispatch lands, then **RETIRE at W31 close-out** (zombie-lever hazard per
the E-C2 precedent — a demo lever must not outlive the faithful fix). Its census
row: class demo/workaround, NON-FAITHFUL, noting the E3 sit-churn side effect.

**Q(e) — census/classification + pin.** Single regen
(`python3 scripts/analysis/native_compat_census.py gen`) AFTER flip + retirement.
Expected row delta: +5 new envs (`RB3_BANDPERF_PROBE`, `RB3_BANDPERF_BT`,
`RB3_BANDPERF_CLIPS`, `RB3_BAND_PERF_FORCE_PLAY`, `RB3_PROP_CENSUS_DBG`) +1
(`RB3_PROP_POSE_FULL_OFF`) −3 retired envs (`CHARDRV_BT`, `C13_PROBE`,
`RB3_CROWD_PANEL_DBG`) → **410 → ~413** (generator is source of truth; explain any
difference in the commit). Row updates: `RB3_PROP_POSE_FULL` default off→**on**
(15th default, class feature); `RB3_PROP_POSE` note "forced on by FULL's default;
opt-in only relevant under FULL_OFF"; `CHARDRV_PROBE` site count drops. Pin: **no
engine edits occurred this wave — expect NO pin bump**; verify `MILO_ENGINE_PIN`
still `17807afd…` and bump only if the engine repo moved for an independent reason
(and then at most ONCE, per standing rule).

**Q(f) — Wave-31 menu (ranked) + F1 disposition.**
F1 disposition first: the PROP flip plausibly retires the **hand** spike-fans on
gameplay members (W28/W29 closeups + Lane 2 A/B dst collapse; F1's hand fans were
captured default-OFF). It does NOT plausibly retire: the crumpled cones / floating
frames around the drum kit and the **waist-level** hub-walker stick-fans — those are
undriven prop meshes (W27(b): prop-tip clip tracks carry constant authored LocalXfm
and only animate when performance clips actually play; hub walkers play
`playerN_{f,m}` walk clips, and the redirect only applies to `bone_target_*`
instrument frames). Expected split verified by the Q(c)#5 named retest. Gameplay
prop fans/cones therefore fold into W31 lane 1's acceptance; the hub-walker fan is
tracked as its own residual (may need walk-clip prop-track scoping later).
1. **W31-SET-PLAY-DISPATCH (primary)** — Lane 1's faithful recharter: make the
   song-authored venue-mood stream (`[play]`/`[intense]`/`[mellow]`/`[solo]`)
   dispatch `set_play` to BandCharacter natively. Discriminator-first: (i) does the
   mood/venue event data exist parsed natively (BandDirector-side census)? (ii) who
   should send it (DTA venue scripts vs song.anim events)? Spans >1 system — expect
   a scoped multi-edit, not a one-line lever. Acceptance: with the DEMO LEVER OFF,
   sustained rhythm/solo `CHARDRV_PLAY` census (Lane-1 A/B rerun, OFF=W29-idle
   baseline); **no sit-group churn** (E3 bound: ON-run `grp='sit'` BANDPERF_STATE
   same order as OFF, not thousands); F1 gameplay retest — drumstick/prop-tip bones
   driven, cones/fans gone or explicitly re-scoped; retire
   `RB3_BAND_PERF_FORCE_PLAY` at close-out.
2. **W31-HUD-GLYPHS (secondary)** — F2 (translucent score pill) + F3 (white glyph
   class) + F4 (star-slot row) as ONE HUD material/texture-bind family lane: F2/F3
   are plausibly one mechanism (texture unbound → white fallback, or dropped tint);
   retail pairs exist for all three; trace one glyph end-to-end, fix the bind, verify
   the class across hub/song_select/overshell + the pill/star row vs retail.
3. **W31-EXIT-TRAP (small, hygiene)** — the teardown SIGSEGV every wave's gates must
   tolerate (menu item 4, deferred since W29). Bounded and well-reproduced; paying it
   off cleans every future gate. Riders: F5 patch-shard is NOT chartered this wave —
   but record its NEW capability (deterministic on-camera repro: `coop_g_cg` closeup
   + existing ASan harness) for a future wave with a fresh hypothesis (two prior
   rewrites bisect-reverted; do not re-attempt without one). F6 hub night grade: DO
   NOT charter until the coordinator reconciles it with the held
   RB3_UI_POST_GRADE/UIGRADE rationale — it may be the already-adjudicated residual;
   adjudication note in README suffices this wave. F7 cosmetic backlog; F8 is
   not-a-finding until a settle-frame recapture series (W4.1 method). `part:`-verb
   tooling stays coordinator-owned opportunistic (unblocks drums/keys sweeps for
   future waves — worth doing before W31 dispatch if cheap).

**Q(g) — additional.** (1) The analyzer guard fix (E5) is coordinator-executed in
place, one commit, no lane redo. (2) Lane 2's "all 3 A/B ON/OFF runs" phrasing vs
the 4-run ledger is the superseded first census run — disclosed, no issue. (3) Lane
1's CA7 base-SHA line is correct as written; the checkpoint-SHA drift is E1. (4)
Coordinator E1 numbers spot-checked here (census row counts, mask census, PROP
census 16/11, OFF dst 53057) all reproduce; the expensive reproductions
(batch_objdiff triple, focusON acceptance pair) are accepted on the E1 record.

---

*Reviewer note: no source edits, no builds, no process kills were performed by this
review; all checks were greps/reads of committed artifacts plus one read-only run of
the committed analyzer on committed gz (Caveat-A repro).*
