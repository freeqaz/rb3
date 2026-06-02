# Difficulty + Instrument-Select Flow (Native + Web Port)

**One-line verdict:** The native boot path hardcodes **guitar / Expert** in a single
synthetic-input helper (`rb3_game_input.cpp:582`, `bu->SetDifficulty(kDifficultyExpert)`)
and bypasses the entire retail overshell part/difficulty state machine — the player
never gets to choose; the real selector code (`OvershellSlot::SelectPart` /
`SelectDifficulty`) is fully present and works, it is just never reached.

---

## 1. Current state — what works vs what is stubbed/missing

### 1a. Difficulty storage and propagation — ALL PRESENT AND WORKING (matched fork)

Difficulty lives on `BandUser`, and the full propagation chain into gameplay is
already decompiled and links natively:

- `enum Difficulty { kDifficultyEasy=0, kDifficultyMedium=1, kDifficultyHard=2,
  kDifficultyExpert=3, kNumDifficulties=4 }` — `src/band3/game/Defines.h:6-12`.
- `BandUser::mDifficulty` (offset 0x8) — `src/band3/game/BandUser.h:91`. Default is
  `DefaultDifficulty()` = `SystemConfig("tour")->FindInt("default_difficulty")`
  (`src/band3/game/Defines.cpp:147-149`), set in the ctor and in `Reset()`
  (`BandUser.cpp:34`, `:53`).
- `BandUser::SetDifficulty(Difficulty)` — `BandUser.cpp:70-80`. Sets `mDifficulty`,
  flips the `unk_0xC` "fully-in-game" bit, and (if a `Player` is bound) calls
  `mPlayer->ChangeDifficulty(d)`. There is a `Symbol` overload too (`:90`).
- **The `unk_0xC` bit is load-bearing:** `BandUser::IsFullyInGame()` returns true only
  when `unk_0xC` is set (`BandUser.cpp:82-88`). `TrackPanel::CreateTracks` filters users
  on `IsParticipating() && IsFullyInGame()` (per `docs/sessions/native/K8_BLOCKERS.md:53-55`).
  Only `SetDifficulty` toggles this bit — so the native path MUST set a difficulty or
  gems never appear.
- Into gameplay: `GameConfig::AssignTrack` reads `u->GetDifficulty()` and forwards it to
  `PlayerTrackConfigList::UpdateConfig(...)` (`src/band3/game/GameConfig.cpp:164-166`) and
  `AddConfig` (`:201-203`); this is consumed at song load to filter the MIDI to the right
  difficulty gems. Mid-game changes flow through `Player::ChangeDifficulty`
  (`src/band3/game/Player.cpp:1006-1013`) → `GameConfig::ChangeDifficulty`
  (`GameConfig.cpp:242-243`) → `GemPlayer::ChangeDifficulty`
  (`src/band3/game/GemPlayer.cpp:1558-1568`) → `SongDB::ChangeDifficulty` +
  `mTrack->ChangeDifficulty`. Difficulty also gates scoring/crowd
  (`Player.cpp:677,719,932`).

**Conclusion:** nothing in the difficulty plumbing is stubbed. The only thing missing is
the *choice* — feeding a player-chosen `Difficulty`/`TrackType` instead of a constant.

### 1b. The retail selection UI — PRESENT, but never driven natively

The retail flow on `part_difficulty_screen` runs through the **overshell** slot state
machine (data-driven from `orig-assets/extracted/ui/overshell/slot_states.dta`):

- `kState_ChoosePart` (`slot_states.dta:1184`) shows a `choose_part.lst` UIList; d-pad
  navigates, Confirm fires `SELECT_MSG` which switches on the highlighted `selected_sym`
  and sends `{$this select_part kTrackGuitar}` / `kTrackBass` / `select_drum_part` /
  `select_vocal_part` etc. (`slot_states.dta:1210-1240`).
- `kState_ChooseDiff` (`slot_states.dta:1476`) shows a `choose_diff.lst`; Confirm fires
  `SELECT_MSG` → `{$this select_difficulty kDifficultyEasy|Medium|Hard|Expert}`
  (`slot_states.dta:1480-1488`).
