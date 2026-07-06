# W2.1-flip-blocker.S4 — Dolphin gameplay A/B sign-off package (fresh, Wave 6)

**Package produced by this stage (A.S4, Sonnet packager). This is a package only — the agent did
NOT flip `RB3_PLACEMENT_CONTRACT`'s default and did NOT touch the drawlog goldens.** Coordinator
human-eyes sign-off on this package is the trigger for the flip commit (per WAVE6_KICKOFF
COORDINATOR ACCEPTANCE / WAVE6_REVIEW A4).

This supersedes `../../W2.1-flip/dolphin-ab/` (the Wave-5 package the coordinator HELD on). It
carries the Wave-6 upgrades the hold required: the wash is now characterized (A.S1/A.S2/A.S3
below and in `../STATUS.md`), not eyeballed.

## What changed since the Wave-5 hold

The Wave-5 hold reason: `cap_ON_1` blew out nearly-white while the other three captures rendered
normally (1/2 flag-ON, 0/2 flag-OFF) — an asymmetry the package's own checklist called "a new
finding, not expected."

**A.S1/A.S2 (this item, Wave 6) directly refuted the asymmetry under a songMs-pinned, continuously
scored, statistically analyzed protocol** (`scripts/native/wash_score.py` + `wash-measure.py`, full
detail in `../STATUS.md` §A.S1/§A.S2):

- **VERDICT: A/A-variable.** Two independent flag-**OFF** captures are PINK wash-class (existence
  proof the wash occurs without the flag); the flag-OFF vs flag-ON luma distributions are
  statistically indistinguishable (Mann-Whitney **U=24.0, p=1.0**, n=7/7); at ~equal songMs
  (≈21131) the wash class **flips PINK↔NEARBLACK across boots of the SAME flag state**, so it is a
  run-to-run render-state variable, not flag-caused and not songMs-determined.
- The Wave-5-hold's prime suspect (crowd-emissive feeding the `game.cam`-gated bloom-halo capture)
  is **mechanically excluded** (`Rnd_Wgpu_RB3.cpp:4403` `strcmp(cam,"game.cam")` guard; these are
  venue-cam shots) — confirmed by WAVE6_REVIEW A3's source read and re-confirmed here.
- **A.S3 took the A/A-variable branch: no engine/code fix.** Per WAVE6_REVIEW A4, an A/A-variable
  verdict *unblocks* the flip under two conditions, both satisfied by this package: (1) E1 is
  judged on **detector-selected wash-free captures** (below), (2) the wash is carried as its own
  **standalone backlog item** (`../STATUS.md` §"Backlog proposal — WASH", not this package's
  problem to fix).

## Numeric oracle companion (flag-ON, `--gate both`)

```
$ python3 scripts/native/placement-gate-capture.py \
      --bin native/build-agent-W2.1-flip-blocker/rb3-native \
      --tests native/build-agent-W2.1-flip-blocker/rb3-tests \
      --gate both   # (RB3_PLACEMENT_CONTRACT=1)
exit code: 0   (PASS — both crowd + drum oracle GREEN)
```

Full stdout in [`oracle-gate-ON.log`](oracle-gate-ON.log); raw drawlog+probe artifacts in
[`oracle-capture/`](oracle-capture/). Re-run fresh this stage (own lane binary, engine pin
`8e7eddd`) — both `PlacementOracle.RealCaptureSpansBowl` and `PlacementOracle.RealCaptureDrumPlaced`
PASS.

## Captures — 7 per flag state, songMs-pinned (window 20877–21163), every one scored

All 14 raw captures from `../STATUS.md` §A.S2's songMs-pinned interleaved-sequential measurement
run are committed here (not just the ≥4/state minimum) — this is the full dataset the verdict was
computed on, so the reviewer can inspect every input, not a curated subset. Engine logs alongside
each as `cap_<name>.engine.log` (caveat: `cap_OFF_01_21108` and `cap_OFF_01_21131` share one source
log — the harness's try-index counter collided for that pair; both PNGs and their scores are
independently correct, only the debug log attribution for that one pair is ambiguous between the
two; see `../STATUS.md` for the authoritative per-capture score table this README reproduces).

Scored by `scripts/native/wash_score.py` (`score_image()`), same detector as `../STATUS.md` §A.S2:

