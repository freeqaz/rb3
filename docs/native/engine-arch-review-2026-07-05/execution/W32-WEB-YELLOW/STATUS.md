# W32-WEB-YELLOW — STATUS

Lane A (primary). Base SHA rb3 `30546499`, engine pin `24c4f95`.
Web debug build `native/web/build/debug/rb3-web.wasm` (built 2026-07-12 05:54),
served via `native/web/server.py` on port 39555, `?debug=true` (no-store).

**VERDICT: STEP-0 COMPLETE. Divergence = mechanism (i) render-hook family →
COORDINATOR-ACK-NEEDED, STOPPED before fix per A2 + charter.**

---

## Leg (1) — QUAD NAMED (mesh + draw evidence)

**The quad is the hub focused-menu highlight bar: `highlight_main.mesh` /
`highlight_pattern.mesh`** — a SKINNED UI mesh — rendered at the world ORIGIN
(→ screen-centre, over the middle character's torso) because the per-focus
placement is never applied on web.

Draw evidence (quoted verbatim):

- Engine names the mesh + its placement mechanism,
  `../milo-native-engine/src/platform/Rnd_Wgpu_RB3.cpp:3233-3237`:
  > "The focused-menu-item yellow bar (`highlight_main.mesh` / `highlight_pattern
  > .mesh`) is a SKINNED UI mesh whose 4 corner bones carry the bar quad as their
  > LOCAL transforms (UILabel::UpdateAndDrawHighlightMesh -> botleft/topright->
  > SetLocalPos(...)), with the per-focus world PLACEMENT set on the mesh via
  > mLabelDir->SetWorldXfm(WorldXfm()) at UILabel.cpp:334."
- Engine predicts the exact symptom when the placement decision is absent,
  `Rnd_Wgpu_RB3.cpp:3240-3243`:
  > "the skinned bone palette (BoneOffsetAt * boneWorld) places the bar at the
  > ORIGIN. The skinned path normally forces obj.world=identity ... so the bar
  > renders at screen-CENTRE instead of behind the focused item."
- Only meshes named `highlight_main`/`highlight_pattern` receive the hub-bar
  policies: placement `native/src/rb3_render_hook.cpp:73-78` (`p.hubBarPlacement`)
  and colour-prelit `:235-239` (`p.isHubHighlight`).

Web repro (evidence/web/):
- `joined_00_default.png` — overshell `joined_default`, focus `mb_playnow.btn`
  ("PLAY NOW" highlighted): a solid flat yellow-green quad floats dead-centre
  over the middle character's torso/waist. The collapsed overshell "MENU" bar
  (bottom-left) is correctly positioned and the SAME yellow-green — confirming
  the orphan is the hub highlight-bar material.
- `joined_02_ArrowDown.png` — focus moved to `mb_trainers.btn` ("TRAINING"),
  scene camera rotated, **the yellow-green quad is pinned at the identical
  screen position** (screen-centre). STATIC in screen space, independent of both
  menu focus and scene camera → a bar drawn at obj.world=origin.
- Focus-travel proof (engineState, quoted): after `joined_default`,
  `STATE@ArrowDown#01: focus='mb_career.btn'`,
  `#02: focus='mb_trainers.btn'`, `ArrowUp#03: focus='mb_career.btn'`,
  `#04: focus='mb_playnow.btn'` — the REAL menu focus tracks both directions
  while the quad never moves.

## Leg (2) — WEB-ONLY MECHANISM + DIVERGENCE POINT (file:line)

**Mechanism (i): the entire GameRenderHook (`rb3_render_hook.cpp`) is NOT
compiled into the web build, so no hook is registered; the engine's per-draw
`geomPolicy` stays default (`hubBarPlacement=false`) and the highlight bar's
per-focus placement branch is never taken — it falls through to the origin
path.**

Divergence chain, each link cited:

1. **Build-list divergence (root):** `native/CMakeLists.txt:455` lists
   `src/rb3_render_hook.cpp` in the NATIVE target only; the EMSCRIPTEN branch
   explicitly filters it out — `native/CMakeLists.txt:819`:
   > "rb3_render_hook.cpp — HamRenderHook glue (registers with engine); not
   > needed for clear-frame."
   (Stale W1-era reason: the web build now boots through gameplay.)
   Confirmed empirically: `native/build-web/CMakeFiles/rb3-web.dir/src/
   rb3_render_hook.cpp.o` is ABSENT; the native equivalent is present.
2. **No registration on web:** the hook self-registers via a file-scope static
   ctor (`rb3_render_hook.cpp:381,384`) + explicit `RegisterBandRenderHook()`
   (`:390`). With the TU absent, neither runs on web, so
   `GetGameRenderHook() == nullptr`.
3. **Engine default policy:** `Rnd_Wgpu_RB3.cpp:3226-3229` —
   `DrawGeomPolicy geomPolicy;` then `if (geomHook) geomPolicy = geomHook->
   QueryDrawGeomPolicy(...)`. Null hook ⇒ `geomPolicy.hubBarPlacement` stays
   `false`.
4. **Placement branch skipped:** `Rnd_Wgpu_RB3.cpp:3257`
   `bool hubBarPlacement = skinned && geomPolicy.hubBarPlacement;` is `false`
   ⇒ the `else if (skinned && hubBarPlacement)` arm at `:3350-3354` (which sets
   `obj.world` = identity + the mesh WorldXfm TRANSLATION, i.e. the per-focus
   position) is NEVER taken. The bar takes `else if (skinned)` at `:3355`, whose
   placement-contract/identity path leaves the bar at the bone-palette ORIGIN
   (screen-centre). Static because the per-focus `SetWorldXfm` translation is
   never injected into `obj.world`.

Native is clean because on native the hook IS registered, `hubBarPlacement=true`
for these meshes, and the `:3350` branch positions the bar behind the focused
item (and tracks focus).

Secondary: the same missing hook also disables B8 colour-prelit
(`isHubHighlight`, `rb3_render_hook.cpp:239`) and every other render-hook policy
on web (B1-B13, halo, UI-text/icon glyph routing, tail colours, highway
shading, skel-rebake, mitten). The quad is visibly yellow-green regardless
(evidence), so the colour half is not load-bearing for THIS defect, but the
scope of the missing hook is far wider than the hub bar (see Fix note).

## A2 adjudication — STOP

The named divergence point is mechanism (i), the render-hook TU family
(`native/src/rb3_render_hook.cpp` + `native/CMakeLists.txt` build-list of it).
Per A2 and the lane charter ("if your divergence point lands there, checkpoint
COORDINATOR-ACK-NEEDED and stop"), this lane STOPS before implementing the fix
and hands the proposal to the coordinator for arbitration (Lane C holds
exclusive-write on `rb3_render_hook.cpp` this wave).

## Fix proposal (for arbitration — NOT applied)

Minimal, acts at the named divergence point (A10-compliant — restores the
placement mechanism, not a position/screen suppression):

1. Add `${CMAKE_SOURCE_DIR}/src/rb3_render_hook.cpp` to the web target source
   list (the `RB3_WEB_NATIVE_GLUE` block in `native/CMakeLists.txt` EMSCRIPTEN
   branch) and drop the stale line-819 exclusion note. (Owned surface:
   `native/CMakeLists.txt` web target — NOT a write to the .cpp contents.)
2. Call `RegisterBandRenderHook()` explicitly from `main_web.cpp` startup
   (owned web glue) to be robust against emcc dead-stripping the static
   auto-register ctor. Declaration already public (`:390`).
3. Verify emcc-compilability: the TU is `std::strncmp/strstr/strcmp/getenv`
   only (no POSIX sockets/backtrace/PNG) — the exclusion reason was "not needed
   for clear-frame", never a compile blocker — so it should be emcc-clean;
   confirm on a `scripts/web/build.sh --debug`.

**SCOPE FLAG for the coordinator:** adding the hook restores ALL render-hook
policies on web at once (not just the hub bar). That should move the web build
substantially closer to native, but it is a wide behavioural change to the web
renderer and it touches the render-hook family Lane C is actively editing this
wave — hence coordinator sequencing/arbitration rather than a lane-local land.

## Acceptance status
- (1) quad named + draw evidence — **DONE** (above).
- (2) web-only mechanism named w/ file:line — **DONE** (above).
- (3) after-fix web pair, (4) native control unchanged, (5) no B8 regression —
  **DEFERRED** to post-arbitration (fix not applied per A2).

## Evidence index (evidence/ gitignored)
- `evidence/web/hub_00_initial.png` — options state (flyout open, RETURN
  highlighted correctly).
- `evidence/web/joined_00_default.png` — orphan quad over torso (joined_default).
- `evidence/web/joined_01..04_*.png` — focus-travel replay both directions,
  quad static.
- Native control uidump: ATTEMPTED (`/tmp/w32y_native_control.py`) but the
  headless native boot did not reach `main_hub_screen` inside 180s (stuck at
  `splash_screen`/`intro_movie_screen` — the boot needs `RB3_GAME_INPUT` nav to
  advance past splash, which this quick control omitted; machine also loaded by
  concurrent lanes). NOT required: the mesh name is established authoritatively
  by the engine's own source comment (`Rnd_Wgpu_RB3.cpp:3233-3237`, quoted in
  Leg (1)) which literally names `highlight_main.mesh` / `highlight_pattern.mesh`
  as the skinned hub highlight bar the placement branch targets. A full native
  `/api/uidump` (which is where the actual on-screen highlight member + its
  focus-tracking world lives) can be captured by the fix lane during the
  post-arbitration leg (4) native A/B.

## COORDINATOR ARBITRATION + FIX LANDED (close-out, 2026-07-12)

A2 arbitration: Lane C finished with `rb3_render_hook.cpp` untouched (git
clean, countersigned), so the collision was moot — fix coordinator-executed.
Disposition: the TU's exclusion from the web source list was a stale W1
"clear-frame era" decision; re-adding it is a **parity restoration** (web
gains the exact policy set native already runs), no new flag (build-level,
three-tier N/A). Landed: `rb3_render_hook.cpp` added to `RB3_WEB_NATIVE_GLUE`
(self-registers at static-init; no main_web.cpp call needed) + exclusion
comment rewritten as a warning.
Verification (debug web build, `scripts/web/_w32_yellowfix_check.mjs`):
- floating yellow-green quad over the centre character: **GONE** at
  `joined_default` (was present in the lane's `joined_00_default.png` repro).
- highlight bar now draws **contained on the focused row** and TRACKS focus
  (RETURN → PLAY ON XBOX LIVE on ArrowDown, back on ArrowUp) — B8 placement
  policy live on web, acceptance legs (3)+(5) + A10 replay.
- native control (leg 4): the CMake change touches only the WEB source list;
  native gates on the merged close-out tree: drawlog-golden PASS (792 draws,
  287 known-residuals within bound), bounded boot rc=0 5/5.
- SCOPE flag adjudicated: enabling ALL policies on web is the POINT (they are
  native parity, incl. the W31 F3 glyph fix web never had); release build
  deployed at close-out. Evidence: /tmp/w32-yellowfix2/*.png re-homed to
  evidence/coordinator-fix/.
