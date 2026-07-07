# Wave 5 — Retrospective (hindsight review)

**Run:** `wt2tbkfja`, 20 agents, 2026-07-06. Engine `609efb7` → `a4bde9f`.
**Reviewer:** retrospective (read-only), with Wave 6 outcome in hand.

## Goals (as dispatched)

Lane A (sequential): `W0.3d-fix` (deterministic draw order) → `W2.1-flip` (ship the
crowd/drum placement fix, flip `RB3_PLACEMENT_CONTRACT` default-ON) → `W3.1a` (fog +
projLight fill, the struct-neutral first half of SYS-4 lighting).
Lane B (parallel): `W0.6b` (classify the ~89 game-root `FlagClass::Unknown` flags).

## What shipped

- **W0.3d-fix — complete (`76f51077`).** Deterministic material-name tie-break in
  `SortDraws`, gated `RB3FixedClockActive() && !RB3DrawSortDeterministicOff()`, Wii
  byte-identical. Fail-red proven under max entropy (async+ASLR): default 12/12 identical
  order, opt-out 12/12 distinct. This was the re-golden prerequisite and it delivered cleanly.
- **W0.6b — complete (`1fd2bfc`).** 91 game-root flags classified probe/workaround/feature/
  perf, append-only, zero game-root Unknown rows; single deferred regen at 321 flags. Clean
  hygiene, no behavior risk. The Fable review correctly caught that the drafted exit ("census
  exit 0") was already true and vacuous, and re-pointed the item at classification.
- **W2.1-flip — ready-for-flip (`dbf2758` mechanism, `c49fddb4` drum oracle).** Opt-out-first
  `RB3_PLACEMENT_CONTRACT_OFF`, drum-kind oracle with fail-red, UI A/B (main_hub + song_select,
  byte-identical), Dolphin A/B package produced. Numerically proven; committed default STILL OFF.
- **W3.1a — partial (`a4bde9f`, default-OFF `RB3_ENV_FOG`).** Fog fill from `RndEnviron` into
  existing `SceneUniforms` fields; DC3 zero-blast confirmed, flag-OFF byte-identical, tests
  198/0/2.

## What was held / refuted (the load-bearing part)

**The flip was HELD by the coordinator on the E1 visual gate** — the one high-value outcome the
whole wave was built around did not ship. In the Dolphin A/B montage, flag-ON `cap_ON_1` blew out
nearly full-white while the other three frames rendered normally, so the coordinator read the
blow-out as flag-ON-specific ("an exposure wash appearing in only one flag state is a new
finding") and deferred to a new backlog item **W2.1-flip-blocker**.

**Wave 6 refuted that premise by measurement.** With `wash_score.py` (built in W2.1-flip-blocker.S1)
and n=7 songMs-pinned captures per flag state: wash rate OFF 2/7 vs ON 4/7 (Fisher p≈0.59, NS),
luma Mann-Whitney **U=24.0, p=1.0** — no flag effect on brightness. The wash is a full-frame
magenta env cast covering the venue backdrop + note highway + band, which a *crowd-only* transform
cannot produce. Verdict: **A/A-variable, flip-independent.** The flip shipped Wave 6 (`fced18b`).

## Premise failures and how they were (eventually) caught

1. **"The flag-ON white blow-out is caused by the flip."** FALSE. Caught in Wave 6 by an N≥7
   per-state, songMs-pinned, numerically-scored comparison. **The signal that would have caught
   it was already inside the Wave-5 package the coordinator reviewed:** the S4 agent's own luma
   numbers were OFF_1=95.2, **OFF_2=23.7**, ON_1=202.5, ON_2=122.6 — the flag-OFF pair alone
   swings 4× (95→24, a near-black wash *in flag-OFF*), and the S4 agent explicitly labeled this
   "exactly the flag-independent confound the A/A protocol exists to surface." The two-sample
   budget let the coordinator over-read one flag-ON outlier as a flag effect while an equally
   extreme flag-OFF outlier sat in the same montage.

   The catchable-earlier lever is **not** the hold itself — with only 2 samples/state and a visible
   full-white frame, holding was the disciplined call under the campaign's "visual gate catches what
   looks fine misses" rule. The miss is one level up: the **E1 brief set the sample floor at N=2**
   (`WAVE5_REVIEW.md` R-E, "2 per flag state minimum") *despite the review itself naming the
   pre-existing A/A-variable pink-bloom wash as the known confounder*. Two samples cannot dissolve a
   stochastic wash present in ~half of boots. A brief that mandated N≥6 per state + a numeric wash
   score in-lane would have produced the Wave-6 verdict inside Wave 5.

2. **"A boot-reachable venue authors fog, so W3.1a can render-verify the fill."** FALSE. All 34
   distinct boot-reachable `RndEnviron` report `FogEnable()==false`. Discovered only *after* the fog
   fill landed (S1), leaving Exit-#2 unverifiable; S2 (projLight) then died on API overload. The
   render-proof slipped to Wave 6 W3.1b via an `RB3_ENV_FOG_FORCE` probe. An asset census up front
   would have told the planner "no reachable venue authors fog → render-verify needs a forced/
   synthetic env" before dispatching a subtask whose exit depended on an asset that does not exist.

## Tooling gaps (name the capability, the one-run question, what was done instead)

1. **Numeric stochastic-wash classifier + N-boot sampling protocol.** Capability: score N boots
   per flag state for luma/hue wash class and run a distribution test (Mann-Whitney/Fisher) → one
   run answers *"is this venue-cam wash flag-dependent or A/A-variable?"* It did not exist in
   Wave 5; the E1 gate was human-eyeball over a 2×2 montage. Wave 6 built exactly this
   (`scripts/native/wash_score.py`, `--selftest` green) as **W2.1-flip-blocker.S1** and used it in
   S2 to reach the p=1.0 verdict. Cost of not having it in Wave 5: a full Wave-6 blocker lane —
   S1 (build detector) → S2 (measure n=7) → S3 (backlog + A5 pre-flip checks) → S4 (re-package
   sign-off) ≈ **4 agent-stages plus a coordinator sign-off cycle** — to answer a question whose
   raw data (bimodal luma in *both* flag states) was already sitting in the Wave-5 S4 package.

2. **Environ fog/light-authoring census.** Capability: grep all boot-reachable `RndEnviron` for
   `FogEnable()==true` (and, later, kFakeSpot/gobo authors) → one run answers *"is there any asset
   that will exercise this render path?"* Absent, W3.1a landed a correct fog fill and only then
   found there was nothing to render it with; the render-proof and projLight both slipped to
   Wave 6. Running the census before dispatch would have re-scoped W3.1a to "fill + forced-env
   proof" from the start instead of discovering the block mid-lane.

## Recurring bug families touched

- **WASH / pink-bloom venue-cam blow-out** (kin to the A2/A3/A4 glow-bloom work and W3.3 grayscale
  venue): this wave *mis-attributed* it to the flip and let it block shipping. State after Wave 6:
  proven flip-independent and A/A-variable, filed as its own WASH backlog item (ranked prior: async
  asset residency > RB3PostProc grade > P4 venue-light); still open, but no longer blocking.
- **UI placement (hub bar / scrollbar thumb):** the flip's regression gate confirmed the contract
  is vertex-invariant and the two screens stay byte-identical A/B — the deletion premise for
  `RB3_NO_HUB_BAR_PLACEMENT_FIX`/`RB3_SCROLLBAR_THUMB_FIX_OFF`/`RB3_NO_CROWD_REBIND` stays refuted
  (permanent companions of the contract). No regression.
- **Char-skinning (hands/crowd)** negative controls (HandsBindOracle, SKIN_CLAMP, lineup) held
  across the readiness work.

## Wasted effort

**Moderate.** The crowd/drum placement fix was ship-ready in Wave 5 — numerically proven (crowd +
drum oracle GREEN), safety-proven (flag-OFF byte-identical), package produced — and instead shipped
one wave later. The delta is the entire W2.1-flip-blocker lane (≈4 agent-stages + coordinator),
spent to refute a premise that (a) the Wave-5 S4 agent had already flagged in words and numbers, and
(b) a review-mandated N≥6-plus-numeric-score E1 gate would have dissolved in-wave. This is a
gate-design cost, not an implementation cost: everything that shipped was clean. W3.1a's asset-block
is a smaller, separate slip (fill solid; render-proof + projLight pushed to W3.1b) that a one-run
environ census would have anticipated.

## What the wave got right (worth keeping)

- Fable's pre-dispatch review caught three real defects before dispatch: the "one-line flip" was
  presence-truthy and would have shipped with no opt-out (→ `RB3_PLACEMENT_CONTRACT_OFF`); the flip
  breaks the committed 888-draw golden (→ ordered W0.3d-fix → flip → one re-golden); and the UI
  regression gate was aimed at song_select where the hub bar isn't observable (→ added main_hub).
  All three held up.
- The single-writer `classification.json` / one-regen discipline (D2) prevented the W2.6-style
  gen.inc clobber across three flag-touching lanes.
- The hold, given the evidence *in front of the coordinator*, was consistent with campaign safety
  policy. The fix is upstream in the gate spec, not in the coordinator's judgment call.
