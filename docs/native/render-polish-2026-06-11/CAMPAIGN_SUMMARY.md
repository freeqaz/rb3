# Render-Polish Campaign — CLOSE-OUT SUMMARY (2026-06-11 → 2026-06-16)

A 7-wave ultracode campaign that closed every user-reported rendering/gameplay gap in the RB3
native/web port, fixed every regression and blocking crash it surfaced along the way, and unlocked
the full 83-song library — with the Wii decomp build kept **byte-identical or improved throughout**.

Full per-wave detail is in [`PLAN.md`](PLAN.md); per-issue root cause + fix in `task-*-impl.md`;
independent verification in `verify-*.md`. This file is the one-page wrap-up.

## Result: every original issue fixed + independently verified

| # | User-reported issue | Outcome |
|---|---|---|
| 1 | Song-select difficulty grid misaligned | FIXED (icon glyph re-centered) |
| 2 | Characters: only teeth/eyes, legs flicker/lifted | FIXED (3-wave arc: reload-re-entrancy → C8 character-space rest-bake → stale leaf-bone WorldXfm recompose). Band stands fully dressed. |
| 3 | Crowd merged into one spot, not animating | FIXED (crowd skeleton inverse-bind rebake, −89.5% drops) |
| 4 | Gems: tails only-while-held, flicker, colors | FIXED (mesh-cache owner-gen invalidation + outer-halo-only bloom) |
| 5 | Highway offset, not head-on | FIXED (camRotX −4→0) |
| 6 | "All Instruments"/vocals crash | FIXED (TheNet.mSession wiring + empty-vector guards) |
| 7 | Held frets don't show as held | FIXED (smasher glow: per-slot color, bloom-source exclusion, white-sphere regression fixed) |
| 8 | Main-menu lighting off vs retail | FIXED (mUseEnviron unlit + emissive-all-cams + ambient-floor contrast + street-fog density) |

## Plus everything the waves surfaced (all fixed + verified unless noted)

- Regressions caught & fixed: fret white-sphere, menu fog wash, venue lighting blowout, gameplay-entry
  postproc flash.
- Blocking crashes fixed: All-Instruments/vocals (SIGSEGV+SIGABRT), endgame score-screen abort,
  score-screen over-press (synth iterator off-by-one), `/api/dta/eval` handler crash + burst stack-overflow,
  debug-verb over-press (`ChangeDifficulty` OOB + `EndOverrideFlow` assert), and the no-chart track-load
  SIGABRT.
- **Playability unlock:** the native port went from **3 playable songs to the full 83** — the charts were
  never missing, just in `extracted-xbox-full` rather than the loader's 3-song dev slice (a wiring gap).
  Wiring them in also exposed & fixed a real latent `SongParser` pointer-underflow SIGSEGV that only real
  charts triggered. Native scoring confirmed working end-to-end (156,973 pts / 5★).
- "Not-a-bug" adjudications (correctly NOT changed): endgame crowd green tint = authored disco color-wheel;
  scoring "0" = a `{game jump}` harness artifact.

## Match-discipline outcome

Every fix is `#ifdef HX_NATIVE` / `native/src` OR a proven match-IMPROVING decomp fix. Net Wii effect:
several functions reached 100% (`SerialGroupSeqInst::Poll`, `PassiveMessageQueue::Poll`,
`RndParticleSys::InitParticle` 95.3→97.7%), and overall is unchanged-or-up (81.86505% fuzzy). No fix
regressed the Wii build.

## Engineering method (what worked)

- **scout → implement (worktrees) → orchestrator lands → independent review → next wave**, docs as the
  handoff artifact between agents.
- Engine fixes committed to engine `wt-` branches; orchestrator cherry-picked sequentially with ONE
  `MILO_ENGINE_PIN` bump per wave (disjoint-region diffs + explicit landing notes → zero merge conflicts
  across ~15 engine cherry-picks).
- **Adversarial re-diagnosis paid off repeatedly:** the character bug's true cause changed every wave
  (IK → pose-pipeline → WorldXfm cache); the venue flash was re-diagnosed twice; the fret glow, the
  score-overpress crash, and the "asset gap" all had their first hypothesis overturned by evidence.
  Independent reviewers (separate from implementers, pre-fix A/B binaries) caught under-stated residuals
  (the dta-eval "stall" was actually a hard SIGSEGV) and confirmed match-neutrality independently.
