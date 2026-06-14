# Render-Polish Campaign — 2026-06-11

Multi-wave ultracode campaign to close the current rendering/gameplay gaps in the
native+web port. Orchestrator loops: **scout → plan → implement (worktrees) →
visually verify → land → next wave**. Docs in this directory are the handoff
artifacts between agents (`scout-<key>.md`, `task-<key>-impl.md`, `verify-<key>.md`).

## Issues (user-reported 2026-06-11)

| key | symptom | wave-1 owner |
|---|---|---|
| `diff-grid` | Song select: per-instrument difficulty grid misaligned when a song is selected | opus scout |
| `char-render` | Gameplay band chars: only teeth/eyes render; legs flicker; legs lifted during "walking" scenes | fable scout |
| `crowd` | Crowd characters merged into a single location, not animating | fable scout |
| `gem-polish` | Sustain tails only render while held; gems flicker in/out; colors off | fable scout |
| `highway-offset` | Highway slightly offset, not head-on to camera like retail | opus scout |
| `all-inst-crash` | Crash when "All Instruments" mode enabled (blocks vocal-display testing) | opus scout |
| `fret-held` | Web guitar emulation: held frets don't show as solid/held on the track | opus scout |
| `menu-lighting` | Main menu lighting looks off vs retail | fable scout |
| `wt-dual-repo` | Polish `tools/setup-worktree.sh` for paired rb3 + milo-native-engine worktrees | opus impl |

## Prior art (read before re-deriving)

- `docs/native/CHAR_SKINNING_DEFORM_INVESTIGATION.md` + `CHAR_GAMEPLAY_WORK_2026-06-09.md`
  — torso rebind + head/hair/hands rest-snapshot rebind LANDED (`0de768a1`, `2580e128`).
  If chars are now mostly-invisible, that is a NEW regression on top of a working state.
- Engine Wave-5 perf landed recently: **L1 vertex-unpack cache + WarmGpuForDir**
  (engine `8fb669d`, pin bump rb3 `38c5ca7e`) and earlier `f75339a`
  (compressed-vertex BE float truncation fix). A vertex-unpack cache is the prime
  suspect for "meshes missing/flickering" regressions — A/B it first if an opt-out
  env exists (grep engine for `getenv`).
- Track lighting / bloom / venue lighting are all default-on with opt-outs:
  `RB3_TRACK_LIGHT_OFF`, `RB3_HIGHWAY_BLOOM_OFF`, `RB3_VENUE_LIGHT_OFF`,
  `RB3_NO_SKEL_REBIND`, `RB3_NO_HEAD_REBIND`. Use these for cheap A/B isolation.
- Crowd triage gotcha (memory): intro-cinematic crowd shards were previously
  mis-triaged as broken gameplay — make sure you're looking at REAL gameplay crowd.
- Two-camera depth: venue cam vs `game.cam` (near 30 / far 224) share depth;
  `BandRnd::ClearDepthForOverlay()` from `TrackPanel::Draw` was the fix for
  highway occlusion. Camera/persp issues likely live in the same neighborhood.

## Harness / ground rules (ALL agents)

- Binary already built: `native/build-native/rb3-native` (rebuild:
  `cmake --build native/build-native --target rb3-native`, ~3s warm — but DO NOT
  build in the main repo concurrently with other agents; scouts are read-only).
- Boot to gameplay headless: `python3 scripts/native/keyboard-to-gameplay.py
  --port <P> --diff hard --out /tmp/rp-<key> --game-burst 24 --verbose`.
  Song select capture: `scripts/native/song-select-capture.py`. HTTP API
  (`RB3_HTTP=1`): `/api/health`, `/api/screenshot`, `/api/input`, `/api/dta/eval`.
- **Each agent uses its ASSIGNED port range** (see your prompt) — concurrent
  agents collide otherwise.
