# scout-char-render — gameplay band chars: floating eyes/teeth, dropped bodies, shard fans

Wave-1 scout, 2026-06-11. Issue key `char-render`, ports 8611-8619.
**Status: root-cause area pinned (with one open sub-question); the "Wave-5 engine
regression" framing is REFUTED with A/B + known-good-rebuild evidence.**

---

## 1. SYMPTOM

Reproduced headless on the current master build (`native/build-native/rb3-native`,
built 06-11 18:27) via the canonical harness:

```bash
python3 scripts/native/keyboard-to-gameplay.py --port 8611 --diff hard \
    --out /tmp/rp-char-render/base --game-burst 24 --verbose
```

What the user sees, decomposed into FOUR distinct sub-symptoms (all reproduced):

| sub-symptom | evidence (full-res PNG kept) | mechanism (confirmed, §2) |
|---|---|---|
| "Only teeth/eyes render" — floating eye pairs / glasses / teeth with no head or body | `/tmp/rp-char-render/base/burst_07.png`, `burst_09.png`, `burst_12.png`; also `good0609/burst_14.png` | eyes/teeth are **rigid bone-parented meshes** (not skinned) → never touched by the V24 degenerate-skinned-mesh guard; the skinned head/body meshes around them ARE dropped by the guard |
| Bodies invisible / **flicker** | SHARD_GUARD drop tallies (below) | V24 ratio guard (engine `Rnd_Wgpu_RB3.cpp` ~L4130: drop when blendedExtent > 2.0× bindExtent && >15u) drops band `*_skin.N` / `*_resource` outfit meshes ~20-35×/frame, per-frame → flicker as animation crosses the 2.0 threshold |
| Pale/white **shard fans** (radiating spikes) | `/tmp/rp-char-render/base/burst_08.png`; `guardoff/burst_03.png`, `guardoff/burst_07.png`; `good0609/burst_01.png` | genuinely broken skin poses that are either **rebind-flagged (clamp-exempt)** or below the 2.0 drop ratio — drawn raw |
| "Lifted/wrong legs" in walking scenes | (not captured in a dedicated frame this scout) | most consistent with the engine **fling-clamp** freezing mismatched leg bones to bind (identity) while the root moves — limbs lag the walk. Walk-in is a venue-entrance clip (NOT the intro cinematic; triage gotcha checked) |

Contact sheets (montages of 12-24 burst frames each):
`/tmp/rp-char-render/{base,cacheoff,on-probe,off-probe,on-r2,off-r2,on-r3,off-r3,guardoff,norebind,good0609,probe2}-contact.png`
(non-key individual frames recompressed to `.jpg` in the same dirs — /tmp user quota
was hit mid-scout; key frames kept as `.png`).

Band members + outfits are RANDOM per run — single-run visual A/B is confounded.
Use the SHARD_GUARD drop tally + probe counters (below) as the robust metric.

---

## 2. ROOT CAUSE (ranked, with the A/B evidence)

### 2a. REFUTED: "new regression from engine Wave-5 (8fb669d unpack cache / WarmGpuForDir)"

This was the prime suspect in the issue brief. It is **exonerated on four
independent measurements**:

1. **`RB3_UNPACK_CACHE_OFF=1` A/B** (3 runs per arm: `base`/`on-probe`/`on-r2`/`on-r3`
   vs `cacheoff`/`off-probe`/`off-r2`/`off-r3`): both arms shard. Non-crowd
   SHARD_GUARD drops in the last 800 frames: cache ON ≈ 17.4k vs OFF ≈ 27.4k
   (OFF actually drops MORE; the first OFF run looking cleaner was outfit luck).
   Guard input is not stale either: per-mesh `bindExt` values are identical
   across arms (e.g. fingernails 35.93/50.66 in both).
2. **W5-T2 dwell warm driver is a true no-op by default** — without
   `RB3_GAMEWARM_HOLD=1` it returns before any warm/pre-kick
   (`native/src/rb3_gamewarm_native.cpp` L370). `WarmGpuForDir` has no other caller.
3. **Venue frustum cull is default-OFF** (`src/system/rndobj/Draw.cpp` L175,
   needs `RB3_VENUE_FRUSTUM_CULL=1`) — not the flicker source.
4. **Known-good rebuild reproduces the breakage** (the decisive one): rb3 @
   `2580e128` (the 06-09 verified char-fix commit) + engine @ `58901477` (its
   then-pin), rebuilt in the scout worktree and run with today's harness/assets
   → **same drop volume** (71,423 non-crowd drops / 2,365 frames ≈ 30/frame) and
   **same visuals** (shard fan `good0609/burst_01.png`, floating eyes
   `good0609/burst_14.png`). Engine log: `/tmp/rb3-kbd2game-8612.log`.
   Assets unchanged since 06-09 (`find orig-assets/extracted -newermt 2026-06-09` → empty).

