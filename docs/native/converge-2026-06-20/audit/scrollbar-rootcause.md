# Scrollbar-in-the-Highway — Root Cause + Fix Plan

**Research only. No code/engine changes; nothing committed.** Companion to
`probe-data.md` §4 and `scout-residual.md` (a). This doc ROOT-CAUSES why
`scrollbar_bg.mesh` (a UI list-scrollbar background) is submitted into the
gameplay 3D world draw and produces an **implementable, HX_NATIVE-gated FIX PLAN**
(not implemented).

`scrollbar_bg.mesh` is **74% of all shard-guard drops** (834 / 1123 in the run
below; 71% in the probe). The guard masks it, so the screen looks clean today —
but it is an over-draw bug, not a deform bug, and the clean fix removes the
band-aid.

---

## TL;DR

The leaked mesh is the **character-swap list scrollbar** (`chars.sbd` →
`scrollbar_bg.mesh` / `scrollbar.mesh`) from the SHARED preloaded resource
`ui/resource/scrollbar_display.milo`. It lives in each **overshell player slot**
(`overshell_player.milo`). During gameplay the overshell is `kOvershellInSong`
and its slot character-list has no scrollable overflow, so the scrollbar's
`m_fSavedScale` stays at its **ctor default `0`**
(`ScrollbarDisplay.cpp:24`); `ScrollbarDisplay::DrawShowing`'s gate
`mAlwaysShow || m_fSavedScale < 1.0f` (`ScrollbarDisplay.cpp:179`) is therefore
TRUE, so it unconditionally `pDir->Draw()`s the shared resource dir
(`ScrollbarDisplay.cpp:181`).

That resource dir is drawn **in the venue WORLD camera pass, at world origin,
interleaved with the gameplay band characters** — measured below — so its
`scrollbar_bg.mesh` (authored as a 200u-stretch ribbon, `mScrollbarHeight=200`
ctor default `ScrollbarDisplay.cpp:21`) sprawls across the note highway as an
ornate teal filigree (`shots/audit/scrollbar_guardoff_highway.png`).

On **Wii this never appears** because `RndDrawable::Draw` frustum-culls the
off-frame widget (`Draw.cpp:208-212`, the `#else` arm). The **native build
disables frustum culling by default** (`Draw.cpp:199-206`, the `#ifdef HX_NATIVE`
arm — "the baked Xbox mSphere was wrong, so culling dropped visible meshes"), so
the widget is never culled and draws every frame. **This is the same native
over-draw family** as the MusicLibrary stale-slot text and the menu-void
backdrop occluder — UI geometry that Wii clipped but native paints.

Retail draws **no** scrollbar in the gameplay venue
(`images/retail-screenshots/yt_qRagnZCIMzk_gameplay_guitar.png` — clean highway),
so hiding it is **convergence-correct**.

**Fix (recommended): a `MenuVoidDrawHook`-style native skip** — extend the
existing per-drawable native draw hook (`Draw.cpp:63 MenuVoidDrawHook`, already
wired into `RndDrawable::Draw`/`DrawBudget`) to skip `scrollbar_bg.mesh` /
`scrollbar.mesh` when drawn under the world camera, default-ON with an
`RB3_*_OFF` opt-out. Alternatively gate `ScrollbarDisplay::DrawShowing`
(`ScrollbarDisplay.cpp:179`) on the list actually having scrollable content.
Both are detailed in §5. No deform/rebind change.

---

## 1. The asset + where it loads (resident during gameplay)

- Source widget: `src/system/bandobj/ScrollbarDisplay.{h,cpp}` (a `UIComponent`;
  `ScrollbarDisplay.h:7`).
- Typedef: `ui/ui_objects.dta:568-598` (`ScrollbarDisplay`, default + accomplishments
  types). `resource_file = resource/scrollbar_display.milo`
  (`ui_objects.dta:574-575`). The four endpoint "bones" are meshes:
  `top_bone=scrollbar_bg_bone_top.mesh`, `bottom_bone=scrollbar_bg_bone_bottom.mesh`,
  `thumb_top_bone`, `thumb_bottom_bone` (`ui_objects.dta:578-585`).
- Asset milo: `orig-assets/extracted/ui/resource/gen/scrollbar_display.milo_xbox`
  (and `scrollbar_accomplishments.milo_xbox`). Holds the `scrollbar` dir with
  `scrollbar.mesh`, `scrollbar_bg.mesh`, and the four `*_bone_*.mesh` widgets.
