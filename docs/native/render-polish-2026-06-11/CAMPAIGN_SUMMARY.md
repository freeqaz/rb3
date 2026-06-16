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
- Web song-load via `server.py` symlink-following — confirm once in-browser (native verified).
- Minor polish/taste: menu hub final contrast (6.8 → ~10:1), venue song-start exposure, pose-fling
  footwear-on-deep-curl shard residual, endgame backdrop tint.
- Deferred features: crowd 2D imposters (Fix B) + venue bridge (Fix C, native pins small_club_01).
- Harness nit: `play_one.py` `--bin` defaults to a stale path — pass `--bin` explicitly.

## Status: COMPLETE — campaign closed 2026-06-16.
