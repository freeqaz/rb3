# Web Perf Handoff Pack

_Written 2026-06-09 after a deep audit of every WEB_PERF_ROADMAP.md item. Each numbered
file is a self-contained implementation handoff for a subagent: context, current code
with file:line, exact change spec, risks, and acceptance gate. Audited facts below
supersede the roadmap where they differ._

_**Verification pass (same day):** every doc in this pack was adversarially re-verified
by Opus subagents against the actual code (every file:line quote-checked, key semantics
like `CompareSphereToWorld` return convention and Loader API signatures confirmed from
source) and corrected in place. Verifier-found corrections are marked inside each doc._

## Status changes discovered during audit + verification (vs WEB_PERF_ROADMAP.md)

| Roadmap item | Finding |
|---|---|
| P2.2 mesh-cache v2 | **DONE — implemented, strict-gate VALIDATED, and PROMOTED** (engine `b5309b3` = HEAD of branch `main`; rb3 `91468cd5` already bumped `MILO_ENGINE_PIN`). [04](04-meshcache-v2-validation.md) is now confidence checks only (web crashrate sweep, gameplay multi-draw capture, perf numbers) |
| P4.2 link trap | **Resolved** — `rndobj_synth_link_stubs.s` is clean; engine has a strong `CleanupGpuMesh` (Rnd_Wgpu_RB3.cpp:440-442). Only a stale stub comment remains (cleaned up as part of [06](06-texgpu-leak.md)) |
| P2.3 cull blocker | **Cleared** — char-skinning landed (engine `12455b0` / rb3 `acd9c19a`, 2026-06-06); `Rnd_Wgpu_RB3.cpp` clean on engine `main` |
| P2.4 prewarm | **Spec flaw found by verification**: wave-04's A2 mechanism doesn't work as written — `UIPanel::Load` always `new`s its own DirLoader and never calls `DirLoader::Find`, so seeding TheLoadMgr alone does NOT make the ENTER parse free. [07](07-songselect-prewarm.md) now includes the required adoption hook in `UIPanel::Load` (also a matched TU, HX_NATIVE-gated) |
| P0.3 telemetry | **Far ahead of the roadmap**: worktree `wt-session-telemetry` is 9 commits ahead with M0 (recorder + SQLite ingest), M1 (Tier-1 replay) and an M4 prototype committed. [08](08-telemetry-m0.md) is now a review-and-merge-to-master contract, not an implementation |
| P1.2 -O>0 crash | **Shared, not RB3-specific**: DC3 also pins `-O0` citing the same matched-fork exception-edge brittleness — the bisect should expect the faulting TU in shared/matched-fork code; a fix likely helps both ports |
| P1.1 root cause | **Refined**: the budgeted loader yields are ALREADY throttled (16 ms) and were measured boot-neutral; the ~7 s idle is JSPI suspend-per-read, with `PollUntilLoaded`'s un-throttled per-slice yield the one remaining loader-side target. See the corrected Step-0 measurement plan in [02](02-boot-sync-read.md) |
| P0.2 quiet-box re-measure | **Cannot run now** — box load avg 12–24 (Ghidra VT headless job, next-server, two other Claude sessions). Run when quiet; [04]'s perf-numbers step gives a strong early signal on the render→audio-jitter hypothesis |

## Handoffs

| # | Item | Roadmap | Repo(s) touched | Effort (est) |
|---|---|---|---|---|
| [01](01-audio-lowwater-adaptive.md) | Ring low-water telemetry + adaptive-buffer integration | P0.1+P3.1 | engine (`AudioDevice_Web.cpp`, `audio-worklet.js`), rb3 (`audio-jitter-profile.mjs`) | S |
| [02](02-boot-sync-read.md) | Boot ~7 s App-ctor idle: measure-first, then sync fast path | P1.1 | rb3 (`native/src/native_file.cpp`), possibly rb3 (`Loader.cpp` `PollUntilLoaded` HX_WEB arm) | M |
| [03](03-release-opt-build.md) | Un-break `-O>0` release build (+ closure, separate) | P1.2 | rb3 (`native/CMakeLists.txt`, `scripts/web/build.sh`, `native/web/rb3_pre.js`), UB hunt in shared matched-fork code | M–L |
| [04](04-meshcache-v2-validation.md) | Mesh-cache v2 confidence checks (validation + pin ALREADY DONE) | P2.2 | none (measurement only; web build once) | S |
| [05](05-frustum-cull.md) | Venue frustum cull (tight spheres + world.cam-scoped cull) | P2.3 | engine (`Rnd_Wgpu_RB3.cpp`), rb3 (`src/system/rndobj/Draw.cpp` HX_NATIVE block) | M |
| [06](06-texgpu-leak.md) | `sTexGpu` GPU-texture leak fix | P4.1 | engine (`Rnd_Wgpu_RB3.cpp`), rb3 (`Tex.cpp` dtor hook, link stubs) | S |
| [07](07-songselect-prewarm.md) | song_select prewarm during main_hub idle (incl. UIPanel::Load adoption hook) | P2.4 | rb3 (`src/system/ui/UIScreen.cpp` + `UIPanel.cpp`, HX_NATIVE only) | M |
| [08](08-telemetry-m0.md) | Session telemetry: review + merge `wt-session-telemetry` (M0/M1 already built there) | P0.3 | rb3 (merge from worktree; resolve App.cpp conflict) | M (own track) |

## Sequencing & conflict matrix

- **`Rnd_Wgpu_RB3.cpp` is shared by 04, 05, 06.** Run 04 (validation, read-only on the
  file) FIRST; then 06 (small, additive) and 05 (larger) — serialized, or as one agent.
  Engine commits land in `../milo-native-engine` first, then one rb3 `MILO_ENGINE_PIN`
  bump can carry 04+05+06 together.
- **01, 02, 03, 07 are mutually independent** and don't overlap the renderer file.
- **08 (telemetry M0)** has its own worktree (`.claude/worktrees/session-telemetry`) and
  established working mode; its App.cpp frame-tap edit conflicts with 25 uncommitted
  lines in `src/App.cpp` (`RB3RenderHeldFrame` test-harness helper, a concurrent
  agent's) — coordinate before landing.
- **Concurrent-agent warning:** engine commits `a0f98ad`/`b5309b3`/`936bd8f4` all landed
  TODAY and `docs/native/audio-perf-loop/STATE.md` has uncommitted edits — at least one
  other session is active in this exact area. Before any agent edits engine renderer or
  audio files, it must check `git status` in both repos and skip/halt on dirty overlap.
- Engine is **18 commits ahead of origin** — don't push; pin by SHA as usual.

## Standing rules for implementation agents (put in every prompt)

1. Engine fixes commit in `../milo-native-engine` first; bump `MILO_ENGINE_PIN` in
   `native/CMakeLists.txt` in a matching rb3 commit.
2. Stage only files you changed; never `git add -A`; no stash/revert/checkout in the
   main repo (concurrent agents).
3. Decomp-matched TUs (`src/system/...`, `src/App.cpp`): all changes `#ifdef HX_NATIVE`,
   no struct member additions, verify Wii match% unchanged after build.
4. Native build: `cmake --build native/build-native` (~3 s); verify headless via the
   HTTP debug API before any web build (web build = minutes, do it once at the end).
5. Every handoff has an acceptance gate — run it, paste numbers in the commit/doc.