- **Globally preloaded**: `orig-assets/extracted/config/preload_subdirs.dta:80`
  (`("ui/resource/scrollbar_display.milo")`, in the `(ui …)` group) → resident in
  every screen, gameplay included. The preload is just a refcounted resource
  holder list (`UIManager::mResources`, `UI.cpp:789-803`); it is **NOT** parented
  into any draw tree by preloading. So preload alone does not draw it — a live
  widget must.
- **Shared single instance**: `UIManager::InitResources` dedups by path
  (`UI.cpp:789-803`) and `FindResource` returns the existing `UIResource`
  (`UI.cpp:803-826`), so every `ScrollbarDisplay` across all screens points
  `mResource->Dir()` at the **same** preloaded `scrollbar` dir.

Which screens instantiate the widget (binary class-name scan of `*.milo_xbox`):
`song_select{,_details,_filter}`, `accomplishments` (×3), `store_{menu,browser}`,
and the gameplay-resident **`overshell.milo`** + **`overshell_player.milo`**. The
overshell is the only one resident-and-drawn during a song.

---

## 2. The draw chain (overshell slot → shared resource dir → world pass)

- `overshell_player.milo_xbox` (the per-player slot panel, an `OvershellDir :
  public PanelDir`, `OvershellDir.h:6`) contains a `ScrollbarDisplay` named
  `chars.sbd`, paired with the character-swap list `chars.lst` (strings dump:
  `chars.lst` / `chars.sbd` adjacent). This is the leaked widget.
- Slots are created per player: `OvershellPanel.cpp:1129-1131`
  (`LoadedDir()->Find<OvershellDir>(slotName)` → `new OvershellSlot(...)`); the
  slot's PanelDir is `OvershellSlot::mOvershellDir` (`OvershellSlot.cpp:64`,
  `GetPanelDir()` `OvershellSlot.cpp:142`).
- `ScrollbarDisplay::DrawShowing` (`ScrollbarDisplay.cpp:173-183`):
  ```cpp
  RndDir *pDir = mResource->Dir();              // the SHARED preloaded scrollbar dir
  UpdateSavedListInfo();                         // sets m_fSavedScale IFF m_pList != null
  UpdateScrollbarHeightAndPosition();            // stretches bottom bone by mScrollbarHeight
  UpdateThumbScaleAndPosition();
  if (mAlwaysShow || m_fSavedScale < 1.0f) {     // DRAW GATE
      pDir->SetWorldXfm(WorldXfm());             // resource dir <- widget WorldXfm
      pDir->Draw();                              // walks scrollbar_bg.mesh etc.
  }
  ```
- The draw gate is satisfied because **`m_fSavedScale` is at its ctor default `0`**
  (`ScrollbarDisplay.cpp:24`). `UpdateSavedListInfo` only RAISES it toward 1.0
  when `m_pList` is set AND `NumDisplay() >= NumProviderData()`
  (`ScrollbarDisplay.cpp:94-104`); during gameplay the slot char-list is not
  scrolling (no overflow / no attached provider data visible), so it stays 0 and
  `0 < 1.0f` → draws. (`mAlwaysShow` is 0 by default, `ScrollbarDisplay.cpp:21`.)
- `scrollbar_bg.mesh` is a **skinned 2-bone stretch mesh** spanning
  `scrollbar_bg_bone_top` → `scrollbar_bg_bone_bottom`.
  `UpdateScrollbarHeightAndPosition` drives the bottom bone
  `mScrollbarHeight` units below the top (`ScrollbarDisplay.cpp:139-143`); the
  ctor default `mScrollbarHeight = 200.0f` (`ScrollbarDisplay.cpp:21`) → the bg
  ribbon is stretched ~4× its 80.8u bind to 324u world. That is the probe's exact
  ratio (80.8 → 324.1 = 4.01). This stretch is **authored, not a bug**; the
  bug is that it is drawn in-world at all.
