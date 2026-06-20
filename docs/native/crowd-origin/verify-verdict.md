# VERIFY VERDICT — adversarial re-derivation of the crowd/band position dump

Date: 2026-06-20. Agent: adversarial verify (Opus). Engine pin
`MILO_ENGINE_PIN = 884ab17d…` (matches the measure run + engine HEAD). Tool
committed at `fe6b5a73` (`native/src/rb3_http_handlers.cpp` `{rb3_pos_dump}`).

I re-derived the verdict from the RAW dump logs in `/tmp/rb3-posdump-*.{,snapshot.}log`
(NOT from `measure-results.md`'s prose) and audited the committed tool for bugs that
could make the data lie.

**Bottom line: I AGREE with the measure agent's verdict — H1/H2/H4/H5 are refuted
as "everything-at-origin"; the visible pile is the V24 `[SHARD_GUARD]` dropping
over-deformed skinned instrument/extra geometry (a SKINNING problem). The band
placement DEMONSTRABLY works.** I found NO tool bug that flips the verdict. But the
measure write-up has two factual inaccuracies and ONE genuine evidentiary gap on the
crowd half that the next batch must not lean on, detailed below.

---

## 1. What I independently re-derived from the RAW logs

### Band — placement WORKS (H2/H5 correctly REFUTED). Re-derived, decisive.

`[POSDUMP] kind=band` across 3 independent runs (38787, 43235, 36337):

| slot | root (38787) | root (43235) | root (36337) | parent |
|---|---|---|---|---|
| player0 | (88.9, 49.6, 13.25) | (69.0, 51.6, 13.25) | (68.7, 51.4, 13.25) | NULL |
| player1 | (-69.8, 81.2, 13.50) | (-69.0, 81.0, 13.50) | (-70.1, 80.7, 13.50) | NULL |
| player2 | (-10.1, 31.5, 13.22) | (-15.8, 26.5, 13.22) | (-12.8, 33.0, 13.22) | NULL |
| player3 | (14.43, 146.13, 13.18) | (14.43, 146.13, 13.18) | (14.43, 146.13, 13.18) | NULL |

The DECISIVE detail the measure write-up under-sold: **player3 is byte-stable at
(14.43, 146.13, 13.18) across ALL runs**, and EVERY member's root z is a consistent
stage-floor height (13.18–13.50). x/y for players 0–2 jitter a few units (idle/anim
motion in a playing song); a true at-origin collapse would read (0,0,0) with no such
structure. The `mInstDir` (kit) world-sphere co-locates with each root (e.g. player0
root (88.9,49.6,13.3) ↔ inst (96.6,59.3,46.6): dx 7.7, dy 9.7, dz 33.3 — kit held at
torso height above the stage spot). `band_at_origin=0/4`. **H2 ("root never placed →
origin") and H5 ("kit floats away from drummer") are REFUTED at the data layer.** This
is the strongest, most direct claim in the whole investigation and it is airtight.

NOTE on `parent=NULL`: every band root has a NULL TransParent yet a real spread
WorldXfm. That is the "parent NULL is benign" the measure agent flagged — the placement
mechanism writes the root WorldXfm absolutely rather than re-parenting under a venue
dir. The PLAN.md §1 fork "parent NULL ⇒ H2 confirmed" is therefore a FALSE TELL here:
NULL parent + spread WorldXfm = placed-by-absolute-xfm, not unplaced. (This matters for
the fix: see §4 — we still don't know WHAT writes that absolute WorldXfm.)

### Static props (control) — HOLDS. Re-derived.

`[POSDUMP] kind=prop`: the only non-origin props are the venue cameras / interest
(`world.cam` = (-267, -431, 131)). Everything legitimately at origin is cams, the
default light, the `world`/`small_club_01` WorldDirs, the crowd ARCHETYPE Characters
(`crowd_male01..04`, `crowd_female01..04` — re-`SetWorldXfm`'d per draw, expected at
origin between draws), and `crowd_chars.grp`. No broadly-collapsed prop. **The
dartboard-control premise holds; H1 (shared reparent collapse) is REFUTED** — neither
half is at origin and the venue dirs sit correctly at their own origin.

### Crowd — the bug: the AUDIENCE crowd was NEVER directly measured.

`crowd=0` in every run: `dynamic_cast<WorldCrowd*>` matched zero objects (the
`WorldCrowd` container lives under a per-song venue proxy subdir the walked roots don't
reach — a documented tool limitation, acknowledged by the measure agent). The H4
refutation therefore rests ENTIRELY on the `[SHARD_GUARD]` bone0 fallback.

Re-deriving the shard data from the raw logs (this is where the measure prose drifts):

| run | total drops | dominant dir | crowd-AUDIENCE body drops |
|---|---|---|---|
| 38787 | 1389 | `instrument` 1161 (`guitar_brain_strings.mesh`), `scrollbar` 104, `male_extras02` 62, `male_extras11` 62 | **0** |
| 43235 | 1421 | `instrument` 1192, `scrollbar` 105, `male_extras*` 124 | **0** |
| 36337 | 1287 | `''` 1058, `scrollbar` 105, `male_extras*` 124 | **0** |

`[SHARD_GUARD]` bone0 IS spread (range |40..622|, `bone0_at_origin=0`), confirming the
DROPPED meshes render at real venue positions, not origin. **So the visible pile is the
guard dropping deformed geometry, not placement-at-origin — that conclusion is correct.**

BUT: the spread-bone0 evidence comes from `male_extras*` (venue VIGNETTE extras) and
`instrument`/`scrollbar`, **NOT from the `WorldCrowd` AUDIENCE archetypes
(`crowd_male*`/`crowd_female*`).** The audience bodies are dropped **0 times in every
run** — so the audience crowd's actual draw positions are UNMEASURED by both the direct
path (`crowd=0`) and the shard fallback (audience never dropped). H4 ("crowd `unk0`
decodes to zero") is **NOT directly refuted** for the audience; it is refuted only for
the venue extras. This is the one place I downgrade the measure agent's confidence.

---

## 2. Factual errors in `measure-results.md` (do not propagate)

1. **The "needs to be re-exported" NOTIFY does NOT name `guitar_brain_strings.mesh`.**
   The measure write-up (lines 137, 178) cites
   `NOTIFY: …Skinned mesh needs to be re-exported: guitar_brain_strings.mesh` as the
   "upstream cause" for the instrument drops. Grep of all logs: the 336 re-export
   NOTIFY lines name `eyes.mesh`/`head.mesh`/`tongue.mesh` (`char/main/head/*/head.milo`)
   and `clap.mesh`/`*crowd_body*`/`fist.mesh`/`horns.mesh`/`lighter.mesh`
   (`char/crowd/crowd_female0*.milo`). NONE names the guitar. The re-export NOTIFY is a
   RED HERRING for the instrument drop — it fires for heads + crowd-body meshes, which
   are NOT the meshes the guard drops. Do not use it as the instrument-drop lead.
2. **`dir='instrument'` is NOT in every run.** It dominates 38787/43235 (~1161–1192
   drops) but is ABSENT in 36337 (where `dir=''` dominates at 1058). The instrument
   drop is real and frame/run-variant, but "1161 drops/run" is not invariant.

Neither error changes the verdict; both would mislead the fix agent if taken at face
value.

---

## 3. Tool audit — is `{rb3_pos_dump}` lying? NO load-bearing bug.

I read the committed handler (`native/src/rb3_http_handlers.cpp:518-653`) line by line.

- **Cast order is SOUND.** `WorldCrowd : RndDrawable+RndPollable` (NOT a
  RndTransformable), so it can only match the first cast. `BandCharacter : Character :
  RndDir : ObjectDir+RndTransformable`, so it would ALSO match the trailing
  RndTransformable cast — but BandCharacter is tested FIRST, so band chars are never
  miscounted as props. No double-count (a `std::set<Object*> seen` de-dups across the
  multi-root walk). Verified against the actual class headers.
- **`WorldXfm()` returns the LIVE composed xfm.** `Trans.h:104` forces
  `WorldXfm_Force()` when the cache dirty bit is set, so the band roots are real
  composed world positions, not a stale `mWorldXfm`. NOT a "read-before-compose" bug.
- **`m3DChars[i].unk0.v` is the right field.** `Crowd.h` confirms `Char3D::unk0` is the
  per-member `Transform` and `.v` its translation — the authored absolute position. The
  read is correct; it simply never executes because `crowd=0`.
- **Gameplay genuinely loaded.** The harness waits `songMs > GAMEPLAY_SONGMS` + a settle
  before dumping; logs reach `f=2499`; `band=4` with spread roots; the crowd/head
  re-export NOTIFYs prove the venue+chars actually loaded. The dump did NOT fire early on
  an empty scene. `band=4 ✓`, but `crowd>0 ✗` (the documented gap, not an early-dump).
- **`MakeWorldSphere(sph,false)` for the kit** is a virtual on Character; reading it
  post-settle on a loaded `mInstDir` is fine — its co-location with the root is the
  cross-check, and it agrees.

**The ONE real tool limitation (not a bug, but it makes the H4 conclusion indirect):**
the walk roots (`sMainDir`, `world_panel->LoadedDir()`, the 3 resident venue milos) do
NOT descend into the per-song venue proxy subdir where `WorldCrowd` actually lives, so
the audience `unk0` is never read. The tool falls back to SHARD_GUARD bone0 — which only
covers EXTRAS, not the audience. To truly close H4 the tool must descend that subdir
(measure agent's own follow-on #4).

---

## 4. Corrected verdict + the OPEN QUESTION the fix must answer

**Corrected verdict (I stand behind):**
- **H2 — REFUTED (high confidence, direct).** Band roots are placed at distinct spread
  stage spots (player3 byte-stable across runs; all z≈stage floor); kit rides each root.
- **H5 — REFUTED (direct).** Kit co-located with each drummer.
- **H1 — REFUTED (direct).** No shared collapse; venue dirs at their own origin; props
  control holds.
- **H4 — REFUTED FOR EXTRAS, UNPROVEN FOR THE AUDIENCE (medium confidence).** Extras +
  instrument shard bone0 are spread; the `WorldCrowd` audience was never measured (it is
  never SHARD-dropped, so it is NOT the visible pile — the pile is `instrument` +
  `male_extras*` + `scrollbar`). H4-for-audience is a loose end, NOT the cause of the
  reported symptom.
- **ACTUAL cause (confirmed): the V24 `[SHARD_GUARD]` (engine `Rnd_Wgpu_RB3.cpp:
  4924-5141`) drops the band instrument geometry (`guitar_brain_strings.mesh`,
  `dir='instrument'`, worldExt/bindExt ≈ 4.5–5.1× vs the 2.0× non-band threshold) +
  `male_extras*` + `scrollbar_bg.mesh`.** Bones place correctly; the SKIN composes to a
  runaway world AABB → the guard fires → the meshes that should fill the stage/crowd are
  dropped → sparse/jumbled remnant. A skinning/inverse-bind deform, NOT placement.

**Why the instrument drop is the priority:** `guitar_brain_strings.mesh` binds
`dir='instrument'` (NOT `skeleton_unshared.milo`), so the engine's `bandMember`
detector does NOT recognize it as a band garment and applies the STRICT 2.0× ratio cap
(`Rnd_Wgpu_RB3.cpp:5108`) instead of the band-relaxed 4.0×/110u caps (:5101-5106). At
ratio 4.7 it trips the strict cap. This is the same false-positive family the
band-garment relaxation (:5073-5106) already handles for `skeleton_unshared` meshes —
the instrument-on-band-bones case was just never added to that relaxation.

**THE OPEN QUESTION the fix must answer (two forks — answer both before coding):**

1. **Is the instrument skin genuinely exploding, or is the guard's bind-vs-world ratio
   a false positive on instrument geometry?** Re-run with `SHARD_GUARD_OFF=1` +
   `/api/screenshot`: if the un-dropped guitar/kit renders at the correct stage spot
   intact → guard false-positive → fix = extend the band-garment relaxation
   (`Rnd_Wgpu_RB3.cpp:5073-5106`) to recognize instrument-on-band-bones (the bone's
   owning dir is `instrument` AND a sibling bone resolves to `skeleton_unshared.milo`),
   i.e. treat it as a band member. If the un-dropped kit is visibly EXPLODED/flung →
   real skin-deform bug → fix the inverse-bind/rebake for `mInstDir`, same class as the
   char-skinning-deform saga (`BandCharacter.cpp` rest-rebake path :785-799), NOT the
   guard. **This A/B is the single highest-value next action and it is cheap.**

2. **(Lower priority, audience loose end) Is the `WorldCrowd` AUDIENCE actually drawn,
   and where?** It is never SHARD-dropped and never enumerated. Before spending any fix
   cycle on the audience, extend `{rb3_pos_dump}` to descend the per-song venue proxy
   subdir and read `m3DChars[i].unk0.v` directly (measure follow-on #4), OR confirm via
   screenshot that the audience is visibly present-and-spread. Do NOT assume H4 is
   settled for the audience.

**Concrete files to investigate (in priority order):**
- `milo-native-engine/src/platform/Rnd_Wgpu_RB3.cpp:5073-5108` — the `bandMember`
  detector + the strict-vs-relaxed ratio fork (where the instrument false-positive, if
  it is one, must be fixed). Add an `instrument`-on-band-bones recognition.
- `milo-native-engine/src/platform/Rnd_Wgpu_RB3.cpp:4924-5071` — the skin-extent
  computation feeding the ratio (verify worldExt isn't inflated by a bad palette).
- `src/system/bandobj/BandCharacter.cpp:147-149` (`mInstDir`), `:785-799` (rest-rebake),
  `:2738-2748` (instrument merge `SetLocalXfm`/ReplaceRefs) — if the A/B shows a REAL
  explosion, the inverse-bind for the merged instrument is the culprit, HX_NATIVE-gated.
- `native/src/rb3_http_handlers.cpp:518-653` — extend the walk to reach `WorldCrowd`
  for the audience loose end (tool change, native-only, no gating).

**Do NOT pursue:** `world/Instance.cpp` (`SyncDir`), `BandWardrobe.cpp` proxy
placement, or any "place the band root in the venue spot" fix — the dump proves the
root IS placed. Those were the leading PLAN hypotheses and the dump has retired them.

---

## 5. Confidence

- Band placement works (H2/H5 refuted): **HIGH** — direct, multi-run, byte-stable
  control (player3), control props hold.
- Cause = SHARD_GUARD dropping deformed instrument/extra skin: **HIGH** — 1058–1192
  `instrument` drops/run at ratio 4.5–5.1, bone0 co-located with player0's stage spot.
- H4 fully refuted for the AUDIENCE crowd: **MEDIUM** — refuted for extras, but the
  audience was never measured (and is never the dropped/visible pile, so it is not the
  reported symptom regardless).
- The fix is the band-garment-relaxation / skin-deform path (NOT venue placement):
  **HIGH** — but which of the two forks (guard false-positive vs real explosion)
  decides the exact edit is OPEN and must be settled by the `SHARD_GUARD_OFF=1` A/B
  FIRST.
