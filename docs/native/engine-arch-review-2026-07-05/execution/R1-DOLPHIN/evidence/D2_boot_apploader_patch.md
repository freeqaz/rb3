# R1-DOLPHIN M1 — D2 / Option 1: patched-disc apploader boot → M1 GO (2026-07-07)

Lane-D side-lane D2 executed STATUS.md **Option 1** ("restore the clean map path"):
boot the **Bank-8 debug DOL under a real apploader** by patching the retail disc,
keeping retail assets. Result: **M1 = GO** — interactive boot of OUR Bank-8 image
(map valid by construction) + a clean, map-named, pointer-verified, rigid CharBone
world-matrix chain for both hands. The prior Lane-D NO-GO is **superseded** by this
route; the earlier route-A/route-B findings stand as the record of why it was needed.

## Why Lane D's routes failed, and what D2 changed
- **Route A** (`-e main.dol` + `DefaultISO`) boots the DOL image but SKIPS the disc
  apploader → no IOS/BI2/disc env → game never inits. (Lane-D, decisive.)
- **Route B** (retail wbfs direct) boots, but the retail DOL ≠ Bank-8 → the Bank-8
  symbol map is invalid (0 CharBone-vtable hits) → no map-named read. (Lane-D.)
- **D2 / Option 1**: put the Bank-8 DOL *inside* the disc so the **retail apploader**
  loads it with the full environment → the Bank-8 map is valid by construction.

## The real blocker (more specific than PLAN §6.1's "ARK reject")
Swapping only `sys/main.dol` FAILED with a decisive apploader gate, NOT an ARK issue:

```
APPLOADER ERROR >>> [production mode] One of the sections in the dol file exceeded
its boundary. All the sections should not exceed 0x80900000 (production mode)
```

The Bank-8 **debug** DOL links high — sections run to `0x80c7c4c0`, BSS to
`0x80dcdf60`; its map symbols live there (`__vt__8CharBone` 0x80bfeaa8, `TheTaskMgr`
0x80cacb98, `TheBandDirector` 0x80d16c9c). The retail Dec-2009 **production**
apploader hard-rejects any DOL section past **0x80900000** (a debug DOL is meant for
an NDEV apploader). No debug apploader exists in-repo. **This is exactly why route A
and bare-DOL boots never init — the same memory-bound gate, silently.**

### Fix: force the apploader's development-mode branch (1 instruction)
The apploader chooses production vs development at guest `0x81200e20`:
```
0x81200e14  lis  r3, 0x8000
0x81200e18  lwz  r0, 0x2c(r3)      ; r0 = *(0x8000002C) = OS console-type global
0x81200e1c  rlwinm. r0, r0, 0,0,3  ; r0 & 0xF0000000 (hardware-class nibble)
0x81200e20  bne  0x81200e54        ; nonzero → DEVELOPMENT; zero (retail) → PRODUCTION
```
Dolphin emulates a retail console (top nibble 0) → production → 0x80900000 → reject.
The **development** branch uses limit **0x81200000** (just below the apploader load
addr); our DOL's BSS ends at `0x80dcdf60` < `0x81200000`, so dev mode PASSES (only a
non-fatal WARNING prints). Patch: at apploader.img image offset **0xe40** change the
conditional `bne` `40820034` → unconditional `b` `48000034`.

## The full patch chain (all files provenance-hashed; NONE committed — too large)
Working dir: `/home/free/tmp/wave17-d2/disc` (wit-extracted DirectoryBlob, out of repo).

| Step | Action | Hash / note |
|---|---|---|
| 1 | `wit extract "Rock Band 3 (USA).wbfs" disc` | disc ID SZBE69; Dolphin DirectoryBlob boots `disc/sys/main.dol` (auto-Wii via boot.bin magic 0x5D1C9EA3), regenerates FST+hashes → no re-signing |
| 2 | swap `sys/main.dol` ← Bank-8 DOL | new sha1 `e26b3daf41886f0d09670135910f2510cd093ae8` (retail 9.3MB backed up `main.dol.retail`) |
| 3 | ADD `files/band_r_wii.sel` ← Bank-8 SEL | sha1 `a813c9ab39bada6dd24a3123de21febef7a5f460` (retail ships `band_s_wii.sel`; debug DOL loads `_r` by name) |
| 4 | patch `sys/apploader.img` off 0xe40 `40820034`→`48000034` | patched sha1 `4983b1b8738563b196a8446965106b8727a81727` (retail bak `apploader.img.retail` `7eb6a497...`) |
| 5 | `cp files/{cntsdrso,hmbrso,keyboardrso}_s.rso → *_r.rso` | debug DOL requests `_r` overlay RSOs (release-with-symbols) not on the retail disc; the same-source `_s` shipping RSOs relocate cleanly against the debug DOL by name |

