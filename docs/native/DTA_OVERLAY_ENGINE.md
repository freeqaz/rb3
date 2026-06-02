# DTA Overlay Engine (native + web)

The RB3 native/web port ships small, git-tracked DTA patches that **shadow**
the extracted game assets without modifying them. This is how the port adds
the `button_meanings` block the shipped Xbox-flavoured `config/joypad.dta`
lacks (without which keyboard/gamepad menu nav does nothing — see below).

Ported from DC3's overlay engine (`dc3-decomp/native/src/platform/File_Native.cpp`
+ `System_Native.cpp`), adapted to RB3's different native file layer.

## How it works

A tracked **`native/dta/`** directory mirrors the archive's relative path
layout. When the engine opens a file for **read**, the overlay is checked
first: if `native/dta/<rel>` exists, the engine reads that copy; otherwise it
falls through to the extracted original. The extracted assets
(`orig-assets/extracted/`, gitignored) are never modified.

```
Engine opens "config/joypad.dta"
  1. native/dta/config/joypad.dta exists?  YES -> read the overlay copy
  2.                                        NO  -> read orig-assets/extracted/config/joypad.dta
```

### Native (desktop) — `native/src/native_file.cpp`

RB3's native `NewFile()` (`os/File.cpp`, under `HX_NATIVE`) routes straight to
`HmxNativeOpenFile(path, mode)` — it bypasses the Wii ArkFile/AsyncFile/
FileIsLocal machinery entirely. So the overlay intercept lives in
`HmxNativeOpenFile`:

- `OverlayDir()` resolves `native/dta/` once (cached). Lookup order:
  `RB3_DTA_OVERLAY` env → `<RB3_DATA>/../../native/dta` → CWD fallbacks
  (`../../native/dta` post-chdir, `native/dta` at repo root). The native boot
  `chdir`s into `RB3_DATA` (default `orig-assets/extracted`), so the overlay
  sits two dirs up.
- `ResolveOverlay(path)` — for a **read** open (Wii mode bit `0x2`) of a clean
  relative path, returns the overlay path if `native/dta/<path>` exists.
  Absolute paths, write/append opens, and any path containing `..` are
  rejected (a `..` path would escape the overlay tree back out to the repo —
  a false-positive "hit" that opens the same bytes anyway).

The DTA `#include` resolver (`DataFile.cpp:64`,
`FileMakePath(FileGetPath(gFile), c)`) turns `#include joypad.dta` (from
`config/band_keep.dta`) into the relative `config/joypad.dta` **before** calling
`NewFile`, so the include arrives at the intercept as exactly the overlay key.
The intercept is therefore a single chokepoint covering both top-level DTA
loads and `#include` opens. An overlay hit logs once to stderr
(`RB3 native: DTA overlay HIT: 'config/joypad.dta' -> ...`).

### Web (browser) — `native/web/server.py`

On web there is no host disk overlay; files reach the engine through MEMFS via
the dev server (`/api/bundle` at boot, `/api/file/<rel>` on demand). The
overlay is applied **server-side**: an `OVERLAY_DIR` (auto-detected as
`native/dta/`, overridable with `--overlay-dir` / `RB3_DTA_OVERLAY`) is
preferred over the extracted asset in both:

- `_serve_bundle()` — the boot DTA bundle (`config/joypad.dta` is a boot-path
  DTA, so the overlay copy lands in MEMFS at boot), and
- `_serve_asset_file()` — on-demand `/api/file` fetches.

Same `..`-rejection guard as native, so the overlay only shadows files that
genuinely live inside `native/dta/`. The bytes that reach the browser's MEMFS
are thus already the overlay copy — the engine's web file path is unchanged.

## Current overlays

| Overlay file | What it does |
|---|---|
| `native/dta/config/joypad.dta` | Full-file shadow of `config/joypad.dta` that **adds the `button_meanings` block**. The shipped Xbox-flavoured joypad.dta has none, so `gButtonMeanings` is NULL and `Joypad`'s `ButtonToAction` returns `kAction_None` for every menu key — menus never advance. The overlay re-adds it (lines 1–63 are byte-identical to the extracted file; the `button_meanings` block at the bottom is the addition). |

## Adding a new overlay

1. Copy the extracted original to the same relative path under `native/dta/`:
   `cp orig-assets/extracted/<rel> native/dta/<rel>` (mkdir parents).
2. Edit the overlay copy.
3. Native picks it up automatically (no rebuild for DTA-only changes — it's
   read at runtime). Web picks it up on the next server start / bundle fetch.
4. To revert, delete the overlay file — the engine falls through to the
   extracted original.

## Properties

- **Original assets untouched.** `orig-assets/extracted/` stays pristine
  (gitignored); the overlay is the single source of truth for the patch.
- **Git-tracked.** `native/dta/` is committed; every change is a normal diff.
- **Full-file shadow**, per-file granularity — each overlay replaces one file
  entirely (no partial-section merge). Keep an overlay in sync with the game
  version it shadows.
- **Native + web parity.** The same `native/dta/` tree drives both: a native
  disk intercept and a web server intercept.
