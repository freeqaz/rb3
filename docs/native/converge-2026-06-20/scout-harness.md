# Scout — Deterministic "Force Band Closeup" Capture Harness (spec)

Convergence batch 2026-06-20. **Spec only — research, no code written.** This is
the implementable design for `scripts/native/band-closeup-capture.py`, the
missing tool that unblocks all further C7/C8 char-pose/skin measurement.

Read first: [`probe-data.md`](probe-data.md) (on-device measurements). The
single most important finding it carries: **`force_shot` is a SILENT NO-OP in the
native build** — there is no `band_director` DTA accessor, so the existing
`crowd-shot-capture.py`/`char-burst-capture.py`/`band-closeup-probe.py` cannot
pin a shot, and every A/B frame they capture is at a *different* auto-director
angle. That is the camera-desync false-positive the prior batches kept hitting.
So this harness CANNOT be pure-Python: it requires one tiny **native-only**
(`native/src`, Wii-neutral) DTA accessor hook. The rest is Python that reuses
`keyboard-to-gameplay.py`.

Source facts this spec is built on (all verified this batch):

- `BandDirector::ForceShot(BandCamShot*)` sets `mNextShot = shot; mDisablePicking
  = mNextShot;` (`src/system/bandobj/BandDirector.cpp:900-903`).
- `OnSelectCamera` re-picks `mNextShot` **every frame** UNLESS `mDisabled`
  (`BandDirector.cpp:1446-1466`): `if (!mDisabled) { ... mNextShot =
  FindNextDircut(); if (!mNextShot && unk58) FindNextShot(); }  PlayNextShot();`.
- `PlayNextShot()` consumes `mNextShot` → `mCurShot` and calls
  `ForceCameraShot` (`BandDirector.cpp:570-611`); after one apply `mNextShot` is
  NULL again.
- `OnSelectCamera` is driven every frame at gameplay: `App.cpp:533-539`
  (`TheBandDirector->Poll()`) → `WorldDir::Poll` → `HandleType(select_camera_msg)`
  (`src/system/world/Dir.cpp:158`). The native venue-poll fix already guarantees
  this tick runs (opt-out `RB3_VENUE_POLL_OFF=1`).
- **Pinning therefore needs BOTH**: `ForceShot(shot)` (sets `mNextShot`) AND
  `mDisabled = 1` (stops the per-frame re-pick). With `mDisabled=1`, the forced
  shot applies on the next `OnSelectCamera`, then stays — `mNextShot` is NULL and
  nothing re-picks. **force_shot does NOT need re-issuing every frame** once
  `mDisabled` is set. (Set `mDisabled` FIRST, then force the shot, so no
  intervening frame can re-pick over it.)
- The whole `BandDirector` class is `public:` (`BandDirector.h:14`), and
  `rb3_http_handlers.cpp` already dereferences `TheBandDirector->mVenue.Dir()`
  (`rb3_http_handlers.cpp:632-633`). So the accessor can touch `mDisabled`,
  `mVenue`, `mCurShot`, and call `ForceShot()` with **no friend decl and no
  header change**.

---

## 1. Native hook (REQUIRED — minimal, `native/src` only, Wii-neutral)

Without this, the harness is impossible (probe §1). Scope it to two new DTA funcs
registered next to the existing four in
`RB3HttpRegisterDtaFuncs` (`native/src/rb3_http_handlers.cpp:772-777`). No engine
change, no pin bump — purely a debug accessor in the RB3 native shim, like the
existing `rb3_pos_dump`/`rb3_char_probe`. **This is an implementation-batch task,
but it is on the critical path; the Python harness below assumes these verbs
exist.**

### 1a. `{rb3_force_shot "<name>"}` — pin a shot (idempotent, returns status)

