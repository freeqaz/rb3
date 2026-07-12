# R4 — Motivation + Constraints Ledger (DC3 Render Bridge)

Recon lane R4. Read-only pass over the engine-arch-review campaign, the two native
CMake configs, and the shared engine repo. This is the GO/NO-GO *input* lane: it
classifies every render-defect family by whether a renderer swap could help, and
enumerates the integration cost and the "keep fixing" baseline. It does not decide.

## Summary

1. **A bridge helps only class (b) — RB3 render-path code where DC3's backend is genuinely ahead.** Mapped against the campaign's own SYS-1..7 taxonomy, the *deepest, longest-fought* families are class (a)/(c) — char skinning/bind, native load-path, game-logic animation — which a renderer swap cannot touch.
2. The flagship pain (hands/finger shard, ~waves 2–21) is **terminal and renderer-independent** (vert-encoded inter-bone geometry); the six-wave crowd chain resolved to a **measurement artifact**; perf-clip / prop-IK / black-head / HUD-position are **game-logic**. None is a renderer-quality gap.
3. The class-(b) surface where DC3 is genuinely ahead (a real transparent/depth pass, shadow/bloom/dof/postproc passes, a data-driven non-content-coupled backend) is **either already addressed in RB3's own backend** (DrawContext, placement contract, chroma-preserve, UI-post-grade) **or measured near-no-op on RB3's actual venue content** (0 directional lights; Lambert already beats box-ambient).
4. Integration cost is dominated by one hard fact: the DC3 render TUs read **DC3's 2012-era `rndobj` shapes that RB3-Wii's 2010-era `rndobj` cannot compile** — the exact Wave-2.3 finding that forced RB3 onto its own `BandRnd` backend in the first place.
5. "Keep fixing" shows **sharply diminishing returns** (≈3 default flips in the last 12 waves, all class (c)), but that decline reflects a class-(c) frontier the bridge does not address.

## Findings

### (1) Defect ledger + class-of-fix classification

The campaign's own root-cause taxonomy is `ARCHITECTURE_REVIEW.md:57-130` (SYS-1..7)
and the bug→cause map `ARCHITECTURE_REVIEW.md:139-158`. I classify each family as:
**(a)** shared-engine code both ports execute · **(b)** RB3-side render-path code (a
bridge can help) · **(c)** game-logic/animation/load/asset (renderer swap irrelevant).

Load-bearing architectural fact for the classification: the render backend is
**flavor-split in the shared engine repo**, not shared at execution. RB3 builds
`MILO_ENGINE_GPU_BACKEND=rb3` → `BandRnd : Rnd` = `src/platform/Rnd_Wgpu_RB3.cpp`
(`rb3/native/CMakeLists.txt:62-70,181-188`). DC3 builds `MILO_ENGINE_GPU_BACKEND=dc3`
→ `WgpuRnd : NgRnd` + 6 rndobj-coupled gfx TUs
(`dc3-decomp/native/CMakeLists.txt:257`). Both share only the **rndobj-free gfx CORE**
(`GpuDevice`/`PipelineManager`/`Screenshot`) + Dawn/WebGPU. So "RB3 render-path code"
= `milo-native-engine/src/platform/Rnd_Wgpu_RB3.cpp` + the `RB3*` TUs
(`RB3PostProc`, `RB3MaterialBinder`, `RB3HaloPass`, `RB3Quad`, `RB3MeshCache`) —
class (b). The DC3 passes it would be swapped for
(`milo-native-engine/src/gfx/{BloomPass,DofPass,ShadowPass,PostProcPass,DrawRect2D}.cpp`)
compile only against the `dc3` rndobj shapes.

