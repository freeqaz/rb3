# RB3 Native — Retail-Parity Roadmap

**Authored:** 2026-05-28 (planning subagent, Opus, read-only on source).
**Goal:** drive the native Linux/clang/LP64 RB3 build (links against
`milo-native-engine`) closer to 1:1 with retail Rock Band 3 — fill the remaining
visual/feature gaps. User priorities: **(1) render the loading screen while songs
load, (2) overall 1:1 visual fidelity** (venue/stage 3D and live score digits
weighted highest as the most-visible gaps).

**Baseline state (verified):** boots → menus (~80% recognizable) → loads a song
(`Game::mLoadState=kReady`) → real audio plays → **full-song guitar gameplay**
renders (centered down-highway camera, 5-color GRYBO strike plate, gems flow the
*entire* song after the V17 nofail fix, sustains, side-rails). Exits clean (code 0).

> **Important correction to the V16 review's leading hypothesis.** V16 blamed the
> "gem stream dies mid-song" on a *still-stubbed `MidiParserMgr` ctor*. That is
> **stale**: `nm build-native/rb3-native` shows `_ZN13MidiParserMgrC1...`,
> `...4PollEv`, `...5ResetE*` all resolve to **`T` (strong)** — `MidiParserMgr.cpp`
> compiles via the `ENGINE_MIDI` glob (`native/CMakeLists.txt:239`) and the weak
> stubs in `band3_link_stubs.s:295-308` are dead. The V17 fix (synthetic `nofail`
> directive, `MetaPerformer::SetBandNoFail`, `rb3_game_input.cpp:177-182`) already
> fixed the actual root cause (player booed off → `SetGemsEnabled(-1)`); memory
> note `rb3-gameplay-loop-achieved.md` confirms gems now flow the whole song. So
> the gem stream is **NOT** a top gap anymore — it is resolved. This roadmap
> reflects that.

---

## Three-layer reminder (every item tags its layer)

| Layer | Lives in | Compiles under | Permuter? | Prefer fixes here? |
|---|---|---|---|---|
| **(a) Matched fork** | `rb3/src/system/**`, `rb3/src/band3/**` | MWCC PPC (asm-match) | YES — never `git add -A` | only additive `#ifdef HX_NATIVE … #else … #endif`, `#else` byte-identical to permuter |
| **(b) Engine runtime** | `milo-native-engine/src/**` | clang LP64 (on link line) | no | shared across 3 decomps — change with care |
| **(c) Per-decomp glue** | `rb3/native/src/**` | clang LP64 (on link line) | no, no asm-match | **YES — prefer this layer** |

Shared-file-scope hot spots that force serialization: `native/CMakeLists.txt`,
`milo-native-engine/src/platform/Rnd_Wgpu_RB3.cpp`, `native/src/band3_link_stubs.s`,
`native/src/main_native.cpp`, `native/src/rb3_game_input.cpp`,
`native/src/rb3_http_server.cpp`.

---

## Work items (each with retail vs current, root cause + files, effort, layer, agent fit, concurrency, priority)

### G1 — Loading screen while songs load  *(PRIORITY 1 — user headline; parallel agent active)*

- **Gap:** the song-load transition (the `game_panel_load` window, ~frames 456→ready)
  shows no loading UI — black/static during the multi-second `Game::LoadSong`.
- **Retail:** a load screen with song art / tip text / a progress meter while the
  song streams in (RB3's `meta_loading` + `loading` UIs).
- **Current native:** load happens but the load-screen panel doesn't render. The
  assets exist: `orig-assets/extracted/ui/loading/loading.dta`,
  `ui/meta_loading.dta`, `ui/gen/meta_loading.milo_xbox`,
  `ui/global/gen/content_loading.milo_xbox`,
  `ui/resource/gen/content_loading_meter.milo_xbox`.
- **Suspected root cause + files:** the in-game load flow is driven by
  `GamePanel`'s `mLoadProf("game_panel_load")` (`rb3/src/band3/game/GamePanel.cpp:60`)
  and the `ui/game.dta` `pick_intro` / `game_screen` `(load …)` script chain
  (`game.dta:30,270`). The `meta_loading` interstitial is requested via
  `interstitial_mgr pick_interstitial_between_screens meta_loading_outro …`
  (`game.dta:245`). The `ContentLoadingPanel` TU
  (`rb3/src/band3/meta_band/ContentLoadingPanel.cpp`) is the *"finding additional
  content"* panel (DLC scan) — **not** the song-load screen, so don't conflate
  them. The song-load screen is the `meta_loading` UIScreen + whatever `GamePanel`
  shows during `mLoadProf`. Likely the panel is loaded but never `SetShowing(true)`
  in the headless flow, or its proxy/anim doesn't run (same class of issue as the
  InterstitialPanel camshot gate, G_VENUE).
