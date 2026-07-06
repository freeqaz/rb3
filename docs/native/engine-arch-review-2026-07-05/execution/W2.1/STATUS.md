# W2.1 — Skinned-placement contract (SYS-1 placement half) — STATUS

Append-only. Update under `flock /tmp/rb3-docs.lock`. One `## <subtask-id> — done|partial|blocked`
section per subtask with commit SHAs + blockers. Re-runs read this + `git log --grep=W2.1` and skip
done work.

Plan: `PLAN.md` (this dir). Engine pin `6221a56` (do NOT bump — coordinator only).
Lane A item 1 (engine), sequential BEFORE W2.3. Flag: `RB3_PLACEMENT_CONTRACT` (default-OFF).

## Planning — done
- PLAN.md written (Opus planner). Verified against engine HEAD `6221a56`: bug site
  `Rnd_Wgpu_RB3.cpp:2847-2848` (`else if (skinned) { identity }`), palette `:3159-3160`/`:3315`,
  double-transform trap comment `:2797-2800`, crowd ground truth `Crowd.cpp:403` (`SetWorldXfm(spXfm)`),
  flag template `RB3_FIXED_CLOCK` in `NativeCompatFlags.classification.json`. Subtasks S1 (gate-first,
  fail-red RED on current build) → S2 (coupled contract, default-OFF, atomic) → S3 (flag-ON exit
  evidence, A/A discipline). All opus.

## W2.1.S1 — done

Gate-first: stood up the placement correctness gate the splash golden cannot provide, and proved
it goes RED on the UNCHANGED `6221a56` build. No behavior change committed.

**Commits (rb3):**
- `4c0d268d` — probe + oracle + test + capture harness (gate infra).
- `519add4c` — frame-scope the probe log to the capture window (script-only refinement).

**Files:**
- `src/system/world/Crowd.cpp` — `HX_NATIVE` + `RB3_PLACEMENT_PROBE`-gated diagnostic `fprintf` at
  the `curChar->SetWorldXfm(spXfm)` site (the actual line is `:403`, not `:404`), dumping
  `inst=<i> x/y/z = spXfm.v`. Cached-static getenv (default-OFF), Wii-compile-inert. **No behavior
  change** (does not touch spXfm or the draw).
- `native/tests/placement_oracle.h` (new) — the "right, not just different" oracle. Correlates the
  probe (faithful per-instance spXfm) with the drawlog (`drawlog_compare.h` DrawLogFrame). Three
  defect kinds: `kPosedNotDrawn` (coverage — far-posed positions with no matching drawn skinned
  obj.world), `kDrawnCollapsed` (span — drawn skinned bbox extent vs posed extent), `kDrawnColocated`
  (distinctness — < 2 clusters). Reference-sanity guard returns INCONCLUSIVE (not a false pass) when
  the capture never reached a spread crowd frame.
- `native/tests/test_placement_oracle.cpp` (new, wired into `rb3-tests` in `native/CMakeLists.txt`)
  — 6 synthetic always-run tests (probe parse; catches current-build co-location naming all 3 kinds;
  passes on correct spread; tolerates sub-eps jitter; inconclusive-not-pass on empty crowd; catches
  shared-non-origin collapse) + the env-gated live gate `RealCaptureSpansBowl` (SKIPs unless
  `RB3_PLACEMENT_DRAWLOG` + `RB3_PLACEMENT_PROBE_LOG` are set).
- `scripts/native/placement-gate-capture.py` (new) — boots rb3-native headless (`RB3_HTTP`,
  `RB3_DRAWLOG`, `RB3_PLACEMENT_PROBE`, `RB3_FIXED_CLOCK`), navigates to gameplay (reuses
  `keyboard-to-gameplay` nav), pins a wide venue shot (`rb3_director_disable` FIRST then
  `rb3_force_shot`), GETs `/api/drawlog`, extracts the probe lines, runs the live oracle.

**Build:** `cmake -B native/build-agent-W2.1 -S native -DCMAKE_C_COMPILER=/usr/bin/clang
-DCMAKE_CXX_COMPILER=/usr/bin/clang++` → `rb3-native` + `rb3-tests` both build clean.

