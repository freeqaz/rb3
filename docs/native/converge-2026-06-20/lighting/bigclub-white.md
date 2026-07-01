# GAP 3 root-cause — big_club_01 crowd renders as flat stark-WHITE silhouettes

**BIGCLUB-WHITE agent (Opus). RESEARCH ONLY — no code/engine changes, no rebuild,
no commit.** Used the prebuilt `native/build-native/rb3-native` (115,998,568 bytes,
engine pin `5cbe8556` = `../milo-native-engine` HEAD) via `/tmp/bch_override.py`
(the venue-override wrapper around `scripts/native/band-closeup-capture.py`).

Screenshots: `docs/native/converge-2026-06-20/lighting/shots/`.

---

## TL;DR — ROOT CAUSE FOUND (high confidence)

The big_club_01 audience renders flat white because the **2D bowl-imposter crowd is
rendered into an off-screen RT through an UNNAMED `RndCam` (`gImpostorCamera`)**, and
the engine's venue-lighting path is gated on the camera being **named exactly
`world.cam`**. The unnamed impostor cam never matches, so every crowd archetype is
lit by the engine's hardcoded **default lighting (one white directional `(1,1,1)` +
`0.45` grey ambient)** instead of the crowd's own authored environ. Baked into the
crowd impostor texture, that flat full-bright lighting makes the audience read as
stark white cut-outs.

