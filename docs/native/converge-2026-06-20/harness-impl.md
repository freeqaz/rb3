# Harness-Impl — Deterministic "Force Band Closeup" Capture (IMPLEMENTED)

Convergence batch 2026-06-20. Implements [`scout-harness.md`](scout-harness.md)
§1 (native hook) + §2 (Python harness) + §3 (A/B protocol) + §5 (verdict metric).
**Code written, built, smoke-tested, and determinism-validated on-device.**

- Worktree: `/home/free/code/milohax/rb3/.claude/worktrees/converge-harness`
- Branch: `wt-converge-harness`
- Changed files (only):
  - `native/src/rb3_http_handlers.cpp` — 3 new DTA accessors + 3 registration lines
  - `scripts/native/band-closeup-capture.py` — the harness (new)
  - `docs/native/converge-2026-06-20/harness-impl.md` — this doc
- No engine edit, no `MILO_ENGINE_PIN` change (native-only glue, Wii-neutral).

---

## 1. The native hook (built, working)

Three native-only DTA funcs added to `native/src/rb3_http_handlers.cpp`, registered
in `RB3HttpRegisterDtaFuncs` next to the existing four:

- `{rb3_force_shot "<name>"}` → sets `mDisabled=true` FIRST, then
  `ForceShot(wdir->Find<BandCamShot>(name,false))`. Returns a **status STRING**:
  `force_shot ok` / `force_shot not_found:<name>` (backed by a `static std::string`
  per the MakeString-lifetime gotcha) / `no_director` / `no_venue`.
- `{rb3_director_disable <0|1>}` → set/echo `mDisabled` (returns the int state).
- `{rb3_cur_shot}` → live `mCurShot->Name()` (the determinism proof).

All three reuse the existing `TheBandDirector->mVenue.Dir()` plumbing
(`rb3_pos_dump` already uses it). `mDisabled` (0xb5), `mCurShot`
(`ObjPtr<BandCamShot>` 0xb8 — converts to a raw ptr via `operator T1*`), and
`ForceShot(BandCamShot*)` are all `public:` on the RB3 `BandDirector`, so no
friend decl / header change. Build: full native build is green
(`cmake --build native/build-native --target rb3-native -j16`; binary
115,964,048 bytes).

---

## 2. SMOKE-TEST RESPONSES (step 3 — the determinism proof the probe could NOT do)

Booted to gameplay (default `--song-downs 4`, hard), then the exact §6.2 sequence.
EXACT responses (run on port 53487, songMs≈16585):

| DTA expr | response |
|---|---|
| `{rb3_director_disable 1}` | `1` (int) |
| `{rb3_force_shot "coop_g_cg.shot"}` | `force_shot ok` |
| `{rb3_cur_shot}` × 6 frames | `coop_g_cg.shot` × 6 (**held on every frame**) |
| `{rb3_force_shot "this_shot_does_not_exist"}` | `force_shot not_found:this_shot_does_not_exist` |
| `{rb3_director_disable 0}` | `0` (int) |

`SMOKE_RESULT pinned=6/6 ok=True`. This is the exact assertion the probe's §1
table FAILED on with the dead `band_director` path (every cell returned `0`).

### CRITICAL DISCOVERY — the shot vocabulary the spec listed does NOT resolve

The spec/probe §2 default `coop_dir_g_cls00` (etc.) names are **dir-cut SELECTOR
object names from `arena_01.milo_xbox`** — they are NOT resolvable `BandCamShot`
objects, and the default boot song does NOT load arena_01. The **real runtime
shot names** (resolved live via `Find<BandCamShot>`):

- Need the **`.shot` suffix** (`mCurShot->Name()` returns `coop_g_n06.shot`;
  `Find` is an exact hash-table match on the stored name).
- The default boot song loads a **club-family venue** whose guitarist closeups are
  `coop_g_cg.shot` (close-guitar) + `coop_g_cg01.shot` + `coop_g_n01..n06.shot`
  (numbered framings) + `coop_g_b.shot`. There is **no `_cls`** in this venue, and
  the `coop_g_closeup_head`/`coop_g_closeup_hand` source-asset names do NOT resolve
  either.

On-device candidate resolution (live `{rb3_force_shot}`):