**Gate evidence — synthetic (always-run) GREEN:**
`rb3-tests --gtest_filter='PlacementOracle.*'` → 6 passed, 1 skipped (live gate). Combined with
`DrawLogGolden.*` → 15 passed / 2 skipped, **no regression** in the existing draw-log net.

**Gate evidence — FAIL-RED on the unchanged `6221a56` build (the free proof):**
`python3 scripts/native/placement-gate-capture.py` navigated to `game_screen` (songMs≈21441), pinned
`coop_all_n00.shot`, captured 387 draws (227 skinned) + the probe. `RealCaptureSpansBowl` went RED:
```
posed=103064 far=103064 matched=0 skinnedDraws=227 clusters=1 posedExtent=2277.623 drawnExtent=0.000 [FAIL]
  - posed-not-drawn: only 0/103064 far crowd positions have a drawn skinned obj.world within 1.000
  - drawn-collapsed: drawn skinned bbox extent 0.000 < 0.50 * posed extent 2277.623 (1138.811)
  - drawn-colocated: drawn skinned translations form only 1 cluster(s) at eps 1.000
```
The crowd is posed across a 2277-unit bowl (probe positions e.g. x∈[-1816,…], z≈-1016, y up to 1980)
but every one of the 227 skinned draws is drawn at the origin — the exact SYS-1 co-location the gate
was built to catch. **Gate proven RED on the current build.**

**Deviations / notes (no scope creep):**
- The plan cited the SetWorldXfm site as `Crowd.cpp:404`; the actual line is `:403` (verified). The
  bug site `Rnd_Wgpu_RB3.cpp:2847-2848` matches the plan.
- **Did NOT commit `native/tests/goldens/drawlog/gameplay_crowd.json`.** The plan lists it as an
  optional RED reference; committing the raw 387-draw drawlog would bake in the W0.3c draw-submission
  ORDER nondeterminism (unfixed) as a flaky golden. The oracle is order-insensitive (set/bbox-based)
  and derives everything from a fresh capture + probe, so it is the real gate; no exact golden needed.
  The capture script supports `--update-red-golden` if a reference file is later wanted.
- **Probe accumulates per-frame** (100k+ lines/session); `519add4c` frame-scopes the extracted probe
  to the settle+capture window. For S2's flag-ON GREEN threshold this scoped probe is a small superset
  of the exact captured frame; if a wide shot only frames part of the bowl, `spanFrac` (default 0.5,
  env-overridable in `OracleOptions`) may need lowering against the actual fixed-build capture — an S2
  tuning decision, not an S1 gap.
- **Drum prop check:** S1 emits only the crowd probe (the clean, free RED). The oracle format carries
  a `kind` field ("crowd" today, "drum" reserved) so S2 can add a drummer-prop-bone probe without
  touching the oracle. On the current build the drum prop also collapses to origin (all skinned →
  identity), so it is covered by the same span/collocation RED; a drum-specific bone/waypoint
  correlation is deferred to S2's contract work.

## W2.1.S2 — done

Landed the coupled placement contract behind ONE engine-registered default-OFF flag
`RB3_PLACEMENT_CONTRACT`. flag-OFF byte-identical; flag-ON turns the S1 oracle GREEN with the
band/hands invariance nets green and the wide gameplay shots visually equivalent to flag-OFF.

**Commit (engine `milo-native-engine`, pin NOT bumped — still `6221a56`):**
- `6852caa` — `src/platform/Rnd_Wgpu_RB3.cpp` (Half A + Half B + shard-guard world-extent fix +
  cached meshWorld) + `NativeCompatFlags.classification.json` (flag registered, class:feature /
  read:presence / default:off) + `NativeCompatFlags.gen.inc` (regenerated). `FxSendNative.cpp` left
  untouched (sibling lane).

**What the contract actually is (empirically determined, per PLAN):** a PROVABLY VERTEX-INVARIANT
reorganization, NOT a vertex-moving fix. `obj.world = mesh->WorldXfm()` (Half A) + bind-relative
palette `skin * inverse(meshWorld)` (Half B) gives, for EVERY bone,
`worldPos = obj.world*(skin*meshWorld^-1)*v = skin*v` — byte-for-byte the flag-OFF result — while
`obj.world` now records the mesh's real placement. Verified numerically with an in-loop self-check
(worst element diff **0.0000** across all meshes; ≤1e-4 for the far crowd props at ±1816).