- **NEVER `pkill -f rb3-native`** — kills sibling agents' instances. Kill specific
  PIDs, or `pkill -f "[b]uild-native/rb3-native"` scoped to YOUR worktree path.
- Reference screenshots: `images/retail-screenshots/` (see its README).
  `../xenia` exists for ground-truth capture if needed. If you need a reference
  we don't have, list it in your scout doc under "REFERENCE SCREENSHOTS NEEDED".
- Wave-1 scouts: **no source edits in the main repo, no engine repo edits.**
  Env-var A/B + runtime API + reading code only. Code probes go in a worktree
  (`tools/setup-worktree.sh scout-<key>`), and engine-source probes are FORBIDDEN
  in wave 1 (the engine tree is shared by every worktree).
- Git: stage only files YOU created/changed; never `git add -A`; no stash/revert.

## Scout doc template (`scout-<key>.md`)

1. **SYMPTOM** — repro steps + screenshot paths (`/tmp/rp-<key>/…`), what exactly is wrong.
2. **ROOT CAUSE** — or ranked hypotheses with the evidence for/against each.
3. **FIX DESIGN** — files/functions to change, approach, risk, does it need an
   engine-repo change (say so explicitly), match-neutrality concerns.
4. **VERIFICATION** — exact commands + what a pass looks like.
5. **REFERENCE SCREENSHOTS NEEDED** — exact shots wanted, or "none".

## Status log

- 2026-06-11: campaign opened; wave 1 (9 agents) dispatched.
- 2026-06-11: **wave 1 COMPLETE — 8/8 scouts returned root-cause** (see `scout-*.md`);
  `wt-dual-repo` landed (`d92b2a98`, `setup-worktree.sh --engine`). Headlines:
  - `diff-grid`: wide-atlas CellDiff icon glyph top-anchored → center it (Text.cpp, HX_NATIVE).
  - `char-render`: NOT the W5 unpack cache — StartLoad re-fires mid-song + SetDeformation
    churn wipes the rebind rest snapshot → shard guard drops body meshes ("only teeth/eyes").
  - `crowd`: bone palettes mix TWO same-named Character instances (venue + resident
    tv3-vignette) → shard guard drops crowd bodies. Fix = crowd skeleton rebind.
  - `gem-polish`: tails = engine sMeshGpu cache never invalidates owner-proxy meshes;
    colors = bloom re-adds un-subtracted source; flicker = mostly consequence.
  - `highway-offset`: camRotX default -4.0f should be 0.0f (rb3_native_settings.h:34).
  - `all-inst-crash`: TheNet.mSession never wired (L1 SIGSEGV) + Singer empty-vector
    `&v[0]` asserts (L2 SIGABRT). Both rb3-side.
  - `fret-held`: input chain proven fine; gem_smasher_glow.mat black + textureless —
    per-slot recolor anim not landing on the material natively.
  - `menu-lighting`: engine ignores mUseEnviron (unlit mats get scene-lit) + emissive
    zeroed outside game.cam. Plus separate neon_arcade green-slab decode bug.
- 2026-06-11: wave 2 dispatched — 10 implementers in isolated worktrees (engine tasks use
  `--engine` paired worktrees): the 8 fixes above split into highway-offset, all-inst-crash,
  diff-grid, char-render, crowd (rb3-side) + gem-tails, gem-colors, menu-lighting, fret-held,
  neon-slab (engine-side). Landing order is orchestrator-controlled (3 tasks touch
  Rnd_Wgpu_RB3.cpp in different regions → sequential cherry-pick + single pin bump).
- Reference screenshots requested from user: see `REFERENCE_SCREENSHOTS_NEEDED.md`
  (P0: GP-1 approach-tail, FH-1 held-fret glow, ML-1 Wii hub loop, ML-2 ARCADE neon closeup).