- The mesh also legitimately trips the original old-format warning
  (`RndMesh.cpp:902-906`, "Skinned mesh needs to be re-exported: scrollbar_bg.mesh
  (ui/resource/scrollbar_display.milo)") — seen verbatim in the log. Dev noise,
  not a native fault.

---

## 3. Measured proof (this session, prebuilt `native/build-native/rb3-native`)

Harness: `scripts/native/band-closeup-capture.py --member guitar`, pinned
`coop_g_*` club closeups, `SHARD_DBG=1 SHARD_RATIO_DBG=1`. Binary log is NUL-laden
(`grep -a`).

### (a) It is the dominant drop, on the shared scrollbar dir, at world origin
```
[SHARD_RATIO] mesh='scrollbar_bg.mesh' bindExt=80.80 worldExt=324.08 ratio=4.01 other DROP
[SHARD_GUARD] dropped degenerate skinned mesh='scrollbar_bg.mesh' bindExt=80.80
   worldExt=324.08 ratio=4.0 f=627 dir='scrollbar' bone0=(0.5,-0.0,40.0)
```
verdict.json: `drops_total=1123, drops_other=1123, scrollbar_bg.mesh=834` (74%),
`drops_band=0` (verdict PASS — the guard cleans the screen, but masks this).

### (b) The owning instance has NO transform parent — it is the resource dir itself
`C8_PROBE=scrollbar_bg`:
```
[C8_MESH] mesh='scrollbar_bg.mesh' dir='scrollbar' owner='self' nb=2 src=compressed rebound=0/0
[C8_SLOT] b=0 'scrollbar_bg_bone_top.mesh'    bdir='scrollbar'
   bfile='ui/resource/scrollbar_display.milo' root='scrollbar'@0x...  w=(0.5,-0.0,40.0)
   dW=0.00 STATIC  bind=(0.0,0.0,40.0)
[C8_SLOT] b=1 'scrollbar_bg_bone_bottom.mesh' bdir='scrollbar' root='scrollbar'@0x...
   w=(0.5,-0.0,-284.0) dW=0.00 STATIC  bind=(-0.0,0.0,-40.0)
```
- `root='scrollbar'` — the `TransParent()` chain terminates AT the resource dir
  itself (it is a root transform). The widget's `SetWorldXfm(WorldXfm())` set it to
  a **near-identity** WorldXfm (top bone bind (0,0,40) → world (0.5,-0,40); only a
  0.5u x-shift) — i.e. the drawing widget's own `WorldXfm()` is essentially
  identity (world origin).
- Bones are `STATIC` (dW=0) — never animated; the bottom bone is the 200u stretch
  (z = 40 - ~324). This is NOT the char-skinning fling family.

### (c) It is drawn in the WORLD pass, interleaved with the gameplay band
Single-frame `[SHARD_RATIO]` draw order around the scrollbar — band outfit meshes
on **both sides**:
```
… kidgloves_skin.2.mesh   band      ← gameplay band (player0..3)
   bondagepants_resource.2.mesh band
   scrollbar_bg.mesh       other     ← LEAK
   scrollbar.mesh          other
   talldocsfolded_resource.mesh band ← second band-char draw set (overshell slot
   kidgloves_resource.mesh band         character previews)
   vestandtank_resource.mesh band …
```
`C8_PROBE` on the neighbours confirms the roots:
```
mesh='bondagepants_resource.mesh' root='player0'   ← gameplay band, WorldDir bridge
mesh='parkajacket_resource.mesh'  root='player1'   ← gameplay band
mesh='scrollbar_bg.mesh'          root='scrollbar' ← LEAK, same world pass
```
The gameplay band draws in `WorldDir::DrawShowing`'s HX_NATIVE band bridge
(`src/system/world/Dir.cpp:448-461`) which runs BEFORE `TheRnd->EndWorld()`
(`Dir.cpp:474`). The scrollbar (and the overshell-slot character previews after
it) draw in this same world-camera window — so the ribbon lands on the highway.

### (d) Visual + ground truth
- `shots/audit/scrollbar_guardoff_highway.png` (guard OFF) — the teal filigree
  ribbon sprawled across the whole note highway. This is what the guard drops.
- `images/retail-screenshots/yt_qRagnZCIMzk_gameplay_guitar.png` — retail: clean
  highway, no scrollbar. Hiding it converges to retail.

---

## 4. WHY native and not Wii (the real divergence)

`RndDrawable::Draw()` (`src/system/rndobj/Draw.cpp:197-214`):
```cpp
void RndDrawable::Draw() {
  if (mShowing) {
#ifdef HX_NATIVE
    if (MenuVoidDrawHook(this)) return;
    if (RB3VenueFrustumCull(this)) return;   // default OFF
    DrawShowing();                            // <-- always draws
#else
    Sphere sphere;
    int worldSphere = MakeWorldSphere(sphere, false);
    if (worldSphere == 0 || !RndCam::sCurrent->CompareSphereToWorld(sphere))
        DrawShowing();                        // <-- Wii: frustum-culled
#endif
  }
}
```
On Wii the off-frame scrollbar widget is frustum-culled and never drawn. Native
disabled culling by default (`Draw.cpp:199-206` — the baked Xbox `mSphere` was
wrong, so correct culling dropped visible meshes). So the **already-existing**
`ScrollbarDisplay::DrawShowing` over-draw (it draws whenever `m_fSavedScale < 1`,
on both platforms) only becomes VISIBLE on native because culling no longer hides
it. This is the same root family as:
- the MusicLibrary stale-slot text over-draw (memory: "360-ARK draws unused slots
  Wii hid"), and
- the menu-void worldcenter backdrop occluder already fixed by
  `MenuVoidDrawHook` (`Draw.cpp:40-80`).

---

## 5. FIX PLAN (implement in the impl batch — NOT implemented here)

### Option A (RECOMMENDED) — extend the native `MenuVoidDrawHook` skip
**File:** `src/system/rndobj/Draw.cpp` (`MenuVoidDrawHook`, line 63; already
called from `RndDrawable::Draw` line 200 and `RndDrawable::DrawBudget` line 222).
**Change:** add a default-ON skip for the scrollbar bg/ribbon meshes when the
current camera is the world/venue camera (NOT a UI cam), mirroring the existing
worldcenter-occluder skip (`Draw.cpp:79-80`):
```cpp
// (sketch) skip the UI list-scrollbar leaking into the gameplay 3D world.
// scrollbar_bg.mesh / scrollbar.mesh come from the shared preloaded
// ui/resource/scrollbar_display.milo via an overshell-slot ScrollbarDisplay whose
// m_fSavedScale==0; Wii frustum-culled it, native culling is OFF. Retail draws
// no scrollbar in-venue, so dropping it converges. Opt-out: RB3_SCROLLBAR_FIX_OFF.
if (!sScrollbarFixOff &&
    (!std::strcmp(nm, "scrollbar_bg.mesh") || !std::strcmp(nm, "scrollbar.mesh")) &&
    RndCam::sCurrent /* and it is the world/venue cam, not a UI cam */)
    return true;
```
- **Camera predicate:** must skip ONLY under the world/venue cam so a legitimately
  shown scrollbar (song_select, accomplishments, store) — which draws under a UI
  cam — is untouched. The engine exposes the current cam via `RndCam::sCurrent`;
  the existing `RB3VenueFrustumCull` already special-cases "under world.cam"
  (`Draw.cpp:163-174` + `Rnd_Wgpu_RB3.cpp:3895`), so reuse that world-cam
  discriminator rather than a raw name match. (If a clean world-cam test is not
  reachable in `Draw.cpp`, fall back to Option B which is camera-agnostic by
  construction.)
- **Why this site:** it is the exact, proven pattern for "native draws a UI mesh
  Wii culled," already wired with a default-ON fix + `_OFF` opt-out + the
  `MENU_VOID_SKIP=<substr>` A/B diagnostic. Lowest new surface.
- **Blast radius:** name-scoped to two meshes; the world-cam guard prevents
  hiding the scrollbar in menus. No other mesh is named `scrollbar*.mesh`.

### Option B (ALTERNATIVE) — gate `ScrollbarDisplay::DrawShowing` on real content
**File:** `src/system/bandobj/ScrollbarDisplay.cpp:179` (the draw gate).
**Change:** under `HX_NATIVE`, additionally require an actually-scrollable,
list-attached widget before drawing — i.e. don't draw the bg/thumb when there is
no list or nothing overflows. The condition `m_fSavedScale < 1.0f` is true for the
degenerate default-`0` case precisely BECAUSE no list is attached; tighten it:
```cpp
// HX_NATIVE: the shared preloaded scrollbar dir over-draws into the 3D world
// when m_fSavedScale is at its ctor default 0 (no scrollable list). Wii hid this
// via frustum cull (Draw.cpp #else); native culling is OFF, so guard the draw on
// real scrollable content. Wii path unchanged. Opt-out: RB3_SCROLLBAR_FIX_OFF.
bool scrollable = GetListAttached() && m_pList && m_fSavedScale < 1.0f
                  && m_fSavedScale > 0.0f;   // 0 == "no data", not "fully scrolled"
if (mAlwaysShow || scrollable) { pDir->SetWorldXfm(WorldXfm()); pDir->Draw(); }
```
- **Caveat:** must NOT regress a legitimately-empty-but-shown scrollbar where
  retail DID draw a full-height bar. Verify against song_select/accomplishments
  before defaulting on. `GetListAttached()` (`ScrollbarDisplay.cpp:220-229`) +
  `m_pList` are the right discriminators. Camera-agnostic (no `RndCam` needed),
  but touches shared `src/` (Wii-neutral via `#ifdef HX_NATIVE`).
- **Blast radius:** higher than A (affects ALL ScrollbarDisplay widgets, not just
  in-world ones) — but only on native, and only the empty/no-data case. Prefer A
  unless the world-cam predicate in `Draw.cpp` is awkward.

### What NOT to do
- Do **not** rebind/rebake the mesh — there is no skeleton basis error; the
  stretch is authored. (`scout-residual` (a) already established this.)
- Do **not** touch the V24 shard guard — it is correctly dropping a UI mesh that
  should not be in-scene; either fix removes the need for it on this mesh.
- Do **not** edit the milo / `mScrollbarHeight` default — that breaks the real
  scrollbar in menus.

---

## 6. Verification (impl batch)

Same harness, before/after, guard ON:
```
SHARD_DBG=1 SHARD_RATIO_DBG=1 python3 scripts/native/band-closeup-capture.py \
    --member guitar --frames 3 --frame-dt 500 --out /tmp/bc/fix --tag fix
```
Acceptance:
- `verdict.json` `drop_meshes` no longer contains `scrollbar_bg.mesh` (was 834) →
  `drops_other` falls by ~74% (to ~289: the crowd/extras residue only).
- `drops_band` stays 0; verdict still PASS.
- A/B the FIX opt-out (`RB3_SCROLLBAR_FIX_OFF=1`) to confirm the env gate flips the
  drop back on (proves the fix, not a coincidence).
- Guard-OFF visual (`SHARD_GUARD_OFF=1`) with the fix ON: the highway must be
  CLEAN (no teal filigree) — i.e. the fix removes the geometry, not just the
  guard's drop. Compare `shots/audit/scrollbar_guardoff_highway.png` (before).
- **No menu regression:** capture song_select + accomplishments with the fix ON
  and confirm a scrollable list still shows its scrollbar (use
  `scripts/native/song-select-capture.py`). This is the must-not-break gate for
  Option A's world-cam predicate and Option B's content gate.

---

## 7. Convergence value

71-74% of all residual shard-guard drops, and visually loud without the guard.
**But** with the guard the highway already matches retail (no scrollbar), so the
guard's drop is *accidentally correct*. The fix's net visual gain over today's
guarded render is ~zero; its value is: (1) removing the band-aid + the per-frame
old-format warning, (2) eliminating 834 wasted skinned-mesh draws+drops per
~30 frames, and (3) collapsing the residual drop count so the remaining
crowd/extras drops (the genuinely-uncovered families) become the visible signal.
**Priority: do it, but it is cleanup/correctness, not a new visible win.**

---

## Anchors

| claim | anchor |
|---|---|
| draw gate `m_fSavedScale < 1.0f` | `src/system/bandobj/ScrollbarDisplay.cpp:179` |
| `pDir->SetWorldXfm`/`Draw` | `ScrollbarDisplay.cpp:180-181` |
| `m_fSavedScale` ctor default 0 | `ScrollbarDisplay.cpp:24` |
| `mScrollbarHeight` ctor default 200 | `ScrollbarDisplay.cpp:21` |
| bottom-bone stretch | `ScrollbarDisplay.cpp:139-143` |
| `m_fSavedScale` set only if `m_pList` | `ScrollbarDisplay.cpp:94-104` |
| widget `chars.sbd` in slot panel | `overshell_player.milo_xbox` (strings `chars.lst`/`chars.sbd`); `OvershellDir.h:6`; `OvershellSlot.cpp:64,142` |
| slot creation | `OvershellPanel.cpp:1129-1131` |
| preload entry | `config/preload_subdirs.dta:80` |
| shared resource dedup | `src/system/ui/UI.cpp:789-826` |
| typedef + bone meshes | `ui/ui_objects.dta:568-598` |
| world band bridge (pre-EndWorld) | `src/system/world/Dir.cpp:448-461`; EndWorld `Dir.cpp:474` |
| native culling OFF / Wii cull | `src/system/rndobj/Draw.cpp:197-214` |
| `MenuVoidDrawHook` fix precedent | `src/system/rndobj/Draw.cpp:63-155` (wired `Draw.cpp:200,222`) |
| world-cam frustum discriminator | `Draw.cpp:163-174`; `Rnd_Wgpu_RB3.cpp:3895` |
| shard guard (drops it) | `../milo-native-engine/src/platform/Rnd_Wgpu_RB3.cpp:5176-5197` |
| re-export warning | `src/system/rndobj/Mesh.cpp:902-906` |
| retail clean highway | `images/retail-screenshots/yt_qRagnZCIMzk_gameplay_guitar.png` |
| guard-OFF artifact | `docs/native/converge-2026-06-20/shots/audit/scrollbar_guardoff_highway.png` |
