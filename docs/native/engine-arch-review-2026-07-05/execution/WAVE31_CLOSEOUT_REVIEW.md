# WAVE 31 CLOSE-OUT REVIEW — adversarial acceptance (four lanes + web rider)

**Reviewer:** Fable close-out agent, 2026-07-12. **Inputs:** WAVE31_KICKOFF.md
(A1–A12 adopted), WAVE31_COUNTERSIGN.md (`b6828fd1`, pre-E1 raw-artifact
re-derivation), lane STATUS docs W31-{SET-PLAY-DISPATCH, HUD-GLYPHS, EXIT-TRAP,
HUBWALKER-SHARDS} (the first two carry coordinator-ack close-out appendices),
lane commits `a3916764`/`fa8b5d55` (A), `8d46802c` (B, docs-only), engine
`0083bad` (C), `42d4a59a` (D probe TU), close-out `6ccc36e3` (rb3) + `24c4f95`
(engine; pin `b36bcfc` → `24c4f95`, census 415→418). Base SHA `fd119705`.
Coordinator re-derivations on the FINAL merged tree cited below are taken as
ground truth (bounded non-HTTP 5-frame boot rc=0 10/10; drawlog-golden
`--fixed-clock` PASS post-A7-edit, frame=60 count=792, 264 known-residual
divergences within bound; F3 ON-vs-OFF captures).

**Verdicts: Lane A ACCEPT (DONE — and the charter's premise REFUTED in the best
way: decomp bug, not missing feature). Lane B ACCEPT-WITH-ERRATA (charter's
"one family" REFUTED honestly; F3 traced + landed at close-out, F2/F4 split and
priced). Lane C ACCEPT (DONE — the wave's one material countersign gap
discharged by coordinator re-derivation). Lane D ACCEPT (SKEL_FAMILY_STOP,
BINDING). Web-yellow rider ACCEPTED as W32 menu input.**

---

## 1. Per-lane verdict vs charter acceptance

### Lane A — W31-SET-PLAY-DISPATCH: ACCEPT (DONE), charter premise REFUTED

The charter expected "a scoped multi-edit spanning >1 system" adding a missing
native dispatch, HX_NATIVE-gated, default-OFF flag, ≤2 TUs (A6). What the lane
found is strictly better and REFUTES that framing: the entire faithful chain
(song.anim `<inst>_intensity` SymbolKeys → `BandDirector::SyncProperty` →
`BandWardrobe::SendMessage` → CHAR_COMMON DTA mood handler → `OnSetPlay`) was
already present and pumped in-song; the stream was dead because of a
**source-level decomp arg-order bug** — 5 `SendMessage(mood, inst)` sites that
should be `SendMessage(inst, mood)`. The swap silenced every mood dispatch
(only `play` ever leaked through, via the `play` substring of `player_`), and
its asm signature was exactly the r4↔r5 REGISTER_SWAP that had `SyncProperty`
parked at 99.96%. Fix = 5-line arg swap in ONE TU (`BandDirector.cpp`),
UNCONDITIONAL — SyncProperty 99.96 → **100.0% Complete** AND the band performs
in-song. All four verbatim acceptance legs graded:

| leg | verdict |
|---|---|
| lever OFF, sustained rhythm/solo CHARDRV_PLAY | PASS — 3→80, quartiles [25,16,20,19] (4/4 ≥ A3's 3-of-4), countersigned from committed gz |
| ≥2 distinct intensities, not constant (A3 anti-gaming) | PASS — play/idle/intense (3), SETPLAY_SEND-logged; not the lever re-badged |
| no sit-group churn (A4 numeric bound) | PASS emphatically — 24→26 (bound 160; the W30 lever did 16→4373) |
| cones/fans gone or explicitly re-scoped (A5) | RE-SCOPED per the pre-agreed A5 clause — STEP-0(iii) enumeration proved perf clips are body clips; prop-tips are instrument-MIDI-driver territory = the F1 family (see §5, not fully closed) |

Deviation handling was exemplary: the lane checkpointed COORDINATOR-ACK-NEEDED
rather than self-granting the unconditional disposition; the ack (STATUS
appendix, close-out) is correct — an HX_NATIVE gate would have FORKED faithful
behavior. `RB3_BAND_PERF_FORCE_PLAY` retired at close-out exactly per the E-C2
zombie-lever precedent (lever block deleted, registry row removed, engine
`24c4f95`). A1's corrected surfaces held (only `src/system/bandobj/` touched);
A6's routing sub-scope was never needed (discriminator (i) = YES, natively
parsed).