- **Effort:** M (1–3 days) — depends on what the parallel agent finds.
- **Layer:** investigation may land in (c) glue (force the panel showing) or (a)
  matched-fork additive (drive the `meta_loading` screen-flow); prefer (c).
- **Agent fit:** **Opus** (subjective "is the load screen rendering correctly" +
  multi-system screen-flow debug; visual review is Opus-only per user pref).
- **Concurrency:** **A parallel agent is already on this** (task #80 "V18:
  load-screen render investigation"). **Do not dispatch a second agent against
  the same scope.** Reference/coordinate. If it touches `rb3_game_input.cpp`
  (to add a directive that shows the panel) it serializes with G_SCORE/G_FX which
  also may touch that file.
- **Priority:** **1.**

### G_VENUE — Venue / band / crowd 3D stage (black-void background)  *(PRIORITY 2 — biggest fidelity tell)*

- **Gap:** gameplay (and menus' 3D backdrops) render the highway/UI over a pure
  black void — no venue geometry, no band characters, no crowd, no stage lighting.
- **Retail:** a lit 3D stage with an animated band behind the gem highway; menu
  backdrops (e.g. main_hub amp props).
- **Current native:** `WorldInstance::SyncDir` SKIPS proxy-instancing for cosmetic
  venues under `world/vignette/` and `world/shared/` (verified
  `rb3/src/system/world/Instance.cpp:304-361`, the `HX_NATIVE` `IsCosmeticVenue`
  gate + `world/vignette/`/`world/shared/` strstr at :356). The
  `InterstitialPanel::Exiting` / `BackdropPanel::Exiting` camshot/outro gates are
  short-circuited (`rb3/src/band3/meta_band/InterstitialPanel.cpp:21-82`) because
  the venue anim never drives `transition_camshot_done.trg`.
- **Suspected root cause + files:** the V5b audit (`DIVERGENCE_AUDIT.md` hack #2)
  found three structural obstructions in `ObjectDir::PostLoadInlined`
  (`rb3/src/system/obj/Dir.cpp:152`, called at `:429`) for inlined-cached-shared
  dirs:
  (a) `WorldInstance::LoadPersistentObjects` (`Instance.cpp:159`) save/restores
  `mDir->Dir()` mid-load (`:288-296`) — wiring parent `Dir()` before this corrupts
  the proxy hash table;
  (b) shared dirs (e.g. `classic_blacktriple.milo`) are reused across MULTIPLE
  proxy parents — a single `mDir` pointer can't name them all;
  (c) `Hmx::Object::Copy` doesn't copy `mDir`.
  A tested seam already exists: `HxSetDir(ObjectDir*)` in `Object.h`. Proper fix:
  defer parent-wiring until after LoadPersistentObjects (a), per-proxy shadow dirs
  for many-to-one (b), explicit `HxSetDir`+`SetName` reconciliation (c). Once
  venues instance cleanly the camshot anim runs and the two `Exiting()`
  short-circuits flip naturally (so this also retires DIVERGENCE hacks #2a/#2b).
  Band characters additionally need `BandDirector` (`band3_link_stubs.s:214-217`
  `HarvestDircuts`/`ReadyForMidiParsers` are no-op stubs) and `BandCharacter`
  (`band3_link_stubs.s:252` `NameToDrumVenue` stub) brought up, plus skinned-char
  draw (engine `Rnd_Wgpu_RB3.cpp` skinned path landed in V14a, so character meshes
  *should* draw once instanced).
- **Effort:** **L (~1 week)** for the venue-proxy core; +M for band characters on
  top.
- **Layer:** mostly (a) matched-fork additive (`Dir.cpp`, `Instance.cpp`,
  `Object.cpp`/`.h`); possibly (c) glue helpers. Engine (b) `Rnd_Wgpu_RB3.cpp`
  only if the skinned/lit draw needs extension.
- **Agent fit:** **Opus** (deep multi-system decomp + structural design + visual
  review).
- **Concurrency:** mostly self-contained in the `obj/world` matched-fork tree;
  conflicts with nothing else *except* if it needs an engine `Rnd_Wgpu_RB3.cpp`
  change (then serialize vs G_GEMPOLISH). Long pole — start early, run alongside
  the cheaper items.
- **Priority:** **2** (highest-value fidelity gap; long, so kick off early).

### G_SCORE — Live numeric score / multiplier HUD  *(PRIORITY 3 — second-most-visible)*

- **Gap:** no live score digits or multiplier number on the gameplay HUD.
- **Retail:** numeric score top-center, colored multiplier ring, streak.
- **Current native:** the old `%d%%` leak is gone (V14b), but no value renders.
  Note `solo_percent.lbl` is a *solo-section* overlay (correctly hidden when no
  solo) — **not** the main score. The main HUD score path is:
  `ScoreTracker::Poll_` sums `p->GetScore()` into `mScoreTotal`
  (`rb3/src/band3/game/ScoreTracker.cpp:18-23`) → `TrackPanel`'s scoreboard
  (`mScoreboard = …Find<BandScoreboard>("scoreboard")`,
  `rb3/src/band3/bandtrack/TrackPanel.cpp:112`,435,508) → a milo `BandScoreboard`
  / `AppLabel` in `tracksystem.milo`.
- **Suspected root cause + files:** **`TrackPanel::TrackerDisplayReset` is a weak
  no-op stub** (`band3_link_stubs.s:157-158`
  `_ZN10TrackPanel19TrackerDisplayResetEv → __hmx_band3_noop_stub`) even though
  `TrackPanel.cpp:526` has a real impl — investigate whether `TrackPanel.cpp` (or
  the `BandScoreboard` TU) is excluded / not winning the strong def, or whether
  the scoreboard label is simply never fed. Verify the
  `TrackPanel::SendTrackerDisplayMessage` (`:508`) → scoreboard label update fires
  during gameplay. The `TrackerDisplay`/`TrackerBandDisplay`/`TrackerPlayerDisplay`
  classes (`rb3/src/band3/game/TrackerDisplay.cpp`) are real and compiled; the
  break is likely the scoreboard wiring or a missing label in the 360-ARK milo
  (cf. `AppScoreDisplay::UpdateDisplay` null-label skip, DIVERGENCE Pattern 4).
  Also confirm `ScoreTracker` is actually constructed/polled in the native
  gameplay flow.
- **Effort:** M (1–2 days; could be S if it's just the stale `TrackerDisplayReset`
  stub + a label-feed).
- **Layer:** (a) matched-fork (`TrackPanel.cpp` if a real method needs to win over
  the stub) + (b)/glue stub removal in `band3_link_stubs.s`; prefer landing the
  fix in (a)/(c).
- **Agent fit:** **Opus** (scoring→label wiring is multi-hop + needs a visual
  confirm the digits read correctly). A Sonnet could do the mechanical stub
  removal if the root cause turns out to be just that.
- **Concurrency:** touches `band3_link_stubs.s` (shared with G_GEMPOLISH stub
  cleanups and G_FX) → **serialize `band3_link_stubs.s` edits.** Otherwise
  independent.
- **Priority:** **3.**

### G_FX — Hit / flame FX (gem_mash bursts, fire-on-hit)  *(PRIORITY 4)*

- **Gap:** no hit-burst / flame FX when a gem registers a hit; the strike-plate
  flare is a static disc (V16 review #4).
- **Retail:** colored flame bursts on the lane when a gem is hit; star-power
  flames.
- **Current native:** `gem_mash0..5` skinned meshes load (V16 log), and the V14a
  skinned-mesh path draws them, but the hit-triggered FX animation/particle path
  isn't firing. Note in headless there are no real note-hits (the `nofail`
  directive keeps gems flowing but nobody "hits" them), so the hit-FX trigger may
  simply never fire — partly a synthetic-input gap (need a `hit`/strum directive)
  as much as a render gap.
- **Suspected root cause + files:** `rb3/src/band3/game/GuitarFx.cpp`,
  `rb3/src/band3/bandtrack/` gem_mash hit handlers; `GemManager`/`Gem` hit
  callbacks; particle/anim draw in engine `Rnd_Wgpu_RB3.cpp` if particles aren't
  rendered. Likely needs (i) a synthetic note-hit input verb in
  `rb3_game_input.cpp` to *trigger* FX, then (ii) confirm the FX mesh/particle
  draws.
- **Effort:** M (2–3 days), partly gated on whether particles render at all in the
  engine layer.
- **Layer:** (c) glue (input verb in `rb3_game_input.cpp`) + (a) matched-fork
  (GuitarFx) + possibly (b) engine `Rnd_Wgpu_RB3.cpp` (particles).
- **Agent fit:** **Opus** (visual + multi-system, and uncertain whether particles
  render).
- **Concurrency:** touches `rb3_game_input.cpp` (shared with G1 load-screen and
  G_SCORE if they add directives) and possibly `Rnd_Wgpu_RB3.cpp` (shared with
  G_VENUE / G_GEMPOLISH). **Serialize those two files.** Lower priority — do after
  score + venue.
- **Priority:** **4.**

### G_TOKENS — Placeholder token strings still showing raw text  *(PRIORITY 5)*

- **Gap:** placeholder tokens like `SHELL_PRESS_START_TO_ROCK` show raw token text
  instead of localized strings (and the V16 "`%S %I SONGS` uppercase wide-string
  variant" setlist header).
- **Retail:** localized display strings.
- **Current native:** the V5c `LocaleChunkSort::FastSort<3>` LP64 fix recovered
  ~50% of locale lookups, but specific tokens still leak. (Note: a grep for
  `SHELL_PRESS_START_TO_ROCK` in `src/`/`native/` found no literal — the token is
  data-driven from the locale `.dta`, so the leak is a lookup-miss, not a hardcoded
  string. Confirm the exact failing tokens at runtime.)
- **Suspected root cause + files:** locale lookup path
  `rb3/src/system/utl/Locale*.cpp` + `LocaleChunkSort` (V5c site); the wide-string
  `%S`/`%I` printf-style substitution in `Locale`/`UILabel` format path. Likely a
  remaining LP64 width/format bug in a sibling of the V5c fix, or a missing entry
  in the extracted locale `.dta`.
- **Effort:** S (½–1 day) per token-class; mechanical once located.
- **Layer:** (a) matched-fork additive (Locale) or asset-side (locale .dta).
- **Agent fit:** **Sonnet** (mechanical — find the failing lookup, mirror the V5c
  fix pattern; confirm visually only at the end). Visual confirm step can be a
  brief Opus check or deferred to a batch review.
- **Concurrency:** self-contained (Locale TU). Parallel-safe with everything.
- **Priority:** **5.**

### G_MIDI — 25or6to4 MIDI stack-smash (PART REAL_GUITAR parse)  *(PRIORITY — robustness, not visual; do opportunistically)*

- **Gap:** buffer overflow / stack-smash parsing `25or6to4`'s MIDI during
  `PART REAL_GUITAR` — crashes that song (and any with similar real-guitar text
  events).
- **Retail:** parses fine.
- **Current native:** ASan-localizable per prior notes.
- **Suspected root cause + files:** two fixed-size stack buffers on the MIDI text
  path are the prime suspects —
  `rb3/src/system/midi/MidiParserMgr.cpp:167` `char buf[256];` in `ParseText`
  (guarded by `MILO_ASSERT(strlen(str) < 256)` at :170 — but `StripEndBracket`
  writes into `buf` from `str+1`; if a real-guitar text event is exactly 255+ chars
  or the bracket-strip miscounts, it overflows), and
  `rb3/src/system/midi/MidiReader.cpp:329` `char buf[0x100];` (writes `buf[numVal]`
  with a `numVal >= 0x100` guard at :330 — verify the off-by-one
  `buf[numVal]='\0'` when `numVal == 0xFF`). Run under ASan on `25or6to4` to pin
  which one.
- **Effort:** S (½–1 day) once ASan localizes it.
- **Layer:** (a) matched-fork additive (`#ifdef HX_NATIVE` bounds clamp).
- **Agent fit:** **Sonnet** (mechanical ASan-guided bounds fix). Needs `25or6to4`
  available in the extract; the current canonical test song is `20thcenturyboy`,
  so confirm the asset is present first.
- **Concurrency:** self-contained (midi TUs). Parallel-safe. Note: only matters if
  we test `25or6to4` specifically — not blocking the canonical-song visual work.
- **Priority:** **6** (robustness; opportunistic — bundle with G_TOKENS as a
  Sonnet wave).

### G_HTTP — HTTP scene-root tooling (rebuild-free introspection)  *(PRIORITY — force multiplier; do FIRST)*

- **Gap:** `/api/dta/eval` resolves expressions from `gDataDir` (the global
  DataReadString context), **not** the live gameplay scene root, so you can't
  `find` gameplay objects (game.cam, track widgets, scoreboard) without their full
  data path. `/api/scene/tree` and `/api/objects` are unimplemented. Flagged 3×
  across prior docs as the single thing that makes all remaining investigation
  **rebuild-free**.
- **Retail:** N/A (dev tooling).
- **Current native:** `rb3_http_server.cpp` has `/api/health`, `/api/screenshot`,
  `/api/dta/eval` (:289), `/api/input` (:314) — verified. No scene-tree / objects /
  scene-root resolver.
- **Suspected root cause + files:** `rb3/native/src/rb3_http_server.cpp` — add
  (i) a `{rb3_find <path>}` DTA resolver rooted at the live gameplay scene root
  (the `game_screen` UIScreen / `TrackPanelDir` dir), (ii) `/api/scene/tree`
  (walk the live `ObjectDir` graph), (iii) `/api/objects` (list/inspect). The DC3
  HTTP server (`dc3-decomp/native/src/platform/HttpServer.cpp`) has `/api/objects`
  + `/api/scene/tree` implemented — port those handlers. The eval primitive
  `DataReadString` is at `rb3/src/system/obj/DataFile.cpp:615`.
- **Effort:** M (1–1.5 days; the DC3 handlers are a near-verbatim port).
- **Layer:** (c) glue (`rb3_http_server.cpp`) only.
- **Agent fit:** **Sonnet** (mechanical port of existing DC3 handlers; no
  subjective judgment). Could be Opus if the scene-root resolution proves subtle.
- **Concurrency:** touches `rb3_http_server.cpp` only — no overlap with any visual
  item. **Fully parallel-safe.** **Do this FIRST** — it makes G_VENUE / G_SCORE /
  G_FX / G1 investigations rebuild-free (mutate camera/objects/visibility on a
  live booted instance, re-screenshot over HTTP).
- **Priority:** **0** (enabler; cheap, unblocks everyone — front-load it).

---

## Priority-ranked summary

| Rank | Item | Gap | Effort | Layer | Agent |
|---|---|---|---|---|---|
| **0** | G_HTTP | scene-root tooling (rebuild-free enabler) | M | (c) glue | Sonnet |
| **1** | G1 | loading screen while songs load | M | (c)/(a) | Opus *(parallel agent active)* |
| **2** | G_VENUE | venue/band/crowd 3D (black void) | **L ~1wk** | (a) | Opus |
| **3** | G_SCORE | live numeric score/multiplier HUD | M | (a)/(c) | Opus |
| **4** | G_FX | hit/flame FX | M | (c)/(a)/(b) | Opus |
| **5** | G_TOKENS | placeholder token strings | S | (a)/asset | Sonnet |
| **6** | G_MIDI | 25or6to4 MIDI stack-smash | S | (a) | Sonnet |

**Already resolved (not a gap):** gem-stream-dies-mid-song (V17 nofail fix; gems
flow full song — supersedes the V16 MidiParserMgr hypothesis, which was stale).

---

## Shared-file-scope serialization map

| File | Items that touch it | Rule |
|---|---|---|
| `native/src/rb3_http_server.cpp` | G_HTTP only | isolated — safe anytime |
| `native/src/rb3_game_input.cpp` | G1(maybe), G_SCORE(maybe), G_FX(hit verb) | **serialize** — one agent at a time, or land additive non-overlapping verbs |
| `milo-native-engine/src/platform/Rnd_Wgpu_RB3.cpp` | G_VENUE(maybe), G_FX(particles), G_GEMPOLISH(future) | **serialize** any engine-render edits |
| `native/src/band3_link_stubs.s` | G_SCORE(TrackerDisplayReset), G_VENUE(BandDirector/BandCharacter) | **serialize** stub edits |
| `native/CMakeLists.txt` | any TU-bring-up (G_VENUE, G_SCORE) | **serialize** CMake edits |
| `src/system/obj/Dir.cpp`, `src/system/world/Instance.cpp`, `Object.*` | G_VENUE only | isolated to G_VENUE |
| `src/system/utl/Locale*.cpp` | G_TOKENS only | isolated |
| `src/system/midi/*.cpp` | G_MIDI only | isolated |

Permuter caution: `Dir.cpp`, `Instance.cpp`, `Object.cpp`, `Locale*.cpp`,
`midi/*.cpp`, `TrackPanel.cpp`, `GuitarFx.cpp` are all matched-fork — edits MUST be
additive `#ifdef HX_NATIVE … #else …`, stage explicit file whitelists, never
`git add -A`, and re-apply if the permuter shifts the file underneath.

---

## Recommended dispatch plan (concrete, for the coordinator)

Six dispatches across three waves. Waves gate on file-scope + the enabler.

### Wave 1 — enabler + long pole + cheap parallel (launch together)

1. **Dispatch A — G_HTTP scene-root tooling.** **Sonnet.** Scope:
   `native/src/rb3_http_server.cpp` only (port DC3's `/api/scene/tree`,
   `/api/objects`, add `{rb3_find}` rooted at the live gameplay scene). Isolated
   file — zero conflict. **Front-load it: the moment it lands, every later
   investigation is rebuild-free.** ~1 day.

2. **Dispatch B — G_VENUE venue/band/crowd 3D.** **Opus.** Scope: matched-fork
   `obj/Dir.cpp` (`PostLoadInlined`), `world/Instance.cpp` (`SyncDir` /
   `LoadPersistentObjects`), `Object.*` (`HxSetDir`), and the `BandDirector` /
   `BandCharacter` stubs. The ~1-week long pole — **start it first** so it runs
   while the shorter items complete. Isolated to the obj/world tree (only collides
   with others if it needs `Rnd_Wgpu_RB3.cpp` or `band3_link_stubs.s` — coordinate
   those edits through this dispatch since it's the long-running owner).

3. **Dispatch C — G_TOKENS + G_MIDI (combined Sonnet wave).** **Sonnet.** Scope:
   `utl/Locale*.cpp` (find + fix the leaking tokens, mirror V5c) **and**
   `midi/MidiParserMgr.cpp:167` / `MidiReader.cpp:329` (ASan-localize the
   25or6to4 stack-smash, additive bounds clamp). Two isolated TU sets, both
   mechanical, no overlap with A/B. ~1 day.

> **Already in flight (do NOT re-dispatch):** **G1 loading screen** — task #80 is
> active (Opus). Treat it as a running Wave-1 dispatch; coordinate if it needs
> `rb3_game_input.cpp` (then it serializes with the Wave-2 G_SCORE/G_FX directive
> edits).

### Wave 2 — score (after Wave 1 frees shared files)

4. **Dispatch D — G_SCORE live score/multiplier HUD.** **Opus.** Scope: confirm
   `TrackPanel::TrackerDisplayReset` strong-def-vs-stub, wire `ScoreTracker` →
   `BandScoreboard` label. Touches `band3_link_stubs.s` (stub removal) and
   possibly `TrackPanel.cpp` / `CMakeLists.txt` — **serialize the
   `band3_link_stubs.s` + CMake edits with Dispatch B (G_VENUE)**; run after B's
   stub/CMake edits settle, or hand B the stub-removal as part of its scope.
   Benefits directly from G_HTTP (inspect the scoreboard label live). M.

### Wave 3 — FX (last; depends on score + engine-render serialization)

5. **Dispatch E — G_FX hit/flame FX.** **Opus.** Scope: add a synthetic note-hit
   verb in `rb3_game_input.cpp` (serialize vs any other `rb3_game_input.cpp`
   owner — coordinate with G1), wire `GuitarFx` / gem_mash hit FX, and (if needed)
   extend particles in `Rnd_Wgpu_RB3.cpp` (**serialize vs G_VENUE's engine
   edits**). Lowest priority of the visual gaps; do after score lands. M.

### Sequencing rationale

- **G_HTTP first (cheap, isolated, force-multiplier)** so B/D/E/G1 iterate
  rebuild-free.
- **G_VENUE second-but-concurrent** because it's the ~1-week long pole and the
  highest-value fidelity gap — it must run in the background while shorter items
  finish.
- **G_TOKENS+G_MIDI as one Sonnet wave** alongside — fully isolated, knock them
  out cheaply.
- **G_SCORE then G_FX** serialized after the shared files (`band3_link_stubs.s`,
  `rb3_game_input.cpp`, `Rnd_Wgpu_RB3.cpp`, `CMakeLists.txt`) are free.
- **Visual reviews are Opus-only** (user pref): each visual item (G1, G_VENUE,
  G_SCORE, G_FX) ends with an Opus screenshot review; the Sonnet items (G_HTTP,
  G_TOKENS, G_MIDI) need only a brief Opus visual confirm or can fold into the next
  batch review.

**Concurrency at a glance:** Wave 1 runs **A (Sonnet) + B (Opus) + C (Sonnet) +
G1 (Opus, already active)** in parallel — four-up, no file conflict. Wave 2 (D,
Opus) and Wave 3 (E, Opus) serialize behind B's shared-file edits.