**Key empirical findings (the PLAN's premises were partly wrong; measured with a one-shot
mesh-world dump + a cancellation self-check, both removed before commit):**
1. **Character meshes have meshWorld == IDENTITY** (band `head/hands/outfit`, `*_crowd_body*`,
   `*_extra_*`): their placement lives in world-space BONES, not the mesh world. So the reorg is an
   exact no-op for them (gated on a non-identity meshWorld) → bit-for-bit identical. Only real
   non-identity mesh worlds take the reorg: bone-attached PROPS (`fist.mesh` v=(-110,-203,3.6),
   `bonesandspikes`) and the per-instance CROWD draws that `Draw3DChars` placed via
   `SetWorldXfm(spXfm)` (`clap`/`lighter` at ±1816, z=-1016).
2. **The palette's identity FALLBACK bones must also cancel** — the null/runaway/V24/skin-clamp
   `continue` paths that leave a bone at identity are initialized to `inverse(meshWorld)` (not I)
   under the flag, else clamped crowd bones fly to `meshWorld*v` while survivors stay at `skin*v`
   → torn shards whose flung emissive geometry feeds the bloom pass into a **full-screen wash**
   (observed, then fixed). This was the coupled-half trap the PLAN warned about.
3. **meshWorld MUST be read ONCE and cached** for both obj.world and its inverse: a later
   same-draw pass (SKEL_WORLDFIX) re-dirties `WorldXfm()`, so two separate reads don't cancel →
   reintroduces the shards+wash. Caching the copy was the decisive fix (clean wide shot after).
4. **The V24 shard guard reads the palette to compute a world bbox** — under the reorg the palette
   is mesh-relative, so the guard must apply `obj.world` to its blended vertex to measure the TRUE
   world extent. Gated on the contract arm; flag-OFF path is literally unchanged.
5. `worldInvTranspose` stays identity for the contract arm (matches the flag-OFF skinned path;
   for the pure-translation meshWorlds that dominate, normals are byte-identical anyway).

**Gate evidence:**
- **flag-OFF byte-identical:** `drawlog-golden.py --fixed-clock --canonical-order` → **888 PASS**
  (canonical multiset match; the ~240-300 divergences are the pre-existing W0.3d CharEyes eye-jitter
  residual, within bound). `lineup-gate.py` → **PASS all layers** (img/segA/ratioB/countC/pin).
- **rb3-tests:** `PlacementOracle.*`/`HandsBindOracle.*`/`DrawLogGolden.*` → 18 passed / 3 skipped
  (live-capture gates).
- **engine invariance suite:** `milo-engine-tests` (context build `build-agent-W2.1-tests`,
  `DC3_DATA`+`MILO_LIB`, `ctest -j1`) → **198 pass / 0 fail / 2 skip** (bar met). SkinGolden +
  ClipPoseFixture green.
- **census:** `native_compat_census.py check` → **exit 0** (flag registered; regen clean).
- **flag-ON oracle:** `RB3_PLACEMENT_CONTRACT=1 placement-gate-capture.py` →
  `PlacementOracle.RealCaptureSpansBowl` **GREEN** — crowd drawn `obj.world` translations == the
  posed `spXfm` (spans the bowl, distinct clusters). Fail-red proven on the pre-change build in S1.
- **flag-ON wides clean:** `coop_all_n00` + `coop_dir_crowd` gameplay shots, flag-OFF vs flag-ON,
  **visually equivalent** — band correctly placed/posed, venue + visible crowd correct, no
  shards / no wash / no holes. (`/tmp/w21-ab/{off,on}_*.png`.)

