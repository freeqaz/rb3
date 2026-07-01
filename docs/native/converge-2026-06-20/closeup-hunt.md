# Closeup-Hunt — using the deterministic harness to find band-closeup convergence bugs

Convergence batch 2026-06-20. The payoff stage: now that
[`band-closeup-capture.py`](../../../scripts/native/band-closeup-capture.py) gives a
deterministic, pinned (`pinned=N/N`) band-member closeup with a machine verdict
(`drops_band`, `max_band_ratio`), I swept songs × members × shots to find
genuinely BAND-CLOSEUP-VISIBLE render-convergence bugs vs the original game.

- Worktree: `/home/free/code/milohax/rb3/.claude/worktrees/converge-harness` (branch `wt-converge-harness`)
- Binary: `native/build-native/rb3-native` (115,964,048 bytes — prebuilt, no rebuild)
- All runs: `SHARD_DBG=1 SHARD_RATIO_DBG=1 MILO_HEADLESS=1`, guard ON unless noted.
- 44 boots scanned for the aggregate drop table.

---

## TL;DR

**One genuinely band-classified convergence bug found: band FOOTWEAR explodes
(thin `_skin.2` shoe meshes) and the V24 guard drops it.** Two meshes hit it in my
sample — `lowtopsneaks_skin.2.mesh` (15,700 drops across 44 boots) and
`saddleshoe_skin.2.mesh` (4,935). Same root cause as the char-skinning-deform
family (rotation-basis fling), but on FEET bones (ankle/toe/knee), which **no
existing rebind covers** (the torso rebind is hard-gated to
trackjacket/vestdenim/plaidshirt/shred; InstStrings is guitar-only). So it is a
**NEW gap**, not fixed by `RB3_NO_SKEL_REBIND` / `RB3_NO_INST_REBIND`.

