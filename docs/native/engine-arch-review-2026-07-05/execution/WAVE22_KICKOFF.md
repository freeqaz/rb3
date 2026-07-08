# Wave 22 — Kickoff (VISUAL PUSH: FOREARM-FLOAT fix ∥ HUD family ∥ discovery sweep)

**Author:** coordinator. **Status:** DRAFT — for Fable pre-dispatch review.
Parent: `WAVE21_CLOSEOUT_REVIEW.md` Q7 (hands CLOSED; FOREARM-FLOAT first) + owner directive
2026-07-08: *"keep pushing through the visual bugs we can find."*
Engine pin `be70ca8`. TWELVE defaults ON. **FIX + DISCOVERY wave** — flag-gated fixes
default-OFF; coordinator-only default decisions at close-out.

## COORDINATOR ACCEPTANCE (<pending review>)

_To be filled from `WAVE22_REVIEW.md`._

- **Hazard note:** engine tree carries uncommitted `M FxSendNative.cpp`; rb3 tree carries
  `native/src/rb3_session_trace.cpp` — never stage either. Hands are CLOSED — no lane may
  re-charter the hands-finger family or re-attempt any of the 8 dead cells / the reskin.

## Shape

Owner wants momentum on visual bugs. Wave 22 = one named-bug fix (FOREARM-FLOAT, the R5
close-out's #1 deferred visual item), one UI-family fix (the gameplay HUD gaps from the
2026-07-02 visual diff), and a fresh native-vs-retail discovery sweep so the pipeline stays
full for Wave 23. All three are independent; no engine/rb3 file overlap expected (FOREARM is
bandobj/char + maybe engine pose; HUD is band3 UI; SWEEP is scripts + triage docs only).

## Lanes

**Lane FOREARM — fix the player3 right-forearm float (Opus):**
Lane dir `execution/W22-FOREARM/`. The R5 CLOSURE backlog key, already NAMED by T2
(`T2-WORLDROI/evidence/M5-forearm-float-triage.md`): the persistent top-center floating
flesh structure = `gloves_resource.mesh` + `clearcoat_resource.mesh` (sleeve), owner
**player3**, bones `bone_R-foreArm`/`bone_R-foreTwist1`/`foreTwist2`/`bone_R-hand`;
`boneFallback=0` on every draw → NOT a bind-clamp/skin-fallback issue → the arm bone chain is
genuinely POSED to an elevated detached screen position. This is a POSE/placement float,
distinct from the closed finger-level class.
- **Discriminator FIRST (Wave-20/21 lesson — binding before pose):** determine whether
  player3's right-arm outfit meshes (`gloves_resource`/`clearcoat_resource`) bind their
  `bone_R-foreArm`/`foreTwist`/`hand` slots to player3's OWN per-member skeleton or a
  SHARED/wrong instance (the `RebindOutfitBonesToOwnSkeleton` family — reuse the Wave-20/21
  LOADBIND slot-dump machinery, `RB3_LOADBIND_PROBE`, scoped to player3 right-arm). If the
  forearm binds a WRONG instance, that IS the bug (a binding fix, torso-rebind-pattern) — and
  UNLIKE the hands this is compact-ish arm geometry that the torso rebind already handles.
- **Only if binding is exonerated:** investigate what POSES `bone_R-foreArm` up-and-detached
  (clip driver? IK? a prop/instrument attachment transform? mirrored/transposed ObjPair like
  the A4 venue-env 1-line transpose `d988a301`?). Use T2's `uidump_query.py --roi` +
  `RB3_DRAWLOG_PROV=1` to track the bone world across frames; compare the R-foreArm world vs
  the L-foreArm world (bilateral symmetry is a strong tell — a transpose/sign bug shows as
  R-only divergence).
- **Fix flag-first, default-OFF, HX_NATIVE.** Gate: matched-frame E1 (float GONE, band arm in
  place, bilateral symmetry restored, other members non-regressing); flag-OFF drawlog-792
  byte-identical; batch_objdiff==baseline on any touched src/system unit; if engine-side,
  commit engine first + note the pin (coordinator bumps once at close-out). Absorb the Wave-21
  mitten-OFF "arguably worse" observation IF it proves forearm-level (check whether the
  mitten-OFF worsening is this same right-arm structure).

**Lane HUD — gameplay HUD/star-meter family (Opus):**
Lane dir `execution/W22-HUD/`. From the 2026-07-02 visual diff
(`memory/project_visual_diff_2026_07_02.md`): (1) gameplay **score HUD renders mid-screen**,
not top-right per retail; (2) **star meter renders one outlined star that never fills** vs
retail's filling star-pip row. **CONFIRM-THEN-FIX:** first re-capture native gameplay vs the
retail ground truth (`images/retail-screenshots/gameplay_highway_wikipedia.jpg`,
`fandom_gameplay_guitar.png`) on the CURRENT build — some may have been fixed since 2026-07-02.
For each still-broken item: root-cause (UI panel anchor/resolution math — cf the RndFont
CellDiff wide-atlas + UIList slot family; the score HUD position likely a panel-anchor or
safe-frame constant; the star meter likely a fill-driver/percent-binding not updating) and fix
flag-first default-OFF. If BOTH are already fixed, PIVOT to the next-highest gameplay-HUD
visual gap the recapture surfaces (report the pivot). Gates: matched-frame E1 vs retail,
drawlog-792 flag-OFF byte-identical, no regression to the shipped HUD.