Also checked and clean: `EndianSwapEq<int>` fix `a2ee7e4a` (all int-typed call
sites reaching native are host-guarded), drum-gear fix `727cf01e` (not in the
06-09 worktree, which still reproduces).

**Conclusion: this is NOT a new regression.** The 06-09 "verified coherent" state
does not reproduce as clean today with identical code+assets; the 06-09
verification was a small-n camera-burst that undersampled the failure — its own
artifact `/tmp/rb3-contact-HEADFIX.png` (still on disk, dated 06-09 09:28)
contains floating-eye closeup frames in row 3. The char-skinning fix shipped
06-09 is **incomplete/fragile under reload churn** (2c), and per-run random
outfits/venues make any single run look better or worse.

### 2b. CONFIRMED: what makes each pixel-level symptom

- `SHARD_GUARD_OFF=1` run (`/tmp/rp-char-render/guardoff/`, port 8619): with the
  drop-guard disabled the screen fills with naked shard fans
  (`guardoff/burst_03.png`, `burst_07.png`) → **the guard is correctly hiding
  genuinely broken skin poses**, not misfiring on good ones. Root cause is
  upstream of the guard. Bodies invisible = guard drops; flicker = per-frame
  ratio crossing.
- `RB3_NO_SKEL_REBIND=1 RB3_NO_HEAD_REBIND=1` run (`/tmp/rp-char-render/norebind/`):
  **no shard fans at all** (chars frozen-ish/clamped instead) → the drawn
  explosions come specifically from meshes with `mNativeBonesRebound=true`,
  which are **exempt from the engine fling-clamp**
  (`Rnd_Wgpu_RB3.cpp` ~L3890-3950: `reboundSkip` bypasses `sSkinClamp`).
- Eyes/teeth are rigid (bone-parented, `NumBones()==0`) → never guard-dropped,
  never clamped → always render → "only teeth/eyes".

### 2c. THE UPSTREAM DRIVER (new ground truth from this scout's probe build)

Probe build (worktree, rb3 master + 3 `RELOAD_PROBE` printf probes in
`src/system/bandobj/BandCharacter.cpp` — see §"worktree state"):
run `/tmp/rp-char-render/probe2/`, engine log `/tmp/rb3-kbd2game-8613.log`
(NOTE: the plain `8613.log` is this probe2 run; the earlier cache-ON probe run's
log on the same port was gzipped to `8613.log.gz`).

Timeline for `player0` (identical shape for all 4 members):

```
frame ~0-4    2× [STARTLOAD] + [SETDEFORM]            initial load, expected
frame ~13     [HEAD_REBIND] full rebind SUCCEEDS      176 bones / 15 meshes / restPose=108
frame ~13     [STARTLOAD]  ← RELOAD wipes restPose 108→0 right after the rebind
frame ~57,118,858,905   more [STARTLOAD]s             rest snapshot stays empty;
                                                      rebind scans find nothing
                                                      (reboundBones=0 forever),
                                                      give-up latch at quiet=120
frame ~4538   2× [STARTLOAD] DURING THE SONG
frame 4544-4560+  [SETDEFORM] EVERY 1-2 FRAMES        SetDeformation re-runs against a
                                                      MID-ANIMATION skeleton, repeatedly
```

Three load-bearing facts:

1. **`BandCharacter::StartLoad` re-fires 5-6× per member through the flow,
   including mid-song.** Each re-entry executes the HX_NATIVE re-arm block
   (`BandCharacter.cpp` L1520-1545): clears `mNativeRestPose` + un-latches the
   rebinds. The first re-entry lands immediately after the one successful
   first-Poll rebind, destroying the rest snapshot the offsets were baked from.
   After that, scans never rebind anything again (`reboundBones=0`,
   `restPose=0`, `slots=4-26`, `pending=1-5`) and latch via the 120-quiet-frame
   give-up — so every mesh streamed/re-merged after frame ~13 never gets a
   correct bind and lives on the clamp or the guard.
2. **`SetDeformation` re-runs every 1-2 frames during gameplay** (driven through
   `SyncObjects`, called from the `RemoveDrawAndPoll`/`OnSetFocus` paths and the
   merge-completion handler at `BandCharacter.cpp` ~L2773). On the Wii this is a
   load-time operation against a rest-pose skeleton; here it executes
   `PoseMeshes`/`CharMeshCacheMgr::SyncMesh+StuffMeshes`/`RndMeshDeform::Reskin`
   against a skeleton that is 4,500 frames into venue clips. That is exactly the
   class of operation that corrupts the bind/vert state the V24 guard then
   measures as degenerate.