- 2026-06-11: **wave 2 COMPLETE — 9/10 verified, all LANDED.** rb3 master `5248158d..cca1869a`
  (highway camRotX, vocal-crash TheNet+Singer+VocalTrack, diff-grid icon centering, crowd
  inverse-bind rebake −89.5% drops, char reload-re-entrant rebind, Part.cpp InitParticle
  decomp fix 95.3→97.7 — the "neon slab" was actually runaway street-fog particles);
  engine main `eda796d..469c550` (mesh-cache owner-gen → approach tails, outer-halo bloom →
  saturated gems, unlit+emissive-all-cams → menu lighting, smasher glow → held frets,
  shard-guard diagnostics); single pin bump `cca1869a`. All cherry-picks clean, Wii report
  regenerated, smoke boot-to-gameplay green on the composed build.
  - `char-render` is PARTIAL: bodies/heads/hands render + persist now, but own==bound
    garments still guard-hidden — root cause = C8 pose-pipeline rotation-basis divergence
    (follow-up below). Levers in place: RB3_BOUND_REBAKE, RB3_GUARD_EXEMPT_REBOUND, SHARD_DBG.
- New follow-ups queued for wave 3+: C8 rotation-basis deep dive; gem-flicker re-triage
  (post-fix); venue pink-wash adjudication (authored stage lighting vs emissive blowout?);
  song-select FRIEND-RANKINGS overlay + grey album-art box obscuring the grid; endgame
  SIGABRT (`ui/endgame/endgame_helpers.dta(64):meta_performer`, seen by crowd scout);
  crowd residual ~6.7k drops; crowd Fix B (2D imposters) + Fix C (venue bridge) deferred.
