# R1-DOLPHIN — Lane D STATUS (Wave 17)

**Milestone:** M1 (boot go/no-go + one named bone matrix). **Verdict:**
**BOOT-GO on route B (retail wbfs); clean named-bone-matrix NO-GO within the ~1-day
M1 box — priced.** No default flips, no pin bumps. Writable surfaces used: milo-trace
(tool) + this lane hub; rb3/engine source untouched.

## What M1 set out to prove
Per `PLAN-R1-dolphin-probe.md` §4: headless Dolphin boots the game and one scripted,
**map-named** memory read returns a sane bone matrix. GO = interactive-scene boot + one
sane named CharBone matrix (either route); NO-GO within the box = priced report + STOP.

## Result (all live-verified 2026-07-07; evidence/ committed)

| Step | Outcome |
|---|---|
| Route A (debug Bank-8 DOL + `DefaultISO=<wbfs>`, `-e main.dol`) | **DECISIVE FAILURE** — DOL image loads but the GAME never inits (MEM2 heap 3/16384 pages, `TheTaskMgr`/`TheBandDirector`=0, no disc ID, 0 CharBone hits, stable at 150s+). Bare-DOL boot skips the disc apploader. Per A4 this is the "first decisive route-A failure" → switched to B. |
| Route B (retail `Rock Band 3 (USA).wbfs` direct) | **BOOT-GO** — banner `…\| HLE \| SZBE69`, disc ID = "SZBE69", MEM2 heap 6423→populated, reaches `main_hub` with 411→1087 interned skeleton bone names. |
| Guest-RAM read channel | **WORKING** — `/proc/<pid>/fd/<memfd>` (deleted `dolphin-emu.<pid>` shm); `/proc/<pid>/mem` is blocked (ptrace_scope=1). MEM1@shm 0, MEM2@shm 0x02040000. |
| Struct offsets (Bank-5 DWARF) | Extracted (`evidence/dwarf_offsets.txt`, `struct_offsets.md`): RndTransformable mLocalXfm@32, mWorldXfm@96, mName@260; CharBone(size 92) mTrans.mPtr@84. |
| Named-object discovery on retail | **PARTIAL** — object graph IS name-discoverable by suffix cluster (name@+24, vtable@0): `.mesh`(RndMesh 0x80876cb0), `.cb`(CharBone 0x8089a57c, ~3542), `.ikf`, `.quat`. |
| One CLEAN posed bone WORLD matrix | **NO-GO within box** — retail layout diverges from BOTH the Bank-8 map (0 vtable hits) AND the Bank-5 DWARF (mName@+24 not +260; no rigid matrix at +96/+32). Empirical offset derivation yields only false positives (row-normalized "rotations" with 68× raw scale; inconsistent offsets). Needs symbol-anchored per-class RE. |

## Why this is decision-relevant (not just a boot hiccup)
R1's premise is a **HIGH-confidence, map-symbol-driven** Wii ground truth feeding the
HARD R5 hands-endgame gate. The only bootable route is **retail** (route B), which has
**no valid symbol map** and a heap layout that does not match our Bank-5 DWARF offsets.
So a route-B ground truth would rest on **symbol-free per-class reverse engineering**
(lower layout confidence) of a **retail build** (body differs from the Bank-8/decomp the
native port matches — accepted for poses per PLAN §6.1, but compounding with the layout
uncertainty). R5/coordinator should weigh this confidence tradeoff **before** Wave-B
effort is sunk. This is exactly the M1 decision point the plan built in.

## Priced options forward (for coordinator / R5 to choose)
1. **Restore the clean map path (recommended if R1 must be high-confidence).** Boot the
   **debug Bank-8 DOL under a real apploader** by building a patched disc (Riivolution
   `main.dol` replacement, or a rebuilt wbfs via wit/wbfs-tools with the debug DOL). Then
   the Bank-8 map + Bank-5 DWARF offsets are all valid and the original §3.4 map-vtable
   discovery works as designed. **Risk:** the debug DOL may reject the retail ARK
   version (PLAN §6.1, untested). **Est.:** ~1–2 lane-days, uncertain outcome.
2. **Full symbol-free retail RE.** Anchor a Ghidra program on the retail SZBE69 DOL (or
   port Bank-8 types by structural signature), derive the retail RndTransformable /
   RndMesh / CharBone layouts, then discover posed bones on the found clusters
   (`.cb`→mTrans→Trans→world) and reach an animated scene via Dolphin pipe-input. Ground
   truth = retail build, symbol-free layout (lower confidence). **Est.:** ~2–3 lane-days.
3. **Priced NO-GO / defer.** Accept that pose-level Wii ground truth is expensive here;
   R5 proceeds on the native-only + authored-bind evidence it already has, treating the
   "no more native-only skinning verdicts" rule as aspirational until (1) or (2) funds it.

## What landed (verified, reusable — the R5 handoff substrate)
- `milo-trace tools/wii_bone_probe.py` — boot orchestration (route A/B, setsid pgid),
  the `/proc/fd` guest-RAM bulk reader (`GuestMem`), global/vtable/name/structural scan,
  pgid-clean teardown. `boot-check`/`scan`/`kill` subcommands reproduce every finding above.
- `evidence/` — `M1_boot_findings.md` (full narrative + numbers), `dwarf_offsets.txt`
  (gdb ptype transcript), `struct_offsets.md` (constants), `scan_globals.py`.

## Reproduction
```bash
cd /home/free/code/milohax/milo-trace
tools/wii_bone_probe.py boot-check --route A   # observe: no game init (route-A failure)
tools/wii_bone_probe.py boot-check --route B   # observe: BOOT-GO, SZBE69, bone names, clusters
tools/wii_bone_probe.py scan                   # re-scan a running dolphin
tools/wii_bone_probe.py kill                   # pgid-clean teardown
```
Guest RAM is read from `/proc/<pid>/fd/<memfd>`; no root, no ptrace. DWARF offsets:
`gdb -batch -ex 'ptype /o RndTransformable' "<Bank-5 band_r_wii.elf>"`.

## Process-lint compliance
Pointer identity recorded on every claim (object base VAs, vtable VAs quoted).
Instruments demonstrated on known-good AND known-bad: the boot heartbeat separates
route-A-dead (heap empty) from route-B-live (heap populating); the orthonormal check was
shown to FALSE-POSITIVE on scaled/degenerate data (why the matrix read is NO-GO, not a
silent pass). Evidence committed under `execution/R1-DOLPHIN/evidence/`. No unvalidated
oracle promoted — the delta table is explicitly NOT produced because no clean matrix was
validated (refusing to emit an unvalidated ground truth is the disciplined outcome).
