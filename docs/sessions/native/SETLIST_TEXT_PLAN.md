# SETLIST_TEXT_PLAN — cosmetic text sweep (raw/placeholder/untranslated UI strings)

**Authored:** 2026-05-29 (planning subagent, Opus, READ-ONLY — no build, no code
edit, no commit). One doc only.
**Scope:** enumerate every raw / placeholder / untranslated UI string that leaks
to screen on the rb3-native LP64 port, root-cause the substitution mechanism, and
produce a per-string fix plan + an explicit files-to-edit list for the
dependency-graph / dispatch step. This is status item **N6** plus its siblings.
**Sources read:** `STATUS_AND_NEXT_GOALS.md` (N6), `TEXT_TOKENS.md` (V28 prior
locale work), `ViewSetting.cpp`, `UILabel.cpp`, `SelectDifficultyPanel.cpp`,
`MetaPerformer.cpp/.h`, `AppLabel.cpp`, `BandTrack.cpp`, the seldiff `.dta` +
`.milo_xbox`, the `eng/locale_keep.dta` setlist tokens, `band3_link_stubs.s`,
`rb3_game_input.cpp`, and screenshots `v34-status-review/`, `v37-songselect/`,
`v28-text-tokens/`.

---

## 0. TL;DR

- **One genuinely-broken on-screen string remains: `%S %I SONGS`** on the
  seldiff (SelectDifficultyPanel) `setlist_title.lbl`, frame `07_f0360`. Confirmed
  still present in the latest sweep (`v37-songselect/07_f0360.png`). This is
  TEXT_TOKENS **Token-3 (OPEN)**.
- **Root cause (now fully pinned):** the seldiff milo bakes a default
  `text_token = set_list_named_title` on the **top-level** `setlist_title.lbl`.
  Its locale value is `"%s <alt>%i songs</alt>"`. On milo load `UILabel::PostLoad`
  → `SetTextToken("set_list_named_title")` fires with **no format args**, so
  `SuperFormatString` leaves the `%s`/`%i` literal; `kForceUpper` uppercases →
  `%S` / `%I SONGS`. This raw default persists because **no enter-time handler
  ever overwrites this label in the native quickplay path** — and the existing
  partial fix (hiding `setlist_label.grp`) does not help because the label is a
  top-level object, not a child of that group.
- All other historically-raw strings are already fixed and verified absent in the
  current screenshots: `SHELL_PRESS_START_TO_ROCK` (V28 Token-1, Locale.cpp
  HX_PC scope), the filter-view `%S %I SONGS` (V28 Token-2, ViewSetting.cpp node
  swap), and the gameplay `%d%%` solo-percent (V29, BandTrack.cpp seed).