- `kState_ReadyToPlay` (`slot_states.dta:1536`) → on confirm,
  `overshell end_override_flow kOverrideFlow_SongSettings TRUE` → `Game::LoadSong`.

The C++ handlers that consume those messages are fully implemented:
- `OvershellSlot::SelectPart(TrackType)` / `SelectVocalPart` / `SelectDrumPart` /
  `SelectPartImpl` — `src/band3/meta_band/OvershellSlot.cpp:369-438`. Sets the user's
  track type + preferred score type, then advances the slot state.
- `OvershellSlot::SelectDifficulty(Difficulty)` — `OvershellSlot.cpp:506-541`. Calls
  `pUser->SetDifficulty(diff)` and `EndOverrideFlow(kOverrideFlow_SongSettings, true)`
  (the song-settings path at `:514-516`).
- DTA message wiring: `HANDLE_ACTION(select_part, ...)` `OvershellSlot.cpp:1912`,
  `select_vocal_part` `:1913`, `select_drum_part` `:1914`, `select_difficulty` `:1920`,
  `end_override_flow` `:1936`.
- `SelectDifficultyPanel` (`src/band3/meta_band/SelectDifficultyPanel.{h,cpp}`) is the
  **screen-level** panel for `part_difficulty_screen` (marquee + setlist label +
  `overshell->SetMinimumDifficulty(kDifficultyEasy)` at `SelectDifficultyPanel.cpp:124`);
  the actual part/difficulty picking is the overshell sub-flow above, not this panel.

**Why native skips it:** the overshell `SELECT_MSG` is consumed by the *slot's* state, not
by a screen focus component. A raw screen-level `ButtonDownMsg(Confirm)` on
`part_difficulty_screen` is not consumed by the slot (the synth user's slot focus is
"(none)"), so it never crosses. This is documented in-code at
`rb3_game_input.cpp:644-700` (the `WebDrivePartSelect` workaround) and
`K8_BLOCKERS.md:41-79`.

### 1c. The native hardcode — exactly here

`native/src/rb3_game_input.cpp`, `ExecTrack()` (lines 575-590):

```cpp
void ExecTrack(const std::string &trackSym) {
    ...
    TrackType ty = SymToTrackType(sym);
    bu->SetTrackType(ty);
    bu->SetDifficulty(kDifficultyExpert);   // <-- line 582: HARDCODED Expert
    ...
}
```

`ExecTrack` is reached three ways, all of which currently hardcode Expert and a
script/host-chosen instrument:
1. `track:<sym>` RB3_GAME_INPUT script directive (parsed `:360-368`, dispatched via
   `kVerbTrack` → `DispatchVerb` `:858`). Scripts use `track:guitar`
   (`scripts/native/song-end-test.py:60`, `gameplay-depth-capture.py:22`).
2. The web part-select crossing state machine `WebDrivePartSelect` hardcodes
   `v.trk.trackSym = "guitar"` (`rb3_game_input.cpp:712`) then `end_override_flow`.
3. HTTP `/api/input` `track:<sym>` verb (`ExecVerb` `:872`).

There is **no difficulty selector at all** — no `difficulty:<n>` verb, no UI nav into the
overshell choose-diff list, nothing. Instrument is chosen only by the script/host string;
the in-game player has no way to pick either.

Additional hardcodes that the chosen difficulty/part must respect (currently bypassed):
- `SynthUser()` forces `DebugSetControllerTypeOverride(kControllerGuitar)`
  (`rb3_game_input.cpp:477`) — so even the controller type is pinned to guitar.

---

## 2. Goal — desired/retail behavior

At the song-select → part_difficulty hand-off, the player picks an **instrument/part**
(Guitar / Bass / Drums / Pro-Drums / Keys / Pro-Keys / Vocals / Harmony / Pro-Guitar /
Pro-Bass — gated by their controller type) and a **difficulty** (Easy / Medium / Hard /
Expert), exactly as the retail overshell choose-part → choose-diff → ready-to-play flow
does, then the chosen values flow into gameplay (gem filtering + scoring) via the existing
`GameConfig`/`PlayerTrackConfigList` path. Keyboard (and HTTP, for the harness) drives it.

