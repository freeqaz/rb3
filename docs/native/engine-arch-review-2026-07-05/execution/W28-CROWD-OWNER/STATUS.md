# W28-CROWD-OWNER — STATUS

## Headline

**Lever B (re-charter). The FIFTH and — with direct runtime evidence — decisive
narrative on the "missing main_hub crowd walkers": the crowd we have measured
walking-then-freezing since W23 is the SPLASH (sv8 cityscape) crowd, which faithfully
dies when the splash panel unloads. The 8 walker proxies are SHARED chars that rebind
CORRECTLY to the RESIDENT main_hub (sv3 streetslomo) clip set at the transition — there
is NO clip-set binding divergence. The real hub walk (`playerN_f/m` clips) is simply
never triggered natively (streetslomo scene enters with `nTriggers=0`), a
world/vignette scene-trigger gap OUTSIDE this lane's CharDriver/CharClip ownership.**

Lever A's premise is refuted by the ownership dump; no fix code was written. Per the
charter, the committed re-charter ([RECHARTER.md](RECHARTER.md)) naming the real hub
walkers, their native state, and the W29 acceptance target set is FULL lane success.

## STEP-0 discriminators (checkpointed BEFORE any fix — `/tmp/wave28-checkpoints/CROWD-step0.json`)

One boot, `RB3_CROWD_PANEL_DBG=1 CHARDRV_PROBE=crowd CHARDRV_BT=1 RB3_FIXED_CLOCK=1`,
stderr+stdout → one file (`evidence/raw/step0-combined.log.gz`, 3006 lines).

### (i) Torn-down owner — NAMED DIRECTLY (backtrace, not raw-string inference)

`sv8_panel` (the SPLASH cityscape backdrop). The destroyed `CharClipSet` is
`cityscape_clips.milo`, owned by sv8_panel's `WorldDir`. Symbolized
`CHARDRV_REPLACE_BT` (7 identical backtraces, `evidence/kill-backtrace-symbolized.txt`):

```
App::Run → App::RunOneFrame → BandUI::Poll → UIManager::Poll → BandScreen::Enter
 → UIScreen::Enter → UIScreen::UnloadPanels(splash_screen)
  → UIPanel::CheckUnload → UIPanel::Unload → WorldDir::~WorldDir
   → (mSubDirs vector clear) → nested WorldDir::~WorldDir → PanelDir::~PanelDir
    → RndDir::~RndDir → ObjectDir::~ObjectDir → CharClipSet::~CharClipSet
     → ObjectDir::DeleteObjects → CharClip::~CharClip(crowd4.clp)
      → Hmx::Object::~Object → CharDriver::Replace(clip, NULL)
```

Interleaved PANELDBG (raw log lines 618-620, immediately preceding the 7 REPLACE kills
at 621-933): `UnloadPanels screen=splash_screen beat=2.433` → `splash_panel refs->0
UNLOAD beat=2.433` → `sv8_panel refs->0 UNLOAD beat=2.433`. **This CONFIRMS close-out
E2 (sv8 owner) with a DIRECT backtrace** — the first boot in this campaign to actually
set `CHARDRV_BT` (all W27 logs have 0 `REPLACE_BT` lines). The kill is the FAITHFUL
splash-panel unload.

### (ii) Ownership chains + mClips swap — the decisive evidence

The 8 walkers are **shared chars** `char/crowd/crowd_{male,female}0N.milo` (`main.drv`),
reused across vignettes. `CHARDRV_CLIPSWAP` (unsampled, PathName chains):

| beat | bound clip set (PathName) | pointer |
|---|---|---|
| 0.000 | `clips (world/vignette/shell/sv8/a/cityscape/cityscape_clips.milo)` | 0x…66d0080 |
| 2.433 | `clips (world/vignette/shell/sv3/a/streetslomo/streetslomo_clips.milo)` | 0x…bf3d280 |

All 8 drivers bind the sv8 **cityscape** set during splash, then rebind to the sv3
**streetslomo** set at beat 2.433. **The rebind is to the RESIDENT main_hub set — it is
CORRECT.** There is no wrong-binding divergence at any of A3's layers (mClips load
resolution, SetClips swap, trig object-ref). **E3 confirmed:** `crowd_female04` gets
`CHARDRV_ENTER` but never `CHARDRV_PLAY` (7 PLAY / 5 distinct clips crowd1-5 across 7
drivers; only 5 cityscape clips exist, so the 8th proxy is never assigned one).

### (iii) E5 DEFCLIP — mDefaultClip==NULL is FAITHFUL DATA

`CHARDRV_DEFCLIP` × 8: **`serialized=''` resolved=(nil)** on every crowd driver. The
data authors NO default clip name → NULL is Wii-identical → the W27 "mDefaultClip
resolution" lever is DEAD (no resolution fix exists). Captured via a manual
read+resolve replicating `ObjPtr::Load` (ObjPtr_p.h:536-543).

### (iv) Wii-GT identity — the measured crowd is the SPLASH crowd

- `main_hub.dta:744` → `(panels meta sv3_panel main_hub_panel …)`: main_hub backdrop is
  **sv3 (streetslomo)**. splash uses `sv8_panel`+`splash_panel` (W27, splash.dta).
