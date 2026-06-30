# Deferred backlog items 4 + 5 — STEP 2 re-eval & STEP 1 exposure

**STEP2-REEVAL + EXPOSURE agent (Opus). RESEARCH ONLY — no code/engine changes,
nothing committed.** Built the current master `rb3-native` (engine pin
`20dba55`, which INCLUDES all landed convergence fixes: STEP 1 GX falloff
`a360e3c`, GAP B(a) crowd-dim `ada6e56`, GAP A1 watermark `b8f3cfa`). STEP 2
(`bae1aae`, tag `converge-step2-crowd-wip`) confirmed **NOT** in the pin (HELD).
All A/B captures via `/tmp/bch_override.py` (the venue-override wrapper around
`scripts/native/band-closeup-capture.py`), pinned deterministically.

Pin verification:
```
B(a)  = ada6e56 in pin 20dba55:  YES (landed)
STEP2 = bae1aae in pin 20dba55:  NO  (held)
```

---

## ITEM 4 — STEP 2 RE-EVAL → **VERDICT: CLOSE_OBSOLETE**

GAP B(a) (landed) **alone** fixes the visible big_club_01 white crowd. STEP 2 is
**redundant** on that venue (it does not move the visible crowd, proven twice now)
AND **would conflict** (double-dim → near-black crowd) if applied on top of B(a).
Close it. Keep the runtime opt-out `RB3_CROWD_DIM_OFF=1` as the lever.

### (a) Is the impostor crowd now correct (dim) from B(a) ALONE? — YES, decisively

Fresh A/B on `big_club_01`, crowd shots `coop_dir_crowd` + `coop_all_n00`, anchored
6000ms, 4/4 pinned both runs. "crowd strip" = left/right 22% columns where the
audience figures line the stage; white% = all-channels > 200.

| mode | frameLuma | crowdL white% | crowdR white% | maxWhite% |
|---|---|---|---|---|
| **B(a) ON (default)** | 43.1–50.4 | **0.3** | **0.0** | **0.7** |
| **B(a) OFF (`RB3_CROWD_DIM_OFF=1`)** | 35.9–39.7 | 3.9–4.6 | **7.2–9.3** | 4.5–6.3 |
| small_club_01 (engine ground-truth, correct) | 34.9 | 0.1 | 0.0 | — |

Visual (the money shots, `/tmp/converge_ba/{on,off}/*coop_dir_crowd_0.png`):
- **OFF:** stark WHITE cut-out crowd figures line BOTH highway edges — identical to
  the logged bug `refs/native_bigclub_white_crowd.png`.
- **ON:** the figures are dim/dark, the white silhouettes are GONE, the venue reads
  as the intended dim B/W club backdrop.

→ **B(a) lands the crowd at small_club's correct ~0% white.** The visible big_club
white crowd is FIXED by B(a) alone. (B(a) also keeps festival_01 clean: B/W comic
poster backdrop intact, band lit, crowd white% 0.3, no over-dim —
`/tmp/converge_fest/on/feston_coop_dir_crowd00_0.png`.)

### (b) Would STEP 2 on top of B(a) double-dim / conflict? — YES, double-dim → near-black

They act on **different draws in the SAME serial pipeline**, so they multiply:

| | what it dims | where |
|---|---|---|
| **STEP 2** (held) | the 3D crowd char rendered INTO the off-screen impostor RT (the *bake*) | `WriteSceneUniforms` for the unnamed `gImpostorCamera` → reads `crowd.env` (dim, 0 lights, 0.18 amb) instead of white default |
| **B(a)** (landed) | the FINAL composited impostor BILLBOARD quad (the *blit*), ×0.10 base color | `DrawMesh`, `impostorBillboard` branch (world.cam && !skinned && empty mesh+mat name) |

