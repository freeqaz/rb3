# W24-RECON — Debug Report (FOREARM float + CROWD hub walkers)

Date: 2026-07-09 · Lane: W24-RECON (Fable coordinator, 2 Opus sub-lanes) · Mode:
DISCOVERY/RECON ONLY — no fixes, no default flips, no pin bumps. All probe code is
`#ifdef HX_NATIVE` + env-gated, byte-inert by default. Gate:
`drawlog-golden.py --fixed-clock --canonical-order` = **PASS 792** with probe envs
unset (re-verified by the coordinator on the combined tree).

Both Wave-24 candidate bugs were driven to a **confirmed root-cause verdict**. Both
turned out to be a *different* mechanism than the Wave-23 candidate framing — and in
both cases the Wave-23 candidate is now **refuted**, exactly as the close-out
(ERRATA-C1 / ERRATA-F1) warned. The two bugs are the **same class**: a skinned mesh
whose *bind/skin is correct* but whose *driving bone is placed wrong* (IK target-space
mismatch for the band arm; undriven/unposed skeleton for the crowd) -> scrambled skin.

---

## PROBLEM 1 — FOREARM float (exploded arm/hand spike-fan)

### (1) Confirmed mechanism
The spike-fan is driven by **`CharIKHand::Poll()`** (`src/system/char/CharIKHand.cpp`)
placing the **`bone_R/L-upperArm.mesh`** bone at a grossly wrong world position during
**in-song gameplay** (`clipType=guitar_body`/`drum_body`). The forearm and hand ride the
displaced upperArm as an internally-rigid block; skin verts weighted to the flung upperArm
splay into the visible spike-fan. **It is the upperArm, not the forearm or hand** — and
it is **not** the walk-on vignette.

Decisive discriminator (adversarial kill-switch, already present at CharIKHand.cpp:27):
`RB3_NO_IK=1` collapses the in-song player0 upperArm stretch ratio from
**p50 21.2 / max 49.6 -> p50 1.000 / max 1.944**, and the arm explosion **vanishes
visually** (forearm-guitarist-IK-spikefan.png vs forearm-guitarist-NOIK.png).

The anatomical trigger (Q6.1 step 1) replaced the polluted y>50 trigger:
`authoredLen = |child.LocalXfm().v|` (rest bone length),
`liveDist = |childWorld - parentWorld|`, `ratio = liveDist/authoredLen`, detach if
`ratio > 1.5`. Validated at rest (ratio ~= 1.0). Under this correct signal, **foreArm and
hand log ZERO detach events, ever** — only `bone_R/L-upperArm.mesh` detaches. The W23
"forearm-localized" reading is a metric artifact.

### Refuted (with evidence)
- **Forearm/hand IK not constraining** (hypothesis a, literal reading): REFUTED.
  `BandIKEffector::Poll()` early-returns for `GetType()==4` (foreArm) —
  `src/system/bandobj/BandIKEffector.cpp:657` — the forearm IK effector never moves
  anything; it is only an `mMore` chain link. 0 foreArm detach events.
- **Crossed member<->clip vignette pairing** (hypothesis c, the KEY W23 lead): REFUTED as
  the driver. The crossing (player0<->player3_m, etc.) is **BY DESIGN** — slot<->player-name
  indirection: `BandWardrobe::OnEnterVignette` sets
  `mVignetteNames.names[slot]=player_names[idx]` and `FindTarget` maps "playerN"->the
  target whose name=="playerN". Vignette-driven members show only **maxRatio ~= 1.84**
  (MILD) on upperArm; the vignette is a walk-on-phase path and does **not** use
  `BandRetargetVignette`/`BandIKEffector` during the in-song `guitar_body`/`drum_body`
  explosion at all.
- **Bone mis-parenting**: REFUTED. CHAIN dump shows `inParentKids=1` at every link
  (hand->foreArm->upperArm->clavicle->spine3..pelvis->playerN) even at the exploded frame.

