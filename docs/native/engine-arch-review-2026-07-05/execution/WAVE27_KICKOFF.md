# WAVE 27 KICKOFF — CROWD ui/world panel-residency (the real crowd-walkers repair)

Status: DRAFT (pending Fable pre-dispatch review → COORDINATOR ACCEPTANCE)
Coordinator: Fable (this session). Lane agents: Opus.
Prior state: rb3 HEAD `95cd7601`+, engine pin `8d0e5b0`, census 408, FOURTEEN defaults ON,
drawlog-golden 792 PASS, rb3-tests 116/0.

## Why this wave

Three waves of narrowing (W23 recon → W25 partial → W26 refutation) leave exactly ONE
proven mechanism for the missing main_hub crowd walkers, with a symbolized backtrace:

> On the splash→main_hub transition, `UIManager::Poll → BandScreen::Enter →
> UIScreen::UnloadPanels → UIPanel::Unload → WorldDir::~WorldDir → … →
> CharClipSet::~CharClipSet → CharClip::~CharClip → Replace(clip,NULL) →
> CharDriver::Replace → DeleteClip → mFirst=NULL` (kill at beat 2.433, frame 72;
> `ui/UIScreen.cpp:570` area). The `streetslomo` crowd `CharClipSet` lives inside a
> panel `WorldDir` that native UNLOADS. `streetslomo_clips.milo` loads exactly once at
> boot and is NEVER reloaded, so the 8 crowd drivers stay `animating=0` forever and
> `RB3_CROWD_CLIP_KEEP` can never fire. (W26-CROWD/STATUS.md; probes FMERGE_PROBE +
> CHARDRV_BT; W25 merge/bank-swap theory REFUTED E1-E5.)

Per `WAVE26_CLOSEOUT_REVIEW.md` Q6: **ONE meaty lane** this wave. PROP is parked
(probe-only tail, only if the main lane lands early). No GLOW lane (S5 closed faithful).

## Lane W27-CROWD — charter

**Goal:** hub crowd walkers visibly walking in main_hub (`animating>0` on the 8
`streetslomo` crowd CharDrivers + 8 lit figures in isolate captures), via a fix in the
ui/world/vignette layer, flag-gated default-OFF.

### BINDING STEP 0 — Wii ground truth FIRST (no fix code before its checkpoint)

Answer with evidence, not a guess: **on real Wii, across the splash→main_hub
transition, is the `streetslomo` vignette panel KEPT RESIDENT (so its clip set never
tears down), or is it UNLOADED and then RELOADED (with `play_clip` re-fired) under
main_hub?**

Evidence sources (use several, they cross-check):
- **UI DTA config** (extracted assets): which panels do the splash screen vs the
  main_hub screen *reference*? `UIScreen::UnloadPanels` on Wii only unloads panels the
  incoming screen does not reference — so shared-reference vs reload is decidable from
  the screen/panel definitions (`ui/*.dta`, hub vignette DTA that fires the initial
  `play_clip` / `vignette_start.trig`). Also check the panel's `mAlwaysLoad`-class
  config.
- **Decomp source** (`src/system/ui/UIScreen.cpp` `Enter`/`UnloadPanels`,
  `src/system/ui/UIPanel.cpp` `Load/Unload/CheckUnload`, `PanelDir`): what retention
  semantics does the FAITHFUL code implement? Is native running the faithful path with
  divergent DATA (panel identity/paths differ → shared panel not recognized as shared),
  or is a native shim short-circuiting retention?
- **Ghidra bank8** (`bin/analyze-function`, e.g. `UnloadPanels__8UIScreenFP8UIScreen`,
  `CheckUnload__7UIPanelFv`) to confirm the decomp matches target where match% is low.
- **Native trace**: log panel names + referenced-set at the transition
  (`RB3_HTTP=1 RB3_FIXED_CLOCK=1`, env-gated probe) to see WHY native decides to unload
  the streetslomo panel — name mismatch? missing reference in the parsed screen config?
  splash screen object differing from Wii?

**Checkpoint the STEP 0 verdict to `/tmp/wave27-checkpoints/CROWD-step0.json` BEFORE
writing any fix code** (fields: verdict `resident|reload|native-divergence-in-X`,
evidence pointers, chosen lever + why).

### Levers (choose from STEP 0 evidence — implement exactly one as the primary)

- **A. Keep-resident:** make the streetslomo vignette panel survive the transition the
  way Wii does (fix the reference-set/identity divergence if that's the root, or
  honor `mAlwaysLoad`-class config natively). Preferred if STEP 0 says Wii keeps it.
