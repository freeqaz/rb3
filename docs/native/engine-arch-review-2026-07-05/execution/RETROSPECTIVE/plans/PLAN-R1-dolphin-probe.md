# PLAN R1 — Dolphin ground-truth probe (thin)

**Item:** ROADMAP.md R1. **Author:** Fable planner, 2026-07-07 (read-only research pass;
every load-bearing claim below was re-verified against source/filesystem this session —
commands quoted inline). **Consumers:** R5 (hands endgame decision), every future
skinning/anim claim, eventual true pixel goldens (deferred, OPTIONS §3b).

---

## 0. RECORD CORRECTION (resolve the stub-vs-live contradiction FIRST)

The Wave-16 HANDS-FIX lane called `../milo-trace` "a project SKELETON — almost everything
is a documented stub" (`execution/HANDS-FIX/STATUS.md:102-104`); the retrospective called
it "6 waves live" (OPTIONS §1). **Both quoted real text; the lane quoted a STALE README.**
Verified by direct inspection:

- The stub sentence is the **first paragraph of `milo-trace/README.md` (lines 4-8)**,
  written at scaffold time (first commit `371fca0`) and never updated. That is what the
  Wave-16 lane read.
- The repo has **15 commits / 6+ waves of landed, verified work** (`git log --oneline`:
  `81a57f9` M0-core value-proof → `2ebb490` W6). The **Dolphin/Wii track specifically is
  NOT greenfield** — it is landed and live-verified:
  - **W2 (build):** Dolphin cloned + built at `/home/free/code/milohax/dolphin` (HEAD
    `aabea5b`, shallow). Binary **exists today**:
    `/home/free/code/milohax/dolphin/build/Binaries/dolphin-emu-nogui` (23.6 MB, ran
    `--version` → `Dolphin aabea5b` this session). Full verified boot recipe in
    `milo-trace/docs/capture-patches/dolphin-build-notes.md` — headless Null-GFX boot of
    the rb3 **Bank-8 debug `main.dol`**, GDB stub answering RSP `?/g/m` with the CPU
    halted at DOL entry `0x80006310`.
  - **W4 (capture client):** `milo_trace/capture/dolphin.py` (962 lines, pure-stdlib RSP
    client, 24 tests) — live-verified register/memory reads byte-identical to dtk ground
    truth. `milo_trace/capture/wii_symbol_map.py` (385 lines) parses the CW map.
  - **W5 (M3 value gate): PASSED byte-identical** (`docs/STATUS-X5-wave.md` §W5) —
    `strlen@0x80a38588` constructed over RSP on the bare Bank-8 DOL, replayed vs the
    decomp `.o`, byte-identical.
  - **W7 (interpreter hook): LANDED in the fork** — commit `bc3b1f5` "PowerPC: milo-trace
    W7 interpreter capture hook (Wii/Gekko)" on top of `aabea5b`, working tree clean.
    `Source/Core/Core/PowerPC/MiloTrace.{h,cpp}` on disk; captures full `ppcState` incl.
    ps1/GQR; env-driven (`MILO_TRACE_OUT`/`MILO_TRACE_ENTRIES`), inert when unset.
- What has genuinely **never been done**: a full-game boot (W1 "no disc" was the blocker
  at the time), reaching an interactive scene, and any object/bone-level dump. That — and
  only that — is R1's new ground.
- **The W1 blocker is gone:** `Rock Band 3 (USA).wbfs` sits at the rb3 repo root —
  verified this session: 4,112,515,072 bytes, magic `WBFS` (`xxd -l 16`).