```
coop_g_cg            -> not_found        coop_g_cg.shot      -> OK  cur=coop_g_cg.shot
coop_g_cg01          -> not_found        coop_g_cg01.shot    -> OK  cur=coop_g_cg01.shot
coop_g_n01           -> not_found        coop_g_n01.shot     -> OK  cur=coop_g_n01.shot
coop_g_n06           -> not_found        coop_g_n06.shot     -> OK  cur=coop_g_n06.shot
coop_g_b             -> not_found        coop_g_b.shot       -> OK  cur=coop_g_b.shot
coop_d_n01           -> not_found        coop_d_n01.shot     -> OK  cur=coop_d_n01.shot
coop_g_closeup_head[.shot] -> not_found  coop_b_closeup_head[.shot] -> not_found
```

The harness therefore (a) sends each candidate **bare and with a `.shot` suffix**
and keeps whichever returns `force_shot ok`, and (b) ships per-member defaults of
the REAL resolvable names (`MEMBER_SHOTS`), not the probe's `coop_dir_*_cls`.
`not_found` candidates are skipped + recorded in `manifest.json["skipped"]`, so a
different `--song`/venue degrades gracefully.

---

## 3. The harness (`scripts/native/band-closeup-capture.py`)

Forks `crowd-shot-capture.py`'s boot→gameplay nav (reuses `keyboard-to-gameplay`
as `k`, verbatim), swaps the dead `{band_director force_shot}`/`{set disabled}` for
the new `{rb3_director_disable}`/`{rb3_force_shot}`/`{rb3_cur_shot}` verbs, and
adds the §2 force+capture loop, the §3 matched `manifest.json`, the §5
`verdict.json` + one-line verdict, and the binary-log `[SHARD_GUARD]`/
`[SHARD_RATIO]` parse (bytes read + `replace`-decode, never plain grep).

Key flags: `--member {guitar,bass,drums,keys,vocals,all}` (default **guitar**, the
only role with `_cg` closeups), `--shots "a,b,c"` (override), `--frames`,
`--frame-dt`, `--anchor-ms`, `--out`, `--tag`. A/B env toggles (`SHARD_GUARD_OFF`,
`RB3_NO_INST_REBIND`, `RB3_NO_SKEL_REBIND`, `SHARD_RATIO_DBG`, `SHARD_DBG`) are
inherited from `os.environ` and propagated to the child (NOT flags).

### `--anchor-ms` (added during impl — required for honest A/B pixel diffing)

The spec §3 demands capture at a DETERMINISTIC `songMs` target. The relative-`t0`
default (each shot starts at its own arrival clock) makes two boots' frame
clocks drift apart (measured: cumulative drift 70ms→1069ms across a 5-shot loop).
`--anchor-ms T` instead captures `(shot_i, frame_j)` at the ABSOLUTE target
`T + shot_i*frames*frame_dt + frame_j*frame_dt`, so two passes with the same
`--anchor-ms` hit the same `(shot, songMs)` pair. Measured: two runs at
`--anchor-ms 25000` landed within **2.8–20.6 ms** of each other on every frame.

### Verdict line format (last stdout line) + a REAL example

```
BAND_CLOSEUP verdict=<PASS|FAIL> member=<m> shots=<K> frames=<N> \
  pinned=<ok>/<total> drops_total=<n> drops_band=<n> drops_other=<n> \
  max_band_ratio=<r> closest_band_to_cap=<mesh:ratio|none>
```

Real example (run1, `--member guitar --frames 3`):

```
BAND_CLOSEUP verdict=PASS member=guitar shots=5 frames=3 pinned=15/15 \
  drops_total=766 drops_band=0 drops_other=766 max_band_ratio=3.15 \
  closest_band_to_cap=femaledestroyedchucks_resource.mesh:3.15
```

Exit code: 0=PASS (pinned N/N, drops_band==0); 1=FAIL (lost pin OR a band drop);
2=ERROR (hook missing → force_shot returned an int; no venue; never reached
gameplay). A full `verdict.json` + `manifest.json` (per-PNG `{shot, frame_idx,
songMs, cur_shot, pinned, file}`) are written to `--out` for the A/B comparator.

---

## 4. DETERMINISM VALIDATION (step 5) — numbers