| Family (waves) | SYS | Root cause (cited) | Class | Status | Bridge relevance |
|---|---|---|---|---|---|
| **Hands/finger shard "spike-fans"** (W2–W21, flagship) | SYS-1 | Bind-basis split: appendage verts skinned vs shared static `skeleton.milo` magnet while drawn on per-member animated `skeleton_unshared`; baked `invBind` in magnet basis → R·sin θ shell tear. `ARCHITECTURE_REVIEW.md:64-72,141`; native-introduced `FilterSubdir` shim suppresses per-member remap (`README.md:716`). | **(c)** char-bind/skinning + native load-path; the mitten *workaround* lives render-side | **Terminal** (8 dead offset-bake cells + reskin refuted; `README.md:766-775`). Shipped mitigation `RB3_HANDS_MITTEN` default-ON (`README.md:644`). | **NONE.** Vert-encoded inter-bone geometry; DC3 chars use the same skinning engine. A DC3 renderer would host a different workaround, not a fix. |
| **Crowd / drum-kit "at one point"** (W4–W6 placement; W23–W29 walkers) | SYS-1 | Placement half: `obj.world=identity`, position derived entirely from bone palette (`Rnd_Wgpu_RB3.cpp:4556-4557`) → origin collapse when placement arrives via `SetWorldXfm`. Walkers half: SIX narratives → **measurement artifact** (measured the splash crowd faithfully dying; real `player0-3` walkers DO render; `README.md:967`). | **(b)** placement (in `Rnd_Wgpu_RB3.cpp`); **(c)** walkers (world/vignette trigger + artifact) | Placement **FIXED** + flipped `RB3_PLACEMENT_CONTRACT` default-ON (`README.md:249,258`, flag at `Rnd_Wgpu_RB3.cpp:2909`). Walkers **CLOSED, not a bug**. | Placement half was a real (b) RB3 backend gap — but **already fixed in RB3's own path**. Walkers never were a renderer issue. |
| **Venue wash / grayscale / WHITE over-exposure (emissive→bloom "glow")** (W5–W10) | SYS-4 | Stage-2 composite grade desaturates hot venue input (sub-knee mid-tone desat); `RB3PostProc` grade differs from Wii GX. `README.md:263,312,342-343`. | **(b)** render-path (RB3 composite/tonemap) | **FIXED** + flipped `RB3_PP_CHROMA_PRESERVE` + `RB3_VENUE_FALLBACK_FIX` default-ON (`README.md:343`). | DC3's `PostProcPass`/`BloomPass` are the mature model — but the specific defect was **fixed in RB3's own composite**; SceneUniforms struct is a shared 656B DC3-zero-blast constraint throughout. |
| **UI drawn before grade / depth-clear red band / song_select depth** (W13–W16) | SYS-3-adj | Menu PanelDirs never flush → one EndFrame composites venue+UI together; depth-clear artifacts. `README.md:485,520`. | **(b)** render-path compositing | **FIXED** + flipped `RB3_UI_POST_GRADE` default-ON (`README.md:520`). | Genuine (b); DC3's `TransparentQueue`/`RenderState` handle ordering natively — but **fixed in RB3's path**. |
| **State leak / order-dependent render / no transparent pass** (W0–W3) | SYS-3 | `mSceneBindGroup` mutable member re-created unbounded per frame (`ARCHITECTURE_REVIEW.md:85-94`); 2,670-line `DrawMesh`; **`TransparentQueue` is DC3-only**. | **(b)** render-path (backend architecture) | **FIXED** (W1.6 `RB3DrawContext`/`RB3SceneBinding`, `README.md:130`). | The one piece where DC3 is *structurally* ahead (transparent/depth-sorted pass). **But RB3 submits in traversal order** (`BandRnd::DrawMesh`), so the sort fix was **NO-GO / structurally absent** (`README.md:129`, Exit-A refuted). |
| **Lighting model inverted/incomplete (fog/shadow/spot hard-off, box-ambient absent, 4→8 lights)** (W3–W7) | SYS-4 | Real/approx routing inverted vs `IsValidRealLight` (`Env.cpp:172`); box-ambient collapsed to scalar; fog/shadow/spot hard-off `Rnd_Wgpu_RB3.cpp:1566-1568` (`ARCHITECTURE_REVIEW.md:96-104`). | **(b)** render-path lighting | Fog/projLight verified asset-blocked; **box-ambient DROPPED** — per-pixel Lambert judged strictly higher fidelity, venues have 0 directional approx lights (`README.md:260,317`). | DC3 has real `ShadowPass`/`DofPass`/`BloomPass`. **But measured near-no-op on RB3's actual venue content** — the SYS-4 completeness gap does not manifest on reachable venues. |
| **Content-coupling / ~30 strcmp asset-name branches** (W1.7, ongoing) | SYS-2 | Backend branches on hardcoded RB3 asset-name strings; detects text by `!mesh->Name()[0]`; "lights" highway by material-name surgery (`ARCHITECTURE_REVIEW.md:74-83`). | **(b)** render-path (code health) | Mitigated: all 13 asset-name branches relocated behind flags (W1.7, `README.md:89`). | Strongest *architectural* argument for a data-driven backend (DC3's `WgpuRnd`) — but a **code-health** argument, not a per-bug visual gap; largely contained behind flags. |
| **Patch-mesh composites (BandPatchMesh, tattoos/facepaint shards)** (W2.4 deferred; F5 W30) | SYS-1-adj + LP64 | `MeshVert` 32-bit-ABI-shaped; asm-faithful rewrites yield degenerate topology under LP64 (`ARCHITECTURE_REVIEW.md:150`). Twice bisect-reverted; `coop_g_cg` repro (`README.md:1022`). | **(c)** char customization geometry / LP64 ABI | **OPEN, no hypothesis** (twice-burned; not chartered). | **NONE.** Char-mesh geometry + LP64 layout, not a renderer gap. |
| **Perf-clip: band plays only idle, no instrument-performance clips** (W29–W31) | — | `set_play` intensity stream dead in-song; no C++ sender (`README.md:1013`). | **(c)** game-logic (BandDirector/BandCharacter::SetState, DTA venue-mood) | Mechanism NAMED; faithful fix rechartered W31; demo lever `RB3_BAND_PERF_FORCE_PLAY` non-faithful, default-OFF. | **NONE.** Animation-driver selection, not rendering. |
| **Prop / IK spike-fans (drumsticks, instrument-prop bones)** (W25–W28) | SYS-1-adj | IK weight=1 with target far past arm reach → over-rotation; `mFinger` feedback; prop-tip bones not clip-driven (`README.md:842,859`). | **(c)** char/IK game-logic | **FIXED** + flipped `RB3_IK_REACH_CLAMP` (W25) + `RB3_PROP_POSE_FULL` (W30) default-ON. | **NONE.** CharIKHand posing, not rendering. |
| **Black singer head** (W6) | — | Head detail texture in nested subdir unreachable by non-recursive `dir->Find` → null diffuse (`README.md:262`). | **(c)** asset-load / char texture bind | **FIXED** + flipped `RB3_BLACK_HEAD_FIX` default-ON. | **NONE.** ObjectDir traversal / texture bind. |
| **UI text color (pale-on-pale focus, white-on-yellow rows)** (W6–W16) | SYS-2/SYS-5 | Unconditional floor `max(0.6,color)` in `RB3MaterialBinder.cpp:145-149`; RndText glyph shader ignores font-material color (`README.md:269,555`). | **(b)** partly render-path (material binder + glyph shader) | **FIXED** + flipped `RB3_UI_TEXT_FLOOR` + `RB3_ROWFIX` default-ON. | Partial (b) win — fixed in RB3's material binder + a small engine RndText color-honor. |
| **Hub grey quad / menu quads** (W6) | SYS-5 | 360-ARK `LabelShrinkWrapper` renders opaque-grey on native, hidden on Wii (`README.md:261`). | **(b)/(c)** native blend vs asset | **FIXED** + flipped `RB3_HUB_MENU_QUAD_HIDE` default-ON. | Marginal (b) — a name-scoped hide, not a renderer capability. |
| **HUD score position; album-art/ticker Y-anchor; "(null)" gamertag** (W13–W22) | SYS-5 | 360-asset layout offsets; our own K9 zeroed scoreboard.x; `PlatformMgr::GetName` weak stub (`README.md:798,488,556`). | **(c)** UI layout / asset / platform stub | **FIXED** + flipped (`RB3_HUD_SCOREBOARD_TOPRIGHT`, `RB3_SS_ART_YFIX`, `RB3_HUB_TICKER_YFIX`, `RB3_PLAYER_NAME_FALLBACK`). | **NONE.** Layout + platform glue. |
| **HUD glyphs: translucent score pill, white glyphs, missing star outlines** (W30 F2/F3/F4, W31) | SYS-2/SYS-5 | One HUD material/texture-bind family; retail pairs exist (`README.md:1020`). | **(b)** likely render-path texture-bind | **OPEN** (W31 secondary lane). | Plausible (b) — but a texture-bind bug, addressable in RB3's own binder; not a renderer-architecture gap. |
| **Draw-order / boot nondeterminism (gRand stream, loader completion order)** (W0–W19) | SYS-3-adj | Async loader completion order feeds gRand consumption + draw submission (`README.md:421,450,610`). | **(a)/(c)** shared load-path + determinism infra | **FIXED** (DrawContext, SortDraws tie-break, `RB3LoadDetStream` per-tag isolation, PRIMARY 10/10 `README.md:610`). | **NONE.** Determinism infra, orthogonal to renderer quality. |
| **Particles never render / "song never ends"** (pre-campaign) | SYS-6 | Silent weak stubs `DrawParticlesBillboard`/`EndGame` no-op (`ARCHITECTURE_REVIEW.md:115-121`). | **(c)** link stubs / game-logic | Fixed earlier. | **NONE.** |

**Tally.** Of ~17 families: **class (b) = 6** (placement✔, wash/glow✔, UI-post-grade✔,
state-leak✔, lighting≈no-op, content-coupling contained; + text-color✔ partial; + HUD-glyph
open). Of those 6, **5 are already fixed inside RB3's own backend** and the 6th (lighting
completeness, where DC3 is genuinely ahead) is **measured near-no-op on RB3's real venues**.
**class (a)/(c) = the rest**, including every terminal/open/deep item (hands, patch-mesh,
perf-clip, prop-IK, crowd-walkers). The residual render-quality pain sits almost entirely
in (a)/(c), which the bridge cannot touch.