**Second premise correction (native seam):** OPTIONS §1 says the native capture-seam
endpoints "already exist". True but the semantics matter: **`/api/memory` is a SANDBOXED
ARENA** (offsets into a 1 MiB scratch buffer; it can NOT read live engine memory —
`native/src/rb3_replay_api.cpp:24-31`). The live-state seam is **`/api/call`**, which
invokes an exported `extern "C"` symbol **on the main thread** (dispatched from
`ProcessCommands`, same contract as DTA-eval — `rb3_replay_api.cpp:18-23`);
`native/src/rb3_replay_capture.cpp` (`rb3rc_capture_sweep`, line 357) is the working
precedent for "dump live engine state via /api/call". R1's native half is a new exported
dump function in that pattern, NOT a use of `/api/memory`.

---

## 1. OBJECTIVE + non-goals

**Objective:** a one-command probe that produces, for one band member's hand chain, a
**two-adjacent-bone RELATIVE-pose diff** between the real game running under Dolphin
(ground truth) and the native port, joined at **matched clip time** — plus the
authored-bind comparison (Wii-ARK vs 360-ARK) that several "faithful-to-what?" arguments
silently depend on. Output = per-bone-pair delta table (JSON + human table), committed
under `execution/R1-DOLPHIN/evidence/`.

Spec anchor (Wave 16, `execution/HANDS-FIX/STATUS.md:104-110`): the hands shard is a
MULTI-bone blend tear; a single-bone WorldXfm diff cannot capture it. The probe's
**primary output is inter-bone deltas** (`D_pair = inv(W_a)·W_b` for adjacent bones,
compared matrix-relative across sides); single-bone dumps are the building block, kept in
the output for provenance.

**Non-goals:**
- NOT full milo-trace M3 record/replay for gameplay (explicitly deferred, OPTIONS §3).
- NOT a fix lane. R1 produces evidence; R5 decides. No engine/rb3 gameplay-code edits.
  (The only allowed native-code edit is the additive, env-gated dump TU in `native/src/`.)
- NOT pixel goldens (revisit after R1 per OPTIONS §3b).
- NOT per-function tracing/breakpoints — the X5 wave found this headless Dolphin does not
  honor `Z0`/`c` reliably (`docs/STATUS-X5-wave.md` "Breakpoints" note); the design below
  needs neither.

---

## 2. CURRENT STATE (all verified this session)

| Asset | State | Evidence (command run) |
|---|---|---|
| Dolphin fork | Built, W7 hook committed (`bc3b1f5`), tree clean | `git -C ~/code/milohax/dolphin log --oneline -5; status --short` |
| `dolphin-emu-nogui` | On disk, runs, `Dolphin aabea5b` | `--version` executed |
| Headless boot of Bank-8 DOL | Proven (halts at `0x80006310` under stub) | `dolphin-build-notes.md` §4 (W2, reproduced recipe incl. config-key corrections: `Dolphin.Core.CPUCore=0` = interpreter, `Dolphin.General.GDBPort`) |
| RSP capture client | `milo_trace/capture/dolphin.py` (962 ln) + tests | `wc -l`, STATUS-X5 §W4 |
| Disc | `rb3/Rock Band 3 (USA).wbfs`, 4.1 GB, `WBFS` magic | `xxd -l 16` |
| Debug DOL | `orig/SZBE69_B8/sys/main.dol`, 13,068,128 B | `ls -la` |
| DOL-boot-with-disc path | Dolphin inserts `MAIN_DEFAULT_ISO` when booting a DOL | `dolphin/Source/Core/Core/Boot/Boot.cpp:480-482` |
| Savestate from CLI | `dolphin-emu-nogui --save_state=<file>` supported | `MainNoGUI.cpp:216-230` |
| Scripted input backend | Pipe input backend compiled in | `Source/Core/InputCommon/ControllerInterface/Pipes/Pipes.{cpp,h}` |
| Map anchors | `__vt__8CharBone` @ **0x80bfeaa8**; `__vt__13CharServoBone` @ 0x80c05d60; `TheBandDirector` @ 0x80d16c9c; `TheTaskMgr` @ 0x80cacb98 | `grep band_r_wii.map` |
| Bank-5 DWARF ELF (struct layouts) | 40.6 MB, on disk | `ls milo-executable-library/rb3/Wii Proto (Bank 5) (Debug)/band_r_wii.elf` |
| Native live-state seam | `/api/call` main-thread invoke; `rb3rc_capture_sweep` precedent | `rb3_replay_api.cpp:1-40`, `rb3_replay_capture.cpp:357` |
| Native pose dump (offline) | `rb3-native --viewer ... --pose-dump <json>` dumps every Trans local+world; **NO clip/frame playback control** (only `--sim` hair settle, `--test-bone`) | `rb3_viewer.cpp:120-180` |
| Native matched-frame harness | `scripts/native/keyboard-to-gameplay.py` + `RB3_FIXED_CLOCK=1` (Wave-16 protocol) | HANDS-FIX STATUS gates block |
| Layout gotchas | `RndTransformable : public virtual RndHighlightable` (virtual base → offsets non-trivial); `WorldXfm()` is **dirty-cached** (`mCache->mFlags & 1` → `WorldXfm_Force()`), so a paused read of `mWorldXfm` can be stale | `src/system/rndobj/Trans.h:42,95-109` |

