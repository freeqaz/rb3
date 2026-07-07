# PLAN R4 — Scoped loader determinism + per-axis ledger

**Item:** ROADMAP R4 (`RETROSPECTIVE/ROADMAP.md:27`, `OPTIONS.md` §2#4). Sequenced Wave 19
(after the instrument waves; no hard dependency on R1–R3 — R4 can run independently).
**Planner:** Fable, 2026-07-07. All file:line claims below re-verified against source at
rb3 `master` (engine pin `51640ff`, confirmed = `native/CMakeLists.txt:74` and engine HEAD).

---

## 1. OBJECTIVE + non-goals

**Objective.** Make the existing fixed-clock-scoped `RB3_LOAD_DETERMINISM` seam *sufficient*:
10/10 boots with identical post-anchor gRand stream position under induced contention +
worker-latency jitter (the exact PRIMARY that failed in Wave 12 A-S2), by isolating the
*actually-divergent* variable-count gRand consumers onto per-consumer seeded Rand streams.
Ship a standing **per-axis PASS/FAIL ledger** (count / order / stream-position / clock, per
boot) so a "partial" landing can never again silently read as "landed" (the W4→W11 failure).
Then immediately **cash in**: re-grade the held `RB3_VENUE_WHITE_GUARD` on the now-resolving
gate and dispatch the wash per-FX co-sampling instrument on matched boots.

**Non-goals.**
- NOT fixing the WHITE wash itself. BOOTRNG proved the wash driver is FX/swept-light phase,
  one axis deeper (`BOOTRNG/STATUS.md` "NEW FINDING"); R4 makes its gate *resolvable*, no more.
- NOT de-randomizing default boots. Retail-faithful randomness stays live: every change is
  `RB3FixedClockActive() && RB3LoadDeterminism()`-gated and `#ifdef HX_NATIVE`, default-OFF,
  flag-OFF byte-identical (the Wave-12 A3 precedent, already the landed seam's shape).
- NOT touching the MWCC match build's codegen (preprocessor-guarded only; G3 gates this).
- NOT full record/replay or Dolphin work (R1's lane).

---

## 2. CURRENT STATE (verified 2026-07-07, commands run against source)

### 2.1 What is already landed (Wave 12, W0.3d-b A-S2) — all confirmed in-tree

| Piece | Location (verified) |
|---|---|
| Global-stream draw counter `RB3GRandDrawCount()` | `src/system/math/Rand.cpp:18-19` (bumped in `Rand::Int()` `:70-71`, gRand-only) |
| H-RESEED `RB3ReseedGRandAtAnchor()` (canonical `0x5EED`) | `src/system/math/Rand.cpp:28-40`, decl `Rand.h:33-50` |
| Anchor call site (is_playing 0→1) | `src/band3/game/GamePanel.cpp:320-322` |
| Opt-in flag helpers | `native/src/rb3_replay.h:79` (`RB3FixedClockActive`), `:98` (`RB3LoadDeterminism`) |
| Worker-serialize lever | engine `src/platform/ThreadCall_Native.cpp:65-71` (`LoadDetSerialize`), `:199-205` |
| Fail-red jitter `RB3_LOADDET_JITTER` | engine `ThreadCall_Native.cpp:29-40` |
| Deterministic `mAnims` name-sort | `src/system/rndobj/Dir.cpp:68-84` (flag-gated, in `SyncObjects`) |
| Probe TU + frame/completion taps | `native/src/rb3_loaddet_probe.cpp`, taps per W0.3d-b STATUS |
| Gate harness (post-anchor delta, arms, jitter) | `docs/.../execution/W0.3d-b/loaddet_gate.py` (`--n/--k/--jitter/--arm`, default n=10, k=480, input-free by default; autohit is an explicit confound flag) |
| Flag registered PARTIAL / DO-NOT-flip | engine `NativeCompatFlags.gen.inc:197` |

**Measured state of the seam (A-S2 pre-registered gate, input-free, jitter=200µs):** reseed
alone FAIL; +worker-serialize FAIL (~62% spread reduction, OFF 1910 → ON 733, 3/3 distinct);
+mAnims-sort FAIL (~36%, 4/4 distinct). PRIMARY (spread==0) never met. Coordinator correctly
did not flip. (`W0.3d-b/STATUS.md` gate table.)

**Also verified:** all global-stream draws route through the `RandomInt/RandomFloat/SeedRand`
free functions in `Rand.cpp:104-124` — `grep -rn "gRand" src` shows **no** direct `gRand.Int()`
callers outside `Rand.cpp`, and `Rand::Gaussian` has no gRand-instance callers at all. This
makes wrapper-level attribution (M1 below) a *complete* tap on the shared stream.

### 2.2 PREMISE CORRECTIONS — the "three named families" list is partially wrong

The A-S2 staged design names four "concrete next sites". Verification against source:

1. **`Wind::` Gaussian — REFUTED as a live consumer.** `src/system/rndobj/Wind.cpp:9-35`:
   the Gaussian rejection loop runs on a **local** `Rand sRand(0x7FEF8A)` created in
   `RndWind::Init()` and **deleted at the end of Init** (`:25`, `:33-34`). It never touches
   gRand and never runs per-frame. Wind's only global-stream draws are `RandomFloat(0,1)`
   ×0x400 at Init (`:30`) — a **fixed count**, which cannot couple order into count.
   `grep -rn Gaussian src` confirms Wind.cpp is the *only* Gaussian caller in-tree.
   → "Give Wind its own Rand" (staged site #2) is a dead site; implementing it would be
   wasted work and a false sense of coverage.
2. **`Crowd` Fisher-Yates — location right, liveness unproven.** The shuffle is at
   `src/system/world/Crowd.cpp:1233-1238`, inside `WorldCrowd::OnIterateFrac` — a **script
   handler** (`HANDLE(iterate_frac, ...)` `:1198`). Whether venue scripts fire it per-frame
   (and in the gate window) has never been measured. The other Crowd draws (`:810-812`,
   random color-palette picks) are crowd-*build*-time, not per-frame.
3. **`CameraShot::Shake` conditional draws — CONFIRMED variable-count.**
   `src/system/world/CameraShot.cpp:258-274`: `if (RandomFloat() < freq) { ...4 more
   RandomFloat draws... }` → 1 or 5 gRand draws per call, per-frame **while a shaking shot is
   active**. Real — but it cannot explain the W0.3d-b *headless-boot* divergence (first
   divergent frame 4, menus, no camera shots), so it is at most one of several sites.
4. **`mAnims` walk — already fixed** (Dir.cpp sort, landed; verified above).

**Consequence (the load-bearing planning fact):** the per-frame consumers that actually
produce the residual count divergence in the gate window **have never been named** — A-S1
itself said the exact leak sites are "one instrument deeper (per-consumer tagging)". Wave 12
then burned three escalating fix variants against an unverified site list. R4 must therefore
buy the attribution instrument FIRST (M1) and fix only measured-divergent sites — not
re-inherit the staged list.

Candidate surface for reference (per-frame subsystems calling `RandomInt/RandomFloat`,
verified by grep): `rndobj/{Part,PartLauncher,AnimFilter,EventTrigger,PostProc,Gen,Utl,Wind}`,
`char/{CharClipSet,CharClipGroup,CharClipDriver,CharDriver,CharInterest,CharLookAt,CharEyes,
Waypoint,FileMergerOrganizer}`, `world/{Crowd,CameraShot,LightPresetManager}` — ~20 files.
17 more in `band3/`. M1 ranks these by measured divergence; nothing is fixed on suspicion.

### 2.3 The scale of the problem (BOOTRNG, verified in STATUS)

At the pinned gameplay capture: `gdraw` 349,822–361,053 (**~11,231-draw spread**, 12 distinct
in 12 boots); lighting event COUNT deterministic (24/24) but ~7k draws vary *between* events
(`BOOTRNG/STATUS.md`). Post-anchor (the gate metric): OFF spread 1910–2086 under jitter.

---

## 3. DESIGN

### 3.1 Decision: per-consumer isolated Rand streams, NOT call-site reseeding

Picked with evidence:

- **Call-site reseeding is measured-dead.** Anchor reseeding (the value-pinning family's
  cheapest member) failed PRIMARY three times with escalating hardening (A-S2 table).
  Reseeding fixes *values*, not *order/count*: any variable-count consumer downstream of a
  reseed still consumes a boot-varying number of draws once order varies.
- **Per-call-site reseeding of the shared stream is worse than the disease.** Reseeding gRand
  before each consumer every frame would hand every consumer the *same* values every frame —
  frozen/repeating randomness (crowd, FX) — a visible behavior change under the flag that
  would corrupt exactly the wash/FX gates R4 exists to serve.
- **Isolation decouples count from order by construction.** Move every measured
  variable-count consumer onto its own stream → the global per-frame draw count becomes a sum
  of **fixed-count** consumers → invariant under any order permutation of the walk. The
  address-shuffle race can persist and PRIMARY still collapses. Each isolated stream's own
  position depends only on that consumer's own call sequence (deterministic given the proven
  identical loaded set + fixed clock).
- **It is an existing engine idiom, hence Wii-faithful in shape.** The retail code itself
  uses private Rand instances (`Wind.cpp:9` `sRand(0x7FEF8A)`, `CameraManager::sRand` per the
  Rand.cpp comment `:14-16`). We add flag-gated equivalents, never touching the default path.
- **Faithfulness constraint honored:** all routing is
  `RB3FixedClockActive() && RB3LoadDeterminism()`-gated inside `#ifdef HX_NATIVE` — the exact
  Wave-12 A3 precedent already reviewed and landed. Default boots: byte-identical (G2/G3).

The landed reseed + worker-serialize + mAnims-sort **stay** (harmless hardening under the
same flag; serialize also removes the load-time race that would otherwise shuffle
*within-consumer* value assignment).

### 3.2 New API (small): per-tag streams

`src/system/math/Rand.{h,cpp}`, `#ifdef HX_NATIVE`, beside the existing seam helpers:

```cpp
// Returns the per-consumer isolated stream for `tag` when the load-determinism
// seam is active, else nullptr. Streams are lazily created, seeded
// 0x5EED ^ fnv1a(tag), reset at RB3ReseedGRandAtAnchor so post-anchor state is
// boot-invariant. Registry: std::map<std::string, Rand*> (main-thread only,
// same MainThread() contract as RandomInt).
Rand *RB3LoadDetStream(const char *tag);

// Convenience used at call sites (keeps diffs one-line):
inline int   RB3DetRandomInt(const char *tag)                 { if (Rand *r = RB3LoadDetStream(tag)) return r->Int();        return RandomInt(); }
inline int   RB3DetRandomInt(const char *tag, int a, int b)   { ... }
inline float RB3DetRandomFloat(const char *tag)               { ... }
inline float RB3DetRandomFloat(const char *tag, float a, float b) { ... }
```

Call-site pattern in shared-with-MWCC files (template = the landed `Dir.cpp:68-84` shape):

```cpp
#ifdef HX_NATIVE
        int j = RB3DetRandomInt("crowd.iterfrac") % (i + 1);   // routes to gRand when seam off
#else
        int j = RandomInt() % (i + 1);
#endif
```

Anchor integration: `RB3ReseedGRandAtAnchor()` additionally re-seeds every registered tag
stream, so post-anchor per-consumer state is independent of pre-anchor consumption (same
rationale as the gRand reseed).

### 3.3 Attribution instrument (M1) — names the real divergent consumers

Extend the existing probe (new env `RB3_LOADDET_ATTRIB=1`, implies `RB3_LOADDET_PROBE`):

- In the four free-function wrappers (`Rand.cpp:104-124`) — the verified-complete tap —
  record `__builtin_return_address(0)` per draw into a per-frame `addr → count` map
  (flat array + open addressing; this is a probe, perf just needs to not distort scheduling).
- At the existing per-frame flush (`RB3TraceSetFrame` tap in `native/src/rb3_session_trace.cpp`),
  emit `[LOADDET] attrib frame=N pc=0x... sym=<dladdr name> draws=K` per caller.
- Harness (`loaddet_gate.py --attrib`, M1 deliverable): align boots on frame (boot window) and
  on anchor+k (gate window), diff per-caller per-frame counts across boots, output a ranked
  **divergent-caller table** (caller, frames divergent, per-frame count range across boots).

This converts "which unsorted walk feeds which rejection sampler" from hypothesis to a table,
for ~1 TU of probe code. Known blind spot: draws made via a gRand *member* call would bypass
the wrappers — verified none exist today (§2.1); M1 re-asserts this with
`grep -rn "gRand\." src` in its runbook.

### 3.4 Per-axis ledger — design + where it lives

**Producer:** promote the gate harness to `scripts/native/loaddet_gate.py` (from
`docs/.../W0.3d-b/loaddet_gate.py`; leave a one-line pointer stub behind). New `--ledger`
mode writes `ledger.json` per run:

```json
{ "meta": { "date", "binSha", "flags", "n", "k", "jitter", "tolMs": 150 },
  "referenceBoot": 0,
  "boots": [ { "boot": 1,
      "axes": {
        "count":  { "value": "<final gdraw + md5(per-frame gdraw deltas)>", "pass": true },
        "order":  { "value": "<md5(completion sequence)> [+ md5(attrib caller sequence) when --attrib]", "pass": true },
        "stream": { "value": "<postAnchorDelta>", "pass": true },
        "clock":  { "value": "<anchorFrame, songMs@capture, fixedClockActive>", "pass": true } } } ],
  "summary": { "count": "10/10", "order": "10/10", "stream": "10/10", "clock": "10/10",
               "PRIMARY": "PASS|FAIL" } }
```

Axis definitions (each PASS = equal to reference boot within its rule):
- **count** — per-frame gdraw delta sequence identical (md5) AND final gdraw equal in the
  post-anchor window. (Pre-anchor absolute gdraw is *reported* but non-gating — the seam
  re-bases at the anchor by design.)
- **order** — DirLoader/DataLoader completion sequence md5 identical (the W0.3d-b `complete`
  lines); when `--attrib` is on, also the caller-sequence md5.
- **stream** — `postAnchorDelta` identical (spread 0). **This axis is PRIMARY.**
- **clock** — anchor frame equal, capture songMs within ±150 ms (the Wave-12 Task-0 `--tol`
  discipline), `RB3_FIXED_CLOCK` confirmed active in-log.

**Where it lives:** the harness in `scripts/native/`; per-run evidence committed under
`docs/native/engine-arch-review-2026-07-05/execution/R4/evidence/ledger-<tag>.json`
(OPTIONS §4 lint #7: probes write under `execution/<KEY>/evidence/`, never bare `/tmp`;
close-out checklist includes "evidence committed"). The classjson entry for
`RB3_LOAD_DETERMINISM` is updated at M3 with the ledger location so future waves find it.

### 3.5 Files touched (implementation inventory)

| File | Change |
|---|---|
| `src/system/math/Rand.{h,cpp}` | `RB3LoadDetStream` + Det wrappers + attrib tap (all HX_NATIVE) |
| `native/src/rb3_loaddet_probe.cpp` | attrib aggregation + per-frame flush |
| measured-divergent consumer files (from M1; e.g. `world/CameraShot.cpp`, `world/Crowd.cpp`, TBD) | one-line flag-gated reroute per site, Dir.cpp-template shape |
| `scripts/native/loaddet_gate.py` | promoted harness + `--attrib` + `--ledger` |
| engine `NativeCompatFlags.classification.json` (+regen gen.inc) | flag text: PARTIAL → outcome at M3; ledger pointer |
| `execution/R4/{PLAN,STATUS}.md`, `execution/R4/evidence/` | wave docs + committed evidence |

---

## 4. MILESTONES (with go/no-go exits)

### M1 — Attribution: name the real divergent consumers (cheapest decisive step)
Build §3.3, then run **OFF-arm** (seam off, jitter 200µs), input-free, N=4, in BOTH windows:
(a) boot window frames 0–120 (the W0.3d-b frame-4 divergence), (b) the gate window
anchor→anchor+300. Produce the ranked divergent-caller table; commit it to
`execution/R4/evidence/attrib-<window>.json`.

- **GO:** divergent variable-count callers form a small named set (≤ ~8 call sites) →
  proceed to M2 with exactly that list. (Expectation from §2.2: CameraShot::Shake and some
  subset of Part/PartLauncher/CharClip*/Crowd; Wind formally exonerated.)
- **NO-GO:** divergence is diffuse (> ~8 sites, or dominated by unresolvable/inlined frames)
  → STOP, report honestly, re-price. Fallback options at re-price: (i) tag at owning-walk
  granularity (isolate the whole `RndDir::SetFrame`/Poll walk behind one stream), (ii)
  ledger-only landing (standing instrument, no PRIMARY claim), flag text stays DO-NOT-flip.
- M1 also re-verifies wrapper completeness (`grep gRand\.` = no member-call bypasses).

Est: 0.25–0.5 lane-wave. This retires the plan's main risk before any fix is written.

### M2 — Isolate the M1-named consumers + ledger harness
Land `RB3LoadDetStream` + one-line reroutes at the M1 sites (Dir.cpp-template, flag-gated);
promote the harness with `--ledger`/`--attrib`. Iterate per-site: after each site,
`loaddet_gate.py --n 4` quick pass — spread must shrink monotonically toward 0 with each
isolation (per-site delta is committed evidence; if a site's isolation moves nothing, back it
out — no dead flags).
**Exit:** N=4 ON-arm `deltaSpread == 0` (1 distinct) with jitter on. Est: 0.5 wave.

### M3 — PRIMARY gate at scale + inertness proofs + flag disposition
`scripts/native/loaddet_gate.py --n 10 --k 300 --jitter 200 --ledger` (input-free):
- **PRIMARY PASS:** ON arm `deltaSpread == 0`, 10/10 boots, all four ledger axes PASS.
- **Fail-red:** OFF arm under identical jitter reproduces spread > 0 (it always has: 468 /
  211 / 1910 / 2086 across prior runs).
- **Inertness:** G2 + G3 below.
Then the coordinator (not the lane) flips the classjson disposition: `RB3_LOAD_DETERMINISM`
PARTIAL/DO-NOT-flip → PASS-PRIMARY, becomes the standard env for fixed-clock gate harnesses
(it stays opt-in for user boots — it is a determinism *harness* seam, per the A3 precedent),
with the required **flag-ON drawlog re-golden** for any gameplay-scene goldens (the seam
changes the gRand stream by construction; splash goldens are unaffected — reseed never fires
pre-gameplay, proven in A-S2).
Est: 0.25 wave.

### M4 — Immediate cash-in (the ROADMAP Wave-19 protocol)
1. **WHITE guard re-grade.** Re-run the held `RB3_VENUE_WHITE_GUARD` grade
   (WHITE-fix/WASH harnesses: `wash_matrix.py` / `white_ab.py`, `--tol 150`) with
   `RB3_LOAD_DETERMINISM=1` on BOTH arms, N=10 guard-ON vs guard-OFF. **Validity precondition:
   the ledger must PASS 10/10 on these exact boots** — otherwise the re-grade is void (this is
   the lint that prevents re-grading on a still-noisy floor). Outcome (either way) updates the
   WHITE-fix STATUS + the guard's classjson entry.
2. **Dispatch wash per-FX co-sampling.** Write the kickoff charter for the BOOTRNG backlog
   item 3 instrument (PartLauncher emission + swept-light position co-sampled against
   `hi_frac` per frame) explicitly premised on matched boots ("compare frame N of boot A vs
   boot B at identical stream position"). R4 delivers the substrate + charter; the instrument
   itself is that lane's work, not R4's.
Est: 0.25–0.5 wave.

---

## 5. GATES (each with its fail-red demonstration)

| # | Gate | PASS condition | Fail-red demonstration |
|---|---|---|---|
| G1 | **PRIMARY** stream axis | ON arm: `postAnchorDelta` spread == 0, 10/10, jitter 200µs, input-free | OFF arm same jitter: spread > 0 (reproduced in every historical run: 468/211/1910/2086) |
| G2 | Flag-OFF runtime inertness | Normal boot: 0 `[LOADDET]` lines; `drawlog-golden --fixed-clock --scene splash_screen` = 792 byte-identical flag-OFF | Deliberately un-gate one reroute in a scratch build → probe line appears / gameplay drawlog diverges → gate must catch it (proves sensitivity), then revert |
| G3 | Wii-match inertness | `batch_objdiff` on every touched match unit (Rand, Crowd, CameraShot, + M1 sites): match % byte-unchanged | Scratch build with one reroute outside `#ifdef HX_NATIVE` → match % drops on that unit → revert |
| G4 | Ledger axes are real gates | Each axis FAILs on the OFF arm (count/order/stream) and PASSes on ON, same run | The OFF arm *is* the fail-red (per-axis FAILs must actually appear in `ledger.json`, not be vacuous) |
| G5 | Re-grade validity (M4) | WHITE re-grade boots carry an attached 10/10-PASS ledger | Run one re-grade boot with seam OFF → ledger stream axis FAIL → harness must refuse/mark the re-grade invalid |
| G6 | No dead flags | Every isolated site shows a measured per-site spread reduction in M2 evidence | A site with no effect is backed out (evidence committed either way) |

---

## 6. RISKS + mitigations (and what could invalidate the plan)

1. **M1 shows diffuse divergence (many small consumers).** The single biggest risk — it
   invalidates the "small named set" premise the ROADMAP row inherited from A-S2. Mitigation:
   M1 is first and cheap; the NO-GO branch (walk-granularity isolation or ledger-only landing)
   is pre-priced in §4. Do not silently widen scope past ~8 sites.
2. **The divergent consumers differ between boot window and gate window.** Likely (menus vs
   gameplay). Mitigation: M1 measures both; **only the gate window gates PRIMARY** — the boot
   window is informative (BOOTRNG absolute-position spread) but non-blocking.
3. **A variable-count consumer's count depends on another consumer's *values*** (e.g. spawn
   count derived from a RandomFloat elsewhere). Isolation still closes this — every isolated
   stream is deterministic given deterministic per-consumer call order, which fixed clock +
   identical loaded set (proven, 511/511 completions byte-identical) provide. Residual: a
   consumer whose call order depends on a *still-shuffled address-ordered container* — if M2
   plateaus above 0, that container walk gets the Dir.cpp name-sort (template exists).
4. **dladdr symbol resolution too coarse** (static functions, inlining at -O2 native builds).
   Mitigation: run attribution on the debug/clang `-O0`-ish native build
   (`native/build-agent-*`); fall back to raw PCs + `addr2line` offline in the harness.
5. **Isolation changes flag-ON visuals** (different random values than the shared-stream
   path). True and accepted: flag-ON is a harness regime; gameplay-scene goldens re-golden at
   M3 (explicit provision), splash goldens unaffected. Flag-OFF untouched (G2/G3).
6. **The wash re-grade still doesn't resolve** (WHITE variance driven by something the seam
   doesn't pin, e.g. GPU/driver timing). That is a legitimate M4 *finding*, not a plan
   failure: the ledger proves the boots were stream-matched, so a persisting spread cleanly
   indicts a non-RNG axis — exactly the discrimination the campaign lacked.
7. **Duplicate-work collision** (another lane touches Rand/loader). Mitigation: re-grep the
   touched files on current master before landing (the WAVE-5 ghidriff lesson); the seam flag
   and helpers already exist, so churn surface is small.
8. **`std::map` allocation inside the stream registry perturbing the alloc race.** The
   registry allocates only under seam-ON; the OFF arm (fail-red) is untouched. Under ON the
   goal is determinism, and main-thread lazy allocs at first-use are themselves deterministic
   once order is; if M2 evidence suggests otherwise, pre-size the registry at anchor time.

---

## 7. COST + what it unblocks

**Cost:** 1.25–1.75 lane-waves (M1 0.25–0.5, M2 0.5, M3 0.25, M4 0.25–0.5) — inside the
ROADMAP's 1–2 estimate. One implementation lane (Opus) + coordinator gate/flip authority;
no new build infrastructure (existing probe TU, harness, jitter lever, gate protocol all
landed and verified present).

**Unblocks (OPTIONS §2#4's consumer list, restated concretely):**
- `RB3_VENUE_WHITE_GUARD` re-grade on a resolving gate (M4.1 — held since Wave 9/10).
- The wash per-FX co-sampling instrument's matched-boot substrate (M4.2 → the WHITE
  real-lever chain).
- BOOTRNG A9-PRIMARY clause 3 ("same gRand stream position") — currently 12/12 distinct.
- Every future per-boot numeric visual gate (the E1-adjacent gates R1/R3 items will want),
  which today all sit on the ~11k-draw BOOTRNG noise floor.
- The standing ledger turns any future "partial landed" into four explicit per-axis verdicts.