### (2) Integration constraints

- **How rb3/native consumes the engine.** `add_subdirectory(${MILO_ENGINE_PATH} …)`
  (`rb3/native/CMakeLists.txt:224`) against a local checkout at `../../milo-native-engine`
  (`:72-73`), with a **soft SHA pin** `MILO_ENGINE_PIN` (`:74`) that only *warns* on
  mismatch (`:80-85`) — currently `b36bcfc…`. RB3 injects its matched-fork include roots +
  MWCC compat flags (`:92-100`) and **excludes 16+ DC3-shaped platform TUs** whose headers
  are older/absent in RB3-Wii (`:153-179`) — a standing inventory of how much DC3-shaped
  code already fails to compile against RB3 headers.
- **The dominant feasibility wall.** The DC3 render backend (`WgpuRnd : NgRnd` + the 6
  coupled TUs `DrawRect2D/DofPass/ShadowPass/PostProcPass/BloomPass/…`) **reads DC3's
  2012-era `rndobj` shapes** (`RndMat/RndCam/RndMesh::Vert`) that **RB3-Wii's 2010-era
  `rndobj` cannot compile** — verbatim: *"RB3-Wii's 2010-era rndobj/ is structurally older
  than DC3's and cannot compile those TUs (Wave 2.3 finding)"* (`rb3/native/CMakeLists.txt:67-70`).
  This is *why* RB3 has its own `BandRnd`/`Rnd_Wgpu_RB3.cpp` backend at all. Independently,
  even RB3's *own* `rndobj/` is only partly native-clean: *"22/64 rndobj … TUs do not yet
  compile under clang LP64"* incl. `Trans.cpp`, `Tex.cpp`, `Mesh.cpp`, `Rnd.cpp`
  (`:231-244`). So "RB3 scene graph → DC3 renderer" requires **either** a translation shim
  (RB3-Wii rndobj objects → DC3 `Mesh_Wgpu`/`Tex_Wgpu`/`Part_Wgpu`) **or** RB3-variants of
  the 6 coupled TUs — the latter already the **deferred roadmap Phase 2**
  (`rb3/native/CMakeLists.txt:70`).
