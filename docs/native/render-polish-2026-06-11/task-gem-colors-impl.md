# task-gem-colors — outer-halo-only highway bloom (gem color wash fix)

Wave-2 implementer for `gem-polish` sub-symptom **(3) colors washed out**. Engine
fix only; rb3 just bumps the pin. Followed `scout-gem-polish.md` §3's fix design
(option A: subtract the source footprint, keep the halo) verbatim.

## SYMPTOM (confirmed before fixing)

Highway gems / sustain tails wash to white under the default-on highway bloom.
`RB3_HIGHWAY_BLOOM_OFF=1` restores retail-saturated gems (A/B proven by the scout,
re-confirmed here). The wash is worst on **held tails / strike-line emissive gems**.

## ROOT CAUSE (from scout, re-verified)

`CompositeHaloBloom` (engine `src/platform/Rnd_Wgpu_RB3.cpp`) replayed each
halo-source draw into `mHaloView`, blurred it (its own `BloomPass` → `haloView`),
and **additively blit the WHOLE blurred image** — which still contains the source's
own bright footprint. So every emissive gem body received `+blend × its own bloom`
(≈ white) and saturated to white. The intended feature (the outer glow) was fine;
the body wash was the bug.

## WHAT CHANGED (engine repo `milo-native-engine`, commit `eecacbd`)

`src/platform/Rnd_Wgpu_RB3.cpp` + `src/platform/Rnd_Wgpu_RB3.h` — the halo blit now
composites the **outer halo only**:

- `kRB3HaloBlitShaderSource` `fs_blit` now binds **two** textures + a uniform:
  `srcTex@0` = bloomed halo (`haloView`), `rawTex@2` = the un-blurred source
  footprint (`mHaloView`), `blendUB@3` = the blend factor. It emits
  `halo = max(bloom - raw, 0) * blend`. Inside the gem body (`raw` bright) the
  contribution cancels to ~0 so the gem keeps its saturated base-pass color; in the
  outer ring (`raw == 0`, `bloom > 0`) the glow survives and is added. The blurred
  halo is half-res and the raw is full-res, but both are sampled by the same UV so
  the sampler handles the size difference.
- `EnsureHaloBlitPipeline` BGL grows from 2 → 4 entries (adds `rawTex@2` texture +
  `blendUB@3` uniform); a 16-byte `mHaloBlendBuf` uniform buffer is created.
- The composite pass writes `blend` into `mHaloBlendBuf` per-frame (so
  `RB3_HIGHWAY_BLOOM_BLEND` stays live-tunable) and binds all 4 entries, with
  `mHaloView` as `rawTex`.
- `BandRnd::Shutdown` drops `mHaloBlendBuf` with the other halo blit refs.

`blend` moved out of `BloomPass.Run` (which **ignores** the `intensity` arg for the
mip-0 threshold/blur output — confirmed by reading `BloomPass.cpp`; that is why the
prior code short-circuits on `blend<=0` instead of trusting Run) and into the blit
shader. The `RB3_HIGHWAY_BLOOM_OFF=1` and `blend<=0` paths remain byte-identical
no-ops (the early-return is untouched). Diff is tight: shader + `EnsureHaloBlitPipeline`
+ the composite pass + one Shutdown line; no touch to mesh-cache/`OnSync`/`DrawMesh`/
`WarmGpuForDir`.

`native/CMakeLists.txt` — `MILO_ENGINE_PIN` bumped
`8fb669d…` → `eecacbdc2d58523704301ae0ca1c65c73fb21cc6`.

## BRANCHES + COMMITS

- engine branch `wt-task-gem-colors` @ `eecacbdc2d58523704301ae0ca1c65c73fb21cc6`
  (`fix(rnd-rb3): outer-halo-only highway bloom composite`)
- rb3 branch `wt-task-gem-colors` @ `c708886b`
  (`build(native): bump MILO_ENGINE_PIN to eecacbd …`)

Landing order (one-way dep): land the **engine** commit first, then the rb3 pin bump.

## VERIFICATION — PASS (verified=true)

Method: single-frame A/B between the **OLD** (unmodified, engine `8fb669d`) and
**NEW** (fixed) rb3-native binaries built side-by-side in the same worktree
(`build-OLD` vs `build-native`), then matched frames by `songMs` (≤8 ms apart) so
the chart state is identical and only the bloom composite differs. Song: Antibodies,
expert guitar, autohit, early 19.68 s sustain chord window. (The whole-frame venue
desyncs across runs — the known bloom-A/B gotcha — so I compare the **highway-only**
crop, which aligns to sub-pixel at matched `songMs`.)

