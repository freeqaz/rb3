# WAVE 32 CLOSE-OUT REVIEW — adversarial acceptance (four lanes + F7 rider)

**Reviewer:** Fable close-out agent, 2026-07-12. **Inputs:** WAVE32_KICKOFF.md
(+ COORDINATOR ACCEPTANCE A1–A14, base SHA `30546499`), WAVE32_REVIEW.md,
WAVE32_COUNTERSIGN.md (`9eb50c3c`, pre-E1 raw-artifact re-derivation), lane
STATUS docs W32-{WEB-YELLOW, PROP-FAN, HUD-F2F4, ARG-ORDER, F7-CLIP} (each with
a coordinator close-out appendix), lane commits `41d52acb` (A, STEP-0 docs) /
`3ed6118a` + engine `c0bc00a` (B) / `b0f6e3b7` (C, docs-only) / `ad0130f4` (D)
/ `d07738cb` (rider, docs-only), close-out `69103c77` (rb3) + engine `2ea8e34`
(pin `24c4f95` → `2ea8e34`, census 418). Coordinator re-derivations on the
FINAL merged tree cited below are ground truth: web fix verified on BOTH debug
and release builds (floating quad GONE at `joined_default`; highlight bar
contained on the focused row and tracking RETURN → PLAY ON XBOX LIVE and
back), evidence re-homed to `W32-WEB-YELLOW/evidence/coordinator-fix/`;
post-flip drawlog-golden PASS (792 draws, 287 known-residuals within bound);
bounded non-HTTP boot rc=0 5/5.