---

## 3. Proposed approach — phased, layer-tagged

Prefer **layer (c) per-decomp glue** (`rb3/native/src/rb3_game_input.cpp`). The entire
selection-handler chain (1a/1b) is already present in the matched fork and links — we only
need to *drive* it and *un-hardcode* the constant. No layer (a) edits required. No layer
(b) engine edits required.

### Phase 0 — un-hardcode + add a `difficulty:` verb (quick win, layer (c))

1. In `ExecTrack` (`rb3_game_input.cpp:575`), stop forcing Expert. Add a file-static
   `gSelectedDifficulty` (default `DefaultDifficulty()` for retail-faithful default;
   the existing scripts/tests can keep Expert via the new verb so behavior for the harness
   is preserved). Set `bu->SetDifficulty(gSelectedDifficulty)` instead of the literal.
2. Add a new verb `difficulty:<easy|medium|hard|expert>` (also accept `0..3`): parse it in
   `ParseScript` (alongside `track:` at `:360`), in `ExecVerb` (alongside `track:` at
   `:872`), and add a `kVerbDifficulty` to the `VerbKind` enum (`:271`) + `DispatchVerb`
   (`:853`). Handler `ExecDifficulty(Difficulty d)`: set `gSelectedDifficulty = d`, and if
   a `BandUser` already exists, call `bu->SetDifficulty(d)` immediately. Mirror `ExecTrack`
   (`SymToDifficulty` from `game/Defines.h`).
3. Also expose a `controller:<guitar|drum|...>` verb (or extend `track:`) so the
   `DebugSetControllerTypeOverride(kControllerGuitar)` pin at `:477` can be set from the
   chosen instrument family rather than always guitar. Lowest-risk: keep guitar default but
   let the verb override it before `SynthUser()` pins it.

This alone gives the harness (and `/api/input`) full instrument+difficulty control and
makes "choose difficulty" real end-to-end (verified via gem density / `/api/dta/eval`).

### Phase 1 — drive the REAL overshell choose-part/choose-diff flow (layer (c))

Replace the `track:`/`end_override_flow` shortcut with the actual retail handler calls so
the visible part_difficulty UI reflects the choice and the state machine is exercised:

4. Add `ExecOvershellSelect`: resolve the OvershellPanel (`ObjectDir::Main()->Find
   <OvershellPanel>("overshell", true)`, see `OvershellPanel.cpp:123`), get the synth
   user's slot via `OvershellPanel::GetSlot(BandUser*)` (`OvershellPanel.h:76`), then call
   `slot->SelectPart(ty)` / `SelectVocalPart(b)` / `SelectDrumPart(b)` and
   `slot->SelectDifficulty(d)` directly (`OvershellSlot.cpp:369/506`). These run the same
   state transitions a real Confirm would, ending at `kState_ReadyToPlay` →
   `end_override_flow`. New verbs: `part:<track>` and `diff:<n>` (or fold into the gated
   web/script sequence `WebDrivePartSelect` `:704`, replacing the track+end_flow steps).
5. Keep readiness gating (the existing `VerbReady`/`VerbName`/`DispatchVerb` machinery,
   `:758-862`) — only fire once the slot exists and `part_difficulty_screen` is stable.

### Phase 2 — keyboard-driven nav of the on-screen list (layer (c), optional/full parity)

For genuine "player chooses with arrow keys", route d-pad/Confirm to the overshell slot's
focused list. The overshell consumes input through its slot state, not the screen focus
panel; the practical native approach is: when on `part_difficulty_screen`, map Up/Down to
moving the highlighted entry in `choose_part.lst`/`choose_diff.lst` (via the slot's
`UpdatePartSelectList` provider, `OvershellSlot.cpp:1800`) and Confirm to firing the slot's
`SELECT_MSG` for the highlighted `selected_sym`. This is more involved (overshell focus
routing) and is the only part that may need investigating the overshell input interceptor;
defer unless full retail-identical nav is required. Phases 0-1 already deliver real choice.

