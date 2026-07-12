# W31-HUD-GLYPHS — PLAN

**Lane B.** KEY=W31-HUD-GLYPHS. Base SHA fd119705, engine pin b36bcfc.

## Charter (verbatim Q(f))
F2 (translucent score pill) + F3 (white glyph class) + F4 (star-slot row) as ONE
HUD material/texture-bind family lane: "trace one glyph end-to-end, fix the bind,
verify the class across hub/song_select/overshell + the pill/star row vs retail."
Start glyph: song_select footer pill (F3).

Late-add (2026-07-12): song-select per-instrument difficulty icons reported
MISSING on a focused song row. Grade MISSING vs WHITE-FALLBACK vs PRESENT; if
MISSING/WHITE-FALLBACK fold into the family verification class.

## Retail pairs
- `images/retail-screenshots/yt_qRagnZCIMzk_gameplay_guitar.png` (opaque pill + 5-star row)
- `images/retail-screenshots/yt_qRagnZCIMzk_song_select_list.png`
- `images/retail-screenshots/yt_mhKNp9uAT48_menu_hub.png`
- atlas `images/retail-screenshots/ui_buttons_wii_spriters.png`

## STEP 0 — end-to-end trace (checkpointed, NO fix code first)
1. Lint-4 registry sweep (engine flag registry) — done, see STATUS.
2. Trace ONE glyph (song_select footer pill) milo→draw; NAME mechanism + TU.
3. Probe: boot native w/ `RB3_UI_FLOOR_DBG=1`, dump UI-text material names at
   song_select + gameplay; identify the white-blob button-glyph material name(s).
4. Confirm whether fix TU is engine-side (→ engineAckNeeded, no engine write) or
   game-side writable per grant.

## A8 gate
STEP-0 = named-TU mechanism checkpoint + coordinator ack before first engine
write. `native/src/rb3_render_hook.cpp` edits need coordinator sign-off per grant.

## Verification
Per-screen A/B crops (hub, song_select incl. focused-song sidebar, overshell,
gameplay pill + star row) into evidence/, human-eye graded vs retail pairs.
Flag hit-count on any no-change claim (lint 8).

## Exit
One-mechanism fix verified across the class, OR split memo proving ≥2 mechanisms
each priced, OR engineAckNeeded checkpoint (named TU + proposed edit).