- Transient API rate-limits killed two dispatches mid-run; re-dispatching fresh recovered cleanly (no
  work lost — agents commit to branches + write docs as they go).

## Remaining (none block gameplay)

- **Reproduce the song-chart symlinks on a fresh checkout** (machine-local; `orig-assets/*` is gitignored).
  One-liner (from repo root):
  ```bash
  cd orig-assets && for d in extracted/songs/*/; do s=$(basename "$d"); [ "$s" = gen ] && continue; \
    t="extracted/songs/$s/$s.mid"; src="$PWD/extracted-xbox-full/songs/$s/$s.mid"; \
    [ ! -e "$t" ] && [ -e "$src" ] && ln -s "$src" "$t"; done
  ```
- Deferred FEATURE (now has a design): crowd 2D bowl-imposter pipeline (Fix B) — a render-to-texture
  path, scoped plan-only in wave 8 (`task-crowd-venues-impl.md`); future wave.
- KNOWN native gap (new, tracked): `festival_01` venue override SIGSEGVs during load (festival-specific;
  `arena_06` loads clean). Surfaced once Fix C let non-small_club venues load.
- Minor taste remaining: endgame backdrop green-peak (authored disco wheel; could soften further).

## Wave 8 — wrap-up (2026-06-19): 4 items, all landed + reviewed (0 reject)

Per-item Plan→Implement→Review pipeline + fan-in (`WAVE8_FANIN.md`); orchestrator landed serially.
rb3 master `7f17a077`/`402c8561` + pin bump `79bea7fa` → engine `1010f5f` (`d9f8243` venue exposure +
`1010f5f` band-aware V24 guard). Composed build verified (menu/song-select/gameplay venue all correct —
no blowout/crush/pink-flood; lighting changes compose with the wave-4/5 backstops at identity-when-off).
Wii byte-identical (overall 81.86505; BandDirector edits HX_NATIVE, EnterVenue 100%).

- **web-songload — CONFIRM**: the full 83-song set loads + plays in the BROWSER too (3-4 previously-dead
  songs verified in-browser via the new `scripts/web/_w8-songload-verify.mjs` Playwright harness);
  `server.py` follows the `.mid` symlinks + lazy-fetch correctly, no manifest change needed. DEPLOY
  ACTION: run `scripts/web/build.sh` on the deploy host — the live wasm is gitignored/stale and predates
  the SongParser fix.
- **lighting-polish — CONFIRM_W_RESIDUALS**: env-tunable venue lit-path exposure scale
  (`RB3_VENUE_POINT_EXPOSURE`/`RB3_VENUE_DIR_EXPOSURE`, in `WriteSceneUniforms`) tames the song-start
  over-bright reveal + lifts menu contrast; identity at exposure=1.0; gameplay/song-select/score
  unregressed. Residual: menu bright-side still short of retail ~10:1 (further taste tuning).
- **pose-footwear-shard — CONFIRM**: band-aware V24 shard guard (per-band 4.0× ratio cap + 110u world
  cap + 40u world floor; opt-outs `RB3_BAND_SHARD_*`) — deep-curl footwear/gloves (lowtopsneaks,
  kidgloves, eightholedocs) now render while genuinely-torn / cross-instance meshes STILL drop (negative
  control held). The character work is fully closed.
- **crowd-venues — Fix C landed**: `BandDirector::EnterVenue` now honors the `MetaPerformer` venue
  override (was pinned to small_club_01), so non-small_club venues load (arena verified). Fix B (2D
  bowl-imposter crowd) scoped plan-only — a render-to-texture feature for a future wave.

## Status: COMPLETE — campaign closed 2026-06-16; wave-8 wrap-up landed 2026-06-19.
All user-reported issues + every surfaced regression/crash fixed & reviewed; full 83-song library plays
native AND web; Wii match byte-identical-or-improved throughout. Open = the 2D-imposter feature (designed),
the festival_01 venue gap, and pure taste tuning — none block gameplay.
