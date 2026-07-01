# verify-char-render — wave-3 independent verification (composed build)

Wave-3 verifier, 2026-06-11. Ports 8801-8804. Read-only: ran the already-built
merged binary `native/build-native/rb3-native` (built 06-11 21:00, rb3 master
`c011c886`, engine pin `469c550`). Evidence: `/tmp/rp3-char-render/`.
Baseline comparators: `/tmp/rp2-char-render/BEFORE-*` (wave-2's broken-state captures).

**VERDICT: PARTIAL — as predicted by the implementer, and confirmed adversarially
on the composed build.** The wave-2 reload-re-entrancy fix is genuinely active and
holds up under composition (4 independent sessions, 112 burst frames, random
band/outfits each run). The baseline's headline horrors — floating eyes/teeth with
no head, naked radiating shard fans — did not occur once. What remains is the
own==bound garment class (C8 rotation-basis divergence): per run, **20-24 band
garment meshes are guard-hidden or smeared**, which a player sees as
partially-dressed / partially-missing band members, strobing clothing, and
occasional full-screen pale smears.

---

## 1. Sessions run

```bash
# n=3 standard + 1 dense-burst, all on the composed binary, probes armed:
SHARD_DBG=1 RELOAD_PROBE=1 HEAD_REBIND_PROBE=1 REBIND_DRAW_SKINPOS=1 REBIND_DRAW_FLING=1 \
python3 scripts/native/keyboard-to-gameplay.py --port 880N --diff hard \
    --out /tmp/rp3-char-render/rN --game-burst 24 --verbose          # r1-r3 (8801-8803)
# r4 (8804): --game-burst 40 --burst-interval 0.25  (consecutive-frame flicker/animation)
```

All 4 reached gameplay; engine logs `/tmp/rb3-kbd2game-880{1..4}.log`;
contact sheets `/tmp/rp3-char-render/r{1,2}-contact.png`, zoom montages
`r{1,2,3,4}-zoom*.png`.

## 2. What is FIXED (verified on the composed build)

| check | result | evidence |
|---|---|---|
| Floating-eyes-only frames (baseline class) | **0 in 112 burst frames, 4 runs** | every visible character has head/hair/face; cf broken `/tmp/rp2-char-render/BEFORE-burst_07.png` |
| Naked radiating shard-fan explosions (baseline class) | **0 in 112 frames** | no irregular spike-fan frames; the recurring white pyramid clusters are the authored KISS-armor/nailboots studs, attached to a coherent figure (`r4/burst_29.png`) |
| Heads/hair/faces/hands/bare limbs render + persist | PASS | `r1/06_game_screen.png` (coherent guitarist + guitar, red intro), `r2/burst_11.png` (afro + raised arm), `r4/burst_16.png`→`burst_17.png` (singer + hair, pose changes between 0.25s frames = animating) |
| Reloads no longer corrupt rebind state | PASS 4/4 | `[STARTLOAD]` ×7-8/member incl. mid-song `Wardrobe::StartClipLoads` ×2 (poll≈20032): `restPose=105-113` preserved on EVERY line, never wiped to 0 (baseline wiped at frame ~13) |
| SetDeformation storm | PASS | 32 `[SETDEFORM]` events total per run (8/member, all `restCaptured=1`) vs baseline every-1-2-frames storm; `[REST_SEED] SKIPPED (clip playing)` poison guard observed firing |
| Fling | **FLING=0 on 4/4 runs**, non-vacuous (SKINPOS armed) | worst `REBIND_DRAW_SKINPOS` delta 92.2u (`sleevelesstee`/`bikinichain` foreTwist bones; clean bar <65u, fling = hundreds) — matches impl doc's 92.5u |
| Rebind latch health | as designed | `pending=1-7` own==bound meshes/member at the 600-poll give-up latch; `reboundBones=0` post-latch (no rescan churn) |

## 3. What a player STILL SEES WRONG (the C8 acceptance bar)

### 3a. SHARD_DBG census — band (non-crowd/extras) guard drops

Steady-state, last 1000 frames per run (drop EVENTS; each hidden mesh logs ~2/frame
across the two draw passes, so ÷2 ≈ hidden mesh-draws/frame):

| run | drop events /1000f | ≈ hidden meshes/frame | distinct band meshes affected | hidden 100% of frames | intermittent (flicker) |
|---|---|---|---|---|---|
| r1 (8801) | 30,480 | ~15 | 21 | 12 | 9 |
| r2 (8802) | 41,244 | ~20 | 24 | 14 | 10 |
| r3 (8803) | 31,044 | ~15 | 23 | 11 | 12 |
| r4 (8804) | 33,851 | ~17 | — | — | — |

Unchanged from the impl doc's 32-35/frame residual (and from the pre-fix baseline
volume — the fix moved meshes from *wrongly-broken* to *correctly-hidden*, it did
not unhide them). Crowd+extras drops additionally ≈40k/run (sibling `crowd` issue).