### Lane B — W31-HUD-GLYPHS: ACCEPT-WITH-ERRATA, charter "one family" REFUTED

The charter hypothesized F2+F3+F4 as ONE HUD material/texture-bind family. The
lane's split memo REFUTES it with three distinct mechanisms, each priced:
- **F3 (chartered anchor) CONFIRMED + traced end-to-end:** `buttons.mat` (7150
  draws/frame) is a colour-icon atlas (RGB artwork + alpha cell mask) whose
  name lacks "icon" → falls into the `useAlphaAsRGB` text path → every button
  prompt collapses to a solid white blob. Named TU + 1-line predicate fix.
- **F2 (score pill) = NOT the glyph family:** named HUD mesh, does not route
  through `useAlphaAsRGB`; candidate bind/prelit/blend causes; priced MEDIUM.
- **F4 (star row) = NOT a texture bind at all:** unearned-slot show-state /
  anim-frame visibility; priced LOW-MEDIUM.

A8 compliance was exact: mechanism checkpoint first, `engineAckNeeded=true`,
**zero code written by the lane** (commit `8d46802c` docs-only; working-tree
`rb3_render_hook.cpp` clean — countersigned). The fix landed
coordinator-executed at close-out (`6ccc36e3`), and the coordinator upgraded
the lane's proposed default-OFF opt-in to **default-ON with opt-out
`RB3_NO_BUTTON_GLYPH_FIX`** per the B8 precedent, earned by ON-vs-OFF
song_select captures on the merged tree (white lozenges OFF vs real glyph
artwork ON — coordinator-verified). Lint-4 registry sweep done BEFORE the
mechanism claim (none of HUB_TEXT_CONTRAST / text floor / ROWFIX /
SCOREBOARD_TOPRIGHT / W2.7 contradict). Late-add difficulty icons adjudicated
NOT-A-DEFECT (icons PRESENT on a focused song row; the user's report almost
certainly observed a header/shortcut row — correct empty state). Errata E4/E6
below (naming drift; /tmp evidence; workaround-metric tension).

### Lane C — W31-EXIT-TRAP: ACCEPT (DONE)