### Correlated evidence (same-frame, fixing the W23 "separate boots" flaw)
CHAIN dump, player0 guitarist, exploded in-song frame — coherent torso, torn-off arm:
```
pelvis  (104.0,  40.2, 48.1)   ... spine3 (105.1, 40.8, 61.8)
clavicle(105.7,  40.3, 69.1)  inParentKids=1
upperArm( 23.3,-118.3, 82.4)  inParentKids=1   <-- ~178u from its clavicle parent
foreArm ( 17.9,-125.4, 85.9)   (rigid w/ upperArm)
hand    ( 14.3,-134.7, 84.0)   (rigid w/ upperArm)
```
`IK_TGT_DBG` shows the IK hand target bones are the instrument tips
(`bone_R-tip_snare.mesh`, cymbals, toms, pedals) at **y~=105** while the IK hand is at
**y~=209** — a systematic **~100u (~=pelvis-height) vertical space mismatch** between the
animated skeleton arm and the instrument-prop target bones.
Screenshots: `evidence/forearm-guitarist-IK-spikefan.png` (spike-fan, IK on),
`evidence/forearm-guitarist-NOIK.png` (coherent, IK off),
`evidence/forearm-drummer-IK-spikefan.png`.

### Phase split (Q6.1 step 3)
ANAT detach is present on many screens where BandCharacters are polled off-stage
(main_hub 12296/max76, song_select 15536/max69, tv3_a 12996/max84,
part_difficulty 4655/max72), and in-song game_screen 1030/max48.9. The **visible**
explosion is the on-stage in-song `guitar_body`/`drum_body` regime (p50 ratio ~21, max
~49). Vignette-clip frames are mild (p50 1.8). So: **the explosion is a steady in-song IK
condition, not a walk-on/camera-cut transient** — this refines (does not match) W22's
"transition-only" framing.

### (2) Exact code sites
- `src/system/char/CharIKHand.cpp:25` `CharIKHand::Poll()` — **the driver**. Kill-switch
  `RB3_NO_IK` at :27; target read/`IK_TGT_DBG` at :44-67; `Interp(...mWorldDst)` at :141;
  `mHand->SetWorldXfm(...)` :166/:194/:201; `PullShoulder(...)` :211. Comment at :40
  references the known "hand-IK target ~300u away" pose error from prior VENUE_RENDER
  V26/V32 work.
- `src/system/bandobj/BandCharacter.cpp:3460/3475` (CharIKHand instances collected into
  `unk5d0`); `:3829` per-frame poll loop.
- `src/system/bandobj/BandIKEffector.cpp:657` — type==4 (foreArm) early-return.
- Instrument target bones `bone_target_*` under `<inst>_resource.milo` (snare/cymbal/tom/
  pedal tips) — the low-y IK targets; their space vs the skeleton arm is the open question.

### (3) DEBUG/FIX plan (for the W25 fix lane)
Root cause is inside CharIKHand's target-space, narrowed to **2 sub-hypotheses**:
- **H-A (attach-space):** the instrument-resource target bones are TransParent-attached in
  a different world/root frame than the animated member skeleton (missing/duplicated root
  translation ~pelvis height).
- **H-B (skeleton basis):** the member skeleton's arm rest/world basis is itself ~100u off
  pre-IK, so `PullShoulder`/`IKElbow` over-rotate the whole arm to reach the low target.

**Discriminator:** dump `bone_target_snare.mesh`'s TransParent chain to root vs the member
skeleton's root and compare world roots. Roots differ by ~100u -> H-A (fix the
resource-dir parenting). Roots equal but arm rest basis already ~100u high pre-IK -> H-B.
Also A/B Wii vs native via `bin/analyze-function` on `CharIKHand::Poll` / `PullShoulder`
to confirm native didn't diverge in the world-compose (not yet checked — see caveat).

