# W0.3d-b — Lane A, Stage A-S1 (attribution-first) — PLAN

Checkpoint: `/tmp/wave12-checkpoints/A-S1.json`. Build: own dir
`native/build-agent-W0.3d-b`. Engine pin `146fd19` (NO bump — coordinator only).
Mode: DIAGNOSIS + attribution instrument only. NO seam implemented this stage.

## Binding corrections carried (WAVE12_KICKOFF A1/A2/A3)
- NO staged loader patch exists — this is NEW design work.
- Mechanism "insertion order -> gRand consumption order" is REFUTED: SortPolls
  name-sorts (`Utl.cpp:207-214`), completion callbacks assert MainThread and draw
  no gRand (`Rand.cpp:80-97`), and the ~11,231-draw spread is a COUNT axis a
  permutation cannot produce.
- Prime suspect = completion-FRAME TIMING (async ThreadCall / DataLoader parse
  completions landing on different sim frames).

## Declared edit ranges (declare-before-edit)
- `scripts/native/wash-measure.py` — `--tol` default arg line (~226). DONE.
- `scripts/native/wash_matrix.py` — `--tol` default arg line (~83). DONE.
- `docs/.../execution/BOOTRNG/bootrng_probe.py` — `--tol` default arg line (~91). DONE.
- NEW `native/src/rb3_loaddet_probe.cpp` — probe TU (create).
- `native/CMakeLists.txt` — add the probe TU to the rb3-native source list (1 line).
- `native/src/rb3_session_trace.cpp` — `RB3TraceSetFrame` body (~903-905): 1 call
  to `RB3LoadDetFrameTap(frame)` (native TU, per-frame, has frame#).
- `src/system/obj/DirLoader.cpp` — after `mState = &DirLoader::DoneLoading;` in
  `LoadObjs` (~736): HX_NATIVE getenv-gated completion log call.
- `src/system/obj/DataFile.cpp` — `DataLoader::ThreadDone` after
  `ptmf = &DataLoader::DoneLoading;` (~828): HX_NATIVE getenv-gated completion log
  call (this is the async-worker-completion point — the H-TIMING hook).
- NEW `docs/.../execution/W0.3d-b/loaddet_probe.py` — N>=6-boot harness + diff.

All source edits additive, HX_NATIVE-guarded, getenv-gated (`RB3_LOADDET_PROBE`),
default-OFF. drawlog-golden flag-OFF must stay byte-identical.

## Tasks (in order)
0. Tighten wash/BOOTRNG `--tol` 2000/250 -> 150 ms. Commit. [DONE]
1. Attribution instrument: per-frame gRand draw count + per-dir load-completion
   frame across N>=6 fixed-clock boots; find FIRST divergent frame between two
   boots and name what happened on it (load-completing-on-different-frame vs order swap).
2. Pre-register H-TIMING vs H-ORDER vs H-RESEED with per-boot evidence tables +
   recommended seam design (files, shape, risks). Do NOT implement.