- **Adding dc3-decomp/native as a dependency.** DC3's render classes live in a **third
  repo** (`dc3-decomp/native/src/platform/{Mesh_Wgpu,Tex_Wgpu,Part_Wgpu,TransparentQueue,
  RenderState_Native,Rnd_Wgpu}.cpp` and `native/src/gfx/*Pass.cpp`). Note the engine repo
  *already carries* both flavors (`milo-native-engine/src/platform/Rnd_Wgpu*.cpp` +
  `src/gfx/*Pass.cpp`), so a bridge would more likely turn on the engine's existing `dc3`
  flavor for RB3 than pull DC3's repo — but that flavor is gated OFF precisely because of the
  rndobj-shape wall above. License/layout risk is low (same milohax org, sibling decomp
  repos; no LICENSE file found in any of the three trees to flag a conflict) but should be
  confirmed by R2.
- **Emscripten / web-build implications.** Both ports ship dual web builds. RB3's web wiring
  (`rb3/native/CMakeLists.txt:206-223`) **explicitly mirrors DC3's**
  (*"Mirrors DC3's pattern (dc3-decomp/native/CMakeLists.txt:274-283)"*, `:204-205`), and
  DC3 has `native/build-web` + `build-web-release` (confirmed on disk;
  `dc3-decomp/native/CMakeLists.txt:274`). So the Emscripten path is **structurally
  aligned** — a render-backend swap would inherit the same emdawnwebgpu/MEMFS seam on both.
  The web risk is not the build wiring; it is that any RB3-flavor render TUs a bridge adds
  must also survive the `File_Web`/`GpuDevice_Web` exclusion dance (`:216-222`) and the
  28 MB wasm brotli budget (CLAUDE.md web section).