---

## 3. DESIGN

### 3.1 Approach in one paragraph

No breakpoints, no per-function tracing. Boot the **full game** in headless Dolphin
(debug DOL + retail disc filesystem via `DefaultISO`; retail-DOL fallback), drive it to a
scene with animated band characters (hub first, gameplay stretch), **pause** via RSP
interrupt (`0x03`), **bulk-read guest RAM** (primary: `/proc/<pid>/mem` on the emulated
region, the dolphin-memory-engine technique; fallback: RSP `m` packets — already proven),
**discover CharBone instances by vtable scan** (`0x80bfeaa8` from the map), and dump per
bone: name, `mLocalXfm`, `mWorldXfm` (cached AND recomputed from the parent chain), parent
name — plus the owning member's **clip driver state (clip name, frame)** as the join key.
The native side dumps the same JSON shape from a live fixed-clock session via a new
`/api/call` entry point. A join/diff tool matches records on `(member, clip, frame)`,
computes adjacent-pair inter-bone deltas on both sides, and reports the matrix-relative
angle between them per pair.

Key insight on "matched clip time" (the hard part): **do not synchronize execution —
label every dump with the clip state read from the same memory image and join offline.**
Both sides pass through the same `(clip, frame)` keys because the anims are clip-driven;
BOOTRNG (camera/crowd) does not perturb the join key. No wall-clock anchor needed.

### 3.2 Components + file layout

| Piece | Where | What |
|---|---|---|
| `tools/wii_bone_probe.py` | **milo-trace repo** (owns Dolphin capture; reuses `RspConnection`, `WiiSymbolMap`) | boot orchestration, pause, memory access (proc-mem + RSP fallback), vtable scan, typed struct reads, JSON dump. Subcommands: `boot-check`, `dump --out wii_bones.json`, `freeze-dump --n 5` |
| `struct_offsets.py` (module of the above) | milo-trace | offsets for `Hmx::Object` name, `RndTransformable::{mLocalXfm,mWorldXfm,mParent,mCache}`, `CharBone`, clip-driver members — extracted ONCE from the Bank-5 DWARF ELF (`gdb -batch -ex 'ptype /o RndTransformable' band_r_wii.elf`), checked in as constants WITH the gdb transcript in evidence/. Bank-5-vs-8 drift is a named risk → runtime validation gates (G2) |
| `rb3_bone_probe_native.cpp` | **rb3** `native/src/` (pattern: `rb3_replay_capture.cpp` — additive TU, env-gated, one CMake source entry) | `extern "C" int rb3bp_dump_bones(void)` → walks live band characters (ObjectDir census, as rb3-viewer `--list` does), writes the SAME JSON shape (bones + clip keys) to `RB3_BONE_PROBE_OUT`. Invoked via `/api/call` at moments chosen by the harness |
| `scripts/analysis/interbone_diff.py` | **rb3** | join on `(member, clip, round(frame, q))`, compute `D_pair = inv(W_a)·W_b` per adjacent pair per side, report `angle(D_wii · inv(D_native))` + translation delta; per-population rows (per member / per mesh / per pair — OPTIONS §4.2); self-test mode (G4) |
| Lane hub | `execution/R1-DOLPHIN/{PLAN→this file's copy, STATUS.md, evidence/}` | evidence committed, never bare `/tmp` (OPTIONS §4.7) |

