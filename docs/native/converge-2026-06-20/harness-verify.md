# Harness-Verify — adversarial re-derivation of the force-band-closeup harness

Convergence batch 2026-06-20. Independent verification of the impl agent's
harness (commit `8927b2e9`, doc [`harness-impl.md`](harness-impl.md)). I did NOT
trust the impl agent's numbers — every claim below was re-run on-device in the
worktree `/.../worktrees/converge-harness` against the already-built
`native/build-native/rb3-native`.

**Verdict: LAND_WITH_NOTES.** The harness's primary deliverable — a deterministic,
reproducible camera PIN with a hard `pinned=N/N` gate — is SOLID and independently
confirmed. I found and FIXED one real verdict-metric defect (`drops_band` was
structurally always-0, a false PASS while a band mesh was being dropped 4048×;
fix `9c1d8500`). Two residual NOTES (A/B pixel diff can't isolate the dropped
geometry; drop metrics need debug env) are honestly documented by the impl agent
and are limitations of the *measurement*, not of the pin.

---

## 1. Code audit — native hook (PASS, with one harmless redundancy)

`native/src/rb3_http_handlers.cpp:792-839`. All checks pass:

- **Null guards:** `RB3DtaForceShot` guards `!TheBandDirector` → `no_director` and
  `!wdir` (`mVenue.Dir()`) → `no_venue`. `RB3DtaDirectorDisable` guards
  `!TheBandDirector` → `0`. `RB3DtaCurShot` guards `!TheBandDirector` and
  `s && s->Name()`. No unguarded deref.
- **Ordering (the load-bearing bit):** `mDisabled = true` is set FIRST (`:807`),
  THEN `ForceShot(shot)` (`:808`). Verified causally correct against
  `BandDirector::OnSelectCamera` (`src/system/bandobj/BandDirector.cpp:1446`): the
  per-frame re-pick block is `if (!mDisabled) { ... mNextShot = FindNextDircut() }`.
  Disabling first means no intervening `OnSelectCamera` can overwrite `mNextShot`
  between the two writes. `ForceShot` (`:900`) sets `mNextShot = shot;
  mDisablePicking = mNextShot;`; `PlayNextShot` then consumes `mNextShot` →
  `mCurShot` and the disable keeps it from being re-picked. Correct.
- **1-based DTA indexing:** `a->Str(1)` / `a->Int(1)` (index 0 is the func sym).
  Correct, matches `RB3DtaSetSetting`.
- **Member access:** the entire `BandDirector` body is under `public:`
  (`BandDirector.h:14`, no later `protected:`/`private:`). `mDisabled` (0xb5),
  `mCurShot` (0xb8, `ObjPtr<BandCamShot>` → raw via `operator T1*`), `mVenue`
  (0xf0), `ForceShot(BandCamShot*)` (`:77`) all reachable. No header change / no
  friend decl needed. Confirmed.
- **Wii-match impact = ZERO:** `native/src/rb3_http_handlers.cpp` is referenced
  ONLY by `native/CMakeLists.txt:545` (the native build). It is NOT in
  `config/SZBE69_B8/objects.json`, NOT in `splits.txt` — the Wii/decomp build
  never compiles `native/src`. No HX_NATIVE gating needed.
- **String lifetime (MakeString gotcha):** the impl backs the `not_found:%s`
  branch with a `static std::string sForceShotResult` (`:796`). NOTE: this is
  *harmless but unnecessary* — `DataNode::DataNode(const char *c)` deep-copies at
  construction (`src/system/obj/DataNode.cpp:486`: `new DataArray(c, strlen(c)+1)`).
  The string is consumed in the same statement, so even a transient buffer would
  be safe here. Keeping the static (mirroring `sPosDump`) is fine — no UAF, no
  defect. Not a blocker either way.

---

## 2. Pin proof — RE-RUN, independently confirmed (PASS)

Two independent end-to-end harness runs (`--member guitar --frames 3`), plus a
purpose-built negative control. I did not reuse any of the impl agent's captures.