### 3b. Stable invisibility — partially-dressed / partial-body members

The 100%-dropped class is whole garment pieces: tops (`sleevelesstee`, `rompertop`,
`parkajacket`, `suspenders`, `kissarmor` pieces), bottoms (`romperbottom`,
`rolledjeans`, `hotpants`, `bondagepants`), footwear (`saddleshoe`,
`timberlandboots`, `modavengerboots`, `eightholedocs`, `femaledestroyedchucks`),
gloves (`drivinggloves`) and `fingernails_resource` (every run). Because there is
no body mesh under a torso garment, a hidden top = a missing torso, not a naked one:

- `r1/burst_18.png` — **bare legs + black skirt at the mic, NOTHING above the
  waist** (the romper member: `rompertop_skin.1` + `romperbottom_*` hidden
  1000/1000 frames).
- `r2/burst_14.png` — disembodied legs in patterned leggings, no upper body.
- `r2/burst_11.png`, `r2/burst_21.png` — **disembodied white glove** floating at
  the fret position / mid-air (`drivinggloves_*` member: hands render, everything
  connecting them doesn't). Per-member attribution (r2): player3 had hotpants +
  socks + suspenders + gloves + shoes ALL hidden = essentially the entire outfit.
- Per member: 2-4 pieces missing is typical; worst case the whole outfit.

### 3c. Flicker — the marginal (ratio≈2.0) class strobes

Meshes dropped on 5-95% of frames pop in/out at 60fps = visible strobing:
r1 `jumpsuitshorts_skin.2` 17%, `bondagepants_resource.1` 29%,
`sleevelesstee_resource` 69%; r2 `flannelcoat_skin.2` 13%, `wrestlingboots` 73%,
`loudleggings` 9%; r3 `escapeartist_resource` 38%, `jeansripped_resource` 36-80%,
`loudleggings` 14-32%, and notably **`chainsaw_strings.mesh` 66% — the guitar's
strings strobe**, i.e. the C8 blast radius includes instrument meshes, not just
clothing.

### 3d. Sub-threshold smears drawn RAW (arguably the ugliest residual)

Garments whose blended extent stays under the 2.0× guard ratio draw smeared
instead of hidden:

- `r4/burst_04.png` — **giant pale-pink tubes sweeping the full frame** (singer
  closeup cam; also `r4/burst_03/05`, `r1/burst_01.png` red variant).
- `r3/burst_10.png`, `r3/burst_11.png` — flat pale slabs crossing the upper frame.
- `r4/burst_29.png` — flat dark silhouette blob floating over the highway.
- `r4/burst_16/17.png` — singer's pink hair/garment mass renders as an oversized
  smear hovering over the highway (animates, so it's live geometry).
- Full-frame texture washes, camera apparently inside a smeared mesh or pressed
  into one: `r1/burst_04.png`, `r3/burst_18..21` (**4 consecutive seconds**),
  `r4/burst_08/09.png`. (Read-only run — could not probe to fully adjudicate
  smear-vs-venue-wall for the wash class; the slab/tube class is unambiguous.)

Rough player-impact rate: **~20-40% of venue-camera shots contain a noticeable
artifact** (r1 ≈ 4-6/24 frames, r3 ≈ 10/24, r4 ≈ 9/40).

### 3e. Not verified

- **Walk-in/entrance legs**: bursts start at songMs≈17s (harness waits for
  "playing"), entrance cams were already over in all 4 runs. No lifted-leg frames
  observed anywhere; visible legs plant/step plausibly (`r1/burst_18`,
  `r2/burst_10`). Same gap as wave-2's criterion 5 — still not explicitly proven.

## 4. Verdict reasoning

PASS would require the user-reported symptom set gone end-to-end. The "only
teeth/eyes" + shard-fan + reload-fragility mechanisms are gone (composed build,
n=4, adversarial framing: I went looking for floating eyes and fans and found
none). But the user-visible defect "band characters look wrong" is NOT closed:
every run shows partially-missing bodies, strobing garments, and full-screen
smears. That matches the wave-2 root-cause analysis (own==bound garments, C8
rotation-basis divergence) and the PLAN's expectation. **PARTIAL.**

### Acceptance bar for the C8 follow-up (measured floor = this doc)

1. Band steady-state guard-drop events → ~0/frame (today 30-41/frame) **with
   sane extents** — the bar is NOT just "unhidden" (`RB3_GUARD_EXEMPT_REBOUND=1`
   already achieves 0/frame by drawing slabs): per-mesh `worldExt ≲ 1.2×bindExt`
   during normal play.
2. Zero disembodied-part frames (torso gaps, floating gloves, legs-only) over
   n≥3 runs × 24 bursts.
3. Zero pale slab/tube/full-frame-wash smear frames; `chainsaw_strings`,
   `loudleggings`, `jeansripped`, `wrestlingboots`, `jumpsuitshorts` either
   always-drawn-sane or never-drawn (no 5-95% strobing band).
4. Regression floor preserved: FLING=0 (SKINPOS armed), skinToBone ≤ ~92u,
   `restPose` preserved across all STARTLOADs, SETDEFORM bounded (~32/run),
   no floating-eyes/shard-fan frames.
5. Explicitly capture one entrance/walk-in scene (start bursts at songMs<10s or
   screenshot manually at game_screen) to finally close the "lifted legs" claim.

## 5. New issues noticed in passing (off-topic)

1. **Song-select header junk integer**: "VIEWING 83 SONGS, SORTED BY SONG
   NAME**-1752614592**" (`r1/01_song_select.png`) — a raw negative int rendered
   into the header format string. Not on the PLAN follow-up list yet.
2. FRIEND RANKINGS overlay + grey album-art box obscuring the song grid —
   already on the PLAN follow-up list; confirmed still present on the composed
   build (`r1/01_song_select.png`).
3. Instrument meshes are in the C8 blast radius (`chainsaw_strings` strobing,
   §3c) — worth listing in the C8 task scope so it isn't filed as a separate
   "guitar strings flicker" bug later.
4. Venue extras drop hair/eyebrow meshes (`male_extras_hair02`,
   `male_extras_eyebrows11`) → bald/eyebrowless background extras; bookkeep under
   the crowd sibling issue (extras ≠ crowd dirs — make sure the crowd follow-up's
   census includes `extras*`).

## 6. Evidence inventory

`/tmp/rp3-char-render/`: `r1..r4/` (full-res frames), `r1-contact.png`,
`r2-contact.png`, `r{1..4}-zoom*.png` (montages), `r3-b09-cropLL.png` (stud-spike
magnification), `r*-harness.log`. Engine logs `/tmp/rb3-kbd2game-880{1..4}.log`
(restPose/SETDEFORM/SHARD_GUARD/SKINPOS raw data — /tmp logs get reaped; census
numbers above are the durable record). Baselines: `/tmp/rp2-char-render/BEFORE-*`.