### 3.3 Data shape (both sides emit identically)

```json
{ "side": "wii|native", "build": "bank8_debug_dol|retail_dol|native@<sha>",
  "scene": "hub|gameplay:<song>", "pause_id": 3,
  "members": [ { "member": "guitarist0", "outfit_meshes": ["hands_naked.mesh", ...],
    "clip": { "name": "idle_groove", "frame": 143.0, "source": "CharClipDriver@0x92xxxxxx" },
    "bones": [ { "name": "bone_R-hand", "parent": "bone_R-forearm",
      "local": [12 floats, row-major 3x4], "world_cached": [12], "world_recomputed": [12],
      "dirty": false, "addr": "0x92xxxxxx", "vtable": "0x80bfeaa8" } ],
    "bind": [ { "mesh": "hands_naked.mesh", "bone": "bone_R-hand", "offset": [12] } ] } ] }
```

`bind` = the mesh's embedded `BoneTransAt` inverse-bind offsets — this is what makes the
**Wii-ARK-vs-360-ARK authored-bind delta measurable instead of assumed** (OPTIONS §2#1
risk ii). `world_recomputed` = product up the parent chain from locals — defeats the
dirty-cache staleness trap (Trans.h:104-109) and is itself a consistency check.

### 3.4 Wii-side mechanics (detail)

- **Boot:** `dolphin-emu-nogui --platform=headless --video_backend=Null -u <scratch-user>
  -C Dolphin.General.GDBPort=2345 -C Dolphin.DSP.Backend="No Audio Output"
  -e orig/SZBE69_B8/sys/main.dol` with `DefaultISO = <wbfs>` set in the scratch user's
  `Dolphin.ini` (Boot.cpp:480 inserts the disc for DOL boots; exact `-C` key confirmed in
  M1). **JIT core (default) for the probe** — we only pause+read memory; the interpreter
  (CPUCore=0) is needed only if the optional W7-hook freeze extension is used.
- **Liveness/heartbeat without video:** poll a monotonically advancing guest word (e.g.
  `TheTaskMgr` time fields @ 0x80cacb98, or the frame counter once identified) via memory
  reads; "booted to interactive" = heartbeat advancing + CharBone vtable hits > 0.
- **Memory access:** primary = find the emulated-RAM host mapping in `/proc/<pid>/maps`
  (Dolphin maps guest RAM through a `/dev/shm/dolphinemu.<pid>` memfd; MEM1 = 0x01800000
  bytes at guest 0x8000_0000, MEM2 at 0x9000_0000 — same technique as
  dolphin-memory-engine), read via `/proc/<pid>/mem`. Fallback = RSP `m` reads (proven in
  W2/W4; a full MEM1+MEM2 scan over RSP is minutes, acceptable).
- **Pause protocol:** RSP `0x03` interrupt → dump → dump AGAIN → require byte-identical
  bone records (G3) → `c`. Mid-frame-tear mitigation: `world_recomputed` consistency +
  N≥5 pauses; if tearing is ever observed, the priced upgrade is a ~20-line extension to
  the landed W7 hook (`MILO_TRACE_FREEZE=<hex_va>`: spin at a frame-boundary VA, e.g.
  `TheTaskMgr` poll from the map, until a sentinel file appears) — deterministic
  frame-edge freeze, interpreter core required, savestate compatibility to be checked.