**BUT — the honest convergence caveat:** in the venue auto-director's actual shot
vocabulary (all guitarist/drummer closeups keep the note highway as the dominant
lower-2/3 of the screen and frame the member's UPPER body / instrument), the
exploded shoe is at foot height **below the visible frame**. The drop fires
frequently and is real geometry-vs-retail, but it is **rarely closeup-visible** in
the shots the game cuts to. There is **no dedicated feet / kick-pedal / full-body /
wide shot that resolves in this club venue** (`coop_d_pnt`/`d_lt`/`d_kp`/`legs`/
`feet`/`pedal`/`wide`/`all` all `not_found`; the probe's `coop_dir_d_pnt_m00`/
`coop_dir_d_lt00` are arena_01 dir-cut SELECTOR names, not club shots). The closest
to feet-visible is `coop_front_n00` (frontman wide) where band legs are tiny and at
the screen edges.

Net: **fix-eligible (real band geometry, clean rebind-family pattern), but LOW
closeup-visible ROI in practice** because the offending geometry is off-frame in
the shots the auto-director uses. Worth doing only as part of a broader
foot-rebind, not as a standalone closeup fix.

---

## 1. Sweep coverage (songs × members × shots)

`--member guitar` uses the resolvable club guitarist closeups
`coop_g_cg, coop_g_cg01, coop_g_n01, coop_g_n03, coop_g_b`; drums uses
`coop_d_n01..n03`; bass `coop_b_*` (resolved at runtime). Every run held
`pinned=15/15` (or N/N) — the camera pin is solid across the whole sweep.

| song (--song-downs) | member | verdict | drops_band | max_band_ratio | closest band-to-cap |
|---|---|---|---|---|---|
| 0 | guitar | PASS | 0 | 2.70 | tightdistressedpants_skin.2 |
| 2 | guitar | PASS | 0 | 3.64 | **lowtopsneaks_skin.2:3.64** (just under cap) |
| 4 | guitar | **FAIL** | 346 | 4.76 | **saddleshoe_skin.2** over cap |
| 4 | drums | PASS | 0 | 3.40 | wovensteppers_skin.2 |
| 4 | bass | PASS | 0 | 4.05 | gloves_resource.1:3.97 (40u-floor saved) |
| 6 | guitar | PASS | 0 | 3.08 | eightholedocs_resource |
| 8 | guitar | **FAIL** | 778 | 5.45 | **saddleshoe_skin.2** over cap |

Songs covered: `--song-downs` 0, 2, 4, 6, 8 (5 songs). Members: guitar (primary),
drums, bass. **Skipped:** keys/vocals as primary (no `_cls`/`_cg` member closeups —
vocals only `coop_front_*`/`coop_v_*` framings which I exercised in the wide probe);
songs beyond `--song-downs 8`; arena venues (the boot songs all load the small-club
venue, so the arena_01 `coop_dir_*` shot set never applies — confirmed by the impl
doc + the feet-shot probe below).

---

## 2. The band-footwear drop — repro + characterization

`saddleshoe_skin.2.mesh` (bindExt **11.21u**, a tiny shoe submesh) bound to
`bone_R-ankle / bone_R-toe / bone_L-ankle / bone_L-toe / bone_{L,R}-knee` (asset
`char/main/feet/female/gen/saddleshoes_leather.milo_xbox`). During the pinned
guitarist closeup the foot/leg bone pose drives worldExt to **44–60u → ratio
4.0–5.45**, exceeding the relaxed band cap (4.0x), so the V24 guard drops it every
frame the pose is bad (346–778 drops in the sweep; up to 4,206 in a single 5-boot
baseline batch). `lowtopsneaks_skin.2.mesh` is the same story (ratio peaks 4.6–5.5).

Drop-line sample (guard ON, sd8):
```
[SHARD_GUARD] dropped degenerate skinned mesh='saddleshoe_skin.2.mesh'
  bindExt=11.21 worldExt=44.85 ratio=4.0 dir='' bone0=(-76.2,69.1,16.2)
```
bone0 y≈69 = ankle/foot height. The `_resource` rigid companion mesh stays under
cap (ratio ≤3.9); only the thin `_skin.2` deform submesh explodes — same signature
as the landed char-skinning fixes.

### This is the rotation-basis fling family (engine confirms)
The guard header (`Rnd_Wgpu_RB3.cpp:4907-4984`) describes exactly this: a far-from-
bone vertex flings by `R·sin(theta)` under a rotation-BASIS error. The
`RB3_GUARD_EXEMPT_REBOUND` experiment note (`:4930-4942`) is decisive: exempting +
rest-rebaking these meshes "anchors translation … but the native rotation-basis
divergence remains … exempt meshes drew as full-screen slabs. The 2.0x ratio guard
is CORRECT about those poses; keep dropping them by default until the pose-pipeline
basis root-cause (C8) is fixed." So a naive exempt/rebind would draw a worse
artifact — the guard drop is the correct stopgap.

---

## 3. lowtopsneaks hunt (probe §5 follow-up) — REPRODUCED, song/outfit-gated

Probe §5 said lowtopsneaks was present-but-not-dropped on the boot song. With the
camera PINNED I reproduced its drop:

- **--song-downs 2:** lowtopsneaks present, ratio 3.64 — **just under** the 4.0 cap,
  0 drops (confirms probe §5's "near the cap" reading on a song where the pose
  stays bounded).
- **--song-downs 8 (multiple boots):** lowtopsneaks ratio **4.6–5.5**, dropped
  1,408–2,704× per boot — **over the cap**.

The deciding variable is **which footwear the band's randomly-selected prefab rolls
that boot** (the gameplay band uses random `char/main/prefab/*.milo` prefabs, so the
wardrobe — incl. footwear — changes every boot). saddleshoe / lowtopsneaks are the
two footwear in my sample that ship a thin `_skin.2` submesh; rigid-only footwear
(wrestlingboots, eightholedocs, talldocs, timberlandboots — `_resource` only) never
explode. `wovensteppers_skin.2` / `maleslipons2_skin.2` are the next-closest thin-
skin shoes (ratio peaked 3.0–3.4, 0 drops in my sample).

Songs/outfits tried for the lowtopsneaks/footwear repro: `--song-downs` 0,2,4,6,8 ×
~25 boots; thin-skin footwear rolled and DROPPED on sd4 and sd8; rolled but stayed
under cap on sd2; absent (rigid-only footwear) on most sd0/sd6 boots.

---

## 4. Existing-fix coverage — A/B + rebind-env toggles (it's a NEW gap)

| condition (guard ON, sd8 guitar) | footwear in wardrobe | drops_band |
|---|---|---|
| baseline | saddleshoe/lowtops rolled | **346–4,206** (drops) |
| baseline | rigid-only rolled | 0 |
| `RB3_NO_SKEL_REBIND=1` | saddleshoe rolled (`fnoskel`) | **3,778** (still drops) |
| `RB3_NO_INST_REBIND=1` | (boot rolled rigid-only) | 0 (no thin shoe that boot) |
| `SHARD_GUARD_OFF=1` | saddleshoe/lowtops rolled | 0 drops, mesh **renders exploded** (ratio 5.5) |

**Conclusion:** neither `RB3_NO_SKEL_REBIND` nor `RB3_NO_INST_REBIND` changes the
footwear drop — when a thin-skin shoe is in the wardrobe it drops regardless. This
is expected from the source: `RebindOutfitBonesToOwnSkeleton` is hard-gated to
torso meshes (`BandCharacter.cpp:982` — `trackjacket/vestdenim/plaidshirt/shred`),
and `RebindInstStringsToRestBasis` is guitar-strings-only. **No rebind touches
feet.** So the footwear shard is a genuinely uncovered gap in the same family.

> METHODOLOGY NOTE (important for any follow-up): the band **wardrobe is randomized
> per boot** (random prefab chars). This is a large confound — drops_band swings
> 0↔4000 across boots of the SAME config purely on whether a thin-skin shoe rolled.
> Single-boot env A/B is therefore UNRELIABLE; you must either pool many boots and
> compare only boots where the shoe is present, or implement a same-process
> wardrobe pin. The harness-verify doc's same-process re-force A/B (still
> unimplemented) would remove BOTH the wardrobe and the pose nondeterminism.