**Acceptance test:** `scripts/native/_w24_forearm_capture.py --out X`, parse
`[BAND_ANIM] evt=ANAT`; in-song (game_screen) upperArm max ratio must drop from ~49 to
**< 2.0 WITH IK ON** (not via RB3_NO_IK). Visual: guitarist/drummer closeup (pin
`coop_g_cg`/`coop_d_*`) shows intact arms, no spike-fan. Gate: drawlog-golden 792.

### (4) Risk / do-NOT-touch
- `RB3_NO_IK=1` is the **discriminator only** — it removes the explosion but also removes
  correct fret/drum hand posing. Do **not** ship it as the fix.
- Do not re-open forearm binding (Wave-22 exonerated: own==bound at draw) or the
  hands-finger family (CLOSED). This is a pose/IK bug, consistent with Wave-22's
  pose-not-skin-compose finding.

### Caveat (unconfirmed)
The H-A vs H-B choice is not yet resolved (that is the W25 fix lane's job). It was **not**
confirmed on Wii/retail whether the same CharIKHand produces a correct arm there (native
divergence vs faithful-but-broken port) — the :40 comment implies this path was
under-tested on native.

---

## PROBLEM 2 — CROWD hub walkers absent (sv3_a, center-street)

### (1) Confirmed mechanism — vertex theory REFUTED
The Wave-23 "crowd body meshes load with 0 vertices" theory is **REFUTED** — it was the
ERRATA-C1 artifact exactly. The W23 census read `mVerts.size()` only; native compressed
meshes keep `mVerts` empty **by design** and draw from `mNumCompressedVerts`.

Re-census (Q6.2 step 0) with compressed fields + the `hasGeom` draw-gate, plus the
mandatory **positive control**:

| mesh | mVerts | mNumCompressedVerts | mFaces | hasGeom |
|---|---|---|---|---|
| male_crowd_body01.mesh | 0 | **1193** | 1548 | **1** |
| female_crowd_body01_lod02.mesh | 0 | **1145** | 1378 | **1** |
| props (horns/fist/clap/lighter) | 0 | 108-376 | 96-504 | **1** |

Every crowd body mesh has geometry (`comp>0 && faces>0`, `geomOwner=self`). Positive
control: 54 skinned band-outfit meshes render on the identical native path in the same
frame; zero crowd bodies appear in the drawlog. **So the meshes are NOT empty** — the bug
is downstream (Branch B).

### Redirected root cause — draw/animation-time, not load
The crowd bodies pass every load and draw gate and reach the GPU (`SubmitDraw`,
`pipe=1 nf=1378 skinned=1`, 3570 DrawMesh calls/run; not dropped by SHARD_GUARD,
ratio~=1.00). The `RB3_ISOLATE_MESH=crowd_body` control is the smoking gun: max pixel
value **17/255**; brightened 20x it is a **collapsed, scrambled dark triangle mass — not
8 standing figures**. Two compounding draw-time defects:
- **(A) Undriven skeleton (primary):** census shows `animating=0` — the crowd driver is
  present but `FirstPlayingClip()==NULL`. `streetslomo_clips.milo` loads but no clip ever
  plays -> the skin bone palette is undriven -> verts scramble within each figure. (Bind
  pose is coherent: `bindExt=79.6`, so geometry/decode is fine.)
- **(B) Near-black material** under the crowd's draw camera (`world.cam`/nameless).

`PostLoadVertices` compressed decode (`src/system/rndobj/Mesh.cpp:640-706`) is proven
correct; the `gAltRev<3` "re-export" `MILO_WARN` (`Mesh.cpp:1116-1120`) is a benign
authoring nag, **not** a read failure. (This retires the W23 `gAltRev<3` decode lead.)

Crowd chars are proxies (`isProxy=1`, `proxyFile=char/crowd/crowd_*.milo`), so
`Character::PostLoad` (`src/system/char/Character.cpp:791`) skips LOD load (`lods=0`) —
expected for proxies; the body mesh is still in `mDraws` and reaches `BandRnd::DrawMesh`.

### (2) Exact code sites
- Undriven skeleton: the sv3_a shell-vignette / crowd proxy driver never starts the
  `streetslomo` clip. Trace where the hub vignette is supposed to trigger the crowd
  driver's clip (`src/band3/` shell-vignette or a native-src `CharDriver::Poll` shim) —
  census func at `native/src/rb3_http_handlers.cpp` (`RB3DtaCrowdCensus`) reports
  `animating=0 / clip=-`.
- Near-black material: crowd draw under `world.cam`/nameless camera in
  `../milo-native-engine/src/platform/Rnd_Wgpu_RB3.cpp` (SubmitDraw path ~:5595).
- NOT the loader (`Mesh.cpp` compressed path) — proven correct.

### (3) DEBUG/FIX plan (branch-scoped)
- **Primary:** find why the sv3_a vignette never starts the `streetslomo` clip on the
  crowd proxy drivers (anim-trigger/flow gap). Fix scoped to the crowd-proxy driver /
  hub-vignette path only.
- **Secondary:** if a near-black material persists after posing, an env-scoped lighting
  fix for the crowd draw camera.
- **Do NOT** touch the RndMesh loader (unnecessary — decode is correct; blast radius =
  every skinned mesh) or the WorldCrowd/RndMultiMesh gameplay path.

**Discriminator:** `{rb3_crowd_census}` -> `animating > 0`; `RB3_ISOLATE_MESH=crowd_body`
capture -> 8 lit standing figures (not a scrambled mass).

**Mandatory acceptance test (WorldCrowd A/B):** the gameplay crowd renders via the
disjoint `RndMultiMesh` instancing path (`WorldCrowd::CharData::mMMesh`) — run gameplay
drawlog crowd-draw counts + screenshot SSIM before/after the fix and confirm **unchanged**
(the fix must not perturb the protected oracle at `src/system/world/Crowd.cpp:884-1000`);
plus the hub center-street walkers must appear.

### (4) Risk / do-NOT-touch
- Do not edit `src/system/world/Crowd.cpp:884-1000` or the `RndMultiMesh` gameplay hot
  path (protected oracle). The correct diagnosis (draw-time, not loader) means the shared
  loader is untouched, so blast radius is contained to the crowd-proxy driver / crowd
  camera.
- Symptom is partly camera-angle-dependent (native sits on a band-face close-up shot vs
  retail down-street) — verify fixes on the camera-independent `RB3_ISOLATE_MESH` capture,
  not just the default hub framing.

### Caveat (unconfirmed)
Defect (A) undriven-skeleton is the primary and best-evidenced cause (`animating=0` +
scrambled isolate capture). Whether (B) near-black material is an independent second
defect or merely a consequence of the crowd camera not being reached is not fully
separated — resolve after posing is fixed.

---

## Files touched (all probe-only, inert by default)

Committed by the coordinator (this report + evidence + inert probes + new scripts):
- `src/system/bandobj/BandCharacter.cpp` — ANAT + CHAIN probes (`BAND_ANIM_ANAT`,
  `BAND_ANIM_CHAIN`, `BAND_ANIM_CHAIN_HZ`).
- `src/system/char/Character.cpp` — `CROWD_DRAW_DBG` probe.
- `native/src/rb3_http_handlers.cpp` — `RB3DtaCrowdCensus` compressed-field + positive
  control (`CROWD_POSCTRL`, `CROWD_CENSUS_DRAWS`).
- `scripts/native/_w24_forearm_capture.py`, `scripts/native/_w24_crowd_recon.py`.
- `docs/native/.../W24-RECON/{REPORT.md, evidence/*}`.

Engine (committed separately in ../milo-native-engine): `src/platform/Rnd_Wgpu_RB3.cpp` —
`CROWD_SUBMIT_DBG` probe (inert unless env set).

**NOT staged** (pre-existing concurrent-agent edits, forbidden per lane guardrails):
`native/src/rb3_session_trace.cpp`, `../milo-native-engine/src/platform/FxSendNative.cpp`.

Gate (coordinator-verified on combined tree, probe envs unset):
`drawlog-golden --fixed-clock --canonical-order` = **PASS 792**.
