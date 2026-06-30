# Full RB3 Xbox 360 disc extract — festival jumbotron bik hunt

**Date:** 2026-06-30 · **Agent:** disc-extract (Opus) · **Goal:** pull the loose festival
jumbotron Bink movies (`fest1_mass*.bik`, `fest2_mass*.bik`) from the full RB3 Xbox 360 disc
to unblock the festival crowd-backdrop render (Bink Option B).

## VERDICT: FAILED — the festival biks are NOT on the Xbox 360 disc, in any form

The full retail RB3 Xbox 360 disc was successfully reconstructed and read end-to-end. The
prior research's premise — that the jumbotron biks "ship loose on the disc filesystem,
OUTSIDE `main_xbox.ark`" — is **REFUTED**. The 360 disc filesystem contains **no loose game
assets at all**; it is entirely the ARK (+ a tiny title-patch ARK + the XEX). The festival
crowd jumbotron biks are:

- **NOT loose** on the disc XDVDFS filesystem (there is no `world/` tree on the disc),
- **NOT in `main_xbox.ark`** (the disc's `main_xbox.hdr` is **byte-identical** — same MD5 —
  to the already-extracted `orig-assets/extracted-xbox-full`; that full extraction is 7861
  files with **only 6 biks total**: intro, end-credits, 4 guitar tutorials),
- **NOT in the title patch ARK** (`patch_xbox_0.ark`, 51 files, zero biks),
- **NOT embedded inside the festival milos** (decompressed + scanned — see §4).

The two festival milos reference the biks only as **external `bink_movie_file` paths** to
files that simply do not exist in any accessible RB3 360 retail data. There is no PS3 RB3
disc on the system (the only RB3 360 title id present is `45410914`).