---

## 4. Key files

- `rb3/native/src/rb3_game_input.cpp` — **(c) the only file to edit.** `ExecTrack` (575,
  the hardcode at 582), `WebDrivePartSelect` (704), `ExecVerb` (867), `ParseScript` (324),
  `DispatchVerb` (853), `VerbReady` (758), `SynthUser` controller pin (477).
- `rb3/src/band3/game/Defines.h` — (a, read-only) `enum Difficulty`, `SymToDifficulty`,
  `DefaultDifficulty`, `SymToTrackType` via `beatmatch/TrackType.h`.
- `rb3/src/band3/game/BandUser.{h,cpp}` — (a, read-only) `SetDifficulty`/`SetTrackType`,
  `IsFullyInGame`/`unk_0xC` bit, `GetDifficulty`/`GetTrackType`.
- `rb3/src/band3/game/GameConfig.cpp` — (a, read-only) `AssignTrack`/`AssignTracks` read
  `GetDifficulty()` into `PlayerTrackConfigList` — the gameplay consumer.
- `rb3/src/band3/meta_band/OvershellSlot.cpp` — (a, read-only) `SelectPart` (369),
  `SelectDifficulty` (506), DTA handlers (1912-1964). The real selector logic.
- `rb3/src/band3/meta_band/OvershellPanel.{h,cpp}` — (a, read-only) `GetSlot(BandUser*)`
  (`OvershellPanel.h:76`) to reach the slot.
- `rb3/src/band3/meta_band/SelectDifficultyPanel.cpp` — (a, read-only)
  `part_difficulty_screen` screen panel (marquee/setlist; not the picker itself).
- `rb3/orig-assets/extracted/ui/overshell/slot_states.dta` — (read-only) the choose-part
  (1184) / choose-diff (1476) state DTA; ground truth for the message names.
- `rb3/scripts/native/song-end-test.py` (`track:guitar` at :60),
  `gameplay-depth-capture.py` (:22) — harness scripts to update / use for verification.

---

## 5. Quick wins (< 1 day) vs larger work

**Quick wins (Phase 0, a few hours):**
- Replace the literal `kDifficultyExpert` at `rb3_game_input.cpp:582` with a settable
  `gSelectedDifficulty`.
- Add the `difficulty:<easy|medium|hard|expert|0-3>` verb to script + `/api/input`.
- This makes difficulty fully controllable from the harness and proves the whole chain
  (gem filtering changes with difficulty) without touching the UI.

**Larger work:**
- Phase 1 (drive real overshell `SelectPart`/`SelectDifficulty` via the slot): ~1 day,
  mostly slot-lookup + readiness wiring + replacing the web/script crossing sequence.
- Phase 2 (keyboard nav of the on-screen overshell lists): ~2-3 days; requires
  understanding overshell input routing (slot focus, not screen focus). Optional for parity.

---

## 6. Dependencies & risks

- **Depends on the existing boot→song path** (splash→main_hub→song_select→part_difficulty,
  `BOOT_TO_SONG.md`) which is already working; this task starts at `part_difficulty_screen`.
- **`unk_0xC` invariant:** any difficulty path MUST go through `BandUser::SetDifficulty`
  (it flips the IsFullyInGame bit). Setting `mDifficulty` directly would silently drop the
  player from `TrackPanel::CreateTracks` (no gems). Low risk if you call the setter.
- **Part-entry reset:** entering part_difficulty resets the user's track to `kTrackNone`
  (`BandUser::Reset`/`SelectDifficultyPanel::Enter` → `ClearTrackTypesFromUsers`,
  `SelectDifficultyPanel.cpp:106`). So `track:`/`SelectPart` must fire AFTER the screen
  enters (K8_BLOCKERS.md:77-79 — the script uses frame 360 for this). Keep the readiness
  gate.