Full charter arc honored: STEP-0 symbolized backtrace FIRST (no committed one
existed before this wave), root cause NAMED with a live pointer-to-member
identification (`gBandRnd+1432` == `BandRnd::mComposeDiffView`): two late-added
GPU-handle clusters (compose/C8-RTT + billboard-particle) were never added to
`BandRnd::Shutdown()`, so a surviving TextureView held the last strong Dawn
device ref and real teardown deferred to static-dtor phase → SIGSEGV in the
torn-down Vulkan ICD. Fix = release both clusters ahead of `mGpu.Shutdown()`
(engine `0083bad`, ONE file, non-behavioral, NO flag — correct: ordering fixes
are not behavior). The **iterative proof** is the evidentiary highlight of the
wave: the compose-only fix MOVED the backtrace to the particle cluster
(`mComposeDiffView→~TextureView` became `mPartShader→~ShaderModule`), directly
demonstrating each leak before closing it. Acceptance (rc=0 10/10, rb3-tests
116/7/0, drawlog + lineup PASS) was claimed on the lane's own tree — the
countersign's one material gap — and DISCHARGED at close-out by coordinator
re-derivation on the final merged tree (rc=0 **10/10**). A7 then executed:
drawlog-golden's rc-tolerance removed, non-zero rc now hard-FAILs as a
regression of this fix; `song-end-test.py:269` KEEPS its crash-detection band
(the fix targets the SIGSEGV chain; SIGABRT coverage unproven, so narrowing
correctly deferred per A7's own condition). One letter-vs-spirit note: A7 said
"separate post-merge coordinator commit" — the removal rode inside the
close-out commit `6ccc36e3` rather than standalone. Post-merge, coordinator,
gated on the 10/10: substance intact, deviation immaterial (recorded, no
erratum).

### Lane C rider — WEB-YELLOW: ACCEPTED as capture-only finding

CONFIRMED_ON_WEB with a smoking-gun discriminator: the detached yellow-green
quad over the lead character's torso does NOT move when focus changes
(PLAY NOW → CAREER), while the real highlight tracks correctly — a static
orphan quad, not a mis-positioned tracker. A9 deploy-freshness verified first
(deploy postdates base SHA commit) — the stale-build hypothesis stays dead.
Native clean (coordinator W31-REPRO stands). Observed state transition
`overshell: options → joined_default` gives W32 a concrete entry hypothesis
(highlight-mesh instance surviving the flyout close). No fix this wave, per
charter. → W32 menu item 1.

### Lane D — W31-HUBWALKER-SHARDS: ACCEPT, SKEL_FAMILY_STOP (BINDING)

Diagnosis-only mandate honored to the letter: NO fix code; distinct probe TU
(A2-compliant, not BandCharacter/BandCamShot); all three discriminators
resolved with pointer-keyed, matrix-relative evidence (lints 1/2, E7). The
34-row per-mesh per-bone table names the forehead-cone meshes exactly; the
undriven-track hypothesis — the ONLY branch on which a fix was legal — is
REFUTED (bones driven, 70-track walk clips, coherent 250–780u live rotations,
not bind-frozen); the (iii) live-bone probe (pre-authorized by A12) pins the
basis error to the SKEL seed-R rotation-basis class (skinDet=1.0, coherent
~42°, all 33 face bones collapsing to a shared apex ~290u — same class as R5's
87.2°). The E7 census-trap payoff is real: CharCache player0-3 carry **0
skinned meshes** — the visible shards live on CROWD/EXTRAS street characters +
band outfit fringe, i.e. under TWO already-closed families (R5-HANDS-ENDGAME +
the W23-29 CROWD chain). Verdict **SKEL_FAMILY_STOP is BINDING: no recharter
without a new hypothesis** (lint 6 — no 7th cell). Probe TU + harness stay as
reusable diagnosis tooling (now registered: `RB3_SHARD_PROBE_SCENE/_OUT`).

---

## 2. Countersign gaps — discharge ledger

| gap (countersign) | status at close-out |
|---|---|
| **Lane C acceptance NOT RE-DERIVABLE** (post-fix rc=0 10/10 + three gates had no committed artifact; only pre-fix 139 5/5 on disk) | **DISCHARGED** — coordinator re-ran the bounded non-HTTP 5-frame boot on the FINAL merged tree: rc=0 **10/10**; drawlog-golden `--fixed-clock` PASS after the A7 hard-fail edit (frame=60 count=792; 264 known-residual divergences within bound — the lane-tree run said 281; drift across trees expected and within bound both times). This was also the A7 gate, now consumed. |
| Lane B naming drift: STATUS cites `crop_footer_overshell.png` / `crop_diffsidebar.png`, both absent (functional equivalents present) | **OPEN (soft)** — filenames never reconciled in STATUS; the countersign's mapping to `b_footer_whiteblobs_F3.png` / `b_diff_sidebar_PRESENT.png` is the record. Erratum E4. |
| Lane B "overshell" has no standalone crop (rides inside hub/song_select frames) | **PARTIALLY DISCHARGED** — the close-out F3 ON-vs-OFF captures show the overshell MENU chip rendering real artwork vs the blob… but those captures live in `/tmp/w31-f3/` (see E4b — not properly closed). |
| Lane A SyncProperty 99.96→100.0 not re-run by countersign (objdiff not in the authorized list) | **OPEN (minor)** — accepted on the lane's own objdiff run + the exact-mechanism corroboration (the residual WAS the r4↔r5 REGISTER_SWAP at precisely the 5 fixed sites). Cheap to countersign: one `batch_objdiff` on `SyncProperty__12BandDirector…` — W32 rider. |
| Lane A baseline identity: A/B "base" = buggy-arg run (sit=24), not the A4 `OFF=16` W29-idle baseline | **DISCHARGED-AS-DISCLOSED** — STATUS disclosed it; 26 ≪ 160 under either baseline. Rule going forward: name the baseline's identity (which build, which flags) in the acceptance table, not just its numbers. Erratum E1. |
| Process scan (pkill/killall/bare-ninja) | CLEAN, 0 violations — carried. |

---

## 3. Evidence-honesty audit

The good: this wave's lanes were unusually clean. Lane A checkpointed a
disposition question instead of self-granting; Lane B wrote a REFUTATION of
its own charter plus a NOT-A-DEFECT verdict on the late-add user report
(declining two easy "fix" claims); Lane C committed its pre-fix baseline and
an iterative two-stage proof; Lane D returned a STOP with no fix despite
having a fix grant on one branch. Countersign re-derived every load-bearing
number that had a committed artifact.

Errata (one sequence, E1..E7):

- **E1 (Lane A, minor):** A/B baseline identity under-specified — "base" was
  the buggy-arg build, not the chartered W29-idle OFF. Disclosed; margins make
  it non-load-bearing. Rule: acceptance tables name the baseline build/flags.
- **E2 (Lane A, method):** naive `zgrep -c idle_play` = 75 (multi-tag lines);
  the load-bearing count is the per-driver key (`CHARDRV_PLAY dir='player3'` =
  14). Future censuses grep the per-driver key, never the aggregate.
- **E3 (Lane A, open verification):** the 100.0% SyncProperty figure has no
  committed countersign artifact. One-shot batch_objdiff rider in W32.
- **E4 (Lane B, soft):** (a) two STATUS-cited crop filenames absent —
  naming drift vs the functional equivalents on disk; "overshell" verified
  only as an overlay within full-frames, never a standalone crop. (b) the
  close-out F3 ON/OFF evidence is cited at `/tmp/w31-f3/…` — `/tmp` is scratch
  per lint 7; **not properly closed** until copied under
  `execution/W31-HUD-GLYPHS/evidence/` (§5).
- **E5 (Lane C→Lane D collision, process):** Lane D's untracked WIP probe TU
  (`rb3_shardprobe_native.cpp`) transiently broke Lane C's rb3-native link in
  the shared source tree (Matrix3 vs Hmx::Matrix3). The A2 fence covered
  *committed-surface* collisions but not *on-disk untracked TUs swept up by
  the build*. Rule: a new TU left on disk in a globbed/shared source dir must
  compile clean before its author yields the CPU — or live outside the shared
  tree until it does.
- **E6 (close-out, metric tension):** `RB3_NO_BUTTON_GLYPH_FIX` is registered
  class=workaround default=on, bumping the "default-ON workarounds (§W5.3
  drives to 0)" metric 74→75 — yet it is a *faithful-rendering fix's escape
  hatch*, not a workaround for un-derived behavior. Either the class taxonomy
  needs a "fix-opt-out" bucket or §W5.3's metric will accumulate false
  positives as more B8-precedent fixes land. Flagged for the registry owner.
