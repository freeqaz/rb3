# WAVE 32 KICKOFF — WEB-YELLOW + PROP-FAN + HUD-F2F4 + ARG-ORDER-AUDIT

Coordinator: Fable main loop. Dispatch: ultracode Workflow (4 Opus lanes + 1
Sonnet rider + Opus countersign). Base SHA rb3 `11d1ad1b`; engine pin
`24c4f95` (== engine HEAD).

**Binding process rules:** the Process rules, ten-lint pre-dispatch checklist,
checkpoint-before-fix discipline, A7 raw-log mechanics, E4 verbatim-quote
rule, and evidence convention (evidence/ on disk gitignored + findings quoted
in tracked STATUS.md) of `WAVE31_KICKOFF.md` apply **verbatim, incorporated by
reference** — every lane reads WAVE31_KICKOFF.md §Process before starting.
`WAVE31_CLOSEOUT_REVIEW.md` rulings carry: three-tier flag rule
(decomp-correctness → UNCONDITIONAL; retail-proven faithful restoration →
default-ON + `RB3_NO_*` opt-out, earned by ON-vs-OFF evidence;
uncertain → default-OFF opt-in), Lane-D-style STOP verdicts are BINDING,
new TUs in globbed dirs must compile clean before yielding (E5), and no lane
re-merges F2/F4 into "one family" (refuted W31).

Checkpoints: `/tmp/wave32-checkpoints/<lane>.json` (check-first,
write-before-return). Lane build dirs: `native/build-agent-W32-<LANE>` (never
the shared `native/build-native`). Builds via `tools/ninja-locked` for Wii
objdiff; cmake for native. Harness: `RB3_HTTP=1 RB3_FIXED_CLOCK=1`, free
ports, pgid-only cleanup; `boot-to-song.py` / `song-select-capture.py`
canonical.

## Coordinator pre-kickoff dispositions

- W31 riders (a) F3 evidence re-home and (b) SyncProperty batch_objdiff
  countersign: **already discharged** at W31 close-out (`5e6eae3b` addendum).
- Rider (c) probe disposition: `RB3_SETPLAY_PROBE` **KEEP** (acceptance
  instrument for the set_play stream); `RB3_SHARD_PROBE_SCENE/_OUT` +
  `rb3_shardprobe_native.cpp` **retire at W32 close-out** (family STOPPED,
  question answered) unless a W32 lane states a concrete need in its STATUS.
- F5 remains NOT chartered (needs fresh hypothesis + own oracle). F6 remains
  BLOCKED on UIGRADE reconciliation. SKEL/CROWD families CLOSED — any lane
  whose diagnosis lands in those families writes a STOP memo and stops.

## Lane A — W32-WEB-YELLOW (primary; Opus)

**Target:** the user's floating yellow square: a solid flat yellow-green
quad over the lead character's torso on `main_hub_screen` (overshell
`joined_default`), STATIC across focus moves (ArrowDown moves the real focus
highlight correctly; the quad stays). **Web release build ONLY — native is
clean** (W31 rider CONFIRMED_ON_WEB, A9 deploy-freshness verified). This is a
chartered exception to the debug-native-first rule: the defect is genuinely
web-specific; still use native as the CONTROL (same screen, same nav, quad
absent).

- **STEP-0 (blocking, no fix before this):** NAME the quad — mesh/draw/screen
  element — on the web build, and name the web-vs-native divergence point.
  Entry hypothesis (W31 rider): a highlight-mesh instance surviving the
  `options`→`joined_default` overshell transition. Candidate mechanisms to
  discriminate: (i) B8 hub-highlight fix (`highlight_main`/`highlight_pattern`
  prelit routing in `rb3_render_hook.cpp`) behaving differently under
  Emscripten; (ii) web data-symbol zero-resolution class
  (`web_data_stubs.cpp` precedent — a state read that is garbage/0 on web);
  (iii) an overshell flyout auto-open sequence that only occurs on web
  leaving a stale quad. Tools: `scripts/web/menuhub-probe.mjs`,
  `scripts/web/_fullboot*.mjs` patterns, drawlog/uidump equivalents on the
  web debug build (`?debug=true`), native control captures.
- Fix per the three-tier flag rule. Iterate on the **debug** web build
  (`scripts/web/build.sh --debug`, gzip-only, fast); ONE final release build +
  deploy check at the end.