| capture | flag | songMs | mean_luma | hi% (blowout) | lo% (near-black) | pink% | class |
|---|---|---|---|---|---|---|---|
| `cap_OFF_01_21108.png` | OFF | 21108 | 0.095 | 1.7 | 80.5 | 0.0 | NEARBLACK |
| `cap_OFF_01_21131.png` | OFF | 21131 | **0.229** | 2.0 | 41.9 | **33.6** | **PINK** |
| `cap_OFF_02_21149.png` | OFF | 21149 | 0.093 | 1.5 | 80.6 | 0.0 | NEARBLACK |
| `cap_OFF_03_21163.png` | OFF | 21163 | 0.093 | 1.5 | 80.4 | 0.0 | NEARBLACK |
| `cap_OFF_04_21071.png` | OFF | 21071 | 0.097 | 1.8 | 80.5 | 0.0 | NEARBLACK |
| `cap_OFF_05_21019.png` | OFF | 21019 | 0.103 | 1.9 | 76.3 | 0.0 | NEARBLACK |
| `cap_OFF_06_21083.png` | OFF | 21083 | **0.570** | 4.3 | 6.6 | **70.0** | **PINK** |
| `cap_ON_01_21025.png`  | ON  | 21025 | 0.262 | 2.1 | 42.3 | 36.2 | PINK |
| `cap_ON_01_21108.png`  | ON  | 21108 | 0.090 | 1.7 | 81.4 | 0.0 | NEARBLACK |
| `cap_ON_02_20877.png`  | ON  | 20877 | **0.590** | 11.5 | 6.8 | 37.9 | **PINK** |
| `cap_ON_03_21131.png`  | ON  | 21131 | 0.088 | 1.6 | 81.5 | 0.0 | NEARBLACK |
| `cap_ON_04_21132.png`  | ON  | 21132 | 0.251 | 2.2 | 42.4 | 33.9 | PINK |
| `cap_ON_05_21113.png`  | ON  | 21113 | 0.437 | 2.6 | 5.8 | 64.6 | PINK |
| `cap_ON_06_21116.png`  | ON  | 21116 | 0.090 | 1.7 | 81.4 | 0.0 | NEARBLACK |

**flag-OFF wash rate 2/7 (PINK); flag-ON wash rate 4/7 (PINK).** Fisher exact p≈0.59 (NS);
Mann-Whitney on the 14 luma values **U=24.0, p=1.0** (no flag effect on brightness). Raw JSON:
`../measure/batch_log.json` (this table) + `../measure/verdict.json` (the compare() verdict this
package's oracle/note relies on) + `../measure/montage.png` (S2's own wash-class-first strip).

Note on class distribution: at this songMs window every capture is either PINK or NEARBLACK —
none scored NEUTRAL or WHITE. NEARBLACK is treated as wash-free per the detector's design (see
`wash_score.py` docstring: "NEARBLACK can also be a legitimately dark venue shot ... reported but
is NOT counted as a wash for the existence-proof") — visually confirmed below (the NEARBLACK
captures used in the layout montages show a normally-lit, dim pub-venue gameplay frame with the
band clearly visible, not a rendering failure).

## Layout vs ground truth — built from DETECTOR-SELECTED wash-free captures only

Per WAVE6_REVIEW A4 condition (1): E1 must be judged on wash-free captures so the wash (already
proven flip-independent) cannot re-confound the sign-off the way it did in Wave 5. The montages
below use only the **NEARBLACK-classified (non-PINK/non-WHITE) "wash-free"** captures — 2 per flag
state, chosen for songMs diversity:

- flag-OFF: `cap_OFF_01_21108.png` (songMs 21108), `cap_OFF_04_21071.png` (songMs 21071)
- flag-ON: `cap_ON_01_21108.png` (songMs 21108, same songMs as its OFF pair), `cap_ON_03_21131.png` (songMs 21131)

The wash-affected (PINK) captures are deliberately **excluded from the visual layout** — they are
still fully present, scored, and linked above for anyone auditing the wash itself, but judging
placement on a washed frame is exactly the confound the Wave-5 hold got stuck on.

- [`layout_crowd_vs_dolphin.png`](layout_crowd_vs_dolphin.png) — the 4 wash-free captures side by
  side against `gp_00.png` (Dolphin, crowd spread house-left).
- [`layout_drum_vs_retail.png`](layout_drum_vs_retail.png) — the same 4 wash-free captures against
  `fandom_gameplay_drums.png` (retail, drum-kit position).
- Ground-truth reference images copied alongside: `gp_00.png`, `fandom_gameplay_drums.png`.

(Ground-truth note, unchanged from Wave 5: `gp_00.png`/`fandom_gameplay_drums.png` are generic
crowd-spread / drum-kit-position references from a different venue/shot than our captured pub
venue — they establish what *correct* crowd distribution and drum placement look like in general,
not a pixel-exact same-venue overlay. Same convention as the Wave-5 package.)

## Reviewer checklist (E1 sign-off)

