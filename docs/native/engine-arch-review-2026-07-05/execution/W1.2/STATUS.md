# W1.2 — Extract RB3MeshEntry mesh-upload cache into its own TU — STATUS

Append-only. Update under `flock /tmp/rb3-docs.lock`. One `## W1.2.S<n> — done|partial|blocked`
section per subtask with commit SHAs, the byte-identical gate result (lineup PASS + WIDE-PNG hash
equality), and any blocker. Re-runs read this + `git log --grep=W1.2` (engine repo) and skip done
work.

Plan: `./PLAN.md`. Repo: engine (`/home/free/code/milohax/milo-native-engine`). Own build dir:
`/home/free/code/milohax/rb3/native/build-agent-W1.2`.

---

## W1.2.S0 — done

**Build:** own dir `native/build-agent-W1.2` (configured with `-DCMAKE_C_COMPILER=clang
-DCMAKE_CXX_COMPILER=clang++` — the plain `cmake -B ... -S native` default picked up GNU/g++ on
this host and failed on Clang-only flags `-ferror-limit=0`/`-fms-compatibility*`/
`-fdelayed-template-parsing`; every sibling `build-agent-*` dir already pins clang++, this just
makes that same requirement explicit for a from-scratch configure). Built `rb3-native` clean.

**Primary — lineup gate:** `python3 scripts/native/lineup-gate.py --bin
native/build-agent-W1.2/rb3-native --out /tmp/w1.2-lineup-baseline` -> **PASS** all four layers
(`img=PASS segA=PASS ratioB=PASS countC=PASS pin=PASS`, `max_band_ratio=2.91`). WIDE PNGs +
`manifest.json`/`verdict.json` copied to `evidence/baseline/lineup/`; sha256:
```
103d49ba48bee48e07272e7da7a2beba3ca0c65d911118a487f84e16261fe964  cand_coop_g_b_0.png
7573e7cce3bf1e5aed478ffcbc3bd3dc963e53bb16025b0d55edc3928d9e7485  cand_coop_g_b_1.png
b6bc789cbfafab387396d394012a028eb84663273b3b00ec8614502dae2e65ce  cand_coop_g_n03_0.png
ef41f2a009116cf073ad56e5448985d466a246c84f5852b10f476eb332fc8ee3  cand_coop_g_n03_1.png
```

**Corroborating — song-select screenshot hash:** `python3 scripts/native/song-select-capture.py
--bin native/build-agent-W1.2/rb3-native --out /tmp/w1.2-songselect-baseline` -> reached
`song_select_screen` (frame=267), captured 5 depths. PNGs + sha256 copied to
`evidence/baseline/song-select/`:
```
daeb4487a8fed031904f981d6da1fff2ff1602fb951d894b837534ea7a23b30d  native_depth_00.png
b258b389414dd99f6d2ad32dac2d0c9e458ec8613816346114938eb522fc83ce  native_depth_08.png
1e498b7a0e24034c911d591263736cd8579c1c03a27dc6346c5be34d0193c09e  native_depth_16.png
59a0cb56d81ceade19655882abadfe38d3e176217be365d5950ceb7fc601e8d9  native_depth_30.png
43f30517544d1df1bb760e3a26a98d2a1303145845f97d9efdb75225c1180ff4  native_depth_50.png
```

**Corroborating — draw-log dump (non-gating):** `RB3_DRAWLOG=1` at the lineup-gate's gameplay
scene (`game_screen`, boot -> main_hub -> song_select -> part_difficulty -> game_screen nav,
guitar/easy, forced `coop_g_n03.shot`, `songMs~20392`), fetched `/api/drawlog` (106023 bytes,
`{frame, count, draws}`) -> `evidence/baseline/drawlog.json`
(sha256 `12f722e521378d0ba094d0e9d45e734d4addb6b7d89b364b137253a0c2e209b0`). Captured via a
one-off script reusing `band-closeup-capture.py`'s proven nav module (`bc`/`bc.k`) rather than
re-deriving boot->gameplay navigation; not committed as a permanent script (PLAN.md marks this
step non-gating/corroborating only — no reusable harness was specified for it). Per PLAN.md this
is diagnostic-only pending W0.3b's frozen-clock seam, not a byte-identical gate.

**milo-engine-tests:** `SkinGolden.*` (3 real + 1 intentionally-skipped `CaptureGolden`) and
`ClipPoseFixture.*` (12/12) all **PASS** — 15 passed, 1 skipped, 0 failed. Ran via
`build-agent-W0.4` (engine repo; reused rather than re-deriving W0.4's documented
`-D_GLIBCXX_NO_ASSERTIONS` workaround for the latent `CharBones::PoseMeshes` OOB-assert-under
`-O0` — confirmed a fresh `build-agent-W0.1` config WITHOUT that flag aborts (SIGABRT, exit 134)
on `ClipPoseFixture.PoseMeshesDoesNotCrash`, matching W0.4/STATUS.md's characterization exactly;
not a new regression). Full transcript -> `evidence/baseline/milo-engine-tests.log`.

**Deviation from PLAN.md, recorded:** PLAN.md's S0 step 5 cites `execution/W0.1/STATUS.md`'s
working configure recipe (`build-agent-W0.1`); in practice that dir needs W0.4's additional
`-D_GLIBCXX_NO_ASSERTIONS` cache flag to get a green `ClipPoseFixture.*` run (W0.1's own scope
was SkinGolden only and never needed it), so `build-agent-W0.4` was used for the combined
`SkinGolden.*:ClipPoseFixture.*` filter run instead of `build-agent-W0.1` alone. No source edits
made or reverted; purely a build-dir choice, both dirs are Wave-1 artifacts reused per the task
brief ("reuse if exists").

**Note for the coordinator:** the engine repo (shared working tree, its own git repo) is
currently at `834954b` ("W0.3b: register RB3_FIXED_CLOCK..."), ahead of the `9561a19` SHA rb3's
`native/CMakeLists.txt` pins via `MILO_ENGINE_PIN` — a concurrent lane's (W0.3b) commits landed
on the shared engine tree between the coordinator's last pin bump and this subtask running.
`native/build-agent-W1.2`'s `rb3-native` (and this baseline) were therefore built against the
engine's current HEAD (`834954b`), not literally the `9561a19` string in the pin file. This is
expected under the "shared working tree, pin bumped once per wave" model (every Wave-2 agent
building from the live engine checkout is in the same position) and does not block W1.2 — the
lineup-gate PASS above is itself evidence the current engine HEAD is behavior-clean for this
baseline's purpose. Not a blocker; flagged for visibility only.

**Remains for later W1.2 subtasks (S1-S4):** none — this baseline is complete and ready to be
diffed against by every subsequent MOVE commit per PLAN.md's byte-identical evidence procedure.

**Blockers:** none.
