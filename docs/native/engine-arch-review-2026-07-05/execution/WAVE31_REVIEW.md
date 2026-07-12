# WAVE 31 PRE-DISPATCH REVIEW — adversarial amendments A1..A12

**Reviewer:** Fable pre-dispatch agent, 2026-07-12. **Document under review:**
`WAVE31_KICKOFF.md` @ rb3 `bb750efa`. **Method:** every file:line and flag claim
re-derived against the current tree (R-C executed by symbol, not by trusting W30
lines); ledger/harness/closure existence checks run fresh. No source edits, no
builds.

**Headline: ONE BLOCKING error (A1 — Lane A's write grant names a directory that
does not exist; the actual receiver/probe/lever TU is outside the granted
surface). Dispatching as written would have sent the primary lane into an
ownership stall (or a self-expanded scope violation) on its first edit, and the
lint-9 flavor check the kickoff marks [x] would have caught it.** Everything
else is gate-tightening and precision.

Claims checked and VERIFIED as written (no amendment): engine pin `b36bcfc` ==
engine HEAD, dirty only `M src/platform/FxSendNative.cpp`; `BANDPERF_*` probes
present post-retirement (`src/system/bandobj/BandCharacter.cpp`,
`src/system/bandobj/BandCamShot.cpp`) and `CHARDRV_PLAY` in
`src/system/char/CharDriver.cpp` + BandCharacter.cpp; `RB3_BAND_PERF_FORCE_PLAY`
present + getenv-gated default-OFF (BandCharacter.cpp:482-490); Symbol decls at
`src/system/utl/Symbols.h:202` / `Symbols.cpp:192`; Lane A's acceptance block is
byte-verbatim Q(f) (E4 honored); `scripts/native/_exit-trap-test.py` is indeed
the SEPARATE web-release past-score-screen trap (its own docstring, lines 2-7);
retail pairs `yt_qRagnZCIMzk_gameplay_guitar.png` /
`yt_qRagnZCIMzk_song_select_list.png` / `yt_mhKNp9uAT48_menu_hub.png` and
`ui_buttons_wii_spriters.png` all exist in `images/retail-screenshots/`;
`R5-HANDS-ENDGAME/CLOSURE.md` exists, binds the 87.2° seed-R family with a
pre-registered reopen condition, and Lane D's STOP-and-memo branch matches it;
Lane D's census caveat wording matches W29 Q(b) (README:967) + E7 exactly;
lint-4 for Lane D is correctly discharged (Q(c)#5/Q(f): hub-walker fans survive
the PROP flip).

---

## A1 — BLOCKING — Lane A's write surface names a nonexistent directory; the real edit TUs are outside the grant

**Kickoff sentences:** "Key sites from W30: receiver `HANDLE(set_play, OnSetPlay)`
`src/band3/bandobj/BandCharacter.cpp:4340`, rewrite `:4508`" and "Owned surfaces:
`src/band3/bandobj/` + `src/band3/game/` band-perf plumbing (WRITE, HX_NATIVE-gated)".

**Evidence:** `src/band3/bandobj/` does not exist (`ls: cannot access
'src/band3/bandobj/': No such file or directory`). The receiver is
`src/system/bandobj/BandCharacter.cpp:4344` (`HANDLE(set_play, OnSetPlay)`), the
rewrite is `src/system/bandobj/BandCharacter.cpp:4512`
(`SetState(mGroupName, mPlayFlags & 0xFFF80FFF | da->Int(2), 3, ...)`) — both
drifted +4 lines from the W30-quoted numbers, exactly the R-C hazard the kickoff
itself flagged. W30's own STATUS "Files touched" already said
`src/system/bandobj/BandCharacter.cpp` + `src/system/bandobj/BandCamShot.cpp`.
`BandDirector.cpp`/`BandDirector.h` are ALSO in `src/system/bandobj/`;
`BandPerformer.{h,cpp}` are in `src/band3/game/`. rb3-native compiles
`src/system/bandobj/*.cpp` via glob (native/CMakeLists.txt:268) and
`src/band3/game/*.cpp` (:271), so flavor membership is fine once the path is
fixed.

**What dispatch would have wasted:** the primary lane's entire edit surface
(receiver, probes, lever, likely dispatch sites BandDirector/BandCamShot) sits
in `src/system/bandobj/`, which the kickoff grants to nobody. An obedient lane
stalls at first edit or files an ownership exception; a sloppy one writes
outside its grant. Either way the wave's primary loses its first cycle.

