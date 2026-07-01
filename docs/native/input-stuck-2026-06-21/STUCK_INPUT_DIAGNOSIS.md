# Stuck-input regression — diagnosis (2026-06-21)

## Symptoms
1. **Red fret held on song entry.** Entering a song, the RED fret is already
   "down". Tapping the red-fret key once clears it.
2. **Menu auto-scrolls to the bottom after the start screen.** Clicking past the
   "Rock Band 3" splash, the main menu scrolls down on its own as if DOWN is held.

Both are **web-only** (the in-browser Emscripten build). The desktop windowed
GLFW path and the headless `pad:`/HTTP harness path are clean (verified — see
"Why the harness can't repro" below).

## Root cause (file:line)

**Two independent browser key-listener installers register keydown/keyup
handlers that flip DIFFERENT bits for the SAME physical keys (`a`/`s`/`d`) in the
ONE shared `window._rb3Keys` bitmask.**

- `native/src/rb3_game_input.cpp:160-166` — `InitWebInput()` (the menu-nav
  installer) maps the WASD letters to the **d-pad**:
  `w/W→bit12 (DUp)`, `s/S→bit14 (DDown)`, `a/A→bit15 (DLeft)`, `d/D→bit13 (DRight)`.
- `native/src/rb3_joypad_native.cpp:269-271` — `InitWebGameplayKeys()` (the
  gameplay-frets installer) maps the SAME letters to **guitar frets**:
  `a/A→bit1 (kPad_R2, green)`, `s/S→bit5 (kPad_Circle, RED)`, `d/D→bit4 (kPad_Tri, yellow)`.

Both call `document.addEventListener('keydown'/'keyup', …, true)` on the same
`window._rb3Keys`. So pressing **`s`** sets BOTH bit 14 (DDown) and bit 5 (red
fret); pressing `a` sets bit 15 (DLeft) AND bit 1 (green fret); `d` sets bit 13
(DRight) AND bit 4 (yellow fret).

Exact conflict set (computed):

| key | menu bit (InitWebInput) | gameplay bit (InitWebGameplayKeys) |
|-----|-------------------------|------------------------------------|
| a/A | 15 (DLeft)  | 1 (R2 = green fret) |
| s/S | 14 (DDown)  | 5 (Circle = **RED fret**) |
| d/D | 13 (DRight) | 4 (Tri = yellow fret) |

