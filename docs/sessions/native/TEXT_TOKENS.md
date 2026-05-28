# TEXT_TOKENS — locale-miss / format-substitution fixes (2026-05-28)

Session: v28-text-tokens.  Two un-localized token strings that leaked to screen
on the rb3-native LP64 port.  Root causes, fixes applied, remaining open items.

---

## Token 1: `SHELL_PRESS_START_TO_ROCK`

### Symptom
Sideways text "SHELL_PRESS_START_TO_ROCK" (raw symbol name) visible on the
left-side overshell player slot during gameplay and the preloading screen.
Should read "PRESS START" (or similar — the locale value for the token).

### Root cause
`shell_press_start_to_rock` has no `#ifdef HX_WII` variant in
`ui/locale/eng/locale_keep.dta`.  It exists only inside `#ifdef HX_PS3`,
`#ifdef HX_XBOX`, and `#ifdef HX_PC` blocks.  `Locale::Init()` reads DTA files
with only `HX_WII` defined (the native port's `DataReadFile` macro table has
`HX_WII=1` set by `PreInitSystem`).  The `#ifdef HX_PC / HX_WII` branch for
those tokens is never entered → the token is absent from the locale table →
`Localize(shell_press_start_to_rock)` returns the raw symbol string →
`SetTextToken` sets the label text to the raw symbol name → `kForceUpper` in
RndText uppercases it → `SHELL_PRESS_START_TO_ROCK` on screen.

70 tokens total are in PC/PS3/XBOX locale blocks but have no HX_WII entry.

### Fix
`src/system/utl/Locale.cpp`, inside `Locale::Init()`, before the
`DataReadFile` loop:

```cpp
#ifdef HX_NATIVE
    // Temporarily activate HX_PC so PC-only locale tokens (e.g.
    // shell_press_start_to_rock, mm_need_a_profile_net_msg, accomplishments)
    // are included when reading locale files. Unset immediately after so no
    // other DTA file (overshell_dir.dta, config/*.dta, etc.) sees HX_PC.
    DataArrayPtr hx_pc_root(1);
    DataSetMacro("HX_PC", hx_pc_root.mData);
#endif
// ... DataReadFile loop ...
#ifdef HX_NATIVE
    DataSetMacro("HX_PC", nullptr);
#endif
```

Added `#include "obj/DataUtl.h"` for `DataSetMacro`.

Side-effect: 58 tokens exist in both HX_WII and HX_PC blocks; they load twice
→ 10 "Locale symbol redefined" NOTIFYs from `overshell.dta` token block. The
first-loaded (WII) text wins — correct (WII text says "online" vs "Xbox LIVE").

### Verification
`MILO_LOCALE_DBG=1` + `MILO_SETTOKEN_DBG=1`:
```
LOCALIZE_DBG sym=shell_press_start_to_rock size=13129 hit=1 ret="PRESS START"
SETTOKEN_DBG sym=shell_press_start_to_rock loc="PRESS START" hit=1 ... out="PRESS START"
```
Locale table grew from 13059 → 13129 (+70 tokens).

Screenshot: `before_press_start_SHELL_PRESS_START_TO_ROCK_raw.png` (v18, frame
540) vs `after_press_start_PRESS_START_localized.png` (v28, frame 1200).

### Files changed
- `src/system/utl/Locale.cpp` — scoped HX_PC define in `Locale::Init()`

---

## Token 2: `%S %I SONGS` (filter-view song-count label)

### Symptom
Song-select filter-view shows a raw printf-format string `%S %I SONGS` (or
just the bare count number) instead of e.g. "83 SONGS" or "30 SONGS" for each
filter category.

### Root cause
`FilterViewSetting::Text()` in `src/band3/meta_band/ViewSetting.cpp` builds a
2-element `DataArray` to pass to `UILabel::SetTokenFmt`:

```cpp
// ORIGINAL (WRONG):
DataNode word(fmt);        // kDataSymbol, e.g. song_select_songs
DataNode num(LocalizeSeparatedInt(count));  // kDataString, e.g. "83"
DataArray *da = new DataArray(2);
da->Node(0) = num;   // ← count string at index 0
da->Node(1) = word;  // ← locale symbol at index 1
label->SetTokenFmt(da);
```

`SetTokenFmt` → `SetTokenFmtImp` calls `da->ForceSym(0)` to get the locale
symbol.  `ForceSym(0)` on a `kDataString` node interns the count string (e.g.
"83") as a `Symbol` → `Localize("83")` → miss → raw string "83" displayed.

### Fix
Swap the two assignments so the locale symbol is at index 0:

