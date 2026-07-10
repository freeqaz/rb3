# WAVE 27 — Pre-Dispatch Review (Fable)

**Scope reviewed:** `WAVE27_KICKOFF.md` (draft, `6dc950ec`) against W26-CROWD/STATUS.md,
WAVE26_CLOSEOUT_REVIEW.md Q6, execution/README.md W25/W26 rows, the actual ui/band3 decomp
source at HEAD, and the extracted DTA/milo assets. Read-only probes only; no source edits.

## VERDICT: **ACCEPT-WITH-AMENDMENTS (A1–A9)**

The wave shape is right (one meaty lane, STEP-0-first, flag-gated, PROP as probe-only tail).
But the kickoff's model of the faithful retention mechanism is **wrong in a load-bearing way**
(A1), the Wii ground truth it asks the lane to establish is **already half-decidable from
on-disk data — and the answer routes through the band3 interstitial system the kickoff never
mentions and the owned-files grant does not cover** (A2, A3). Dispatching as written has a
high probability of a W26-style out-of-grant stall plus a STEP-0 spent re-deriving what this
review already pinned.

---

## Evidence baseline (verified this review, reviewer-level)

1. **Screen panel lists (extracted Xbox DTA, plaintext, readable in-workspace):**
   - `splash_screen (panels meta sv8_panel splash_panel)` — `orig-assets/extracted/ui/splash/splash.dta:171`
   - `main_hub_screen (panels meta sv3_panel main_hub_panel accomplishments_status_panel)` — `orig-assets/extracted/ui/main/main_hub.dta:744`
   - `song_select_screen (panels meta postsong_sfx_panel sv4_panel song_select_panel ...)` — `orig-assets/extracted/ui/song_select/song_select.dta:1934`
   - The splash and hub **backdrop panels are DISJOINT** (sv8 vs sv3); the only shared regular
     panel is `meta`.
2. **streetslomo lives ONLY in sv3_a**: raw-string scan of the shell vignette milos finds
   `streetslomo` in `sv3_a.milo_wii` and `sv3_a.milo_xbox` and in NO other sv milo
   (sv3_b/sv7_a/sv8_a/sv2_a/sv5_a negative). There is no standalone `streetslomo_clips.milo`
   on disk — it is a nested subdir inside the sv3_a container.
3. **splash prefetches sv3 as an INTERSTITIAL**: `orig-assets/extracted/config/vignettes.dta:46,126-129`
   — `(interstitials ... (splash_screen (dummy (TRUE ("sv3")))))`. Interstitials are loaded as
   BandScreen `mExtraPanels` via `BandScreen::LoadInterstitials` →
   `InterstitialMgr::GetInterstitialsFromScreen` (`src/band3/meta_band/InterstitialMgr.cpp:38-67`)
   → `CheckLoad()` on `sv3_panel` (`src/band3/meta_band/BandScreen.cpp:64-73`), and released by
   `BandScreen::Exit` → `UnloadInterstitials()` → `CheckUnload()` (`BandScreen.cpp:20-24,75-77`).
   **This is why the hub crowd is loaded and animating during splash at all.**
4. **Faithful retention is a REFCOUNT HANDSHAKE, not a reference-set check** (see A1):
   `UIPanel::CheckLoad/CheckUnload` (`src/system/ui/UIPanel.cpp:47-50,52-59`),
   `IsReferenced() { return mLoadRefs != 0; }` (`src/system/ui/UIPanel.h:63`).
   Order on a goto: `UIManager::GotoScreenImpl` (`src/system/ui/UI.cpp:686-689`) →
   outgoing `Exit(to)` calls **`to->LoadPanels()` FIRST** (`src/system/ui/UIScreen.cpp:508`)
   → (BandScreen) `UnloadInterstitials()` (`BandScreen.cpp:23`) → … later, at transition
   completion, `UIManager::Poll` (`UI.cpp:569-583`) → incoming `Enter(from)` calls
   `from->UnloadPanels()` **LAST** (`UIScreen.cpp:414`).
