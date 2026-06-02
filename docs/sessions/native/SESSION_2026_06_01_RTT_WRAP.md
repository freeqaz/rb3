# Session wrap — Offscreen-RTT hack-unwind + PostProc/Tier-2 (2026-06-01)

**End state:** rb3 `df431c62`, engine `main` `1a1f84e`, `MILO_ENGINE_PIN` matches,
rb3 build no-op, web rebuilt + verified. All target hacks retired.

## What landed

The premise — "offscreen-RTT is a native no-op" — was **stale**. char-bone skinning
and mesh-RTT already worked; the real gaps were `DrawRect` and per-screen PostProc.
Across this + prior sessions (engine `070562a → 1a1f84e`):

| Work | Where | Result |
|---|---|---|
| `BandRnd::DrawRect` (self-contained 2D textured/tinted quad + shared pipeline) | engine `5cfaf30` | textured 2D quads render |
| Per-screen PostProc (offscreen intermediate → grade composite, honors `RndPostProc::Current()`) | engine `f7e0fd6` | grade applies per-screen |
| V2: texture noise grain (`kNoiseGain`+midtone mask, no gray-wash) + `BloomPass` wired | engine `dc95098` | grain/bloom |
| **Tier-2 venue-only grade layering** (`FlushPostProcMidFrame`/`ClearDepthForOverlay`, driven by `TrackPanel::Draw`) | engine `132add5` | gameplay HUD/gems stay **colored** over a B&W-graded venue |
| Outfit `DrawPreClear` **default-on** (outfit RTT tints) | engine `1a1f84e` | opt-out `RB3_NO_PRECLEAR` |
| song_select **cover-hide + etched-hide** hacks retired | rb3 `017c20ef` | skinning/RTT caught up |
| **details-pane `RB3_NO_DETAILS_FIX`** hack retired (redundant since AnimTask arg-swap `ca671682`) | rb3 `0f42c529` | engine hides the pane itself |

**Gates:** `RB3_PP_OFF`, `RB3_RTT_OFF`, `RB3_NOISE_OFF`, `RB3_BLOOM_OFF`,
`RB3_NO_TRACK_DEPTH_CLEAR`, `RB3_NO_PRECLEAR`.

**Non-bugs ruled out:** the "gameplay grayscale" scare was the song's *authored* intro
B&W camera shots — steady-state (songMs > ~25 000) is fully colored. No value bug.

## Remaining polish (low-ROI)

- **Full-fidelity PostProc noise** — `kNoiseGain=0.04` is conservative vs the Wii's
  texture-driven grain. Cosmetic; only open RTT item.

## How we worked (subagents / workflows)

- **Scoping workflow** → parallel investigators (barrier) → synthesis, producing the
  three plan docs below before any code.
- **Isolated-engine-change mechanic** (the engine is shared + soft-pinned): engine
  git-worktree on a branch + reconfigure the warm `native/build-native` via
  `-DMILO_ENGINE_PATH=<worktree>` (reuses 308 rb3 objects, ~27 s), verify, merge →
  engine `main`, bump pin, reconfigure back.
- **Gated sequential impl stages** with StructuredOutput schemas; one focused
  background `Agent` for the last (details-pane) hack.
- **Concurrent-session reconciliation:** another freeqaz session advanced engine+rb3
  mid-flight; reconciled via `git merge main` into the worktree branch (resolve dup
  same-named overrides → redefinition), build-verify, ff.
- **Visual fidelity loop:** `rb3-native` headless (`/api/screenshot`, ~3 s rebuilds)
  vs `images/retail-screenshots/`; confirm final fix once on web.
- **Pitfall:** never probe PropAnim by bare symbol at DTA root (`{x showing}`,
  `{y.trg trigger}` silently no-op) — use panel-relative paths. Caused two wrong
  flip-flop conclusions on the details pane.

## Docs

- `OFFSCREEN_RTT_INVESTIGATION.md` — current RTT state; supersedes stale
  `CHAR_OUTFIT_DIAGNOSIS.md` §2/§5.
- `RTT_HACK_UNWIND_ROADMAP.md` — feature specs + impl-status banner (all retired).
- `RTT_ENGINE_IMPL_PLAN.md` — build-ready drawrect/postproc-rtt/smear plan.

## Next

Roadmap for new client work (audio-web, input, difficulty select, loader stalls,
completeness) is being spec'd into `roadmap-2026-06-02/` → `NATIVE_PORT_NEXT_ROADMAP.md`.