- **E7 (Lane A, acceptance-letter gap):** the kickoff's W31-REPRO addendum
  (BINDING) asked for re-shot floating-legs crop PAIRS at matched songMs
  (vocalist no longer frozen mid-jump; guitarist leg pose re-graded). The
  lane's matched shots (`{fix,base}_gp0{05,10}.png`) frame the TRACK, not the
  band; STATUS says so and leans on the census oracle. The census is the
  validated instrument and the mechanism is proven — but the USER-FACING
  visual leg of the floating-legs report is closed by inference, not by
  before/after eyes-on-band evidence. Carried into W32's F1 item (§5).

---

## 4. Process lessons

1. **The arg-order class of decomp bug is a cheap, high-yield audit target.**
   A single swapped `SendMessage(Symbol, Symbol)` argument pair silenced an
   entire animation subsystem for 31 waves while the function sat at 99.96%
   with a "cosmetic-looking" REGISTER_SWAP residual. Same-typed adjacent
   parameters are invisible to the compiler and to fuzzy matching — but the
   asm diff names them exactly (r3-r6 order swaps at call sites). Audit
   recipe: sweep ≥99% functions whose sole residual is a call-site
   REGISTER_SWAP on same-typed args; each is a candidate *behavior* bug, not
   match noise. This also inverts the campaign's usual arrow: decomp match%
   verified a NATIVE fix (99.96→100.0 was the proof the swap was faithful).