### (3) Alternatives baseline — "keep current path, keep fixing"

Cost measured directly from the campaign's results tables (README wave-by-wave):

- **Cumulative output:** ~**15 default-ON visual fixes** shipped over 31 waves
  (2026-07-05 → 07-12, ≈7 days). Flips cluster in the **middle** waves — 11 of the 15
  defaults landed W6–W16 (`README.md:526-527` "Defaults now NINE"; `:560,585` TEN/ELEVEN;
  `:648` TWELVE).
- **Recent marginal rate (last 12 waves, W20–W31, ≈5 days):** only **~3 default flips** —
  W22 `RB3_HUD_SCOREBOARD_TOPRIGHT` (`README.md:798`), W25 `RB3_IK_REACH_CLAMP` (`:842`),
  W30 `RB3_PROP_POSE_FULL` (`:1014`). **All three are class (c) game-logic/IK, none render-path.**
- **Zero-fix waves are now the norm:** W20 (hands audit, no fix), W21 (hands attempt →
  terminal, `:766`), W23 (2 discriminators, 0 fixes, `:809`), W26 (3 hypotheses died, 0
  fixes, `:850`), W27 (0 fixes), W28 (crowd narrative #5, 0 visible), W29 (crowd = artifact,
  no fix, `:957`), W31 (in progress).