Decisive measurements (top-N brightest highway pixels, RGB / saturation / min-channel):

| frame | OLD (washed) | NEW (fixed) |
|---|---|---|
| held tail @ ~21.6 s (top 3%) | **255,255,255  sat 0.00  min 255** (pure white) | **170,168,143  sat 0.16  min 143** (no wash) |
| approaching bars @ 18.8 s (top 2%) | 254,255,236  sat 0.07  min 236 | 219,224,200  sat 0.11  min 200 |

Visual A/B (evidence files below): the held-tail strike-line gems are a solid
white bar in OLD and crisp green/red/yellow/blue/orange in NEW; the approaching
yellow/blue/orange sustain bars are blown-white in OLD and saturated with a soft
glow still visible in NEW. So: saturated bodies ≈ retail / bloom-off, **with bloom
still ON** (`[RB3_HALOBLOOM] … blend=0.70 halo=1` confirmed firing in the NEW run).

No regression: main-hub and song-select (+ scroll) render correctly with the NEW
binary (composite is a no-op outside game.cam frames with halo draws); gameplay
HUD/venue render normally in the full after frames. Wii build untouched (engine-only
change; no `src/band3/` or `src/system/` edits — `git status` in the rb3 worktree
shows only the pin bump).

### Evidence (canonical small set in `/tmp/rp2-gem-colors/evidence/`)

- `HELD_OLD.png` / `HELD_NEW.png` — strongest case: held-tail white-bar wash → saturated. **The money shot.**
- `held_BEFORE_full.png` / `held_AFTER_full.png` — full frames of the above.
- `ab_OLD2.png` / `ab_NEW2.png` — approaching sustain bars, matched frame (washed → saturated + glow).
- `before_OLD_aligned.png` / `after_NEW_aligned.png` — full matched A/B frames (18.8 s).
- `zoom_default.png` (washed) / `zoom_bloomoff.png` (saturated ref) / `zoom_after.png` (fixed) — original 3-way.
- `hub.png` / `songsel.png` — menu/song-select regression check (NEW binary, OK).

Larger working set (dense sequences, metas, logs) under `/home/free/tmp/rp2-gem-colors/`
(tmpfs `/tmp` hit its per-user quota mid-run from concurrent agents; captures were
redirected to disk-backed `/home`).

## LANDING NOTES (for the orchestrator)

- **Sibling conflict: `wt-task-gem-tails`** (sub-symptom #1, mesh-cache) edits the
  SAME files `src/platform/Rnd_Wgpu_RB3.{cpp,h}` but in **disjoint regions** —
  tails touches `RB3MeshEntry` (~:377), `CleanupGpuMesh` (~:466), `DrawMesh`
  needUpload (~:3286), `WarmGpuForDir` (~:4662), `RndMesh::OnSync` (~:4811) and adds
  `sGeomSyncGen`/`fpOwnerGen`. THIS task touches only the bloom region
  (`Shutdown` ~:996, `EnsureHaloTarget`/shader/`EnsureHaloBlitPipeline` ~:1957-2024,
  `CompositeHaloBloom` blit ~:2172-2195) and adds `mHaloBlendBuf`. Cherry-picks
  should apply cleanly; if git flags `.h` overlap, it is only adjacent member
  insertions — resolve by keeping BOTH new members.
- Both tasks bump `MILO_ENGINE_PIN` in `native/CMakeLists.txt`. When landing both,
  the LAST engine commit's SHA wins the pin — bump it once to the final landed
  engine HEAD, not to either intermediate per-task SHA.
- No CMake/structural changes beyond the pin; no new build deps.
- `RB3_HIGHWAY_BLOOM_OFF=1` and `RB3_HIGHWAY_BLOOM_BLEND/THRESH` still work as before.

## TEARDOWN (when landed)

```
git -C /home/free/code/milohax/rb3 worktree remove --force \
    /home/free/code/milohax/rb3/.claude/worktrees/task-gem-colors
git -C /home/free/code/milohax/rb3 branch -D wt-task-gem-colors
git -C /home/free/code/milohax/milo-native-engine worktree remove --force \
    /home/free/code/milohax/milo-native-engine-worktrees/task-gem-colors
git -C /home/free/code/milohax/milo-native-engine branch -D wt-task-gem-colors
```