| run | pinned | shots forced | result |
|---|---|---|---|
| hv1 | **15/15** | coop_g_cg/cg01/n01/n03/b (all `.shot`) | every frame `cur_shot==forced` |
| hv2 | **15/15** | identical set | every frame `cur_shot==forced` |

`hv1/manifest.json`: each shot held `cur_shot` across all 3 frames spanning ~1 s
of song clock (songMs monotonic 21633→30198), `pinned=True` on all 15. The pin
holds across frames AND across shot transitions. Bad name →
`force_shot not_found:this_is_not_a_real_shot` (static-string backing held — full
name returned, not garbage). `{rb3_director_disable}` reads state (0); `{... 1}`
sets+echoes 1.

### Negative control (the "is it luck?" guard) — DECISIVE

`/tmp/negctl.py` booted to gameplay, then:
- **NOLOCK** (no disable, echo=0): `cur_shot` DRIFTS across **3 distinct shots**
  over 18 frames: `coop_g_n01.shot → coop_b_cg02.shot → coop_gv_n08.shot`. The
  auto-director re-cuts on its own.
- **LOCK** (`director_disable(1)` then `force coop_g_cg.shot`): `cur_shot` is
  **CONSTANT** (`distinct=1`, only `coop_g_cg.shot`) across 12 frames.

→ The lock is *causal*. Without it the camera moves; with it, it holds. Not luck.

---

## 3. Determinism + log-parse + JSON outputs (PASS; one fix)

- **Binary-log parse is real, not silently-zero.** hv1 parser found
  `drops_total=4968`, `ratio_lines=13702`. `grep -a` on the same log:
  `SHARD_GUARD=4968`, `SHARD_RATIO=13709` (the 7-line delta is benign
  splitlines-vs-grep edge handling). The NUL-byte gotcha is handled (bytes read +
  `replace`-decode). Plain `grep -c` happened to count here too, but the harness
  does NOT rely on plain grep.
- **`verdict.json` / `manifest.json` well-formed.** manifest carries
  `{shot, shot_full, frame_idx, songMs, cur_shot, pinned, file, ok}` per PNG +
  `forced_shots`/`skipped`/`env`. verdict.json carries the full metric set. The
  one-line `BAND_CLOSEUP verdict=…` is the last stdout line, parseable.

### FIX `9c1d8500` — `drops_band` was structurally always 0 (false PASS)

The single real defect. The `[SHARD_GUARD]` DROP line
(`../milo-native-engine/src/platform/Rnd_Wgpu_RB3.cpp:5134`) carries **no
band/other class token** — only the `[SHARD_RATIO]` line does (trailing
`band`/`other` word, `:5116`). So `parse_shard_log`'s class detection had nothing
to match on a drop line and fell back to `other` for EVERY drop. Result:
`drops_band` could never be nonzero.

Proven false PASS: in hv1, the engine tagged `lowtopsneaks_skin.2.mesh` (a
band-outfit shoe) `class=band` on its RATIO lines AND **dropped it 4048×**
(`dir=''`, ratio 4.2 > cap), yet the verdict reported `drops_band=0
verdict=PASS`. The harness's stated convergence gate (`drops_band==0`, exit-1 on
band drop) was incapable of firing.

Fix: two-pass parse — (1) harvest the set of mesh names ever classified `band`
from RATIO lines; (2) classify each drop by name lookup in that set. Re-parsing
hv1's log with the fix yields `drops_band=4048 drops_other=920` (correct). Live
re-run hv3 (no band mesh dropped that boot) correctly yields `drops_band=0` — the
fix is correct in BOTH directions (no false positive: fist/scrollbar/male_extras
all confirmed RATIO `class=other`).

