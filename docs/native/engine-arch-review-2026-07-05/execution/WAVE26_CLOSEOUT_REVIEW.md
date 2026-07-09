# Wave 26 — Close-out Review (Fable)

**Scope reviewed:** W26-CROWD (`5b7aabc5`+`a94a189a`), W26-PROP (`1498c400`), W26-GLOW (`7b9068fc`),
cross-checked against WAVE26_REVIEW.md A1–A9, code at HEAD, and evidence files. No visible fix shipped;
all three lanes are recon/no-fix/handoff outcomes. Verdicts below; ERRATA are append-only.

---

## Q1 — CROWD root cause: is the panel-unload backtrace genuinely the kill? **YES — ACCEPT**

- The symbolized chain (`evidence/step0-backtrace-rootcause.txt`) is coherent and specific:
  `UIManager::Poll` (UI.cpp:583) → `UIScreen::Enter` → `UIScreen::UnloadPanels` (`src/system/ui/UIScreen.cpp:570`,
  loop body ~:575) → `UIPanel::CheckUnload`/`Unload` → `WorldDir::~WorldDir` (world/Dir.cpp:64/70) →
  `CharClipSet::~CharClipSet` → `CharClip::~CharClip` (CharClip.cpp:194) → `~Object` (Object.cpp:131) →
  `CharDriver::Replace` (CharDriver.cpp:851), for `from='crowd4.clp'` at beat 2.433 / frame 72 — the exact
  W25 kill signature. (A few addr2line frames near the bottom — `BandUI::Init`, `App::App:206` — are
  inlining/symbolization noise; the load-bearing UI→WorldDir→ClipSet segment is clean.)
- A1 refutation is clean and double-keyed: FMERGE_PROBE shows all 182 boot FileMerger events are the
  `char/main/main.milo` wardrobe mergers, **zero** crowd/streetslomo/sv3, last merge 237 log lines
  (~19 frames) before the kill (`evidence/step0-fmerge-discriminator.txt`); the backtrace independently
  names a non-FileMerger deleter. Corroborated by bank state: drivers KEEP `clips` (`clipsName='clips'`),
  nclips 11→8, `streetslomo_clips.milo` loads exactly once (8 boot events, never reloaded), and the
  flag-ON retest stays `animating=0` (`evidence/census-flag-ab.txt`).