```cpp
// FIXED:
da->Node(0) = word;  // locale symbol at index 0 (for ForceSym(0))
da->Node(1) = num;   // count string at index 1 (substitution arg)
```

`SetTokenFmtImp` then: `ForceSym(0)` → `song_select_songs` → `Localize` →
`"%s Songs"` → `SuperFormatString` → substitutes `%s` with "83" →
`SetDisplayText("83 Songs")` → `kForceUpper` → "83 SONGS".

### Files changed
- `src/band3/meta_band/ViewSetting.cpp` — `FilterViewSetting::Text()`, node
  assignment order (~line 200)

---

## Token 3 (open): `%S %I SONGS` on seldiff setlist label

### Status: OPEN — not fixed this session, escalation candidate for Opus

### Symptom
After navigating to a 2-song setlist in quickplay seldiff, `setlist_title.lbl`
shows `%S %I SONGS` in large type above "SETLIST (2 SONGS)".

### Root cause (investigated)
`setlist_title.lbl` in `seldiff.milo_xbox` has milo-default
`text_token = set_list_named_title`.  When the milo is loaded,
`SetTextToken("set_list_named_title")` fires with no args → raw format
`%s <alt>%i songs</alt>` → `kForceUpper` → `%S %I SONGS`.

On the first seldiff Enter (song selected, quickplay mode, `mp->HasSetlist()`
false), `update_setlist_label(2)` fires → `setlist_title.lbl set_token_fmt
set_list_title 2` → "SETLIST (2 SONGS)" — correct.

On the second seldiff Enter (after navigating through the "MAKING SETLIST"
flow), `update_named_setlist_label` fires → `{setlist_title.lbl set_token_fmt
set_list_named_title $setlist $song_count}`.  This call doesn't appear in
`SETTOKEN_DBG` output, suggesting it either:
  a) never fires (`mp->HasSetlist()` returns false after `ResetSongs()`), or
  b) fires but `SuperFormatString` can't substitute — the `$setlist` arg is
     not a plain string/symbol that `%s` can consume.

The milo-default `set_list_named_title` raw text then persists on screen.

### DTA fix attempted (insufficient)
Added `{setlist_label.grp set_showing FALSE}` to the seldiff.dta `enter`
event.  Does not fix it because `setlist_title.lbl` is a TOP-LEVEL object in
the seldiff milo, not a child of `setlist_label.grp` (the group holds
background mesh art only).

### Recommended follow-up
Opus agent: trace `SelectDifficultyPanel::Enter()` second call.  Determine
whether `mp->HasSetlist()` is false (because `ResetSongs()` cleared `mSetlist`
between the two enters) or whether `update_named_setlist_label` fires but
`$setlist` arg is wrong type for `SuperFormatString %s`.  If (a): call
`update_setlist_label(numsongs)` unconditionally first, then conditionally
overlay the setlist name.  If (b): check `GetSetlistName()` return type and
ensure it passes as a `kDataString` node.

Note: this issue only manifests when `@320:down` selects a SETLIST entry (2+
songs) in the song browser.  Single-song quickplay (no `down` navigation, or
navigating to a single song) does not trigger this path.

---

## Other format issue: `%d%%` score display

Not fixed this session.  A score percent label uses a C `%d%%` format string
that is not substituted (shows raw).  Separate from locale miss — this is a
`printf`-style format not going through the locale/SetTokenFmt path.  Visible
in gameplay venue view screenshots.  Out of scope for this session.

---

## Screenshots

| File | Description |
|------|-------------|
| `before_press_start_SHELL_PRESS_START_TO_ROCK_raw.png` | v18 frame 540 — raw token sideways on left |
| `after_press_start_PRESS_START_localized.png` | v28 frame 1200 — "PRESS START" correctly localized |
| `before_seldiff_pct_S_pct_I_SONGS_raw.png` | v18 frame 450 — seldiff with %S %I SONGS |

---

## Summary of changes (file:line)

| File | Change |
|------|--------|
| `src/system/utl/Locale.cpp` | Add `#include "obj/DataUtl.h"`; scoped `DataSetMacro("HX_PC", ...)` around `DataReadFile` loop in `Locale::Init()` |
| `src/band3/meta_band/ViewSetting.cpp` | `FilterViewSetting::Text()`: swap `da->Node(0) = word` / `da->Node(1) = num` (was reversed) |
| `orig-assets/extracted/ui/seldiff/seldiff.dta` | `enter` event: add `{setlist_label.grp set_showing FALSE}` (partial fix only — does not resolve root cause) |
| `native/src/rb3_game_input.cpp` | Add `gLastFiredFrame` tracking + 2-frame button-settle guard after button-press verbs (fixes state-driven queue racing `down`→`select_highlighted_node` on consecutive frames) |
