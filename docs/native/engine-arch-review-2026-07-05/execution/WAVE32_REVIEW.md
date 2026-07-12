# WAVE 32 PRE-DISPATCH REVIEW — adversarial kickoff audit

**Reviewer:** Fable pre-dispatch agent, 2026-07-12. **Input:** WAVE32_KICKOFF.md
(`3d9f3492`), audited against WAVE31_KICKOFF.md §Process + ten lints,
WAVE31_CLOSEOUT_REVIEW.md (+ discharge addendum), README Wave-32 menu, and the
ACTUAL TREE (every path, tool, flag, and claim below was spot-checked on disk —
nothing accepted on the kickoff's word). Read-only on code; this doc is the
only write.

## Verified clean (no amendment needed)

- **Base SHA / pin:** `11d1ad1b` == HEAD~1 (parent of the kickoff commit) —
  correct, no W31-A9-class staleness. Engine `24c4f95` == engine HEAD; the
  only dirt is the known untouchable `M src/platform/FxSendNative.cpp`.
- **Tools all exist:** `scripts/web/menuhub-probe.mjs`,
  `scripts/web/keyboard-to-gameplay.mjs` + `lib/core.mjs`,
  `scripts/web/_fullboot*.mjs` (×3), `scripts/native/band-closeup-capture.py`
  (real matched-(shot,songMs) band-framing pin harness — the E7 debt is
  dischargeable as chartered), `boot-to-song.py`, `song-select-capture.py`,
  `drawlog-golden.py`, `scripts/analysis/bank_divergence.py`,
  `bin/analyze-function`, `native/web/server.py`, `scripts/web/build.sh`
  (`--debug` gzip-only fast loop + `?debug=true` no-store dual build: real).
- **Flags all match the registry**
  (`../milo-native-engine/src/platform/NativeCompatFlags.classification.json`):
  `RB3_SETPLAY_PROBE` class=probe default=off; `RB3_SHARD_PROBE_SCENE`/`_OUT`
  probe/off; `RB3_NO_BUTTON_GLYPH_FIX` default=on; `RB3_NO_HUB_HIGHLIGHT_FIX`
  default=on; `RB3_BAND_PERF_FORCE_PLAY` ABSENT (retired, as claimed).
- **W31 riders (a)/(b) genuinely discharged:** the `5e6eae3b` close-out review
  carries the coordinator discharge addendum — F3 evidence re-homed to
  `W31-HUD-GLYPHS/evidence/f3-closeout/`; SyncProperty independent
  batch_objdiff **fuzzy 100.0 / raw 100.0 COMPLETE** quoted as the committed
  record. Kickoff's "already discharged" claim is TRUE.
- **Rider F7 material exists:** `images/retail-screenshots/` has
  `yt_qRagnZCIMzk_song_select_{list,diff_ratings,filter_panel,album_art}.png`
  + `yt_qSRJ8HHPXzM_song_select_wii.png`.
- **Ten-lint incorporation-by-reference is usable:** WAVE31_KICKOFF.md
  §"Process rules (carried)" (lines 75–94) and §"Pre-dispatch checklist"
  (96–107) exist, each lint states its general rule self-containedly before
  the W31-specific application. A lane agent can extract the rules. (But see
  A13 — the checkboxes are instantiated for W31 lanes, not W32.)
- **Countersign lane is executable:** `batch_objdiff` exists (orchestrator
  MCP) for the Lane-D 100.0% re-derivation.
- **Menu coverage:** items 1–4 → Lanes A/B/C + rider F7; item 5 (F5) correctly
  NOT chartered; rider (c) probe dispositions present; rider (d) → Lane D.
- **Sizing:** each lane is one-Opus-sized (C's two priced sub-charters MEDIUM
  + LOW-MEDIUM; D is bounded at ~25 shortlist / ~10 landings). No lane is
  egregiously over- or under-scoped.

---

## Amendments

### A1 (BLOCKING) — Lane A STEP-0 tooling is DOUBLY over-promised: no drawlog/uidump on web, and menuhub-probe's engine hook is GONE

Two independent tree facts against the kickoff's tool list:

1. `/api/drawlog` and `/api/uidump` are endpoints of
   `native/src/rb3_http_server.cpp` (`:369`, `:385`) — and the web target's
   source list (`native/CMakeLists.txt` `else() # EMSCRIPTEN` branch, line
   769ff) **excludes** `rb3_http_server.cpp` (vendored httplib pulls POSIX
   sockets + threads). There is **no drawlog/uidump equivalent reachable on
   the web build**, `?debug=true` or otherwise. The README menu made this
   error first ("uidump/drawlog on the web build") and the kickoff inherited
   it.
2. `scripts/web/menuhub-probe.mjs` works by setting `ENV.MENU_DBG=1` and
   filtering console for `[MENU_DBG]` lines from "the engine's
   BandRnd::DrawMesh" — but **`MENU_DBG` appears NOWHERE in the current
   engine or rb3 trees** (grep of both src trees + `git log -S`: zero hits;
   consistent with the W30 probe retirement). The probe still boots/navigates/
   screenshots, but its mesh-NAMING half will dump zero lines against today's
   build.

Since STEP-0 is blocking ("no fix before this"), dispatching Lane A with dead
naming instruments guarantees a stall or an improvised-probe scramble.

**Change** the Lane A tools sentence from:

> Tools: `scripts/web/menuhub-probe.mjs`, `scripts/web/_fullboot*.mjs`
> patterns, drawlog/uidump equivalents on the web debug build
> (`?debug=true`), native control captures.

**to:**

> Tools — web side: Playwright console capture + `window.rb3*` engineState
> exports (`scripts/web/lib/core.mjs`) + screenshots;
> `menuhub-probe.mjs`/`_fullboot*.mjs` as nav/boot patterns ONLY (their
> `[MENU_DBG]` engine hook was retired — the dump will be empty). For
> draw-level NAMING on web, Lane A adds its own env-gated (default-OFF)
> draw-time mesh-name console probe in owned web glue, or a `window.*`
> export — drawlog/uidump do NOT exist on the web target
> (`rb3_http_server.cpp` is native-only, CMakeLists EMSCRIPTEN branch).
> Native CONTROL: use the real `/api/uidump` + `/api/drawlog` (RB3_HTTP) to
> enumerate the expected hub highlight/overshell mesh set, then cross-name
> the web orphan against it.

### A2 (BLOCKING) — `rb3_render_hook.cpp` is granted to Lane C but named in Lane A's mechanism list: collision fence needed

Lane C: "`native/src/rb3_render_hook.cpp` is PRE-AUTHORIZED for append-only
predicate/policy additions". Lane A's candidate mechanism (i) is "B8
hub-highlight fix (`highlight_main`/`highlight_pattern` prelit routing in
`rb3_render_hook.cpp`) behaving differently under Emscripten" — verified: the
`RB3_NO_HUB_HIGHLIGHT_FIX` predicate lives in exactly that TU (+ engine
`RB3MaterialBinder.cpp`). If Lane A's divergence point lands on mechanism (i),
two lanes write one TU mid-wave — the A2/E5 collision class W31 just spent an
erratum on. `rb3_render_hook.cpp` is also NOT covered by Lane A's grant as
written ("web-specific `native/src/` glue" — it is a SHARED native+web TU).

**Add to both lane sections:** "`native/src/rb3_render_hook.cpp` is Lane C
**exclusive-write** this wave. Lane A reads freely; if Lane A's named
divergence point lands in that TU (mechanism (i)), it checkpoints
COORDINATOR-ACK-NEEDED and waits for arbitration (expected outcome: a
coordinator-sequenced edit or a handoff — not concurrent writes)."

### A3 (BLOCKING) — Lane D needs an exclusion list + claim protocol; "isolation" as written is insufficient

Lane D lands UNCONDITIONAL fixes in arbitrary ≥99% TUs across `src/band3/` +
`src/system/` — which OVERLAPS Lane B's owned surface (`src/system/char/`
driver binding + `src/band3/` senders), Lane C's `src/band3/` HUD TUs, and
Lane A's shared-TU rider (`src/system/bandobj/` hub/UI TUs). Two failure
modes: (a) git/textual collision on concurrent edits; (b) worse — an arg-order
fix **changes runtime behavior by design** (that is the SyncProperty payoff),
so a D landing inside another lane's diagnosis surface mid-wave silently
invalidates that lane's baselines and A/Bs.