**Verdicts: Lane A ACCEPT (STEP-0 exemplary; A2 STOP honored to the letter;
fix coordinator-executed and verified — and the root insight is bigger than
the charter: the ENTIRE render-hook policy family, B1–B13 including the W31 F3
glyph fix, had been absent on web since the W1 clear-frame era). Lane B ACCEPT
(the wave's cleanest discriminator→fix arc; default-ON flip earned). Lane C
ACCEPT (F2 real bug named + correctly withheld from landing; F4 premise
REFUTED — the W30 finding was itself an A11-trap instance). Lane D ACCEPT
(honest negative: sweep class EXHAUSTED 0/1185; one genuine behavioral find,
adjudicated KEPT). F7 rider ACCEPT (mechanism memo + W33 charter draft
delivered; correctly reused Wave-13 prior work instead of re-deriving it).**

---

## 1. Per-lane verdict vs charter acceptance

### Lane A — W32-WEB-YELLOW: ACCEPT (STEP-0 + model A2 STOP; fix landed at close-out)

The charter's blocking STEP-0 (name the quad, name the divergence point) was
delivered in full and with unusual precision: the quad is the hub
focused-menu highlight bar (`highlight_main.mesh`/`highlight_pattern.mesh`, a
skinned UI mesh) rendering at the world ORIGIN because the per-focus placement
policy is never applied on web; the divergence root is not a code bug at all
but a **build-list divergence** — `native/CMakeLists.txt` excludes
`rb3_render_hook.cpp` from the EMSCRIPTEN target ("not needed for
clear-frame", a stale W1-era decision), so `GetGameRenderHook()==nullptr` on
web and every `geomPolicy` stays default. The four-link divergence chain is
cited file:line at every link (CMakeLists `:455`/`:819`, hook registration
`:381/:384/:390`, engine `Rnd_Wgpu_RB3.cpp:3226-3229/:3257/:3350-3355`), and
confirmed empirically (the `.o` absent in `native/build-web`, present native).

The lane's **A2 STOP-and-checkpoint behavior is the process highlight of the
wave**: its named divergence point landed on mechanism (i), the render-hook
family Lane C held exclusive-write on — so it checkpointed
COORDINATOR-ACK-NEEDED, wrote a fix *proposal* (with an emcc-compilability
pre-check and an explicit SCOPE FLAG that the fix restores ALL policies, not
just the hub bar), and stopped. No concurrent write, no self-granted
exception. This is exactly what A2 was written to produce, and the first time
the fence has been exercised rather than merely declared.

Arbitration + fix (coordinator, close-out): Lane C finished with
`rb3_render_hook.cpp` untouched, so the collision was moot; the TU was added
to `RB3_WEB_NATIVE_GLUE` (build-level parity restoration — no new flag, the
three-tier rule is N/A for source-list membership) and the exclusion comment
rewritten as a warning. Acceptance legs, graded on the merged tree:

| leg | verdict |
|---|---|
| (1) quad named, mesh + draw evidence | PASS — engine-source naming + web repro pair (quad static across focus AND camera moves), A10 replay quoted |
| (2) web-only mechanism, divergence point file:line | PASS — four-link chain, each link cited; empirically confirmed via object-file presence/absence |
| (3) after-fix web pair | PASS at close-out — quad GONE at `joined_default`, verified on BOTH debug and release builds |
| (4) native control unchanged | PASS-BY-CONSTRUCTION + gates — the landed change touches only the WEB source list (native target's sources untouched); merged-tree drawlog-golden PASS + boot rc=0 5/5. See §2 for the judged residue |
| (5) B8 no-regression | PASS — highlight bar contained on the focused row, yellow, tracks focus both directions (RETURN → PLAY ON XBOX LIVE and back) |

A10 anti-gaming: satisfied by construction — the fix IS the named mechanism
(restore the policy source), the opposite of a positional suppression, and the
focus-travel replay was re-run post-fix.

### Lane B — W32-PROP-FAN: ACCEPT (clean discriminator→fix arc; 17th default-ON earned)

The textbook lane of the wave, start to finish: E7 debt discharged FIRST
(matched-songMs band-framing crop baselines + shard census: drums
drops_band=1107 ratio 5.92, guitar 843/5.15, vocals 0 — the before-eyes the
W31 review demanded); then the discriminator, rendered **per prop class**
(A8), pointer-keyed and matrix-relative (lint 1), with a decisive probe:
`CTOR 23 / ENTER 0 / POLL 17 / FEED 0` — the drivers are created and polled
every frame but **never Entered**, so they never AddSink onto their MIDI
parser and no per-note clip ever plays. Verdict branch (b) STARVED for all
four instrument prop classes (vocals the clean control), with the mechanism
named at the container level: `RndDir::SyncObjects` rebuilds `mPolls` AFTER
the one-time `Character::Enter`, so Enter's traversal misses drivers Poll
later picks up. Branch (c) SKEL explicitly REFUTED for the props, and the
residual shoe-mesh fans correctly left under the binding W31 SKEL_FAMILY_STOP
rather than annexed.

Fix disciplined to the owned surface (A7): lazy one-time Enter on first
native Poll, `#ifdef HX_NATIVE`, no struct member (Wii layout untouched),
shipped default-OFF with a PROPOSED flip — the lane did not self-grant the
tier. Evidence earned the flip: ON-vs-OFF same-build — ENTER 0→21, OnMidiParser
FEED 0→173, drum-hit clips 0→416, drummer shards 1107→2, guitarist 843→0,
stick-fans visually gone in the after-crops; W31 set_play NON-REGRESSED
(CHARDRV_PLAY 81/80, SETPLAY_SEND 26 both); Wii objdiff neutrality
countersign-verified (Poll/Enter/ctor 100.0 raw, pre-existing residuals
byte-identical). Coordinator flip at close-out: `RB3_NO_MIDIDRV_ENTER_FIX`
default-ON opt-out (engine `2ea8e34`), the **17th default-ON**. This also
closes the second half of the W31 floating-legs user report (prop-tip fans),
whose first half (body performance) W31 fixed — the two-wave arc is complete.

### Lane C — W32-HUD-F2F4: ACCEPT (F2 named + correctly deferred; F4 premise-refutation)

Two mechanisms, two checkpoints, zero code — exactly as chartered, and both
halves honest in opposite directions:

- **F2 (score pill): REAL BUG, mechanism named, fix withheld.** HEADMAT dump
  quoted byte-for-byte (three unlit layers, all textures BOUND — no missing
  bind), pixel proof quantitative (native pill body == venue color ⇒ the
  `sb_refract` dark backing contributes ~0 coverage; retail's is opaque and
  venue-independent), hypothesis precise (Wii refraction material's opacity
  does not derive from diffuse.a; native's generic unlit path multiplies by
  it). The lane then did the RIGHT thing twice: no render-hook pill tint
  (would satisfy the screenshots and violate A11), and no engine write —
  engineAckNeeded checkpoint. Coordinator adjudication: ack NOT granted; a
  RB3MaterialBinder blend-semantics change has blast radius beyond one pill
  and gets its own wave → **W33-F2-PILL** with a mandatory cross-screen
  material sweep. Deferring a fix you could have landed is acceptance-grade
  discipline, not a gap.
- **F4 (star row): premise REFUTED — CLOSED NOT-A-BUG.** The W30 finding
  ("retail always renders 5 slots") compared native-at-0-stars against
  retail-at-4-stars — mismatched states, i.e. **the exact trap A11 was
  written against, discovered retroactively in the campaign's own W30
  evidence**. The refutation is airtight in both directions: source
  (BandStarDisplay progressive reveal, ResetStars/SetupStars/SyncObjects
  100.0% matched — retail runs this code) and retail's own screenshots
  (2 stars = 3 discs; 4 stars = 5 discs), plus native captures at THREE earned
  counts (0/1/3 → 1/2/4 discs) — A11's two-state requirement over-satisfied
  with five states. The A11 formula itself ("dim == 5−earned") was written
  under the false premise; the lane refuted its own acceptance formula with
  evidence, which is the correct precedence.
- Lint-4 registry sweep done BEFORE any mechanism claim (8 flags enumerated,
  none touching the pill or star meshes); A14 honored (no engine staging;
  FxSendNative untouched); `rb3_render_hook.cpp` exclusive grant held but
  never used — countersign-verified git-clean.

### Lane D — W32-ARG-ORDER: ACCEPT (honest negative; the audit-class thesis re-scoped)

The W31 lesson said arg-order bugs are "a cheap, high-yield sweep class." Lane
D industrialized the sweep and **refuted the yield claim honestly**: across
1185 in-scope fuzzy ≥99 functions (A4 fresh report regen quoted; A3 claim
protocol + seed exclusions applied; A6 at-limit sources consulted), the
clean-raw-100 static arg-order class is **EXHAUSTED — 0 hits**. The classifier
progression (605 naive → 174 → 40 → 19 → 0 strict; argscan7 cross-opcode 52,
all noise on inspection) is itself a keeper: it names WHY the signature is
almost always noise (callee-saved renames, scheduling, commutative operands,
FPR cascades, string-pool offsets), and the detector-validity check against
the fixed SyncProperty (whose two args were set by DIFFERENT opcodes — a
same-opcode detector would have missed the one true historical positive) shows
the sweep was built to find the real class, not a strawman.

The one genuine find came the same way SyncProperty did — **behaviorally**
(convention anomaly: the lone `SetFrame(1.0f, 0.0f)` in a TU where every
other call passes blend=1.0f): `VocalTrackDir::SetRange` drove the
pitch-window material-config anim with blend=0.0 (silently no effect) on the
from-chromatic tonic transition. Retail-byte-verified at the call site
([140-147] match exactly post-fix), but the function stays at raw 99.28% due
to a pre-existing orthogonal FPR cascade — the lane FLAGGED the strict-A5
tension transparently instead of hiding it or reverting a correct fix.
Coordinator adjudication: **KEPT** (the A5 gate exists to prevent fake
matches; call-site bytes are retail-exact, the residual is permuter-class and
pre-existing, the port north-star is faithful behavior). Countersign
independently reproduced the 99.28 and the A5 unit neutrality.

**What this means for the audit-class thesis:** the W31 arg-order lesson
survives, but demoted from a static signature sweep to a **behavioral
investigation heuristic** — when a subsystem is silently dead, check the
dispatch path's ≥99% functions for call-site value swaps (and convention
anomalies within the TU) FIRST. As a blind sweep over the residual set it has
near-zero hit rate; the residual population at ≥99% is regalloc/scheduling
noise plus the already-documented permuter classes. Coordinator ruling
adopted: **the sweep class is not to be renewed as a sweep.** The ranked
backlog (FPR-cascade cluster → permuter; non-commutative arithmetic;
stack-temp orderings) is routed to the right tools, not carried as Lane-D
debt.

### Rider — W32-F7-CLIP: ACCEPT (diagnosis-only; both STEP-0 questions answered)

Q1 answered with a genuinely non-obvious verdict: **nothing clips the
character** — all world.cam character draws are inside the screen; the
"clip line" is the boundary between the list column (50% dimmer + opaque
row quads) and the sidebar column (50% dimmer ONLY). Q2: retail has a
near-opaque sidebar panel (photographic, two independent sources); ours is a
**missing panel draw / asset-completeness gap** — every candidate backing mesh
census'd at draws=0 because the `song_select_details` drill-in sub-panel is
never shown in quick-view. The rider correctly REUSED W4.3-C2/C2a (Wave 13)
instead of re-deriving, re-confirmed it with fresh W32 drawlog/uidump
evidence, added the full-screen-dimmer half neither prior lane had, and
documented its own dead ends (DTA camera probe, filtered uidumps, the
too-coarse ROI query). Deliverable complete: mechanism memo + candidate fix
surface + a W33 charter draft (native backing quad, retail-proven
faithful-restoration tier) with an explicit anti-blast-radius non-goal (do
NOT raise `header_list_bg`'s global alpha). Sonnet-sized lane, Opus-grade
epistemics.

---

## 2. Countersign gap dispositions

| gap (countersign, pre-E1) | disposition at close-out |
|---|---|
| **§1 Lane A: native control never captured; quad name source-comment-sourced; legs (3)(4)(5) deferred per A2** | **DISCHARGED IN SUBSTANCE, judged here.** The deferral was legitimate (A2 STOP) and the coordinator's post-fix record covers the legs' substance: (3)+(5) verified directly on debug AND release; (4) holds *by construction* — the landed diff touches only the web source list, so the native binary is bit-unaffected — plus merged-tree drawlog-golden PASS and boot 5/5. The fix's outcome (quad gone + bar tracking focus the moment the placement policy went live) is also stronger corroboration of the named mechanism than the planned static cross-name would have been. **Residue (recorded, §5.1):** the mandated `/api/uidump` enumeration of the native hub highlight/overshell mesh set was never captured, even post-fix — the quad's identity rests on the engine source comment plus behavioral confirmation, not a runtime native dump. Cheap to capture; do it when the native hub control is next booted. |
| **§2 Lane B: discriminator event counts /tmp-only (W31-Lane-C locality class)** | **RESOLVED BY ACCEPTED-CENSUS RULING.** Coordinator chose the countersign's second offered path: the committed shard-census + clip-census quotes (verdict.json, independently re-derived) ARE the flip's earn; the /tmp event-count logs are the mechanism trace, quoted in tracked STATUS per E4/lint 7. Defensible — the census is the validated instrument and the visual after-crops are committed — but note this is the second consecutive wave resolving a log-locality flag by acceptance rather than re-homing. If it recurs, make probe-log re-homing part of the flip checklist, not a countersign negotiation (§4.4). |
| Process scan (pkill/killall/bare-ninja) | Not re-run this wave by countersign; no violation surfaced in any lane STATUS or coordinator appendix. |

No other gaps: the countersign found **no fabrication and no
build-dir-only headline number** — every landed edit committed, Lane C
verifiably code-free, Lane D's sub-100 honestly disclosed and independently
reproduced.

---

## 3. Evidence-honesty audit

This wave continues the W31 trend and improves on it — every lane's headline
is either an honest negative, a self-refutation, or a countersign-reproduced
positive:

- **Lane A** stopped at a fence it could have argued around ("CMakeLists is
  not the .cpp") and disclosed its own failed native control attempt (splash
  stall) rather than quietly dropping the leg.
- **Lane B** shipped default-OFF and *asked* for the flip with evidence,
  rather than self-granting; left the shoe-fan residual to the closed family
  it belongs to instead of inflating the win.
- **Lane C** refuted a standing W30 finding (F4) using the anti-gaming rule
  chartered against itself, and withheld a landable engine fix on blast-radius
  grounds.
- **Lane D** reported 0/1185 as the headline instead of dressing up the one
  behavioral find as sweep yield, and flagged its own A5 tension before the
  countersign could.
- **Countersign** re-derived every load-bearing number from committed
  artifacts and independently re-ran batch_objdiff for both B's neutrality
  set and D's claims.

Errata, one sequence (continuing W31's E-numbering convention per wave):

- **E1 (Lane A, minor):** the native uidump control was skipped for a
  defensible reason but the STATUS's "NOT required" framing overstates — the
  charter marked it mandatory; the correct framing is DEFERRED-WITH-CAUSE.
  The coordinator appendix implicitly accepts the reframe; recorded here so
  the letter/spirit distinction stays visible (residue in §5.1).
- **E2 (Lane B, soft):** the decisive discriminator counts live in /tmp
  (§2 ruling accepted); the family's probe (`RB3_MIDIDRV_PROBE`) is retained
  as acceptance instrument, so the counts are cheaply reproducible — which is
  the real mitigation.
- **E3 (Lane C, none):** no erratum. Both halves clean.
- **E4 (Lane D, definitional):** the STATUS's "0 clean landings" and "1
  landed fix" coexist — the fix is real but is NOT a member of the swept
  class (found by convention anomaly, not by the classifier). Future sweep
  lanes state the found-by channel per landing so class-yield numbers stay
  auditable. (This wave's STATUS does state it; the erratum is to make it a
  standing requirement.)
- **E5 (close-out, taxonomy — A12 disposition):** `RB3_NO_MIDIDRV_ENTER_FIX`
  is registered class=workaround default=on, joining `RB3_NO_HUB_HIGHLIGHT_FIX`
  and `RB3_NO_BUTTON_GLYPH_FIX`: the E6 (W31) taxonomy debt now has **THREE
  default-ON faithful-fix opt-outs sitting in class=workaround**, inflating
  the §W5.3 "default-ON workarounds → 0" metric with entries that are escape
  hatches for *fixes*, not workarounds for un-derived behavior. This is no
  longer a footnote — the metric is now measurably distorted. Owed to the
  registry owner as a W33 item: a `fix-opt-out` class (or metric exclusion),
  one JSON edit + census regen (§5.4).

---

## 4. Process lessons

1. **The stale-exclusion class: a build decision is a silent fork.** The
   yellow square was never a rendering bug — it was a W1-era source-list
   exclusion ("not needed for clear-frame") that stayed correct for one week
   and wrong for ~30 waves, during which the web build silently lacked the
   ENTIRE render-hook policy family (B1–B13, halo, glyph routing, highway
   shading — including fixes like W31's F3 that were "shipped" to a web build
   that couldn't run them). No gate caught it because every gate ran native.
   Two rules follow: (a) **close-out should run a one-time web/native
   source-list parity audit** — diff the native vs EMSCRIPTEN source lists in
   `native/CMakeLists.txt` and require every intentional exclusion to carry a
   current justification comment (the stale ones are exactly the bugs) — YES,
   adopt as a W33 rider; it is one afternoon and the class has now bitten
   once; (b) when a default-ON fix lands in a hook/policy layer, the earn
   evidence should state WHICH targets actually compile that layer (the F3
   flip's ON-vs-OFF was native-only; nobody noticed web wasn't in the
   population).
2. **The A2 fence produced its intended behavior on first contact.** Lane A
   hit the exact collision the amendment predicted (mechanism (i) → Lane C's
   exclusive TU) and the protocol held: checkpoint, proposal, wait. The cost
   was one deferred acceptance leg-set, discharged at close-out; the avoided
   cost was the W31-E5 collision class on the campaign's most-edited TU.
   Fences with named arbitration outcomes work; keep writing them.
3. **Premise-refutation needs matched states — including OUR OWN premises.**
   F4's "bug" was manufactured in W30 by comparing mismatched star counts;
   the A11 anti-gaming clause, written to stop lanes from gaming acceptance,
   turned out to describe the error in the ORIGINAL finding. Standing rule
   for visual findings: a "native lacks X" claim requires the retail and
   native captures to be state-matched (same earned count, same songMs, same
   focus) at FINDING time, not just at fix-acceptance time.
4. **Sweep classes get ONE pilot before industrialization — and the pilot's
   negative is a result.** W31 priced the arg-order sweep as "cheap,
   high-yield"; the W32 pilot measured yield = 0/1185 and localized the value
   in the behavioral heuristic instead. The campaign paid one lane to learn
   this; the ruling ("do not renew as a sweep") makes sure it doesn't pay
   twice. Corollary: the classifier progression (naive→strict→cross-opcode)
   is reusable tooling for the NEXT hypothesis-driven audit, and the ranked
   backlog routes to the permuter, not to another sweep.
5. **Log-locality flags are becoming a pattern (second consecutive wave).**
   W31 Lane C, W32 Lane B — both resolved by post-hoc acceptance. The fix is
   procedural, not moral: add "probe ON/OFF log pair re-homed under
   evidence/" to the default-ON flip checklist so the coordinator never has
   to adjudicate it again.

---

## 5. Not properly closed (honest list)

1. **Lane A leg-4 letter: the native `/api/uidump` hub-mesh control was never
   captured, even post-fix** (§2, E1). The mechanism is behaviorally proven,
   but the campaign record has no runtime native enumeration of the
   highlight/overshell mesh set to cross-name against. One boot + one curl
   next time a native hub control runs.
2. **The web build now runs the FULL policy family, validated only at the
   hub.** The coordinator verified `joined_default` on debug + release; but
   song_select and gameplay on web were never re-checked with B1–B13 live —
   **release users now get F3 glyphs (and 11 other policies) on web untested
   beyond the hub**. Also the debug-build song_select was never re-checked.
   This is the top W33 rider: a cheap web full-policy validation pass
   (song_select + gameplay screenshots, both builds) — it is the ON-leg of
   the earn that the family's native-only evidence never covered (§4.1b).
3. **F2 chartered, not fixed** — W33-F2-PILL (engine blend-semantics change,
   mandatory cross-screen sweep). Correctly deferred; still user-visible.
4. **E6 taxonomy debt, now ×3** (E5 above): fix-opt-out class or metric
   exclusion for `RB3_NO_HUB_HIGHLIGHT_FIX` / `RB3_NO_BUTTON_GLYPH_FIX` /
   `RB3_NO_MIDIDRV_ENTER_FIX` — registry owner, one edit + regen.
5. **F8 (settle-frame recapture): carried, not chartered** — A12 disposition
   recorded; third wave on the ledger without an owner. Either charter it as
   a rider or close it explicitly as obsolete.
6. **Probe soak-retirement clock:** `RB3_MIDIDRV_PROBE` and
   `RB3_SETPLAY_PROBE` are both KEEP-as-acceptance-instrument; per the W30
   probe-retirement discipline they need a named retirement condition
   (suggested: retire both once their families survive one further wave with
   no regression — i.e., a W33/W34 close-out decision, not open-ended).
7. **Lane B mechanism-trace logs remain /tmp-only** (accepted-census ruling,
   §2/E2) — reproducible via the retained probe, but not on disk.
8. **F5 patch-shard repro** stays recorded-not-chartered (needs fresh
   hypothesis + own oracle); **F6 hub grade** stays BLOCKED on UIGRADE
   reconciliation. Unchanged, carried.
9. **Lane D's VocalTrackDir fix has no runtime visual/behavioral capture**
   (the pitch-window material anim on the from-chromatic transition). The
   retail-byte proof makes it faithful by construction; a vocal-gameplay
   capture would close the loop from the behavioral side. Low priority,
   noted for completeness.

None of these gate acceptance. Items 1 and 2 are minutes-to-hours; item 2
should run FIRST in W33 pre-work since release users are already on the new
web build.

---

## 6. Wave-33 menu (recommendation — ordering rationale)

The two shovel-ready user-visible lanes lead; the web validation rider is
mandatory pre-work given §5.2; the taxonomy and ledger items are
coordinator-cheap.

1. **W33-F2-PILL (chartered this wave):** the score-pill dark backing —
   engine-side RB3MaterialBinder coverage handling (refraction-material
   opacity not derived from diffuse.a), the ONLY chartered engine
   blend-semantics change. Mandatory: cross-screen material sweep (enumerate
   every mesh the new coverage path touches before flipping anything),
   ON-vs-OFF retail-paired crops, A11 (mechanism-named fix, no pill tint).
2. **W33-F7-SIDEBAR-BACKING (rider's drafted charter):** native-only opaque
   backing quad behind the song-select difficulty grid — retail-proven
   faithful-restoration tier (default-ON + opt-out, earned by ON-vs-OFF),
   append-only render-hook predicate, real-song-row focus (W31 heading trap),
   drawlog-golden + list-column-unchanged acceptance. Non-goal: do NOT touch
   `header_list_bg` global alpha.
3. **Web full-policy validation pass (cheap Sonnet rider, run FIRST):**
   song_select + gameplay on web (debug AND release) now that the render-hook
   family is live — screenshot sweep vs native, confirm F3 glyphs + B-family
   policies render and nothing regresses. Closes §5.2.
4. **Web/native source-list parity audit (coordinator-cheap, one-time):**
   diff native vs EMSCRIPTEN source lists; justify or kill every exclusion
   (§4.1 rider). The stale-exclusion class has bitten once; this closes the
   remaining exposure.
5. **E6 taxonomy fix (registry owner):** add a `fix-opt-out` class (or §W5.3
   metric exclusion) and re-classify the three default-ON fix opt-outs;
   single census regen.
6. **F8 settle-frame recapture:** charter as a micro-rider or close as
   obsolete — third wave carried without an owner (§5.5).
7. **Probe soak-retirement:** set the retirement condition for
   `RB3_MIDIDRV_PROBE` + `RB3_SETPLAY_PROBE` (§5.6); Lane A's leg-4 native
   uidump capture (§5.1) can ride whichever lane boots a native hub control.
8. **Carried/blocked:** F5 (needs fresh hypothesis + oracle), F6 (UIGRADE),
   SKEL/CROWD families CLOSED (STOPs binding); arg-order NOT renewable as a
   sweep (Lane D ruling) — behavioral-heuristic use only.

_Author: Wave-32 Fable close-out reviewer. Read-only on code; this doc + the
README results/menu section are the only writes._

## Coordinator discharge addendum (post-review, 2026-07-12)

§5 item (2) — web full-policy validation beyond the hub — DISCHARGED for
song_select: release-build smoke (boot → Music Library, real rows focused)
shows the policy family behaving correctly on web: focus highlight bar
contained + row-tracking (B8), footer hint pills render F3 glyph ARTWORK
(yellow pills, not white blobs — first time on web), overshell MENU chip
glyph correct, no orphan quads. Evidence:
`execution/W32-WEB-YELLOW/evidence/coordinator-fix/web_song_select_policies.png`.
Remaining exposure: web GAMEPLAY under the policy family (tail colours,
highway shading) — folded into the W33 web-validation rider with the
source-list parity audit. §5 items (1) uidump cross-name, (3)-(9): carried as
written into the Wave-33 menu.