**Deviations / notes (no scope creep):**
- **flag-ON splash canonical is EXPECTED to diverge (888→792 draws).** The records show the SYS-1
  signature directly: flag-OFF the crowd mesh `0xc57f…` draws 211× ALL at `obj.world=identity`
  (the co-location); flag-ON it draws 115× **spread** across the bowl (`obj.world = spXfm`,
  z=-1016). The vertices are invariant (proven), but obj.world now carries real placement, so the
  far off-screen audience instances flag-OFF wastefully drew at the origin are handled differently
  (impostor/instance bookkeeping keyed on the now-varying obj.world — NOT the shard guard nor the
  venue frustum cull, both ruled out). Zero visible impact (the audience is off-screen behind the
  venue in these pub shots; the wide A/B is identical). This is exactly the exit note's
  "world axis saturates by design — use the other axes." NOT a hard gate for S2.
- **gen.inc / ledger regen also refreshed 86 PRE-EXISTING stale `rb3/src` getenv rows** (char-skin
  probes, loader flags, etc.) from prior lanes that were never regenerated — HEAD's gen.inc/ledger
  were already census-stale before this change. `SCAN_ROOTS` canonically includes `rb3/src/system`,
  so the current generated output is 317 rows; committing it is required for `check` exit 0.
- Name-scoped scrollbar-thumb / hub-bar arms kept structurally intact (flag only rewrites the
  general `else if (skinned)` arm + its palette); their opt-out-no-op proof is S3's job.
- Removed all temp diagnostics (`RB3_MESHWORLD_PROBE`, `RB3_NOSTRIP`, `RB3_CANCEL_PROBE`) before
  commit; no census leakage.

## W2.1.S3 — done

Full flag-ON exit-evidence package produced with the **B2 A/A verify protocol** (NOT the W1.6-era
"residual-name world failure = non-blocking" rule). Read-only verify: **no engine source edits**,
pin NOT bumped (still `6221a56`; build consumed engine HEAD `6852caa` = the S2 contract via the soft
SHA-pin add_subdirectory). Build: `native/build-agent-W2.1` clean, `rb3-native` + `rb3-tests`.

### flag-OFF (`RB3_PLACEMENT_CONTRACT` unset) — byte-identical, all green
- **census** `native_compat_census.py check` → **exit 0** (317 flags, regen clean).
- **splash canonical** `drawlog-golden.py --fixed-clock --canonical-order` → **PASS (888 draws)**,
  162 known eye-residual divergences within bound (non-blocking).
- **lineup-gate** → **PASS all layers** (img/segA/ratioB/countC/pin).
- **rb3-tests** `PlacementOracle.*:HandsBindOracle.*:DrawLogGolden.*` → **18 passed / 3 skipped**
  (the 3 skips are the live-capture gates).
- **engine invariance** `milo-engine-tests` (`build-agent-W2.1-tests`, `DC3_DATA`+`MILO_LIB`,
  `ctest -j1`) → **198 pass / 0 fail / 2 skip** out of 200 (the 2 by-design skips =
  `SkinGolden.CaptureGolden` + `ExtractBik.ExtractSmallest`). `SkinGolden.*`/`ClipPoseFixture.*` green.

### flag-ON (`RB3_PLACEMENT_CONTRACT=1`) — placement correct
- **S1 placement oracle** `RB3_PLACEMENT_CONTRACT=1 placement-gate-capture.py` →
  `PlacementOracle.RealCaptureSpansBowl` **GREEN** — crowd drawn `obj.world` translations == the
  posed `spXfm`, spans the bowl, distinct clusters (381 draws / 222 skinned, 2610 probe lines).
- **lineup-gate flag-ON** → **PASS all layers** (band meshes are meshWorld≈I → vertex-invariant).
- **HandsBindOracle** synthetic green (RealPathFixture is a live-skip).

### B2 A/A discipline — genuine eye-flake re-separated from W2.1 effects (the key deliverable)
- **A/A determinism, flag-OFF** (3 boots, `--determinism-check 3 --fixed-clock --canonical-order`):
  counts **888/888/888**, 0 name-set drift.
- **A/A determinism, flag-ON** (3 boots): counts **792/792/792**, 0 name-set drift — flag-ON is as
  deterministic as flag-OFF; the 888→792 is the S2-documented off-screen crowd-impostor bookkeeping.