1. Open `layout_crowd_vs_dolphin.png`: does the crowd (visible left/right of the band, house-left
   in the pub venue) look spread rather than piled at one point, in BOTH the flag-OFF and flag-ON
   wash-free frames? (The numeric crowd oracle above already proves this exactly — this is the
   human-judged corroboration.)
2. Open `layout_drum_vs_retail.png`: does the drum kit / band sit at a plausible position (not
   floating at the venue origin/center) in both wash-free frames? (Numeric drum oracle above
   already proves this exactly.)
3. Confirm the two wash-free flag-OFF and two wash-free flag-ON frames look like normal, similarly
   dim gameplay renders of the same venue/shot — i.e. nothing new or flag-attributable is visible
   once the wash is filtered out. (This is the point of detector-selection: there should be nothing
   left to eyeball here.)
4. The wash itself (PINK broken-env cast on 2/7 OFF and 4/7 ON captures, see table above) is
   **not a flip blocker** — it is proven A/A-variable (statistically indistinguishable across flag
   states, existence-proofed in flag-OFF) and is carried forward as its own backlog item
   (`../STATUS.md` §"Backlog proposal — WASH"). Do not re-litigate it here; if something in the
   *wash-free* frames looks wrong, that is a new finding.
5. If satisfied: coordinator flips `kPlacementContractDefaultOn` 0→1 (the one-line change staged by
   `W2.1-flip.S1`) and re-goldens the drawlog goldens.

## A5 pre-flip checklist (from `../STATUS.md` §A.S3, WAVE6_REVIEW A5 — coordinator-owned actions)

Recorded here for the coordinator's flip review cycle (none of these are applied by this package):

1. **Expected re-golden count: 792.** Re-measured (not assumed) this wave against the lane's own
   clean binary under the current deterministic order (post W0.3d-fix):

   | flag state | env | measured `drawlog-golden.py --canonical-order` count | runs |
   |---|---|---|---|
   | flag-ON | `RB3_PLACEMENT_CONTRACT=1` | **792** | 3/3 identical |
   | flag-OFF (control) | (no env) | 888 | 1/1 (== committed golden) |

   Coordinator re-golden target: `splash_screen.json` 888 → **792** + fresh per-name-eps residual
   sidecar.

2. **OFF-arm semantics inversion — files to sweep in the SAME review cycle as the flip commit**
   (post-flip, "no env" = contract-**ON**; the OFF arm must become `RB3_PLACEMENT_CONTRACT_OFF=1`):

   | File | OFF-arm site | Action needed post-flip |
   |---|---|---|
   | `scripts/native/w21flip-dolphin-ab.py` | `("OFF", 1/2, {})` :186-187 | OFF arm → `{"RB3_PLACEMENT_CONTRACT_OFF":"1"}` |
   | `scripts/native/w21flip-ui-ab.py` | `("OFF", 1/2, {})` :170-171 | OFF arm → `{"RB3_PLACEMENT_CONTRACT_OFF":"1"}` |
   | `scripts/native/wash-measure.py` | `STATES = [("OFF", {}), ...]` :259 | OFF arm → `{"RB3_PLACEMENT_CONTRACT_OFF":"1"}` |

   Non-inverting but noted: `crowd-bone-gate-capture.py:84` already forces the contract ON
   (harmless post-flip); `_w32-boxambient-ab.py:68`'s "OFF" baseline silently gains contract-ON on
   both arms post-flip (a different flag under test — sibling W3.2 lane should be aware).
   Recommended durable fix: give the two `w21flip-*.py` scripts an explicit `--flag-state
   {on,off}` mapped to the post-flip envs.

3. **`classification.json` row updates (drafted, not applied)** —
   `../../../../../../../milo-native-engine/src/platform/NativeCompatFlags.classification.json`
   rows :91-92:
   - `RB3_PLACEMENT_CONTRACT` (:91): `"default": "off"` → `"default": "on"`; faithfulStatus →
     `"live: SYS-1 skinned-placement contract (obj.world=meshWorld + bind-relative palette),
     default-ON as of Wave 6 flip; opt out via RB3_PLACEMENT_CONTRACT_OFF"`.
   - `RB3_PLACEMENT_CONTRACT_OFF` (:92): `"default"` stays `"off"`; faithfulStatus → `"...Now the
     live opt-out: the contract is default-ON as of the Wave 6 flip; setting this disables it
     (takes precedence over the opt-in)."`
   - Also update `docs/native/engine-arch-review-2026-07-05/NATIVE_COMPAT_LEDGER.md` :228-229 to
     match (regenerated by the census, but the source rows above drive it).

**This package's exit is "package produced + oracle GREEN companion recorded + wash
characterized" — never "flip done."**