- **B. Reload + re-fire:** reload `streetslomo_clips.milo` under main_hub and re-fire
  `play_clip` (`vignette_start.trig`) on the 8 crowd drivers after the transition
  settles. Preferred if STEP 0 says Wii reloads.
- If STEP 0 reveals a **native data/ordering divergence upstream** (e.g. the screen's
  panel list parsed wrong), fix THAT — smallest faithful fix wins over either lever.

### Flag + scope rails (binding)

- Fix lands **env-gated default-OFF**: `RB3_HUB_CROWD_RESIDENT` (name final unless
  review amends), `#ifdef HX_NATIVE`, byte-identical `#else`. **NO default flips, NO
  pin bumps by the lane** — coordinator only, at close-out.
- **EXCEPTION — faithful-restoration carve-out:** if STEP 0 proves the faithful Wii
  code path is being *broken by an existing native shim or parse bug*, fixing that
  divergence in-place (no flag) is allowed ONLY with the discriminator evidence
  checkpointed + a boot A/B showing no regression; say so explicitly in STATUS.
- Owned files: `src/system/ui/UIScreen.cpp`, `src/system/ui/UIPanel.cpp`,
  `src/system/ui/PanelDir.cpp` (+ narrowly `src/system/ui/UIManager.cpp` if the
  decision point is there), native-side probe files. **NOT owned / DO NOT TOUCH:**
  the protected gameplay WorldCrowd/RndMultiMesh oracle (`Crowd.cpp:884-1000`), the
  proven-correct RndMesh loader, `CharDriver.cpp`/`FileMerger.cpp` beyond reading,
  hands/finger family (CLOSED), `native/src/rb3_session_trace.cpp`, engine
  `src/synth/FxSendNative.cpp` (concurrent agents' files — never stage).
- Shared ui/world blast radius = the known MEDIUM-tractability risk. Mitigation is
  mandatory: flag-gated + **boot A/B flag-ON** (splash→main_hub fully loads; nav
  main_hub→song_select works; gameplay reachable via the standard headless harness) +
  **memory sanity** (the panel kept resident must not double-load or leak on a second
  visit — cycle main_hub→song_select→main_hub once and check).

### Gates (all must be in STATUS.md with evidence)

| Gate | Requirement |
|---|---|
| STEP 0 checkpoint | verdict + evidence BEFORE fix code (`/tmp/wave27-checkpoints/CROWD-step0.json`) |
| batch_objdiff | touched decomp fns: HX_NATIVE-gated → exact equality with baseline; any faithful-fidelity fix → match% ≥ baseline with `run_diff_inspect diagnose` justification; untouched → trivially baseline |
| drawlog-golden | `python3 scripts/native/drawlog-golden.py --fixed-clock --canonical-order` flag-OFF: 792 PASS |
| rb3-tests | 116 pass / 0 fail |
| boot A/B flag-ON | splash→hub→song_select→gameplay reachable, crash-free; hub revisit cycle no leak/double-load |
| **Acceptance** | flag-ON: crowd census `animating>0` on the 8 drivers + **8 lit figures** in isolate captures (`/api/screenshot`) |
| Near-black follow-on | if figures lit: run the deferred material discriminator (isolate max-pixel vs 17/255) and report; do NOT open a material fix lane |
| E-C2 tail | if the fix works, note in STATUS that `RB3_CROWD_CLIP_KEEP` (+ its E-C3 prune) is now removable — coordinator decides at close-out |

### Process rails (standing)

- Build only via `tools/ninja-locked` / cmake with `flock /tmp/rb3-native-build.lock`.
- Headless testing: `RB3_HTTP=1 RB3_FIXED_CLOCK=1`, free ports, pgid-only cleanup.
- Checkpoints: `/tmp/wave27-checkpoints/<stage>.json`, check-first / write-before-return.
- Commits under `flock /tmp/rb3-git.lock`; stage ONLY your own files BY PATH.
- Deliverables: `W27-CROWD/{PLAN.md,STATUS.md,evidence/}` + focused commit(s).
- Report back (StructuredOutput): paths + short findings, NO large diffs inline.

## Optional tail (only if W27-CROWD lands with capacity left): W27-PROP-PROBE

Probe-only, NO fix code: (a) bypass-test the E7 mFinger re-projection inference
(`CharIKHand::Poll` ~:319-330) — does disabling finger re-projection make the reach
clamp dormant with `RB3_PROP_POSE` ON? (b) enumerate whether any clip tracks exist for
the prop tip bones (`bone_pick_strum`, `bone_[RL]-tip_*`) in the band vignette clips.
Checkpoint verdicts; deliver `W27-PROP-PROBE/STATUS.md`. No flags, no defaults.

## COORDINATOR ACCEPTANCE

(to be filled after WAVE27_REVIEW.md)
