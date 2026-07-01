# T-web — web band-character FACE capture (2026-07-01)

**Question this resolves:** user reports "characters render WITHOUT their faces
on **web**." Native (this session, FINDINGS.md) shows faces *present* (flat/
over-bright). Is the web symptom web-specific, or the same shared-engine C8 issue?

## Verdict — NOT web-specific. Same C8 broken-face-skin symptom.

On the **current deployed release build** (`native/web/build/release`, served at
`http://localhost:8421/`, wasm dated 2026-07-01 16:53), driven to **in-song
gameplay** (`game_screen`, "25 or 6 to 4", blues-club/tavern venue):

- **Faces are PRESENT geometrically, but BROKEN.** Every band member renders a
  full head (cap, hair, eyes, teeth) — nothing is missing. But the **face SKIN
  renders as a flat, wrongly-shaded blank surface with NO skin tone / brows /
  lips / texture**, while the **rigid eye + teeth meshes glow bright white on
  top** as "googly eyes." This is *exactly* the C8 symptom PLAN.md describes:
  "Face SKIN renders dark/black; the rigid eyes + teeth (NumBones()==0) glow
  bright on top."
- The face-skin color tracks the venue lighting: **grey-teal blank** on the
  vocalist, **solid green blank** on the front character, **near-black** on
  members in shadow, **blown-out white plastic** in a bright closeup — i.e. the
  skin catches light wrong (the dark-normals mechanism), never a proper face.
- **This matches native, not diverges from it.** Web compiles the same `src/` +
  engine, so it reproduces the identical broken-face behavior. It is **not** a
  web-only texture/asset-load gap.

The user's "without faces on web" is an accurate description of the *appearance*
(blank/wrong face skin + floating glowing eyes = reads as faceless). The heads
are not geometrically absent. Web looks *more* faceless than native's reference
only because the web auto-director gave dim/back-lit/tinted angles + a magenta
wash; native's reference was a FORCED, warmer vocalist closeup. Same bug.

## Evidence (screenshots, all under `/tmp/web-face/`)

| file | what it shows |
|---|---|
| `g_011s_5.png` | **best frame.** Female vocalist = grey-teal blank face + pale googly eyes; front char (green cap) = solid-green blank face + 2 bright bulging white eyes; left guitarist = near-black faceless head |
| `g_009s_4.png` | vocalist torso closeup — skin flat/over-bright plasticky pink-white, no skin detail; teeth glow bright white (face cut off top) |
| `g_005s_2.png`, `g_007s_3.png` | capped guitarist, face = dark blank (+ magenta wash) |
| `g_020s_9.png`, `g_018s_8.png` | wide 5-member band shots — all members dark grey/faceless silhouettes |
| `face_web_vocalist.png`, `face_web_frontchar.png` | 4× zoomed web faces (the two crops above) |
| `CMP_native_vs_web_faces.png` | **side-by-side:** native forced vocalist (orange face, eyes+mouth, reads as a FACE) \| web vocalist (grey-teal blank + googly eyes) \| web front-char (green blank + bright eyes) |
| `flow.json`, `console.log` | per-frame brightness stats + browser console |

Native reference (for comparison): `/tmp/bc-voc/voc_coop_front_n00_0.png` (full
frame), `/tmp/voc-face-n00.png` (native face crop). Native face = over-bright
orange, but eyes + open mouth + blonde pigtails visible → reads as a face.

## How it was captured

- Harness: `scripts/web/_web_face_capture.mjs` (scratch, `_`-prefixed like the
  other web harnesses). Boots headless Chromium (Playwright, WebGPU/ANGLE-Vulkan,
  no xvfb) against the **release** build, sets `window.rb3WebUseAids=1` +
  `rb3WebTargetSong='20thcenturyboy'`, keyboard-navigates
  splash→hub→song_select→part_difficulty→**game_screen**, then grabs a
  full-canvas PNG every 2 s for 60 s.
- Reached `game_screen` and confirmed in-song rendering. **Caveat:** aids autohit
  did not keep the song alive (no web `nofail` verb path) → **SONG FAILED at ~3 %
  (~t≈23 s)**, so the useful gameplay window is `g_000s`–`g_023s`; later frames
  are the fail-menu overlay. This was enough — the auto-director cut to band-
  member framings within the first ~11 s (`g_005`, `g_007`, `g_009`, `g_011`).
- I could **not** force a matched vocalist front-closeup on web (the native
  `{rb3_force_shot}` / `{rb3_director_disable}` DTA hooks are native-HTTP-only;
  the web build exposes no camera control), so the native↔web comparison is
  same-venue/same-song but different director shots + lighting. The *mechanism*
  (broken face-skin shading + over-bright rigid eyes) is unambiguously the same.

## Bottom line for the campaign

Confirms FINDINGS.md §"Consequence": the C8 face issue is **shared-engine, not
web-specific**. Fix belongs in `../milo-native-engine` (skinned-normal transform
/ face-skin shading), gated DC3-safe, then verified in both native and web. No
web-only work needed to address the reported symptom.