3. **The reloads do NOT re-point bones** — `[REBIND_STALE] = 0` occurrences
   (probe checked every rebound mesh's `BoneTransAt(b)` vs a live `Find` each
   scan). So the already-rebound meshes keep valid bone POINTERS; it is the
   re-deform/re-merge VERT + pose state, and every never-rebound mesh, that break.

**Open sub-question for wave-2 (first action): what triggers the mid-song
`StartLoad`s?** Candidates, in suspicion order — all funnel into the same
re-arm block, so one probe line in each caller settles it:
- `RecomposePatches` (`BandCharacter.cpp` ~L2061): patch/texture compose →
  `unk224|=1; StartLoad(true, mInCloset, true)`. If native async texture
  compress completes late (`TextureCompressed` flow), it would fire mid-song.
- `SetInstrumentType` / venue gear assignment (drum gear NOW loads after
  `727cf01e`, a new mid-flow merge for drummers that 06-09 didn't have).
- the `{start_load}` DataNode handler (~L2125) fired by venue/shot scripts.
- `MiloReload` (~L2091).

---

## 3. FIX DESIGN (for the wave-2 implementer)

`needsEngineChange: NO` for the primary fix (all rb3-side, HX_NATIVE-gated,
match-neutral — the Wii arms are untouched). An engine change is only on the
table if wave-2 decides to re-tune the V24 guard / clamp thresholds afterwards
(they currently do their backstop job correctly).

**Step 1 — identify + stop the reload churn (likely THE user-visible fix).**
Instrument the four `StartLoad` callers above (one `fprintf` each, env-gated like
my probes) + `FileMerger::StartLoad`, run once, read which caller fires at
frame ~13 and mid-song. Then suppress that trigger during gameplay: e.g. if it
is `RecomposePatches`, defer recompose-triggered full reloads while
`!mInCloset && in-song` (the Wii never recomposes mid-song; composition should
complete during meta-loading). Same logic for a gear/`start_load` script trigger:
the merge should coalesce into the loading dwell, not re-fire on `game_screen`.

**Step 2 — make the rebind machinery reload-re-entrant.**
The current design (one-shot first-Poll rest snapshot, cleared by every
`StartLoad`, give-up latch after 120 quiet frames) cannot survive ANY reload:
- Do NOT blanket-`clear()` `mNativeRestPose` in `StartLoad` (L1534). The
  per-member skeleton bones PERSIST across these reloads (proved by
  `REBIND_STALE=0` + `slots` staying small) — their rest WorldXfms are still the
  correct bake basis. Keep the snapshot; only un-latch the quiet counters so new
  meshes get scanned.
- Re-capture rest **only** at a point where the skeleton is deterministically at
  rest: immediately after `SetDeformation()` inside `SyncObjects()` (the deform
  clip leaves the skeleton at the gender-bind rest pose there). CAVEAT from the
  06-09 investigation (CHAR_SKINNING_DEFORM_INVESTIGATION.md): at SyncObjects
  time `Find` may still resolve the shared MAGNET skeleton, so capture-at-
  SyncObjects must tolerate that (capture per-bone on first DISTINCT resolve,
  as today — but seeded from the post-deform pose, never from a mid-clip Poll).
- Drop the 120-frame give-up latch in favor of "latched when no UNREBOUND mesh
  remains" + re-arm on every SyncObjects; with Step 1 done, churn is bounded.

**Step 3 — verify the clamp/guard residuals shrink to the known baseline**
(1 `pending` own==bound mesh per member on the clamp; crowd props on the guard —
the `crowd` issue is a sibling scout's lane; note `male/female_crowd_body*` are
the top guard-dropped meshes, so the fixes may be related).

Files: `src/system/bandobj/BandCharacter.cpp` (Poll/rebind/StartLoad/SyncObjects,
all already HX_NATIVE-gated), possibly `src/system/utl/FileMerger.cpp` probe only.
Engine read-only references: `milo-native-engine/src/platform/Rnd_Wgpu_RB3.cpp`
L3879 (clamp), L4130 (V24 guard), L3275 (L1 unpack cache — leave as is).

Risk: medium. The rebind machinery has had two prior hardening rounds; changes
must be re-verified in BOTH gameplay and closet/preview (the StartLoad re-arm
exists for closet outfit swaps — keep that behavior: in-closet reloads DO need
the full reset).

## 4. VERIFICATION (exact commands + pass criteria)

```bash
cmake --build native/build-native --target rb3-native
for i in 1 2 3; do
  SHARD_DBG=1 HEAD_REBIND_PROBE=1 RELOAD_PROBE=1 \
  python3 scripts/native/keyboard-to-gameplay.py --port 861$i --diff hard \
      --out /tmp/rp-char-render/fix-r$i --game-burst 24 --verbose
done
# tallies (log path is printed by the harness):
grep "SHARD_GUARD. dropped" /tmp/rb3-kbd2game-861N.log | grep -vc crowd
grep "\[STARTLOAD\]\|\[SETDEFORM\]" /tmp/rb3-kbd2game-861N.log   # with probes kept
montage /tmp/rp-char-render/fix-rN/burst_*.png -tile 6x4 -geometry 320x180+2+2 contact.png
```

PASS = all of, on 3/3 runs (outfits are random per run — n≥3 is mandatory):
1. Zero `[STARTLOAD]`/`[SETDEFORM]` events after `game_screen` is reached
   (probe counters; today: 2 reloads + per-frame deform storms mid-song).
2. Non-crowd SHARD_GUARD drops ≈ 0/frame steady-state (today: 20-35/frame;
   crowd drops excluded — sibling issue).
3. No pale shard fans and no body-less floating-eyes frames in any burst
   (compare against `/tmp/rp-char-render/base-contact.png` = broken baseline,
   `/tmp/rb3-contact-FINAL.png` = best-known-good look).
4. `REBIND_DRAW_FLING=0` unchanged; closet + song-select preview chars still
   coherent (regression check for the StartLoad re-arm change:
   `scripts/native/song-select-capture.py`).
5. Walking venue-entrance scene: legs track the walk (visual spot-check of the
   first ~10 burst frames after game_screen).

## 5. REFERENCE SCREENSHOTS NEEDED

- Retail gameplay **band-member closeup** (venue camera cut to guitarist/vocalist
  head+hands mid-song) — the current `images/retail-screenshots/` set is
  HUD-centric (drums/guitar highway shots only).
- Retail **venue-entrance walk-in** (band walking to positions at song start) —
  to judge the "lifted legs" pose against ground truth.
- Both are nice-to-have (broken vs not-broken is unambiguous without them);
  Xenia capture per PLAN.md if convenient.

---

## Appendix: run inventory + worktree state

| run dir (`/tmp/rp-char-render/`) | build | env | port | takeaway |
|---|---|---|---|---|
| `base` | master | default | 8611 | symptom reproduced (key PNGs kept) |
| `cacheoff` | master | `RB3_UNPACK_CACHE_OFF=1` | 8612 | looked cleaner — outfit luck |
| `on-probe`/`off-probe` | master | +SHARD_DBG etc. | 8613/8614 | drop tallies ≈ equal both arms (logs gzipped: `/tmp/rb3-kbd2game-8613.log.gz`, `8614.log.gz`) |
| `on-r2`,`on-r3`,`off-r2`,`off-r3` | master | replication | 8615-8618 | both arms shard → cache exonerated |
| `guardoff` | master | `SHARD_GUARD_OFF=1` | 8619 | naked shards → guard correct, pose broken |
| `norebind` | master | `RB3_NO_*_REBIND=1` | 8611 | no shards → explosions are clamp-exempt rebound meshes (run truncated by /tmp quota; 15 frames valid) |
| `good0609` | rb3 `2580e128` + engine `58901477` | +SHARD_DBG | 8612 | **same breakage → not a regression** (log: `/tmp/rb3-kbd2game-8612.log`) |
| `probe2` | master + RELOAD_PROBE edits | probes | 8613 | reload/deform churn timeline (log: `/tmp/rb3-kbd2game-8613.log`) |

Worktree (left in place for wave-2): `.claude/worktrees/scout-char-render`
(detached at master `d92b2a98` + 3 uncommitted probe edits in
`src/system/bandobj/BandCharacter.cpp`: `[STARTLOAD]`, `[SETDEFORM]`,
`[REBIND_STALE]`, all gated on `RELOAD_PROBE=1`) with paired engine worktree
`/home/free/code/milohax/milo-native-engine-worktrees/scout-char-render`
(detached at `8fb669d`). Build gotchas hit: pass
`-DDawn_DIR=/home/free/code/milohax/dc3-decomp-deps/dawn/lib/cmake/Dawn` and
`-DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++` on the first configure
(default `cc` picks gcc → `-ferror-limit` errors). Teardown commands are printed
by `tools/setup-worktree.sh` (worktree remove in both repos + branch -D).

/tmp note: user tmpfs quota was at the limit during this scout; non-key burst
frames were recompressed to `.jpg` and two probe logs gzipped. Budget /tmp
writes in wave-2 (or point `--out` at a home-dir path).
