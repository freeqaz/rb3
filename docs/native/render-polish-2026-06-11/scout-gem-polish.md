# scout-gem-polish — highway gem rendering (tails / flicker / colors)

Wave-1 scout, 2026-06-11. Issue key `gem-polish`, ports 8631-8639.
Test song: **Antibodies** (Poni Hoax) — quickplay list position = 4× DDOWN from
top (`--song-downs 4`), expert guitar. First notes of the song are a Y/B/O
3-lane sustain chord @ **19680 ms** (1.44 s long); more sustain chords @ 69120
(0.84 s), 70080 (1.8 s), 76800, 77760 ms (parsed from
`orig-assets/extracted/songs/antibodies/antibodies.mid`). Capture driver:
`/tmp/rp-gem-polish/gem_probe.py` (reuses `scripts/native/keyboard-to-gameplay.py`
helpers; adds `msg:game:jump:<ms>` + rapid `/api/screenshot` loop — at headless
load the engine renders ~2.7 fps, so consecutive captures are ~1 engine frame
apart, i.e. true frame-by-frame sequences).

Verdict summary — the three sub-symptoms have **different root causes**:

| sub-symptom | status | root cause |
|---|---|---|
| (1) sustain tails only while held | **ROOT-CAUSED, A/B-proven** | engine per-mesh GPU cache misses the dirty signal for geom-owner-proxy meshes (`RndMesh::OnSync` keyed on the synced mesh; tails sync the never-drawn owner) |
| (3) colors washed out | **ROOT-CAUSED, A/B-proven** | `RB3_HIGHWAY_BLOOM` additive halo includes the source footprint → gem bodies get +white and saturate to white. NOT track lighting (A/B'd) |
| (2) flicker | **partially diagnosed** | no gem-disappearance found in ~170 consecutive frames; found 1-frame **venue blackout** frames (5% of one window) + tail pop-in at hold start (consequence of #1) + sporadic magenta venue flash. Re-triage after fixes #1/#3 |

---

## 1) SUSTAIN TAILS — only render while held

### SYMPTOM

Retail: a sustain's tail is visible (dim, lane-colored tube) from the moment the
gem scrolls in (`images/retail-screenshots/yt_qRagnZCIMzk_gameplay_guitar.png`).
Native: the tail is invisible while the note approaches; it pops in only once
the note is hit/held.

Repro (organic, no aids in the approach window):

```
python3 /tmp/rp-gem-polish/gem_probe.py --port 8631 --out OUT \
        --jump 15500 --shots 40 --label ea        # song: Antibodies, expert guitar
# shots with songMs 18600..19600 = the 19.68s sustain chord approaching
```

Evidence:
- `/tmp/rp-gem-polish/ea_c012_approach_NO_tails_baseline.png` — songMs 19236,
  chord gems at ~60% track height, 0.44 s before the now-bar. The 1.44 s tails
  should fill the track above the gems; the track above them is EMPTY. Default
  env (all caches on).
- `/tmp/rp-gem-polish/sus19_tracklight_06_game_screen.png` — same window from an
  independent run (RB3_TRACK_LIGHT_OFF=1): gems, no tails — i.e. NOT lighting.
- Hold state works: `/tmp/rp-gem-polish/bo_01.png` (held Y/B/O tails render,
  20051 ms) and `/tmp/rp-gem-polish/sustain-base/sus_015.png` (held G/R/B tails).

### ROOT CAUSE (A/B-proven)

**Engine `sMeshGpu` cache invalidation misses geometry-owner proxies.**

The decisive A/B — *identical* window, *identical* commands, only env differs:

| env | approach tails @ ~19.2s? | evidence |
|---|---|---|
| default | **NO** | `/tmp/rp-gem-polish/ea_c012_approach_NO_tails_baseline.png` |
| `RB3_NO_MESH_CACHE=1` | **YES — full retail-like tails** | `/tmp/rp-gem-polish/nm7_approach_TAILS_meshcache_off.png` |
| `RB3_UNPACK_CACHE_OFF=1` | NO (same as default) | `/tmp/rp-gem-polish/nu8_approach_no_tails_unpackcache_off.png` |

So it is the **per-mesh GPU vertex/index cache** (engine
`milo-native-engine/src/platform/Rnd_Wgpu_RB3.cpp`, `sMeshGpu` decl :446), not
the Wave-5 L1 vertex-unpack cache and not culling (native frustum cull is
default-off + world.cam-only — rb3 `src/system/rndobj/Draw.cpp:158-191`).

Mechanism (all refs current as of engine pin `8fb669d` / rb3 `38c5ca7e`):

1. A `Tail` (rb3 `src/band3/bandtrack/Tail.cpp`) renders via TWO proxy meshes
   `mTail1`/`mTail2` that `SetGeomOwner()` a pooled, never-drawn owner mesh
   (`Tail::ConfigureMeshes` :96; pool = `GemRepTemplate::GetTail/ReturnTail`).
   For chords, ALL lanes' proxies share the FIRST tail's owner (`Gem::AddRep`,
   `Gem.cpp:118` passes `i1`).
2. `Tail::UpdateVerts` (Tail.cpp:209) rewrites the OWNER's verts and calls
   `mTailGeomOwner->Sync(0x9F)`.
3. Engine `RndMesh::OnSync` (Rnd_Wgpu_RB3.cpp:4811) does
   `sMeshGpu.find(this)` — `this` is the OWNER, which has **no cache entry**
   (it is never drawn). The drawn proxies' entries (keyed by drawn-mesh pointer,
   `sMeshGpu[mesh]` at :3285) are **never marked dirty**.
4. The fingerprint fallback (:3286-3293 — ownerKey + vert count + face count +
   skinned flag, counts read from the owner) only catches **count** changes.
5. An **approaching** (inactive) tail uses tail-type 1 = `2` giant sections
   (`GemRepTemplate::SetupTailVerts`, GemRepTemplate.cpp:171-174:
   `mNumTailSections[1]=2`, section length ≈ `kTailMaxLength`). As the tail
   scrolls in, its visible length grows every frame, but
   `used_sections = capInc + ceil(midLen/hugeSection)` saturates at 2 almost
   immediately — **vert/face counts freeze while positions keep changing**.
   Result: the GPU buffer freezes at the first post-saturation upload = a tail
   ~1/90th of max length (cap section = `kTailMaxLength/90`) → invisible sliver.
6. A **held** tail uses tail-type 0 = 90 fine sections, so counts change every
   section boundary → fingerprint mismatches → re-uploads → it renders (which
   is exactly the reported "only while held").

Note an inconsistency kept honest: in two runs that had earlier done a large
FORWARD `jump` (64s/66s), the later 69s sustains DID show approach tails
(`/tmp/rp-gem-polish/nh_c008_69s_approach_tails_visible.png`). The root-cause
A/B above is unaffected (same window, cache-off flips the behavior), but the
implementer should use the EARLY-window protocol (19.68 s chord) for
verification, not the 69 s one.

Secondary effect of the same bug: while held, vert positions only refresh when a
section-count boundary crosses → the whammy X-wiggle
(`Tail::Poll` active branch) renders in steps / appears frozen between
boundaries. Same fix covers it.

### FIX DESIGN — **needs engine-repo change** (milo-native-engine)

File: `src/platform/Rnd_Wgpu_RB3.cpp`. Recommended: **owner geometry-generation
counter** (O(1), no map sweeps):

- Add `static std::unordered_map<RndMesh*, uint32_t> sGeomSyncGen;`
- `RndMesh::OnSync` (:4811): keep the existing `sMeshGpu[this].uploaded=false`
  AND `++sGeomSyncGen[this];`
- `RB3MeshEntry` (:377): add `uint32_t fpOwnerGen = 0;`
- `DrawMesh` needUpload (:3286): add
  `|| meshEntry.fpOwnerGen != LookupGen(owner)` (missing entry ⇒ 0); stamp
  `meshEntry.fpOwnerGen` where ownerKey/fpVerts/fpFaces are stamped inside the
  `needUpload` arm.
- `WarmGpuForDir`'s twin fingerprint stamping (:4662-4708) must stamp
  `fpOwnerGen` identically, or warmed meshes re-upload once (harmless but noisy).
- `CleanupGpuMesh` (:466): also `sGeomSyncGen.erase(mesh)`. Pointer-reuse after
  free is safe in practice: a recycled tail owner always gets `Sync()`d from
  `Tail::Init` → gen ≥1 ≠ stamped 0 → forced re-upload.
- The L1 unpack cache keys off the same `needUpload` (:3329), so it inherits the
  fix — no separate change.

Risk: low. The gen-lookup adds one hash lookup per draw for proxy meshes only if
gated `if (owner != mesh)` (self-owned meshes already work via `OnSync(this)` —
keep their path unchanged). No bind-group/layout changes, dc3 backend untouched,
Wii build untouched (engine-only). Match-neutral by construction.

Alternative (rejected): sweep `sMeshGpu` for `ownerKey == this` in OnSync —
O(N-meshes) per Sync; RndText fires Sync per glyph change, too hot.

---

## 3) COLORS — gems wash to white

### SYMPTOM / ROOT CAUSE (A/B-proven)

Baseline gems are white/washed blobs; retail gems are saturated colored capsules
(`/tmp/rp-gem-polish/retail_hw_wii2.png`).

A/B (same song/window, port-isolated runs):

| env | gem appearance | evidence |
|---|---|---|
| default | green/yellow caps washed nearly white | `/tmp/rp-gem-polish/zoom_base_14.png` |
| `RB3_TRACK_LIGHT_OFF=1` | WORSE — fully white blobs | `/tmp/rp-gem-polish/sus19_tracklight_06_game_screen.png` (and zoom in tracklight-off run) |
| `RB3_HIGHWAY_BLOOM_OFF=1` | **saturated green/red/yellow gems ≈ retail** | `/tmp/rp-gem-polish/zoom_bloff_14.png`, `bloom_off_hw_10.png` |

So the wash is the **highway bloom halo** (engine `CompositeHaloBloom`,
default-ON, thresh 0.55 / blend 0.7 — Rnd_Wgpu_RB3.cpp:2062-2069), not track
lighting. Cause: the "additive-halo-only" design (:1894-1936) replays each
halo-source draw (any game.cam mat with an emissive map, excluding `*surface*` —
`IsHaloSourceMat` :1930), blurs it, and additively blits the WHOLE blurred image
— which **includes the source's own footprint**. The gem emissive cap is
white-ish, so every gem body receives +0.7×white → saturates to white. Held
tails are also halo sources (emissive) → the giant white-cored ribbons in
`/tmp/rp-gem-polish/sustain-base/sus_015.png` / `bo_01.png`.

### FIX DESIGN — **needs engine-repo change**

`CompositeHaloBloom` should composite the **outer halo only**: subtract the
un-blurred halo-source image from the blurred result before the additive blit
(`halo = max(0, blur(src) - src) * blend`), e.g. bind both the bloom output and
`mHaloView` (the pre-blur replay target — already a sampleable texture) in
`kRB3HaloBlitShaderSource`'s `fs_blit` and emit the clamped difference. The gem
body then keeps its base-pass color; only the glow around it is added.
Cheaper-but-lossier fallback: drop `blend` default 0.7 → ~0.25-0.35 and raise
`thresh` 0.55 → ~0.7 (tunable live via `RB3_HIGHWAY_BLOOM_BLEND/THRESH` — good
for picking the target numbers before code changes). Risk: low; the env opt-outs
and the `blend<=0 == OFF` negative control already exist.

Secondary color note: with bloom off, per-lane gem hue/saturation visually
matches retail well (green/red/yellow/blue capsules with bright caps); no
separate gem-material color bug found. Track-lighting (`RB3_TRACK_LIGHT_OFF`)
should stay ON — without it gems are pure white (the dark-track ×0.12 + emissive
re-enable is what makes colors readable at all).

---

## 2) FLICKER — gems flicker in/out

### What was tested

~170 consecutive engine frames sampled across 5 runs (headless ≈2.7 fps ⇒ one
screenshot ≈ one engine frame): `sustain-base` (60 shots, 64-86 s),
`early-auto` (40, 15-32 s), `early-nomeshcache` (30), `early-nounpack` (30),
`sustain-nohit` (35, 66-81 s). Numeric scans: bright-pixel counts + mean
luminance of the track region per frame; venue-region luminance per frame;
magenta-pixel counts.

### Findings

1. **No gem-disappearance event found.** Gem fields evolve consistently with
   the chart frame-over-frame (e.g. `/tmp/rp-gem-polish/seq22_consecutive_a.png`
   vs `seq23_consecutive_b.png`). The "gems flicker" report did NOT reproduce
   under headless captures at ~2.7 fps.
2. **Venue blackout frames** — in `sustain-base`, the venue (or its right half)
   renders BLACK for single frames while track+gems stay correct: shots 10, 11,
   14 of 60 (songMs 67574/67944/68872) — right-side venue luminance drops to
   0.0, recovers the next frame. Full-frame example:
   `/tmp/rp-gem-polish/sb_011.png` (venue gone, highway fine; compare
   `sb_009.png`). At 60 fps this class of 1-frame dropouts reads as heavy
   flicker *around* the highway and may be what the user perceives as gems
   flickering (gems sit on a suddenly-black backdrop). Likely owned by the
   venue/char rendering scout-lane; cross-reference `char-render` (legs
   flicker) — plausibly the same 1-frame dropout class.
3. **Tail pop-in/pop-out** at hold start/end + stale approach tails are a
   direct consequence of root cause #1 and will read as flicker; fixed by the
   mesh-cache fix.
4. **Sporadic magenta flashes**: (a) whole-venue magenta wash for ~2 frames
   (`/tmp/rp-gem-polish/nh24_venue_magenta_flash.png`, songMs ~75.8-76.7 s in
   `sustain-nohit`) — could be a legit venue strobe cue, could be venue-light
   glitch; (b) two star-shaped magenta splats mid-track for 1 frame
   (`/tmp/rp-gem-polish/bo_02.png`, 21263 ms, right after a sustain ended) —
   looks like an untextured release-FX sprite. Low priority, noted for the FX
   lane (cf. A1 hit-flame memory: `DrawParticlesBillboard` is a no-op stub on
   RB3's backend).

### Recommendation

Land fixes #1 and #3 first, then re-triage flicker interactively (real-time
windowed run, not headless) — if it persists, instrument the venue-blackout
class (it is measurable: venue-region luminance per frame, scan script inline in
this doc's history; see VERIFICATION). Frustum culling and the unpack cache are
EXONERATED for default configs (cull default-off; unpack-cache A/B showed no
behavioral change for gems/tails beyond baseline).

---

## VERIFICATION (for the implementation agent)

Tails (the must-pass):
```
# after the engine fix, default env — capture the early window:
python3 /tmp/rp-gem-polish/gem_probe.py --port 863X --out /tmp/verify-tails \
        --jump 15500 --shots 40 --label v
# PASS = the shot at songMs ≈ 19.2-19.5 s shows three colored tails extending
# from the Y/B/O chord gems to the top of the track, matching
# /tmp/rp-gem-polish/nm7_approach_TAILS_meshcache_off.png (the cache-off truth),
# WITHOUT RB3_NO_MESH_CACHE set.
# Also re-run with RB3_NO_MESH_CACHE=1 and diff — should now be visually identical.
# Regression guard: text rendering (RndText Sync path), song-select scroll,
# crowd/chars (skinned cache shares needUpload) — run
# scripts/native/song-select-capture.py + a full keyboard-to-gameplay.py pass.
```

Colors:
```
# A/B base vs RB3_HIGHWAY_BLOOM_OFF=1 at the same frame; sample gem-cap pixels.
# PASS = with bloom ON (fixed), a green gem's cap keeps G-dominance
# (min(R,B) < ~0.75*G), i.e. close to the bloom-off frame
# /tmp/rp-gem-polish/zoom_bloff_14.png, while still showing an outer glow.
# The held-tail frame (cf sustain-base/sus_015.png) must no longer be a white bar.
```

Flicker (re-triage):
```
# 40+ rapid shots in any gameplay window; per-frame venue-region luminance scan
# (crop boxes (60,300,300,560) + (1000,300,1240,560) at 1280x720; BLACK = both < 6).
# PASS for the gem lane = no frame where a chart-expected gem region is empty
# between two populated frames; venue blackouts tracked separately.
```

Worktree note: `tools/setup-worktree.sh scout-gem-polish` exists at
`.claude/worktrees/scout-gem-polish` with an UNCOMMITTED, unbuilt `TAIL_DBG`
logging probe in `src/band3/bandtrack/Tail.cpp` (Init/Poll state, owner
vert/face counts, showing flags). It was not needed (env A/B was decisive) but
may help verification; safe to discard.

## REFERENCE SCREENSHOTS NEEDED

1. Retail (Wii preferred) frame of an **approaching, un-hit sustain** on the
   5-lane guitar highway — to calibrate the dim-tail alpha/width
   (`kTailMinAlpha` look). Current refs only show tails incidentally/at distance.
2. Retail **held sustain with active whammy** close-up (pulsing tail) — target
   for post-fix tail look + bloom amount on tails.
3. Retail **missed sustain** (gray `tail_miss` tube) — we have no native truth
   for the miss state at all.
4. Retail **overdrive-deployed highway** (track turns blue) — to disambiguate
   the bright-blue track state seen in some captures from a lighting bug.

## Evidence index

All under `/tmp/rp-gem-polish/` (full-res frames + run logs; large sequences in
`/home/free/tmp/rp-gem-polish/` — early-auto/, early-nomeshcache/,
early-nounpack/, sustain-nohit/ with `*_meta.json` songMs-per-shot). Engine refs
at `milo-native-engine` @ pin `8fb669d`. No main-repo or engine-repo source was
modified by this scout.