**Amendment (adopt verbatim):** Lane A owned surfaces → "`src/system/bandobj/`
(named WRITE files: `BandCharacter.cpp`, `BandCamShot.cpp`, `BandDirector.cpp`)
+ `src/band3/game/` band-perf plumbing (WRITE, HX_NATIVE-gated)". Key sites →
"receiver `HANDLE(set_play, OnSetPlay)` `src/system/bandobj/BandCharacter.cpp:4344`,
rewrite `:4512`". Also mark lint 9's Lane A entry as re-verified with these
paths (as written the [x] was unearned — the check would have failed on the
named dir).

## A2 — MAJOR — after A1, Lanes A and D can collide on the same bandobj TUs; partition by file

**Kickoff sentences:** Lane D "Owned surfaces: probes + (conditionally) a narrow
walk-clip/prop-track scoping fix" (§Lane D) vs Lane A's corrected
`src/system/bandobj/` grant.

**Evidence:** every standing walker/char probe family Lane D would naturally
extend lives in the same TUs Lane A now owns: `RELOAD_PROBE` at
BandCharacter.cpp:50 (ledger:403), BANDPERF at BandCharacter.cpp:420-551. The
kickoff's collision audit ("A and D both read char/anim surfaces") only
considered reads.

**Amendment:** add to both lanes: "`src/system/bandobj/BandCharacter.cpp` and
`BandCamShot.cpp` are Lane A EXCLUSIVE-WRITE this wave. Lane D probes land
engine-side (char/rnd) or in a distinct TU; if Lane D's (iii)-undriven
conditional fix wants CharClip/CharDriver-adjacent writes, that requires
coordinator sign-off first (engine char is Lane A READ-ONLY under the carried
A8/W28 arbitration, so a D write there must be explicitly arbitrated, not
assumed)."

## A3 — MAJOR — Lane A's census gate: "sustained" is undefined and the census is gameable by a constant dispatch (the lever re-badged)

**Kickoff sentence:** acceptance "...sustained rhythm/solo `CHARDRV_PLAY` census
(Lane-1 A/B rerun, OFF=W29-idle baseline)...".

**Evidence:** the only quantified precedent is the NON-FAITHFUL lever's 55
(STATUS headline, `grep -acE "CHARDRV_PLAY.*clip='(stand_rhythm|stand_solo)"`).
A lane that dispatches one unconditional `set_play(P)` at song start reproduces
a nonzero "sustained" census while being exactly the lever moved one layer up —
the census alone cannot distinguish them.

**Amendment (three clauses):** (a) **sustained** := rhythm/solo `CHARDRV_PLAY`
plays present in ≥3 of 4 quartiles of the matched songMs window (same gz greps
as W30); (b) **non-gameable:** the mandatory dispatch-site hit log (lint 8) must
show `set_play` sends correlated to parsed song-authored mood events with ≥2
distinct intensity values across the window (or a quoted proof from the authored
data that the test song genuinely authors a single mood) — a constant
unconditional promotion is REJECTED as lever-equivalent; (c) **floor:** do NOT
grade against the lever's 55 — the faithful stream may legitimately idle during
breaks; grade count against the authored event count from discriminator (i).

## A4 — MAJOR — the E3 sit-churn bound is not operationalized ("same order" is not a number)

**Kickoff sentence:** "**no sit-group churn** (E3 bound: ON-run `grp='sit'`
BANDPERF_STATE same order as OFF, not thousands)".

**Evidence:** the E3 baseline is concrete: OFF `grp='sit'` = **16** rows, lever-ON
= **4373** (WAVE30_CLOSEOUT_REVIEW §1 / STATUS E3; A7 table 87→4444 total).

**Amendment:** operationalize as: "ON-run `grp='sit'` BANDPERF_STATE count ≤
10× the songMs-matched OFF-run count (OFF precedent: 16), and total
BANDPERF_STATE within ~2× OFF — computed by the same `grep -ac` on the committed
gz pair. Anything in the thousands = E3-class churn = acceptance FAIL."

## A5 — MAJOR — the F1-retest bullet's default expectation ("cones/fans gone") rests on a contested reading of W27(b); decide it at STEP 0, not at grading

**Kickoff sentence:** "F1 gameplay retest — drumstick/prop-tip bones driven,
cones/fans gone or explicitly re-scoped".

**Evidence:** README:889 records W27(b) as "prop-tip clip-track enumeration
NEGATIVE: `bone_pick_strum`/`bone_[LR]-tip_*` carry constant LocalXfm while
`bone_target_*` parents animate (**behavioral inference, E7-errata softened**)" —
observed at runtime when ONLY idle clips played. Whether the resident
`stand_rhythm_*`/`stand_solo_*` clips contain prop-tip tracks at all is
unmeasured; if they don't, a fully successful faithful dispatch still leaves the
fans, and the lane's primary visual bullet fails through no fault of the
mechanism.

