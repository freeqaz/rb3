# W5.1 — Venue black poster quads (Lane C, Wave 8) — STATUS

## 2026-07-06 — C.S1 probe census (Opus) — done

**Verdict: the "black poster quads" are NOT a missing-texture / null-diffuse bug of ANY
family (NOT W2.7, NOT a texture-bind/alpha failure). The reference central "SHOW … ALL AGES"
poster is an AUTHORED-DARK textured concert poster that binds and samples its texture
correctly. The other dark venue quads are AUTHORED solid-color/black materials rendering as
designed. → Outcome (b): NO in-fence game/asset fix exists; CLOSE as not-a-missing-texture-bug
(backlog a single optional Wii/Dolphin ground-truth spot-check to confirm authored-dark vs
a subtle lighting-fidelity nuance whose only lever is the FORBIDDEN render backend).**

No source changed (pure zero-new-code probe, per WAVE8_REVIEW A6).

### What the reference actually is (corrects the Wave-6 framing)

`/tmp/wave6-current-state/partdiff_default.png` (Wave-6 frame 390, flagged "ANOMALOUS … large
solid black rectangular patches") is a **mid-transition frame of the `tv3_a` transition
vignette** — the "traveling into the venue" cork-board flythrough
(`orig-assets/extracted/world/vignette/transition/gen/tv3_a.milo_xbox`), reached as its own
`tv3_a_screen` during the part_difficulty → gameplay load. This confirms W4.1's Wave-6 finding
("frame-390 was a mid `part:guitar` camera transition; widgets render fine at settle"). The
cork-board scene is NOT the gameplay venue and NOT the shell — it is a transition vignette.
Reproduced identically on the current pin `a94762f`: `census/tv3a_corkboard_black_poster.png`.

(Separately, the current native quickplay flow always shows the **downtown-street meta shell**
for part_difficulty at settle — subway/bar/Jupiter-Club/Baboon-Nest — not a random gameplay
venue: `SelectRandomVenue`/`SetVenue` don't run in this flow, `BandDirector.cpp:642`. So the
"part_difficulty backdrop" and "gameplay set-dressing" black quads are the same asset family,
censused below.)

### Census method (zero new code)

`RB3_HEADMAT_DBG=1` (engine `RB3MaterialBinder.cpp:230`) emits one `[HEADMAT]` line per drawn
material: `diffuse='<name|null>' hasTex=<0|1> isRT=<0|1> blend=… color=(r,g,b,a) prelit=…
useEnviron=…`. Discriminator:
- `diffuse='<null>' hasTex=0` = material has **no diffuse-texture pointer** (authored solid
  color, OR the W2.7 null-diffuse family).
- `diffuse='<name>' hasTex=0` = texture **referenced but failed to bind** (upload/alpha family).
- `diffuse='<name>' hasTex=1` = texture bound and sampling.

Censused: the `tv3_a` vignette live (`census/vignette.py`, `census/tv3a_vignette_census.txt`),
the part_difficulty shell across 8 songs + 5 boots (`census/census.py`) — **546 distinct shell
meshes + 34 vignette poster/menu/flyer meshes**, `RB3_PP_LUMA_CEILING` unset in every arm (A7).

### Census table — the reference cork-board (`tv3_a`) poster/decal quads

| quad (mesh) | material | diffuse | hasTex | color | class |
|---|---|---|---|---|---|
| **showtonight_poster** (the "SHOW ALL AGES" central poster) | showtonight_poster.mat | **show_tonight_poster_01.tex** | **1** | (1,1,1) | **authored-dark texture, BINDS + samples** |
| corkboard | corkboard.mat | corkboard.tex | 1 | (1,1,1) | textured, lit fine |
| corkboard_frame2 / corkboardy_frame | corkboard_frame.mat | corkboard_frame.tex | 1 | (1,1,1) | textured |
| corkboard_02 | transit_schedule_backing.mat | bus_schedule_backing.tex | 1 | (1,1,1) | textured |
| menu_02 / menu_03 / menu_07 | menu.mat | menu.tex | 1 | (1,1,1) | textured menus (readable) |
| restaurant_sign | alitalia_menu.mat | alitalia_menu.tex | 1 | (1,1,1) | textured |
| bill_on_pole_05..13 | flyer_04..10.mat | flyer_04..10.tex | 1 | (1,1,1) | textured flyers |
| tape_for_poster01/02 | tape_for_poster.mat | tape_for_poster.tex | 1 | (1,1,1) | textured (the yellow tape/banner) |
| menu_01 / menu_04 / menu_05 / menu_06 | menu_01/04/05/06.mat | `<null>` | 0 | (0.88,0.34,0.34) | **authored solid salmon** (no tex in asset) |
| corkboard_frame_02 | corkboard_frame_02.mat | `<null>` | 0 | (0.00,0.22,0.41) | **authored solid dark-blue** |
| bill_on_pole_03 | bill_on_pole_03.mat | `<null>` | 0 | (0.65,0.90,0.90) | **authored solid cyan** |
| glass_for_corkboardw | glass.mat | glass.tex | 1 | (0,0,0,a=1) blend=3 | glass reflection overlay (additive) |

### Census table — shell backdrop dark/null quads (representative)

| quad family | material | diffuse | color | class |
|---|---|---|---|---|
| skyscrapers_01, lightpole_A*, fence, bike_lock, trash_bag* | black.mat / gloss_black.mat | `<null>` | (0,0,0) | **authored night-silhouette black** (correct) |
| bar_sign_backing, hotel_sign_hanger | background_black.mat | `<null>` | (0,0,0) | **authored sign backing** |
| lightpole_B* | red.mat | `<null>` | (0,0,0) | authored (dark red pole) |
| park_neon*, capitolsign_neon, red/yellow/white/purple_neon, palace_*, tiki_* | *_neon / colored | `<null>` | authored RGB, **useEnviron=0 (unlit full-bright)** | **authored neon/logo, correct** |
| advert_02 | default_gray.mat | `<null>` | (0.59,0.59,0.59) | authored grey placeholder |
| billboard_frame_new | dark_gray01.mat | `<null>` | (0.20,0.20,0.20) | authored dark-grey frame |
| advert_01, adboard_01, paper_a/b/c/e, newspaper_01, hotel_sign, big_sign_music, sign_*, neonsigns_*, capitolsign | (real) | real .tex | (1,1,1) | **all textured, bind fine** |

### The decisive discriminator — ZERO poster/decal texture-bind failures

Across the ENTIRE census (546 shell + 34 vignette meshes), the ONLY `diffuse='<name>' hasTex=0`
(referenced-but-failed-to-bind) cases are:
- `clouds_rnd.tex` on `sky_dome`/`sky_dome02` — **isRT=1** (unpainted cloud render-target; separate)
- `head_skin_diffuse_output.tex` on `tongue` — **isRT=1** (the RB3_SKIN_RTT skin-composite RT; the
  pre-existing gap already noted in W2.7)

**Not one poster / menu / flyer / decal / sign quad exhibits a texture-bind failure.** Every
authored-texture poster binds (hasTex=1). Every "black" quad is either an authored solid-color/
black material with no texture pointer, or (the central poster) an authored-dark texture that
binds and samples — its faint embossed "SHOW"/"ALL AGES" outline text is visible IN the black
field, which a bind failure (uniform flat black) could not produce.

### Family verdict

1. **NOT the W2.7 null-diffuse family.** W2.7's null-diffuse was caused by
   `OutfitConfig::SetSkinTextures`'s non-recursive `dir1->Find` on the **character-skin** path
   (`OutfitConfig.cpp`). Venue/vignette milos load their textures directly at milo-load and
   **never pass through SetSkinTextures** — so that mechanism is architecturally inapplicable
   here. Confirmed empirically: no venue/vignette poster is null-diffuse-that-should-be-textured;
   the null-diffuse quads have no texture in the asset at all (authored solid color).
2. **NOT a texture-bind/upload/alpha failure** (0/580 poster quads; the only fails are isRT RTs).
3. **The "black poster" = an authored-dark concert-poster texture** (`show_tonight_poster_01.tex`,
   binds + samples) with an authored yellow banner top/bottom and a black body with dark outline
   text — a normal concert-flyer design that reads "anomalous" only by contrast with the tan
   cork-board menus around it during a fast camera flythrough.
4. **The other dark quads = authored solid-color/black materials** (night silhouettes via
   black.mat/gloss_black.mat; solid salmon/blue/cyan placeholder menus; grey `default_gray`/
   `dark_gray01` frames; unlit neon). Rendering as designed.

### Outcome — (b) backlog / close with diagnosis (NO C.S2 handoff)

There is **no in-fence game/asset fix** to hand to C.S2:
- Nothing is null-diffuse-recoverable (no missing texture exists in the asset to bind — the W2.7
  recursive-find generalization A6 anticipated has nothing to recover).
- The central poster already binds its texture; it is authored-dark.
- The ONLY conceivable "make it brighter" levers are (i) the material unlit/`useEnviron` handling
  (`RB3MaterialBinder.cpp` — **FORBIDDEN**, Lane-A-adjacent) or (ii) vignette scene lighting
  (`Rnd_Wgpu_RB3.cpp` — **FORBIDDEN** Lane A). Per A6 that is a Wave-9 staged patch, and only IF a
  ground-truth comparison proves the poster is *under-lit* rather than *authored-dark*.

**Recommendation to coordinator:** CLOSE W5.1 as **not-a-missing-texture-bug** (the SYS-5 "black
poster" is authored-dark content rendering faithfully; the census rules out every texture/bind
cause). File ONE optional low-priority Wave-9 backlog item: a Wii/Dolphin ground-truth spot-check
of the `tv3_a` cork-board poster; only if retail shows a *bright* poster does a lighting/unlit-bit
fix (in the forbidden render backend) become warranted. Do NOT spend a Lane-C fix cycle chasing an
in-fence fix — there is no texture to recover.

### Artifacts

- `census/tv3a_corkboard_black_poster.png` — the reference reproduced on pin `a94762f`.
- `census/partdiff_shell_subway.png` — the current part_difficulty settle shell.
- `census/tv3a_vignette_census.txt` — full `[HEADMAT]` census of the vignette poster/menu wall.
- `census/census.py`, `census/vignette.py` — the harnesses (regenerable; raw logs in `/tmp/w51-census/`).

### Fence — RESPECTED

C.S1 changed **no source** (probe-only). Only `W5.1/**` docs + `census/**` artifacts written.
No engine files, no forbidden files, no flags, no default flips.

## 2026-07-06 — C.S2 closure (Sonnet) — done (no-code-needed)

**Per the C.S2 dispatch instructions:** if C.S1's verdict is anything other than a confirmed
W2.7-family null-diffuse bug with an in-fence fix design, do not force a fix — write the
documentation/staged-patch outcome C.S1 specified and mark status honestly. C.S1's verdict was
**authored-dark + authored-solid-color, NOT W2.7-family, NO in-fence fix** — that is exactly the
"anything else" branch, so this stage does no source work.

**Action taken:**
1. Confirmed C.S1's census is internally consistent and its "no in-fence fix" conclusion holds
   (re-read the full census table + discriminator argument above; the poster's `hasTex=1` +
   visible embossed text through the dark field is the load-bearing piece of evidence and it's
   sound — a bind failure cannot produce readable-through-black text).
2. Filed the one optional Wave-9 follow-up C.S1 recommended:
   `W5.1/BACKLOG-tv3a-poster-groundtruth-spotcheck.md` — a Wii/Dolphin ground-truth spot-check of
   the `tv3_a` cork-board poster's brightness, explicitly scoped as low-priority/optional, with no
   presumption a bug exists (the census already rules out every fixable cause; this is a pure
   authored-vs-lighting-fidelity tiebreak, and only escalates to a fresh staged patch — outside
   every current lane's fence — if retail is shown to be materially brighter).
3. Superseded the ancestor `W4.1/BACKLOG-partdiff-venue-poster.md` handoff (which had routed this
   to "the Lane D venue family" for further diagnosis): that diagnosis is now complete via C.S1's
   census, so no further routing is needed. Left `W4.1`'s file untouched (historical record,
   another lane's directory) — this STATUS.md entry + the new backlog file are the terminal
   record for the trail SYS-5 → W4.1 → W5.1.

**No code changed. No flags added. No gates run** (the C.S2 gate list in `WAVE8_KICKOFF.md`
— census A/B, visual A/B, lineup, fail-red — applies to a fix under test; there is no fix here,
so those gates are inapplicable, not skipped-and-failing).

**Outcome: no-code-needed.** Recommend the coordinator CLOSE W5.1/SYS-5 for Wave 8, carrying only
the optional Wave-9 backlog item forward (see task #27 close-out list: "W5.1 quads" resolves to
"closed, no fix; optional ground-truth spot-check filed").

### Fence — RESPECTED (C.S2)

No source touched. Files written this stage: this STATUS.md section,
`W5.1/BACKLOG-tv3a-poster-groundtruth-spotcheck.md`. No engine files, no forbidden files
(`BandCharacter.cpp`, `RB3MaterialBinder.cpp` untouched by this lane), no flags, no default flips.
