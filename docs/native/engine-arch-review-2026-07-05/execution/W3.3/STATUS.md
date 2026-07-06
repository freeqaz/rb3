# W3.3 (D.S2) — Grayscale venue at song start — STATUS

Item: Lane D, characterization-first. KEY=W3.3, checkpoint id D-S2. Opus.
Engine pin `8e7eddd` (== engine HEAD at run time). rb3 master.

## 2026-07-06 — S2 characterization + diagnosis — DONE (characterize + diagnose; NO fix landed, per file fence)

### Verdict
The "grayscale venue at song start" is a **native-only postproc-composite over-exposure
artifact triggered by the authored song-start stage-light REVEAL** — NOT an authored B&W
treatment, NOT the P4 grey-fallback misfiring, NOT an uninitialized tonemap. It is
**already documented in the engine** (`rb3_postproc.wgsl.inc`, the "Gameplay-entry
first-frame flash guard" comment): the Tier-2 composite runs the venue's song-start
lighting reveal **hotter than the Wii GX backdrop**, over-brightening the venue toward a
flat pink/white field; the existing per-channel Reinhard ceiling guard compresses the
clipped bright channels, and that **per-channel compression desaturates the hot pink
reveal into the observed grey/washed field**. It self-corrects to full color once the
reveal settles (~songMs 9000).

### Exact time window (songMs sweep, RB3_FIXED_CLOCK=1, default pub venue)
- ms200–1000: colored (pink-lit venue + song-intro card) — NO wash yet.
- ms2000: wash onset.
- **ms3000–4000: peak grey/desaturated wash + heavy authored intro smoke.**
- ms6000: clearing.
- **ms9000+: fully resolved to warm/colored venue** (matches retail gameplay look).
Evidence: `evidence/sheet_default.png` (8-frame contact sheet). The wash appears AFTER the
initial colored frames — i.e. it tracks the stage lights ramping/flashing UP (~2–6s), not a
"lights-not-on-yet" gap.

### Flag-isolation matrix (all at ms3000, same default pub venue) — the proof
| Config | venue-light | composite (pp) | ms3000 result |
|---|---|---|---|
| default | ON | ON | **grey/washed** (`evidence/A_default_ms3000_GREY.png`) |
| `RB3_PP_OFF=1` | ON | OFF | **pink/colored, no wash** (`evidence/B_ppoff_ms3000_COLOR.png`) |
| `RB3_VENUE_LIGHT_OFF=1` | OFF (flat white) | ON | **warm/colored, no wash** (`evidence/C_venuelightoff_ms3000_COLOR.png`) |

Reading: the wash needs BOTH the venue-light path's HOT reveal lighting AND the composite.
- Disabling the composite (`RB3_PP_OFF`) → venue stays colored through the reveal
  (matches the doc's "rendering DIRECT ... clipW 0/16 boots"). So the composite STRUCTURE
  causes the wash, not a grade term.
- Disabling the venue-light path (`RB3_VENUE_LIGHT_OFF`) → flat default lighting never
  reaches the composite clip ceiling → colored. So the hot reveal lighting is the input the
  composite over-brightens.
- Both ON (shipping default) → grey.

### Hypotheses adjudicated
- **(i) authored B&W camera/postproc treatment — REFUTED.** The active gameplay grade is
  `world.pp` with **saturation=0.0 (neutral)** the entire window (`RB3_RENDER_DBG` timeline:
  `evidence/default_postproc_timeline.txt`; last change `world.pp sat=0` at f1860, ms3000 is
  f2539). `B+W_film02.pp` (saturation −40) NEVER activates in gameplay — it is a
  song_select/etched-screen grade. The desaturation is NOT a saturation term: `RB3_PP_OFF`
  keeps color while the `world.pp` composite greys, so it is the composite STRUCTURE
  (over-exposure → per-channel ceiling clip), not a `mSaturation` grade.
- **(ii) P4 grey-fallback (`sVenueGreyKey`) misfiring before first environ set — REFUTED.**
  The environ IS set and produces COLORED (pink) lights — visible under `RB3_PP_OFF`. The
  `dl==0 && pl==0` grey-key branch (`Rnd_Wgpu_RB3.cpp:1422-1434`) is NOT the path (it would
  also show under `RB3_PP_OFF`, but that render is pink). The wash is downstream of lighting,
  in the composite.
- **(iii) postproc/tonemap uninitialized — REFUTED.** Postproc IS initialized (`world.pp`
  active). The artifact is over-exposure, not absence of a grade.

### Faithfulness
NOT "working as intended." The stage-light REVEAL itself is authored/faithful (retail flashes
the venue lights up at song start), but the **wash is a native artifact** — the composite runs
the reveal hotter than Wii and the per-channel ceiling guard's desaturating compression is only
a partial mitigation. The faithful look during the reveal is the COLORED one (our `RB3_PP_OFF`
and settled ms9000+ captures; retail gameplay screenshots in `images/retail-screenshots/` show
richly-colored moody venues). Ground-truth caveat: no retail/Dolphin capture exists at the exact
~3s reveal window, so faithfulness is argued from (a) the engine's own documented "hotter than
Wii GX" admission and (b) the direct-render / post-settle colored result, not a pixel A/B at 3s.

### Root-cause files & fix disposition (per Wave-6 FILE FENCE)
Root cause lives in engine render files: venue reveal exposure
(`Rnd_Wgpu_RB3.cpp:1380-1393`, `sVenueDirExposure`/`sVenuePointExposure` — **FORBIDDEN**
Lane-A file) and the composite ceiling guard (`rb3_postproc.wgsl.inc` fs_postproc +
`RB3PostProc.cpp` — engine render files inside Lane A's W3.1b cross-backend WGSL/composite
contract). **There is NO rb3 game-side / `native/src` lever.** Therefore **NO fix landed this
wave.** Delivered instead:
- **Staged patch** `staged-luma-preserving-ceiling.patch`: make the composite ceiling
  LUMINANCE-preserving (compress on luma, preserve chroma) so a hot reveal reads
  bright-but-COLORED instead of grey; identity below the knee (settled frames byte-identical).
  Recommended over the two alternatives (lower reveal exposure; skip composite when hot).
- **Backlog proposal** (below) for Wave 7, after Lane A settles the WGSL/composite files.

### Backlog proposal — W3.3-fix (Wave 7, engine, Lane-A-sequenced)
Apply `staged-luma-preserving-ceiling.patch` (or an engine-owner-preferred lever). Add
`--song-name` to `grayscale-sweep.py` to pin the QUICKPLAY pick (this wave's 3 configs each
drew a different QUICKPLAY default — all the same pub venue, so the composite-on/off variable is
still decisive, but a pinned A/B removes the confound). Verify: BEFORE grey at ms3000, AFTER
colored at ms3000 with ms9000+ byte-identical; fail-red = revert to per-channel ceiling → grey
returns. Gate: DC3 zero-blast (fs_postproc-only, no UniformStructs change), milo-engine-tests
198/0/2, lineup gate.

### Reproduce
```
cp native/build-native/rb3-native /tmp/rb3-native-w33   # HEAD-matching binary
python3 docs/native/engine-arch-review-2026-07-05/execution/W3.3/grayscale-sweep.py \
    --config default          --out /tmp/w33/default          # grey at ms3000
python3 .../grayscale-sweep.py --config pp_off           --out /tmp/w33/pp_off      # color
python3 .../grayscale-sweep.py --config venue_light_off  --out /tmp/w33/venue_light_off  # color
```
Harness note: headless free-runs the sim fast during Python `time.sleep` but paces to ~1
frame/HTTP-poll, so the song clock races to 20s+ during menu-confirm sleeps. The harness goes
strictly poll-paced from part_difficulty onward (no sleeps) to actually catch the 0–20s window.
`{game jump <ms>}` does NOT faithfully re-derive the venue-director state on a backward jump
(venue renders black) — do not use jump to reproduce this.

## 2026-07-06 — Wave 7 A.S1 (W3.3-fix) — IMPLEMENTED, flag-first, default-OFF — DONE

Implemented the staged `staged-luma-preserving-ceiling.patch` (comment-only proposal,
verified byte-exact against the `rb3_postproc.wgsl.inc:186-191` quote per WAVE7_REVIEW A3)
behind a new registered flag, **`RB3_PP_LUMA_CEILING`** (default-OFF; classification.json
entry landed, `class: workaround`, `owner: render/post`). Engine commit `7943bfa`
(`milo-native-engine`, pin unchanged at `1b045d9` — coordinator bumps `MILO_ENGINE_PIN`).

**What changed** (all 3 in the Lane-A WGSL/composite fence):
- `rb3_postproc.wgsl.inc`: the ceiling guard now branches on `pp.lumaCeilingActive`. ON
  compresses on luma and rescales RGB uniformly (hue/chroma survive); OFF is the untouched
  original per-channel `select(color, ppRolled, color > ceil)` — byte-for-byte identical text.
- `RB3PostProc.h` / `.cpp`: `PostProcUniforms` grown by one `vec4` group (160B → **176B**,
  multiple-of-16 for the WGSL uniform address space) and a new `RB3PPLumaCeilingActive()`
  cached-getenv accessor (same lazy-eval pattern as `RB3PostProcDisabled()`). Every consumer
  of the struct's size (`RB3Quad.cpp` bind-group `minBindingSize`, buffer `size`,
  `WriteBuffer`) reads `sizeof(PostProcUniforms)` dynamically, so **no edit needed there** —
  stayed inside the 3-file Lane-A fence.

**Flag-OFF byte-identical:**
- `drawlog-golden.py --fixed-clock --canonical-order`: **792 PASS**, 4/4 runs (the "72
  unexpected divergence" FAIL seen on the very first run was the pre-existing `field=world`
  jitter residual — reproduces identically on the unmodified reference binary, 3/3 runs,
  confirmed NOT introduced by this change).
- Screenshot A/A: the codebase's documented boot-model limitation (W1.6 STATUS: "the
  literal screenshot-md5 form ... is not achievable") holds here too — live-gameplay
  captures at a matched `songMs` land on different absolute frame counts run-to-run
  (menu-nav + asset-load are wall-clock-paced), so raw pixel AE floors at ~85-95% even
  binary-vs-itself. Used the codebase's frame-pinned alternative instead (`RB3_GAME_INPUT`
  scripted nav + `RB3_FIXED_CLOCK`, same technique as `_framepin_capture.py`): pinning both
  builds to the **same absolute frame** reproduces the same camera/venue/character pose
  (only note-highway/score differ, which is autohit-timing noise, not a scene change).
  flag-OFF (old ref binary) vs flag-OFF (this build) cross-diff (80.7-86.3% AE) sits inside
  the same-binary A/A noise band (86.7-86.8% AE) — no new divergence attributable to the
  struct growth.