**Add to Lane D:**

> **Claim protocol (BINDING):** each lane writes its owned/claimed TU list to
> `/tmp/wave32-claims/<lane>.txt` at start and updates it when STEP-0 names
> new TUs. Before landing ANY fix, D checks the union of claim files plus this
> seed exclusion list: `src/system/bandobj/BandDirector.cpp`,
> `src/system/bandobj/BandCharacter.cpp`,
> `src/system/bandobj/OvershellDir.cpp`, `src/system/char/CharDriver*.cpp`,
> `src/system/char/CharDriverMidi.cpp`, `src/system/char/CharIKMidi.cpp`,
> `src/system/char/CharIKSliderMidi.cpp`, plus any TU named in another lane's
> checkpoint. A candidate in a claimed/excluded TU goes to the ranked BACKLOG
> with its verdict (the coordinator may land it at close-out, after lane
> baselines are frozen) — it does NOT land mid-wave.

### A4 (BLOCKING) — Lane D must regen report.json before enumerating: the on-disk one is stale

`build/SZBE69_B8/report.json` is dated **2026-07-10 22:45**; decomp TUs
changed after it (`a3916764` BandDirector.cpp — SyncProperty is 100.0% on the
tree but still 99.96 in the report; `6ccc36e3` BandCharacter.cpp; `b94e6a0a`
CharDriver.cpp). Enumerating from it yields phantom candidates (the #1
would-be hit, SyncProperty, is already fixed).