Data flow (`src/system/world/Crowd.cpp:547-581`): per frame, `gImpostorCamera->Select()`
→ `curChar->DrawShowing()` renders the 3D char into the RT (STEP 2's target),
baking a near-white impostor diffuse (because the white-default lighting makes it
bright); then `mmesh->DrawShowing()` composites that texture as a billboard under
world.cam (B(a)'s target, ×0.10).

B(a)'s **0.10 multiplier was calibrated assuming the billboard diffuse is near-WHITE**
(engine comment `Rnd_Wgpu_RB3.cpp:5536`: "The impostor diffuse is near-white, so the
multiplier must be small (~0.10)"). STEP 2 would make that bake already DIM →
B(a) then multiplies the already-dim billboard by 0.10 → **double-dim → near-black
crowd**. Stacking them over-darkens; they are not complementary, they compound.

### (c) Does STEP 2 still help any OTHER venue B(a) doesn't? — NO (and on big_club it never moved the crowd anyway)

Two independent reasons STEP 2 adds no value:

1. **STEP 2 verify ALREADY measured STEP 2 does NOT move the visible crowd.**
   (`lighting/step2-verify.md`): big_club crowd white% was INVARIANT ~9–11% across
   every STEP 2 toggle (on 9.2, `RB3_CROWD_LIGHT_OFF=1` 10.3, `RB3_CROWD_GREY_KEY=0`
   10.0). The visible crowd renders via the **world.cam** billboard path, which STEP 2
   (the impostor *cam* path) doesn't touch. So even the venue STEP 2 was BUILT for
   (big_club) showed zero visible benefit from it.
2. **B(a) is venue-agnostic** — it dims the impostor billboards + skinned crowd/extras
   under world.cam in EVERY venue (verified big_club, arena_02 [crowd shot below],
   festival_01). It covers the same surface STEP 2 aimed at, on every venue, and is the
   path that actually reaches the visible pixels.

There is no venue where B(a) leaves a white impostor crowd that STEP 2 would rescue.
STEP 2's only proven effect is on the off-screen RT bake, which (a) the visible
billboards don't track in this build, and (b) where they WOULD track, it now collides
with B(a).

### STEP 2 verdict — **CLOSE_OBSOLETE**
- B(a) supersedes it: B(a) alone makes the visible crowd correct (8–9% → 0% white),
  proven by direct A/B + eyes.
- STEP 2 adds no value B(a) doesn't (STEP 2 verify already proved it doesn't move the
  visible crowd; B(a) is venue-agnostic).
- STEP 2 would CONFLICT if landed on top of B(a) (serial double-dim → near-black).
- **Action:** do NOT push/pin `bae1aae`; mark the tag `converge-step2-crowd-wip` as
  superseded. Keep `RB3_CROWD_DIM_OFF=1` (+ `RB3_CROWD_DIM=<f>`) as the runtime lever.
- **Caveat (honest):** the structural insight STEP 2 captured (the unnamed impostor
  cam misses the world.cam venue gate and falls to white-default) is *correct* and is
  why the bake is near-white in the first place. B(a) chose to fix the symptom at the
  billboard composite (simpler, exact discriminator, measured win) rather than the
  bake. If a FUTURE need arises to light the impostor RT correctly (e.g. colored crowd
  lighting that should track the venue), revisit STEP 2's gate-widening — but then B(a)
  must be REPLACED, not stacked. For now the crowd is dim-and-correct via B(a); close
  STEP 2.

---

## ITEM 5 — STEP 1 ARENA BAND EXPOSURE → **VERDICT: ACCEPT (fine as-is)**

The landed STEP 1 GX point-falloff (default-on, `RB3_VENUE_POINT_FALLOFF_LEGACY=1`
to revert) lifts the arena_02 band out of near-black silhouette into a readable,
moody-spotlit range. Exposure `sVenuePointExposure=0.70` lands it moderate (readable,
not flooded). No tuning pass needed.

### A/B — arena_02 band, GX falloff ON (default) vs LEGACY

Shots `coop_fs_b_c` (band closeup), `coop_fs_all_n00` (wide), `coop_dir_crowd`,
anchored 6000ms, 6/6 pinned both runs. "bandRegionLuma" = central upper 60% (the band
behind the highway, highway excluded); black% = band-region pixels luma < 25.

| shot | mode | bandRegionLuma | bandBlack%(<25) |
|---|---|---|---|
| **coop_fs_b_c** (closeup) | **GX ON (default)** | **61.0–61.9** | **24.4–24.5** |
| coop_fs_b_c (closeup) | LEGACY | 31.2–40.9 | **53.5–62.2** |
| coop_fs_all_n00 (wide) | GX ON | 51.3–51.4 | 29.7–30.0 |
| coop_fs_all_n00 (wide) | LEGACY | 45.3–46.4 | 31.2–31.8 |

Visual (`/tmp/converge_s1/{gxon,legacy}/*coop_fs_b_c_0.png`):
- **LEGACY:** band member is a near-black silhouette — unreadable against the dark
  stage / crowd LED board. (53–62% of the band region is black.)
- **GX ON:** band member is lit and readable (skin, outfit, pose visible),
  moody-spotlit but NOT flooded. Black% halves (~24%), bandRegionLuma ~doubles on the
  closeup (31→61).

The closeup is where it matters most (LEGACY 31→62% black vs GX 61/24%); the wide
moves less because directional fill already dominates there. game.cam (highway) and
menu cams are byte-identical (mode 1 is set only on the world.cam venue path — verified
in `step2-verify.md`).

### Ground-truth check (band brightness intent)
- **In-engine ground-truth:** small_club_01's band renders correctly readable/lit
  (the engine's own correct reference); the GX fix brings arena into the same readable
  regime rather than the near-black LEGACY look.
- **Retail screenshot** (`refs/retail_rb3_vocalist_ps3_lit.jpg`, `images/retail-
  screenshots/yt_qRagnZCIMzk_gameplay_guitar.png`): RB3 performers are clearly LIT and
  readable, not black silhouettes — consistent with GX ON, NOT with LEGACY.
- **Web search** (June 2026): no clean retail *arena/big_club* gameplay frame is
  publicly indexed (confirms prior agents' note that `images/retail-screenshots/` has
  only club gameplay). The RB3 Deluxe project documents a "Restored per-pixel lighting"
  fix, confirming RB3's original venue lighting was characterful/per-pixel (a readable
  spotlit band), not a flat-dark fallback — i.e. GX ON is directionally correct.
  Sources: rb3dx.milohax.org/features, rockband.fandom.com/wiki/Category:Venues.
  No retail arena/big_club frame was savable; none added to `refs/`.

### STEP 1 exposure verdict — **ACCEPT**
- The GX falloff is a clear, substantial visible win (near-black silhouette →
  readable spotlit band) and is the right curve (inverse-linear long-tail key reaches
  the band 70–100u from the tight arena spots; legacy `^2` falloff died before it
  reached them).
- `sVenuePointExposure=0.70` lands the band in a moderate, readable, non-flooded range
  (closeup luma ~61, black% ~24). No evidence it reads hot or washed; the wide stays
  ~51. **No tuning pass needed.**
- If a future retail arena/big_club ground-truth frame surfaces and shows the band
  brighter/dimmer than ours, `RB3_VENUE_POINT_EXPOSURE=<f>` (default 0.70) and
  `RB3_VENUE_GREY_KEY` are the tuning knobs (no rebuild). Until then: accept-as-is.
- **Honest caveat:** STEP 1's blast radius is venue-WIDE, not arena-only (it's set on
  the whole world.cam point-lit path → also brightens point-lit chars in city/festival
  envs; see `step2-verify.md` "blast radius BROADER"). That's consistent with the
  band-lit intent and masked where a directional dominates; not a regression, but the
  exposure knob is shared across venues, so a per-venue tune would need per-env scoping
  (out of scope for "is it fine?" — it is).

---

## Evidence index
- Captures: `/tmp/converge_ba/{on,off}/` (big_club B(a) A/B),
  `/tmp/converge_s1/{gxon,legacy}/` (arena STEP 1 A/B),
  `/tmp/converge_fest/on/` (festival B(a) coverage).
- Engine: `../milo-native-engine/src/platform/Rnd_Wgpu_RB3.cpp`
  — B(a) `5513-5584`, watermark `5641+`, STEP 1 falloff `1144-1158` + `1350-1359`
  exposure, default-white branch `1362-1373`, per-environ rewrite `3530-3536`.
- Crowd render pipeline: `src/system/world/Crowd.cpp:547-581` (RT bake then billboard
  composite), `gImpostorCamera` unnamed `Crowd.cpp:88`.
- Prior docs: `lighting/bigclub-white.md` (GAP 3 root cause), `lighting/step2-impl.md`
  + `lighting/step2-verify.md` (STEP 2 held + already proved no visible move),
  `audit/RANKED-GAPS.md` (GAP 2/3).
- Refs: `refs/native_bigclub_white_crowd.png` (the bug), `refs/native_arena02_black_band.png`
  (the bug), `refs/retail_rb3_vocalist_ps3_lit.jpg` (retail lit performer).
