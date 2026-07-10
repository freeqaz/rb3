# WAVE 28 KICKOFF — CROWD clip-set OWNERSHIP (name the owner, then ONE lever)

Status: DRAFT (pending Fable pre-dispatch review → COORDINATOR ACCEPTANCE)
Coordinator: Fable (this session). Lane agents: Opus.
Prior state: rb3 HEAD `a47d82f0`+, engine pin `be401ec`, census 410, FOURTEEN defaults ON,
drawlog-golden 792 PASS (per-name eps recalibrated `_w27_residual_recalib.py`, fail-red
audit OK), rb3-tests 116/0.

## Why this wave

Wave 27 settled the UI question and reopened the ownership question:

- **PROVEN (W27, stands):** `sv3_panel` is RESIDENT across splash→main_hub — the
  interstitial→regular-panel refcount handshake (`mLoadRefs` 1→2→1) is faithful; the
  A7 revisit cycle is leak-free. There is NO ui/panel-residency lever.
- **REINSTATED (close-out E1, raw logs):** seven `CHARDRV_REPLACE` kills of
  `crowd1-5.clp/clip` fire at beat 2.433 — the W26
  `WorldDir::~WorldDir → CharClipSet dtor → Replace(clip,NULL)` mechanism reproduces —
  sourced at the **FAITHFUL splash-side panel unload** (`splash_panel`+`sv8_panel`
  refs→0 in the kill frame, E2).
- **OWNER CORRECTED (E2):** the crowd clips / walker proxies / walk meshes are
  raw-string-present in `sv8_a.milo` (the splash backdrop), NOT in sv3_a's raw strings.
  W26 mis-attributed the destroyed clip set to sv3/streetslomo.
- **Root cause reframed (E4):** clip-set ownership/binding divergence — there are TWO
  same-named `clips` sets (11 vs 8 entries); do the resident streetslomo drivers
  resolve `play_clip` against the splash-owned copy that faithfully dies?
- **E3:** only 7 of 8 drivers ever Play (`crowd_female04` never triggered).
  **E5:** `mDefaultClip` is serialized-only — NULL may be faithful; log the serialized
  name before treating it as a gap.

This is the FOURTH narrative correction on this bug (W23 verts=0 → W25 load-merge →
W26 sv3-unload → W27 sv8-owner). Per `WAVE27_CLOSEOUT_REVIEW.md` Q6 the wave is
**discriminator-first with checkpoint-before-fix BINDING**: NO fix code, of any kind,
before the STEP-0 checkpoint file exists with a verdict.

## Lane W28-CROWD-OWNER — charter

**Goal:** answer, with named objects and evidence, WHO owns the clip set the hub crowd
drivers actually resolve against and WHOSE crowd we have been measuring — then apply
exactly ONE lever (or re-charter, if the answer says the bug was mis-scoped).

### BINDING STEP 0 — ownership + identity discriminators (≤1 day; probes exist)

All four items; checkpoint to `/tmp/wave28-checkpoints/CROWD-step0.json` BEFORE any
fix code (fields: owner-verdict, identity-verdict, per-item evidence pointers, chosen
lever + why).

1. **Name the torn-down owner directly.** One boot with
   `RB3_CROWD_PANEL_DBG=1 CHARDRV_PROBE=crowd CHARDRV_BT=1` (probes already exist:
   `CharDriver.cpp` ~:245/:273/:533/:848-870; panel probe from W27). Interleave the
   beat-2.433 Replace BACKTRACE with the panel-unload markers and read off the
   torn-down `WorldDir`'s actual name/owner — no inference from raw-string greps.
