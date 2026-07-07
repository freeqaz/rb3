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

---

## D2 / Option 1 (2026-07-07) — patched-disc apploader boot → **M1 flips to GO**

Side-lane D2 executed **Option 1** above ("restore the clean map path"). **Result:
M1 = GO.** The above route-A/B NO-GO stands as the record of *why* Option 1 was
needed; D2 supersedes it with a bootable Bank-8 image whose map is valid by
construction. Full detail + reproduction: `evidence/D2_boot_apploader_patch.md`;
deliverable `evidence/D2_wii_bones.json` + `evidence/D2_interbone_table.md`; boot
proof `evidence/D2_boot_log_excerpt.txt`. Tool: `milo-trace tools/wii_bone_dirboot.py`.

**What D2 did.** wit-extracted the retail disc to a Dolphin DirectoryBlob, swapped in
the Bank-8 `main.dol` + `band_r_wii.sel`, kept all retail assets, and booted the
*directory* so the **retail apploader** loads our Bank-8 DOL with a full IOS/BI2/disc
environment (the thing route A lacked). Two blockers found and cleared:

1. **Production apploader memory-bound gate (the real blocker, sharper than §6.1's
   "ARK reject").** Retail Dec-2009 production apploader hard-rejects any DOL section
   past `0x80900000`; the Bank-8 *debug* DOL links to `0x80dcdf60` (its map symbols
   live in that debug region — that is precisely why route A/bare-DOL never init).
   Fixed by a **1-instruction apploader patch** forcing its development-mode branch
   (`bne`→`b` at image off 0xe40; dev limit 0x81200000 > our DOL end). Non-fatal
   WARNING now; DOL loads fully incl. sections >0x80900000 (log-proven).
2. **`_r` (release-w/symbols) vs `_s` (shipping) companion files.** Debug DOL requests
   `band_r_wii.sel` + `{cntsdrso,hmbrso,keyboardrso}_r.rso`; retail ships `_s`. Added
   the SEL; copied the same-source `_s` RSOs to `_r` names — they relocate cleanly.

**The §6.1 ARK-version risk did NOT materialize** — Bank-8 (Build 100924_C) reads the
retail USA `main_wii` ARK set and boots to the `ui/overshell` shell with 992 live
`__vt__8CharBone` instances (was 0 on retail), `TheTaskMgr`/`TheBandDirector` live.

**M1 GO datum.** Bank-8 struct offsets **re-derived empirically on the live image**
(G2 — Bank-5 DWARF diverges: name @+12 not +24, packed 12-byte Matrix3 rows not
16-byte). Per bone: `CharBone`(name+12) → `mTrans`(+72) → world Transform(+76,packed).
**989/992 CharBones rigid** (det=+1.000). Emitted the per-pair inter-bone table
`D=inv(W_parent)·W_child` for **both hands** (forearm→hand anchor per M-2, + middle/
ring/thumb cascades), pointer-verified. **Bilaterally symmetric** (identical L/R
relRot°, mirrored translations) — independent proof of real posed matrices, defeating
exactly the scaled-false-positive failure the retail route hit. This is the **Wii
ground-truth half of the R5 artifact**; the Wii-vs-native join is Wave-B (native
`/api/call` dump + matched clock, PLAN §3.6), now **unblocked**.

**Not committed:** the patched disc / any large binary (per rules) — only scripts,
exact patch instructions + provenance hashes, and evidence text/JSON. Disc left in
`/home/free/tmp/wave17-d2/` (out of repo). Process lints held: pointer identity on
every bone row; instrument shown red (production-mode reject) AND green (dev-mode
boot); rigidity+topology validated before the table was emitted (no unvalidated
oracle); other lanes' Dolphin untouched (instance-scoped by `-u`); pgid-only teardown.

---

## D3 / Wave-B (2026-07-07) — native-side probe + Wii-vs-native inter-bone JOIN

Lane D3 built the **native half** of the inter-bone delta table and joined it
against the D2 Wii ground truth. **Verdict: TABLE DELIVERED + validated machinery,
with a priced matched-clock caveat.** Full detail + reproduction:
`evidence/D3_findings.md`; deliverable `evidence/D3_delta_table.{md,json}` (primary,
shell/D2-matched) + `D3_delta_table_gameplay.{md,json}` (director band); raw dumps
`evidence/D3_native_bones_{shell,gameplay}.json`.

**What D3 did.** Added `native/src/rb3_bonedump_native.cpp` — env-gated
(`RB3_BONE_PROBE_OUT`, default-OFF) native bone probe, `extern "C"
rb3bp_dump_bones()` reached over the existing `/api/call` endpoint
(RB3_REPLAY_API=1, static-vaddr+bias resolve like `rb3rc_capture_sweep`). It walks
each band member's **OWN** BandCharacter dir and dumps every hand-chain
`RndTransformable` bone's world/local matrix + parent + **pointer identity**. The
harness `scripts/native/d3-bonedump-capture.py` boots headless (RB3_FIXED_CLOCK=1),
reaches the shell (main_hub) and gameplay, POSTs the dump. The join
`scripts/analysis/interbone_diff.py` computes `D=inv(W_parent)·W_child` per pair per
side and `delta=angle(D_wii·inv(D_native))` for D2's full M-2 pair list (forearm→hand
anchor + middle/ring/thumb cascades, BOTH hands), all 4 members.

**Validity gates (all GREEN):** (1) convention **self-calibrated** to reproduce D2's
20 stored pairs to worst **0.133°** (layout=col, order=inv(P)·C) → native measured in
D2's own convention; (2) **red-team** known-bad pair reads **168.8°** (machinery
separates); (3) **anti-magnet pointer-verified** — 0/54 bones shared across members
(dumped OWN animating bones, avoided the campaign own/bound trap); (4) gender/member
split (lint 2); (5) evidence committed (lint 7).

**Headline (shell, D2-matched):** anchor delta mean 90.6°, finger delta mean 29.7°.
The **anchor + thumb chain agree in relRot magnitude to ~1°** (thumb01→02 Δmag 0.02°,
thumb02→03 0.03°) while **middle/ring diverge 20–45°** (native fingers more curled).

**The priced caveat (why the numbers don't yet close R5):** D2's static shell artifact
carries **no clip/frame join key**, so a frame-exact "matched clock" join (PLAN §3.6)
is impossible from it — both sides are in un-synced idle poses. The "equal magnitude,
large delta" signature (anchor/thumb) is a per-bone local-frame **conjugation**
(`inv(L)·D·L` preserves angle, rotates axis) / unmatched-frame artifact, NOT a scale
defect; the middle/ring divergence is the one pose-level signal but can't be separated
from a frame difference without a shared label. **Exact unblock:** emit the member's
CharClipDriver clip+frame on BOTH sides (native probe + D2's `wii_bone_dirboot.py`),
drive to the same `(clip,frame)`, re-run the join — then the residual is the real
skeleton delta. Details in `evidence/D3_findings.md`.

**Not committed:** no default flips, no pin bumps, no engine edits (rb3-side probe TU
only). Process lints held: pointer identity on every native bone row; metric shown red
on a known-bad pair AND calibrated green on D2; own-vs-magnet proven by pointer
distinctness before the table was trusted (no unvalidated oracle promoted).