The crowd's authored environ **IS** correctly set as `RndEnviron::sCurrent` during the
impostor draw (`Crowd.cpp:549`), and it is **dim by design** (`RB3_crowd_mesh.env`:
ambient `(0.18,0.18,0.18)`, **zero lights**). The engine simply refuses to read it for
the unnamed cam. If it did, the crowd would render dark/dim (the correct, retail look —
exactly how small_club_01's crowd already renders).

This is **NOT** the `RB3_VENUE_LIGHT` path failing (the A/B proves the crowd is
guard-AND-venue-light-independent). It is a SEPARATE camera (the impostor RT cam) that
the venue-light path was never extended to cover.

**Relationship to GAP 2 (arena band dark): SAME FAMILY, DIFFERENT CAMERA.** Both are
"a character draw not lit by its authored environ." GAP 2's band IS under world.cam
(gets the venue path, but the arena env's spots don't reach it); GAP 3's crowd is under
the impostor cam (never gets the venue path at all). The shared lesson — *per-environ
character lighting must follow `RndEnviron::sCurrent`, not a hardcoded cam name* — is
the same. See §6.

---

## 1. Repro + measurement (confirmed: present guard ON AND OFF; venue-light-independent)

`set_venue_override big_club_01`, crowd shot `coop_dir_crowd.shot` (+ wide
`coop_all_n00.shot`). All shots pinned 6/6, deterministic. Crowd figures line the
LEFT + RIGHT edges of the frame; "white%" = fraction of pixels with all channels > 200.

| mode | crowdL luma | crowdL white% | crowdR luma | crowdR white% | frame luma |
|---|---|---|---|---|---|
| **guard ON** (default) | 40.5 | **9.4** | 47.3 | **14.4** | 37.6 |
| **guard OFF** (`SHARD_GUARD_OFF=1`) | 36.9 | **7.3** | 43.0 | **12.0** | 35.6 |
| **`RB3_VENUE_LIGHT_OFF=1`** | 126.3 | **9.7** | 142.5 | **18.3** | 111.2 |
| small_club_01 (correct, dim) | 23.0 | **0.1** | 19.7 | **0.0** | 34.9 |

Reads:
- **Guard ON ≈ Guard OFF white%** → the crowd-white is NOT a masked shard; it is how
  the crowd is *shaded*. (Confirms the audit's Group-B G3.)
- **`VENUE_LIGHT_OFF` leaves the crowd white% essentially UNCHANGED** (9.4 → 9.7,
  14.4 → 18.3) even though the venue backdrop luma triples (37.6 → 111.2, because the
  venue-light path goes off → the backdrop floods to default lighting). **The crowd
  does NOT track the venue-light path.** This is the decisive A/B.
- **big_club crowd white% = 9–18% vs small_club = 0–0.1%** → the bug is specific to
  venues that draw 3D impostor crowd characters (see §4). small_club's dim crowd is the
  in-engine ground-truth for "correct."

Screenshots:
- `shots/bigclub_crowd_guardON.png`, `shots/bigclub_crowd_guardOFF.png` — identical
  white audience lining the stage either way.
- `shots/bigclub_crowd_VENUELIGHTOFF.png` — backdrop bright/grey-washed but the crowd
  is STILL stark white (the A/B money shot).
- `shots/bigclub_wide_n00_guardON.png` — wide; white figures both edges, band behind
  the highway renders correctly dark/lit.
- `shots/smallclub_crowd_OK.png` — the correct dim crowd.

---

## 2. WHAT renders white: the 3D crowd impostor characters (not billboards, not a
   missing texture)

The white figures are **WorldCrowd 3D impostor characters** rendered through the
"2D bowl-imposter crowd" path:

- `src/system/world/Crowd.cpp:88` — `gImpostorCamera = Hmx::Object::New<RndCam>();`
  **never named** → `cam->Name()` returns `""`.
- `Crowd.cpp:78-87` — `gImpostorMat`: `SetPreLit(false)`, `SetUseEnv(true)`,
  `SetPointLights(true)`. (The impostor *quad* mat; the actual character body mats are
  also non-prelit skin/cloth.)
- `WorldCrowd::DrawShowing` (`Crowd.cpp:429`) → per archetype:
  - `Crowd.cpp:547-563`: `RndEnvironTracker tracker(mEnviron, &charWorldXfm.v)` sets
    `RndEnviron::sCurrent = mEnviron`, then `gImpostorCamera->Select()`, then
    `curChar->DrawShowing()` draws the **real 3D character** into the impostor RT,
    then `curCam->Select()` restores.
  - `Crowd.cpp:567-581`: the resulting impostor texture is composited as a billboard
    via `mmesh->DrawShowing()`.

So the white is the *character draw baked into the impostor RT*, not an unlit billboard
fallback and not a missing/white-default texture (the diffuse samples fine — it's the
**lighting term** that is full-bright). The non-prelit, non-unlit skin/cloth material
takes the engine's LIT shader branch.

---

## 3. WHY it is white — engine trace (file:line)

All in `../milo-native-engine/src/platform/Rnd_Wgpu_RB3.cpp`:

1. `gImpostorCamera->Select()` makes the unnamed cam `RndCam::sCurrent`. The next
   `DrawMesh` sees `camChanged` (line **3483**) and calls
   `WriteSceneUniforms(impostorCam)` (line **3492**).
2. In `WriteSceneUniforms` the venue-light branch is gated:
   ```
   1267: if (sVenueLightEnabled() && camNm && std::strcmp(camNm, "world.cam") == 0 && venv && venv->mAmbientFogOwner) {
   ```
   `camNm == ""` (unnamed impostor cam) → **condition FALSE** → fall through to the
   `else` at line **1352**:
   ```
   1353:  s.numLights = 1;
   1354:  s.lightDirs[0] = (-0.4,-0.5,-0.75);
   1355:  s.lightColors[0] = (1,1,1,1);            // FULL WHITE directional
   1356:  s.ambientColor  = (0.45,0.45,0.45,1);    // bright grey ambient
   ```
3. The per-environ re-write that *would* pick up `RndEnviron::sCurrent` is ALSO gated
   on `world.cam` (lines **3505-3507**), so it never fires for the impostor cam either
   — even though `RndEnviron::sCurrent` is correctly the crowd's `mEnviron`.
4. Shader (`src/gfx/standard_wgsl.inc:825-834`): non-prelit + non-unlit (UseEnv=true)
   → LIT branch: `finalColor = baseColor.rgb * softClipLighting(0.45 + N·L*(1,1,1))`.
   With full white directional + 0.45 grey ambient, a light-toned crowd skin/cloth
   texture → near-white. Baked into the impostor texture → stark white silhouettes.

**Cross-check via `RB3_LIGHT_PROBE` (logs the cam name at each WriteSceneUniforms):**
big_club_01 crowd shot — distinct cam names over the run:
```
66626  'world.cam'
16063  ''            <-- the unnamed impostor cam (huge count = many 3D crowd archetypes)
13617  'game.cam'
 5452  'overshell.cam'
 ...
```
The `''` cam is the impostor cam; 16k WriteSceneUniforms with the empty name, all
hitting the default-white branch.

**Asset ground-truth (what the crowd SHOULD get), via `RB3_VENUE_PROBE`:**
```
env=RB3_crowd_mesh.env  ambRaw=(0.18,0.18,0.18)  numApprox=0  showing=0   <- dim, ZERO lights
env=RB3_chars.env       ambRaw=(0.00,0.00,0.00)  numApprox=10 showing=10  <- band: 10 stage lights
```
The crowd's authored environ is ambient-only `0.18` grey with **no lights** → if read,
the crowd would render *dim/dark* (correct). Instead the hardcoded default gives it
`(1,1,1)` directional + `0.45` ambient → white. The 2.5× brighter ambient ALONE
(0.45 vs 0.18) plus a full white key vs zero lights is the entire delta.

---

## 4. Cross-check: small_club crowd (OK) vs big_club crowd (white) — why they differ

`RB3_LIGHT_PROBE` unnamed-cam (`''`) WriteSceneUniforms counts per run:
- **small_club_01: 90** — almost no 3D impostor crowd; the dim small venue draws the
  audience mostly as flat 2D billboard sheets (pre-shaded dark) → white% ≈ 0.
- **big_club_01: 16,063** — the big venue draws **many 3D impostor crowd characters**
  every frame → each lit by the default white branch → white% 9–18%.

So the bug is **not** unique to big_club's *environ*; it is that big_club exercises the
3D-impostor-crowd path heavily (more/larger crowd `WorldCrowd` archetypes at this LOD),
which small_club barely uses. Any venue that draws a lot of 3D impostor crowd through
`gImpostorCamera` will show the same white audience (expect arena/festival too — they
have the largest crowds; festival's `coop_crowd_mass*` is the GAP-4 stress case). This
is a single root cause, surfacing wherever the impostor crowd is dense.

---

## 5. NOT the cause (ruled out)

- **Not a masked shard / V24 guard** — present guard ON and OFF, identical white%.
- **Not the `RB3_VENUE_LIGHT` path** — A/B `RB3_VENUE_LIGHT_OFF=1` leaves crowd white%
  unchanged while the venue backdrop luma triples.
- **Not a missing/white-default texture** — the crowd diffuse samples fine; it is the
  *lighting term* that is full-bright (LIT branch × white directional + grey ambient).
- **Not a billboard atlas missing on native** — these are live 3D impostor characters
  re-rendered each frame, not a pre-baked atlas.
- **Not the band path** — the band renders correctly dark/lit (it's drawn under
  `world.cam`, which DOES get the venue path); only the impostor-cam crowd is white.

---

## 6. Relationship to GAP 2 (arena band dark) — SAME FAMILY, but DISTINCT camera

Both gaps are "a character draw is lit by the wrong source instead of its authored
`RndEnviron::sCurrent` lights." But they fail at **different cameras**:

| | GAP 2 (arena band) | GAP 3 (big_club crowd) |
|---|---|---|
| cam | `world.cam` (gets venue path) | unnamed impostor cam `''` (NEVER gets venue path) |
| symptom | too DARK (env's spots don't reach the band draw) | too WHITE (hardcoded `(1,1,1)`+`0.45`) |
| env IS sCurrent? | yes (RB3_chars.env, 10 lights) | yes (RB3_crowd_mesh.env, 0 lights, 0.18 amb) |
| likely root | venue path reads env but the band's spot geometry/direction misses the character, OR ambient too low | venue path is name-gated to `world.cam` so it is SKIPPED entirely for the impostor cam |

They are the SAME underlying design issue (character lighting must follow
`RndEnviron::sCurrent`, not be conditioned on a hardcoded cam name), so a unified fix to
the lighting path benefits both. But they are NOT one-line-identical: GAP 3 needs the
venue path to *also run* for the impostor cam; GAP 2 needs the *already-running* venue
path to deliver light that reaches the band. **My read: investigate/fix together (one
lighting-path refactor), but they have two distinct triggers.**

---

## 7. FIX HYPOTHESIS (do NOT implement — research only)

**Primary (engine, `../milo-native-engine/src/platform/Rnd_Wgpu_RB3.cpp`):** widen the
venue-light gate so the impostor-crowd cam reads `RndEnviron::sCurrent` instead of being
forced onto the hardcoded-white default.

- **`WriteSceneUniforms` line 1267** — the gate is
  `std::strcmp(camNm, "world.cam") == 0`. Change so it ALSO admits the crowd impostor
  cam. Two viable discriminators (pick whichever is safest):
  - (a) detect the impostor cam by `cam == gImpostorCamera` (or by the
    cam having a `TargetTex()` + the active `RndEnviron::sCurrent` being a crowd/char
    env), OR
  - (b) admit ANY cam that currently has a non-null `RndEnviron::sCurrent` with real
    authored ambient/lights (i.e. read the env when one is scoped, regardless of cam
    name). This is the cleaner, GAP-2-friendly direction.
- **Per-environ re-write lines 3505-3507** — same name gate; widen identically so the
  impostor cam picks up `RB3_crowd_mesh.env` when it becomes `sCurrent`.

With the gate widened, the crowd impostor would read `RB3_crowd_mesh.env`
(ambient 0.18, 0 lights) → render dim/dark (matches small_club + retail). Because the
env has **zero** lights, the `dl==0 && pl==0` fallback at lines 1337-1349 would add a
soft grey key — that key is `sVenueGreyKey()*sVenueDirExposure()` and may still be a bit
bright for a crowd; tune `sVenueGreyKey` for the crowd case or skip the fallback key when
the scoped env is a crowd env (let the 0.18 ambient alone carry it).

**Scoping / safety:** the change is `world.cam`-adjacent but must NOT alter game.cam
(highway) or menu cams. Keep it env-driven (only acts when a real `RndEnviron` is
scoped, which for the impostor cam is guaranteed by `Crowd.cpp:549`). Gate behind the
existing `RB3_VENUE_LIGHT` opt-out (and/or a new `RB3_CROWD_LIGHT_OFF`) so it can be
A/B'd. Engine change → `MILO_ENGINE_PIN` bump in `native/CMakeLists.txt` if landed.

**Verify (research handoff to impl):** A/B the same `big_club_01 coop_dir_crowd.shot`
pin — crowd white% should drop from ~9–18% toward small_club's ~0%, mean crowd luma
into the 20s, WITHOUT changing the band or the highway (game.cam byte-identical). Then
re-check arena_02/festival_01 crowds for the same improvement and confirm no menu-hub
regression.

---

## 8. Anchors

| claim | anchor |
|---|---|
| impostor cam created unnamed | `src/system/world/Crowd.cpp:88` |
| impostor mat non-prelit + useEnv | `src/system/world/Crowd.cpp:78-87` |
| crowd env set as sCurrent during draw | `src/system/world/Crowd.cpp:547-563` (RndEnvironTracker @549) |
| impostor character drawn into RT | `src/system/world/Crowd.cpp:550-555` (gImpostorCamera->Select + curChar->DrawShowing) |
| venue-light gate (name == world.cam) | `../milo-native-engine/src/platform/Rnd_Wgpu_RB3.cpp:1267` |
| default white-directional else branch | `Rnd_Wgpu_RB3.cpp:1352-1357` |
| WriteSceneUniforms call on cam change | `Rnd_Wgpu_RB3.cpp:3483-3498` |
| per-environ re-write also name-gated | `Rnd_Wgpu_RB3.cpp:3505-3511` |
| shader LIT branch (× ambient+lights) | `../milo-native-engine/src/gfx/standard_wgsl.inc:825-834` |
| crowd env is dim/lightless | `RB3_VENUE_PROBE`: `RB3_crowd_mesh.env ambRaw=(0.18,0.18,0.18) numApprox=0` |
| impostor-cam count big_club vs small_club | `RB3_LIGHT_PROBE`: `''` cam 16063 vs 90 |
| retail ground-truth (club only) | `images/retail-screenshots/yt_qRagnZCIMzk_gameplay_guitar.png` (no big_club/arena/festival frames; small_club in-engine dim crowd used as proxy) |
