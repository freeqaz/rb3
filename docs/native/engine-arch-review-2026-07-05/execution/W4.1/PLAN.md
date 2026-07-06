# W4.1 — Phase 4 UI Parity (Lane C) — PLAN

**Stage:** C.S1 (planner, Opus). **Wave 6.** **Author:** W4.1 planner.
**File fence (WAVE6_REVIEW A7):** fixes in **band3/UI game code only** — `rb3/src/band3/`,
`rb3/src/system/ui/`, `rb3/src/system/bandobj/` UI classes. **FORBIDDEN:**
`rb3/native/src/rb3_render_hook.cpp` (Lane A collision) and **all engine files**
(`../milo-native-engine/**`).
**Ground truth used (re-derived, not trusted from memory):** `images/retail-screenshots/`,
`/tmp/visdiff-20260702/` (still present), `/tmp/wave6-current-state/`, plus **fresh native
captures** taken this stage (see each subitem). Harnesses preserved under `W4.1/harness/`.

## TL;DR verdicts (re-derived)

| Subitem | Verdict | Action for impl |
|---|---|---|
| (a) main_hub grey quad | **REAL, current bug** | Fix: HX_NATIVE hide of a spurious grey backing mesh in `menu_buttons.grp`, default-OFF flag. |
| (b) song_select overlap | **LARGELY ALREADY FIXED** | Verify + document the resolved parts; investigate one minor red-sliver residual. No large fix. |
| (c) part_difficulty widgets/black quads | **NOT A BUG** (mid-transition frame) | No fix in-lane. Widgets render correctly. Black poster quads = venue backdrop during zoom → **backlog handoff (venue-side, out of fence).** |

---

## Subitem (a) — main_hub grey menu/"ticker" quad — CONFIRMED REAL

### Diagnosis (evidence-backed)
An opaque light-grey **rounded-capsule quad**, fixed screen-space at roughly
**x∈[380,895], y∈[365,410]** (1280×720), sits mid-screen below the menu list. Retail
(`images/retail-screenshots/yt_mhKNp9uAT48_menu_hub.png`) has **no** such quad there — the news
ticker renders as text at the bottom, and the menu list has no grey bar.

Live-toggle bisection over the embedded `/api/dta/eval` server (harnesses
`W4.1/harness/hub-quad-*.py`, raw shots `/tmp/wave6-hub-probe{1..6}/`) localizes it precisely:

- Owned by **`main_hub_panel`** — `{main_hub_panel set_showing FALSE}` removes it (and the menu).
- A **direct child of `menu_buttons.grp`** — hiding `menu_buttons.grp` removes it; hiding the
  parent `main_menu.grp` removes it; hiding `all.grp` removes it.
- **NOT any named button or subgroup:** hiding `mb_playnow.btn`, `pn_quickplay.btn`, `pn_tour.btn`
  (all `BandButton`), `playnow.grp`, `quickplay.grp`, `mb_playnow.grp` each leaves it present.
- **NOT the message system:** hiding `message_area.grp`, `messages.grp`, `message_area.lst`
  (`BandList`) leaves it present. (`messages.grp` = the bottom MOTD/"NEXT MESSAGE" text — confirmed
  by that text disappearing when it is hidden; the quad stays.)
- **NOT any find-reachable mesh:** `message_bg.mesh`, `difficulty_bar.mesh`, `bar_icon_bg.mesh`,
  `dropshadow.mesh`, `main_header_song_bg.mesh`, `header_song_bg.mesh` — hidden **persistently
  (12×/frame)** — do not remove it. Its `showing` is **re-driven every `Poll`** (one-shot mesh
  hides are transient; only a parent-**group** hide sticks).