Both installers read out of the same mask: `JoypadPoll()`
(`rb3_joypad_native.cpp:477` `ReadWebButtons()`) ORs `_rb3Keys` and calls
`SendButtonMessages(0, btns)` (`src/system/os/Joypad.cpp:338`), the single engine
broadcast that drives BOTH the menu (TheUI's JoypadClient) and the gameplay
GuitarController.

### How each symptom falls out
- **Red fret held on song entry:** the player navigates menus with `s`/`a`/`d`
  (or those letters get pressed). Each press sets a *fret* bit (5 / 1 / 4) in
  addition to the d-pad bit. That fret bit is carried as a held button into
  gameplay, where the GuitarController shows the red fret (bit 5) down. A single
  clean keyup of `s` clears bit 5 → `SendButtonMessages` emits ButtonUp(Circle) →
  fret releases. Hence "tap the red key once to clear it."
- **Menu auto-scroll-down:** pressing/holding the red-fret letter `s` also sets
  bit 14 (DDown). `JoypadClient` arms d-pad auto-repeat for DDown
  (`src/system/os/JoypadClient.cpp:169-170`, repeat mask `0xf000` set in
  `UIManager::UseJoypad` at `src/system/ui/UI.cpp:847`; `hold_ms 1000 /
  repeat_ms 80` from joypad.dta). Auto-repeat re-sends ButtonDown(DDown) every
  80 ms until a ButtonUp(DDown) resets it. With the dual capture-phase listeners
  the matching keyup can fail to clear one of the two bits (browser focus /
  `preventDefault` skew across the two listeners, plus the install-time skew —
  the two listeners are installed on different frames: `InitWebInput` on the
  first `RB3GameInputPoll`, `InitWebGameplayKeys` on the first web `JoypadPoll`),
  leaving DDown latched → the list scrolls to the bottom.

Even without a dropped keyup, the conflict is a bug on its own: a single key drives
two game functions at once (menu-down also presses the red fret, and vice versa).

## Which commit introduced it
**`ce2cab80`** ("native+web: keyboard/gamepad gameplay input via real
JoypadPoll"). It added `InitWebGameplayKeys()` in `rb3_joypad_native.cpp` with the
`a/s/d→fret` aliases. The colliding `a/s/d→d-pad` aliases in `InitWebInput()`
already existed before `ce2cab80` (confirmed via `git show ce2cab80^`). So
`ce2cab80` created the dual-meaning collision in the shared `_rb3Keys`.

**Commit `46c028b6` (the prime suspect) is NOT the cause.** It is a legitimate fix
(`JoypadInitCommon` reuses `gJoypadMsgSource` on re-init instead of orphaning the
JoypadClient's subscription). It does not touch the per-pad button mask or the
web key maps. Verified: the headless `pad:` path produces clean `0→bit→0` edges
with a non-null pad user at all times (so the down/up pair is balanced).

## Proposed fix (concrete)
Remove the conflicting **letter** aliases from the gameplay installer; keep the
non-conflicting digit-row and `f/g/j/k` aliases (the menu map uses none of
`1 2 3 4 5 f g j k`, so they are conflict-free). This keeps WASD as pure menu
navigation and gives gameplay frets unique keys.

In `native/src/rb3_joypad_native.cpp`, `InitWebGameplayKeys()` (lines 269-271),
drop the `a/A`, `s/S`, `d/D` entries:

```js
// BEFORE (conflicts with InitWebInput's WASD d-pad map):
m['1'] = 1 << 1;  m['a'] = 1 << 1;  m['A'] = 1 << 1;   // kPad_R2  (green)
m['2'] = 1 << 5;  m['s'] = 1 << 5;  m['S'] = 1 << 5;   // kPad_Circle (red)
m['3'] = 1 << 4;  m['d'] = 1 << 4;  m['D'] = 1 << 4;   // kPad_Tri (yellow)
m['4'] = 1 << 6;  m['f'] = 1 << 6;  m['F'] = 1 << 6;   // kPad_X   (blue)
m['5'] = 1 << 7;  m['g'] = 1 << 7;  m['G'] = 1 << 7;   // kPad_Square (orange)

// AFTER (digit row + f/g only — no overlap with WASD menu nav):
m['1'] = 1 << 1;                                       // kPad_R2  (green)
m['2'] = 1 << 5;                                       // kPad_Circle (red)
m['3'] = 1 << 4;                                       // kPad_Tri (yellow)
m['4'] = 1 << 6;  m['f'] = 1 << 6;  m['F'] = 1 << 6;   // kPad_X   (blue)
m['5'] = 1 << 7;  m['g'] = 1 << 7;  m['G'] = 1 << 7;   // kPad_Square (orange)
```

Notes:
- `f/g` stay (menu map has no `f`/`g`); `j/k`→strum stay (menu map has no `j`/`k`).
- Also note a secondary, lesser overlap: menu `Escape`/`Backspace`→bit5 (Circle)
  shares bit 5 with gameplay `s/S`. Removing `s/S` from the gameplay map removes
  the dangerous part; `Escape`/`Backspace` keep their single Cancel meaning.
- The gameplay frets are still fully playable on `1 2 3 4 5` (and `f/g`), which
  the docstring in this file already advertises as the primary fret keys.
- This is `native/`-only, web-only glue. Zero shared-`src/`, matched-fork, or
  engine impact. No `#ifdef` needed (the file is already `#ifdef HX_NATIVE`, and
  the block is inside `#ifdef __EMSCRIPTEN__`).

### Optional hardening (not required for the fix)
`src/system/os/JoypadClient.cpp:190` — `JoypadClient::OnMsg(ButtonUpMsg)` gates
its ENTIRE body (forward-to-sink AND auto-repeat `Reset`) on `msg.GetUser()`
non-null, whereas `OnMsg(ButtonDownMsg)` forwards to the sink even with a null
user. This is a latent asymmetry: a ButtonUp with a null user would be dropped,
never clearing a held fret or stopping d-pad repeat. It does NOT bite today
(`gJoypadData[0].mUser` is non-null by the time any press is polled — verified),
so do not change it as part of this fix (it is matched Wii engine code; any change
would need `#ifdef HX_NATIVE` and is out of scope). Recorded only as the reason
the symptom would be *worse* if the pad user were ever transiently null.

## Why the harness can't repro (verification notes)
- Native is headless: `DISPLAY` unset → `JoypadPoll`'s desktop GLFW branch
  early-returns at `if (!w) return;` (`rb3_joypad_native.cpp:485`). Input comes
  from the `pad:`/SEQ/HTTP queue, whose press/release is forced clean.
- `scripts/native/keyboard-to-gameplay.py` reaches `game_screen` cleanly; a
  temporary `RB3_BTN_TRACE` in `SendButtonMessages` showed every press as a
  balanced `0x0 → bit → 0x0` with a non-null `mUser`.
- The bug lives in the **web** dual-`document`-listener path, which only exists
  under `__EMSCRIPTEN__` and is not exercised by the native harness. To confirm
  the fix in-browser: load `?debug=true`, navigate the menu with WASD, enter a
  song, and verify (a) the menu does not auto-scroll and (b) no fret is held at
  song start. (The trace used during diagnosis was reverted; `src/system/os/
  Joypad.cpp` is unchanged.)
</content>
</invoke>