**Lane SWEEP — fresh native-vs-retail visual discovery + triage (Opus; scripts + docs only):**
Lane dir `execution/W22-SWEEP/`. Keep the visual-bug pipeline full. Capture native at the
canonical screens (main_hub, song_select, part/diff select, gameplay — reuse
`scripts/native/song-select-capture.py` / `keyboard-to-gameplay.py`) and diff against the
retail ground truth in `images/retail-screenshots/` + the prior `/tmp/visdiff-20260702`
baseline. Produce a RANKED triage table (severity × confidence) of the top remaining visual
gaps, each with: screen, description, retail-vs-native crop, and — where it's a world/skinned
element — T2 `uidump_query.py --roi` provenance NAMING the owning mesh/bone/owner (the
FOREARM-FLOAT precedent: convert eyeball sightings into one-query named targets). EXCLUDE the
CLOSED hands-finger family and known-shipped items (the twelve defaults — grep them first so
the sweep doesn't re-report fixed bugs). Deliverable = the Wave-23 visual menu. NO fixes.

## Process rules (carried) — VERBATIM per KICKOFF_TEMPLATE.md

Locks: rb3 `/tmp/rb3-git.lock`; engine `/tmp/milo-engine-git.lock`; classjson
`/tmp/milo-engine-classjson.lock` (append-only, single coordinator regen at close-out).
Checkpoints `/tmp/wave22-checkpoints/<lane>.json` — check-first, write-before-return, update
every milestone. PLAN/STATUS under `execution/<KEY>/`. Evidence committed or it doesn't exist.
New flags default-OFF; NO default flips, NO pin bumps by lanes (coordinator, ONCE, close-out).
Refuted flags UNSET. TWELVE defaults stay ON. Headless `RB3_HTTP=1 RB3_FIXED_CLOCK=1`, free
ports, frame-count settling, pgid-only cleanup. Build under `/tmp/rb3-native-build.lock` or own
worktree. Stage only your own files by path; NEVER `rb3_session_trace.cpp` / engine
`FxSendNative.cpp`. Hands family is CLOSED (no re-charter, no dead-cell re-attempt).

## Pre-dispatch checklist — the ten §4 lints

- [x] **1. Matrix-relative + pointer-verified** — FOREARM binding claims are pointer/dir
  identity; pose claims compare R-vs-L bone WORLDS (bilateral), not scalars.
- [x] **2. Split by population** — FOREARM per-member (player3 vs others); HUD per-screen;
  SWEEP per-screen/per-element.
- [x] **3. No unvalidated oracles** — E1 matched-frame visual is decisive; SWEEP is triage
  (no fix, no oracle needed); HUD confirm-then-fix re-validates the bug exists first.
- [x] **4. Shipped-flag contradiction grep** — SWEEP greps the twelve defaults to avoid
  re-reporting fixed bugs; no lane touches them.
- [x] **5. Grants** — FOREARM writes bandobj/char (+ maybe engine pose) + evidence; HUD
  writes band3 UI + evidence; SWEEP scripts + docs only.
- [x] **6. Option table before 2nd fix attempt** — FOREARM does binding-discriminator BEFORE
  pose (no blind second attempt); hands dead cells restated banned.
- [x] **7. Evidence committed** — matched-frame crops + gate logs.
- [x] **8. Flag hit-counts on negatives** — FOREARM binding-probe reports per-slot counts.
- [x] **9. Flavor-membership grep** — each fix lane step 0: verify edited TU compiles into
  rb3-native.
- [x] **10. Instruments before fixes** — FOREARM triage already exists (T2 M5); HUD confirms
  before fixing; SWEEP is pure instrument.

## Risks / open questions for the reviewer

- **R-A (FOREARM):** is the T2 M5 triage (pose-float, boneFallback=0) enough to skip the
  binding discriminator, or is binding-first still the right guard given the hands lesson that
  "pose-looking" bugs were sometimes binding? (I lean binding-first — cheap, and the hands
  saga burned waves assuming pose.)
- **R-B (FOREARM):** is FOREARM-FLOAT deterministic enough to E1-gate? T2 noted "non-
  deterministic band frame → triage on ONE frame." Does the fix lane need a fixed-clock
  matched-frame protocol (RB3_FIXED_CLOCK) to get a stable before/after, and is player3's
  right-arm pose reproducible under it?
- **R-C (HUD):** are the score-HUD/star-meter bugs still present on the current build, or
  should the lane be re-scoped? (confirm-then-fix handles this, but bless the pivot clause.)
- **R-D (SWEEP):** which retail screenshots are trustworthy ground truth for which native
  screen (resolution/aspect/UI-era mismatches)? Should the sweep weight the milohax
  `images/retail-screenshots/README.md` provenance notes?
- **R-E:** anything here that risks re-opening the hands closure or re-citing a banned cell
  (esp. if FOREARM's binding fix touches the RebindOutfitBonesToOwnSkeleton path the hands
  share)?
