# W2.7 — Singer flat-black head (Lane D, Opus) — STATUS

## 2026-07-06 — D.S1 characterization + fix (Opus)

**Verdict: ROOT-CAUSED + FIXED (rb3 game code, within fence). Fix landed default-ON,
opt-out `RB3_BLACK_HEAD_FIX_OFF`. Deterministic census A/B + lineup gate PASS.**

### Symptom
`/tmp/wave6-current-state/gameplay_default_2.png` (songMs≈20153): a band member's
head renders **solid flat black** while the eye submesh, hair, and the fully-textured
body/arms render normally. Reproduces flag-ON/OFF for `RB3_PLACEMENT_CONTRACT`
(placement-independent, as the manifest noted). This is the C8-faces family — but a
**distinct, un-fixed cause** from the four in `docs/native/c8-ground-truth-2026-07-01/`
(skin RTT composite / freed head.mesh / composite blend lerp / char-env lights).

### Reproduction harness
- Deterministic member closeups via `scripts/native/band-closeup-capture.py`
  (`rb3_director_disable`+`rb3_force_shot` pin the venue camera per band member).
  Env `RB3_HEADMAT_DBG=1` gives a one-shot-per-mesh census of every DrawMesh's
  material state (`RB3MaterialBinder.cpp:166`, engine).
- The venue/song pick is **wall-clock-seeded and non-deterministic across boots**
  even under `RB3_FIXED_CLOCK=1` (which only freezes the sim clock). The manifest
  boot landed the dark practice-space venue (→ black head); my boots landed the
  warm club venue (→ **pink** head — same null-albedo cause, different venue light).
  Because of this the *census* (below), not eyeballed frames, is the load-bearing
  proof; the head defect is venue-light-modulated but the root cause is venue-independent.

### Root cause (deterministic — head census matrix)
The band **`head.mesh` (`dir='outfit'`) samples `diffuse='<null>' hasTex=0`**, unlike:
- crowd `female_extra_head.mesh` → `female_extras_head_naked_diff.tex` (hasTex=1)
- band `hands_naked.mesh`/`torso_naked.mat` → `dummy_torso.tex` (hasTex=1)
- band `feet_socks_skin.mat` → `dummy_feet.tex` (hasTex=1)

Flag matrix (single vocalist closeup, `head.mesh dir='outfit'` census line):
| flags | head_naked.mat diffuse | hasTex | isRT |
|---|---|---|---|
| default | `<null>` | 0 | 0 |
| `RB3_SKIN_FIX_OFF=1` | `<null>` | 0 | 0 |
| `RB3_SKIN_RTT=1` | `head_skin_diffuse_output.tex` | **0** | 1 (unpainted RT) |

Mechanism, verified in source:
1. The default native skin path (`OutfitConfig::SetSkinTextures(dir1,dir2,desc)`,
   `src/system/bandobj/OutfitConfig.cpp`) binds each skin part's source detail
   texture with `dir1->Find<RndTex>("<gender>_<part>_diff.tex", false)`. The bool is
   **`parentDirs`, not recurse-into-subdirs** (`obj/Dir.h:327`).
2. On the matched **Wii** build the character milo flat-merges the head textures into
   `dir1`, so the `Find` hits. On **native** the milo merge keeps them in a nested
   subdir — `char/main/head/<gender>/gen/head.milo_xbox` holds `male_head_diff.tex`,
   `female_head_diff.tex`, `head_skin_diffuse_output.tex`, **and** `head_naked.mat`.
   `dir1->Find` (non-recursive) can reach none of them.
3. So the direct-bind loop leaves **every** skin material on its authored diffuse.
   torso/legs/feet have a valid `dummy_*` flesh fallback → they still render. The
   head's authored `head_naked.mat` diffuse is **NULL** and there is **no
   `dummy_head.tex`** anywhere in the asset tree (verified).
4. A null-albedo, `useEnviron=1`, `color=(0.91,0.87,0.70)` head shades as flat lit
   material colour: **near-black under a dim venue** (the manifest practice-space),
   **pink under a warm venue** (club). Body/arms/eyes are unaffected because they
   carry real textures.
