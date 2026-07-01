# task-hub-bar-placement — the hub focused-item highlight BAR is mispositioned (FIXED)

Follow-up to `task-hub-enlarge-impl.md` (which fixed Defect 1, the bar's colour, and
diagnosed but DEFERRED Defect 2, the bar's placement). This is **Defect 2 — DONE**.

**STATUS: done — root-caused, fixed, verified. VERIFIED: yes (headless before/after,
≥2 focus positions + opt-out negative control). Wii byte-identical (no rb3 src
touched — engine-native-backend only). DC3-inert (RB3-only file).**

- engine worktree `wt-task-hub-bar-placement`
  (`/home/free/code/milohax/milo-native-engine-worktrees/task-hub-bar-placement`),
  based on pin `ce8ecda`:
  - `925cc29` — Defect 1 (cherry-picked from `wt-hub-highlight-fix` `53d31cf`):
    draw the bar at its register/yellow colour (not lit→black).
  - `81c9dcc` — Defect 2 (this task): place the bar behind the focused item.
- rb3: **no `src/` change** — only this doc.

---

## TL;DR (the surprising result, refines the task-hub-enlarge diagnosis)

The task-hub-enlarge doc said the corner bones "CORRECTLY track the focused label's
world position (-376.8,0,104.4)" — measured at the *Milo* instrumentation site
(`UILabel::UpdateAndDrawHighlightMesh`). **At DRAW time in the renderer, that is NOT
what the bones hold.** The highlight bar is a **skinned** mesh, and the bone palette
the renderer actually reads places the bar at the **ORIGIN**, not the label:

```
[HUB_BAR_DRAW] mesh='highlight_main.mesh' skinned=1 numBones(owner)=4
               meshWorld.v=(-376.83, 2.50, 104.65)   <- CORRECT (the focused label)
[HUB_BAR] b=0 bone='bone_bottom_left.mesh' parent='pentatonic_display'(w=0.0,10.0,0.1)
          boneWorld.v=(0.0,10.0,-32.9)  skin.v=(57.0,10.0,-17.1)   <- at the ORIGIN
```

The mesh's own WorldXfm is at the label, but for a **skinned** mesh the renderer
ignores meshWorld and places verts purely by the bone palette
(`BoneOffsetAt(b) * boneTrans->WorldXfm()`), with `obj.world` forced to identity. The
bar's corner bones are parented under `pentatonic_display`, which **stays at the
origin** — so the skinned bar renders at screen-centre (≈(762,384), over the venue
"PAL/MUSIC" sign), exactly the reported symptom.

This is the same *family* as the char-skinning work (skinned mesh rendering at the
wrong world despite a "correct" bone) but a DIFFERENT mechanism: NOT a stale WorldXfm
cache (the pose-fling fix does not help — the bones' chain is internally consistent,
they are genuinely anchored at the origin), and NOT an inverse-bind mismatch (isolated,
the bar is a *clean* full-height rounded rect — only its POSITION is wrong).

---

## 1. ROOT CAUSE — the evidence chain

Two render-inert env-gated probes added to `BandRnd::DrawMesh` (`HUB_BAR_PROBE`):

1. **The bar IS a skinned mesh, 4 corner bones.** `owner->IsSkinned()` true,
   `numBones(owner)=4` (`bone_{top,bottom}_{left,right}.mesh`). So it takes the
   skinned vertex path (`vs_skinned`).

2. **The mesh WorldXfm is CORRECT, the bones are at the ORIGIN.** `HUB_BAR_DRAW`
   reports `meshWorld.v=(-376.83,2.50,104.65)` (the focused PLAY NOW label world,
   from `UILabel.cpp:334` `mLabelDir->SetWorldXfm(WorldXfm())`). But the per-bone
   `HUB_BAR` reports every corner bone's `parent='pentatonic_display'` with parent
   `world=(0,10,0.1)` — the ORIGIN, not the label. So `boneWorld` and the composed
   `skin` (`BoneOffsetAt * boneWorld`) are all near origin.

3. **Why the bones miss the label.** `UILabel::UpdateAndDrawHighlightMesh`
   (`src/system/ui/UILabel.cpp:316`) does, for the focused label:
   ```cpp
   mLabelDir->SetWorldXfm(WorldXfm());     // line 334: set the LABEL/mesh world
   topleft->SetLocalPos(x1, 0, z2); ...    // 335-338: corner bones = bar quad LOCAL
   ```
   The world placement is set on the mesh/labelDir; the corner bones get only LOCAL
   positions (the quad extents). On native the corner bones' transform PARENT
   (`pentatonic_display`) is NOT the object whose world got set — it stays at the
   origin — so the bones' WORLD (and the skin palette) never pick up the label
   translation. The mesh draws at the label, but the SKINNED verts follow the bones.

4. **The skinned path forces obj.world=identity** (`Rnd_Wgpu_RB3.cpp` ~:3962, mirrors
   DC3's `Mesh_Wgpu.cpp` and `BoneSetup.cpp`): for CHARACTER skeletons the palette is
   already WORLD-space (bones are top-level world objects, mesh WorldXfm is identity),
   so `vs_skinned`'s `worldPos = object.world * blendedPos` must keep `object.world = I`.
   For the hub bar the palette is NOT world-space (origin-anchored bones), and the
   world placement that SHOULD lift it lives only in the (discarded) mesh WorldXfm →
   the bar renders at the origin.

5. **Isolation proves the SHAPE is fine.** `RB3_ISOLATE_MESH=highlight_main` with the
   shipped (identity) obj.world draws a **perfect solid rounded rectangle** — correct
   width/height/rounded-corners — but at **screen centre**. So only the POSITION is
   wrong; the palette geometry is correct.

6. **Full meshWorld SKEWS, translation-only is correct.** Setting `obj.world =
   meshWorld` (full) moves the bar to the label but renders it THIN + skewed — the
   palette ALREADY orients the bar in screen space, so the full meshWorld rotation
   double-applies and collapses the vertical extent. Setting `obj.world =
   translation-only of meshWorld` restores the full clean rounded-rect AND places it
   behind the focused item. (Verified isolated: mode-2 thin/skewed; mode-1 perfect.)

---

## 2. THE FIX (engine, native-backend only, default-on, opt-out)

`milo-native-engine/src/platform/Rnd_Wgpu_RB3.cpp`, `BandRnd::DrawMesh`, the skinned
`obj.world` selection (~:3962, right next to the W6 SKEL_REBAKE / W5 pose-fling and the
Defect-1 `isUiHighlightOverlay` material region) — commit `81c9dcc`:

```cpp
// for the named UI bar meshes only: inject the mesh WorldXfm TRANSLATION into
// obj.world (rotation kept identity); the palette already oriented the bar.
if (skinned && hubBarPlacement) {            // highlight_main* / highlight_pattern*
    for (int i=0;i<16;i++) obj.world[i] = (i%5==0)?1.f:0.f;   // identity rotation
    const Vector3& mwv = mesh->WorldXfm().v;
    obj.world[12]=mwv.x; obj.world[13]=mwv.y; obj.world[14]=mwv.z;  // label translation
} else if (skinned) { /* identity (shipped) */ }
else { MiloXfmToColMajor(mesh->WorldXfm(), obj.world); }
```

- **Scope:** SAME specific mesh-name match as the Defect-1 colour fix
  (`strncmp(name,"highlight_main",14)==0 || strncmp(name,"highlight_pattern",17)==0`),
  so the overshell choose-difficulty highlight (`highlight.mesh`) and every
  gameplay/HUD/character skinned mesh are untouched.
- **Default-on; opt-out `RB3_NO_HUB_BAR_PLACEMENT_FIX=1`.**
- Native-only file (`MILO_ENGINE_GPU_PLATFORM_SOURCES_RB3`, not in the Wii image, not
  compiled by DC3) → Wii byte-identical + DC3-inert by construction.
- Also leaves the env-gated, render-inert `HUB_BAR_PROBE` diagnostics that localized
  the bug (`HUB_BAR_DRAW` one-shot + per-bone `HUB_BAR`).

### Why translation-only, not a parent-rebind?
The structurally "cleaner" fix would be to make the corner bones' transform parent
follow the label world (or re-derive the palette in label-local space). But the bar's
palette is ALREADY correctly oriented + sized in screen space (proven by isolation);
the only missing datum is the label TRANSLATION, which the mesh WorldXfm already
carries correctly. Injecting that one translation is minimal, region-scoped, and
behaves exactly like retail across focus moves. A full-meshWorld or parent-rebind
re-introduces the screen-orientation rotation a second time and skews the bar.

---

## 3. VERIFICATION (before / after, headless)

Harness: boot `rb3-native` (`RB3_GAME=1 RB3_HTTP=1 MILO_HEADLESS=1`), splash→main_hub,
`pad:14`/`pad:12` to move focus, `/api/screenshot`. Evidence in `/tmp/hub-bar/`.

| state | shipped (identity obj.world) | FIX ON (default) |
|---|---|---|
| focus PLAY NOW | yellow bar at screen-CENTRE (over PAL/MUSIC) | bar BEHIND "PLAY NOW", text dark-on-yellow |
| DDOWN → CAREER | bar still centre-ish | bar moves to CAREER row |
| DDOWN → TRAINING | — | bar on TRAINING row |
| DUP → CAREER | — | bar back on CAREER row |
| isolated (`RB3_ISOLATE_MESH`) | clean rect at centre | clean rect at top-left (PLAY NOW) |
| opt-out `RB3_NO_HUB_BAR_PLACEMENT_FIX=1` | n/a | bar BACK at screen-centre (control passes) |

- **Tracks focus** correctly in both directions (DDOWN/DUP), matching retail
  `images/retail-screenshots/yt_mhKNp9uAT48_menu_hub.png` (solid yellow bar behind the
  focused item; focused text reads dark).
- **No regression:**
  - `scripts/native/keyboard-to-gameplay.py --diff hard` PASSES end-to-end
    (main_hub → song_select → choose_part_guitar → choose_diff(hard) → game_screen
    playing at songMs≈21.8k).
  - **Overshell choose-difficulty highlight (`89e3beef`) still correct** — the yellow
    bar sits on EASY then tracks to HARD (`/tmp/hub-bar/regress/03,04_*.png`). The fix
    matches only `highlight_main`/`highlight_pattern`; the overshell uses
    `highlight.mesh` → untouched.
  - Song library list renders + scrolls (`01_song_select.png`); gameplay/venue
    unaffected (the obj.world branch only fires for the 4-bone hub bar meshes).
- **Crashes/asserts:** 0 across all runs.

---

## 4. LANDING NOTES

- **The COMBINED engine cherry-pick is BOTH commits, in order**, onto the engine's
  canonical branch (both based on pin `ce8ecda`):
  1. `925cc29` — Defect 1 (bar colour, register-prelit path). *(originally `53d31cf`
     on `wt-hub-highlight-fix`; cherry-picked here so the bar is yellow not black.)*
  2. `81c9dcc` — Defect 2 (bar placement, this task).
  Both edit ONLY `src/platform/Rnd_Wgpu_RB3.cpp`, `BandRnd::DrawMesh`, and are
  additive + self-contained.
- **Pin bump REQUIRED to land:** merge `925cc29`+`81c9dcc` into the engine's canonical
  branch, then bump `MILO_ENGINE_PIN` in `rb3/native/CMakeLists.txt`
  (`ce8ecda...` → new SHA) in a matching rb3 commit. **No rb3 `src/` files change** →
  Wii build byte-identical, no report.json regen needed.
- **Conflict regions** (both edits): `BandRnd::DrawMesh` sits next to the W5
  pose-fling WorldXfm-force pre-pass, the W6 SKEL_REBAKE pre-pass, the menu-lighting
  `mu.unlit`/`mu.prelit` text-prelit heuristics, and the Defect-1 `isUiHighlightOverlay`
  material block. Defect 2 adds one branch to the skinned `obj.world` selection (~:3962)
  + two render-inert probes; Defect 1 adds one bool + one `||` to `mu.prelit` (~:5230).
  A concurrent engine agent touching the skinned object-uniform setup or the
  material-uniform block could conflict; both changes are small and clearly scoped.
- **Build (rb3 worktree configured against the engine worktree):**
  `cmake --build native/build-native --target rb3-native -j`. Clean.

### Follow-up (optional, not required for the fix)
A fully structural variant would rebind the corner bones' transform parent to the
label-world dir (or re-derive the palette in label-local), removing the reliance on the
mesh WorldXfm translation. Not needed: the translation-only fix is correct, retail-
matching, and minimal. Touch it only if a future menu uses a ROTATED highlight label
(then the translation-only assumption would need the label rotation too — but no
in-scope menu does, and retail does not rotate the hub items).
