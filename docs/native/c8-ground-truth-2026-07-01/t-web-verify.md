# T-web-verify — verify the flesh-skin fix on the WEB build (2026-07-01)

**Question:** Does rb3 `372baf7b` ("composite band-character flesh-skin textures")
actually reach and work on the **web** build? Pre-fix, web showed band members
"without faces" — blank/flat-grey face skin + bare grey arms/legs (see
`t-web-face.md`).

## VERDICT — FIX CONFIRMED on web. Exposed skin now renders TEXTURED (flesh tone).

Driven to in-song gameplay on the **debug** build (`http://localhost:8421/?debug=true`,
wasm dated 2026-07-01 **20:05:23** — *after* the fix commit at 20:02:25, so it
genuinely contains `372baf7b`; release wasm is 16:53 = pre-fix). Song "25 or 6 to 4",
Chicago, blues-club/tavern venue (CORCAIGH CORK). Reached `game_screen`, auto-director
cut to band-member framings before the ~44 s aids-fail window.

**Post-fix:** band members' exposed skin — **arms, hands, and (where the director
shows a lit face) faces — render with actual warm flesh tones**, with clear
**per-character skin-tone variation** (light-tan hand next to a darker-brown arm
in the *same* shot under the *same* light). Per-character variation under one light
can only come from the composited skin **diffuse texture** — i.e. the fix's
`MatSwap::Compose` into `*_skin_diffuse_output.tex` is firing on web.

**Pre-fix (release):** the same exposed skin was a **uniform pale grey / lavender-
white blank** with no per-character tone — a mannequin look. Critically this held
**even under WARM orange venue light** (`PREFIX_g018_skincrop.png`), proving the grey
skin was a *missing diffuse*, not a cool-light artifact. That was the confound-killer.

### Confound ruled out
The venue lighting cycles warm/cool, so "post-fix looks warmer" alone could be a
lighting difference. Two independent facts rule that out:
1. **Per-character skin variation under one light** in the post-fix shots (tan hand +
   darker brown arm together) — a texture signature, not a lighting one.
2. **Pre-fix skin stayed grey/pale even under warm orange light** (`g_018`), so the
   pre-fix grey was not "cool light on textured skin."

## Not addressed by THIS fix (known, separate follow-ups — expected to still be off)
- **Glowing/bright rigid eyes** — still present (eye-material self-illumination),
  called out in RESOLUTION.md as a separate item.
- **Flat / over-bright face shading** and **normal/wrinkle-map surface detail**
  (`RndTexBlender::DrawShowing` stub) — faces are textured but not fully shaded/
  detailed. Out of scope for the flesh-diffuse fix.
- The auto-director favored down/side/shadowed face angles in this dark tavern, so a
  perfectly front-lit hero face wasn't captured — but the flesh-diffuse material
  family (proven textured on arms/hands) is the same one the face skin samples.

## Key screenshots (all absolute paths)
Post-fix (debug + fix), `/tmp/web-verify/`:
- `FACE_g017_frontsinger_arm.png` — **best proof:** hands + arms in distinct warm
  tan / darker-brown flesh tones (per-character), under one light.
- `FACE_g019_bluehair.png` — raised hand/arm in flesh tone; `FACE_g019_afrovoc.png` —
  lit vocalist face with open singing mouth (features present, not a flat blob).
- `BR_g_017s_7.png`, `BR_g_019s_8.png` — full brightened band shots.
- `g_003s_1.png`, `g_005s_2.png`, `g_007s_3.png` — raw full frames.

Pre-fix contrast (release, no fix), `/tmp/web-verify/`:
- `PREFIX_g018_skincrop.png` — **confound-killer:** warm-lit band, skin still uniform
  pale grey/lavender blank.
- `PREFIX_BR_g_011s_5.png` — brightened band, all limbs flat grey/white.

Side-by-side:
- `CMP_skin_prefix_vs_postfix.png` — pre-fix grey-blank arms | post-fix flesh-tone arms.

Original pre-fix evidence set: `/tmp/web-face/` (from `t-web-face.md`).

## How captured
`scripts/web/_web_face_capture.mjs --debug --port 8421 --song 20thcenturyboy
--out /tmp/web-verify --play-seconds 44 --interval 2000` (headless Chromium,
WebGPU/ANGLE-Vulkan, aids autohit). Reached `game_screen` at ~100 s; 18 gameplay
frames. Server (`native/web/server.py`) already running on 8421 from the repo cwd;
debug wasm served `no-store` (fresh fix build, not the cached pre-fix release).
Crops/brightening: PIL (source not modified).
</content>
</invoke>