2. **The flag-disposition rule is now three tiers, and it emerged from acked
   deviations, not doctrine:** (i) decomp-correctness fix → UNCONDITIONAL, no
   flag (Lane A — a gate would fork faithful behavior); (ii) faithful-behavior
   restoration proven against retail → default-ON with opt-out escape hatch
   (Lane B F3, B8 precedent); (iii) uncertain/heuristic change → default-OFF
   opt-in (the old default). Lanes should propose the tier with the
   disposition rationale; the coordinator grades it at ack time.
3. **Countersign-then-rederive works as designed.** The wave's one material
   evidence gap (Lane C's acceptance) was named pre-E1 and discharged by a
   targeted re-run on the merged tree — not accepted on report, and not
   allowed to block an otherwise-proven fix.
4. **Diagnosis lanes that end in STOP still pay rent:** Lane D's stop verdict
   retired a live user-visible report into two already-closed families with a
   pointer-keyed table, extended the E7 census-trap doctrine (name-keyed
   player0-3 ≠ the walkers you can see), and left a registered reusable probe.

---

## 5. Not properly closed (honest list)

1. **F3 close-out evidence in `/tmp`** (E4b): copy `/tmp/w31-f3/{on,off}` +
   crops into `execution/W31-HUD-GLYPHS/evidence/` before they evaporate.
   Until then the default-ON's earning evidence is volatile.
2. **SyncProperty 100.0% uncountersigned** (E3): one batch_objdiff, W32 rider.
3. **Floating-legs visual leg closed by census, not by eyes** (E7): the W32 F1
   lane must open with matched-songMs band-framing crops (boot-to-song closeup
   harness exists) — both to close the user report visually and as the F1
   family's baseline.
4. **F1 undriven prop-tip family is now the explicitly re-scoped debt** of
   Lane A's A5 clause: drumstick/guitar-neck/mic prop tips (instrument-MIDI
   drivers), kit cones, magenta stick-fan guitar. Re-scoped ≠ fixed.
5. **Web-yellow confirmed, unfixed** (by charter) — top W32 candidate.
6. **W31 probes not yet on a retirement clock:** `RB3_SETPLAY_PROBE`,
   `RB3_SHARD_PROBE_SCENE/_OUT` registered this wave; per the W30
   probe-retirement discipline they get a keep/retire decision once their
   families' W32 needs are known (SETPLAY_PROBE is plausibly KEEP as the
   set_play acceptance instrument; the shard probes' family is STOPPED).
7. **E6 taxonomy question** (fix-opt-out vs workaround) — registry owner.

None of these gate acceptance; items 1–2 are minutes of work and should be
done early in W32 pre-work.

---