**Add as Lane D's first action:** "`tools/ninja-locked
build/SZBE69_B8/report.json` on the base-SHA tree BEFORE enumeration; quote
the regen timestamp in STATUS."

### A5 (BLOCKING) — Lane D acceptance ruling: the 100.0%-or-revert logic HOLDS, with two tightenings

**Ruling (adopt verbatim):** a swap that takes the function to **raw 100.0% /
verdict COMPLETE** is by construction what mwcc compiled — the emitted bytes
equal retail's, so semantic regression against RETAIL is impossible; the
gate is self-verifying and the no-asm/fake-match risk is nil. The logic is
sound. Two tightenings so it cannot be gamed or leak collateral:

1. The gate is **raw 100.0 (COMPLETE)**, not fuzzy-only — state "raw"
   explicitly in the acceptance line.
2. **Unit neutrality:** a swap in a shared header/inline (or any TU rebuild
   side-effect) can shift SIBLING functions. After each landing, run
   `batch_objdiff` (or objdiff) over the unit and confirm no OTHER function
   regressed; quote the unit-neutral result alongside the 100.0%.

(Native-side behavior change is intended and is covered by the existing
drawlog-golden spot-gate leg — no change there.)

### A6 (BLOCKING) — Lane D at-limit pointer is wrong: "the execution docs" contain no at-limit registry

The kickoff says "Skip functions with known at-limit/permuter-class notes in
the execution docs." Verified: `execution/` holds the arch-review campaign,
not decomp at-limit notes. The findable, verified sources are:

- the **orchestrator DB**: `query_functions status='at_limit'` (works,
  spot-checked) + `get_attempts(symbol)` for per-function history;
- `docs/decomp/deep-dive-targets-2026-05-26.md` §"AVOID — documented
  at-limit" (+ `docs/decomp/analysis-20260530.md` skip list).

**Replace the sentence with:** "Skip functions the orchestrator DB marks
at_limit (`query_functions status='at_limit'`; check `get_attempts` on each
shortlisted symbol) and those on the docs/decomp deep-dive AVOID lists."

### A7 (BLOCKING) — Lane B owned surface fails lint 9 as written: `dmidi` senders are not a nameable `src/band3/` surface

`dmidi`/`ikmidi` appear **nowhere in `src/` as source identifiers** — they are
asset-side object-name suffixes (the W31 STATUS `:54` names them as driver
OBJECTS: "`strum.dmidi`, `fret.ikmidi`, `right_hand.dmidi`,
CharDriverMidi/CharIKMidi"). The real code TUs are
`src/system/char/CharDriverMidi.cpp`, `CharIKMidi.cpp`, `CharIKSliderMidi.cpp`
(all verified present). The `src/band3/` sender TU is UNKNOWN today — the
grant "src/band3/ instrument-track/dmidi senders" names a directory + a
hypothesis, which is exactly the W31-A1 failure shape.

**Change Lane B owned surfaces to:** "`src/system/char/CharDriverMidi.cpp` /
`CharIKMidi.cpp` / `CharIKSliderMidi.cpp` (exclusive-write); the `src/band3/`
sender TU(s) are owned **only after** STEP-0 derives and checkpoints their
real names (lint 9 re-earn — no `src/band3/` write before that checkpoint);
BandCharacter.cpp read-only (see A9)."

### A8 (BLOCKING) — Lane B discriminator is non-exhaustive and unsplit

The branch set {(a) bound+fed, (b) bound-but-starved, (c) wrong-basis} has a
hole: **(a0) NOT BOUND AT ALL** — the dmidi/ikmidi driver objects never
created/bound natively (a plausible outcome given the senders are unlocated,
and the exact shape of the W31 arg-order find). As written, a lane hitting
(a0) has no branch and no fix-legality ruling. Also, lint 2: the family spans
distinct prop classes (drumstick tips / guitar neck / kit cones / magenta
stick-fan guitar) that may branch DIFFERENTLY — an aggregate verdict cannot
refute per-prop mechanisms.

**Add:** "(a0) drivers not bound/created natively → fix legal under the same
terms as (b) IFF the root cause is a decomp/routing bug (check the dispatch
path's ≥99% functions first — arg-order lesson); otherwise memo. The
discriminator verdict is rendered **per prop class** (sticks / neck / kit
cones / stick-fan guitar), pointer-keyed and matrix-relative per lints 1/2 —
one aggregate row is not acceptance."

### A9 (ADVISORY) — Lane B's "BandCharacter.cpp read-only (W31 Lane A surface)" cites an expired grant

W31 lane exclusivity ended with W31; no W32 lane owns BandCharacter.cpp.
Read-only-by-default is still RIGHT (D-sweep hygiene + churn risk on a
4000-line TU), but the rationale should be current and the door should exist:
if B's branch-(b)/(a0) fix site provably lands in BandCharacter.cpp,
coordinator arbitration MAY grant it mid-wave (it is on A3's exclusion list,
so Lane D cannot collide). Reword the parenthetical to "(read-only by
default; coordinator arbitration required for any write — no W32 lane owns
it)".

### A10 (ADVISORY) — Lane A anti-gaming clause (A3-class)

Legs (3)+(5) already block the crude "hide ALL highlights" game. Remaining
vector: a positional/screen-space suppression ("skip static quads over the
torso region on web") passes every written leg without fixing anything.
**Add:** "the fix must act at the divergence point named in leg (2) — a
suppression keyed to position/size/screen rather than to the named orphan
mechanism is NOT acceptance; after-fix evidence must include a replay of the
entry-hypothesis transition (`options` → `joined_default`) showing the
highlight lifecycle correct through it, both directions of focus travel."

### A11 (ADVISORY) — Lane C anti-gaming clauses

- **F4 is gameable as written** ("retail draws 5 slots") by always drawing 5
  filled slots. **Add:** captures at TWO matched states with different earned
  counts (e.g., early window 0 stars vs later window ≥3): filled == earned AND
  dim == 5−earned in BOTH, retail-paired.
- **F2:** the fix must act on the mechanism the material dump names
  (bind/blend/prelit), not a hardcoded pill tint; digits legible
  white-on-dark per the retail pair.

### A12 (ADVISORY) — two §5/§6 items silently dropped; record their dispositions

The kickoff carries F5/F6/SKEL/CROWD but is silent on: **F8** (settle-frame
recapture, §6.7 "pending") and **E6** (workaround-vs-fix-opt-out taxonomy,
§5.7 — now TWO default-ON fixes, `RB3_NO_HUB_HIGHLIGHT_FIX` +
`RB3_NO_BUTTON_GLYPH_FIX`, sit in class=workaround inflating the §W5.3
metric). Neither needs a lane; both need a line in §Close-out so they stay on
the ledger ("F8 carried, not chartered; E6 owed to the registry owner —
target: a fix-opt-out class or a metric exclusion").

### A13 (ADVISORY) — instantiate the ten lints for W32 lanes at adoption

The by-reference incorporation is usable (rules are self-contained), but the
W31 checkboxes are instantiated per-W31-lane; nobody has run them for W32.
This review discharges the tree-facing ones (lint 9 → A7; lint 10 → A1;
lint 2 → A8). At adoption, the coordinator should paste a short instantiated
block covering the rest — in particular **lint 4 for Lane C** (registry sweep
incl. `RB3_HUB_TEXT_CONTRAST`, the W4.2 text floor, ROWFIX,
SCOREBOARD_TOPRIGHT, W2.7 FilterSubdir, AND the two new default-ONs before
any "engine drops X" mechanism claim — W15 lesson) and **lint 1 for Lane B**
(bone claims matrix-relative + pointer-keyed).

### A14 (ADVISORY) — restate the FxSendNative hazard for the one engine-writing lane

The engine tree still carries the untouchable `M src/platform/
FxSendNative.cpp` (verified). The never-stage rule IS incorporated via the
W31 §Process Locks bullet, but Lane C is this wave's only engine-writing lane
and the hazard note lived in W31's acceptance section, not §Process. One
sentence in Lane C's owned-surfaces paragraph ("engine commits: never stage
`FxSendNative.cpp`") is cheap insurance.

---

## DISPATCH: NO-GO until A1–A8 are adopted (A9–A14 advisory, adopt-at-will).

The blocking set is one docs edit — no lane re-chartering: A1/A2 fix Lane A's
dead STEP-0 tooling and the render-hook fence; A3/A4/A5/A6 make Lane D's
sweep collision-safe, fresh, self-verifying-as-ruled, and pointed at a real
at-limit source; A7/A8 re-earn lint 9 and close the discriminator hole for
Lane B. Everything else in the kickoff verified true against the tree.

_Author: Wave-32 Fable pre-dispatch reviewer. Read-only on code; this doc is
the only write._