- The "MENU/MENT" garbled overshell header (`02_f0025`, `03_f0050`) is **NOT** a
  text-token leak — the underlying strings ("CONNECT CONTROLLER", "CHOOSE
  INSTRUMENT", "YOU MUST CHOOSE …") are correctly localized; it is a label
  layout/z-overlap defect (N3-class), explicitly out of scope here.
- **Net new work: one string, effort S.** Best fix is asset-layer (one line in the
  runtime-loaded `seldiff.dta` `enter` event) with an optional glue-layer
  belt-and-suspenders that matches the existing N4 pattern in `rb3_game_input.cpp`.

---

## 1. Table of every broken / suspect string

| # | String on screen | Screen / frame | Expected | Status | Root cause |
|---|---|---|---|---|---|
| T3 | `%S %I SONGS` (large, two-line `%S` over `%I SONGS`) | seldiff / SelectDifficultyPanel — `v34/07_f0360`, `v37/07_f0360`; fixed in `v40/07_f0360` | song title + artist (the marquee song-preview) | **FIXED (V40, glue) — see "Implemented" below** | Leak is on the marquee `song_preview.lbl` (NOT `setlist_title.lbl` as first thought) — it bakes default `text_token=set_list_named_title`; the `update_preview_song`→`set_song_and_artist_name_from_sym` handler is an `AppLabel` method but the object loads as base `BandLabel`, so the baked `%s`/`%i` default is never overwritten. Fixed by glue in `rb3_game_input.cpp` filling the song+artist via base `UILabel::SetDisplayText`. |
| T1 | `SHELL_PRESS_START_TO_ROCK` (sideways, left overshell slot) | overshell / preloading | "PRESS START" | **FIXED (V28)** — verified absent in `v34/f3400` (correctly-localized) | Token only in HX_PC/PS3/XBOX locale blocks, no HX_WII variant; native reads with HX_WII only → locale miss → raw symbol. Fixed by scoped `DataSetMacro("HX_PC", …)` around the DTA read in `Locale::Init()`. |
| T2 | `%S %I SONGS` (filter-view song-count) | song-select filter view | "N SONGS" | **FIXED (V28)** | `FilterViewSetting::Text()` passed the count string at `Node(0)`; `ForceSym(0)` interned "83" as a symbol → locale miss. Fixed by swapping so the locale symbol is `Node(0)`. |
| P1 | `%d%%` (solo percent) | gameplay HUD | "0%" | **FIXED (V29)** | `solo_percent.lbl` bakes `text_token=solo_percent_fmt` ("%d%%"); raw at load. Seeded "0%" at load in `BandTrack.cpp`. |
| L1 | "MENU/MENT" overlapping header | overshell — `v34/02_f0025`, `03_f0050` | "MENU" + "CHOOSE INSTRUMENT" non-overlapping | **NOT a text-token bug — OUT OF SCOPE** | Both strings are correctly localized; the defect is label placement/z-overlap (N3-class layout). Not a substitution/locale problem. |

**Conclusion of the sweep: exactly one new text-token fix is needed (T3).** No
other raw/placeholder/locale-key string was found leaking in any reviewed screen
(hub, main menu, music library, seldiff besides T3, venue, gameplay highway, HUD).

---

## 2. Root-cause detail for T3 (the substitution mechanism)

### The `%S`/`%I` specifiers and the substitution routine

`%S`/`%I` are RB3's **uppercased** rendering of the standard `%s`/`%i` printf
specifiers — they are NOT custom format specifiers. The flow:

1. Locale value for `set_list_named_title` = `"%s <alt>%i songs</alt>"`
   (`orig-assets/extracted/ui/locale/eng/locale_keep.dta:22535`).
2. The substitution engine is `SuperFormatString` (`utl/SuperFormatString.h/.cpp`),
   invoked from `UILabel::SetTokenFmtImp` (`src/system/ui/UILabel.cpp:791`). It
   consumes `%s`/`%i` from `da1`/`da2` arguments and expands `<alt>…</alt>` markup
   (the alt-style run that draws the "songs" suffix smaller).
3. With format ARGS present (the `set_token_fmt … $setlist $song_count` handler
   path) the `%s`/`%i` get substituted → "SETLIST (2 SONGS)".
4. With **no args** (the baked milo-default path, `UILabel::PostLoad:213` →
   `SetTextToken(mTextToken)` → `SetTokenFmtImp(sym, 0, 0, 0, true)`), there is
   nothing to substitute → the literal `%s`/`%i` survive → `RndText` `kForceUpper`
   (`src/system/rndobj/Text.cpp:1029`) uppercases to `%S`/`%I`, and the `<alt>`
   markup splits "songs" to a second line → on-screen `%S` / `%I SONGS`.

So this is a **missing-format-arg / handler-never-fired** problem, not a locale
miss and not a token-resolve gap. The locale entry exists and resolves fine; the
label simply never receives its substitution args natively.

### Why the handler never overwrites it (the actual native gap)

`SelectDifficultyPanel::Enter()` (`SelectDifficultyPanel.cpp:61`) chooses one of
four `update_*_setlist_label` messages by game mode:

- tour → `update_tour_setlist_label`
- party_shuffle → `update_partyshuffle_setlist_label`
- `mp->HasSetlist()` → `update_named_setlist_label` (the `set_list_named_title`
  path)
- else → `update_setlist_label` (the `set_list_title` = "SETLIST (N SONGS)" path)

The native quickplay run is none of tour/party_shuffle and `HasSetlist()` is
false → it takes `update_setlist_label` (`seldiff.dta:62-69`):

```
(update_setlist_label
   ($song_count)
   {if_else {== $song_count 1}
      {setlist_label.grp set showing FALSE}                       ; single song
      {do {setlist_label.grp set showing TRUE}
          {setlist_title.lbl set_token_fmt set_list_title $song_count}}}) ; multi
```

- **Single-song quickplay** (the common native repro): the handler only does
  `{setlist_label.grp set showing FALSE}` and **never touches `setlist_title.lbl`**
  → the baked `%S %I SONGS` default is left on screen. (`setlist_label.grp` holds
  only background art — verified: its child count in the milo is 3 = bg.mat /
  bg.mesh / career_bg.mesh — `setlist_title.lbl` is a sibling top-level object,
  confirmed in the `.milo_xbox` objdir dump.)
- **Multi-song** would correctly fire `set_token_fmt set_list_title $song_count`
  → "SETLIST (N SONGS)". TEXT_TOKENS observed the `%S %I SONGS` raw text
  *co-existing* with a "SETLIST …" line — consistent with the label briefly
  rendering its raw default before / instead of the overwrite, or a single-song
  run where the overwrite path is the no-op branch.

### Ruled out (important — do NOT chase these)

- **`HasSetlist()` / `GetSetlistName()` are NOT broken stubs.** `band3_link_stubs.s`
  (lines 854-855, 864-865) still carries WEAK noop stubs for them, but
  `MetaPerformer.cpp` (lines 253-254) provides STRONG defs and is compiled
  (`nm` on `MetaPerformer.cpp.o` shows `T _ZNK13MetaPerformer10HasSetlistEv` and
  `T _ZNK13MetaPerformer14GetSetlistNameEv`). Strong wins over weak → the real
  functions run. `HasSetlist()` correctly returns false in quickplay because
  `mSetlist == gNullStr` (no named setlist was chosen). This is correct behavior,
  not a defect — the leak is purely that nothing overwrites the baked label
  default in that mode.
- **It is not a `kForceUpper`/markup bug** — those are working as designed; they
  only become visible because the literal `%s`/`%i` reach them.

---

## 3. The fix (per layer) and the recommended approach

The label must be cleared/overwritten on every seldiff enter, before (or instead
of) the mode-specific handler that may skip it. Two viable layers; **recommended =
A (asset), with B available as a hardening fallback** if asset-only proves
insufficient under the native flow (e.g. the enter `cond` ordering / single-song
no-op branch).

### Option A — asset layer (RECOMMENDED, effort S, lowest risk)

Add one statement to the `seldiff.dta` `enter` event (it already hides
`setlist_label.grp` there) to also neutralize the top-level label's baked default:

```
(enter
   {tour.grp set_showing FALSE}
   ...
   {setlist_label.grp set_showing FALSE}
   {setlist_title.lbl set text_token ""}      ; NEW: clear baked %s <alt>%i songs</alt>
   ...)
```

Setting `text_token` to `""` → `SetTextToken(gNullStr)` → `SetDisplayText(gNullStr)`
(empty), so the label is blank until a mode handler legitimately fills it with the
substituted "SETLIST (N SONGS)". This is the exact mechanism the other
`update_*` handlers rely on; it is permuter-safe (asset, not src) and matches the
already-present partial edit on the line above.

- `seldiff.dta` is `#include`d from `ui/init.dta:24`, so it is genuinely loaded at
  runtime and edits take effect (this is how V28's partial `set_showing FALSE`
  edit was applied — verified in-tree at `seldiff.dta:10`).
- **Caveat to verify at implementation:** confirm the `enter` event runs *before*
  `Enter()`'s `update_setlist_label` C++ dispatch, OR that clearing first then
  letting the handler re-fill is the observed order. `UIPanel::Enter()` runs the
  DTA `enter` handler; `SelectDifficultyPanel::Enter()` calls it via
  `UIPanel::Enter()` at line 62 *before* dispatching the `update_*` message
  (lines 74-93), so clear-then-fill is the correct order. Good.

### Option B — glue layer (FALLBACK / hardening, effort S)

If the asset edit alone does not stick (e.g. the milo re-applies its default after
the enter handler, or the native run hits the single-song no-op branch and we want
a guaranteed invariant), enforce it per-frame in the input glue, mirroring the
**existing N4 fix pattern** already in `rb3_game_input.cpp` (the
`song_select_details` `SetShowing(false)` block, lines ~699-725):

- On the seldiff screen (gate on `currentScreen == part_difficulty_screen`),
  find `setlist_title.lbl` in the panel's loaded dir; if its current displayed
  text begins with `%` (raw token), clear it via `SetDisplayText("", true)` or
  re-issue the correct `set_token_fmt set_list_title <numsongs>`.
- Opt-out env (e.g. `RB3_NO_SETLIST_FIX`) consistent with `RB3_NO_DETAILS_FIX`.

Prefer A; only add B if verification shows A insufficient. Do **not** ship both
unconditionally without testing — A should be enough.

### Why NOT to touch the C++ / matched-fork

`SelectDifficultyPanel::Enter()` and the seldiff handler logic are correct and
asm-match-owned (permuter territory). The bug is data (a baked default token), so
fix it in data or in glue — keep the matched fork untouched.

---

## 4. EXPLICIT files-to-edit list (full paths + layer)

Layer tags: (a) matched-fork `src/system|band3/**`; (b) engine
`milo-native-engine/src/**`; (c) glue `rb3/native/src/**`; (d) asset
`rb3/orig-assets/**`.

| Priority | File (full path) | Layer | Edit | Touched by other work? |
|---|---|---|---|---|
| **1 (do this)** | `/home/free/code/milohax/rb3/orig-assets/extracted/ui/seldiff/seldiff.dta` | (d) asset | In the `enter` event (after the existing `{setlist_label.grp set_showing FALSE}` at line 10), add `{setlist_title.lbl set text_token ""}` to clear the baked `set_list_named_title` default. | No. Isolated to seldiff. The N4 song-select fix is a *different* screen (song_select), no overlap. |
| 2 (fallback only) | `/home/free/code/milohax/rb3/native/src/rb3_game_input.cpp` | (c) glue | Optional per-frame hardening in `RB3GameInputPoll` gated on `currentScreen == part_difficulty_screen`: clear `setlist_title.lbl` if its text starts with `%`. Opt-out `RB3_NO_SETLIST_FIX`. ONLY if Option A is verified insufficient. | **YES — high contention.** This file is the shared input-verb / cosmetic-glue seam: it already holds the N4 details-pane fix and is named as the serialize-on file for N7 (load hold) and N8 (hit FX). Any concurrent work touching `rb3_game_input.cpp` must serialize. |

**Reference-only (read to understand the mechanism; DO NOT edit for this task):**
- `/home/free/code/milohax/rb3/src/band3/meta_band/SelectDifficultyPanel.cpp`
  (Enter() handler dispatch — correct, matched-fork, leave alone)
- `/home/free/code/milohax/rb3/src/system/ui/UILabel.cpp`
  (`SetTokenFmtImp`/`PostLoad` substitution path — correct)
- `/home/free/code/milohax/rb3/src/band3/meta_band/MetaPerformer.cpp`
  (`HasSetlist`/`GetSetlistName` — strong defs, working; do NOT touch the
  leftover weak stubs in `native/src/band3_link_stubs.s`)
- `/home/free/code/milohax/rb3/orig-assets/extracted/ui/locale/eng/locale_keep.dta`
  (token values — correct, no edit needed)

**No engine (b) edits and no matched-fork (a) edits are required for the text
sweep.** This keeps it fully concurrent-safe with N1 (engine MeshIB), N2/N3/N5
(matched-fork venue/char/world), and the RTT/menu-void planning items.

---

## 5. Effort / risk / verification

- **Total effort: S** (single asset line; +S if the optional glue fallback is
  needed). One string fixed.
- **Regression risk: low.** Option A only adds a clear of one top-level label's
  baked default on seldiff enter; the legitimate `update_*_setlist_label` handlers
  still fill it afterward in the modes that show a setlist title. Worst case the
  multi-song "SETLIST (N SONGS)" title is momentarily blank before the handler
  fills it (sub-frame). Option B is opt-out-gated and screen-scoped.
- **Concurrency:** Option A (asset) conflicts with nothing. Option B
  (`rb3_game_input.cpp`) must **serialize** against N4/N7/N8 and any other glue
  owner — flag this to the dependency-graph step.

### How to verify

1. Repro the seldiff screen with the standard `RB3_GAME_INPUT` boot-to-song
   script and capture the frame that previously showed `%S %I SONGS`
   (the `07_f0360`-equivalent — the seldiff/loading transition with the
   difficulty card "MIGHTY MICOS… BASS" + "SETLIST" header).
2. **Pass criteria:** the top-left no longer renders `%S %I SONGS`. For a
   single-song quickplay it should be blank/absent; for a multi-song setlist it
   should read "SETLIST (N SONGS)".
3. Optionally enable `MILO_SETTOKEN_DBG=1` and confirm no `SETTOKEN_DBG …
   loc="%s <alt>%i songs</alt>" … out="%s …"` line is the final state for
   `setlist_title.lbl` (i.e. the raw format is not the displayed text).
4. Re-shoot the full boot→gameplay sweep (the `01_f0007 … 11_f1400` set) to
   confirm no regression to any other label (esp. that the legitimate
   "SETLIST (N SONGS)" multi-song title still renders, and the music-library
   filter counts `0/10`, `0/5`, `0/30★` and gameplay HUD "0%" are unchanged).
   Screens that matter: seldiff (`07`), song-select (`06`), gameplay HUD (`10`).

---

## Implemented (V40, glue approach B) — 2026-05-29

**Status N6 — FIXED.** Implemented as the durable, tracked, isolated **glue**
fix (approach B), not the asset edit (A): the asset dir is shared / gitignored /
non-durable across worktrees, so the fix lives in tracked source.

- **File:** `native/src/rb3_game_input.cpp` — in `RB3GameInputPoll(int frame)`,
  a per-frame block immediately after the N4 details-pane block
  (label `// N6 fix …`, ~line 733). Env opt-out: **`RB3_NO_SETLIST_FIX=1`**
  (mirrors `RB3_NO_DETAILS_FIX`). Opt-in diag: `RB3_SETLIST_DBG=1` (logs any
  panel-dir label still carrying a raw `%`, and the replacement when it fires).
  Added includes: `ui/UILabel.h`, `rndobj/Text.h`, `meta_band/BandSongMgr.h`,
  `meta_band/BandSongMetadata.h`, `utl/Locale.h`, `utl/MakeString.h`.

- **CORRECTION to §2 root-cause label identity.** The plan asserted the leak was
  on `setlist_title.lbl`. It is NOT — at the seldiff frame `setlist_title.lbl`
  resolves correctly to `SETLIST <ALT>(2 SONGS)</ALT>` via `update_setlist_label`.
  Walking every `UILabel` in `part_difficulty_panel`'s loaded dir showed the only
  label carrying a raw `%` is the marquee song-preview label **`song_preview.lbl`**.
  It bakes a default `text_token = set_list_named_title` ("%s <alt>%i songs</alt>")
  → `PostLoad`→`SetTextToken` with no args → literal `%s`/`%i` → kForceUpper →
  `%S` / `%I SONGS`. Retail fills it via the `update_preview_song` DTA handler →
  `set_song_and_artist_name_from_sym`, **but that is an `AppLabel` handler and the
  object loads here as a base `BandLabel`** (verified `ClassName()=='BandLabel'`),
  so the message is unhandled and the baked default is never overwritten. (So the
  asset edit A — clearing `setlist_title.lbl` — would NOT have fixed the actual
  on-screen string; B targeting `song_preview.lbl` is what was needed.)

- **What the glue does:** on `currentScreen == part_difficulty_screen`, find
  `part_difficulty_panel` (via `ObjectDir::sMainDir`), require `GetState()==kUp`,
  find `song_preview.lbl` (base `UILabel`). If its `RndText::RawText()` still
  contains `%` (the raw-token state — so a legitimately substituted preview is
  never clobbered), fill it with the current song+artist text computed exactly as
  `AppLabel::SetSongAndArtistNameFromSymbol` (title + master/`store_famous_by`
  artist, joined via `song_artist_fmt[_number]`), applied through the class-
  agnostic base `UILabel::SetDisplayText`. NumSongs/GetSongSymbol are the strong
  MetaPerformer defs. (A use-after-free was avoided by copying `RawText()` into a
  `String` before `SetDisplayText` reallocates `RndText::mText`.)

- **Before / after (frame 352, the first eligible seldiff frame; screenshot frame
  07_f0360):**
  - before: `song_preview.lbl` raw = `%S <ALT>%I SONGS</ALT>` → on screen
    `%S` / `%I SONGS`.
  - after: `1. 20TH CENTURY BOY <ALT>T. REX</ALT>` → on screen
    `1. 20TH CENTURY BOY  T. REX` (song title + artist); the legit "SETLIST"
    header below is unchanged.
  - opt-out (`RB3_NO_SETLIST_FIX=1`) reproduces the raw `%S %I SONGS` — confirming
    the glue is solely responsible.
  - Screenshots: `docs/sessions/native/screenshots/v40-setlist-text/` (07_f0360 =
    seldiff fixed).

- **Regression sweep:** full boot→menu→song-select→seldiff→gameplay
  (`MILO_MAX_FRAMES=24000`, the standard input script) exits 0, 24001 frames
  drawn, no assert/fail/segv. Music-library filter counts (`0/10`, `0/5`,
  `0/30★`, frame 06), gameplay highway + HUD (frame 10) and in-song venue
  background (frame 11) all render unchanged. No other label regressed.

## Appendix — what was confirmed fixed (do not re-open)

| Item | Where fixed | Verified |
|---|---|---|
| `SHELL_PRESS_START_TO_ROCK` | `src/system/utl/Locale.cpp` HX_PC scope (V28) | `v34/f3400` shows correctly-localized "PRESS START" |
| filter-view `%S %I SONGS` | `src/band3/meta_band/ViewSetting.cpp` `FilterViewSetting::Text()` node swap (V28) | present in-tree at `ViewSetting.cpp:200-202` |
| gameplay `%d%%` | `src/system/bandobj/BandTrack.cpp` seed "0%" at load (V29) | present in-tree at `BandTrack.cpp:143-149` |
| music-library status column | `AppLabel::SetViewSettingStatus` → `GetCurrentStatus()` → direct `Localize` (no raw format) | `AppLabel.cpp:637`; `06_f0280` shows clean `0/10`-style counts |