5. Additional native quirk (why the first two fix attempts missed): `head_naked.mat`
   is **split into ~6 distinct instances** by the native merge, and the drawn
   `head.mesh` lives in the **outfit dir (`dir2`)**, not `dir1`. The `dir1->Find`
   copy is not the instance the mesh samples.

This is a **bug** vs retail (a black/flat-untextured face is never intended; Wii
faces are dark-but-**textured** rim-lit skin — `c8-ground-truth .../RESOLUTION.md`).

### Fix (rb3 game code — WITHIN FENCE)
`src/system/bandobj/OutfitConfig.cpp`, new `#ifdef HX_NATIVE` block at end of the
3-arg `SetSkinTextures` (~:564). Resolves `<gender>_head_diff.tex` **recursively**
(`ObjDirItr<RndTex>` over dir1 then dir2 — matching the Wii-flattened reach), then
binds it to the material of **every drawn `head.mesh`** (scanning both dir trees)
**whose diffuse is still null**. Head-only + null-diffuse-only, so the working
torso/legs/feet skin is byte-untouched. Skipped under `RB3_SKIN_RTT` (that path
binds+paints the composite RT instead) and behind opt-out `RB3_BLACK_HEAD_FIX_OFF`.

- **Fence compliance:** lives entirely in rb3 game code; `Rnd_Wgpu_RB3.cpp` (Lane-A
  FORBIDDEN) and `rb3_render_hook.cpp` (shared-risk) **untouched**.
- **Wii byte-identical:** whole change is `#ifdef HX_NATIVE`; the MWCC build (HX_NATIVE
  undefined) compiles nothing new.

### Verification
- **Deterministic census A/B** (`head.mesh dir='outfit'`):
  - OFF (`RB3_BLACK_HEAD_FIX_OFF=1`): `diffuse='<null>' hasTex=0`
  - ON  (default): `diffuse='male_head_diff.tex' hasTex=1`  ← fix confirmed
- **Visual** (`captures/`): OFF = flat, featureless face skin
  (`11_off_flat_face_crop.png`); ON = textured, shaded face (`21_on_textured_face_crop.png`).
  Matched-frame A/B is confounded by the wall-clock venue/exposure variance + the
  separate Lane-A pink-bloom wash, so the census is the primary proof.
- **Lineup / shard gate:** every `band-closeup-capture.py` run returned
  `verdict=PASS` (0 band drops, 0 band-ratio regression) — no geometry/shard
  regression from the fix.
- **Fail-red:** `RB3_BLACK_HEAD_FIX_OFF=1` restores the null-diffuse head (census +
  flat face), i.e. the opt-out reproduces the bug on demand.

### Flag registered
`RB3_BLACK_HEAD_FIX_OFF` added to engine
`NativeCompatFlags.classification.json` (flock, append-only, class=workaround,
default=on). No `gen.inc` regen (coordinator's single wave-end regen).

### Commits
- rb3: `src/system/bandobj/OutfitConfig.cpp` — the fix.
- engine (milo-native-engine): `NativeCompatFlags.classification.json` — flag row.

### Notes / follow-ups for coordinator
- The direct-bind path (266ffb1b) is effectively a **no-op on this asset config**
  for ALL skin parts (curtex `Find` never hits); torso/legs/feet only look right
  because of their authored `dummy_*` fallbacks. Applying the same recursive-find
  recovery to torso/legs/feet would bind their *real* detail textures
  (`<gender>_torso_diff` etc.) — potentially a fidelity **improvement** (more skin
  detail than the flat dummy), but it CHANGES currently-working parts, so it was
  deliberately left out of this minimal head-only fix. Candidate Wave-7 item.
- The `RB3_SKIN_RTT` composite RT (`head_skin_diffuse_output.tex`) is created but
  **never painted** (hasTex=0) in the current build — a separate, pre-existing
  engine-side composite gap (opt-in path only; not shipped). Not addressed here.