**Conclusion:** the quad is a **backing/divider mesh member of `menu_buttons.grp`** in the 360-ARK
`orig-assets/extracted/ui/main/main_hub.milo` (asset, binary) that the native/desktop backend draws
as a flat opaque grey capsule, while the Wii build authored it hidden or as a translucent glass
strip that reads as absent. This is the **SYS-5 family** ("360 assets on the Wii-derived engine draw
slots/backings the Wii hid") — the same class as the already-landed `MusicLibrary::Text` stale-slot
fix. It is **not** a stale *text* slot (text slots are handled elsewhere). The exact leaf mesh could
not be named from source: it is a `menu_buttons.grp` child not reachable by `find "<name>" TRUE`
from the panel (nested sub-Dir / anonymous / merged mesh) and its material is likely a
glass/translucent `.mat` (candidates in the milo: `message_bg.mat`, `ml_glasstop.mat`,
`difficulty_bar.mat`) flattened to opaque grey on native.

### Owning files (in fence)
- `rb3/src/band3/meta_band/MainHubPanel.cpp` / `MainHubPanel.h` (the panel that owns the menu view;
  `Enter()`, `Poll()`, `UpdateStateView`, `SetMainHubState`).
- The asset `ui/main/main_hub.milo` is binary → **not** source-editable; the fix must be a C++
  runtime hide.

### Smallest-scope fix
1. **Impl step 1 — pin the exact leaf (bounded):** enumerate `menu_buttons.grp`'s draw/child list
   at runtime and identify the lone `Mesh` that is (a) the grey capsule and (b) has no associated
   active button/label. Recommended: iterate the group's objects from `MainHubPanel` (the panel has
   the dir) and log name+class+bounds under a temporary `HX_NATIVE` probe, OR read the 360-ARK
   `main_hub.milo` structure with the milo tooling. Cross-check against the ROI above by A/B
   screenshot. **Do not guess** — the six named-mesh persistent-hide attempts already failed.
2. **Impl step 2 — hide it, persistently:** from `MainHubPanel` (`Enter()` + on state change, and
   re-assert in `Poll()` because `showing` is re-driven), `SetShowing(false)` on **only that one
   mesh**. Guard `#ifdef HX_NATIVE`. **Never hide `menu_buttons.grp` / `main_menu.grp`** (that kills
   the menu buttons) — scope strictly to the leaf.
3. Wii/MWCC build is untouched (`HX_NATIVE` undefined there).

### Flag
Register **`RB3_HUB_MENU_QUAD_HIDE`** (opt-**in**, default-OFF) in the engine
`NativeCompatFlags.classification.json` under `flock /tmp/milo-engine-classjson.lock`, **append-only,
NO gen.inc regen** (coordinator regens once at wave end). Class: `workaround`. Flip to default-ON
only after the gate below passes an A/A on everything except the quad. (Note: the census historically
did not scan `src/band3/`; W2.6/W0.6b extended it to `rb3/src/system/` — confirm `src/band3/` is in
scope or the coordinator will need to classify the new row.)

### Before/after gate
Nav: `RB3_HTTP=1 RB3_FIXED_CLOCK=1`, `@10:start,@30:confirm`, wait `state==main_hub_screen`, settle
~90 frames, `/api/screenshot`. **PASS =** grey capsule in ROI x∈[380,895] y∈[365,410]
**present→absent**; menu text (PLAY NOW / QUICKPLAY / START A ROAD CHALLENGE), MOTD ticker text, and
the bottom overshell bar **unchanged (A/A)**. Numeric aid: mean-luma of the ROI drops to
backdrop level (the capsule is ~uniform light grey ≈ 150–170; backdrop varies). Also run the
**lineup gate** (`scripts/native/lineup-gate.py`) to prove no gameplay regression before any
default-ON flip.

---

## Subitem (b) — song_select overlap family — LARGELY ALREADY FIXED

### Diagnosis (re-derived on current HEAD build; captures `/tmp/wave6-ss-recap/`)
The memory/visdiff "stale-slot / overlapping text" family is **already resolved** on the current
build:

- **Text overlap FIXED.** `src/band3/meta_band/MusicLibrary.cpp:1094-1160` already carries the
  `#ifdef HX_NATIVE` override in `MusicLibrary::Text()`: it clears the slot label before writing
  (restoring the base provider's clear-then-write contract that the 360-ARK `song_select.milo`
  broke) and adds a `UILabel` fallback when the 360 asset authors a slot as a plain `UILabel`
  instead of `AppLabel`. Fresh captures at header rows (SETLISTS / RANDOM SONG / "A" / "B") and song
  rows show **clean, non-overlapping** text — no leftover group-letter / "N SONGS" over titles.
- **Grey placeholder quad below album art FIXED.** The 2026-07-02 visdiff
  (`/tmp/visdiff-20260702/native_song_select_list.png`) showed a solid grey quad under the album-art
  panel when a non-song row was highlighted. On current HEAD that quad is **gone** — the
  difficulty/review panel no longer draws its backing for non-song rows; the area shows the venue
  backdrop.

**Remaining minor residual:** a thin dark-red **vertical sliver** at the list's right edge, position
varying between captures (≈x882/y118-155 at depth 0; ≈x640/y290-340 in the older visdiff). Low
severity; likely a scrollbar-thumb or a highlighted-row right-cap — **must be identified before any
fix** (it may be legitimate).