- **Owned surfaces:** `scripts/web/`, `native/web/`, web-specific
  `native/src/` glue. `src/system/bandobj/OvershellDir.cpp` and shared UI/hub
  TUs are SHARED — any shared-TU edit requires a native A/B (hub capture +
  drawlog-golden PASS) in the same STATUS section, and Wii `.o` neutrality
  (objdiff unchanged) if the TU is a decomp TU.
- **Acceptance:** (1) quad named (mesh + draw evidence quoted); (2) web-only
  mechanism named with the divergence point cited file:line; (3) after-fix
  web screenshot pair (quad gone; real focus highlight still tracks
  ArrowDown/ArrowUp both directions); (4) native control unchanged
  (drawlog-golden PASS, hub capture visually unchanged); (5) no regression of
  B8 (hub focus highlight still yellow, not black).

## Lane B — W32-PROP-FAN (F1 family; Opus)

**Target:** the re-scoped second half of the floating-legs user report:
prop-tip fan/cone artifacts — drumstick tips, guitar neck, kit cones, the
magenta stick-fan guitar — driven by instrument-MIDI drivers (`strum.dmidi`,
`fret.ikmidi`, `right_hand.dmidi`), NOT the mood/set_play stream (W31 Lane A
A5 re-scope).

- **Opens with the E7 debt (blocking):** matched-songMs BAND-FRAMING crop
  pairs via the boot-to-song closeup harness (`band-closeup-capture.py` /
  `boot-to-song.py`), capturing the prop-tip artifacts as they stand today.
  These crops are the wave's before-baseline; without them the lane may not
  proceed to diagnosis (WAVE31_CLOSEOUT_REVIEW §5 debt).
- **Discriminator-first:** are the dmidi/ikmidi prop drivers (a) bound and
  fed (nonzero MIDI-derived streams reaching the bones), (b) bound but
  STARVED (stream dead natively — the set_play precedent), or (c) driven but
  wrong-basis (SKEL class)? Read-only default-OFF probe if needed (distinct
  TU or existing BONE_PROBE family; E5 compile-clean rule).
