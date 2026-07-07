# Wave 12 — Pre-dispatch Review (Fable adversarial pass)

**Reviewer:** Fable subagent. **Input:** `WAVE12_KICKOFF.md` (draft), README Waves 1–11 (Wave 10/11
tables), `WAVE11_KICKOFF.md` acceptance A1–A9, `WAVE7_REVIEW.md` (precedent), STATUS docs
(`BOOTRNG/`, `W2.8f/` + `S2_WAVE12_INSTRUMENT_DESIGN.md`, `W2.8e/`, `W0.3d/`, `W0.3c/`, `W4.1/`,
`W4.2/` incl. evidence PNGs), current captures `/tmp/wave12-current-state/` vs
`images/retail-screenshots/`, plus direct source verification (rb3 + engine @ pin `146fd19`).

## VERDICT: **dispatch-with-amendments** (Lane A's S1/S2 mechanism framing must be rewritten before
dispatch — two of its factual premises are false on the record; Lanes B and C dispatch with gate and
fence corrections.)

The three-lane structure is right, the flag-first discipline is right, and the visible-defect lane
(C) is well-chosen and confirmed against retail refs. But the kickoff contains exactly the class of
error this gate exists to catch — twice in Lane A alone: (1) the "staged since Wave 4" async-loader
patch **does not exist** — the Wave-7 review already established this on the record and the claim
has regressed back in; (2) the causal chain "completion order → object-list insertion order → gRand
CONSUMPTION order" is unsupported and partly mechanically impossible against today's source; (3) the
R-A precedent claim ("SortDraws tie-break shipped default-ON") is **false** — it is fixed-clock-gated;
(4) Lane B's gate list omits the one instrument its own design doc names as the fix gate, leaving a
wext-only gate that a vertex clamp could game; (5) Lane C1's "regression-vs-path" fork is already
resolved by evidence sitting in `W4.2/`'s own Wave-7 PNG — the regression arm should be skipped; (6)
C3's stated fix location (RB3Quad/font path) is the wrong file, and the kickoff's own fence bars Lane
C from the likeliest actual fix TU.

---

## Amendments

### A1 (CRITICAL, Lane A) — the "staged since Wave 4" patch DOES NOT EXIST; this is a regression of a Wave-7 review finding

The kickoff header ("W0.3d part-b (staged since Wave 4)") and the BOOTRNG backlog it inherits
(`BOOTRNG/STATUS.md:246` "Land the staged async-loader/worker completion-order determinism patch")
repeat a claim the Wave-7 review verified false (`WAVE7_REVIEW.md:40–58`): **the only staged W0.3d
part-(b) artifact was `W0.3d-fix.patch` — the `SortDraws` material-NAME tie-break in
`src/system/rndobj/Utl.cpp` — and it LANDED in Wave 5** (rb3 `76f51077`, live at `Utl.cpp:192–199`;
`W0.3d/STATUS.md:164–177` describes exactly and only this patch). There is **no** loader/insertion/
completion-order patch, staged or otherwise; W0.3d part-b beyond SortDraws is *diagnosis only*
(ThreadCall worker → per-thread malloc arena → address-dependent pointer compare,
`W0.3d/STATUS.md:105–135`). The kickoff's S1 "re-derive" wording partially concedes this, but the
lane brief must say plainly: **Lane A is NEW design work with no artifact to apply**, and the
Wave-11/BOOTRNG record should be corrected so this phantom does not resurrect a third time.
Consequence: Lane A's effort estimate and S2's "land the determinism seam" framing must assume a
from-scratch mechanism study (S1) that may legitimately conclude with a *staged* design again.

### A2 (CRITICAL, Lane A) — the mechanism model "insertion order → gRand consumption order" is unsupported; the evidence points at completion-FRAME TIMING (a count axis, not an order axis)

Verified against source and against BOOTRNG's own numbers:

