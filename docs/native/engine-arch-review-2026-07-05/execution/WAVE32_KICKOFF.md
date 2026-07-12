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

---

# COORDINATOR ACCEPTANCE (BINDING) — A1–A14 adopted 2026-07-12

`WAVE32_REVIEW.md` amendments A1–A8 (BLOCKING) and A9–A14 (ADVISORY) are ALL
adopted. Where an amendment gives replacement text, that text SUPERSEDES the
corresponding kickoff sentence above. Lanes read WAVE32_REVIEW.md in full;
its rulings bind. **Base SHA for all lanes = this acceptance commit.**

- **A1 (Lane A STEP-0 tooling):** superseded tools sentence — web side =
  Playwright console capture + `window.rb3*` engineState exports
  (`scripts/web/lib/core.mjs`) + screenshots; `menuhub-probe.mjs` /
  `_fullboot*.mjs` are nav/boot patterns ONLY (their `[MENU_DBG]` engine hook
  is retired — the dump will be empty against today's build). For draw-level
  naming on web, Lane A adds its OWN env-gated default-OFF draw-time
  mesh-name console probe in owned web glue (or a `window.*` export) —
  `/api/drawlog` + `/api/uidump` do NOT exist on the web target
  (`rb3_http_server.cpp` is native-only per the CMakeLists EMSCRIPTEN
  branch). Native CONTROL: use the real `/api/uidump` + `/api/drawlog`
  (RB3_HTTP) to enumerate the expected hub highlight/overshell mesh set,
  then cross-name the web orphan against it.
- **A2 (render-hook fence):** `native/src/rb3_render_hook.cpp` is Lane C
  **exclusive-write** this wave. Lane A reads freely; if Lane A's named
  divergence point lands in that TU (mechanism (i)), Lane A checkpoints
  COORDINATOR-ACK-NEEDED and waits for arbitration — no concurrent writes.
- **A3 (Lane D claim protocol, BINDING):** each lane writes its owned/claimed
  TU list to `/tmp/wave32-claims/<lane>.txt` at start and updates it when
  STEP-0 names new TUs. Before landing ANY fix, Lane D checks the union of
  claim files plus this seed exclusion list:
  `src/system/bandobj/BandDirector.cpp`, `src/system/bandobj/BandCharacter.cpp`,
  `src/system/bandobj/OvershellDir.cpp`, `src/system/char/CharDriver*.cpp`,
  `src/system/char/CharDriverMidi.cpp`, `src/system/char/CharIKMidi.cpp`,
  `src/system/char/CharIKSliderMidi.cpp`, plus any TU named in another lane's
  checkpoint. A candidate in a claimed/excluded TU goes to the ranked BACKLOG
  with its verdict (coordinator may land at close-out) — it does NOT land
  mid-wave.
- **A4 (fresh report):** Lane D's FIRST action:
  `tools/ninja-locked build/SZBE69_B8/report.json` on the base-SHA tree
  BEFORE enumeration; quote the regen timestamp in STATUS. (The on-disk
  report is 2026-07-10, pre-W31 — SyncProperty would be a phantom #1 hit.)
- **A5 (Lane D gate, ruling adopted verbatim):** a swap that takes the
  function to **raw 100.0% / verdict COMPLETE** is by construction what mwcc
  compiled — semantic regression against retail is impossible; the gate is
  self-verifying. Tightenings: (1) the gate is RAW 100.0 (COMPLETE), not
  fuzzy-only; (2) **unit neutrality** — after each landing, run batch_objdiff
  over the unit and quote that no sibling function regressed.
- **A6 (at-limit source):** superseded skip sentence — "Skip functions the
  orchestrator DB marks at_limit (`query_functions status='at_limit'`; check
  `get_attempts` on each shortlisted symbol) and those on the docs/decomp
  deep-dive AVOID lists (`docs/decomp/deep-dive-targets-2026-05-26.md`
  §AVOID, `docs/decomp/analysis-20260530.md`)."
- **A7 (Lane B surface, lint 9 re-earn):** superseded owned-surfaces —
  "`src/system/char/CharDriverMidi.cpp` / `CharIKMidi.cpp` /
  `CharIKSliderMidi.cpp` (exclusive-write); `src/band3/` sender TU(s) owned
  ONLY AFTER STEP-0 derives and checkpoints their real names (no `src/band3/`
  write before that checkpoint); BandCharacter.cpp read-only per A9."
- **A8 (Lane B discriminator):** added branch **(a0) drivers not
  bound/created natively** → fix legal under the same terms as (b) IFF the
  root cause is a decomp/routing bug (check the dispatch path's ≥99%
  functions FIRST — arg-order lesson); otherwise memo. The discriminator
  verdict is rendered **per prop class** (sticks / neck / kit cones /
  stick-fan guitar), pointer-keyed and matrix-relative per lints 1/2 — one
  aggregate row is not acceptance.
- **A9:** BandCharacter.cpp = read-only by default; coordinator arbitration
  required for any write — no W32 lane owns it (it is on A3's exclusion list,
  so Lane D cannot collide).
- **A10 (Lane A anti-gaming):** the fix must act at the divergence point
  named in acceptance leg (2) — a suppression keyed to position/size/screen
  region rather than to the named orphan mechanism is NOT acceptance.
  After-fix evidence must include a replay of the `options`→`joined_default`
  transition showing the highlight lifecycle correct through it, both
  directions of focus travel.
- **A11 (Lane C anti-gaming):** F4 acceptance = captures at TWO matched
  states with different earned counts (filled == earned AND dim == 5−earned
  in BOTH, retail-paired). F2 fix must act on the mechanism the material dump
  names (bind/blend/prelit), NOT a hardcoded pill tint; digits legible
  white-on-dark per the retail pair.
- **A12 (dropped items back on ledger):** close-out records dispositions for
  **F8** (settle-frame recapture — carried, not chartered) and **E6**
  (taxonomy: two default-ON fixes now sit in class=workaround inflating the
  §W5.3 metric — owed to the registry owner: a fix-opt-out class or metric
  exclusion).
- **A13 (lints instantiated for W32):** lint 9 discharged by A7; lint 10 by
  A1; lint 2 by A8. Additionally binding: **lint 4 for Lane C** — registry
  sweep BEFORE any "engine drops X" mechanism claim, incl.
  `RB3_HUB_TEXT_CONTRAST`, the W4.2 text floor, ROWFIX, SCOREBOARD_TOPRIGHT,
  W2.7 FilterSubdir, and BOTH new default-ONs (`RB3_NO_HUB_HIGHLIGHT_FIX`,
  `RB3_NO_BUTTON_GLYPH_FIX`); **lint 1 for Lane B** — all bone claims
  matrix-relative + pointer-keyed; **lint 7 for all lanes** — evidence on
  disk under `execution/W32-*/evidence/` (gitignored) + findings QUOTED in
  tracked STATUS.md.
- **A14:** Lane C engine commits: **never stage
  `src/platform/FxSendNative.cpp`** (untouchable concurrent WIP in the
  engine tree).