**Amendment:** add to Lane A's STEP-0 discriminators: "(iii-b) static track
enumeration of the RESIDENT perf clips (`stand_rhythm_*`, `stand_solo_*`,
`sit idle_play_*`) for prop-tip tracks (`bone_pick_strum`, `bone_[LR]-tip_*`,
drumstick tips) — checkpointed BEFORE the multi-edit. If the perf clips carry no
prop-tip tracks, the 'cones/fans gone' leg is pre-agreed to grade as
'explicitly re-scoped' (F1 prop residual → its own future item), not as lane
failure."

## A6 — MAJOR — R-A answered: the data-absence branch is LIVE, so pre-authorize a bounded sub-scope with a hard edge

**Kickoff sentence (R-A):** "...is the acceptance then reachable this wave, or
should the kickoff pre-authorize a bounded data-plumbing sub-scope (and where's
its edge)?"

**Evidence:** grep of the current tree finds ZERO venue-mood handling anywhere:
no `set_play` sender (only the Symbol + receiver), no `[intense]`/`[mellow]`
event handling in `src/system/bandobj/BandDirector.*` (zero hits) or
`src/band3/game/` (only `set_play_all_tracks`, PracticePanel.cpp:445 — a
different symbol). The mood strings in BandCharacter.cpp:250-256 are group-name
mapping, not event parsing. So discriminator (i) finding "not routed natively"
is the EXPECTED outcome, not a tail risk.

**Amendment:** "Pre-authorized sub-scope: IF discriminator (i) shows the mood
events exist in natively-LOADED song/venue data (parsed DataArrays / anim events
already in memory) but unrouted, Lane A may land the routing as part of the
multi-edit (edge: ≤2 new-touched TUs, no new file-format parsing, no engine
writes). IF the events are absent from natively-loaded data (a loader/format
gap), STOP → priced NO-GO memo naming the missing data layer; do not start
loader work this wave." This resolves R-A; the acceptance stays reachable on the
first branch and honestly NO-GOes on the second.

## A7 — MAJOR — R-B answered: tolerance removal must be a post-merge coordinator commit, and the kickoff misses the second tolerance site

**Kickoff sentences:** "drawlog-golden + lineup gates PASS with the rc-tolerance
lines REMOVED from `drawlog-golden.py`" and "sequenced LAST, after A/B/D harness
runs complete — coordinator lands it".

**Evidence:** the tolerance class has (at least) TWO sites:
`scripts/native/drawlog-golden.py` docstring 186-190 + rc-note 234-237, AND
`scripts/native/song-end-test.py:269` (`if proc.returncode in (134, 139, -6,
-11)`) — the kickoff cites both but only charters removal of the first. No gate
asserts a NONZERO teardown rc (drawlog-golden's rc!=0 path only logs; rc=0
passes today), so removal is safe once the fix lands.