```cpp
// native/src/rb3_http_handlers.cpp — new static, registered as "rb3_force_shot"
static DataNode RB3DtaForceShot(DataArray* a) {
    if (!TheBandDirector) return DataNode("force_shot no_director");
    WorldDir* wdir = TheBandDirector->mVenue.Dir();      // same handle rb3_pos_dump uses
    if (!wdir)       return DataNode("force_shot no_venue");
    const char* name = a->Size() > 1 ? a->Str(1) : "";   // {rb3_force_shot "coop_dir_g_cls00"}
    BandCamShot* shot = wdir->Find<BandCamShot>(name, false);
    if (!shot)       return DataNode(MakeString("force_shot not_found:%s", name));
    TheBandDirector->mDisabled = true;   // STOP the per-frame auto re-pick FIRST
    TheBandDirector->ForceShot(shot);    // then queue our shot (sets mNextShot + mDisablePicking)
    return DataNode("force_shot ok");
}
```
- Mirrors `BandDirector::OnForceShot` (`BandDirector.cpp:1216-1222`) exactly:
  `wdir->Find<BandCamShot>(name,false)` then `ForceShot`. The ONLY addition is
  `mDisabled = true` (which `OnForceShot` lacks — that is precisely why retail's
  `force_shot` doesn't stick either, by design, since editor re-issues it).
- Include already present: `#include "bandobj/BandDirector.h"` and
  `#include "bandobj/BandCamShot.h"` (pulled transitively by BandDirector.h:9).
  Add `#include "obj/Dir.h"` is already there; `WorldDir::Find<BandCamShot>` is
  the templated `ObjectDir::Find`.
- Return value is a status STRING so the Python side gets a real signal instead
  of the silent `0` the probe saw (probe §1 table). The harness must assert the
  return is `force_shot ok` (or the listed reason) — not `0` (= func not
  registered) and not `not_found:<name>` (= wrong shot name / wrong venue).
- **String-lifetime gotcha:** `MakeString` returns a transient rotating buffer
  and `DataNode(const char*)` does not deep-own it. The `not_found:%s` branch
  must back the string with a `static std::string` (exactly as `RB3DtaPosDump`
  does with `sPosDump`, `rb3_http_handlers.cpp:768`) — bare string literals
  (`"force_shot ok"`, `"no_venue"`) are fine, only the `MakeString` case needs it.
  DTA arg indexing is 1-based: index 0 is the func symbol, so the shot name is
  `a->Str(1)` (matches `RB3DtaSetSetting`'s `a->Sym(1)/a->Float(2)`).

### 1b. `{rb3_director_disable <0|1>}` — explicit director freeze/unfreeze

```cpp
static DataNode RB3DtaDirectorDisable(DataArray* a) {
    if (!TheBandDirector) return DataNode(0);
    if (a->Size() > 1) TheBandDirector->mDisabled = (a->Int(1) != 0);
    return DataNode(TheBandDirector->mDisabled ? 1 : 0);  // echo current state
}
```
- Lets the harness (a) freeze before forcing, (b) read back the state for an
  assertion, and (c) **unfreeze** so the auto-director resumes (needed if one
  process captures multiple members — see §2 caveat). `ForceShot` alone leaves
  `mDisablePicking` set; setting `mDisabled=0` here resumes auto-pick on the next
  frame.

### 1c. (optional) `{rb3_cur_shot}` — read the live shot name for the determinism assert

```cpp
static DataNode RB3DtaCurShot(DataArray*) {
    if (!TheBandDirector) return DataNode("");
    BandCamShot* s = TheBandDirector->mCurShot;
    return DataNode(s && s->Name() ? s->Name() : "");
}
```
- This is the cheapest **machine-checkable determinism proof**: after forcing,
  poll `{rb3_cur_shot}` across N frames — it must equal the forced name on every
  frame. Cheaper and more reliable than pixel-diffing two frames (which can
  differ from animation alone even with the camera pinned). **Strongly
  recommended** — it turns probe §3 ("determinism NOT verified") into a hard gate.

### Registration (3 added lines)
```cpp
void RB3HttpRegisterDtaFuncs() {
    DataRegisterFunc(Symbol("rb3_set"), RB3DtaSetSetting);
    DataRegisterFunc(Symbol("rb3_overshell"), RB3DtaOvershellState);
    DataRegisterFunc(Symbol("rb3_char_probe"), RB3DtaCharProbe);
    DataRegisterFunc(Symbol("rb3_pos_dump"), RB3DtaPosDump);
    DataRegisterFunc(Symbol("rb3_force_shot"), RB3DtaForceShot);          // NEW
    DataRegisterFunc(Symbol("rb3_director_disable"), RB3DtaDirectorDisable); // NEW
    DataRegisterFunc(Symbol("rb3_cur_shot"), RB3DtaCurShot);             // NEW (optional)
}
```

**Why not engine + pin bump?** Not needed. Everything touched (`mDisabled`,
`mVenue`, `mCurShot`, `ForceShot`) lives on the RB3 `BandDirector` (in `src/`, not
the shared engine) and is already reachable from the existing native shim. Keep
it in `native/src` so no `MILO_ENGINE_PIN` churn and zero risk to the Wii match
(the file is HX_NATIVE-only glue).

---

## 2. `scripts/native/band-closeup-capture.py` — design

Structure mirrors `crowd-shot-capture.py` (which imports `keyboard-to-gameplay.py`
as `k` and reuses its nav). Differences: it forces BAND-MEMBER closeups (not the
crowd cam), it uses the §1 verbs (`rb3_force_shot`/`rb3_director_disable`/
`rb3_cur_shot`) instead of the dead `band_director force_shot`, and it emits a
machine-readable verdict (§5).

### Boot → gameplay (reuse `k`, unchanged)
Copy `crowd-shot-capture.py`'s nav block verbatim (lines 55-117): wait HTTP →
splash Start → main_hub Confirm → song_select (`--song-downs`) → part →
difficulty → `game_screen` → `nofail` → autohit until `songMs > start+200`. This
is the proven path; do not reinvent it.

### Shot vocabulary (from probe §2)
Default to the explicit single-member CLOSEUP shots the probe enumerated in
`arena_01` (the boot venue). Per role:

| member | default closeup shot(s) | fallback (per-member medium) |
|---|---|---|
| guitar | `coop_dir_g_cls00`, `coop_dir_g_cls01`, `coop_dir_g_cls02`, `coop_dir_g_cls03` | `coop_dir_g00` |
| bass   | `coop_dir_b_cls00`, `coop_dir_b_cls01` | `coop_dir_b00` |
| drums  | *(no `_cls` exists)* | `coop_dir_d00`, `coop_dir_d_pnt_m00`, `coop_dir_d_lt00` (frames kit/feet) |
| keys   | *(no `_cls`)* | `coop_dir_k00` |
| vocals | *(no `_cls`)* | `coop_dir_v_cam_00` family |

Note shot names are **venue-specific** (they live inside `arena_01.milo_xbox`,
not loose `.shot` files — probe §2). A different `--song`/venue will have a
different shot set; the harness must NOT hardcode beyond a default and must verify
each via the `rb3_force_shot` return (`not_found:<name>` → skip + warn). Expose
`--shots-from <venue.milo_xbox>` to regenerate the candidate list with the
probe's command (`grep -rao 'coop_dir_[a-zA-Z0-9_]*' <milo> | sort -u`) as a
documented manual step, not an auto-discovery in-script.

### Force + capture loop (per shot)
```
director_disable(1)                     # freeze auto-pick FIRST (assert echo == 1)
for shot in shots:
    r = dta('{rb3_force_shot "%s"}' % shot)
    assert r startswith "force_shot ok"  # else: 0 => hook missing; not_found => bad name/venue
    # let the forced cam apply: ONE OnSelectCamera tick is enough, but settle 2-3
    for _ in range(3): verb(autohit); sleep(0.2)
    cur = dta('{rb3_cur_shot}')
    assert cur == shot                   # determinism gate (see §2 nuance)
    # capture N frames at fixed time offsets for the A/B protocol (§3)
    for i in range(args.frames):
        screenshot(out/f"{tag}_{shot}_{i}.png")
        # re-assert pin held; animation advances but camera must NOT move
        assert dta('{rb3_cur_shot}') == shot
        if i+1 < args.frames:
            advance_song(args.frame_dt)   # autohit + sleep to a deterministic songMs
```

**Determinism nuance / why §1c matters:** with `mDisabled=1` set BEFORE forcing,
`OnSelectCamera`'s re-pick block is skipped, so `mNextShot` is never overwritten;
after the first `PlayNextShot` the shot is locked. `{rb3_cur_shot}` == forced
name on every frame is the proof. The probe could NOT do this (force_shot was a
no-op, so the auto-director kept cutting — probe §3 file-size table shows every
A/B pair differs). With the hook, `rb3_cur_shot` should be constant; if it ever
drifts, the harness FAILS loudly (means `mDisabled` didn't take — e.g. a code
path re-enabled it).

### CLI flags
```
--port N                 (default: k.free_port())
--bin / --data / --overlay   (defaults from k)
--diff {easy,medium,hard,expert}   (default hard)
--song-downs N           (default 4; selects which song — see §3 caveat on shoe skin)
--member {guitar,bass,drums,keys,vocals,all}  (default guitar — only role with _cls closeups)
--shots "a,b,c"          (override the per-member default list explicitly)
--frames N               (default 2; frames captured per shot for the A/B time series)
--frame-dt MS            (default 500; deterministic song-clock advance between frames)
--out DIR                (default /tmp/rb3-bandcloseup/<tag>)
--tag NAME               (default "cap"; A/B passes use --tag guardon / guardoff etc.)
--verbose
# A/B env toggles are passed in the ENVIRONMENT, NOT as flags (see §3) — the
# script just inherits + propagates os.environ to the child, exactly as
# crowd-shot-capture.py does. Documented combos:
#   guard A/B:        SHARD_GUARD_OFF=1
#   strings-fix A/B:  RB3_NO_INST_REBIND=1
#   skel-rebind A/B:  RB3_NO_SKEL_REBIND=1
#   ratio logging:    SHARD_RATIO_DBG=1   (log EVERY ratio)
#   drop logging:     SHARD_DBG=1         (log [SHARD_GUARD] drops + dir + bone0)
```

### Diagnostics capture
Same as `crowd-shot-capture.py`: child stdout/stderr → `/tmp/rb3-bandcloseup-
<port>.log`. **LOG IS BINARY (NUL bytes)** — the harness must parse with a Python
binary read + `errno='replace'` decode (or `grep -a`), NOT plain `grep`
(probe-data "LOG GOTCHA"). The parser scans for `[SHARD_GUARD]` (drops) and
`[SHARD_RATIO]` (ratios) lines and aggregates them into the verdict (§5).

---

## 3. A/B protocol — MATCHED-frame before/after (the false-positive fix)

The known camera-desync false positive (memory: A234 "closing-gate visual wash"
was a camera-desync false positive; probe §6 `guardon_..._A` vs `guardoff_..._A`
not matched) comes from comparing frames at different camera angles. With the §1
hook this is fully solvable:

**Pinning makes A and B share the same camera.** Both passes:
1. Same `--song` (`--song-downs`), same `--diff`, same `--member`/`--shots`,
   same `--frames`, same `--frame-dt`.
2. Same SHOT forced via `rb3_force_shot` → identical camera pose. `rb3_cur_shot`
   asserts it on both passes.
3. **Same song-clock offset per frame.** This is the second axis the probe could
   not control. The harness must advance the song to a DETERMINISTIC `songMs`
   target before each capture (e.g. force_shot at `songMs ≈ T0`, then capture at
   `T0, T0+frame_dt, T0+2·frame_dt`), reading `/api/health.songMs` to gate —
   NOT wall-clock sleeps. Two passes hitting the same (shot, songMs) pair have
   the same camera AND the same char pose, so the only intended difference is the
   env toggle under test.
4. Emit a `manifest.json` per pass listing, per captured PNG: `{shot, frame_idx,
   songMs, cur_shot, file}`. The A/B comparator pairs PNGs by `(shot, frame_idx)`
   and asserts the two passes' `songMs` are within a small tolerance (±1 frame)
   before diffing — refuse to diff mismatched-clock pairs (that's the
   false-positive guard, enforced).

**Comparator:** `scripts/analysis/visual_diff.py A.png B.png --tol N --json` in
STRICT mode (build-vs-build A/B; both same size; reports `%differing`, max Δ,
bbox). Exit 0 = within threshold. For "did the dropped geometry come back?" the
meaningful diff is guard-ON vs guard-OFF at the SAME (shot, songMs): the
delta region is exactly the shard/residual geometry. For a fix A/B
(RB3_NO_INST_REBIND on vs off), the delta region is the corrected mesh.

**Pairing recipe (two processes, same params, different env):**
```
# pass A (baseline)
SHARD_RATIO_DBG=1 SHARD_DBG=1 python3 band-closeup-capture.py \
    --member guitar --frames 3 --frame-dt 500 --out /tmp/bc/on --tag on
# pass B (env toggle under test)
SHARD_GUARD_OFF=1 SHARD_RATIO_DBG=1 python3 band-closeup-capture.py \
    --member guitar --frames 3 --frame-dt 500 --out /tmp/bc/off --tag off
# compare matched frames
for s in coop_dir_g_cls00 coop_dir_g_cls01; do for i in 0 1 2; do
  visual_diff.py /tmp/bc/on/on_${s}_${i}.png /tmp/bc/off/off_${s}_${i}.png --json
done; done
```
Determinism is per-process (one env per process — probe note); the manifest's
`songMs`/`cur_shot` columns let the comparator reject any pair that drifted.

**Song caveat for the shoe-skin residual (probe §5):** `lowtopsneaks_skin` did
NOT drop on the boot song. To reproduce it the harness must target a different
song/outfit (a song whose band wears the low-top sneakers AND has high-motion
guitarist/drummer animation). `--song-downs` selects the song; a future
`--song <shortname>` (resolving via `{music_library ...}`) would be cleaner but
is out of scope for this spec. Document that the shoe residual is song-gated and
record which `--song-downs` value reproduces it once found.

---

## 4. Hook scope decision

- **force_shot alone does NOT suffice** (proven: `OnForceShot` exists in the
  engine but retail leaves `mDisabled=0`, so even if it were reachable the
  auto-director re-picks next frame). The minimal addition over retail's
  `OnForceShot` is exactly `mDisabled = true` — that single line is the lock.
- **No "lock shot" engine verb / no pin bump needed.** Everything is on the
  RB3-side `BandDirector` and reachable from the existing native shim. Keep the
  whole hook in `native/src/rb3_http_handlers.cpp` (3 new static funcs + 3
  registration lines), HX_NATIVE-only, Wii-match-neutral.
- Reuses existing plumbing: `TheBandDirector->mVenue.Dir()` (already used by
  `rb3_pos_dump`), `BandDirector::ForceShot` (already exists),
  `WorldDir::Find<BandCamShot>` (templated `ObjectDir::Find`, used by
  `OnForceShot`). Net new behavior = setting one bool.

---

## 5. Reusable verdict metric (objective fix gate)

The harness emits a one-line machine-readable summary (last stdout line, like
visual_diff.py) plus a `verdict.json`, so a fix can be gated without eyeballing:

```
BAND_CLOSEUP verdict=PASS member=guitar shots=2 frames=3 \
  pinned=6/6 drops_total=0 drops_band=0 drops_other=0 \
  max_band_ratio=1.74 closest_band_to_cap=gloves_resource.1.mesh:3.7 \
  visual_dropdelta_pct=0.00
```

Components (all parsed from the binary engine log + visual_diff):

1. **`pinned=K/N`** — fraction of (shot×frame) captures where `rb3_cur_shot` ==
   forced shot. Must be N/N or the determinism gate FAILS (hard error). This is
   the harness's own self-check that §1 worked.
2. **`drops_total` / `drops_band` / `drops_other`** — count of distinct
   `[SHARD_GUARD]` drop lines in the log, split by the line's `band`/`other`
   class field. The convergence goal is `drops_band == 0` (already true on the
   boot song — probe §4) AND, for the residual, that a fix moves a target mesh
   (`scrollbar_bg`/`male_extras*`/`lowtopsneaks`) OUT of the drop set without
   re-introducing a real explosion (i.e. its ratio drops below cap, not that the
   cap was widened). Report per-mesh drop counts (table in the doc, like probe
   §4).
3. **`max_band_ratio` + `closest_band_to_cap`** — the largest band-classified
   ratio from `[SHARD_RATIO]` lines and the mesh nearest its cap. This is the
   safety margin: a fix must NOT push any band mesh's ratio up (it should bring
   the *target* mesh's ratio DOWN by rebinding to the correct rest basis, the
   char-skinning-deform fix pattern). `gloves_resource.1.mesh` at 3.7 (saved only
   by the 40u world floor) is the current band-edge case to watch.
4. **`visual_dropdelta_pct`** — `visual_diff.py` STRICT `%differing` between the
   guard-ON and guard-OFF pass at a matched (shot, songMs). Quantifies "how much
   geometry the guard is hiding" at the pinned camera. A successful CONVERGENCE
   fix should make guard-ON look like retail (the residual rendered correctly),
   measurable as a stable, non-exploding delta region rather than a
   screen-crossing filigree (probe §6: `scrollbar_bg` sprawls 80u→324u across the
   whole highway with the guard off).

Exit code: 0 = PASS (pinned N/N, no NEW band drops, no band-ratio regression);
1 = FAIL (lost the pin, or a fix re-introduced a band drop / widened a ratio);
2 = ERROR (hook missing → `rb3_force_shot` returned `0`; bad shot name; no venue).

**Golden option (later):** once a member's closeup is pinned + converged, save
the guard-ON PNG as a golden under `docs/native/converge-2026-06-20/golden/` and
gate future changes with `visual_diff.py --tol` against it (matched shot + songMs
make the golden valid — the thing the probe's unpinned shots could never be).

---

## 6. Implementation checklist (for the impl batch)

1. Add the 3 DTA funcs + 3 registration lines to
   `native/src/rb3_http_handlers.cpp` (§1). Rebuild: `cmake --build
   native/build-native --target rb3-native -j16`.
2. Smoke-test the hook live: boot to gameplay, `{rb3_director_disable 1}` → `1`,
   `{rb3_force_shot "coop_dir_g_cls00"}` → `force_shot ok`, `{rb3_cur_shot}` →
   `coop_dir_g_cls00` held across several frames. (This is the exact assertion
   the probe's §1 table FAILED on with the old `band_director` path.)
3. Write `scripts/native/band-closeup-capture.py` per §2 (fork
   `crowd-shot-capture.py`; swap the force/disable verbs; add manifest + verdict
   emit + binary-log parse).
4. Validate determinism: run twice with identical params/env → `pinned=N/N`,
   `visual_diff` of the two runs' matched frames ≈ 0% (proves repeatability with
   the camera pinned; the thing probe §3 could not show).
5. Then it's ready for the residual root-cause batch: A/B
   `RB3_NO_INST_REBIND`/`RB3_NO_SKEL_REBIND`/`SHARD_GUARD_OFF` on the residual
   meshes with matched frames + the §5 verdict.

## Open questions (for the impl/root-cause batch, not blocking this spec)
- Which `--song-downs` (song/outfit) reproduces the `lowtopsneaks_skin` band-shoe
  drop? Probe §5 says it does NOT drop on the boot song — needs a low-top-sneaker
  outfit + high-motion animation song to surface.
- `scrollbar_bg.mesh` (71% of all drops, a UI scrollbar, 80u→324u) is NOT a
  character mesh — is its fix a UI-dir skinning rebind, or should it simply not
  be skinned/submitted under gameplay at all? Classifying it is the root-cause
  batch's job; the harness just needs to make its drop count an objective gate.
