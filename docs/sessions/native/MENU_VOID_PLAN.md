# MENU / HUB "BLACK VOID" — world-backdrop render plan (N3)

**Authored:** 2026-05-29 (Opus planning subagent, READ-ONLY — no build, no run,
no code edits, no git-write). One doc only.
**Goal:** plan fixing the boot/main-hub + menu "black void" — the 3D world
backdrop (the shell vignette behind the UI) renders only a lower-middle band
with the top ~50% and right edge pure black (`screenshots/v34-status-review/
01_f0007.png`, the rooftop city scene; same class of void on the `BABOON NEST`
menu backdrops). This is item **N3** in `STATUS_AND_NEXT_GOALS.md` and the
single biggest remaining 1:1 menu-fidelity gap.

---

## 0. TL;DR

- **Root cause is confirmed and NOT a binstream/`PostLoad` read bug.** It is the
  `IsDeferredVenueProxy` HX_NATIVE skip in `WorldInstance::SyncDir`
  (`src/system/world/Instance.cpp:361-375`): the shell-vignette backdrops are
  loaded as `WorldInstance` **proxies** of `world/vignette/shell/gen/sv*.milo_xbox`,
  and that path is force-deferred to an *empty* proxy on native, so the backdrop
  draws (almost) nothing → void. The DTA proves the load path: `vignettes.dta:123`
  `new_shell_vignette` builds a `BackdropPanel (file "../world/vignette/shell/%s.milo")`.