- **Supersession: YES.** The W25 mechanism ("async load-merge destroys the playing clip AND swaps
  `mClips` to a player-only sub-bank") is corrected: no merge occurred, and `mClips` was never swapped —
  W25's `nCrowd=0` observation was the SAME bank object minus its 5 deleted crowd clips, misread as a
  bank swap. W25's timing (plays ~1.2s, dies at beat 2.433/frame 72) and its census/probe kit survive;
  only the mechanism attribution and the "async-interleave, same class as hands" analogy fall. See ERRATA E1–E5.
- **Handoff charter: RIGHT, with one sharpening.** "Keep the streetslomo vignette panel resident OR
  reload+re-fire `play_clip`" is the correct lever set and correctly outside the lane's A8 grant
  (FileMerger.cpp+CharDriver.cpp). Tractability: MEDIUM, recon-first — the charter's missing STEP 0 is
  *why native diverges*: `UIScreen::Enter/UnloadPanels` is faithful decomp, so on Wii the unload likely
  fires too, and the hub crowd walks either because main_hub's own panel set reloads
  `streetslomo_clips.milo` (native's dir-sharing/dedup may suppress that second load) or because a hub
  vignette re-fires the walk. W27 must first diff the splash-vs-main_hub panel lists + establish the Wii
  ground truth (Dolphin file-access or panel-resident check) before choosing resident-vs-reload.

## Q2 — the "unconditional crash-fix": **ATTRIBUTION WRONG — it is NOT a Wave-26 change** (ruling: KEEP)

- `git blame src/system/char/CharDriver.cpp:94-105` → commit `65892986` (2026-05-27, "native:
  boot-to-song milestone"), six weeks before this wave. The W26 diff (`git show 5b7aabc5`) shows those
  lines as unchanged **context**; the lane's commit message and STATUS.md honestly claim only the
  CHARDRV_BT probe, the comment correction, and the E-C3 prune. The coordinator's close-out premise
  ("the lane also landed an unconditional dtor fix") is a mis-read of the diff hunk → ERRATA E6. No
  W26 record may credit this wave with it.
- **Scrutiny of the guard itself (since it was questioned): real-class and correct.**
  - Bug shape is real: `mBones` is `ObjPtr<CharBonesObject>` (CharDriver.h:85), `mInternalBones` a raw
    owned `CharBonesAlloc*` (:108, deleted in the dtor body at :116). If aliased (external
    `SetBones(...)`/blend wiring — CharDriver.cpp:301, :934), C++ destroys members AFTER the body, so
    `~ObjPtr` (ObjPtr_p.h:26-42) calls `mPtr->Release(this)` on freed memory — a genuine UAF; with
    virtual-base `CharBonesObject` the vbase-offset read SIGSEGVs on native.
  - The fix only acts when the alias holds (`mBones.Ptr() == mInternalBones`): it runs `Release` while
    the object is alive, then deletes. Non-alias path: condition false, behavior byte-for-byte identical —
    a legitimately-aliased mBones that needed the Release GETS the Release (just earlier, on live memory).
    No correct-path change exists.
  - Wii: whole block `#ifdef HX_NATIVE` → Wii object byte-identical (batch_objdiff baselines confirm).
  - Belt-and-suspenders: the generic `HxAddrWasFreed` guards in `~ObjPtr`/`Replace` (ObjPtr_p.h:28-59,
    which cite this exact CharDriver::mBones case) already backstop it; the dtor guard fixes it at source.
  - **Ruling: KEEP UNCONDITIONAL (HX_NATIVE).** Crash-fixes on the native-only teardown path with a
    provable no-op on the non-alias path should not be env-flag-gated; gating would just re-expose a
    known SIGSEGV. No revert, no gate. (Campaign norm of flag-gating applies to *behavior* changes, not
    memory-safety guards with identical correct-path semantics — consistent with the existing ObjPtr guards.)
- The actual W26 dtor change — the **E-C3 `gCrowdKeep` prune** (CharDriver.cpp:106-114) — is correct
  (erases this driver's map entry, killing the heap-address-reuse stale-snapshot alias when the flag is
  ON) and a no-op while default-OFF. ACCEPT.

## Q3 — PROP verdict + disposition: **ACCEPT-WITH-ERRATA (keep in-tree, default-OFF; W27 item is DEEP)**

- **Clip-binding verdict: sound.** The discriminator table (STATUS, `evidence/step0-ikprop.log`) is the
  right shape: `IK_ROOTCMP same=1` refutes attach/proxy (a); the tip's static LocalXfm
  (`bone_pick_strum` |local|=51.3 vs reach 20.3) with a correctly-posed `bone_target_strum` parent at
  d=18.9 is the clip-binding-fling signature; the mic family is correctly separated as whole-chain
  displaced (A6, out of scope). Redirect code verified at HEAD: `sPropPoseRedirect`
  (CharIKHand.cpp:49-88) with the A5-i guard (`dTip <= r2 || dPar >= dTip` → no redirect, :78-79),
  applied at both target-read sites (:259 single, :302 multi). Wii byte-identical (HX_NATIVE; G3 case-1
  gates pass).
- **mFinger explanation: plausible, consistent, but NOT directly tested.** The math is real
  (Poll :320-330: dest re-projected by `mHand·mFinger⁻¹`; if the finger/pick frame carries the same
  unbound ~50u offset, it re-introduces the error post-redirect), and the evidence is consistent
  (`mworlddst-ON-vs-OFF.txt`: fret med 213→121, left_hand 165→60, but strum 199.5→203.8 and drum-R
  128.6→135.7 — unchanged-to-slightly-worse). But no finger-bypass A/B was run, so "mFinger feedback"
  is inferred, not proven → ERRATA E7 + a 30-minute W27 verification step (temporarily skip the mFinger
  re-projection when the redirect fired; if strum's dst collapses, the mechanism is proven).
- **Disposition: keep default-OFF in-tree.** Correct call: it is the pinned, re-runnable discriminator
  (IK_PROP_DBG/RB3_PROP_POSE_DBG) plus the correct first half of the eventual fix; it is provably inert
  by default and bounded ON (clamp stays the net). Reverting would delete the instrumentation W27 needs.
  Two code nits for W27 (non-blocking): (i) env-parse `e && e[0] != '0'` makes `RB3_PROP_POSE=""` enable
  (CharIKHand.cpp:55; use the `e && e[0] && e[0]!='0'` pattern per rb3_render_hook.cpp:310); (ii) in the
  multi-target path the WEIGHT loop (:277-291) still uses the un-redirected tip's LocalXfm — fine for a
  discriminator, wrong for a real fix.
- **W27 charter actionable but DEEP:** "bind the prop-bone clip tracks natively" is the same
  parse-time-binding class as HANDS (which took W20-21 to a terminal L2-b/mitten outcome), and the
  visible payoff is small — the clamp already produces the correct clip pose in-song (A5-iii: ANATBEAT
  identical ON/OFF). Rank low (Q6).

## Q4 — GLOW no-fix: **ACCEPT — "already faithful" is honest, not a give-up**

- I independently reviewed `evidence/ring_montage_GT_OFF_ON.png`: the default (OFF) 4x ring shows the
  strong radiating glow, structurally matching GT; ON is visually identical at the ring — confirming the
  ring is HUD-driven. Residual: GT's burst reads slightly bluer/deeper than native's whiter burst, but
  that is within venue-lighting/capture variance and not actionable as S5.
- The 3-element decomposition checks out at HEAD: (1) combo ring = `StreakMeter::SetPeakState()` →
  `mPeakStateTrig` (`src/system/bandobj/StreakMeter.cpp:144`, trig found at :33) — HUD/UI cam, untouched
  by the halo flag; (2) SP peakstate overlay = engine `RB3MaterialBinder.cpp:602` (`kHighwayPeakstate`,
  always-on); (3) now-bar plate = `gem_smasher_glow` exclusion at `native/src/rb3_render_hook.cpp:301-315`
  (`QueryHaloPolicy`; the kickoff's ":285-309" anchor drifted a few lines — the material classifier at
  :285-288 plus the policy at :301-315). Keeping RB3_SMASHER_HALO OFF is right: ON adds only the
  documented over-bloom step with zero ring improvement.
- Capture legit: driven ≥4x via natural autohit (direct `set_multiplier` correctly rejected as
  meter-only), 2x negative control captured, checkpointed before any fix code. S5's original "plain"
  read is convincingly explained as a below-4x capture-state artifact. **S5: CLOSE as faithful.**

## Q5 — discriminator discipline: **WORKING**

Three lanes checkpointed STEP-0 discriminators before writing fix code; two refuted their charter
hypotheses and one refuted the proposed lever — and consequently ZERO speculative behavior changes
shipped. That is the discipline doing its job, not lanes giving up:
- **CROWD:** the A8 grant was NOT too tight — the kill site is `ui/UIScreen`+`UIPanel`+`world/Dir`,
  shared high-blast-radius files that other lanes' gates don't cover; a drive-by "minimal" panel-keep
  hack from a char-lane would have been exactly the speculative class this campaign forbids. Handoff
  correct. Minor miss: the lane could have run one read-only sharpener (diff splash vs main_hub panel
  lists) to pre-answer the W27 STEP-0 — noted in the Q1 charter amendment.
- **PROP:** breaking the mFinger feedback WAS in-lane (CharIKHand.cpp) and a probe-level bypass test was
  cheap — the one place a lane stopped a step earlier than it could have (ERRATA E7). But shipping a
  feedback-bypass as a *fix* would have been a workaround-on-workaround with no visible benefit
  (A5-iii identical), so the disposition itself is right.
- **GLOW:** collapsing to a flag decision + a capture-state root cause is the cheapest correct outcome.

## Q6 — Wave-27 slate (ranked)

1. **(a) CROWD ui/world panel-residency/re-fire — DISPATCH (HIGH EV, MEDIUM tractability, recon-first).**
   The only visible-symptom repair on the table (frozen/undriven hub crowd, 8 walkers). Deterministic
   repro, backtrace-pinned kill site, charter written. STEP 0 (binding): establish the Wii-side ground
   truth — does main_hub reload `streetslomo_clips.milo` (then native's fix = reload+re-fire) or keep
   the panel resident (`mAlwaysLoad`/referenced — then fix = residency)? Timebox the panel-lifecycle
   recon; the rabbit-hole risk is panel refcount spelunking without the Wii ground truth, which the
   STEP 0 removes. Gate: crowd census `animating>0` + drawlog-792 + the W25 protected-oracle rules.
2. **(d) crowd near-black material — attach as (a)'s acceptance follow-on** (unchanged W25 ruling; it
   only becomes observable once `animating>0`). Not a standalone lane.
3. **(b) PROP clip-track binding + mFinger — PARK/defer.** Deep (HANDS-class parse-time binding), small
   visible payoff (clamp already yields the correct pose). If capacity exists, do ONLY the 30-min
   mFinger-bypass verification probe (E7) to pin the mechanism for a future wave; do not charter the
   binding work while (a) is open.
4. **(c) GLOW residuals — NONE.** S5 closed as faithful; the minor burst-tint delta is below the
   actionability bar. No lane.
   Also fold into (a)'s close-out: E-C2 — remove `RB3_CROWD_CLIP_KEEP` + its scaffolding once (a) lands
   (its removal criterion is then met), and the E7/E-nit cleanups above.

## Q7 — flags census + hygiene

New this wave (all default-inert, HX_NATIVE): **probes** — `CHARDRV_BT` (backtrace at Replace),
`FMERGE_PROBE` (FileMerger append/notify trace), `IK_PROP_DBG` (target/parent dump),
`RB3_PROP_DST_DBG` (post-mFinger dst), `RB3_PROP_POSE_DBG` (redirect log); **workaround flag** —
`RB3_PROP_POSE` (default-OFF scaffolding, removal tied to the W27 PROP item). Default-ON count
unchanged (14). Forbidden-file check: the four commits touch only
`CharDriver.cpp`/`FileMerger.cpp`/`CharIKHand.cpp`/docs/evidence/harness scripts —
`native/src/rb3_session_trace.cpp` and engine `FxSendNative.cpp` appear in NONE (verified via
`git show --stat` on all four). PASS.

## ERRATA (append-only; exact wording)

- **E1 → `execution/W25-CROWD/STATUS.md`** (append): "W26 SUPERSESSION (`5b7aabc5`): the beat-2.433 kill
  is a UI PANEL-UNLOAD teardown (UIScreen::UnloadPanels → UIPanel::Unload → WorldDir::~WorldDir →
  CharClipSet::~CharClipSet), NOT an async FileMerger load-merge; FMERGE_PROBE shows zero crowd/streetslomo
  FileMerger events. `mClips` is NEVER swapped: the drivers keep the same `clips` bank and nclips drops
  11→8 (the deleted crowd clips) — the W25 'player-only sub-bank swap' was a misread of the same bank
  minus its crowd clips. Timing, census, probes and the re-arm scaffolding stand."
- **E2 → `execution/WAVE25_CLOSEOUT_REVIEW.md`** (append): "W26 supersession: the 'destroyed by the
  merge' mechanism (:33), the bank-swap endorsement, and the Q7 'why does the second load-merge fire'
  amendment (:37, :45) are superseded per W26-CROWD STEP-0 — no second merge exists; the kill is a
  splash→main_hub panel unload. The 'same async-interleave class as hands' analogy is withdrawn."
- **E3 → `execution/WAVE26_KICKOFF.md` + WAVE26_REVIEW.md A1/A2** (append note): "A1's
  `Merger::Clear` mechanism and A2's post-merge re-fire hook were dispatch-time hypotheses; STEP-0
  refuted them (panel-unload). Retained for the record; do not re-derive from them."
- **E4 → commit `b6a8980f` message** (immutable): superseded by `5b7aabc5`; this file is the correction
  of record.
- **E5 → coordinator memory** (`project_engine_arch_review_2026_07_05.md` + MEMORY.md index line):
  replace "crowd load-merge fix handed off" / "async load-merge @beat2.4 DESTROYS it + swaps mClips"
  with "crowd kill = splash→main_hub UI panel-unload tearing down the streetslomo WorldDir
  (backtrace-proven, FileMerger refuted); fix = W27 ui/world panel-residency-or-reload lane."
- **E6 → this wave's close-out premise:** the CharDriver::~CharDriver mBones-alias UAF guard
  (CharDriver.cpp:94-105) was landed by `65892986` (2026-05-27), NOT by W26; W26 added only the E-C3
  gCrowdKeep prune (:106-114). No W26 record may claim the crash-fix; ruling on the pre-existing guard:
  real bug, correct, keep unconditional under HX_NATIVE (see Q2).
- **E7 → `execution/W26-PROP/STATUS.md`** (append): "The mFinger-feedback explanation for strum/drum-R
  is inferred (math + PROP_DST consistency: strum med 199.5→203.8 ON), not directly tested; W27
  verification = a gated mFinger-bypass probe when the redirect fired. Also: RB3_PROP_POSE='' enables
  (parse nit, CharIKHand.cpp:55) and the multi-target weight loop uses the un-redirected tip LocalXfm."

## FINAL VERDICT

**ACCEPT-WITH-ERRATA (E1–E7).** A model recon wave: three checkpointed discriminators, three honest
verdicts, zero speculative behavior changes, Wii byte-identical throughout, gates green, no
forbidden-file staging. The one shipped behavior change this wave is the E-C3 prune (correct, inert
by default); the mBones UAF guard is pre-existing and stays unconditional. **W27 = one meaty lane:
CROWD panel-residency/re-fire (Wii-ground-truth STEP 0 first), with the near-black-material
discriminator as its acceptance follow-on; PROP parked (verification probe only); GLOW/S5 closed.**