- Raw strings: `sv8_a.milo_xbox` has `crowd1-5` clips; `sv3_a.milo_xbox` has `player0-3`
  clips. Runtime: `streetslomo_clips` DOES load (NOTIFY `player0_f`..`player3_m`), but
  `PanelDir::Enter dir=streetslomo_ao` / `dir=sv3_a` fire with **`nTriggers=0`** and
  there are **zero `CHARDRV_PLAY` after beat 2.433** — the streetslomo walk is never
  triggered. Vignette variant = **sv3_a** (fresh save, deterministic per vignettes.dta
  TRUE branch), confirmed by the runtime `sv3/a/streetslomo/` path.

**Conclusion:** the crowd measured walking (crowd1-5) then freezing at 2.433 is the
SPLASH/cityscape (sv8) crowd, faithfully dying with the splash. The MAIN_HUB walkers are
the SAME 8 proxies rebound to streetslomo `playerN_f/m` clips, never triggered.

## Lever verdict: B (re-charter)

Lever A ("fix the driver's clip-set BINDING so the resident streetslomo drivers
resolve/hold the RESIDENT clips copy") does not apply: the drivers already rebind to the
resident streetslomo set correctly. mDefaultClip==NULL is faithful. The gap is the
missing streetslomo walk trigger (world/vignette/eventanm layer), not a CharDriver clip
bind. Forcing a CharDriver fix would be a hack against a correctly-bound driver. See
[RECHARTER.md](RECHARTER.md) for the W29 charter (real walkers, native state, acceptance
target set, folded verts=0/near-black thread).

## Probe-count table (A7) — `evidence/probe-count-table.txt`

| tag | count | tag | count |
|---|---|---|---|
| CHARDRV_ENTER | 16 | CHARDRV_CLIPSWAP | 32 |
| CHARDRV_CLEAR | 0 | CHARDRV_DEFCLIP | 8 |
| CHARDRV_PLAY | 7 | PANELDBG CheckLoad | 37 |
| CHARDRV_DIE | 7 | PANELDBG CheckUnload | 5 |
| CHARDRV_LIFE | 112 | PANELDBG UNLOAD | 3 |
| CHARDRV_REPLACE | 7 | PANELDBG LoadPanels | 3 |
| CHARDRV_REPLACE_BT | 7 | PANELDBG UnloadPanels | 2 |
| CHARDRV_POP | 0 | PANELDBG PanelDir::Enter | 2 |
| CHARDRV_STARVE | 8 | | |

## Gates

| Gate | Result |
|---|---|
| STEP-0 checkpoint before fix | **DONE** — `/tmp/wave28-checkpoints/CROWD-step0.json` + `evidence/CROWD-step0-checkpoint.json`; all four discriminators + verdicts; chosen lever=B |
| batch_objdiff (touched decomp fns) | **PASS** — `SetClips`/`CheckUnload`/`UnloadPanels` 100.0% raw+fuzzy; `Poll__10CharDriverFv` 93.54% == report.json baseline 93.54499% EXACTLY (pre-existing `[120] ble↔beq` residual, unrelated). All edits `#ifdef HX_NATIVE` → Wii `.o` byte-identical. |
| drawlog-golden `--fixed-clock --canonical-order` (flag-OFF) | **PASS** — count=792, canonical-order matches golden, 307 known-residual divergences within bound (non-blocking). No carve-out fired (Lever B = no behavioral change); no escalation. |
| rb3-tests | **PASS 116 / 0 fail** (7 skipped fixtures = baseline) |
| prewarm boot (`RB3_PREWARM_SCREENS=1`) | **PASS** — required by A6ii (ui/*.cpp probe-line edits). Boots to main_hub, prewarm adoption fires (song_select panels), no crash/assert. `evidence/raw/prewarm-boot.log.gz` |
| boot A/B flag-ON (lever A only) | **N/A** — Lever B, no flag added |
| Acceptance (Lever B) | **MET** — committed re-charter [RECHARTER.md](RECHARTER.md) names the real hub walkers, their native state, and the W29 acceptance target set with STEP-0-grade evidence |
| Evidence honesty (A7) | **MET** — full raw stderr gzipped (`evidence/raw/step0-combined.log.gz`, sha256 `0a25665a…`); per-log probe-count table; symbolized backtrace committed; excerpts cite raw log line ranges |

## False start (honest)

First DEFCLIP probe used Tell/ReadString/Seek-back around `mDefaultClip.Load`; the milo
chunkstream `FAIL-MSG: Can only seek forward from current position on chunkstream` →
SIGSEGV. Replaced with a manual read+resolve (one `ReadString` then `FindObject`,
consuming the stream identically). Rebuilt, clean boot. Documented so no future lane
repeats the peek+seek-back approach on a chunkstream.

## Flag reserved, NOT used

`RB3_HUB_CROWD_CLIPBIND` (chosen per A5 at the STEP-0 checkpoint) — NOT added; Lever B
writes no fix code. NO default flips, NO pin bump, NO census edits, NO sidecar edits.

## Files changed (staged by path, this lane only)

- `src/system/char/CharDriver.cpp` — CLIPSWAP detector (Poll + SetClips) + DEFCLIP
  serialized-name probe (all `#ifdef HX_NATIVE`, gated under existing `CHARDRV_PROBE`).
- `src/system/ui/UIScreen.cpp` — `UnloadPanels` marker beat stamp (A1, HX_NATIVE).
- `src/system/ui/UIPanel.cpp` — `CheckUnload` UNLOAD line beat stamp (A1, HX_NATIVE).
- `scripts/native/_w28_crowd_step0_boot.py` — STEP-0 boot harness.
- `docs/.../W28-CROWD-OWNER/{PLAN.md, STATUS.md, RECHARTER.md, evidence/*}`.