---

## 5. Closeup-visibility — the honest caveat (why ROI is low)

Every resolvable club closeup keeps the note highway as the dominant lower screen
and frames the member's UPPER body / instrument:

- `coop_g_cg`/`coop_g_cg01` = guitar neck + fretboard from behind (no feet).
- `coop_g_n01/n03/b` = guitarist head/shoulders + neck (no feet).
- `coop_d_n01..n05/b` = drum kit + cymbals from the side (legs below frame).
- `coop_front_n00` = frontman wide — band legs visible but **tiny + at screen
  edges**, behind/below the highway base.

I captured guard-OFF frames on boots where saddleshoe/lowtops were present and
exploded (ratio 5.5): the foot shard (worldExt ~45–60u, at foot height) is **below
the visible frame** in every guitarist/drummer closeup, and only marginally in-frame
(tiny, edge) in the frontman wide. Screenshots under
`docs/native/converge-2026-06-20/shots/hunt/`:

- `guardON_guitar_cg_lowtops-dropped.png` — common case: lowtops dropped this boot,
  guitar-neck framing, foot off-frame → clean (drop invisible).
- `guardOFF_guitar_b_saddleshoe-present.png` / `_cg01_*` — saddleshoe present +
  exploded (guard OFF), still off-frame in upper-body framing → not visible.
- `guardOFF_front_wide_lowtops-present.png` — frontman wide, band legs at edges; the
  best-case feet framing and even here the shard is small/edge.
- `guardOFF_drummer_b_lowtops-present.png` / `guardON_drummer_n01_kit-framing.png` —
  drum-kit side framing, drummer legs below frame.

No dedicated feet/kick-pedal/full-body/wide shot resolves in this venue (probed 28
candidate names; only `coop_d_n04/n05/b`, `coop_v_n01`, `coop_front_n00` resolved
beyond the default set). Retail ground truth agrees (scout-groundtruth §4): the
retail per-instrument closeups frame upper body; shoes are only visible in a
full-body/feet shot, which the auto-director here does not use.

---

## 6. Non-band drops (separate families — confirmed, NOT chased)

Aggregate drops across 44 boots (classified band vs other via the RATIO band-mesh
set):

| family | mesh | total drops | family / owner | verdict |
|---|---|---|---|---|
| **UI-draw-tree** | `scrollbar_bg.mesh` | **22,293** | UI list-scrollbar widget leaking into the gameplay 3D draw (preloaded `ui/resource/scrollbar_display.milo`) | DOMINANT residual; separate family (see scout-residual (a)). Drop is *accidentally correct* (retail draws no scrollbar in-venue). Defer. |
| crowd-servo | `male_extras_hair02.mesh` | 2,598 | vignette extra (`char/extras/`) | crowd/extras servo-skeleton basis; no rebind exists for non-BandCharacters. Defer. |
| crowd-servo | `male_extras_eyebrows11.mesh` | 2,598 | vignette extra | same. Defer. |
| crowd-servo | `fist.mesh` | 2,312 | crowd hand-prop | single-bone rigid-anchor candidate (like InstStrings) on the crowd path. Defer. |
| crowd-servo | `clap.mesh` | 1,813 | crowd hand-prop (`crowd_male03`) | same. Defer. |

These are exactly the families scout-residual ranked: scrollbar = UI-in-3D-scene
leak (highest raw count, but guarded render already ≈ retail); male_extras/clap/fist
= non-band crowd/extras servo-skeleton shards needing a NEW crowd/`Character` rebind
hook (high blast-radius, low payoff). All distinct from the band footwear family and
out of scope for this batch — flagged for a possible later UI-draw-tree /
crowd-servo batch.