- 2026-06-11: wave 3 dispatched — 7 adversarial verifiers (composed build) + C8 deep dive.
- 2026-06-14: **wave 3 RECOVERED after a multi-day pause** (Fable now unavailable). All 7
  verify docs + the recovered `scout-c8-rotation-basis.md` are on disk; synthesis in
  **`WAVE3_RESULTS.md`**. Scoreboard: highway/diff-grid/crowd/gems(×4) **PASS**, vocals
  **PASS** (caveat: pre-existing endgame abort), menu-lighting/neon-slab **PARTIAL**
  (Part.cpp fix revived menu fog → wash; contrast win didn't reproduce), char-render
  **PARTIAL** (C8 root-caused + fix ready), venue-wash **FAIL** (lighting blowout, shared
  shader, wave-2 exonerated).
  - **C8 root cause** (refutes the old "rotation-basis" framing): rest captured in WORLD
    space (incl. member stage placement) vs model-space verts → `R·sinθ` smear → guard
    drops. Fix = capture rest in CHARACTER space + never capture mid-clip. Measured
    locality 27–60u→5–12u, band drops 25.2→20.4/frame, Wii byte-identical. Committed
    `wt-c8-deep-dive` rb3 `41ff9e97` + engine probe `6a324be` — **NOT landed** (needs
    composed-build visual sign-off). Residual after fix = left-limb IK mispose
    (`RB3_NO_IK` A/B → 4.9/frame): separate follow-up.
  - **4 new issues** for wave 4: (1) fret-held white-sphere regression (venue-dependent;
    `RB3_FRET_GLOW_OFF=1` mitigates), (2) menu fog wash (Part.cpp × menu interaction),
    (3) venue lighting blowout (unbounded lighting sum in shared standard shader,
    pre-existing), (4) endgame abort (pre-existing, instrument-agnostic).
  - Wave-3 verify worktrees + the `c8-deep-dive` worktrees (rb3 + engine) left in place;
    the C8 branches hold un-landed work — do NOT prune before landing/teardown.
- 2026-06-14: **C8 fix LANDED + independently reviewed (CONFIRM_WITH_RESIDUALS).** master
  `491288ec` (character-space rest-bake, BandCharacter.cpp +48/−2, HX_NATIVE-only) +
  `d4c42fa8` land record + `dc1fdadd` independent verify. Engine pin unchanged (469c550 —
  the engine probe `6a324be` is diagnostics-only, NOT landed). Wii match-neutral CONFIRMED
  two ways (BandCharacter fuzzy stays 99.67018; `.text/.rodata/.data/.sdata` + rela byte-
  identical). Drops: implementer 27.2→23.0/frame (−15%, bimodal — clip-free-capture boots
  reach 16-17); reviewer's cleaner true-pre-C8 A/B 31.2→20.6/frame (−34%). 0 crashes / NaN /
  regressions in 9+ runs; crowd + head/hands still coherent. `task-c8-land-impl.md` +
  `verify-c8-land.md` on disk.
  - **Residual sharpened by review:** the remaining drops are an IK-apply class — and it's
    HAND/FINGER IK as much as left-limb legwear: `fingernails_resource.mesh` is the single
    largest contributor and collapses ~18-30k→2166 under `RB3_NO_IK=1` (which takes the whole
    residual to ~5-7.6/frame). The wave-4 IK follow-up is hand+leg IK (likely an engine-side
    IK-apply / L-R-handedness bug; cf. DC3 feet-in-floor).
  - Reviewer nits: master HEAD is a docs commit atop `491288ec` (code state IS the fix);
    venue pink-wash blowout (separate WAVE3 FAIL) makes lit-venue garment screenshots hard →
    the drop-rate metric is the authoritative garment gate, not hero closeups.

## Campaign standing (2026-06-14)

8 user-reported issues: **7 fixed + verified** (highway, vocals/all-instruments, diff-grid,
crowd, gem tails, gem colors, fret-held*) and **char-render now substantially fixed + landed**
(C8 space-bake; residual = IK follow-up). *fret-held has a venue-dependent white-sphere
regression (mitigated by `RB3_FRET_GLOW_OFF=1`). **Open for wave 4:** (1) hand+leg IK mispose
(the C8 residual); (2) fret-held white-sphere; (3) venue lighting blowout = clamp/tone-map the
unbounded lighting sum in the shared standard shader; (4) menu fog density (Part.cpp × menu);
(5) endgame abort `endgame_helpers.dta(64):meta_performer` (pre-existing, instrument-agnostic).
Worktree teardown (scout-*, task-*, c8-deep-dive in both repos) pending campaign close.
- 2026-06-14: **wave 4 implement dispatched** — 5 Opus agents (investigate + fix + self-verify
  in isolated worktrees), one per open issue: `ik-mispose` (engine; the C8 residual — hand+leg
  IK-apply; may land partial = diagnosis), `fret-sphere` (engine; venue-dependent white-ball
  glow), `venue-blowout` (engine; clamp/tone-map the unbounded lighting sum in standard_wgsl.inc
  — HIGH blast radius, multi-scene gate), `menu-fog` (engine/rb3; revived fog too dense), and
  `endgame-abort` (rb3; the meta_performer SIGABRT, gdb-backtrace first). Three touch the engine
  shader/Rnd files → orchestrator lands sequentially with one pin bump (wave-2 pattern). Review
  wave to follow on the composed build.
- 2026-06-11: `fret-held` scout DONE (`scout-fret-held.md`). ROOT CAUSE: NOT
  input — the full message→GuitarController→GemSmasher::SetGlowing(true) chain
  works in native (proven via FRET_DBG worktree probe: 40 presses → 40 OnMsg →
  40 SetGlowing b=1, glow mesh non-null + showing). The glow is invisible because
  `gem_smasher_glow.mat` is `color=(0,0,0)` + **no diffuse texture bound** +
  `kBlendAdd`, so the standard shader's `baseColor = matColor*texture` → 0 →
  additive contributes nothing. Upstream: the per-slot recolor
  (`set_color`/`particle_slot_colors.anim` binding `square_smasher_bright_*.tex`)
  isn't landing on the glow material on native (A1-hit-flame-class FX gap).
  Needs an engine-repo change (anim→material apply, or additive-glow shader
  safety net). Same neighborhood as the emissive-glow work.