**Amendment:** "The tolerance-line removal is a SEPARATE coordinator commit
AFTER all lanes have merged, gated on 10/10 rc=0 bounded non-HTTP boots
reproduced on the FINAL merged tree (not the lane's tree). Scope of removal =
drawlog-golden.py:186-190,234-237 AND song-end-test.py:269 (which currently also
masks SIGABRT 134) — each site either removed or explicitly kept with a comment
naming why. 'Sequenced LAST inside the wave' alone is insufficient: a mid-wave
removal races any A/B/D re-run on a pre-fix binary."

## A8 — MINOR — Lane B's conditional engine-write grant needs a named-TU checkpoint; ledger adjacencies enumerated

**Kickoff sentence:** "Owned surfaces: engine `RB3MaterialBinder.cpp`/texture-bind
path IF the mechanism lands there ... else game-side UI."

**Evidence:** the grant as written lets Lane B roam engine-vs-game at its own
discretion with no checkpoint. `RB3MaterialBinder.cpp` (verified at
`milo-native-engine/src/platform/`) already hosts the `RB3_HUB_TEXT_CONTRAST`
highlight-alpha clamp (ledger:218); adjacent shipped flags a "white glyph /
tint dropped" theory must be checked against: `RB3_UI_TEXT_FLOOR_RELAXED`/
`RB3_UI_TEXT_FLOOR_STRICT` (ledger:383, the W4.2/W7 text floor),
`RB3_ROWFIX_OFF` (:339), `RB3_HUD_SCOREBOARD_TOPRIGHT` (:219, the score-pill
POSITION history — position ≠ F2's translucency, don't conflate), and the
FilterSubdir white-texture shim history in `RB3_HANDS_BINDFIX` (:194) — the one
established "unbound → white" mechanism in this codebase.

**Amendment:** "Lane B's first WRITE (engine or game) requires a checkpointed
STEP-0 mechanism-location finding naming the TU(s), plus coordinator ack for the
engine branch (an engine write commits to the ONE close-out pin bump). The
lint-4 grep must cover at minimum the five ledger rows above; do not disturb the
RB3_HUB_TEXT_CONTRAST clamp when editing RB3MaterialBinder.cpp."

## A9 — MINOR — Base SHA is stale; restate at dispatch

**Kickoff sentence:** "**Base SHA (rb3):** `3a314d6c`."

**Evidence:** HEAD at review time is `bb750efa` (the kickoff commit itself),
with intervening `86646f94` ("web: URL-base shim", 05:33) between `3a314d6c`
(05:28) and it. Web-only; no lane surface touched. The deployed web build
timestamps (native/web/build/release/* at 05:35) POST-date the shim, so the
kickoff's "deploy is FRESH (05:17)" is superseded but still true in spirit.

**Amendment:** restate "Base SHA (rb3): `bb750efa` (or the post-review
amendment commit); intervening `86646f94` is the concurrent web track, no lane
overlap. Lane C rider: verify the deploy it grades includes `86646f94` (disk
says yes, 05:35) and note whether the user's report channel is the `/rb3`
reverse-proxy path that shim serves — if so the user's build lineage predates
it, which bears on the stale-build disposition of bug 1."

## A10 — MINOR — the W26 teardown is a MENU-transition panel-unload, not "in-song"

**Kickoff sentence:** "...and from the W26 in-song panel-unload teardown — do NOT
conflate, README:858."

**Evidence:** README:858 (Wave 26 table): "a **UI PANEL-UNLOAD teardown** on
**splash→main_hub** (WorldDir::~WorldDir → CharClipSet dtor → Replace(clip,NULL),
UIScreen.cpp:570)".

**Amendment:** "...the W26 splash→main_hub panel-unload teardown...". Keeps Lane
C's don't-conflate list accurate — an agent hunting an "in-song" W26 trap would
be hunting a phantom.

## A11 — MINOR — the web-capture rider should name the actual harnesses (they exist; "the Playwright harness" is underspecified)

**Kickoff sentence:** "boot the deployed web release build via the Playwright
harness, capture hub top-level + submenu focused states...".

**Evidence:** two usable committed harnesses verified: 
`scripts/web/menuhub-probe.mjs` (splash→main_hub + screenshot, shared
`scripts/web/lib/core.mjs` launch/navigate/capture) and
`scripts/web/keyboard-to-gameplay.mjs` (pure-keyboard splash→hub→song_select
with real key events — gives the FOCUSED/submenu states the rider needs). Both
import playwright/chromium.

**Amendment:** name them in the rider: "via `scripts/web/menuhub-probe.mjs` +
`scripts/web/keyboard-to-gameplay.mjs` (or a throwaway composed from
`scripts/web/lib/core.mjs`)". The rider is feasible as chartered.

## A12 — MINOR — R-D answered: (iii) is NOT reliably decidable from track enumeration alone; pre-authorize a read-only live-bone probe inside the diagnosis grant

**Kickoff sentence (R-D):** "is it actually decidable from track enumeration
alone, or does it need a live-bone A/B (and if so, is that in-scope for a
diagnosis lane)?"

**Evidence:** the precedent the kickoff itself cites — W27(b) — was downgraded
by its own close-out to "behavioral inference, E7-errata softened" (README:889):
enumeration observed constant LocalXfm at runtime rather than proving track
absence. And distinguishing "driven in the wrong basis" (SKEL family) from
"undriven" REQUIRES observing the bone's live transform vs its rest pose;
enumeration only answers track-presence.

**Amendment:** "Lane D (iii) is pre-authorized to include a READ-ONLY live-bone
transform probe (matrix-relative + pointer-verified per lint 1; per-walker rows
per lint 2; keyed by object pointer per E7) alongside the track enumeration.
This is diagnosis instrumentation, not fix code — the three-supersessions STEP-0
bar still holds; NO fix code before the checkpointed verdict."

---

## Checklist deltas (what the ten-lint [x]s are worth after this review)

- Lint 9 (flavor membership): **unearned for Lane A as drafted** — the named dir
  doesn't exist (A1). Earned once A1's paths are adopted (globs verified in
  native/CMakeLists.txt:268,271).
- Lint 3 (validated oracles): Lane A's census is validated but **gameable
  without A3(b)**; with A3 adopted, earned.
- All other [x]s check out against the tree (lint 4's Lane D flip-survival claim
  verified in Q(c)#5/Q(f); lint 7's gitignore amendment matches `387a70bf`).

*Reviewer note: no source edits, no builds, no process kills; all evidence from
reads/greps of the current tree at `bb750efa` plus committed W29/W30 artifacts.*
