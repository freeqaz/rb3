# T2 — Dolphin (Wii) ground-truth oracle for RB3 band faces

**Goal (retargeted):** boot the *real* RB3 Wii disc in Dolphin, reach a state where a
band-member character is visible, and capture screenshot(s) of a band face/skin — to
know what a **correct** RB3 face looks like (skin detail, eye brightness, shading) vs
our port's flat/over-bright faces.

**Result: SUCCESS.** Drove the real game fully headless — logos → intro → title →
profile → main menu → Quick Play → song select → **live in-song gameplay with the band
rendering**. Captured real-time RB3 band members in two venues plus the menu shell.
Reference shots persisted in `dolphin-shots/`.

---

## 1. Exact setup that worked (headless, no physical display)

The box has **no `$DISPLAY`**, dual RTX 3090 + Vulkan 1.4, and `/dev/dri/renderD12*`
render nodes. Two things had to be solved: (a) offscreen rendering, (b) input +
screenshots without a keyboard/window.

### Rendering + input path
- **Do NOT rely on `dolphin-emu-nogui -p headless`.** The nogui headless platform has
  **no hotkey/screenshot handling** (only `PlatformX11.cpp` maps F9→`Core::SaveScreenShot`).
  Its only capture method is continuous frame-dump-to-video, which grows unbounded and
  is too slow to decode for interactive stepping.
- **Working approach: virtual X display (Xvfb) + `-p x11` + Vulkan on the NVIDIA GPU.**
  Gives on-demand **F9 screenshots** (and Shift+F1..F8 = save state, F1..F8 = load) that
  can be injected with `xdotool`, plus tiny PNG outputs.

```bash
# 1) Virtual X server (persist it via a detached/background launch — a plain '&'
#    child gets SIGHUP'd when the shell returns)
Xvfb :99 -screen 0 1600x900x24 -nolisten tcp &      # (libEGL /dev/dri warnings are harmless)
DISPLAY=:99 xdotool getdisplaygeometry               # -> "1600 900" when ready

# 2) Input pipe: Dolphin's "Pipe" ControllerInterface device (a FIFO under User/Pipes/)
mkfifo /tmp/dolphin-rb3/User/Pipes/rb3pipe            # must exist BEFORE Dolphin starts

# 3) Boot the disc under the virtual display, Vulkan backend
DISPLAY=:99 timeout 1800 \
  /home/free/code/milohax/dolphin/build/Binaries/dolphin-emu-nogui \
  -u /tmp/dolphin-rb3/User -p x11 -v Vulkan \
  -e "/home/free/code/milohax/rb3/Rock Band 3 (USA).wbfs" &
```

### Key config (`/tmp/dolphin-rb3/User/Config/`)
- `Dolphin.ini`: `[Core] SkipIPL=True`, `EmulationSpeed=0.0000` (unlimited → boots to
  menu ~2.5× real-time), `[DSP] Backend = No audio output`, `[Interface] ConfirmStop=False`,
  `[Analytics] Enabled=False / PermissionAsked=True` (skip first-run prompt).
- `GFX.ini`: `[Settings] InternalResolution=3` (screenshots come out **2436×1368**, crisp).
- `WiimoteNew.ini`: **emulated Wii Remote with a Guitar extension** mapped to the pipe.
  RB3 needs an instrument to start a song; a bare Wiimote only reaches menus. Critically,
  the pipe control names carry a **`Button ` prefix** (this cost several dead runs):

```ini
[Wiimote1]
Source = 1
Extension = Guitar
Device = Pipe/0/rb3pipe
Buttons/A = `Button A`          # green fret / confirm
Buttons/+ = `Button START`      # +  = advance title / CONTINUE
D-Pad/Up   = `Button D_UP`      # also Guitar/Strum/Up
D-Pad/Down = `Button D_DOWN`    # also Guitar/Strum/Down
Guitar/Frets/Green = `Button A`
Guitar/Strum/Up    = `Button D_UP`
Guitar/Strum/Down  = `Button D_DOWN`
Guitar/Buttons/+   = `Button START`
# (avoid binding Buttons/Home to the same token as Buttons/B — it opens the Wii HOME menu)
```

### Driving it
- **Input:** write to the FIFO. `PRESS`/`RELEASE` use the *bare* token (no prefix):
  `printf 'PRESS START\nRELEASE START\n' > /tmp/dolphin-rb3/User/Pipes/rb3pipe`.
- **Screenshot:** `xdotool` F9 to the Dolphin window (synthetic KeyPress is honored even
  with no window manager / no focus):
  ```bash
  WID=$(DISPLAY=:99 xdotool search --name SZBE69 | head -1)
  DISPLAY=:99 xdotool key --window "$WID" F9
  ```
  Helper scripts used this session: `/tmp/dolphin-rb3/{tap.sh,snapx.sh}`.

### Where output lands
- **Screenshots (this method):** `/tmp/dolphin-rb3/User/ScreenShots/SZBE69/SZBE69_<date>_<time>.png`
  (Dolphin nests per game ID). PNG, internal-res 2436×1368, instant, ~0.6 MB each.
- **Frame-dump video (the headless fallback, not used for the final capture):**
  `User/Dump/Frames/SZBE69_<ts>_0.avi` (mpeg4). Enabled via `GFX [Settings] DumpFrames=True`.
  Grows ~1 GB/min at res 3 — purge aggressively; decoding the tail is slow.

