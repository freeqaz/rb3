# USB guitar controllers (web build)

The RB3 web build (Emscripten/WASM) supports USB plastic-guitar controllers via
the browser [Gamepad API](https://developer.mozilla.org/en-US/docs/Web/API/Gamepad_API).
Frets, strum, star power (overdrive), Start/pause, the d-pad, whammy (analog),
and tilt are all read each frame and fed through the exact same
`SendButtonMessages(0, btns)` chokepoint the keyboard uses — so a guitar drives
menus AND gameplay identically to a real controller.

Implementation: `native/src/rb3_joypad_native.cpp` (inside the `__EMSCRIPTEN__`
branch — `InitWebGuitar`, `ReadWebGamepadButtons`, `ReadWebGamepadWhammy`, and
the whammy write in `JoypadPoll`). Desktop/GLFW behavior is unchanged.

## Requirements (browser realities)

- **HTTPS or localhost.** The Gamepad API only works in a secure context.
  `http://localhost:8421` counts as secure; a plain-HTTP LAN IP does not.
- **User gesture.** Browsers hide gamepads until you press a button / move a
  stick on the controller AFTER the page loads — that first input is what fires
  `gamepadconnected`. Strum or press a fret once and it appears.
- **Chrome/Chromium recommended.** Non-standard HID pads (`mapping === ""`) are
  renumbered per browser; the defaults below are calibrated against Chrome.

## Supported / detected devices

The controller is classified from `gamepad.id` (checked in this order):

| Class | Rule | Handling |
|---|---|---|
| `guitar` | `id` matches `/guitar\|harmonix\|santroller/i` **or** a known instrument vendor hex (`12ba` PS3/Wii RB dongle, `1bad` Harmonix Xbox, `0738` MadCatz/RedOctane, `1430` GH) | mapped as a guitar (family below) |
| `standard` | `mapping === "standard"` and not a guitar | existing face-button fallback (unchanged) |
| `unknown` | anything else | ignored, but logged so you can report + remap it |

### Mapping families (default button/axis indices)

Indices are **browser Gamepad API** `buttons[]` / `axes[]` indices. They are
best-effort defaults from the documented HID report order (santroller
reverse-engineering docs); browsers renumber non-standard pads, so if your unit
is off, use `_rb3GpDebug` + `rb3GuitarMap` (below) — no rebuild needed.

**`ps3wii_rb`** — PS3 / Wii Rock Band guitar dongle (`12ba:0100`, "Harmonix
Guitar for Nintendo Wii"), the primary target. `mapping === ""`.

| Control | Source | → JoypadButton bit |
|---|---|---|
| Green fret | button 1 | 1 (kPad_R2) |
| Red fret | button 2 | 5 (kPad_Circle) |
| Yellow fret | button 0 | 4 (kPad_Tri) |
| Blue fret | button 3 | 6 (kPad_X) |
| Orange fret | button 4 | 7 (kPad_Square) |
| Star power | button 8 (select) + tilt | 8 (kPad_Select) |
| Start / pause | button 9 | 11 (kPad_Start) |
| Strum / d-pad | hat on `axes[9]` (8-step) | 12/14 up/down, 13/15 right/left |
| Whammy | `axes[0]`, rest −1 → full +1 | analog → RX stick |
| Tilt | button 5 (pedal) | ORs star power (bit 8) |

**`xinput_rb`** — Xbox 360 RB guitar via xpad → browser **standard** mapping.

| Control | Source | → bit |
|---|---|---|
| Green / Red / Blue / Yellow / Orange | buttons 0 / 1 / 2 / 3 / 4 | 1 / 5 / 6 / 4 / 7 |
| Star / Start | buttons 8 / 9 | 8 / 11 |
| Strum / d-pad | buttons 12 (up) / 13 (down) / 14 (left) / 15 (right) | 12 / 14 / 15 / 13 |
| Whammy | `axes[2]` (right-stick X) | RX stick |
| Tilt | `axes[3]` > 0.5 | ORs star power (bit 8) |

**`gh_ps3`** — generic Guitar Hero PS3 guitar (`12ba` / `1430` / `0738`). Same
layout as `ps3wii_rb`.

The hat decoder handles BOTH representations: an 8-step fractional hat axis
(Chrome exposes the HID hat as `axes[9]` with values −1 … 1, idle out of range)
and discrete d-pad buttons 12–15 (standard mapping).

Whammy is normalized to `[0..1]` (0 = rest) and written to the RX stick
(`mSticks[1][0] = -1 + 2*whammy01`, clamped) so `GuitarController::GetWhammyBar`
(traditional whammy = `min(0, -(RX+1)/2)`) reads 0 at rest and −1 fully engaged.

## Diagnostics + remap (no rebuild)

Open the browser devtools console.

**1. Check what was detected** — look for `[rb3-guitar]` logs:
```
[rb3-guitar] USB guitar support ready (Gamepad API)
[rb3-guitar] connected idx=0 id="Harmonix Guitar for Nintendo Wii (Vendor: 12ba Product: 0100)" mapping="" buttons=16 axes=10 -> guitar (ps3wii_rb)
```
If your device shows `-> unknown`, copy the `id=` string and file a report.

**2. Calibrate** — enable the raw dump (throttled, logs on change only):
```js
window._rb3GpDebug = 1;
// now press each fret / strum / whammy and watch:
// [rb3-guitar] fam=ps3wii_rb btns[1] axes[0:1.00,9:-1.00] whammy=1.00 tilt=0
```
Note which `btns[...]` index lights up for each fret, which axis moves for
whammy, and the hat axis / d-pad buttons for strum.

**3. Remap** — set `window.rb3GuitarMap` (shallow-merged OVER the family
default, so you can override just one field). Fields:
```js
window.rb3GuitarMap = {
  green: 1, red: 2, yellow: 0, blue: 3, orange: 4,   // buttons[] indices
  start: 9, select: 8,
  strumMode: "hat",        // "hat" (axis) or "buttons" (12-15)
  hatAxis: 9,              // axes[] index of the hat (strumMode "hat")
  whammyAxis: 0,           // axes[] index of the whammy pot
  whammyInvert: false,     // flip if rest/full are reversed
  tiltAxis: null,          // axes[] index for tilt (or null)
  tiltThreshold: 0.5,      // axis value above which tilt = star power
  tiltButton: 5            // buttons[] index for a tilt/pedal switch (or null)
};
```
The change takes effect on the next poll. Once you have a working map, report it
so it can become a built-in family default.

## Smoke test

`scripts/web/_guitar-smoke.mjs` (Playwright) injects a fake `12ba:0100` guitar
and a standard pad against a running `python3 native/web/server.py`, and asserts
classification, fret/hat/whammy decode, whammy-at-rest fallback, and no console
errors. Run from `scripts/web/` (where `node_modules` lives):
```bash
node _guitar-smoke.mjs --port 8421
```
This proves the JS layer is alive; a full e2e gameplay pass with the fake guitar
driving menus/notes is a separate verification step.

## Xbox 360 guitars on macOS (WebUSB)

Xbox 360 controllers (including the plastic guitars: Guitar Hero X-plorer, RB
Xbox 360 guitars) are **not HID**. They speak Microsoft's proprietary **XUSB**
protocol on a vendor-specific USB interface (class `0xFF`, subclass `0x5D`,
protocol `0x01`). On **macOS there is no OS driver** for that interface, so
`navigator.getGamepads()` stays all-null forever and the Gamepad-API path above
can never see the device. On **Windows** XInput exposes it as a standard gamepad;
on **Linux** the kernel `xpad` driver claims it and it likewise shows up as a
normal gamepad — so on those platforms the ordinary Gamepad-API path already
works and no WebUSB is needed.

For the macOS gap, `native/web/guitar-webusb.js` (a page-level `<script>`, loaded
from `index.html`) claims the guitar over **WebUSB** — which *can* open the raw
vendor interface precisely because nothing else on macOS claims it — reads the
interrupt-IN report stream, decodes it, and re-publishes the state as a
**synthetic "standard" gamepad appended to `navigator.getGamepads()`**. It is
shaped exactly like an xpad Xbox-360 RB guitar (`id` contains `1430`,
`mapping: 'standard'`), so the existing C++ `xinput_rb` mapping consumes it with
**zero wasm changes**. The engine's per-frame `getGamepads()` poll picks up the
first guitar-classified pad as usual.

**Chrome/Chromium desktop only** (WebUSB). Firefox/Safari have no `navigator.usb`,
so the module silently no-ops there.

### Connecting (user flow)

1. A small **`🎸 Connect USB guitar`** button appears bottom-right (only when no
   guitar is already present and WebUSB exists).
2. Click it → Chrome's device chooser opens → pick **"Guitar Hero X-plorer"**
   (or your Xbox-360 guitar). This gesture is required by WebUSB's
   `requestDevice()`.
3. The button hides and the guitar drives menus/gameplay immediately.

**Auto-reconnect:** once authorized, the module re-attaches on every later page
load via `navigator.usb.getDevices()` **without** another gesture (one-click-once,
automatic thereafter), and re-attaches on replug via the `connect` event. The
synthetic pad is torn down on `disconnect`.

**Linux note:** if you run desktop Chrome on Linux, `claimInterface()` fails with
`NetworkError`/`SecurityError` because `xpad` owns the device. The module reports
a friendly message and bows out — the guitar is already a normal gamepad there,
so use the Gamepad-API path (frets/strum work without the button).

### Device filters

`requestDevice()` filters on vendor only (not product), covering the XUSB-era
guitar vendors: `0x1430` (RedOctane/GH), `0x1bad` (Harmonix-Xbox), `0x0738`
(MadCatz). Other X360 guitars from these vendors share the report layout.

### Packet layout (XUSB / X360 guitar, xboxdrv protocol)

Input reports have `byte0 == 0x00`, `byte1 == 0x14` (other report types — LED
status `0x01`/`0x03` — are skipped):

| Field | Location | Notes |
|---|---|---|
| strum-up / dpad-up | byte2 `0x01` | → synthetic `buttons[12]` (kPad_DUp) |
| strum-down / dpad-down | byte2 `0x02` | → `buttons[13]` (kPad_DDown) |
| dpad-left | byte2 `0x04` | → `buttons[14]` |
| dpad-right | byte2 `0x08` | → `buttons[15]` |
| Start | byte2 `0x10` | → `buttons[9]` |
| Back | byte2 `0x20` | → `buttons[8]` (star power) |
| orange (LB) | byte3 `0x01` | → `buttons[4]` |
| Guide | byte3 `0x04` | → `buttons[16]` |
| green (A) | byte3 `0x10` | → `buttons[0]` |
| red (B) | byte3 `0x20` | → `buttons[1]` |
| blue (X) | byte3 `0x40` | → `buttons[2]` |
| yellow (Y) | byte3 `0x80` | → `buttons[3]` |
| whammy | int16 LE @ offset 10 | rest ≈ `-32768`, full ≈ `+32767`; normalized `(raw+32768)/65535` → `axes[2]` in `[-1..1]` so the C++ `(axes[2]+1)/2` recovers `[0..1]` |
| tilt | int16 LE @ offset 12 | active when `raw > 8192` → `axes[3] = 1.0` (C++ threshold `> 0.5`) |

The synthetic-pad button/axis indices above are exactly what the `xinput_rb`
family in `rb3_joypad_native.cpp` reads (frets on standard face buttons, strum on
buttons 12/13, whammy on `axes[2]`, tilt on `axes[3]`).

> **Whammy rest polarity** is assumed to be `raw ≈ -32768` at rest / `+32767`
> fully pressed (the common X360-guitar convention). If a real unit reads
> inverted, flip the normalization in `decodePacket()` — verify against hardware.

### Debug

- `window._rb3GpDebug = 1` → logs raw packet hex on change (prefixed
  `[rb3-guitar]`).
- `window._rb3Xplorer = { connected, lastPacket, state }` → live inspection of
  the synthetic pad.

### Test

`scripts/web/guitar-webusb-e2e-test.mjs` (Playwright) injects a fake
`navigator.usb` (X-plorer `1430:4748`, one `0xFF/0x5D` interface with an
interrupt-IN endpoint, scripted `transferIn` packets), auto-authorizes via
`getDevices()`, and asserts: auto-claim, `xinput_rb` classification, green+strum
reaching the engine chokepoint, whammy tracking, disconnect teardown, and a
no-device regression (button visible, no synthetic pad, no JS errors). Run from
`scripts/web/`:
```bash
node guitar-webusb-e2e-test.mjs --port 8421
```