## 6. Wave-32 menu (recommendation — ordering rationale)

User-visible, evidence-fresh work first; the two split HUD sub-charters are
priced and shovel-ready; F5 stays recorded until someone brings a hypothesis.

1. **W32-WEB-YELLOW (primary):** the hub floating highlight quad — the wave's
   rider-confirmed, user-reported, web-only defect with a concrete entry
   hypothesis (highlight-mesh instance surviving the `options`→`joined_default`
   overshell transition). Candidate surfaces: `src/system/bandobj/
   OvershellDir.cpp` / MainHubPanel highlight mesh + the web-vs-native
   render-hook divergence (why web-only?). Iterate the *logic* natively per
   the native-repro doctrine; confirm on web once. STEP-0: name the quad's
   mesh/draw (uidump/drawlog on the web build), THEN fix.
2. **W32-PROP-FAN (F1 family):** the re-scoped half of the floating-legs
   report — undriven prop-tip bones (drumstick tips, guitar neck, kit cones;
   instrument-MIDI drivers `strum.dmidi`/`fret.ikmidi`/`right_hand.dmidi`).
   Opens with the E7 debt: matched-songMs band-framing crop pairs. Discipline:
   discriminator-first (are the dmidi/ikmidi drivers bound and fed natively,
   or bound-and-starved?), checkpoint before fix.
3. **W32-HUD-F2F4:** the two priced split sub-charters from Lane B's memo —
   F2 score-pill fill (MEDIUM: pill-mesh material dump → bind/blend fix) + F4
   star-row show-state (LOW-MEDIUM: 5-slot milo group, unearned-slot
   visibility). Two mechanisms, one lane, two checkpoints — do NOT re-merge
   them into "one family" (that hypothesis is dead).
4. **F7 song-select right-edge clipping** (user: "still clipping") — incl. the
   no-opaque-panel-behind-sidebar variant. Cosmetic but user-repeated twice.
5. **F5 patch-shard `coop_g_cg` repro** — recorded, deterministic on-camera,
   two prior bisect-reverts; dispatch ONLY with a fresh hypothesis (the
   closeup gate is shard-blind, so any lane must bring its own oracle).
6. **Riders (coordinator-cheap):** (a) F3 evidence re-home from /tmp (§5.1);
   (b) SyncProperty batch_objdiff countersign (§5.2); (c) probe
   keep/retire decision for SETPLAY_PROBE + SHARD_PROBE_* (§5.6); (d) the
   arg-order audit pilot from lesson 1 (sweep ≥99% REGISTER_SWAP-at-call-site
   residuals for same-typed-arg swaps) — cheap, and W31 proves the payoff can
   be an entire subsystem.
7. **Carried/blocked:** F6 hub night grade stays BLOCKED on the UIGRADE
   reconciliation (W30 adjudication note); F8 pending settle-frame recapture;
   Lane D families remain CLOSED (SKEL_FAMILY_STOP binding — no recharter
   without a new hypothesis).

_Author: Wave-31 Fable close-out reviewer. Read-only on code; this doc + the
README results/menu section are the only writes._

## Coordinator discharge addendum (post-review, 2026-07-12)

- §5 F3 evidence re-home: DONE — `/tmp/w31-f3` captures copied to
  `execution/W31-HUD-GLYPHS/evidence/f3-closeout/` (f3_{on,off}_full.png +
  crop_footer_{on,off}.png; on-disk untracked per the amended lint 7, findings
  quoted in the lane STATUS coordinator-ack section).
- §5 SyncProperty countersign: DONE — independent `batch_objdiff` on
  `SyncProperty__12BandDirectorFR8DataNodeP9DataArrayi6PropOp` post-merge:
  **fuzzy 100.0% / raw 100.0%, verdict COMPLETE** (this addendum is the
  committed record).
- §5 floating-legs crop-pair debt: stands — carried as the opening acceptance
  item of the W32 F1 charter, as the review directs.