---

## 7. Ranked list of closeup-visible band convergence bugs

| rank | mesh | repro (song-downs / member / shot) | ratio | existing env fixes it? | recommended fix family | blast radius | worth doing? |
|---|---|---|---|---|---|---|---|
| 1 | `lowtopsneaks_skin.2.mesh` | sd8 guitar `coop_g_*` (when prefab rolls lowtops) | 4.6–5.5 | **No** (NO_SKEL/NO_INST don't touch feet) | **new FOOT rebind** on `BandCharacter` (extend the rebind family to ankle/toe/knee-bound `*_skin.2` footwear) — BUT see C8 caveat: a naive rebind shards worse (rotation-basis), so it needs the pose-pipeline basis fix, OR a foot-specific rigid-anchor like InstStrings | MEDIUM — band-only, but the rotation-basis root-cause (C8) is the real blocker | **LOW** — real geometry but off-frame in every club closeup; only matters in a feet/full-body shot the venue doesn't use. Do only as part of a general foot/C8 fix. |
| 2 | `saddleshoe_skin.2.mesh` | sd4 + sd8 guitar `coop_g_*` (when prefab rolls saddleshoe) | 4.0–5.45 | **No** | same as #1 (identical rig: ankle/toe/knee `_skin.2`) | MEDIUM | **LOW** — same off-frame caveat. Bundle with #1. |
| — | `wovensteppers_skin.2` / `maleslipons2_skin.2` | near-cap (3.0–3.4), 0 drops in sample | <cap | n/a | none needed now (cap-protected) | — | watch-list — would join #1/#2 if a higher-motion song pushes them over. |

**Recommended fix family for #1+#2 (one fix covers both):** extend the band rebind
to footwear. The cleanest tractable approach mirrors `RebindInstStringsToRestBasis`
(`BandCharacter.cpp:1395`) — a **single-anchor rigid rebake** of the thin `_skin.2`
shoe submesh to its least-moving foot bone (likely `bone_R-ankle`/`bone_L-ankle`),
baking `offset = meshWorld · inv(anchorWorld)` so the shoe rides the ankle rigidly
(ratio→~1.0) instead of blending across toe/knee with a divergent basis. This is the
same pattern that fixed the guitar `_strings` explosion. Blast radius is band-only
(HX_NATIVE, Wii-neutral, opt-out env), but **gate it behind a strict A/B**: per the
engine's own RB3_GUARD_EXEMPT_REBOUND note, a translation-only anchor leaves the
rotation-basis fling and can draw a worse slab — so the rigid-anchor must be
validated to actually bring the ratio under cap (the harness's `max_band_ratio` /
`drops_band` is the gate), not just exempt the mesh.

**Honest bottom line:** the harness did its job — it surfaced a real,
band-classified, previously-masked convergence bug (band footwear shard, 20k+ drops
across 44 boots, uncovered by any existing fix) that the unpinned probe could only
guess at. But the same harness also proves the bug is **largely not closeup-visible**
in the shots the game actually uses (foot geometry off-frame), so it is a
**low-priority, fix-eligible-but-defer** item — best folded into a future general
foot-rebind / C8 pose-basis batch rather than chased on its own. A clean bill for
the *visible* upper-body band closeups: across the whole sweep no band garment that
is actually IN a closeup framing (jackets/arms/heads/gloves) was dropped — those
stay within caps (gloves the closest at 3.97, saved by the 40u floor).

---

## 8. Reproduce

```bash
cd /home/free/code/milohax/rb3/.claude/worktrees/converge-harness
# repro the footwear drop (run a few boots — wardrobe is random per boot):
SHARD_DBG=1 SHARD_RATIO_DBG=1 MILO_HEADLESS=1 python3 scripts/native/band-closeup-capture.py \
    --member guitar --song-downs 8 --frames 3 --out /tmp/bch/repro --tag repro
# -> verdict=FAIL drops_band>0 closest_band_to_cap=saddleshoe_skin.2.mesh / lowtopsneaks_skin.2.mesh
#    when the prefab rolls a thin-skin shoe; PASS otherwise.

# see the explosion rendered (guard OFF) on a shoe boot:
SHARD_GUARD_OFF=1 SHARD_RATIO_DBG=1 MILO_HEADLESS=1 python3 scripts/native/band-closeup-capture.py \
    --member guitar --song-downs 8 --frames 3 --out /tmp/bch/off --tag off
```