- **Fix legal ONLY on branch (b) starved/routing** (the W31 Lane A pattern —
  look for a decomp-bug sender first: the arg-order lesson says check the
  dispatch path's ≥99% functions before writing native code). Branch (c) →
  STOP memo (SKEL family, closed). Branch (a) with visual artifact anyway →
  diagnosis memo + recharter proposal, no fix.
- **Owned surfaces:** `src/band3/` instrument-track/dmidi senders,
  `src/system/char/` driver binding (exclusive-write vs other lanes);
  BandCharacter.cpp NOT owned (W31 Lane A surface — read-only here).
- **Acceptance:** (1) crop-pair baseline committed-quoted; (2) discriminator
  verdict with census numbers; (3) if fixed: after crops with fans/cones
  gone + set_play A/B unchanged (do NOT regress W31: CHARDRV_PLAY ~80,
  3 intensities, sit ~26) + objdiff clean; (4) if STOP/memo: mechanism named
  with pointer-keyed evidence.

## Lane C — W32-HUD-F2F4 (Opus)

**Target:** the two priced Lane-B split sub-charters. TWO mechanisms, TWO
checkpoints (`W32-HUD-F2.json`, `W32-HUD-F4.json`); the one-family hypothesis
is REFUTED — do not re-merge.

- **F2 score-pill fill (MEDIUM):** native pill = white/light glossy face +
  dark digits; retail = dark silver-rimmed face + white digits. NOT the glyph
  family (named HUD panel mesh, not unnamed RndText). STEP-0: HEADMAT-style
  material dump keyed to the pill mesh name (candidates: texture unbound →
  white fallback; prelit/unlit white base; blend translucency). Then
  bind/blend fix per flag tier.
- **F4 star-row unearned slots (LOW-MEDIUM):** retail draws 5 slots (filled +
  dim outlines); native draws only earned. Show-state/anim-frame mechanism,
  not texture. STEP-0: enumerate the star-row milo group's slot meshes +
  what hides unearned ones natively vs retail.
- **Owned surfaces:** `native/src/rb3_render_hook.cpp` is PRE-AUTHORIZED for
  append-only predicate/policy additions gated per the flag tier rule
  (ON-vs-OFF captures mandatory for any default-ON) — the W31 A8 stall is
  not repeated; engine (`../milo-native-engine`) writes remain
  engineAckNeeded (named-TU checkpoint + coordinator ack, no push).
  `src/band3/` HUD/track UI TUs shared with nothing else this wave.
- **Acceptance per item:** mechanism named (dump quoted) → fix → retail-paired
  before/after crops at matched frames → drawlog-golden PASS → objdiff clean
  if any decomp TU touched.

## Lane D — W32-ARG-ORDER-AUDIT (Opus)

**Target:** the W31 audit lesson industrialized as a pilot: arg-order decomp
bugs (same-typed adjacent args swapped at a call site) hide at ≥99% match and
can silence whole subsystems (SyncProperty precedent: 99.96% hid a dead
animation stream for 31 waves).

- **Enumerate:** `build/SZBE69_B8/report.json` functions with fuzzy ≥99.0 and
  <100, prioritized `src/band3/` + platform-agnostic `src/system/` (port
  surface). Filter to candidates whose residual is a CALL-SITE argument
  pattern: use `run_diff_inspect` (`regswaps`, `mismatches`) and look for
  swapped/rotated argument registers (r3..r10 / f1..f8) feeding a call, on
  same-typed args.
- **Verify before fixing:** for each candidate, confirm against Bank 8
  (`bin/analyze-function`, `scripts/analysis/bank_divergence.py` if body-era
  matters) that RETAIL's arg order differs from our source — not regalloc
  noise.
- **Fix = swap args in source. Accept ONLY if objdiff reaches 100.0%
  (COMPLETE)** — self-verifying, no fake-match risk (no-asm rule binding).
  Anything short of 100% after the swap → revert, record as
  not-this-class. Skip functions with known at-limit/permuter-class notes in
  the execution docs.
- **Behavioral triage (the real payoff):** for each landed fix, state what
  the swap DID at runtime (dead message? wrong param? benign?) — a
  SyncProperty-class find (silenced subsystem) is worth more than ten benign
  swaps; flag any such find prominently for the close-out.
- **Scope cap:** shortlist up to ~25 candidates; land up to ~10 fixes; report
  the rest as a ranked backlog. UNCONDITIONAL tier (decomp-correctness), one
  commit per fix or small batches per TU, `tools/ninja-locked` only.
- **Acceptance:** candidates table (symbol, unit, %, residual signature,
  verdict); each landed fix at 100.0% quoted from objdiff; behavioral triage
  line per fix; no regressions (spot drawlog-golden if any fixed TU is
  render/anim-adjacent).

## Rider — W32-F7-CLIP-DIAG (Sonnet, diagnosis-only, NO code)

F7 song-select right-edge clipping (user-repeated ×2): the backdrop
character's thighs clip in at the right edge AND show through the sidebar
rows. STEP-0 questions: (1) what clips the character (camera frustum,
scissor, viewport, authored placement)? (2) does RETAIL song_select have an
opaque panel behind the per-song sidebar column (check
`images/retail-screenshots/` + song_select milo assets), i.e. is our gap a
missing panel draw or a missing depth/stencil mask? Deliverable: mechanism
memo + candidate fix surface + W33 charter draft in STATUS. Evidence: crops
with the sidebar focused (use `song-select-capture.py`, focus a real song
row — heading-row trap noted W31).

## Countersign (Opus, pre-E1)

Re-derive every lane's headline numbers from raw committed artifacts (gz
logs, json dumps, quoted STATUS evidence) per the W27 lesson; verify Lane D's
100.0% claims with an independent `batch_objdiff` over its landed symbols;
write `WAVE32_COUNTERSIGN.md` + commit. Flag any number that exists only in
an agent's build dir (the W31 Lane C gap class).

## Close-out (coordinator-owed)

Lane acks (engineAckNeeded adjudication), flag-tier confirmations + any
earned flips, shard-probe retirement per disposition above, census regen
(single, AFTER retirement/flips), ONE pin bump iff engine moved,
WAVE32_CLOSEOUT_REVIEW.md via Fable reviewer, README results + Wave-33 menu,
user report with all user-visible dispositions (yellow square, prop fans,
score pill, stars, clipping).