- **Raw-drawlog per-mesh `obj.world` classification with an A/A control** (splash, fixed-clock, three
  raw dumps OFF/OFF2/ON):
  - **A/A (OFF vs OFF2):** exactly **7 meshes** diverge in world, **all non-skinned** — the W0.3d
    CharEyes/CharLookAt eye-flake residual; 0 name-set diff.
  - **A/B (OFF vs ON):** **8 meshes** diverge = the same 7 non-skinned eye-flake + **exactly 1 NEW,
    skinned** = the crowd mesh `0xc57f0ca822`: `obj.world` translation moves `(0,0,0)` → bowl-spread
    `(-1456.16, 1313.55, -1016.31)`, draw count `211 → 115` — which accounts for the **entire**
    888→792 delta (211-115=96). **No new world divergence on any non-crowd/non-drum mesh.**
  - Conclusion: the eye residual is **A/A-invariant** (present OFF-vs-OFF2 and OFF-vs-ON alike) — it
    is pre-existing eye-flake, NOT a W2.1 effect; W2.1's only draw-stream effect is the crowd mesh.
    **B2 satisfied without the W1.6 non-blocking rule.**

### song_select hub-bar / scrollbar A/B (mandatory — those injections live in the edited block)
- Pixel A/B flag-OFF vs flag-ON (depths 0/8/16) is **within boot-nondeterminism**: A/B changed-px
  16.9/12.5/9.7% vs the A/A(OFF-vs-OFF2) control 15.8/11.9/11.2%, identical maxdelta ≈72; the churn
  is the animated 3D char-preview panel, not UI.
- **Visual**: the yellow hub highlight bar sits behind **RANDOM SONG** and the red scrollbar thumb is
  on the right-edge track — **identical in both flag states**; only the char-preview pose differs
  (boot phase). **Retail parity, no skew / origin-collapse.**
  Artifacts: `s3-artifacts/songselect_flag{OFF,ON}_depth00.png`.

### Name-scoped placement-hack opt-outs — the "contract subsumes them ⇒ no-ops" premise is REFUTED
The S2 implementation gates the contract arm as `sPlacementContract && skinned && !scrollbarThumb &&
!hubBarPlacement` (engine `Rnd_Wgpu_RB3.cpp:2878-2879`) — the name-scoped UI arms are **excluded**
from the contract arm and kept structurally intact (R5). Because the contract is **provably
vertex-invariant** (it reproduces the flag-OFF vertex positions, only recording `obj.world=meshWorld`)
and the hub-bar/scrollbar meshes are **broken flag-OFF** (that is *why* they need the name-scoped
translation-inject / bg-world-reuse fix — a genuine vertex **move**), the contract **cannot** subsume
those UI fixes. Empirically:
- **`RB3_SCROLLBAR_THUMB_FIX_OFF=1` under flag-ON is NOT a no-op.** Disabling it routes
  `scrollbar.mesh` into the vertex-invariant contract arm → the flag-OFF broken position. Region diff:
  scrollbar-thumb area changes **11.14% (max delta 87)** vs **2.12% (max 5)** A/A noise; **visually**
  the red thumb relocates from the right-edge track to **screen-center (~x=640)**. Artifact:
  `s3-artifacts/songselect_flagON_SCROLLBAR_OFF_depth00.png`.
- **`RB3_NO_HUB_BAR_PLACEMENT_FIX`** has the identical code structure (`!hubBarPlacement` exclusion);
  it is **not observable in song_select** (`highlight_main`/`highlight_pattern` are main_hub meshes —
  the song_select hub-bar region was A/A-stable, 0.37% ≤ 0.99% A/A). By code symmetry with the
  empirically-proven scrollbar case it is **NOT a no-op** on main_hub where the bar is drawn.