- **Per-frame consumption order does not flow from insertion order.** `RndDir::SyncObjects` name-sorts
  the poll list (`std::sort(mPolls…, SortPolls)`, `rndobj/Dir.cpp:76`) and `SortPolls` tie-breaks by
  `strcmp(Name())` (`rndobj/Utl.cpp:207–214`) — a total order independent of hash/insertion order.
  Draws likewise (`SortDraws` + the landed Wave-5 name tie-break, `Utl.cpp:192–199`). The one genuine
  order channel that DOES leak hash-layout order is the **unsorted `mAnims`** list
  (`rndobj/Dir.cpp:50–63`, `ObjDirItr` walks `KeylessHash::mEntries` linearly,
  `utl/KeylessHash.h:77–122`; layout depends on names + resize history) — S1 must check whether any
  `mAnims`-ordered consumer draws gRand, but this is a *candidate*, not the named mechanism.
- **Completion callbacks draw no gRand.** `ObjectDir::PostLoad` (`obj/Dir.cpp:390–483`) and the
  `DirLoader` completion chain (`obj/DirLoader.cpp:659–772`) contain no `RandomInt/RandomFloat`;
  `Rand.cpp:80–97` asserts `MainThread()` so the worker cannot draw. So "gRand draws inside async
  completion callbacks" is (on current evidence) not the divergence source either — but S1 should
  confirm transitively (the sweep above was one level deep).
- **BOOTRNG's own data refutes an order-only model.** The divergence is a **~11,231-draw COUNT
  spread** at the pinned capture, 12 distinct positions in 12 boots, while the lighting EVENT count
  is invariant (24/24) and ~7k of the spread accrues *between* deterministic events
  (`BOOTRNG/STATUS.md:30–39,171`). The kickoff imports W0.3c's "single invariant multiset,
  order-only" finding — but that finding was about DRAW SUBMISSION, and a pure permutation of an
  invariant consumer multiset cannot produce a stream-POSITION spread at a pinned songMs. The prime
  suspect consistent with both the source and the data is **completion-FRAME timing**: real-I/O /
  ThreadCall-worker completions (`obj/DataFile.cpp:786` → engine `ThreadCall_Native.cpp`, the exact
  root W0.3d.S2 named) land on different *sim frames* per boot, so per-frame consumers
  (Crowd/CharClipDriver/Part/Wind/AnimFilter…) start or advance at different frame offsets and the
  count diverges — with value-feedback (a different clip pick changes future draw counts) amplifying
  it downstream.