**Flag-ON gate (frame-pinned, grey window):** pinned all 3 configs (default / `RB3_PP_OFF` /
`RB3_PP_LUMA_CEILING`) to the identical frame (2351, songMs ~2700-3250, inside the diagnosed
2000-6000 grey window) — confirmed same camera/venue/pose across all 3 (SMOOVE-poster stage
shot). Sampled a venue-wall crop (HSV, avoiding HUD/highway):

| config | hue | sat | val |
|---|---|---|---|
| default (flag OFF) | 185.4 (meaningless) | 0.005 (grey) | 0.105 |
| `RB3_PP_OFF` (control, composite disabled) | 324.9 | 0.416 | 0.215 |
| `RB3_PP_LUMA_CEILING=1` (flag ON) | **324.5** | 0.232 | **0.427** |

Hue matches the `RB3_PP_OFF` control within 0.4°, sat clearly recovers from grey (0.005 →
0.232), and `val` stays *brighter* than the composite-disabled control (0.427 vs 0.215) —
the composite's exposure ramp is kept, not flattened to the `RB3_PP_OFF` baseline or to a
flat clip. Visual confirmation: default renders the stage-lit pub venue as flat B&W;
`RB3_PP_LUMA_CEILING=1` renders the same frame in the same pink/magenta stage-light hue as
the `RB3_PP_OFF` control, just brighter/glowier (composite still active).

**Fail-red:** temporarily reintroduced the per-channel branch *inside* the flag-ON path
(same file, code-local perturbation) → grey reproduced at the same pinned frame (sat 0.015,
hue 150.6 — meaningless), confirming the gate is sensitive to the actual fix mechanism, not
an artifact of the frame/config selection. Perturbation reverted before commit (`git diff`
against engine HEAD shows exactly the intended 3-file change, no residue).

**Not done / left to the coordinator:** the default flip (this task is explicitly
flag-first — do not flip). `grayscale-sweep.py`'s `CONFIG_ENV` gained a `luma_ceiling` entry
(committed alongside, rb3-side) for the S2/S3 broad-coverage passes.

Checkpoint: `/tmp/wave7-checkpoints/A-S1.json`.

### Artifacts (this dir)
- `grayscale-sweep.py` — songMs-sweep + flag-isolation harness (now includes `luma_ceiling`).
- `staged-luma-preserving-ceiling.patch` — staged fix proposal (implemented 2026-07-06).
- `evidence/` — 3 contact sheets, 4 keyframes (A/B/C/D), postproc timeline.