2. **Dump the ownership chains.** For (a) the 5 clips that Play, (b) BOTH `clips`
   sets (11 vs 8 — and whether any driver's `mClips` pointer SWAPS at the kill), and
   (c) the 8 `crowd_*` char dirs: dump `Dir()` parent chains up to the owning panel
   `WorldDir` (native runtime dump via `/api/dta/eval` or an env-gated probe — the
   milo files nest; ls/grep on raw strings is NOT ownership evidence, that's what
   burned W26).
3. **E5 discriminator.** Log each crowd CharDriver's SERIALIZED default-clip name at
   load time (before any Replace) — decide whether `mDefaultClip==NULL` post-kill is
   a native gap or faithful data.
4. **Wii-GT identity check (the charter-scoping one).** Are the `crowd_*` chars part
   of retail main_hub at all, or are they streetslomo's OWN (differently-named)
   walkers — i.e. is the crowd we have measured since W23 actually the SPLASH crowd
   that faithfully dies with the splash? Evidence: retail screenshots
   (`images/retail-screenshots/`), Xbox DTA / milo dir listings for main_hub's
   vignette, and the sv8-vs-sv3 dir dumps from item 2.

### Levers (exactly ONE, chosen from the STEP-0 verdict)

- **A. Cross-panel clip resolution fix:** if the hub's resident streetslomo drivers
  are wrongly bound to the splash-owned `clips` copy, fix `play_clip`/driver clip
  resolution so they resolve the RESIDENT copies. This is the faithful-restoration
  carve-out class (unflagged allowed ONLY with checkpointed discriminator evidence +
  the A6-style drawlog ruling: unchanged draw count, only world-field crowd-pose
  diffs; anything else → flag-gated default-OFF fallback).
- **B. Re-charter:** if the observed crowd is splash-owned and faithfully dies, the
  hub-walkers bug was mis-scoped since W23 — write the re-charter (what ARE
  streetslomo's own walkers, what state are they in natively, fold in the deferred
  `verts=0` / near-black thread) and STOP. A documented re-charter is a full lane
  success; do not force a fix onto the wrong crowd.

### Flag + scope rails (binding)

- Lever A flag name: `RB3_HUB_CROWD_REBIND` (final unless review amends), chosen ONCE
  at the STEP-0 checkpoint; `#ifdef HX_NATIVE`, byte-identical `#else`; default-OFF
  unless the A6 carve-out fires with countersign-ready evidence. **NO default flips,
  NO pin bumps by the lane** — coordinator only, at close-out.
- Owned files: `src/system/char/CharDriver.cpp` (probes exist; narrow writes allowed
  for lever A resolution only), `src/system/char/CharClip*.cpp` (read + narrow),
  `src/system/ui/UIPanel.cpp`/`PanelDir.cpp` (read-mostly; W27 proved these faithful),
  `src/band3/meta_band/{BandScreen,InterstitialMgr}.cpp` (read), native probe files.
  **NOT owned / DO NOT TOUCH:** the protected gameplay WorldCrowd/RndMultiMesh oracle
  (`Crowd.cpp:884-1000`), the proven-correct RndMesh loader, hands/finger family
  (CLOSED), FOREARM binding (CLOSED), `native/src/rb3_session_trace.cpp`, engine
  `src/platform/FxSendNative.cpp` (concurrent agents' files — never stage).
- `RB3_CROWD_CLIP_KEEP` (E-C2) stays IN PLACE during the lane; removal is re-ruled by
  the coordinator at close-out only.

### Gates (all in STATUS.md with evidence)

| Gate | Requirement |
|---|---|
| STEP 0 checkpoint | ALL FOUR discriminators + verdicts BEFORE fix code (`/tmp/wave28-checkpoints/CROWD-step0.json`) |
| batch_objdiff | touched decomp fns: HX_NATIVE-gated → exact equality with baseline; faithful-fidelity fix → ≥ baseline with `run_diff_inspect diagnose`; untouched → trivially baseline |
| drawlog-golden | `python3 scripts/native/drawlog-golden.py --fixed-clock --canonical-order` flag-OFF: 792 PASS (recalibrated residual is current — do NOT touch the sidecar; ambient-RED reruns escalate to coordinator) |
| rb3-tests | 116 pass / 0 fail |
| boot A/B flag-ON (lever A only) | splash→hub→song_select→gameplay reachable, crash-free; A7 revisit cycle: `mLoadRefs` returns to pre-cycle + crowd census re-passes AFTER revisit |
| Acceptance (lever A) | flag-ON: `animating>0` on the crowd drivers + lit walking figures in isolate captures (`/api/screenshot`) |
| Acceptance (lever B) | committed re-charter doc naming the real hub walkers + their native state, with the same evidence rigor as STEP 0 |
| Evidence honesty | STATUS excerpts must cite RAW log paths + line ranges; coordinator E1 greps RAW logs (W27 E1 lesson — curated excerpts are not evidence) |

### Process rails (standing)

- Build only via `tools/ninja-locked` / cmake with `flock /tmp/rb3-native-build.lock`.
- Headless: `RB3_HTTP=1 RB3_FIXED_CLOCK=1`, free ports, pgid-only cleanup (never pkill by name).
- Checkpoints: `/tmp/wave28-checkpoints/<stage>.json`, check-first / write-before-return.
- Commits under `flock /tmp/rb3-git.lock`; stage ONLY your own files BY PATH.
- Deliverables: `W28-CROWD-OWNER/{PLAN.md,STATUS.md,evidence/}` + focused commit(s).
- Report back (StructuredOutput): paths + short findings, NO large diffs inline.

## Optional tail: W28-PROP-FIX (concurrent, disjoint files; defer without guilt)

The real prop-hand fix, mechanism fully pinned by W26/W27: (1) bind/animate the
prop-tip clip tracks (`bone_pick_strum`, `bone_[LR]-tip_*` carry constant LocalXfm —
W27(b)); (2) redirect the target BEFORE the multi-target weight loop (W27 comment nit
marks the spot, `CharIKHand.cpp` ~:271); (3) break the mFinger re-projection feedback
(collapse target proven: `RB3_PROP_FINGER_BYPASS` takes over-reach 120-240u → 21-25u,
reach 20.3u). Flag-gated default-OFF (`RB3_PROP_POSE_FULL`, name final unless review
amends), building on the existing `RB3_PROP_POSE` scaffold. Owned:
`src/system/char/CharIKHand.cpp` + narrowly the clip-binding site the tip-track fix
requires (enumerate in PLAN before touching). Acceptance: with flag ON in-song, hands
track the strum/fret positions with clamp mode counts ~0 skip (matching the B column
of W27's A/B) AND visible hand-on-instrument in captures. Same gates table as the main
lane (batch_objdiff / drawlog flag-OFF / rb3-tests / boot A/B flag-ON).