**Implication for the render task:** on retail Xbox 360, the festival jumbotron crowd movie
is effectively a dead/cut reference (or a PS3-only asset). The current **Option-A skip**
(don't paint the unpainted `crowd_mass.tex` quad) is plausibly *faithful* to retail 360
behavior. Option B remains blocked on assets that are not obtainable from the 360 disc.

---

## 1. Tooling chain that worked (GoD/LIVE → ISO → files)

The container is an Xbox 360 Games-on-Demand (GoD, "LIVE"-signed) package:
`/srv/torrents/games/arbys/Rock Band 3.zip` →
`Rock Band 3/45410914/00007000/DCA6198BF33670E0345E` (the LIVE header, magic `LIVE`) +
`DCA6198BF33670E0345E.data/Data0000..Data0034` (35 SVOD data chunks, ~5.9 GB).

### Step 1 — unzip (gotcha: false zip-bomb)
```bash
mkdir -p /home/free/rb3-disc-extract/god && cd $_
UNZIP_DISABLE_ZIPBOMB_DETECTION=TRUE unzip -o "/srv/torrents/games/arbys/Rock Band 3.zip"
```
Info-zip's modern "overlapped components / zip bomb" heuristic **false-positives** on this
archive and aborts mid-way (stopped at `Data0027`). Must set
`UNZIP_DISABLE_ZIPBOMB_DETECTION=TRUE` to get all 35 chunks.

### Step 2 — GoD → raw XDVDFS ISO (`god2iso.py`, written here)
No maintained `god2iso` was needed — I derived + verified the SVOD layout directly and wrote
`/home/free/rb3-disc-extract/god2iso.py`. The layout (**verified byte-exact**):

- The LIVE header @`0x379` holds the SVOD volume descriptor; `DataBlockCount` is a 24-bit
  big-endian field at `+0x19` → here `0x15DFC0` = **1,433,536** data blocks of `0x1000`.
- The concatenated `Data00NN` stream is block-interleaved with SHA-1 hash tables:
  - block size `0x1000`;
  - each **L0 group** = one L0 hash block + up to `0xCC` (204) data blocks;
  - one **L1 master** hash block precedes every `0xCC` L0 groups (incl. the very first).
- Reconstruct = stream the data blocks, **skipping** the L0/L1 hash blocks.

Self-check (exact): 1,433,536 data + 7028 L0 + 35 L1 = 1,440,599 raw blocks × `0x1000` =
**5,900,693,504 bytes = the exact total size of Data0000..0034**. Output ISO =
1,433,536 × `0x1000` = **5,871,763,456 bytes (5.872 GB)**.
```bash
python3 god2iso.py "god/Rock Band 3/45410914/00007000/DCA6198BF33670E0345E" rb3.iso
```
Calibration that nailed the layout: the XDVDFS magic `MICROSOFT*XBOX*MEDIA` sits at raw
offset `0x12000` in the stream but must land at ISO offset `0x10000` (sector 32) — a
`0x2000` (2-block) shift = the leading L1+L0 hash pair. After reconstruction it lands exactly
at `0x10000`. ✓

### Step 3 — read the ISO with xdvdfs
```bash
cargo install xdvdfs-cli          # antangelo/xdvdfs 0.8.3 (installs clean, ~1 min)
xdvdfs info rb3.iso               # Valid: true, Root Entry sector 264
xdvdfs tree rb3.iso               # full filesystem listing -> 17 files
xdvdfs copy-out rb3.iso /gen/<f> <out>   # pull individual ARK pieces
```
`xdvdfs` reads the reconstructed XISO directly. It does **not** read the GoD/SVOD container
itself — the `god2iso.py` step is required first.

ARK extraction used `tools/mackiloha/arkhelper ark2dir <hdr> <outdir>` (1.3.2). Milo block
decompression used a hand-written `milo_decompress.py` (mackiloha `superfreq` 1.3.2 cannot
parse RB3-era milo **version 28** — it only supports 10/24/25).

---

## 2. What the disc filesystem actually contains (full `find` dump)

The entire XDVDFS root — only **17 files, 5.871 GB** (saved as
`/home/free/rb3-disc-extract/iso-tree.txt`):

```
/charnames.zbm (258)            /gen/main_xbox.hdr (511281)
/nxeart (1908736)               /gen/main_xbox_0.ark (15116275)
/default.xex (13971456)         /gen/main_xbox_1.ark (2876760)
/AvatarAwards (880640)          /gen/main_xbox_2.ark (74396817)
/gen/patch_xbox.hdr (36142)     /gen/main_xbox_3.ark (454937021)
/gen/patch_xbox_0.ark (3424256) /gen/main_xbox_4.ark (746045260)
                                /gen/main_xbox_5.ark (613780006)
                                /gen/main_xbox_6.ark (1402332797)
                                /gen/main_xbox_7.ark (26755588)
                                /gen/main_xbox_8.ark (1974186004)
                                /gen/main_xbox_9.ark (540003243)
```

**There is no `world/`, no loose venue/streams tree, no loose videos.** Everything is the
ARK. The `main_xbox.hdr` MD5 (`e2416e457385d1a64073495abf8ae2c6`) equals the repo's
`orig-assets/xbox-zip/gen/main_xbox.hdr` → the disc ARK == the ARK already extracted to
`orig-assets/extracted-xbox-full` (7861 files). So re-extracting the 5.8 GB main ARK was
unnecessary and would yield nothing new (confirmed via the byte-identical header).

---

## 3. Loose / disc-only asset inventory (the "what else is here" answer)

**Non-ARK files on the disc** (the only things *outside* `main_xbox.*`):

| File | Size | Notes |
|---|---|---|
| `default.xex` | 13.97 MB | the title executable (retail 360) |
| `nxeart` | 1.91 MB | NXE dashboard art |
| `AvatarAwards` | 880 KB | avatar-award package |
| `charnames.zbm` | 258 B | |
| `gen/patch_xbox.hdr` + `gen/patch_xbox_0.ark` | 36 KB + 3.42 MB | **title-update patch ARK** — new vs the prior extraction |

**Patch ARK contents** (51 files, saved as `patch-ark-inventory.txt`): UI/config/world DTBs
and a handful of `ui/**/*.milo_xbox` (overshell, track HUD, audition) — a gameplay/UI title
update. **Zero biks, zero venue movie assets.**

**All biks in the full 360 ARK** (authoritative, 6 total — no crowd footage of any kind):
```
videos/rb3_intro_cinematic.bik      ui/trainers/videos/guitartutorial01.bik
videos/rb3_end_credits.bik          ui/trainers/videos/guitartutorial02.bik
                                    ui/trainers/videos/guitartutorial02_ps3.bik
                                    ui/trainers/videos/guitartutorial03.bik
```
Broad `find` across **all** of `~/code/milohax` for `fest*mass*.bik` / `mass_crowd*.bik` /
`*mass*.bik`: **0 hits.** The only crowd-named biks anywhere are the Wii
`world/venue/{arena,big_club,small_club}/streams/crowd_*_intro.bik` 4×4 audio-only dummies
(already known).

---

## 4. The festival milos reference the biks externally — nothing is embedded

`festival_0{1,2}.milo_xbox` are compressed (`0xCDBEDEAF`, zlib block format, version-28 dir).
Decompressed with `milo_decompress.py` (flag scheme: size high-byte `0x01` = stored/raw,
`0x00` = raw-deflate) and scanned:

| Milo | Decompressed | Bink container magic | External `.bik` path refs | `TexMovie`/`Movie` objs |
|---|---|---|---|---|
| `festival_01.milo_xbox` (35 MB) | 34.7 MB | **NONE** | `texture/fest1_mass.bik`, `fest1_mass01..06.bik`; `../textures/mass_crowd1..3.bik` | 11 / 11 |
| `festival_02.milo_xbox` (33 MB) | 31.9 MB | **NONE** | `textures/fest2_mass.bik`, `fest2_mass01..05.bik` | (same shape) |

Bink video is already-compressed, so a milo would store it **uncompressed** (flag `0x01`,
raw) — those raw blocks were fully scanned, and a raw on-disk `grep` for `BIK[ibgh]`/`KB2*`
over both milos returns **0**. The biks are genuinely external references, not embedded
streams. (Note: the actual refs differ slightly from the prior doc — festival_02 references
`fest2_mass.bik` + `01..05` and festival_01 carries the `../textures/mass_crowd1..3.bik`
refs; not that it matters, since none of the targets ship.)

---

## 5. Transcode pipeline (step 4) — N/A, already proven

There is no `fest*_mass.bik` to probe/transcode. The `.bik → VP9 .webm` (`-an`) pipeline was
already proven by prior work (`orig-assets/derived/bink-scratch/intro_5s_sample.webm`); ffmpeg
here decodes Bink fine. Nothing changed — there is simply no source to feed it.

---

## 6. Where that leaves Option B

- The festival crowd jumbotron biks are **not obtainable from the RB3 Xbox 360 disc** (the
  most complete 360 source we have — the full retail GoD). They are not loose, not in the
  main ARK, not in the patch, not embedded.
- To unblock Option B one would need a **PS3 RB3 disc** (the milos suggest a PS3 variant of
  these refs exists — `_ps3` shot variants, `../textures/mass_crowd*.bik`) **or** the assets
  from another source; neither is present on this system.
- Recommend **keeping Option A** (the screenmask skip) and treating the festival jumbotron
  movie as a 360-retail no-op. If a PS3 disc image appears, the same `god2iso`→`xdvdfs`
  chain does **not** apply (PS3 uses a different container) — but the milo path refs and the
  transcode recipe documented here/in `bink-unblock.md` carry over.

## Artifacts kept (small; big intermediates deleted to reclaim ~12 GB)

- `/home/free/rb3-disc-extract/god2iso.py` — the GoD/LIVE → XDVDFS ISO reconstructor (reusable)
- `/home/free/rb3-disc-extract/milo_decompress.py` — milo `0xC?BEDEAF` block decompressor + bik scanner
- `/home/free/rb3-disc-extract/iso-tree.txt` — full disc filesystem listing (17 files)
- `/home/free/rb3-disc-extract/patch-ark-inventory.txt` — 51-file patch ARK listing
- `/home/free/rb3-disc-extract/cargo-tools/bin/xdvdfs` — xdvdfs-cli 0.8.3
- (deleted: the 5.5 GB GoD unzip, the 5.87 GB `rb3.iso`, the decompressed milo blob, the copied ARK pieces)