Companion-file naming (`_r` release-with-symbols vs `_s` shipping) is the second-order
issue after the apploader gate: the debug DOL references `band_r_wii.sel`,
`cntsdrso_r.rso`, `hmbrso_r.rso`, `keyboardrso_r.rso` (confirmed via `strings main.dol`).

## Boot-GO evidence (`D2_boot_log_excerpt.txt`, full: /tmp/d2-boot-verbose.log)
- Apploader prints the section-limit as a **WARNING** (non-fatal), not ERROR.
- All DOL sections load, INCLUDING `memOffset: 80b34820 / 80b4a1e0 / 80c788e0` (above
  the old 0x80900000 limit — proves the patch took).
- Game runs its OWN Bank-8 code on RETAIL assets: `Reading the archive`,
  `SystemInit Params: band_r_wii.elf`, `Build: 100924_C  Plat: wii`, full RVL_SDK
  subsystem bring-up (GX/WPAD/AX/mic), content-store (`RVL_SDK - CNTSD`), and the
  `ui/overshell` UI shell. **The §6.1 ARK-version risk did NOT materialize** — Bank-8
  reads the retail USA `main_wii` ARK set fine.
- Live Bank-8 anchors (map VALID by construction): `TheTaskMgr@0x80cacb98=0x80bb9f98`,
  `TheBandDirector@0x80d16c9c=0x92c2495c`, **992 `__vt__8CharBone` hits at the map VA**
  (was 0 on retail route B).

## M1 GO datum — map-named bone matrices (`D2_wii_bones.json`, table `D2_interbone_table.md`)
Bank-8 layout was **derived empirically on the live image** (G2 — Bank-5 DWARF does
NOT apply; it diverges): `CharBone` (stride 0x54): `mName` char* @ **+12**, `mTrans`
holder ptr @ **+72**; transform holder packs two 48-byte **packed** Transforms
(Matrix3 3×3 at **12-byte** row stride — NOT Bank-5's 16-byte-padded — + Vec3): local
rot@+28/t@+64, **world rot@+76/t@+112**. Validation that +76 is world: `inv(W_parent)·
W_child` gives bone-length-scale relative translations (forearm→hand 8.95, hand→finger
6.31, finger segments 2.19→1.14, decreasing toward the tip).

- **989 / 992** CharBones parse to **rigid** world matrices (det=+1.000, unit rows,
  orthogonal; 3 rejects are unnamed template objects).
- Both hands' inter-bone tables are **bilaterally symmetric** (identical relative
  rotations L/R, mirrored translations) — independent proof these are real posed
  matrices, not offset false-positives (the failure mode Lane-D's retail route hit).
- Every pair row carries parent/child object VAs (lint 1: matrix-relative +
  pointer-verified). Table format = plan §3.7 (`D=inv(W_parent)·W_child`).

**This is the Wii ground-truth half of the R5 handoff artifact.** The Wii-vs-native
join is Wave-B (native `/api/call` dump + matched-clock harness, per PLAN §3.6) — now
UNBLOCKED: the high-confidence, map-symbol-driven Wii truth R1's premise required
exists.

## Reproduction
```bash
# 1. patch the disc (out-of-repo working dir)
mkdir -p /home/free/tmp/wave17-d2 && cd /home/free/tmp/wave17-d2
wit extract "/home/free/code/milohax/rb3/Rock Band 3 (USA).wbfs" disc
cp -n disc/sys/main.dol disc/sys/main.dol.retail
cp /home/free/code/milohax/rb3/orig/SZBE69_B8/sys/main.dol         disc/sys/main.dol
cp /home/free/code/milohax/rb3/orig/SZBE69_B8/files/band_r_wii.sel disc/files/band_r_wii.sel
cp -n disc/sys/apploader.img disc/sys/apploader.img.retail
python3 - <<'PY'   # apploader dev-mode patch
a=bytearray(open('disc/sys/apploader.img','rb').read()); assert a[0xe40:0xe44].hex()=='40820034'
a[0xe40:0xe44]=bytes.fromhex('48000034'); open('disc/sys/apploader.img','wb').write(a)
PY
for b in cntsdrso hmbrso keyboardrso; do cp -n disc/files/${b}_s.rso disc/files/${b}_r.rso; done
# 2. boot + read (milo-trace)
cd /home/free/code/milohax/milo-trace
tools/wii_bone_dirboot.py boot-check   # gate: disc SZBE69 + heap + Bank-8 singletons + CharBone hits
tools/wii_bone_dirboot.py bones --out /tmp/wii_bones.json   # M1 GO: inter-bone tables both hands
tools/wii_bone_dirboot.py kill         # pgid-clean teardown (this D2 instance only)
```
Guest RAM read via `/proc/<pid>/fd/<memfd>` (ptrace_scope=1 blocks `/proc/<pid>/mem`).
Instance-isolated: only touches the Dolphin whose `-u` matches `/tmp/dolphin-d2`.