### Full menu path to gameplay (guest, no keyboard needed)
title `+` → CHOOSE PROFILE → down → **[GUEST]** → `A` → "Use Guest? **YES**" `A` →
Guitar nav-help **CONTINUE** `A` → main hub **PLAY NOW** `A` → **QUICKPLAY** `A` →
**CHOOSE SONGS** `A` → **RANDOM SONG** `A` → **GUITAR** `A` → **EASY** `A` → LOADING → gameplay.
(Guests can Quick Play but **cannot** open Create/Customize Character — "must sign into a
non-Guest profile"; the character editor needs a named profile = on-screen-keyboard entry.)

---

## 2. Screenshots captured (persisted, GPU-real, 2436×1368)

`docs/native/c8-ground-truth-2026-07-01/dolphin-shots/`:
- `nav_title.png`, `nav_guest.png` — CHOOSE PROFILE; real-time band members idling on the
  rooftop shell, lit by cool night ambient (best "neutral-lit" face samples).
- `face_guitarist_ambient.png` — crop of the orange-haired guitarist face from the shell.
- `nav_song_sel.png` — instrument-select "subway green room" (band rim-lit).
- `gp_00.png`, `gp_b07.png`, `gp_contact.png` — in-song, **club venue** (20th Century Boy /
  T. Rex): note highway + band cuts. `gp_b07` = singer closeup (backlit).
- `face_singer_rimlit.png` — crop of that singer's face.
- `oye_01.png`, `oye_contact.png`, `reroll_a.png` — second venue (Oye Mi Amor / Maná,
  warehouse-club with red/teal stage lights) confirming the same lighting pattern.
- `gc2_contact.png` — gameplay burst incl. the fail/results screen.

---

## 3. What a real RB3 band face looks like

**Headline: in the real game, band faces are DARK, directionally shaded, and mostly
back/rim-lit — the opposite of the port's flat + over-bright faces with glowing eyes.**

- **Gameplay venues (the exact scenario the port renders):** the cinematic director almost
  always frames band members **in silhouette or edge/rim-light against bright stage
  backdrops** (projector screens, neon, colored key lights). Verified across **two different
  venues** and dozens of camera cuts — faces are in shadow; you catch a rim on the glasses,
  a warm glint on the mouth/teeth, hair edges. Skin is *not* brightly lit; eyes are recessed
  and **do not glow**. See `face_singer_rimlit.png` (near-black face, only glasses edge +
  a warm mouth highlight) and the `gp_contact` / `oye_contact` / `gc2_contact` montages.
- **Menu shell (softest, most face-visible):** on CHOOSE PROFILE the idle band members are
  lit by the scene's cool moonlight. The face shows **normal directional shading** — pale
  skin taking the cool key color, visible nose/cheek falloff, eyes sitting in shadow. Low
  texture detail (distant, small), but clearly *shaded*, not flat, not blown out. See
  `face_guitarist_ambient.png`.

**Implication for C8 / the port's "flat, over-bright" faces:** the fault is over-brightening.
Real RB3 keeps face skin low/keyed to scene lighting and keeps eyes non-emissive; the port
appears to add too much ambient/emissive (flat, glowing eyes) and to be missing the strong
directional/back-lit falloff that makes the real faces read as moody rather than washed out.
A faithful match should *darken* skin toward the scene key and stop the eyes from self-glowing,
not brighten faces further.

Caveat: a fully front-lit, texture-detail face closeup is only guaranteed in the **character
editor**, which is guest-locked; every Quick-Play venue sampled is deliberately dark/back-lit.
(Also note: Dolphin renders the **Wii** assets; the native port loads **Xbox `.milo_xbox`**,
so treat Dolphin as the *intended-look / lighting-intent* oracle, not a byte oracle — see T3.)

---

## 4. Feasibility rating

- **Boot + render headless:** ✅ easy/reliable (Xvfb + `-p x11` + Vulkan).
- **Drive to gameplay with a band on screen:** ✅ **done, reproducible** via the pipe +
  the documented menu path (guest + emulated Guitar). ~2–3 min wall to gameplay.
- **On-demand high-res screenshots:** ✅ F9 via xdotool → 2436×1368 PNGs.
- **A *front-lit, texture-detail* face closeup:** ⚠️ not from Quick Play (venues are
  intentionally back-lit). Would need the **character editor** → a **named (non-guest)
  profile**, which requires driving the on-screen keyboard (doable but fiddly; not done here).
- **Save states (`Shift+F1`, load with `-s`)** are available under `-p x11` and would let a
  follow-up checkpoint at a good camera cut / in the editor for repeatable high-res grabs.

### Secondary (memory read for bone/skin matrices)
`dolphin-emu-nogui --help` exposes **no** gdb-stub flag; the Qt build has a GDB stub
(`[Core] GDBPort`) and there is a scripting/`enet` path, but for *this* task the visual
oracle above superseded RAM inspection, so it was not pursued.

---

## Cleanup / reuse
Artifacts live under `/tmp/dolphin-rb3/` (User dir, helper scripts, `shots/`). Xvfb `:99`
and the Dolphin process were left/killed at session end; re-run section 1 to reproduce.