- **The real engine bug behind the deferral** is in the proxy-instancing loop
  (`Instance.cpp:413-428`): for objects that live in the *shared inlined* source
  dir, `FindObject(name,false)` misses → `NewObject + CopyObject` makes a fresh
  copy, but **`Hmx::Object::Copy` (`Object.cpp:351-372`) does NOT copy `mDir`**,
  so the copy's `Dir()` is null and the very next `MILO_ASSERT(p->from->Dir(),
  0x2CA)` at `Instance.cpp:428` fires. The deferral was added to dodge that
  assert + a downstream "Could not find …mesh" crash.
- **W2a/W2b did NOT fix this** and only *partially* de-risk it. W2b
  (`70873c4a`) root-caused the *separate* "multi-chunk milo load fault" as
  **missing object-factory registration** in the **render harness** path
  (`rb3_render_mesh.cpp::LoadMiloAndWalk`), not the game boot. The game boot
  already registers all the relevant factories via `BandInit()`/`WorldInit()`/
  `UIManager::Init()` (verified: `BandInit`, `WorldInit`, `UIManager::Init` all
  resolve to real `T` symbols in `rb3-native`, not the weak `.s` stubs). So the
  menu-void is **not** a factory-registration gap in the game build. W2b's *value
  to this work* is confirmation that the binstream/ChunkStream/endianness path is
  sound — which **narrows N3 to the proxy-instancing logic, not the load read**.
- **Effort re-estimate: L (unchanged by W2a/W2b), ~3-7 days.** The three V5b
  obstructions (LoadPersistentObjects save/restore, shared-dir many-to-one
  parent, `Copy` doesn't copy `mDir`) are real and structural. W2b did not touch
  any of them.
- **Smallest-correct first step is NOT a full structural fix** — it is to make
  the shell-vignette proxy instance *as much geometry as it safely can* by
  fixing the `mDir`-copy obstruction (c) locally and tightening the deferral to
  only the genuinely-shared `world/shared/` sub-props, while letting the
  per-vignette `world/vignette/shell/` root instance. See §2.

---

## 1. Root-cause analysis (file:line evidence)

### 1.1 What the menu/hub backdrop actually loads

- `orig-assets/extracted/ui/vignettes.dta:58-128` — `{func new_shell_vignette
  ($name) … {new BackdropPanel $panel_name (file [dyn_file]) …
  (dyn_file … {sprintf "../world/vignette/shell/%s.milo" $name})}}`. The shell
  backdrops `sv2/sv3/sv4/…` (`vignettes.dta:183-193`) resolve to
  `world/vignette/shell/gen/sv3_a.milo_xbox` etc. — present in the extract
  (`ls orig-assets/extracted/world/vignette/shell/gen/` → `sv2_a … sv8_a`).
- These milos are loaded as **`WorldInstance` proxies** (the `instance_file`
  prop → `SyncDir()`, `Instance.cpp:470`). The proxy's source dir
  (`world/vignette/shell/gen/sv3_a.milo_xbox`) itself contains *shared inlined*
  sub-dirs under `world/shared/` (props, fx, text_3d — `ls
  orig-assets/extracted/world/shared/` → `fx gen "pyro cans" save_fail
  text_3d`). Both `world/vignette/` AND `world/shared/` are caught by the
  deferral predicate.

This is the analog of the in-song venue (`VENUE_RENDER.md` V19) — **except** the
menu backdrop *is* requested via the real DTA path (V19's gameplay venue was
never even requested, which is why V19's force-load was needed). Here the request
fires; the **instancing** is what's stubbed out.

### 1.2 The deferral that produces the void

`src/system/world/Instance.cpp:361-375` (`WorldInstance::SyncDir`):

```cpp
if (IsProxy()) {
    DeleteTransientObjects();
    mSharedGroup = nullptr;
#ifdef HX_NATIVE
    if (mDir && IsDeferredVenueProxy(mDir.GetFile())) {   // world/vignette/ || world/shared/
        ... MILO_LOG("deferring cosmetic venue proxy ...");
        SyncObjects();
        return;                       // <-- leaves `this` an EMPTY proxy -> void
    }
#endif
    if (mDir) { ... real instancing loop ... }   // skipped on native for these paths
```

`IsDeferredVenueProxy` (`Instance.cpp:351-358`) matches `world/vignette/` ||
`world/shared/`. The shell backdrops hit `world/vignette/` and bail before the
instancing loop, so the `WorldInstance` is left empty (only whatever
`SyncObjects()` wires) → it **draws nothing**. The lower-band rooftop geometry
that *does* appear in `01_f0007.png` is the part of the hub scene that is NOT
behind a deferred proxy (the static non-proxied world content); the void is the
deferred-proxy region.

### 1.3 The real engine bug the deferral hides (the proxy-instancing loop)

`Instance.cpp:392-428` is the loop that would run if not deferred:

1. `:413` `Hmx::Object *foundObj = FindObject(it->Name(), false)` — for objects
   that live in the **shared inlined** source dir, this returns null (the shared
   dir's objects aren't resolvable through the proxy's own `FindObject`).
2. `:415-419` `foundObj = Hmx::Object::NewObject(it->ClassName()); CopyObject(it,
   foundObj, deep, true)` — a fresh copy is made.
3. `:428` `MILO_ASSERT(p->from->Dir(), 0x2CA)` — **fires**, because `p->from ==
   foundObj` and `foundObj->Dir() == null`.

The reason `foundObj->Dir()` is null is precisely **obstruction (c)**:
`src/system/obj/Object.cpp:351-372` `Hmx::Object::Copy(const Hmx::Object *o,
CopyType)` copies only the typedef + type-props — it **never assigns `mDir`**.
(Contrast the *copy-constructor* at `Object.cpp:103` `mDir = obj.mDir`, which is
NOT the path `CopyObject` uses.) So every `NewObject`+`CopyObject` copy reaches
the assert with a null `Dir()`.

### 1.4 The two other structural obstructions (V5b, still open)

Documented inline at `Instance.cpp:326-350` and re-confirmed in
`DIVERGENCE_AUDIT.md` Part I hack #2 / Part II row "Instance.cpp":

- **(a) `LoadPersistentObjects` save/restore.** `WorldInstance::LoadPersistentObjects`
  (`Instance.cpp:191-212`) saves `mDir->Dir()`/name/typedef, loads, then restores
  via `mDir->SetName(dirNameStr, dirDir)`. Wiring a parent `Dir()` onto the
  shared `mDir` *before* this runs makes the restore register the shared dir into
  the wrong (proxy) hash table, breaking the self-entry. Any fix must wire parent
  state **after** LoadPersistentObjects.
- **(b) Shared dir is many-to-one.** The same `world/shared/*.milo` dir is reused
  across MULTIPLE `WorldInstance` proxies (DirLoader cache hit at `Dir.cpp:404-407`
  `iDir.dir = last->GetDir()`). A single `mDir`/parent pointer can only name one
  parent — whichever proxy wires last wins, breaking the others' lookups +
  destruction (`RemoveFromDir` → `Object.cpp:145-154` MILO_FAILs "No entry for
  <name>"). This is why the audit-recommended one-line `PostLoadInlined`
  parent-wire fix does NOT work; a many-to-one abstraction (per-proxy shadow
  dirs) is needed.

### 1.5 What W2a/W2b changed (and why it does NOT fix N3)

- **W2a (`2c14d893`)** — wired `gBandRnd` to render a real `.milo` in-browser;
  split `RunRenderMesh` into `LoadMiloAndWalk`/`RenderFrame`/`RenderToPng`. Pure
  web render-harness wiring. **No `Instance.cpp`/`Dir.cpp`/`Object.cpp` change.**
- **W2b (`70873c4a`)** — root-caused the "multi-chunk milo load fault" (a
  *render-harness* SIGSEGV in `ObjectDir::PreLoad` → `vector<Viewport>::resize`)
  as **missing object-factory registration**: the harness only registered the
  rndobj base factories, so a `*Dir` subclass (`OverdriveMeterDir`, `PanelDir`,
  `GemTrackDir`) couldn't be constructed → `ReadDead` mis-skipped the nested-dir
  byte extent → stream desync. Fix = `native/src/rb3_game_object_factories.cpp`
  `RB3RegisterGameObjectFactories()`, **called only from
  `rb3_render_mesh.cpp::LoadMiloAndWalk` (the render harness)** — NOT from the
  game boot (`App.cpp`). Verified: `grep RB3RegisterGameObjectFactories` shows
  call sites only in `rb3_render_mesh.cpp:323` + the `main_web.cpp` comment.
- **Why this does NOT shrink N3 in the game build:** the game boot path
  (`App.cpp:285-294`) calls `WorldInit()` (`World.cpp` → `WorldDir::Init()` +
  `WorldInstance::Init()` + `WorldCrowd`/`Spotlight`/… ), `BandInit()`
  (`Band.cpp:77-128` → all the bandobj Dir factories), and `UIManager::Init()`
  (`BandUI.cpp:90`, via `UIScreen/UIPanel/PanelDir::Init`). All resolve to **real
  compiled `T` symbols** in `rb3-native` (`nm` confirmed; the weak `.s` stubs at
  `band3_link_stubs.s:82` are overridden by strong defs). So in the game the
  factories ARE registered, the shell-vignette milo DOES load + parse, and the
  `WorldInstance` proxy IS created. The void is purely the **`SyncDir`
  instancing deferral**, downstream of a clean load.
- **Net W2b value to N3:** it *retires* the "binstream `PostLoad` drops authored
  data" hypothesis from `DIVERGENCE_AUDIT.md` Part II / `STATUS` N11 **for the
  multi-chunk-load class** — the binstream reads correctly across chunks; the
  `.milo_xbox` chunk header is little-endian (no-op `EndianSwapEq` is correct)
  and the body is big-endian (swapped by `ReadEndian`). So N3's remaining surface
  is the **proxy-instancing copy logic**, not the byte-level read. (Note: the
  separate V31/V12 "empty `[objects]/[visibles]/[xfms]` save-arrays" question —
  `STATUS` N11 — is a DIFFERENT milo, the `1_player_wide` track-config, and is
  NOT resolved by W2b; it is out of N3 scope but shares the "is the load
  dropping authored data" framing. W2b makes it *more* likely that one too is a
  factory/instancing issue rather than a raw read drop, but that needs its own
  probe.)

---

## 2. Fix approach (smallest-correct first, then full structural)

### Layer

This is a **layer-(a) matched-fork** problem (`src/system/world/Instance.cpp`,
`src/system/obj/Object.cpp`/`Dir.cpp`), additive `#ifdef HX_NATIVE`. The actual
render path (engine `Rnd_Wgpu_RB3.cpp` / `BandRnd`) is already proven to draw
world geometry (V19 venue, V22/V23 venue cuts) — once the proxy instances real
meshes, they draw. **No engine (b) change is expected.** A small glue (c) probe
TU may help diagnose but is not the fix.

### Step 1 — instrument + confirm (S, do first)

Add an env-gated `MENU_VOID_DBG` (HX_NATIVE, render-inert) in
`WorldInstance::SyncDir` logging: proxy name, `mDir.GetFile()`, whether the
deferral fired, the `ObjDirItr` object count, and (when NOT deferred) how many
`objPairs` reach the `:428` assert with null `Dir()`. Run the boot/hub reproducer
(below) with the deferral *temporarily* turned off for `world/vignette/shell/`
only, and capture which assert/crash actually fires and on which object class.
This confirms whether obstruction (c) alone is the wall for the *vignette root*
(it may be — the root's own objects have a real `Dir()`; only the *shared*
sub-props hit the copy path).

### Step 2 — smallest-correct: fix obstruction (c) + narrow the deferral (M)

Two sub-changes, both additive HX_NATIVE:

1. **Make the copy carry `mDir` (obstruction c).** In `Instance.cpp:415-420`,
   after `CopyObject(it, foundObj, …)`, the copy belongs to `this` proxy dir —
   wire it: `foundObj->HxSetDir(this)` (the seam `Object.h:305` already exists)
   **and** ensure it's named into `this` so `RemoveFromDir` (`Object.cpp:145`)
   finds its entry on teardown (the `:443-447` `SetName(p->from->Name(), this)`
   loop already does this for `p->to`; the issue is `p->from`/foundObj's own
   `Dir()` for the `:428` assert). The minimal correct form: set
   `foundObj`'s dir to `this` *before* the assert, OR relax the native assert to
   `MILO_ASSERT(p->from->Dir() || p->from == foundObj-copy, …)` ONLY for the
   freshly-copied case and ensure teardown symmetry. **The clean version** is to
   give `Hmx::Object::Copy` (or `CopyObject`'s caller) an explicit `mDir`
   assignment for the proxy-instance case, mirroring the copy-ctor at
   `Object.cpp:103`. Verify teardown (`DeleteTransientObjects`/`RemoveFromDir`)
   does not then MILO_FAIL "No entry".
2. **Narrow `IsDeferredVenueProxy` to `world/shared/` only** (drop the
   `world/vignette/` clause), so the per-vignette shell root instances normally
   and only the genuinely many-to-one `world/shared/` sub-props stay deferred.
   This is the V19 lesson applied: the *root* venue/vignette dir instances fine
   (V19 proved the in-song venue root + stage + crowd all instance once
   requested); only the shared inlined props are the hard many-to-one case. If
   Step 1 makes the vignette root instance cleanly, this alone lights up most of
   the backdrop (the rooftop scene fills the frame), leaving only minor shared
   props (amps/decals) missing — a large fidelity win for modest risk.

**Expected outcome of Step 2:** the hub/menu backdrop fills the frame
(rooftop/`BABOON NEST` scene complete or near-complete), with at most a few
cosmetic shared-prop gaps. This likely retires the user-visible N3 void without
the full many-to-one rewrite.

### Step 3 — full structural fix (L, only if Step 2 leaves significant gaps)

Solve obstruction (b) — the many-to-one shared dir — with **per-proxy shadow
dirs**: when a `world/shared/*.milo` dir is shared across N proxies, give each
proxy its own lightweight shadow `ObjectDir` that owns the copied objects and
names them, so each proxy's `FindObject`/`RemoveFromDir`/destruction is
self-consistent. Wire parent state **after** `LoadPersistentObjects`
(obstruction a). This is the ~1-week decomp work V5b scoped; only do it if the
remaining shared-prop gaps are visually material. Re-enabling the full
`world/shared/` instancing also retires `InterstitialPanel::Exiting` /
`BackdropPanel::Exiting` short-circuits (`InterstitialPanel.cpp:83-122`,
DIVERGENCE hacks #2a/#2b) — the vignette outro `.anim/.trg` would then fire.

---

## 3. Files-to-edit list (full paths + layer + overlap risk)

| # | File | Layer | Change | Step |
|---|------|-------|--------|------|
| 1 | `/home/free/code/milohax/rb3/src/system/world/Instance.cpp` | (a) matched-fork | Narrow `IsDeferredVenueProxy` (`:351-358`) to `world/shared/` only; instrument `SyncDir` (`:361-375`); wire copied object's `mDir`/teardown in the instancing loop (`:413-447`); add `MENU_VOID_DBG` | 1,2,3 |
| 2 | `/home/free/code/milohax/rb3/src/system/obj/Object.cpp` | (a) matched-fork | `Hmx::Object::Copy` (`:351-372`) — carry `mDir` for the proxy-instance copy case (or add explicit assignment at the `CopyObject` call site so foundObj->Dir() is non-null at `Instance.cpp:428`) | 2 |
| 3 | `/home/free/code/milohax/rb3/src/system/obj/Dir.cpp` | (a) matched-fork | ONLY if Step 3 — `PostLoadInlined` (`:152-163`) / `PostLoad` (`:390-454`) shared-dir parent-chain wiring for per-proxy shadow dirs. **Highest overlap risk** (see below) | 3 |
| 4 | `/home/free/code/milohax/rb3/src/system/obj/Object.h` | (a) matched-fork | The `HxSetDir` seam (`:305`) already exists — likely no edit, used as-is | 2 |
| 5 | `/home/free/code/milohax/rb3/src/band3/meta_band/InterstitialPanel.cpp` | (a) matched-fork | ONLY if Step 3 lands — retire/tighten the `Exiting`/`BackdropPanel::Exiting` short-circuits (`:83-122`) once the vignette outro anim fires naturally | 3 (cleanup) |

**Files NOT expected to change:** the engine render path
(`milo-native-engine/src/platform/Rnd_Wgpu_RB3.cpp`) — world geometry already
draws once instanced. No glue (c) change is the fix (an optional throwaway glue
probe TU is fine but should not ship).

### Overlap-risk callout (web-port milo-load files now in-tree)

The web port is **paused** (`5394c897`) but its code is committed:
- `/home/free/code/milohax/rb3/native/src/rb3_game_object_factories.cpp` and the
  `rb3_render_mesh.cpp` / `main_web.cpp` changes are **render-harness-only** and
  do **not** touch `Instance.cpp`/`Dir.cpp`/`Object.cpp`. **No file-level overlap
  with N3's edit list.** They are safe to leave alone.
- The **conceptual** overlap is the opposite of a collision: W2b's factory work
  is the *correct precedent* for "register the real class so its `PostLoad`
  consumes the right bytes." If the game-boot factory set ever drifts from W2b's
  list, the menu milo could regress — but today the game boot registers a
  superset via `BandInit`/`WorldInit`/`UIManager::Init`, so N3 is independent.
- **The real concurrency hazard is the permuter**, not the web port:
  `Instance.cpp`, `Dir.cpp`, `Object.cpp` are all permuter-owned
  `src/system/**`. Stage an explicit file whitelist on commit; the HX_NATIVE
  blocks are additive and re-appliable but the permuter wipes ~30% of blocks
  between sessions (`SALVAGE_V33`). Re-read + re-apply if a file shifts.

---

## 4. Effort, regression risk, verification

### Effort: L (~3-7 days). W2a/W2b did NOT shrink it.

- W2b removed the *binstream-read* hypothesis (good — narrows the search) but the
  three V5b structural obstructions are untouched and are the actual cost.
- **Step 1 (instrument):** S, ~half day. **Step 2 (mDir-copy + narrow
  deferral):** M, ~1-2 days, and is the high-probability user-visible win.
  **Step 3 (per-proxy shadow dirs):** L, ~3-5 days, only if Step 2's gaps are
  material. Recommend **shipping Step 2 first, gating Step 3 behind visual
  review** of how complete the backdrop looks after Step 2.

### Regression risk: HIGH — call it out explicitly.

- **`Object::Copy` / `CopyObject` is used by EVERY object-clone path** (proxy
  instancing, FileMerger character-outfit merge, prefab spawn). A change to
  `Hmx::Object::Copy`'s `mDir` handling has blast radius across ALL milo
  instancing, including **gameplay venue (V19-V23) and song-load**. Gate every
  change behind an env opt-out (e.g. `RB3_MENU_VOID_FIX_OFF=1`) and A/B.
- **`Instance.cpp::SyncDir` runs for the in-song venue too** — narrowing the
  deferral or changing the instancing loop can regress the V19/V22/V23 venue
  render + camera cuts. Verify the gameplay venue (`small_club_01`) still
  instances + draws + cuts after the change. The V19 force-load + V22/V23 cam
  blocks must remain green.
- **`RemoveFromDir` teardown symmetry** (`Object.cpp:145-154`) is the trap: if a
  copied object gets a `Dir()` but is not correctly entered in that dir's hash,
  destruction MILO_FAILs "No entry for <name>". This is the exact failure mode
  V5b hit. Verify clean teardown (exit 0) over a full boot→menu→song run, not
  just the render frame.
- Per `DIVERGENCE_AUDIT.md`: "binstream changes affect ALL milo loads incl.
  gameplay/song-load — HIGH-risk area." Treat the whole obj/world tree as such.

### Verification (which screens/frames; confirm no gameplay/song-load regression)

- **Menu/hub void (the target):** boot reproducer to the main hub
  (`01_f0007`-class frame) + the `PLAY NOW`/`QUICKPLAY` menu (`04_f0120`,
  `05_f0200`) + song-select. Confirm the shell-vignette backdrop now fills the
  frame (rooftop city / `BABOON NEST` scene) instead of the lower-band-plus-void.
  Compare against `screenshots/v34-status-review/01_f0007.png`,`04_f0120.png`,
  `05_f0200.png`. **This is a subjective visual assessment → Opus per
  `visual-reviews-opus-only` memory.**
- **Gameplay venue NOT regressed:** full song run (the canonical
  `RB3_GAME_INPUT` from `VENUE_RENDER.md`, `track:guitar`, `20thcenturyboy`,
  `MILO_MAX_FRAMES=9000`). Confirm V19 venue geometry + V22/V23 camera cuts +
  band/crowd still render (mesh count ~170-315/frame during the song; gems
  stream; HUD top-center). `VENUE_CAM_LOCK`/`CAM_DBG` canaries still green.
- **Song-load NOT regressed:** confirm `Game::mLoadState=kReady` still reached
  (per `rb3-song-load-achieved` memory) and clean **exit 0** (no teardown
  SIGSEGV introduced by the new `mDir`/`RemoveFromDir` path — this is the
  highest-likelihood regression).
- **A/B:** every change behind `RB3_MENU_VOID_FIX_OFF=1` so the deferral baseline
  is one env var away for diffing.

### Boot/hub reproducer (to capture the menu void)

```
MILO_SCREENSHOT_DIR=$PWD/docs/sessions/native/screenshots/menu-void \
MILO_SCREENSHOT_FRAMES=7,25,50,120,200,280 \
MENU_VOID_DBG=1 RB3_GAME=1 MILO_HEADLESS=1 MILO_AUDIO=1 \
RB3_DATA=$PWD/orig-assets/extracted MILO_MAX_FRAMES=320 \
RB3_GAME_INPUT="@10:start,@30:confirm,@140:select:pn_quickplay.btn,@220:select:qp_quickplay.btn,@280:down" \
./native/build-native/rb3-native
```
(`MILO_SCREENSHOT_DIR` MUST be ABSOLUTE — relative silently fails `WritePNG`.)

---

## 5. Summary for the dispatcher

- **Root cause:** the shell-vignette menu/hub backdrops load fine (DTA
  `vignettes.dta:123` → `world/vignette/shell/gen/sv*.milo_xbox` as `WorldInstance`
  proxies) but `WorldInstance::SyncDir`'s HX_NATIVE `IsDeferredVenueProxy` skip
  (`Instance.cpp:361-375`) bails to an empty proxy → void. The deferral exists to
  dodge `MILO_ASSERT(p->from->Dir(), 0x2CA)` (`Instance.cpp:428`), which fires
  because `Hmx::Object::Copy` (`Object.cpp:351-372`) doesn't copy `mDir` on the
  `NewObject`+`CopyObject` shared-dir path (obstruction c), compounded by the
  many-to-one shared dir (obstruction b) and `LoadPersistentObjects` save/restore
  (obstruction a).
- **W2a/W2b:** did NOT fix it and don't shrink the estimate. W2b's factory fix is
  render-harness-only (`rb3_render_mesh.cpp`), and the game boot already registers
  the factories (`BandInit`/`WorldInit`/`UIManager::Init`, real `T` symbols).
  W2b's value is *eliminating the binstream-read hypothesis* — N3 is proxy-
  instancing logic, not a byte-level `PostLoad` drop. (The V31/V12 empty
  save-arrays — `STATUS` N11 — are a separate milo, still open.)
- **Fix approach:** layer-(a) matched-fork. Smallest-correct = (1) carry `mDir`
  on the proxy-instance copy (fix obstruction c) + (2) narrow the deferral to
  `world/shared/` only so the vignette ROOT instances (the V19 lesson). Full fix
  = per-proxy shadow dirs for the many-to-one shared sub-props (Step 3, ~1 week,
  V5b scope) — gate behind visual review of Step 2's result.
- **Files to edit:** `src/system/world/Instance.cpp` (deferral + instancing
  loop), `src/system/obj/Object.cpp` (`Copy` mDir), `src/system/obj/Dir.cpp` +
  `src/band3/meta_band/InterstitialPanel.cpp` (only for Step 3). All
  permuter-owned — explicit-whitelist commits. No engine/glue change is the fix.
  No file-level overlap with the paused web-port milo-load files.
- **Effort:** L (~3-7 days); ship Step 2 (M, ~1-2 days) first.
- **Key regression risk:** HIGH — `Object::Copy`/`CopyObject` and
  `Instance.cpp::SyncDir` are on the path for ALL milo instancing incl. the
  gameplay venue (V19-V23) and song-load; teardown `RemoveFromDir` symmetry is
  the most likely break (clean exit 0 is a gate). Env opt-out
  (`RB3_MENU_VOID_FIX_OFF=1`) + A/B mandatory. Visual review = Opus.
