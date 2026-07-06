# W0.5 — Non-blind visual lineup gate (numeric assertions under image compare)

**Item:** REFACTOR_PLAN §Phase 0, W0.5. Lane 02 rec 5e (`02-mesh-skinning.md:333`,
"(e) Skinned-AABB CI gate — replace/augment the drop-ratio visual gate (blind to
shards) with the per-mesh blended-extent check wired to a golden, so 'exploded
characters' fails red"). Lane 06 §4.3.3 ("Golden-image lineup gate that is NOT
blind", `02-mesh-skinning.md:287-291`). REFACTOR_PLAN exit gate #2
(`REFACTOR_PLAN.md:38`): "W0.5 numeric per-mesh-bbox check **fails red** on a
captured exploded-patch-shard frame (the exact frame the old gate passed 34/34)".

**Scope:** rb3 repo `scripts/` + `docs/` ONLY. This item writes **no engine code
and no `native/src` code** — it is a test/gate harness built entirely from
existing HTTP + DTA accessors and existing engine debug logs. PLANNING here was
read-only; implementers likewise touch only `scripts/` and this `W0.5/` doc dir.

---

## Objective

The current band-closeup gate (`scripts/native/band-closeup-capture.py`) is
**blind to shard explosions**: it PASSed 34/34 on frames where BandPatchMesh
characters were visibly exploded into "thin teal/green/yellow slivers". Root
causes of the blindness (verified in code):

1. Its only hard numeric is `drops_band == 0` — the count of skinned meshes the
   **shard guard** dropped and post-classified as "band"
   (`band-closeup-capture.py:424-426`). With the guard **on** (default), an
   exploded mesh is silently dropped → invisible; with the guard off it renders
   as slivers but is never counted as a "band drop". Either way the number does
   not move.
2. `BandPatchMesh` (the skin-decal/patch composite — a **separate CPU geometry
   subsystem**, not a guard-tracked skinned `RndMesh`) never emits a
   `[SHARD_RATIO]`/`[SHARD_GUARD]` line at all
   (`milo-native-engine/src/platform/Rnd_Wgpu_RB3.cpp:5478` gates on
   `skinned`), so patch-shard corruption is structurally outside the old gate's
   sensor. This is the exact class the two BandPatchMesh reverts shipped through.
3. A perceptual/SSIM image compare alone is fool-able: scattered slivers of
   roughly the right colours in roughly the right screen region keep the
   translation-tolerant perceptual score above threshold
   (`scripts/analysis/visual_diff.py:289` `diff_perceptual` — content-presence +
   regional edge-energy NCC).

**Build an upgraded gate** that layers, under the image compare, numeric
assertions a shard explosion cannot pass:

- **(A) Screen-space segmentation numerics (self-contained, catches ANY on-screen
  shard including BandPatchMesh):** per-foreground-component bbox extents,
  component count, solidity (fill within bbox), and thin-sliver counts, computed
  from the screenshot. A compact character silhouette scores high solidity / few
  components; an exploded character scatters many thin low-solidity slivers.