### Owning files (in fence)
- `rb3/src/band3/meta_band/MusicLibrary.cpp` (home of the existing stale-slot fix; also the song
  list widget). If the sliver is a list decoration it lives here or in the song_select list config.

### Smallest-scope fix
**No large new fix.** Impl tasks:
1. **VERIFY + DOCUMENT** the two already-resolved items with an explicit before/after A/A capture set
   (header row + song row), so the wave has a record that (b) is closed — do not re-implement.
2. **Investigate the red sliver:** live-toggle-identify it (same `/api/dta/eval` technique as the
   hub probes, applied to `music_library`). If it is a spurious 360-ARK element, HX_NATIVE
   `SetShowing(false)` behind default-OFF **`RB3_SONGSEL_SLIVER_HIDE`**. If it is a legitimate
   scrollbar, **document as no-fix** and close.

### Flag
Only if the sliver fix lands: `RB3_SONGSEL_SLIVER_HIDE`, default-OFF, registered append-only
(same protocol as (a)).

### Before/after gate
song_select captures at a header row and a song row: text clean (already), no grey quad below album
art (already), sliver present→absent **iff** fixed; A/A on list text + album art panel.

---

## Subitem (c) — part_difficulty missing widgets + black poster quads — NOT A BUG

### Diagnosis (RE-CAPTURED with generous settle; captures `/tmp/wave6-partdiff-recap/`)
Per the coordinator's instruction, recaptured with settle frames
(`W4.1/harness/partdiff-settle-recap.py`): navigated to `part_difficulty_screen` and captured a
7-frame sequence at **+0/+30/+60/+120/+180/+240/+360 frames after arrival** (frames 366→734), with
**no `part:`/`diff:` press** (the screen stays stable). The part/difficulty widgets render
**correctly and stably across the entire sequence**:
- song header ("1. 20TH CENTURY BOY — T.REX / SETLIST (2 SONGS)"),
- album art (top-left),
- the "RIGHTY MODE / SONG DIFFICULTY" panel,
- the "CHOOSE INSTRUMENT / GUITAR / BASS" card (and, later in the flow, CHOOSE DIFFICULTY /
  EASY-MEDIUM-HARD-EXPERT).
No black poster quads. This matches the working 2026-07-02 visdiff
`/tmp/visdiff-20260702/native_choose_diff.png`.

**The current-state `partdiff_default.png` (frame 390) was a mid-transition frame.** That capture's
nav pressed `@380:part:guitar` — frame 390 lands in the **part-confirm camera ZOOM into the venue
poster wall** (a "Restaurant Bar & Grille" menu-board backdrop). The "solid-black poster quads + no
widgets" is that transient close-up of the venue backdrop, **not** a UI bug.

**Black poster quads:** they are the **venue backdrop's poster / menu-board meshes** seen during the
zoom close-up — **venue-side** (Crowd / `RndEnviron` geometry + materials), **OUT of this lane's
fence** (band3/UI). Whether they are missing textures (SYS-5) or just unlit-during-zoom is a
venue-render question.

### Owning files
**NONE in this lane. No fix.**

### Handoff / gate
- **Backlog handoff (documented, not a Lane C fix):** "part_difficulty part-confirm zoom shows
  solid-black venue poster/menu-board quads." Route to the **Lane D venue family** (W2.7 black head /
  W3.3 grayscale venue) or file a fresh venue-texture item. Evidence: `/tmp/wave6-current-state/partdiff_default.png`
  vs the clean settle sequence `/tmp/wave6-partdiff-recap/`.
- **Gate:** the recapture sequence is the proof. Optional future regression guard (not required this
  wave): assert the "CHOOSE INSTRUMENT" + "SONG DIFFICULTY" labels draw within ~120 frames of
  `part_difficulty_screen` (already true).

---

## Sequencing / process notes
- (a) is the only substantive fix. (b) = verify + minor probe. (c) = doc + backlog.
- **A7 collision guard:** none of these touch `rb3_render_hook.cpp` or engine files. If the (a) leaf
  hide ever tempts a render-hook edit, STOP and escalate to the coordinator (Lane A owns that file).
- **Commit-per-review-cycle:** commit each fix as its own gate passes, `flock /tmp/rb3-git.lock`,
  own files only, message prefix `W4.1:`. New flags append-only under
  `flock /tmp/milo-engine-classjson.lock`, **no gen.inc regen**.
- **Checkpoint:** `/tmp/wave6-checkpoints/C-S1.json`.