Ran the harness twice with identical params/env, then `visual_diff.py` on the
matched frames. **Four runs total**, all `verdict=PASS`:

| run | params | pinned | drops_band | drops_other | max_band_ratio |
|---|---|---|---|---|---|
| run1 | guitar, 5 shots × 3 frames, relative-t0 | **15/15** | 0 | 766 | 3.15 |
| run2 | guitar, 5 shots × 3 frames, relative-t0 | **15/15** | 0 | 806 | 3.76 |
| det1 | guitar, 2 shots × 2 frames, `--anchor-ms 25000` | **4/4** | 0 | 424 | 4.08 |
| det2 | guitar, 2 shots × 2 frames, `--anchor-ms 25000` | **4/4** | 0 | 3094 | 5.26 |

**The camera pin is fully deterministic + reproducible.** Both members of each
pair forced the identical shot set and held it on every captured frame
(`pinned=N/N` on all four runs). `drops_band=0` on all four. This is the exact
thing the probe §3 could not show — its unpinned A/B file-size table proved every
frame was a different live render.

### The pixel-diff caveat (a genuine engine-nondeterminism finding)

Matched-frame `visual_diff.py` (STRICT, tol 2) is **NOT ~0%**, and the
investigation shows why — it is **independent of the camera pin**:

- run1↔run2 (relative-t0, clock drifts): mean **88.7%** differing.
- det1↔det2 (anchor-matched to **within 7–20 ms** of song clock): mean **84.9%**.
- Within ONE run, the SAME shot 450 ms apart: **85.8%** (pure animation).
- The SAME frame captured TWICE in ONE process with NO clock advance: **48.7%**.

Root: every `/api/screenshot` re-renders a fresh headless frame
(`RB3RenderFreshHeadlessFrame`) off the LIVE, continuously-advancing scene
(autohit-driven song clock + per-frame animation), and the global frame mean
brightness itself varies boot-to-boot (35.9 vs 44.3 — a venue-lighting/exposure
or crowd-seed nondeterminism, top-third delta dominates). So pixel-diffing two
frames — even camera-pinned and clock-matched — is dominated by live animation +
engine nondeterminism, NOT camera position. **This is exactly why scout-harness
§1c specifies `rb3_cur_shot` (pinned=N/N), not a pixel diff, as the determinism
gate.** The harness's determinism guarantee is the CAMERA PIN, which is solid; the
verdict's `visual_dropdelta_pct` (§5.4) is only meaningful for guard-ON vs
guard-OFF at a matched `(shot, songMs)` where the delta region IS the
shard/residual geometry, and the harness's job is to make that comparison
*possible* (matched camera), not to claim two boots are byte-identical.

> For tighter A/B pixel diffing, both passes MUST use the same `--anchor-ms` (so
> the song clock matches to ~10 ms); the residual cross-frame delta is animation +
> engine nondeterminism, so read the diff's **bbox / heatmap of the residual
> region**, not the raw `%differing` headline.

---

## 5. Ready for the residual root-cause batch

The harness now gives the residual batch: a PINNED guitar/drummer closeup, a
machine verdict (`pinned=N/N`, `drops_band`, `max_band_ratio`), matched
`(shot, songMs)` manifests, and the binary-log shard parse. Open items it can now
drive (from scout-harness §6.5 / Open questions):

- A/B `SHARD_GUARD_OFF` / `RB3_NO_INST_REBIND` / `RB3_NO_SKEL_REBIND` on the
  residual meshes (`scrollbar_bg`=UI scrollbar dominant, `male_extras_*`,
  `clap.mesh`) at matched anchor frames.
- `lowtopsneaks_skin.2.mesh` was observed near the band cap in det2
  (ratio 3.99 — `closest_band_to_cap`), confirming it is song/outfit-gated as the
  probe §5 predicted; pin a guitarist closeup on a low-top-sneaker outfit song to
  surface it.
- `max_band_ratio` ranged 3.15–5.26 across boots (engine nondeterminism), so a
  fix's ratio improvement must be judged against a same-boot A/B (one process,
  toggle via re-force), not cross-boot.

---

## Commit

- Branch: `wt-converge-harness`
- Commit SHA: see `worktree_commits` in the StructuredOutput / `git log` below.