**Substantive finding for the next batch:** contrary to probe §5
("lowtopsneaks does NOT drop on the boot song"), with the camera PINNED on the
guitarist closeup it DOES drop heavily (4048× in hv1, nondeterministic boot to
boot — absent in hv3). The pinned closeup drives the guitarist's leg into a pose
that exceeds the band cap. This is exactly the song/pose-gated band-shoe drop the
spec was hunting — and only the pinned harness surfaces it. Run with
`SHARD_DBG=1 SHARD_RATIO_DBG=1` and watch `drops_band`/`drop_meshes`.

---

## 4. A/B protocol — camera matching WORKS; pixel-delta does NOT isolate (NOTE)

Anchor-matched A/B (guard ON vs OFF, `--shots coop_g_cg --frames 2
--anchor-ms 25000`): both passes pinned `2/2`, same shot, songMs within
**55–70 ms** per frame. Guard ON dropped 941, guard OFF dropped 0 (the
`SHARD_GUARD_OFF` env gate works). So the harness DOES deliver matched camera +
matched clock — the thing the probe could never do.

BUT `visual_diff.py` (STRICT, tol 2) on the matched frames reports **~90%
differing, bbox = the WHOLE frame** `[0..1279, 0..719]`. The heatmap
(`/tmp/hv-ab-heat0.png`) confirms the delta is whole-frame, NOT localized to the
dropped geometry. Cause (impl doc §4, re-confirmed here): two independent boots
differ everywhere due to (a) live animation phase even at matched songMs and (b)
boot-to-boot engine nondeterminism (lighting/exposure/crowd-seed). I visually
inspected the ON vs OFF frames: same camera/framing, but different char pose +
exposure.

→ The harness makes the convergence comparison POSSIBLE (matched camera/clock),
but the spec's `visual_dropdelta_pct` (a cross-boot raw `%differing`) does NOT
quantify a localized convergence delta — it's swamped. The impl doc is honest
about this and correctly points to a **same-process re-force A/B** (toggle in one
process, no boot nondeterminism) as the real measurement path. That same-process
A/B is NOT implemented. For now the *machine* gate is the camera pin + the
(now-fixed) `drops_band`/`max_band_ratio` metrics, NOT the pixel diff.

---

## 5. NOTES carried to the root-cause batch

1. **Drop metrics need debug env.** Both `[SHARD_GUARD]` and `[SHARD_RATIO]`
   lines only emit when `SHARD_DBG=1` / `SHARD_RATIO_DBG=1` are set. A plain
   `python3 band-closeup-capture.py` (no env) yields `drops_total=0
   max_band_ratio=0` → always PASS on the drop metrics (the `pinned` gate still
   works). The doc's A/B recipe sets them; consider making the harness export
   `SHARD_DBG`/`SHARD_RATIO_DBG` by default (cheap, throttled) so the drop gate is
   never silently inert.
2. **`max_band_ratio` can exceed the 4.0 cap (saw 4.39).** `closest_band_to_cap`
   only reports meshes with `0 <= margin` (ratio ≤ cap); a band mesh OVER the cap
   (i.e. one that the guard drops) is excluded from `closest_band_to_cap` but now
   correctly shows in `drops_band` after the fix. Fine as-is.
3. **For tighter A/B, implement same-process re-force** (force shot, screenshot,
   then re-force same shot with `SHARD_GUARD_OFF` toggled in-process) to remove
   boot nondeterminism. Out of scope for verify; the next batch's job.

---

## Verdict

**LAND_WITH_NOTES.** Pin determinism + negative control + native hook + log parse
all independently re-derived green. One real defect (`drops_band` always-0) found
and FIXED in the worktree (`9c1d8500`); it was a false-PASS on the convergence
gate while a band mesh dropped 4048×. Residual notes (cross-boot pixel diff can't
isolate the drop; drop metrics need debug env) are measurement limitations the
impl doc already discloses, not blockers. The harness is ready to drive the
residual root-cause batch.

Re-derivation artifacts: `/tmp/hv-run1`, `/tmp/hv-run2`, `/tmp/hv-run3`,
`/tmp/hv-on`, `/tmp/hv-off`, `/tmp/hv-ab-heat{0,1}.png`, `/tmp/negctl.py`.