**Concrete S1 rewrite:** instrument attribution FIRST, using the already-landed
`RB3GRandDrawCount()` probe (BOOTRNG, rb3 `math/Rand.{h,cpp}`): log per-frame draw count + per-dir
load-completion frame across N≥6 fixed-clock boots; find the FIRST divergent frame between two boots
and name what happened on it (a load completing vs an order swap). Pre-register both hypotheses:
H-TIMING (completion-frame variance — fix = deterministic completion frames under fixed clock, e.g.
serialize/quantize ThreadCall completion, the W0.3d "Attribution experiment A" precedent,
`W0.3d/STATUS.md:152–156`) vs H-ORDER (mAnims/registration order — fix = order pinning). A cheap
third candidate S1 should price: **re-seed `gRand` at a canonical deterministic event** (song start /
shot start) under `RB3_FIXED_CLOCK` — the `0x5EED` precedent applied at a mid-boot anchor — which
kills all pre-song divergence without touching the loader at all (residual: consumer STATE chosen
pre-anchor from divergent draws persists; S1's trace will show whether that residual matters).
Pinning object-list INSERTION order alone is **not** a justified seam on today's evidence and the
brief must not pre-commit to it.

### A3 (HIGH, Lane A) — the R-A precedent claim is false: SortDraws is NOT default-ON; the seam must be fixed-clock-scoped + new-flagged + coordinator-sequenced

Kickoff R-A: "the SortDraws tie-break precedent shipped default-ON with opt-out." **Verified false:**
the tie-break is gated `RB3FixedClockActive() && !RB3DrawSortDeterministicOff()`
(`rndobj/Utl.cpp:192`; `native/src/rb3_replay.h:76–86`) — inert on every default/user boot, exactly
as `WAVE7_REVIEW.md:47–49` recorded. Independently, default-ON is the *wrong* shape for a loader
seam: deterministic/serialized completion on the shipping path would regress the entire shipped
incremental-load stack (async seam, prefetch, streaming). Required shape: **active only under
`RB3_FIXED_CLOCK`, plus its own registered flag** (e.g. `RB3_LOAD_DETERMINISM`, opt-in during the
wave) so (i) fail-red A/B is demonstrable post-landing (the exact gap W0.3d.S2 flagged for SortDraws,
`W0.3d/STATUS.md:193–198`), and (ii) the wave's OTHER lanes' `drawlog-792 --fixed-clock` gates don't
change behavior mid-wave under them. Coordinator flips opt-in → fixed-clock-default at wave end with
a re-golden provision (a deterministic-completion seam can legitimately change which objects exist at
the golden's capture frame; the REGRESSION gate must therefore read "flag-OFF 792 byte-identical;
flag-ON 792-or-coordinator-re-golden", not "792 unchanged" unqualified).

### A4 (MEDIUM, Lane A) — gate hygiene: same-tol SECONDARY, A9 escape clause, adversarial-entropy fail-red

- The SECONDARY envelope (0.067–0.362) was measured at `--tol` ±2000 ms; S3 tightens to 100–150 ms.
  Comparing a post-fix spread at ±150 ms against a ±2000 ms envelope is not a like-for-like gate —
  pre-register: both arms measured at the S3-tightened tol, threshold vs a fresh same-tol OFF-arm
  baseline.
- Re-state A9's escape clause verbatim (the kickoff drops it): PRIMARY-pass/SECONDARY-fail = file the
  FX/swept-light-phase finding (`BOOTRNG/STATUS.md:229–242`), not a gate fudge and not a reason to
  keep tuning the seam.
- Fail-red proof (R-A's question): quiet-machine 10/10 is the W0.3c quiescent trap
  (`W0.3d/STATUS.md:119–125` — the flake only manifests under scheduler pressure). Required: seam-ON
  holds 10/10 identical `gdraw_capture` **under induced contention + env-gated worker-latency jitter**
  (randomized sleep in the ThreadCall worker, test-only), seam-OFF under the same jitter reproduces
  the spread. That A/B is also the discriminator between a correct timing fix and a cosmetic
  order-only fix: an insertion-order pin passes an order check but fails the position check if timing
  is the driver — which is exactly why the PRIMARY gate (stream-position identity) is well-formed and
  must stay primary.

### A5 (HIGH, Lane B) — the S2 gate list omits Instrument B, the fix gate Lane B's own design doc mandates; a wext-only gate is gameable by clamping

`W2.8f/S2_WAVE12_INSTRUMENT_DESIGN.md` names the per-vertex shell invariant (**Instrument B**:
`‖s(v) − ŝ(v)‖`, `ŝ` = authored shell transported by only coherent bone motion via
freshness-validated rests, with the **A7 co-variation bar** — must rise/fall with wext) as "the
ACTUAL Wave-12 fix gate", precisely because Tier-2 joint-attachment is structurally blind to the
shell symptom (0–0.33u vs the 106u symptom). The kickoff's S2 gates list joint-attachment GREEN +
`wext ≤60u` + fixtures — **Instrument B is absent.** A wext-only symptom gate can be passed by a
cosmetic fix (any vertex clamp/soft-limit à la the existing SKIN_CLAMP backstop family shrinks
worldExt while still varying frame-to-frame and tracking pose — it would satisfy every listed
clause without touching the composition). Amend S1 to BUILD Instrument B (and run the confirmatory
Instrument A readback only as designed — predicted GREEN, logged either way), and add to S2's gates:
Instrument B co-varies with wext pre-fix (A7), reads ~0 post-fix, **and** the draw-log guard-DROP
census is unchanged (no fix-by-hiding).

### A6 (MEDIUM, Lane B) — the S1 instrument must DISTINGUISH authored-SPACE error from WEIGHT/INDEX error; today's evidence does not exclude the latter

The kickoff's S1 asks only "what space are the verts authored in vs what space the offset maps
from". But every exonerating datum — palette coherent (Tier-2 ≤0.33u), joints attached, pure-CPU
reproducible, R·sin(θ) geometry, zero-at-rest — is **equally consistent with per-vertex bone
INDEX/weight assignment errors** (a vert bound to the wrong bone follows that bone rigidly: fine at
bind, R·sin(θ)-swept under pose, invisible to every palette-internal metric, and shared by the CPU
mirror since it consumes the same decoded `boneWeights`). B.S2's "weight normalization ruled out"
(clean-control comparison) rules out a *global* normalization bug, **not** per-vertex assignment or
V24 decode of indices/weights — which both CPU sides share, so "GPU exonerated" does not exonerate
the shared DECODE. Instrument B is itself the discriminator — pre-register both branches: Instrument
B RED co-varying with wext → offset/space composition (the named axis, supported by Tier-1's uniform
87.3° = A.S1's magnet-vs-own angle); Instrument B **GREEN while wext stays RED** → the axis moves to
weights/indices/decode. Add the cheap per-vertex check either way: do all sampled verts sharing a
dominant bone move by ONE rigid rotation (space error) or scatter independently (per-vertex decode
error)? On R-B's oracle question: the faithful composition reference already exists in-repo — the
W0.1 skin golden (`tests/test_skin_golden.cpp`, `RefSkinVertex ≡ RndMesh::SkinVertex`, includes 6
hand/finger verts); extend it with the real `hands_naked` verts + real uploaded palette rather than
inventing a new oracle; DC3 source is secondary corroboration, not the ground truth.

*(Verified, no amendment: the 5-class STOP-TRIPWIRE list matches README history exactly — static
rebake (W2.2/W7 `RB3_HANDS_BIND_FIX`), rigid anchor (W7 `RB3_HANDS_POSEAWARE`), conjugation (W8
W2.8c), world-space rest (W9 `RB3_APPENDAGE_REST_ROT`), asset rebake (W10
`RB3_APPENDAGE_ASSET_REBAKE`). Also note the dualskin/probe block has MOVED: now ~`:4453–4735` of a
5,775-line `Rnd_Wgpu_RB3.cpp` (hands-attach probe follows at `:4736+`, wext at `:4360–4394`) —
Lane B's PLAN.md must re-declare current ranges, not inherit Wave-11's `4389–4671`.)*

### A7 (HIGH, Lane C1) — the regression-vs-path fork is ALREADY RESOLVED by evidence on file: skip the regression arm

Three independent checks close it:
1. `git log a94762f..146fd19 -- src/platform/RB3MaterialBinder.cpp` (engine) is **empty** — the
   relaxed floor (`RB3MaterialBinder.cpp:182–213`: pass-through unless all channels <0.06, lift to
   0.25) is byte-unchanged since the Wave-7 flip. (The kickoff's cited `:188–205` is ~6 lines stale
   but the code is the code.)
2. **Wave 7's own flag-ON evidence already shows the defect:** `W4.2/cs2_hub_off_vs_on.png` right
   panel (flag-ON) renders focused "PLAY NOW" **pale-yellow-on-gold** — identical to today's
   `/tmp/wave12-current-state/main_hub_sel*.png` and wrong vs retail
   (`images/retail-screenshots/yt_mhKNp9uAT48_menu_hub.png`: dark-on-gold). Nothing regressed; the
   Wave-7 verification protocol looked at QUICKPLAY (a different label route) and the hub top-level
   items were never dark post-flip.
3. The Wave-8 chroma-preserve is genuinely venue-scoped: the shader path requires `venueGrade > 0.5`
   (`rb3_postproc.wgsl.inc:230`), which is set only by the mid-frame venue flush
   (`RB3PostProc.cpp:55`); the end-of-frame composite menus take passes `venueGrade=false`
   (`Rnd_Wgpu_RB3.cpp:1995`, default param `Rnd_Wgpu_RB3.h:249`). Not a U1 mover.

So C1 starts at **path tracing**, day one: which material/color route do hub top-level items (and the
song-select highlight row, part-select `GUITAR`) take, and where does retail's DARK focused color
come from — leading candidates: the UILabel focus-state color is never applied natively (game-side
`src/system/ui` label/color-state path), or these labels ride a font-material variant that bypasses
the binder's UI-text branch entirely. Keep the Wave-7-rescued-labels no-regression clause (ticker /
FRIEND RANKINGS / CHOOSE INSTRUMENT). This also answers R-C's "cheaper first probe": it was sitting
in `W4.2/`'s PNG.

### A8 (MEDIUM, Lane C2c) — devil ratings: start at `BandSongMgr::RankTier`, and probe two concrete failure modes before any widget work

The tier mapping is `BandSongMgr::RankTier` (`src/band3/meta_band/BandSongMgr.cpp:380–390`) +
`RankTierToken` (`:399–401`, reads `SystemConfig(song_groupings, rank)`), fed from
`BandSongMetadata::mRanks` (per-instrument floats, `BandSongMetadata.h:76`). Two native-plausible
failure modes to log FIRST: (i) the `std::find(mSongRankings…)` misses (symbol mismatch /
per-instrument ranking never registered natively) → `r->mTierRanges` derefs `end()` → garbage tier;
(ii) `mTierRanges` empty (dta config not loaded) → the loop returns `i−1 = −1` → the widget indexes a
sentinel/devil glyph. One log line at `RankTier` (song, instrument, rank, returned tier) on the
song-select screen decides data-vs-widget in a single boot. The devil-glyph-as-red-scribble is
likely a SEPARATE icon-font/atlas defect (the `RndFont::CellDiff` wide-atlas family from the web
polish work is the precedent) — keep it a distinct sub-finding as the kickoff already does. The
capture evidence (`song_select_diffpanel.png`: "25 or 6 to 4" guitar/pro-guitar all-devil vs retail
`yt_qRagnZCIMzk_song_select_diff_ratings.png` dot tiers) confirms both sub-defects are real.

### A9 (MEDIUM, Lane C3 + fence) — text does NOT render via RB3Quad; the kickoff's fence bars Lane C from the likeliest fix TU

Glyph quads are built in `RndText::SetupCharVerts` (`src/system/rndobj/Text.cpp:1201–1234`, UVs from
`RndFont::GetTexCoords`, `Font.cpp:60–72`) and rendered through the **generic mesh path in
`Rnd_Wgpu_RB3.cpp`** — object xfm uploaded as-is (`:5042–5048`), culling disabled
(`key.cull = None`, `:5086`, "RB3 winding varies"), no scale normalization / determinant handling.
`RB3Quad.cpp` is the postproc/blit quad TU, not text. A per-element V-flip via a negative-Y-scale
local xfm is therefore mechanically plausible exactly as the kickoff guesses — but the fix, if the
asymmetry lives in transform handling, lands in the same TU the kickoff fences Lane C OUT of.
Resolution: C3 in Wave 12 = **diagnosis-first, read-only** — draw-log the three action-bar labels'
world xfms + determinants and their authored milo xfms; if (and only if) the fix must touch
`Rnd_Wgpu_RB3.cpp`, escalate to the coordinator for an A8-Wave-11-style declared-range grant
(Lane B's regions `:4360–4735+` vs the DrawMesh xfm region `~:5040–5090` are ~300 lines disjoint —
textual conflict implausible, but the grant must be explicit, not assumed). If the negative scale
turns out to be game-side (a label anim/state setting it), the fix is game-side and the fence holds.

### A10 (LOW, Lane C shape + R-D) — fatness ranking and the Lane A×C interaction

R-C: keep one lane but pre-rank and pre-authorize partial return: **C1 > C2c > C3 > C2a ≈ C2b > C4**
(C1 is the highest-visibility family and now starts at path-tracing per A7; C2c is one log line to a
verdict; C2a/b are census/layout work in the SYS-5 family; C4 is a known residual). If the planner
wants a split, C2 (all three sub-defects, one screen, one capture protocol) is the natural second
lane. R-D: with A3's seam shape (fixed-clock-scoped + opt-in flag), Lane C's default-clock captures
are structurally unreachable by Lane A — **no loader-flag pinning needed**; pin settle-frames instead
(the W4.1 frame-390 mid-zoom trap). Lane C's `drawlog 792 flag-OFF` clause stays valid because the
Lane A seam is opt-in during the wave (A3).

### A11 (LOW, C1 gate form) — make the ≥2:1 contrast gate segmentation-free

"Luma contrast ratio ≥2:1 on the highlight-bar text region" needs a pixel rule or it isn't
re-runnable: pre-register percentiles over the focused-bar ROI — text stroke = p5 luma, bar field =
p60 luma, gate `p60/p5 ≥ 2.0` — measured on the same ROI in the retail ref as calibration (retail
dark-on-gold reads ~3–6:1; today's pale-on-gold ~1.1–1.3:1, so the gate is both achievable and
fail-red on the current build). Apply the same rule to the song-select row and partdiff `GUITAR`.

---

## Lane-by-lane assessment

**Lane A — dispatch only after the A1/A2/A3 rewrite.** The gates (stream-position identity PRIMARY,
N≥10) are well-formed and correctly discriminating — they would fail-red a wrong-axis fix, which is
precisely why the S1 brief must not hard-code the insertion-order mechanism the gates would then
kill. S3 (`--tol` tighten) is free and correct; land it first so the SECONDARY baseline is same-tol
(A4). Biggest schedule risk: S1 may honestly conclude "timing axis, fix = deterministic completion
frames" whose landing is coordinator-sequenced against every fixed-clock golden — plan for a
staged-design exit as a legitimate S2 outcome, and this time the word "staged" must mean an
artifact in the repo (`git apply --check`-able), per the W0.3d.S2 standard.

**Lane B — dispatch with A5/A6.** The diagnosis inheritance is solid (Tier-2 exoneration + CPU
reproduction are trustworthy per W2.8f), the dead-class tripwire is accurate, and the freeze trap is
correctly encoded in the wext gate. The two real risks are gate-gaming (wext-only, A5) and
axis-tunnel-vision (space vs weights/decode, A6) — both closed by building Instrument B in S1 with
the A7 co-variation bar and pre-registered branch semantics. Re-declare current line ranges.

**Lane C — dispatch with A7–A11.** All four user reports verified real against retail refs and
current captures (hub pale-on-gold, sidebar devil corruption + header/art overlap + missing panel,
ticker one-line overlap; U3 taken on coordinator sighting). The characterization-first shape is
right; the amendments mostly redirect day-one effort (skip the C1 regression bisect, start C2c at
`RankTier`, make C3 diagnosis-first) and repair the fence contradiction.

**Cross-lane:** file collision is LOW with the amendments: Lane A (rb3 `obj/DataFile.cpp`,
`obj/DirLoader.cpp`, engine `ThreadCall_Native.cpp`, possibly `math/Rand.cpp` reseed) ∩ Lane B
(engine `Rnd_Wgpu_RB3.cpp:4360–4735+`, `BandCharacter.cpp` read-only) ∩ Lane C (engine
`RB3MaterialBinder.cpp`, rb3 `src/band3` UI, `src/system/ui`) = ∅ as declared; the only same-TU
hazard is C3's conditional DrawMesh fix vs Lane B's probes (A9 escalation rule) and the
already-standing rule that binder-header edits force `Rnd_Wgpu_RB3.cpp` rebuilds (own build dirs —
harmless). Lane A's seam must be opt-in during the wave (A3) or every lane's fixed-clock gate binary
shifts mid-flight.

## R-A / R-B / R-C / R-D — direct answers

- **R-A:** fixed-clock-scoped is right and the default-ON "precedent" is a false memory — SortDraws
  is `RB3FixedClockActive()`-gated (`Utl.cpp:192`); default-ON would also fight the shipped
  incremental-load behavior. Fail-red proof = stream-position identity under induced contention +
  worker-latency jitter, both arms (A4); an insertion-order pin that leaves the count spread standing
  fails PRIMARY — which is the gate doing its job (A2).
- **R-B:** yes — but the composition oracle is in-repo: the W0.1 skin golden's `RefSkinVertex`
  (proven ≡ `RndMesh::SkinVertex`, hand verts included) extended with real `hands_naked` inputs; DC3
  is corroboration only (A6).
- **R-C:** C1 is small once the regression arm is dropped (A7 — the "cheaper first probe" already
  existed and refutes it); keep one lane with the A10 ranking, split C2 out only if the planner
  measures it fat.
- **R-D:** no capture-side loader pinning needed given A3's opt-in seam shape; pin settle-frames, not
  flags (A10).

## Source appendix (verified claims)

rb3 repo unless noted; engine @ `146fd19`.

- `src/system/rndobj/Utl.cpp:192–199` — SortDraws tie-break gate
  `RB3FixedClockActive() && !RB3DrawSortDeterministicOff()`; `:207–214` SortPolls name tie-break.
- `native/src/rb3_replay.h:76–86` — both flag helpers.
- `src/system/rndobj/Dir.cpp:47–84` — SyncObjects: mPolls sorted, **mAnims unsorted** (hash order);
  `src/system/utl/KeylessHash.h:77–171` — linear-probe insert, linear iteration, resize at
  `mNumEntries > mSize/2`.
- `src/system/obj/Dir.cpp:390–483`, `obj/DirLoader.cpp:659–772`, `math/Rand.cpp:80–97` — no gRand in
  the completion chain; MainThread assert.
- `W0.3d/STATUS.md:95–198` — part-b root cause + the ONLY staged patch (SortDraws) + its landing
  handoff; `:152–156` worker-serialization experiment (the timing-seam precedent).
- `WAVE7_REVIEW.md:40–58` — prior on-record refutation of the "unlanded part-b patch" claim.
- `BOOTRNG/STATUS.md:30–39` (11,231-draw spread, 24/24 events, ~7k between events), `:105–112` (A9
  clause scoring), `:229–261` (FX-phase finding + backlog wording that reintroduced "staged").
- `W2.8f/STATUS.md` + `S2_WAVE12_INSTRUMENT_DESIGN.md` — Tier-2 blindness, Instrument A predicted
  GREEN, **Instrument B = the fix gate w/ A7 co-variation**; wext at `Rnd_Wgpu_RB3.cpp:4360–4394`;
  dualskin block now `:4453–4735`, TU 5,775 lines.
- engine `RB3MaterialBinder.cpp:182–213` — relaxed floor, unchanged `a94762f..146fd19` (empty git
  log); `RB3PostProc.cpp:55,217,347–351` + `rb3_postproc.wgsl.inc:228–230` +
  `Rnd_Wgpu_RB3.cpp:1995` / `Rnd_Wgpu_RB3.h:249` — chroma-preserve venue-gating verified.
- `src/system/rndobj/Text.cpp:1201–1234`, `Font.cpp:60–72`, engine `Rnd_Wgpu_RB3.cpp:5042–5086` —
  text = generic mesh path, cull None, xfm as-is, no determinant/scale normalization.
- `src/band3/meta_band/BandSongMgr.cpp:380–401`, `BandSongMetadata.h:76` — RankTier/RankTierToken.
- Visual: `W4.2/cs2_hub_off_vs_on.png` (flag-ON pale-on-gold in Wave 7),
  `/tmp/wave12-current-state/{main_hub_sel1,song_select_diffpanel,song_select_list}.png`,
  `images/retail-screenshots/yt_mhKNp9uAT48_menu_hub.png` (retail dark-on-gold).