- **`RB3_NO_CROWD_REBIND=1` under flag-ON**: the placement **oracle stays GREEN** (obj.world==spXfm is
  rebind-independent — placement comes from the crowd dir's `SetWorldXfm`), so it is a no-op *for
  placement*; but it still governs crowd **bone-rebind skinning** (orthogonal to the draw-path
  contract), so it is **not a full no-op**.

**Conclusion / coordinator directive:** the name-scoped hub-bar / scrollbar arms and the crowd rebind
**must be RETAINED** (not deleted) at the eventual default-ON flip — the R5 "delete only after proven
no-op" gate is **not** satisfied for them. This is the safe outcome and **does not block S2's
default-OFF landing** (flag-OFF byte-identical + flag-ON crowd-correct both proven above). It corrects
the PLAN/kickoff optimism that flag-ON would make the three opt-outs no-ops.

### Crowd SKIN_CLAMP negative control — unchanged
`SKIN_CLAMP_PROBE` characterizer (`hands_bind_characterize.py --single default`), flag-ON vs a
same-build flag-OFF A/A control: the **crowd/extras clamp population is identical** — all 10 crowd
body meshes + `clap.mesh` + `lighter.mesh` present in both flag states with **equal clamp counts**;
crowd `skinpos` worst max-delta **2.255u** (boot-phase). The broader clamp-set variation (2 vs 5
unique `*_resource` hair/outfit meshes, event totals 2819 vs 3794) is **boot-random band-character
generation** — it appears flag-OFF-vs-flag-ON on the *same* build, so it is not a W2.1 effect. vs the
committed `W2.2/char/parsed-default.json` baseline the shared-mesh clamp counts are `count-differs=0`.

### Deviations / notes (no scope creep)
- **Opt-out no-op exit criterion REFUTED** (documented above). Safe R5 outcome; name-scoped arms +
  crowd rebind retained. This is the one exit-criterion item that did not land as the PLAN premised.
- Read-only verify: **no engine/rb3-src edits**; only `STATUS.md` + `s3-artifacts/` PNGs added.
  **Default-ON flip left for a separate coordinator-gated one-line commit (NOT done here)** — and per
  the directive above, that flip must NOT also delete the name-scoped arms/rebind.
- All captures used `native/build-agent-W2.1` (never `build-native`/`build-web*`).

## VERIFY — complete (all exit criteria independently reproduced; 2 safe/minor deviations, none block the default-OFF landing)

Adversarial verifier (own build `native/build-agent-W2.1-verify`, own engine test build
`milo-native-engine/build-agent-W2.1-verify-tests`; engine HEAD `6852caa` = the S2 contract,
pin still `6221a56`, NOT bumped). Re-derived every gate from scratch rather than trusting STATUS.

### flag-OFF (default) — byte-identical: CONFIRMED
- **census** `native_compat_census.py check` → **exit 0** (317 flags; `RB3_PLACEMENT_CONTRACT`
  present in classification.json AND gen.inc).
- **rb3-tests** `PlacementOracle.*:HandsBindOracle.*:DrawLogGolden.*` → **18 passed / 3 skipped**.
- **canonical splash** `drawlog-golden.py --fixed-clock --canonical-order` (my bin) → **PASS 888**
  (264 known eye-residual divergences within bound). flag-OFF the contract arm is gated off
  (`sPlacementContract==0`) = verbatim identity, so these residuals are provably pre-existing
  eye-flake, not W2.1.
- **lineup-gate** (my bin) → **PASS all layers** (img/segA/ratioB/countC/pin).
- **engine invariance** `milo-engine-tests` (own context build, DC3_DATA+MILO_LIB, `ctest -j1`) →
  **198 pass / 0 fail / 2 by-design skip**. `SkinGolden.BrokenSkinDivergesFromGolden` (the fail-red
  control) passed → shared skinning math not regressed by the S2 commit.

### S1 gate fail-red on flag-OFF — REPRODUCED (the free proof it sees the bug)
`placement-gate-capture.py --expect-red` on my flag-OFF build → `RealCaptureSpansBowl` **RED**:
`posed=2610 far=2610 matched=0 skinnedDraws=231 clusters=1 posedExtent=319.148 drawnExtent=0.000`.
Independently confirmed from the raw drawlog: **all 231 skinned draws share ONE obj.world
translation (origin), 0 non-origin, world-extent 0.0**.

### flag-ON placement correct — REPRODUCED (not a tautological pass)
`RB3_PLACEMENT_CONTRACT=1 placement-gate-capture.py` → `RealCaptureSpansBowl` **GREEN**.
Independent raw-drawlog check: flag-ON = **29 distinct obj.world translations, 28 non-origin,
world-extent 316.6** (≈ posed 319.1) — real spread matching spXfm, not a green stub. Gameplay
flag-ON: **28 of 79 distinct skinned mesh-names carry non-origin obj.world** (band/props/crowd,
incl. bone-attached instrument props) vs **0** flag-OFF → drum/instrument props ARE placed ≠ origin
by the general contract.

### B2 A/A discipline — REPRODUCED EXACTLY (the key deliverable; NOT the W1.6 non-blocking rule)
Three splash raw drawlogs under identical deterministic env (setarch -R, fixed-clock, stabilize),
per-mesh-name world classification:
- **A/A (OFF vs OFF2):** exactly **7** divergent meshes, **all non-skinned** = the W0.3d
  CharEyes/CharLookAt eye-flake residual. 0 name-set drift; counts 888/888.
- **A/B (OFF vs ON):** **8** divergent meshes = the same 7 non-skinned eye-flake **+ exactly 1 NEW,
  skinned** = crowd mesh `0xc57f0ca822620500` (obj.world `(0,0,0)` → bowl-spread, draw count
  **211 → 115** = the entire 888→792 delta). **No new world divergence on any non-crowd mesh.**
- Conclusion: the eye residual is A/A-invariant (present OFF-vs-OFF2 AND OFF-vs-ON) → pre-existing,
  NOT a W2.1 effect. W2.1's only draw-stream effect is the crowd mesh. **B2 satisfied.**

### Gameplay visual A/B — equivalent modulo a pre-existing A/A-variable wash (adversarial catch, resolved)
Initial single-frame A/B looked alarmingly different (one clean flag-ON frame vs one heavily
bloom-washed flag-OFF frame). **A/A controls dissolved it:** captured OFF twice + ON twice — a
pink bloom/exposure "wash" appears in BOTH flag states with run-to-run-varying intensity
(off1 heavy / off2 moderate; on1 none / on2 moderate). Band placement, venue, and the drum kit
(two cymbals, house-right) are **identical across all four frames** → vertex-invariance holds
visually; **no shards, no holes, no skew** (the coupled-half trap the design guards against). The
wash is orthogonal to W2.1 (pre-existing, likely the W0.3d venue/eye nondeterminism class).

### S3 refutations spot-checked — CONFIRMED
`songselect_flagON_SCROLLBAR_OFF_depth00.png`: with the scrollbar fix disabled under flag-ON the
red thumb sits at **screen-center** (mid-list), not the right-edge track → the vertex-invariant
contract genuinely does NOT subsume the name-scoped scrollbar fix. The S3 "opt-out no-op premise
REFUTED" finding is real.

### Deviations / blockers (none block the default-OFF landing; all are coordinator-flip guidance)
1. **PLAN exit criterion "opt-outs proven no-ops" is REFUTED, not met** (safe R5 outcome, correctly
   documented in S3): `RB3_NO_HUB_BAR_PLACEMENT_FIX` / `RB3_SCROLLBAR_THUMB_FIX_OFF` /
   `RB3_NO_CROWD_REBIND` are NOT subsumed by the contract (it excludes the name-scoped UI arms, and
   they fix genuinely-broken meshes). They **must be RETAINED at the flip** — not deleted.
2. **Drum-specific bone/waypoint oracle assertion never implemented** (S1 deferred → S2 subsumed into
   the general span; oracle checks `kind=="crowd"` only). Drum/instrument props DO get correct
   non-origin placement via the general contract (verified above), but there is no independent
   drummer-waypoint-correlation gate. Minor diagnostic gap.
3. **Pre-existing A/A-variable bloom/exposure wash** at gameplay cameras — orthogonal to W2.1, worth
   a separate look (W0.3d nondeterminism family), not a W2.1 regression.

**VERDICT: W2.1 complete.** flag-OFF byte-identical + S1 fail-red RED + flag-ON oracle GREEN +
B2 A/A clean + census 0 + rb3-tests 18/3 + engine 198/0/2, all independently reproduced. Default-OFF
landed on engine `6852caa`; the coordinator-gated default-ON flip remains a separate commit and per
(1) must NOT delete the name-scoped arms/rebind.