- **Object discovery:** scan MEM1/MEM2 for u32 == `0x80bfeaa8` (`__vt__8CharBone`) at
  plausible object alignment; each hit ± the (DWARF-derived) vtable-slot offset is a
  candidate CharBone; ALSO scan `__vt__13CharServoBone` and the CharClipDriver-family
  vtables (exact set resolved in-lane from the map). Validate every candidate (G2) before
  trusting: name pointer resolves to a printable string, rotation part orthonormal
  (|det−1| < 1e-3), parent pointer is itself a discovered Trans or null, hand-chain names
  form the expected `bone_R-hand → bone_R-*finger01..03` topology.
- **Retail fallback (no map!):** if the debug DOL won't boot the retail filesystem, boot
  the wbfs directly. Retail DOL ≠ Bank-8 → **map addresses are invalid**, so vtable
  anchors are unknown. Discovery then flips to **name-string scan**: find
  `"bone_R-middlefinger03"` bytes in guest RAM → scan for pointers to that address (the
  object name field) → candidate object bases → validate structurally (same G2 checks;
  the shared vtable value across all validated bone candidates then *recovers* the retail
  CharBone vtable address for the rest of the run). Everything downstream is unchanged.
  Note in every report: `build=retail_dol` (OPTIONS §2#1 risk i — still valid for
  authored-bind + inter-bone questions; flag it, don't hide it).

### 3.5 Reaching an animated scene (input)

- **M2 target scene = the hub/title with idling band characters** — reachable with a
  SHORT wiimote input script (press A through boot prompts), vastly cheaper than gameplay.
  The hands question needs *animated multi-bone clips*, which hub idles are.
- **Primary input path:** Dolphin **Pipe input** (compiled in, verified): map the
  emulated Wiimote's buttons to `Pipe/0/...` devices in the scratch user's
  `WiimoteNew.ini`/profile, write button lines to the FIFO from the probe script.
- **Fallback (one-time human assist):** build the Qt GUI from the same fork
  (`-DENABLE_QT=ON`, same Core → savestate-compatible), a human navigates once and saves
  states at hub + in-song; probe boots `--save_state=<file>` headless thereafter
  (`MainNoGUI.cpp:216-230`). Savestates pin the exact Dolphin build — document the hash.
- **Gameplay (M4, for R5's burst frames):** same pipe-input rig with an emulated
  **Guitar extension** on the wiimote, or the savestate route. Song must exist on BOTH
  the Wii disc and the native 360-ARK set (pick one of the on-disc songs the native
  harness already uses).

### 3.6 Native-side mechanics

`rb3bp_dump_bones()` (new TU, `HX_NATIVE`, env-gated `RB3_BONE_PROBE_OUT`) runs on the
main thread via `/api/call`. It walks the loaded band characters (same object-census
approach as `rb3-viewer --list` / `rb3rc_capture_sweep`'s live-state reads), and for each
member emits the §3.3 record: every hand-chain bone's local + world (both direct and
recomputed), the mesh inverse-binds (`BoneTransAt`), and the member's active clip name +
frame. Harness: `keyboard-to-gameplay.py --bin ... RB3_FIXED_CLOCK=1` navigates to the
matching scene and POSTs `/api/call {symbol: rb3bp_dump_bones}` at several frames
(exactly the Wave-16 burst protocol). `rb3-viewer --pose-dump` stays available as a
secondary, asset-isolated comparand but is NOT the primary (no clip playback control —
verified `rb3_viewer.cpp:120-180`; extending it is out of scope for R1).

### 3.7 The diff report

For each joined `(member, clip, frame≈)` key and each **adjacent bone pair** in the hand
chains (`hand→finger01`, `01→02`, `02→03`, both hands; wrist→hand as anchor row):

- `D_side = inv(W_parent) · W_child` per side →
  **`delta_deg = angle(D_wii · inv(D_native))`**, `delta_trans = |t(D_wii) − t(D_native)|`
  (matrix-relative, never angle-to-identity — OPTIONS §4.1).
- Same table for the **authored binds**: `Dbind_wii` vs `Dbind_360` per pair → the
  platform-asset caveat becomes a measured number.
- Per-population rows (per member, per gender, per mesh); aggregates shown but non-gating
  (OPTIONS §4.2). Plus the single-bone world tables as appendix (building block).
- Interpretation contract for R5 (pre-registered): if native inter-bone deltas at matched
  clip time differ from Wii beyond threshold on finger pairs while wrist anchors agree →
  confirms the blend-tear mechanism and grades the reskin-vs-closure decision; if they
  MATCH, the defect is downstream of bone poses (palette/blend), which redirects R5.

---

## 4. MILESTONES (M1 = cheapest decisive risk retirement)

**M1 — boot go/no-go + one named bone matrix (the ROADMAP Wave-17 exit).**
1. `boot-check` A: debug Bank-8 DOL + `DefaultISO=<wbfs>` headless; heartbeat + log for
   disc/ARK acceptance. 2. If A stalls (likely failure mode: debug DOL rejects the retail
   ARK version): `boot-check` B: boot the wbfs directly (retail DOL). 3. On whichever
   boots: pause, scan, dump ONE validated, named CharBone matrix (map-vtable route for A;
   name-scan route for B).
   **GO** = an interactive-scene boot + one sane named bone matrix (either route; record
   which). **NO-GO** = neither boots to an interactive scene within the ROADMAP's ~1-day
   effort box → return a priced report (what failed, what a disc-rebuild with the debug
   DOL inserted à la wit/wbfs-tools would cost) and STOP.
   *Cheapest decisive step: retires the only risks that can kill the whole item (boot +
   discovery) before any tooling investment beyond the scan script.*

**M2 — hub dump, validated.** Scripted input (pipe-input; savestate fallback) to the hub;
full per-member hand-chain dump with clip keys; gates G2/G3 green; N≥5 pauses consistent.
Exit: `wii_bones_hub.json` in evidence/, validation report attached.

**M3 — native dump + joined inter-bone report (R1's deliverable).** Native TU + harness
dumps at the matching scene; join rate reported (a low join rate on clip names is itself
a finding — platform content divergence); the §3.7 delta table for one member's hand
chain, both hands, plus the authored-bind table. Exit: report committed, R5 unblocked.

**M4 (stretch, only if R5 needs animated gameplay poses beyond hub idles):** in-song
capture on a common on-disc song at the Wave-16 burst frames; same report.

---

## 5. GATES (each with its fail-red demonstration)

| Gate | PASS criterion | Fail-red demo (must be shown RED once) |
|---|---|---|
| G1 boot heartbeat | guest heartbeat word advances ≥ N s; CharBone hits > 0 after scene load | run with `DefaultISO` pointed at a nonexistent/corrupt file → heartbeat stalls / 0 hits |
| G2 discovery validity | 100% of dumped bones pass: printable name, orthonormal rotation (|det−1|<1e-3), parent-closure, expected hand-chain topology; count matches expectation (≈38 male / 40 female hand-region bones per Wave-16 census) | scan for a bogus vtable value → 0 hits; deliberately shift the struct-offset table by +4 → orthonormality/name checks reject |
| G3 dump determinism | two dumps within one pause byte-identical; ≥5 pauses give stable inter-bone deltas for a static-ish idle (< noise band, recorded) | dump WITHOUT pausing (running guest) → records differ |
| G4 diff self-test | native-vs-native identical dump → 0.000° every pair; perturbed copy (rotate one bone 0.15 rad, mirroring the shipped `RB3_HANDS_ATTACH_PERTURB` fail-red pattern) → that pair reads ≈8.6°, neighbors ≈0° | the perturbation IS the red demo; both runs in evidence/ |
| G5 oracle-validation lint (OPTIONS §4.3) before R5 consumes it | the joined metric shows separation on a known contrast: native default (rigid "ceiling-hand" conjugation) vs native `RB3_HANDS_AUTHORED_REPOINT=1` (torn blend, Wave-16's measured-bad state) must produce distinguishable inter-bone signatures vs the same Wii ground truth | if the two native arms are indistinguishable through the probe, the metric may not gate R5 — record separation numbers in STATUS either way |

Process lints riding along: evidence committed under `execution/R1-DOLPHIN/evidence/`
(§4.7); per-population rows (§4.2); pointer identity (`addr` field) on every bone record
(§4.1); flag hit-count style proof that the native dump actually ran (record count > 0
returned through `/api/call`, §4.8).

---

## 6. RISKS (honest)

1. **Debug DOL rejects retail-disc assets (ARK version/paths).** Likeliest M1-A failure.
   Mitigation: M1-B retail boot is fully designed (name-scan discovery, no map needed) and
   remains valid for the inter-bone + authored-bind questions; every artifact labels
   `build=`. Residual: retail body ≠ Bank-8 body — behavioral deltas conceivable; for
   *bone pose ground truth* this is acceptable and stated (OPTIONS §2#1 accepted this).
2. **Bank-5 DWARF layouts drift from Bank-8 objects** (known CAUTION class —
   `scripts/analysis/bank_divergence.py` precedent). Mitigation: G2 structural validation
   rejects wrong offsets loudly; cross-check `ptype` offsets against the Bank-8 Ghidra
   program's ported types (`tools/ghidra/port_dwarf_types.py` is already applied there).
3. **Dirty-cache stale `mWorldXfm`** (verified in Trans.h). Mitigation: designed-in
   `world_recomputed` from locals; report both; discrepancy > ε flags the record.
4. **Pause lands mid-skeleton-update (torn snapshot).** Mitigation: G3 multi-pause
   consistency; priced upgrade = `MILO_TRACE_FREEZE` extension to the landed W7 hook
   (fork is ours, commit `bc3b1f5`; ~20 lines) for deterministic frame-edge freeze.
5. **Headless input automation rabbit hole.** Pipe-input for an emulated Wiimote is
   documented Dolphin functionality but fiddly (profile INI + FIFO). Timebox: if not
   navigating within half a lane-day, switch to the Qt-build + human savestate fallback
   (one-time, then fully headless). Gameplay-M4 input (guitar extension) only if R5 asks.
6. **`/proc/<pid>/mem` region identification flakes** (memfd layout differs by version).
   Fallback is RSP `m` reads — already proven live in W2/W4, just slower.
7. **Wii-vs-360 content divergence breaks the join** (clip names/outfits differ, or the
   hub band differs per save-file). Mitigation: dump outfit + clip name censuses from both
   sides FIRST and pick the intersection; a poor intersection is itself a reportable
   finding (it bounds what "ground truth" can mean for this port), not a silent failure.
   Fresh NAND/save on the Dolphin side makes the Wii hub band deterministic.
8. **Plan-invalidating discovery:** if the retail DOL is the only bootable route AND
   name-scan discovery finds no CharBone-shaped objects (e.g. retail strips names —
   considered unlikely: Milo object names are load-bearing for `Find()`), the probe as
   designed dies; the fallback would be the W7-hook route on the debug DOL with
   constructed scenes, which is a re-plan. Priced NO-GO covers this.

---

## 7. COST + what it unblocks

- **Wave A (M1+M2):** one Opus lane. M1 is a ~half-day go/no-go inside it.
- **Wave B (M3, +M4 if asked):** one Opus lane (native TU + join/diff + report).
- Total: **1–2 lane-waves**, matching the ROADMAP row.

**Unblocks:** R5 (hands endgame — the decision is DEFINED as gated on this output;
ROADMAP), the standing rule "no more native-only skinning verdicts", the measured
Wii-vs-360 authored-bind answer, and (later, separate item) true pixel goldens via
scripted Dolphin scenes (OPTIONS §3b revisit).