- **Sunk cost on renderer-irrelevant families:** ≈**10 waves** on the hands saga
  (W2–W21 → *terminal*, mitten workaround only) and **6 waves** on the crowd chain
  (W23–W29 → *measurement artifact*, no fix). That is the bulk of the last two weeks'
  effort, and **none of it is a class-(b) target a bridge would change.**

Read: "keep fixing" has **sharply diminishing returns** and its remaining frontier is
class (c). The bridge would *not* accelerate that frontier — it addresses class (b), which
is already largely resolved. So the declining fix-rate is a reason to re-scope the campaign,
not automatically a reason to build the bridge.

## Implications for the bridge

- **The core GO/NO-GO input is NO-GO-leaning on a per-bug basis.** A bridge helps only
  class (b); the campaign's own SYS taxonomy puts the deep/residual pain in class (a)/(c)
  (SYS-1 char-bind, SYS-5 assets, SYS-6 stubs, plus the game-logic families). The class-(b)
  families where DC3 is genuinely ahead are either already fixed in RB3's backend (SYS-3
  DrawContext, placement, wash, UI-post-grade) or near-no-op on RB3's content (SYS-4).
- **The strongest surviving pro-bridge argument is architectural, not per-bug:** SYS-2
  content-coupling — RB3's backend is a growing pile of asset-name `strcmp`, DC3's `WgpuRnd`
  is data-driven. If the goal is *long-term maintainability of the render path*, a
  data-driven backend is attractive. But that is a code-health bet, and the campaign has
  already relocated most of those branches behind flags.
- **Feasibility is gated by the rndobj-shape wall**, the same one that created the two-backend
  split. Any bridge must budget for a Wii-rndobj → DC3-render-object translation layer *or*
  RB3-variants of the 6 coupled TUs (deferred Phase 2) *plus* finishing RB3's own
  rndobj/synth LP64 bring-up (22/64 + 7/47 TUs still not clang-clean).
- **Scoping to "scene graphics only" does not dodge the wall** — scene meshes are exactly
  the rndobj-coupled path (Mesh/Tex/Mat), which is where the 2010-vs-2012 shape divergence
  lives (R3's lane to quantify).
- **Web parity is a non-issue for the decision** (both ports mirror the same Emscripten
  seam); do not weight it heavily either way.

## Confidence + what I could not verify

- **HIGH** on the class-of-fix mapping and the integration wall: both are cited from the
  campaign's own taxonomy (`ARCHITECTURE_REVIEW.md`), the wave results tables
  (`execution/README.md`), and the two CMake configs read directly.
- **HIGH** on the fix-rate baseline (counted from the README wave tables and default-count
  milestones).
- **MEDIUM** on "SYS-4 DC3-ahead pieces are near-no-op": this rests on the campaign's own
  W3.2b/W7 measurements (0 directional lights, Lambert > box-ambient) — true for the
  **boot-reachable venues tested**, not proven for the full venue set; a human-captured
  point-spot-dominant venue could reopen it (`README.md:317`).
- **NOT VERIFIED (defer to sibling lanes):** the *actual* degree of rndobj shape divergence
  between RB3-Wii and DC3-Xbox (R3 owns this — I have only the CMake's assertion that it is
  compile-blocking); the maturity/coverage of DC3's passes vs RB3's needs (R2); and whether
  the engine repo's existing `dc3` flavor could be coaxed to compile against a shimmed RB3
  rndobj without pulling dc3-decomp as a repo (R2). I did not run any build (Wave 31 owns the
  build dir), so all compile claims are from source/CMake comments, not reproduced.
- **License:** no `LICENSE` file exists in rb3, dc3-decomp, or milo-native-engine trees; I
  infer low risk from common org, but this is an inference, not a confirmed license grant.