- **(B) Per-mesh world-extent ratios (self-contained, genuinely per-mesh):** the
  engine already emits `[SHARD_RATIO] mesh='…' bindExt=… worldExt=… ratio=… band`
  per skinned mesh under `SHARD_RATIO_DBG=1`
  (`Rnd_Wgpu_RB3.cpp:5696`). Assert per-mesh blended/bind `ratio` stays within a
  golden bound (this is rec-5e's "per-mesh blended-extent check wired to a
  golden"), independent of whether the guard dropped it.
- **(C) Per-frame draw/geometry counts (self-contained via `/api/dta/eval`):**
  `{rb3_char_probe <slot>}` returns `meshes=… skinned=… verts=…` per band slot
  (`native/src/rb3_http_handlers.cpp:504-508`). Assert stable vs golden — a
  patch/shard explosion that adds, drops, or re-tessellates draws moves these
  counts.

All three are compared against a **committed golden** captured from the current
known-good build. WIDE frames are retained as PNG artifacts for human review.
The gate is PASS only if the image metric AND all numeric layers pass; it prints
per-layer verdicts so "image PASS but numeric FAIL" is visible.

### Faithful reference / ground truth

- **Golden = current known-good build**, captured once by S3 and committed under
  `scripts/native/goldens/w0.5-lineup/`. This is the oracle (there is no Wii
  frame-buffer capture); the gate proves "did not regress vs the last-known-good
  lineup" plus absolute sliver/solidity bounds.
- Retail ground-truth for reviewer-judged wide frames:
  `images/retail-screenshots/fandom_gameplay_guitar.png`,
  `fandom_gameplay_drums.png` (band members in frame).
- Numeric provenance (per-mesh extent semantics): `Rnd_Wgpu_RB3.cpp:5115-5122`
  (bind-vs-world AABB is "the truer metric"), `:5679-5698` (ratio computation +
  `[SHARD_RATIO]` emission), `:5478-5714` (guard classification the OLD gate
  reads).
- Fail-red flags (verified `getenv` sites): `RB3_NO_SKEL_REBIND`
  (`src/system/bandobj/BandCharacter.cpp:1064` — opts OUT the female-fling
  outfit-rebind fix), `RB3_NO_SKIN_CLAMP` (`Rnd_Wgpu_RB3.cpp:4946`),
  `SHARD_GUARD_OFF` (`Rnd_Wgpu_RB3.cpp:5478` — lets flung verts render as slivers
  instead of being dropped).

---

## Subtasks

Subtasks run **sequentially** by separate agents that see only this PLAN.md +
STATUS.md. Each is self-contained. Every agent: build into your OWN dir
`native/build-agent-W0.5` only; commit under `flock /tmp/rb3-git.lock` with a
`W0.5:` message prefix; stage ONLY the files you created; append a
`## <subtask-id> — done|partial|blocked` section to
`W0.5/STATUS.md` under `flock /tmp/rb3-docs.lock` before returning. This item
needs a running `rb3-native`, not a decomp build — see the build/verify block in
S2. Do NOT bump `MILO_ENGINE_PIN`.

### W0.5.S1 — Segmentation screen-space numeric analyzer
- **model:** opus (metric-design correctness: the numbers must separate a compact
  character from scattered slivers robustly, and a synthetic self-test must prove
  the separation).
- **goal:** A standalone image analyzer that turns one lineup PNG into
  shard-sensitive screen-space numerics, plus a golden-compare gate function.
- **files to touch (create):** `scripts/analysis/lineup_bbox_metrics.py`.
- **approach:**
  1. Reuse `scripts/analysis/visual_diff.py:load_rgb` for RGB loading (import it;
     it is importable). Add no new heavy deps beyond numpy + Pillow (already used
     by `visual_diff.py`).
  2. **Foreground segmentation.** The wide lineup frames have band characters
     against a venue backdrop. Segment foreground as pixels that are *not*
     background: use a robust combination so it is not brittle to venue colour —
     (a) local gradient/edge energy above a floor (reuse the `_grad_mag` idea from
     `visual_diff.py:246`) OR (b) saturation/luma distance from the dominant
     background colour (modal border colour). Produce a boolean foreground mask.
     Keep the method deterministic and documented; expose `--bg-mode` if needed.
  3. **Connected components** on the mask (implement a simple 2-pass / flood or
     use `scipy.ndimage.label` only if scipy is confirmed available — otherwise a
     numpy union-find flood; prefer no scipy dependency, matching visual_diff's
     "plain numpy" ethos). For each component ≥ a min-area threshold compute:
     bbox (x0,y0,x1,y1), area, solidity = area / bbox_area, aspect =
     max(w,h)/max(1,min(w,h)).
  4. **Frame-level metrics** (the shard-sensitive numbers):
     - `fg_bbox`: union bbox of all kept components + its extent (w,h,diagonal).
     - `n_components`: count of kept components.
     - `n_slivers`: components with aspect ≥ `SLIVER_ASPECT` (default ~6) and
       solidity ≤ `SLIVER_SOLIDITY` (default ~0.25).
     - `max_solidity` / `mean_solidity`, `fg_fill` = foreground px / fg_bbox area.
     - `total_fg_px`.
     Rationale: a compact char = few components, high solidity, low sliver count;
     an exploded char = many components, low solidity, high sliver count, and/or
     a fg_bbox that balloons across the frame.
  5. **CLI:** `lineup_bbox_metrics.py IMG.png [--json]` prints the metrics dict as
     JSON on the last line (mirror visual_diff's one-line-summary contract).
  6. **Golden-compare API:** `compare_to_golden(metrics, golden, tol)` returning a
     verdict + per-metric pass/fail, and a CLI mode
     `lineup_bbox_metrics.py IMG.png --golden G.json` that gates:
     `n_slivers <= golden.n_slivers + N`, `n_components <= golden.n_components *
     (1+f)`, `mean_solidity >= golden.mean_solidity * (1-f)`, `fg_bbox` extent
     within a ratio of golden. Exit 0 PASS / 1 FAIL / 2 ERROR. Make the tolerances
     module constants with comments; S3 will tune them against the real golden.
  7. **`--selftest`** (REQUIRED, this is the correctness proof for an opus task):
     synthesize two in-memory images — a compact filled blob, and the same blob
     shattered into ~20 thin scattered slivers — and assert the analyzer's
     `n_slivers`/`mean_solidity`/`n_components` cleanly separate them (compact
     PASSes a golden derived from itself; shattered FAILs it). Exit non-zero if
     the separation collapses. This guarantees the metric is not itself blind.
- **verification:** `python3 scripts/analysis/lineup_bbox_metrics.py --selftest`
  exits 0 and prints the two synthetic metric rows showing the separation. Also
  run it on any existing screenshot (e.g. capture one via
  `scripts/native/song-select-capture.py` or reuse a `/tmp/rb3-bandcloseup/*png`)
  to confirm it emits sane JSON on a real frame.

### W0.5.S2 — Patch-bearing WIDE lineup capture harness
- **model:** sonnet (mechanical: adapt the proven capture/nav/pin machinery; add
  wide shots + numeric collection).
- **goal:** Boot `rb3-native` headless, reach gameplay, pin **WIDE** band shots
  that frame **multiple BandPatchMesh-wearing characters**, and capture matched
  (shot, songMs) WIDE PNGs plus a `manifest.json` carrying all three numeric
  sources per frame.
- **files to touch (create):** `scripts/native/patch-lineup-capture.py`.
- **approach:**
  1. **Reuse, do not re-derive, the nav + pin machinery.** Import
     `scripts/native/band-closeup-capture.py` the same way it imports
     `keyboard-to-gameplay.py` (`importlib.util.spec_from_file_location`), and
     reuse its `force_shot`, `director_disable`, `cur_shot`, `advance_to_songms`,
     `parse_shard_log`, and the boot→gameplay nav block
     (`band-closeup-capture.py:283-336`). Keep the same env-toggle propagation
     (`SHARD_RATIO_DBG`, `SHARD_GUARD_OFF`, `RB3_NO_SKEL_REBIND`, etc. flow
     through `os.environ`).
  2. **WIDE shot list.** The old harness pins tight single-member closeups
     (`coop_g_cg`). For a lineup, pin WIDE/establishing shots that frame the whole
     band. Provide a candidate list of wide names and rely on the existing
     graceful `force_shot` fallback (bare + `.shot` suffix; `not_found` skipped +
     warned): try e.g. `coop_all`, `coop_wide`, `coop_band`, `coop_establish`,
     `coop_crowd`, `coop_g_n03`/`coop_g_b` (wider guitarist framings that still
     catch neighbours), plus a `--shots` override. Whichever resolve are used;
     log which resolved so S3/S4 can pin a known-good wide shot. Enable
     `SHARD_RATIO_DBG=1` by default (env) so per-mesh ratios are logged.
  3. **Per-frame numeric collection.** For each captured WIDE frame, in addition
     to the PNG, record into the manifest:
     - `char_probe`: for slots 0..N (default 0..3), the parsed
       `{rb3_char_probe <slot>}` string → `{meshes,skinned,verts,loading}`
       (self-contained draw/geometry counts, layer C).
     - `shard_ratios`: parse `[SHARD_RATIO]` lines from the engine log for this
       run into `{mesh: {bindExt, worldExt, ratio, class}}` and the
       `max_band_ratio` (extend `parse_shard_log`'s regex, or add a
       `parse_shard_ratios(log)` that captures the full per-mesh tuple — layer B).
     - `songMs`, `cur_shot`, `pinned`, `file` (as the old manifest already does).
  4. **Artifacts:** write WIDE PNGs `<tag>_<shot>_<i>.png` and a `manifest.json`
     to `--out` (default `/tmp/rb3-lineup/<tag>`); these are the reviewer-judged
     wide frames. Exit codes mirror the old harness (0 nav+pin ok, 2 error) —
     BUT this harness is a **capturer**, not the gate; do NOT bake in the old
     `drops_band==0` PASS/FAIL (S3 owns the verdict). Print a machine-readable
     last line `PATCH_LINEUP out=… frames=… forced_shots=… max_band_ratio=…`.
  5. **CHAR_PROBE note:** char_probe needs the preview/char cache; band chars are
     resident in gameplay so slots resolve there. If a slot returns
     `null_char`, record it verbatim (S3 treats a golden→candidate transition
     into `null_char` as a FAIL signal, not a crash).
- **verification (this is the build+run gate for the whole item):**
  ```
  cmake -B native/build-agent-W0.5 -S native \
    && cmake --build native/build-agent-W0.5 --target rb3-native -j8
  RB3_BIN=native/build-agent-W0.5/rb3-native \
    python3 scripts/native/patch-lineup-capture.py --frames 2 --out /tmp/rb3-lineup/base --tag base
  ```
  (Pass the freshly-built binary via the harness's `--bin`, matching
  `band-closeup-capture.py`'s `--bin` default `k.DEFAULT_BIN`; do NOT touch
  `native/build-native`.) Confirm ≥1 wide shot resolved, ≥1 PNG written, and
  `manifest.json` contains non-empty `char_probe` + `shard_ratios` for a frame.

### W0.5.S3 — Composite gate driver + committed golden
- **model:** opus (layering logic + golden generation + tolerance selection — the
  gate must PASS clean on the good build and be provably able to FAIL red).
- **goal:** One driver that runs a capture (S2), compares each frame against a
  committed golden across all layers, and returns a single PASS/FAIL with visible
  per-layer verdicts. Generate + commit the golden from the current build.
- **files to touch (create):** `scripts/native/lineup-gate.py`; golden fixtures
  `scripts/native/goldens/w0.5-lineup/` (golden WIDE PNGs + `golden.json` with
  per-frame segmentation metrics, per-mesh ratio bounds, char_probe counts, and
  the resolved wide shot name(s)).
- **approach:**
  1. **Golden generation mode** (`lineup-gate.py --gen-golden`): build/run S2's
     capture on the current known-good build, pick the resolved wide shot(s),
     run `lineup_bbox_metrics.py` (import S1) on each WIDE PNG, and write
     `golden.json` = `{shot, frame_idx, seg_metrics, char_probe, shard_ratio_max,
     per_mesh_ratio_cap}` plus copy the golden PNGs alongside. Commit these.
  2. **Gate mode** (default): run S2's capture with the same params/shot, then for
     each matched (shot, frame):
     - **image layer:** `visual_diff.py --perceptual` candidate-vs-golden PNG
       (retain as the coarse "did it render at all" check) AND a strict/heatmap
       artifact for humans. Record its verdict but treat it as ADVISORY — the doc
       already shows it is fool-able.
     - **numeric layer A (segmentation):** `lineup_bbox_metrics.compare_to_golden`
       vs `golden.json.seg_metrics`.
     - **numeric layer B (per-mesh ratio):** assert every `shard_ratios[mesh].ratio
       <= per_mesh_ratio_cap` and `max_band_ratio` within golden bound.
     - **numeric layer C (draw counts):** assert `char_probe` `meshes/skinned/verts`
       per slot within golden tolerance and no slot flipped to `null_char`.
     - Overall verdict = AND of the numeric layers (A,B,C). The image layer is
       reported but does NOT gate (it is the blind one). Emit `verdict.json` with
       every layer's verdict + which metric failed, and a one-line
       `LINEUP_GATE verdict=… img=… segA=… ratioB=… countC=…` last line.
  3. **Tune tolerances** (from S1 constants) against the real golden so a clean
     re-run of the good build is a stable PASS across 2–3 boots (account for
     minor frame jitter — use `--anchor-ms` in the underlying capture for
     cross-run pose determinism, as `band-closeup-capture.py:346-361` documents).
  4. Wire nothing into CI here (coordinator does exit-gate #4); just make the
     driver a clean `exit 0/1/2` so CI can call it later.
- **verification:**
  ```
  cmake -B native/build-agent-W0.5 -S native \
    && cmake --build native/build-agent-W0.5 --target rb3-native -j8
  python3 scripts/native/lineup-gate.py --gen-golden --bin native/build-agent-W0.5/rb3-native   # once
  python3 scripts/native/lineup-gate.py            --bin native/build-agent-W0.5/rb3-native      # -> PASS
  ```
  Clean build gates PASS on all layers across two consecutive runs. Commit
  `scripts/native/lineup-gate.py` + `scripts/native/goldens/w0.5-lineup/**`.

### W0.5.S4 — Fail-red proof + STATUS
- **model:** sonnet (run + adjudicate; the pieces exist, this proves them).
- **goal:** Demonstrate the REFACTOR_PLAN exit-gate #2 red: a deliberately-broken
  skin run FAILs a NUMBER in the new gate while (i) the OLD band-closeup gate
  metric and (ii) the perceptual image layer would have PASSed. Capture the
  exploded frame + artifacts as committed fixtures.
- **files to touch (create):** `scripts/native/w0.5-failred.sh` (or `.py`) driver;
  `docs/native/engine-arch-review-2026-07-05/execution/W0.5/failred/` artifacts
  (exploded WIDE PNG(s), `new-gate-verdict.json`, `old-gate-verdict.json`,
  `image-layer-verdict.json`, short `RESULT.md`).
- **approach:**
  1. **Induce shards.** Run S2's capture (and the OLD `band-closeup-capture.py`)
     with the broken-skin env. Start with
     `RB3_NO_SKEL_REBIND=1 SHARD_GUARD_OFF=1 SHARD_RATIO_DBG=1` (unbinds the
     female outfit fix → wrong basis fling; guard off → flung verts render as
     slivers instead of being dropped). If shards are not visually present in the
     WIDE PNG, add `RB3_NO_SKIN_CLAMP=1`. Confirm visually (the artifact frame
     must show the sliver explosion) — do NOT declare success on numbers alone.
  2. **Show OLD metric passes.** Run `band-closeup-capture.py` with the SAME
     broken env and record its `verdict.json`: it reports PASS (or at minimum
     `drops_band=0`) — the documented blindness. Save as `old-gate-verdict.json`.
  3. **Show image layer passes.** Run `visual_diff.py --perceptual` of the
     exploded WIDE frame vs the golden PNG; record that its score ≥ min (PASS) —
     the SSIM/perceptual blindness. Save as `image-layer-verdict.json`.
  4. **Show new gate fails a number.** Run `lineup-gate.py` with the broken env;
     record `verdict.json` FAIL with the specific failing layer(s) (segmentation
     `n_slivers`/`mean_solidity` and/or per-mesh `ratio` cap). Save as
     `new-gate-verdict.json`.
  5. Write `failred/RESULT.md` tabulating: exploded frame path, OLD gate = PASS,
     image layer = PASS, NEW gate = FAIL (which metric, golden vs observed value).
     This is the literal exit-gate #2 evidence.
  6. Append the final `## W0.5.S4 — done` section to STATUS.md with commit SHAs and
     the three verdict values.
- **verification:** `failred/RESULT.md` exists and shows OLD=PASS, image=PASS,
  NEW=FAIL with concrete numbers; the exploded PNG visibly shows slivers. Commit
  the driver + `failred/` fixtures.

---

## Exit criteria

1. `scripts/analysis/lineup_bbox_metrics.py --selftest` exits 0 and cleanly
   separates a compact blob (PASS) from a shattered-sliver blob (FAIL).
2. `scripts/native/patch-lineup-capture.py` boots rb3-native, resolves ≥1 WIDE
   band shot framing multiple characters, writes WIDE PNG artifacts + a
   `manifest.json` with non-empty per-frame `char_probe` (draw counts) and
   `shard_ratios` (per-mesh extents).
3. A committed golden exists (`scripts/native/goldens/w0.5-lineup/golden.json` +
   PNGs); `scripts/native/lineup-gate.py` PASSes on the current build across two
   consecutive runs, printing per-layer verdicts (image / segA / ratioB / countC).
4. **Fail-red (REFACTOR_PLAN exit-gate #2):** with the broken-skin env
   (`RB3_NO_SKEL_REBIND=1 SHARD_GUARD_OFF=1` [+`RB3_NO_SKIN_CLAMP=1` if needed]),
   `failred/RESULT.md` demonstrates the NEW gate FAILs a numeric layer while the
   OLD `band-closeup-capture.py` metric PASSes AND the perceptual image layer
   PASSes, with the exploded WIDE frame committed as evidence.
5. No engine / `native/src` files modified; all changes under `scripts/` +
   `docs/native/.../W0.5/`. `native/build-native` and `build-web*` untouched.

## Risks / conflicts

- **W0.3 (draw-log golden)** will add a `/api/drawlog` endpoint + edit
  `native/src/rb3_http_handlers.cpp` and `Rnd_Wgpu_RB3.cpp`. W0.5 is designed to
  NOT depend on W0.3 (uses `SHARD_RATIO_DBG` log + `rb3_char_probe` for the
  per-mesh/draw-count numerics). **No file-write overlap** — W0.5 never edits
  those files. If W0.3 lands first, a later polish pass MAY switch layer B/C to
  `/api/drawlog`, but that is out of scope for this wave. Check
  `execution/W0.3/STATUS.md` at S3 time; as of planning it does not exist (W0.3
  not landed) → use the fallback path unconditionally.
- **W0.6 (flag registry skeleton)** may reroute `getenv` reads. W0.5 depends on
  the env-var NAMES `SHARD_GUARD_OFF`, `SHARD_RATIO_DBG`, `RB3_NO_SKEL_REBIND`,
  `RB3_NO_SKIN_CLAMP` staying stable (it sets them in the subprocess env). W0.6
  is a registry *skeleton* this wave and keys by name; low risk. If S4's broken
  env stops inducing shards, verify these names weren't renamed and update.
- **W1.1 / W1.7 (Rnd_Wgpu_RB3.cpp edits)** are string-move / relocation, byte
  identical output — they must NOT change the `[SHARD_RATIO]` line format
  (`Rnd_Wgpu_RB3.cpp:5696`) or the golden's per-mesh numerics shift. W0.5 reads
  that format; if a Wave-1 render edit lands after the golden is committed and
  moves numerics, regenerate the golden (`lineup-gate.py --gen-golden`). Note
  this in STATUS if observed.
- **No git collision:** every W0.5 file is newly created under `scripts/` or
  `docs/.../W0.5/`; no other Wave-1 item writes those paths. Stage only your own
  new files; never `git add -A`.
- **Determinism / flake:** cross-run pose jitter can wobble segmentation metrics.
  Use `--anchor-ms` (absolute songMs) for cross-run pose determinism as
  `band-closeup-capture.py:346-361` documents, and set tolerances (S3) from 2–3
  real good-build runs, not a single capture.