5. **Wii ground truth per the data** (subject to lane confirmation, A2): across splash→main_hub
   the sv3 panel is **KEPT RESIDENT** by refcount overlap — splash's interstitial holds ref 1;
   main_hub's `LoadPanels` takes ref 2 (sv3_panel is in its regular list, `mAlwaysLoad`
   defaults TRUE, `UIScreen.cpp:47,54`); splash's `UnloadInterstitials` drops to 1; splash's
   `UnloadPanels` never touches sv3_panel (not in splash's list — it unloads sv8_panel +
   splash_panel). Across **main_hub→song_select the sv3 panel IS unloaded** (song_select uses
   sv4_panel) and re-loaded on return with a re-evaluated randomized `dyn_file`
   (`new_shell_vignette` BackdropPanel, `orig-assets/extracted/ui/vignettes.dta:59-130`;
   campaign-gated variant lists in `config/vignettes.dta:1-45`).
6. **Wii-side DTA on disk is serialized only**: `orig-assets/wii-extracted/ui/splash/gen/splash.dtb`,
   `config/gen/vignettes.dtb` (no plaintext ui/*.dta in the Wii extraction). The plaintext DTAs
   above are the Xbox extraction — which is what native actually runs, so they are the right
   primary source; byte-level Wii cross-check needs a dtb→dta step (see A4).

---

## Amendments

**A1 (TOP — correct the retention-semantics model in "Evidence sources").**
The kickoff states "`UIScreen::UnloadPanels` on Wii only unloads panels the incoming screen
does not reference — so shared-reference vs reload is decidable from the screen/panel
definitions." That is NOT the mechanism. `UnloadPanels` (`UIScreen.cpp:570-576`) unconditionally
`CheckUnload()`s every entry it loaded (`it->mLoaded`); there is no incoming-screen check
anywhere inside it. Retention emerges from the **refcount handshake ordering** (evidence item 4):
incoming `LoadPanels` increments `mLoadRefs` on shared panel OBJECTS before the outgoing side
decrements, so shared panels never reach 0. Consequence for the lane: STEP 0 must probe
**per-panel `mLoadRefs` transitions and CALL ORDER** (Exit→LoadPanels vs UnloadInterstitials vs
Enter→UnloadPanels), not diff "reference sets". A native divergence in *ordering* (e.g. a boot
path through `GotoScreenImpl`'s `mCurrentScreen==NULL` branch `UI.cpp:688-689`, or
`CancelTransition`'s `oldScreen->UnloadPanels()` `UI.cpp:653-658`) breaks retention with
perfectly parsed data. Rewrite the second evidence bullet accordingly.

**A2 (TOP — reframe STEP 0: the resident-vs-reload binary is already answered by data; the
open question is the native WHY).** Per evidence items 1-5, on Wii the answer is "**resident
across splash→hub via the INTERSTITIAL prefetch + refcount overlap; unloaded across
hub→song_select and re-loaded on return**". The kickoff's binary ("kept resident OR
unloaded+reloaded under main_hub") misses the actual mechanism (interstitial → regular-panel
refcount handoff) and so mis-aims the levers. Amend STEP 0 to: (i) record the above as the
data-derived Wii-GT hypothesis (with these file:line anchors) and spend the Wii-GT budget only
on CONFIRMING it (dtb cross-check and/or Dolphin spot-check, timeboxed); (ii) make the binding
native question: **which panel/WorldDir owns the streetslomo CharClipSet natively at boot, and
where does its refcount hit 0** — candidate divergence sites, all in code read this review:
`BandScreen::LoadPanels`' `kTransitionTo && PushDepth()==0` gate + `MILO_ASSERT
TransitionScreen()==this` (`BandScreen.cpp:55-58`) skipping the interstitial/panel handoff;
`TheBandUI.mShowVignettes` forcing `mAlwaysLoad=false` on vignette panels
(`BandScreen.cpp:47-53`); `InterstitialMgr` map/`mRandomSelection` resolution
(`InterstitialMgr.cpp:43-63`); splash's parsed panel list containing sv3_panel natively (would
explain the kill living under `UnloadPanels` rather than `UnloadInterstitials`); and
backtrace symbolization ambiguity between `UnloadPanels` and `UnloadInterstitials` (both are
thin `CheckUnload` loops — one probe line naming the panel + refcount at `CheckUnload` resolves
it in minutes). Keep the checkpoint file requirement; extend its `verdict` enum with
`interstitial-handshake-divergence`.

**A3 (TOP — owned-files grant must include the band3 interstitial surface).** The retention
mechanism spans `src/band3/meta_band/BandScreen.cpp`, `src/band3/meta_band/InterstitialMgr.cpp`
and (narrowly) `src/band3/meta_band/BandUI.cpp` (`mShowVignettes`, `mInterstitialMgr`) — none
of which the kickoff grants. If the divergence is in the interstitial handoff (the single most
likely site per A2), the lane as chartered stalls out-of-grant exactly like W26 did. Add all
three (BandUI.cpp read-mostly/narrow), alongside the existing UIScreen/UIPanel/PanelDir
(+UIManager narrowly). Keep the existing NOT-owned list unchanged.

**A4 (Q1 — evidence-source practicality).** State in the kickoff where the evidence actually
lives: plaintext screen/panel DTA = `orig-assets/extracted/ui/**.dta` (Xbox extraction — the
data native runs; primary source); Wii-side equivalents are **.dtb only**
(`orig-assets/wii-extracted/ui/splash/gen/splash.dtb`, `config/gen/vignettes.dtb`) — a
byte-level Wii cross-check needs dtb decoding tooling or should be explicitly waived as
"Xbox-dta-as-proxy, platforms assumed script-identical"; and `streetslomo_clips` is a NESTED
subdir inside `sv3_a.milo` (no standalone file on disk), so clip-set ownership questions are
answered by a native runtime dump (`/api/dta/eval`) or milo tooling, not by ls/grep. This
converts STEP 0's "which panels do the screens reference" from a research task into a
15-minute confirmation.

**A5 (Q3 — lever framing).** Lever A as written ("keep the panel resident … honor
mAlwaysLoad-class config") risks overshooting: `mAlwaysLoad` already defaults TRUE
(`UIScreen.cpp:47`) and Wii does NOT keep sv3_panel resident across hub→song_select (evidence
item 5) — a blanket keep-resident flag would diverge there (memory + drawlog). Reword lever A
to "**restore the faithful interstitial→panel refcount handshake** across splash→hub
specifically". For lever B (reload+re-fire): note the BackdropPanel's `dyn_file` is a
randomized, campaign-gated pick (`vignettes.dta:59-130`, `config/vignettes.dta:1-45`) — a
re-load must re-evaluate `dyn_file`, not hardcode `sv3_a.milo` (at default campaign level the
list is `("sv3_a")` so behavior is deterministic today, but hardcoding would silently break at
higher campaign levels). The third option (fix the upstream native data/ordering divergence)
is correctly present and, per A2, is now the FAVORITE — the kickoff should say so.

**A6 (Q8 — carve-out vs drawlog-golden tension needs a defined resolution).** The
faithful-restoration carve-out (no-flag fix) and the "drawlog-golden flag-OFF: 792 PASS" gate
contradict each other: an unflagged faithful fix makes 8 crowd walkers animate at boot BY
DEFAULT, which can legitimately change the flag-OFF baseline (W26 already logged world-field
crowd-pose divergences as inherent jitter). Amend the gate: if the carve-out fires, the
requirement becomes "drawlog-golden re-run; PASS with unchanged draw COUNT and only
world-field crowd-pose value diffs, OR a coordinator-approved re-baseline BEFORE landing" —
explicit coordinator sign-off either way. Otherwise the carve-out is well-gated (discriminator
checkpoint + boot A/B) and should be kept.

**A7 (Q5 — hub revisit gate is drivable; keep, with two sharpenings).** The
main_hub→song_select→main_hub cycle is realistic in the current harness: `/api/input` supports
`up/down/confirm/cancel/...` (`native/src/rb3_game_input.cpp:17`), the hub→song_select nav
pattern exists (`scripts/native/song-select-capture.py`), and `/api/dta/eval` `{ui goto_screen
main_hub_screen}` is a deterministic fallback for the return leg. Sharpen the memory-sanity
check: (i) assert `sv3_panel`'s `mLoadRefs` returns to its pre-cycle value (no monotonic
growth), not just "no double-load/leak"; (ii) note the return leg re-picks `dyn_file`
(deterministic `sv3_a` at default campaign level) so the crowd census must be re-run AFTER the
revisit too — a residency fix that survives one transition but not the reload path would
otherwise pass.

**A8 (Q6 — flag name: no collision; one contingency).** Repo + engine
`NativeCompatFlags.classification.json` scanned: nearest existing names are
`RB3_CROWD_CLIP_KEEP`, `RB3_CROWD_DIM_OFF`, `RB3_NO_CROWD_INTRO`, `RB3_HUB_TICKER_YFIX`,
`RB3_HUB_MENU_QUAD_OFF` — no near-collision of the HANDS_BINDFIX/HANDS_BIND_FIX class.
`RB3_HUB_CROWD_RESIDENT` is clear TO USE **if lever A wins**. If STEP 0 selects lever B
(reload+re-fire), the name is semantically wrong; permit the lane to substitute
`RB3_HUB_CROWD_REFIRE` at the STEP-0 checkpoint (one name, chosen once, recorded in the
checkpoint — no mid-lane renames).

**A9 (Q7 — PROP probe tail: KEEP, tightened).** Part (a) (mFinger-bypass A/B) is the exact
E7 verification the close-out chartered — keep, 30-minute timebox, checkpoint-first. Part (b)
("enumerate whether any clip tracks exist for the prop tip bones") is clip-data spelunking with
rabbit-hole risk — keep only with an explicit timebox (≤ half day) and a "negative result is a
valid checkpoint" note. Optionally fold in the two 1-line W26 close-out nits (env-parse
`RB3_PROP_POSE=""` enables, CharIKHand.cpp:55; multi-target weight-loop comment) as probe-tail
cleanups — they are already ruled, mechanical, and inside CharIKHand.cpp.

**A10 (Q8 — minor, add one rail).** The lane's owned files host the HX_NATIVE screen-prewarm
system (`UIScreen.cpp:157-380`: `PrewarmEnabled` web-default-ON / native-opt-in,
`DoPrewarmNextScreen`, the `RB3PrewarmIssuedLoader` adoption bridge into `UIPanel::Load`).
Default next-map is only `main_hub_screen:song_select_screen`, so it is NOT the boot-kill
suspect, but any refcount/lifecycle change interacts with prewarm-loader adoption and the WEB
build exercises it by default. Add one cheap rail: run the boot A/B once with
`RB3_PREWARM_SCREENS=1` natively (approximates web) and don't regress the adoption/evict
invariants documented at `UIScreen.cpp:225-262`.

### Non-amendments (checked, fine as written)

- Near-black material follow-on: correctly report-only ("do NOT open a material fix lane") —
  not premature. Keep.
- E-C2 tail (RB3_CROWD_CLIP_KEEP removal note): correct, coordinator-gated. Keep.
- batch_objdiff / rb3-tests / process rails / checkpoint discipline: standard and right-sized.
- The 14 default-ON flags: this lane ships default-OFF (or carve-out per A6); no interaction
  beyond A6/A10. Census bump is coordinator-side as stated.
- Context-anchor quotes from W26 (backtrace, nclips 11→8, FMERGE refutation, KEEP-can't-fire):
  quoted correctly against W26-CROWD/STATUS.md.

## Dispatch readiness

**READY AFTER AMENDMENTS** — specifically A1+A2+A3 are blocking (mechanism model, STEP-0
reframe, band3 grant); A4-A10 are wording/gate tightenings the coordinator can fold in while
editing. With those applied, the lane starts with the Wii-GT hypothesis pre-pinned and its
entire STEP-0 budget aimed at the native divergence site, and it cannot stall out-of-grant on
the interstitial surface. Expected outcome class shifts from "lever A vs lever B" toward
"upstream native ordering/handshake fix under the carve-out" — which is why A6's drawlog
resolution must be settled BEFORE dispatch, not discovered at gate time.