- **Part validity:** `OvershellSlot::SelectPartImpl` (`:379`) checks the part actually
  plays in the song (`PartPlaysInSong`) and can route to denial states; a chosen part not
  in the song would land in `kState_ChoosePartDenial` rather than ready. Phase 1 must use
  `SelectPart` (which handles this) rather than blindly setting track type.
- **Web/native parity:** all edits are in the shared `rb3_game_input.cpp` under existing
  `#ifdef __EMSCRIPTEN__` patterns; native and web behave identically (per
  `project_native_visual_repro_loop` memory). No layer (a)/(b) edits → zero asm-match risk.
- **Difficulty default:** switching the native default from forced-Expert to
  `DefaultDifficulty()` changes existing harness captures (they assume Expert). Mitigate by
  having the test scripts pass `difficulty:expert` explicitly.

---

## 7. Effort & priority

- **Priority: P1.** Difficulty/instrument choice is core to a playable port but the engine
  plumbing is already complete; this is glue + UX, not a deep engine bring-up. It is not a
  boot blocker (the game runs), so below P0 render/audio fundamentals but above polish.
- **Effort:** Phase 0 ~0.5 day; Phase 1 ~1 day; Phase 2 (full keyboard parity) ~2-3 days.
  Recommended MVP = Phase 0 + Phase 1 (~1.5 days) → real selectable difficulty/part driven
  by keyboard/HTTP through the genuine overshell handlers, deferring on-screen list nav.

---

## 8. Verification plan

**Native harness (preferred — ~3s rebuilds, headless):**

1. Build: `cmake --build native/build-native --target rb3-native`.
2. Phase-0 smoke (difficulty changes gem stream). Run headless with HTTP API:
   ```
   RB3_GAME=1 RB3_HTTP=1 RB3_HTTP_PORT=9123 MILO_HEADLESS=1 \
   RB3_DATA=/home/free/code/milohax/rb3/orig-assets/extracted \
   native/build-native/rb3-native &
   ```
   Drive boot→song with a script that selects a song, then `track:guitar` +
   `difficulty:easy`, capture; repeat with `difficulty:expert`. Compare gem density via
   `/api/screenshot` and `/api/dta/eval` (e.g. evaluate the synth user's
   `{player difficulty}` = `Player.cpp:1098 HANDLE_EXPR(difficulty, ...)`, and the active
   player's gem count). Easy should yield visibly fewer gems than Expert.
3. Phase-0 unit check via `/api/dta/eval`: after firing `difficulty:hard`, confirm
   `BandUser::GetDifficulty()==kDifficultyHard` and `IsFullyInGame()==1` (probe through the
   overshell/player DTA exprs). Confirm `GameConfig`'s `PlayerTrackConfigList` got the value
   (the K8_DBG log at `GameConfig.cpp:152-159` prints the assigned config; extend it to log
   difficulty).
4. Phase-1 check: drive the real `SelectPart`/`SelectDifficulty` via the new `part:`/`diff:`
   verbs; confirm the overshell slot reaches `kState_ReadyToPlay` (PART_DBG log already
   exists at `rb3_game_input.cpp:1291-1303`: `all_slots_ready_to_play`) and that the song
   loads at the chosen difficulty. Use `scripts/native/song-end-test.py` as the template,
   replacing its `track:guitar,...end_override_flow` line with the part/diff verbs.
5. Regression: re-run `scripts/native/song-end-test.py` with explicit `difficulty:expert`
   to confirm the existing song-end behavior is unchanged.

**Web (only after native passes, for the keyboard UX — Phase 2):**

6. `scripts/web/build.sh` then `python3 native/web/server.py`; in the browser on
   `part_difficulty_screen`, use Up/Down + Enter to pick part then difficulty (the
   `kWebKeyMap` arrows/Enter at `rb3_game_input.cpp:101-112` already feed `ExecButton`).
   Confirm the on-screen choose-part / choose-diff lists move and that the selected
   difficulty is what gameplay loads. This is the only step that requires Phase 2's
   list-nav routing.
