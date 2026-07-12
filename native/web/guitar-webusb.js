/*
 * guitar-webusb.js — Xbox 360 guitar (XUSB) support for the RB3 web build via
 * WebUSB, for macOS/desktop Chrome where the OS has no XInput driver.
 *
 * WHY: Xbox 360 controllers are NOT HID. They speak Microsoft's proprietary
 * XUSB protocol on a vendor-specific USB interface (class 0xFF, subclass 0x5D,
 * protocol 0x01). On macOS nothing claims that interface, so
 * navigator.getGamepads() stays all-null forever and the existing Gamepad-API
 * guitar path (native/src/rb3_joypad_native.cpp) can never see the device. But
 * because nothing else claims it, Chrome's WebUSB can open the interface raw and
 * read the interrupt-IN report stream directly.
 *
 * HOW: this page-level module claims the guitar, decodes the report packets, and
 * exposes the decoded state as a SYNTHETIC Gamepad object appended to
 * navigator.getGamepads() — shaped exactly like an xpad "standard" Xbox 360 RB
 * guitar so the existing C++ EM_ASM mapping (family "xinput_rb") consumes it with
 * ZERO C++/wasm changes. The engine polls getGamepads() every frame and picks the
 * first guitar-classified pad.
 *
 * Chrome only (WebUSB). Firefox/Safari lack navigator.usb → this file no-ops.
 * On Linux the kernel xpad driver owns the device and it already appears as a
 * normal gamepad, so claimInterface fails with NetworkError → we bow out and the
 * normal Gamepad-API path handles it.
 *
 * Debug: window._rb3GpDebug = 1 logs raw packet hex on change; window._rb3Xplorer
 * = { connected, lastPacket, state } for live inspection. Logs are prefixed
 * "[rb3-guitar]" to match the existing input layer.
 */
(function () {
  'use strict';

  var TAG = '[rb3-guitar]';

  // Feature-detect WebUSB. Silently no-op where it's unavailable (Firefox/Safari).
  if (!navigator.usb || typeof navigator.usb.requestDevice !== 'function') {
    return;
  }

  // XUSB-era guitar vendors: RedOctane/GH (0x1430), Harmonix-Xbox (0x1bad),
  // MadCatz (0x0738). We do NOT filter productId — other X360 guitars from these
  // vendors share the report layout.
  var USB_FILTERS = [
    { vendorId: 0x1430 },
    { vendorId: 0x1bad },
    { vendorId: 0x0738 }
  ];
  // Same regex the C++ classifier uses to detect an already-present real guitar.
  var GUITAR_ID_RE = /guitar|harmonix|santroller|12ba|1bad|0738|1430/i;

  // ── Synthetic "standard" gamepad ────────────────────────────────────────────
  // Shaped so the C++ classifier maps it to family "xinput_rb":
  //   id contains "1430" → isGuitar; mapping === 'standard' → xinput_rb.
  // xinput_rb reads (Gamepad-standard indices):
  //   frets: green=btn0(A) red=btn1(B) blue=btn2(X) yellow=btn3(Y) orange=btn4(LB)
  //   select/star=btn8  start=btn9
  //   strum up=btn12  strum down=btn13  dpad left=btn14  dpad right=btn15
  //   whammy = axes[2]  (C++: whammy01 = (axes[2]+1)/2, so rest=-1 full=+1)
  //   tilt   = axes[3] > 0.5  (we drive 1.0 when tilted, else 0)
  function makeButtons(n) {
    var a = [];
    for (var i = 0; i < n; i++) a.push({ pressed: false, touched: false, value: 0 });
    return a;
  }

  var syntheticPad = {
    id: 'Guitar Hero X-plorer (WebUSB) (Vendor: 1430 Product: 4748)',
    index: 0,
    mapping: 'standard',
    connected: true,
    buttons: makeButtons(17),  // indices 0..16 (Guide = 16)
    axes: [0, 0, -1, 0],       // LX, LY, whammy(rest=-1), tilt
    timestamp: 0
  };

  var padActive = false;         // is the synthetic pad currently exposed?
  var currentDevice = null;      // the claimed USBDevice
  var readRunning = false;       // read loop guard
  var reconnectInFlight = false;

  // Expose a live debug view.
  window._rb3Xplorer = {
    connected: false,
    lastPacket: null,
    state: syntheticPad
  };

  // ── getGamepads wrap (install ONCE, keep the original) ───────────────────────
  // The C++ side iterates the returned array and picks the first guitar-classified
  // pad. We append our synthetic pad when active; otherwise pass through untouched.
  var origGetGamepads = navigator.getGamepads
    ? navigator.getGamepads.bind(navigator)
    : function () { return []; };

  navigator.getGamepads = function () {
    var real = origGetGamepads() || [];
    if (!padActive) return real;
    var out = Array.prototype.slice.call(real);
    out.push(syntheticPad);
    return out;
  };

  // Is a REAL Gamepad-API guitar already present? (Uses the ORIGINAL getGamepads
  // so we don't count our own synthetic pad.)
  function realGuitarPresent() {
    var pads = origGetGamepads() || [];
    for (var i = 0; i < pads.length; i++) {
      var p = pads[i];
      if (p && p.connected && GUITAR_ID_RE.test(p.id || '')) return true;
    }
    return false;
  }

  // ── Floating connect button + status line ────────────────────────────────────
  var btn = null, statusEl = null;

  function buildUi() {
    btn = document.createElement('button');
    btn.textContent = '🎸 Connect USB guitar';
    btn.setAttribute('type', 'button');
    btn.style.cssText = [
      'position:fixed', 'right:12px', 'bottom:12px', 'z-index:9999',
      'padding:7px 12px', 'font:600 13px system-ui,sans-serif',
      'color:#eee', 'background:#2a2140', 'border:1px solid #5533ff',
      'border-radius:6px', 'cursor:pointer', 'opacity:0.85',
      'box-shadow:0 2px 8px rgba(0,0,0,0.4)'
    ].join(';');
    btn.onmouseenter = function () { btn.style.opacity = '1'; };
    btn.onmouseleave = function () { btn.style.opacity = '0.85'; };
    btn.addEventListener('click', function () {
      requestGuitar();
    });

    statusEl = document.createElement('div');
    statusEl.style.cssText = [
      'position:fixed', 'right:12px', 'bottom:48px', 'z-index:9999',
      'max-width:320px', 'padding:6px 10px', 'font:12px system-ui,sans-serif',
      'color:#ffd', 'background:rgba(20,10,10,0.9)', 'border:1px solid #a33',
      'border-radius:5px', 'display:none'
    ].join(';');

    var attach = function () {
      document.body.appendChild(btn);
      document.body.appendChild(statusEl);
      updateUi();
    };
    if (document.body) attach();
    else document.addEventListener('DOMContentLoaded', attach);
  }

  function showStatus(msg) {
    if (!statusEl) return;
    statusEl.textContent = msg;
    statusEl.style.display = 'block';
  }
  function hideStatus() {
    if (statusEl) statusEl.style.display = 'none';
  }

  // Hide the button when a WebUSB guitar is connected OR a real Gamepad-API guitar
  // is already present; show it otherwise.
  function updateUi() {
    if (!btn) return;
    var hide = padActive || realGuitarPresent();
    btn.style.display = hide ? 'none' : 'inline-block';
  }

  // ── Packet decode (XUSB / X360 guitar layout, xboxdrv protocol) ──────────────
  // Input report: byte0 == 0x00, byte1 == 0x14. Other report types (LED status
  // 0x01/0x03 etc.) are ignored.
  //   byte2: strum-up/dpad-up 0x01, strum-down/dpad-down 0x02, dpad-left 0x04,
  //          dpad-right 0x08, Start 0x10, Back 0x20
  //   byte3: orange(LB) 0x01, Guide 0x04, green(A) 0x10, red(B) 0x20,
  //          blue(X) 0x40, yellow(Y) 0x80
  //   whammy: int16 LE @ offset 10 (rest ≈ -32768, full ≈ +32767)
  //   tilt:   int16 LE @ offset 12 (active when raw > 8192)
  function decodePacket(dv) {
    if (dv.byteLength < 4) return false;
    if (dv.getUint8(0) !== 0x00 || dv.getUint8(1) !== 0x14) return false; // not an input report

    var b2 = dv.getUint8(2);
    var b3 = dv.getUint8(3);

    var strumUp   = (b2 & 0x01) !== 0;
    var strumDown = (b2 & 0x02) !== 0;
    var dpadLeft  = (b2 & 0x04) !== 0;
    var dpadRight = (b2 & 0x08) !== 0;
    var start     = (b2 & 0x10) !== 0;
    var back      = (b2 & 0x20) !== 0;

    var orange = (b3 & 0x01) !== 0;
    var guide  = (b3 & 0x04) !== 0;
    var green  = (b3 & 0x10) !== 0;
    var red    = (b3 & 0x20) !== 0;
    var blue   = (b3 & 0x40) !== 0;
    var yellow = (b3 & 0x80) !== 0;

    // Whammy int16 LE @ 10 → [0..1], rest = 0. Guard length for short reports.
    var whammy01 = 0;
    if (dv.byteLength >= 12) {
      var wRaw = dv.getInt16(10, true);
      whammy01 = (wRaw + 32768) / 65535;
      if (whammy01 < 0) whammy01 = 0;
      if (whammy01 > 1) whammy01 = 1;
    }
    // Tilt int16 LE @ 12 → active when > 8192.
    var tilt = false;
    if (dv.byteLength >= 14) {
      var tRaw = dv.getInt16(12, true);
      tilt = tRaw > 8192;
    }

    var B = syntheticPad.buttons;
    var setBtn = function (i, v) { B[i].pressed = v; B[i].value = v ? 1 : 0; };
    // Frets → Gamepad-standard face buttons (xinput_rb indices).
    setBtn(0, green);   // A
    setBtn(1, red);     // B
    setBtn(2, blue);    // X
    setBtn(3, yellow);  // Y
    setBtn(4, orange);  // LB
    setBtn(8, back);    // Back → select / star power
    setBtn(9, start);   // Start
    // Strum + d-pad → buttons 12..15 (xinput_rb "buttons" strum mode).
    setBtn(12, strumUp);
    setBtn(13, strumDown);
    setBtn(14, dpadLeft);
    setBtn(15, dpadRight);
    setBtn(16, guide);  // Guide

    // Whammy → axes[2] in [-1..1] so the C++ (axes[2]+1)/2 recovers [0..1].
    syntheticPad.axes[2] = whammy01 * 2 - 1;
    // Tilt → axes[3]: 1.0 when tilted, else 0 (C++ threshold is > 0.5).
    syntheticPad.axes[3] = tilt ? 1.0 : 0.0;

    syntheticPad.timestamp = (typeof performance !== 'undefined' && performance.now)
      ? performance.now() : Date.now();

    // Debug hooks.
    window._rb3Xplorer.state = syntheticPad;
    if (window._rb3GpDebug) {
      var hex = [];
      for (var i = 0; i < Math.min(dv.byteLength, 20); i++) {
        var h = dv.getUint8(i).toString(16);
        hex.push(h.length < 2 ? '0' + h : h);
      }
      var sig = hex.join('');
      if (sig !== window._rb3XplorerLastHex) {
        window._rb3XplorerLastHex = sig;
        console.log(TAG + ' xplorer packet ' + hex.join(' ') +
          ' whammy=' + whammy01.toFixed(2) + ' tilt=' + (tilt ? 1 : 0));
      }
    }
    return true;
  }

  // ── Connection lifecycle ─────────────────────────────────────────────────────
  function dispatchGamepadEvent(type) {
    try {
      var ev = new Event(type);
      Object.defineProperty(ev, 'gamepad', { value: syntheticPad });
      window.dispatchEvent(ev);
    } catch (e) { /* older engines: poll still works, event is only for the log */ }
  }

  function activatePad() {
    if (padActive) return;
    // Pick a slot at/after the real pads; collisions are harmless (we append).
    var real = origGetGamepads() || [];
    var n = 0;
    for (var i = 0; i < real.length; i++) if (real[i]) n++;
    syntheticPad.index = n;
    syntheticPad.connected = true;
    padActive = true;
    window._rb3Xplorer.connected = true;
    dispatchGamepadEvent('gamepadconnected');
    hideStatus();
    updateUi();
  }

  function deactivatePad() {
    if (!padActive) return;
    padActive = false;
    window._rb3Xplorer.connected = false;
    // Neutralize state so a stale read can't leave a fret stuck.
    for (var i = 0; i < syntheticPad.buttons.length; i++) {
      syntheticPad.buttons[i].pressed = false;
      syntheticPad.buttons[i].value = 0;
    }
    syntheticPad.axes[2] = -1;
    syntheticPad.axes[3] = 0;
    dispatchGamepadEvent('gamepaddisconnected');
    updateUi();
  }

  // Find the first interrupt-IN endpoint across interfaces/alternates. Expected
  // result is iface 0 / EP 1, but we discover it rather than hardcode.
  function findInterruptIn(device) {
    var cfg = device.configuration;
    if (!cfg) return null;
    for (var i = 0; i < cfg.interfaces.length; i++) {
      var itf = cfg.interfaces[i];
      for (var a = 0; a < itf.alternates.length; a++) {
        var alt = itf.alternates[a];
        for (var e = 0; e < alt.endpoints.length; e++) {
          var ep = alt.endpoints[e];
          if (ep.direction === 'in' && ep.type === 'interrupt') {
            return {
              interfaceNumber: itf.interfaceNumber,
              alternateSetting: alt.alternateSetting,
              endpointNumber: ep.endpointNumber,
              packetSize: ep.packetSize || 32
            };
          }
        }
      }
    }
    return null;
  }

  async function claimAndRead(device) {
    await device.open();
    if (device.configuration === null || device.configuration === undefined) {
      await device.selectConfiguration(1);
    }
    var epInfo = findInterruptIn(device);
    if (!epInfo) {
      showStatus('No interrupt-IN endpoint found on this device.');
      try { await device.close(); } catch (e) {}
      return;
    }
    try {
      await device.claimInterface(epInfo.interfaceNumber);
    } catch (e) {
      // NetworkError here means SOMETHING ELSE holds the device — most often
      // another browser tab (WebUSB claims are exclusive per device), sometimes
      // another app, or on Linux the kernel xpad driver (where the guitar
      // already works as a normal gamepad, no WebUSB needed).
      var name = (e && e.name) || '';
      var msg = (e && e.message) || '';
      showStatus('Could not claim the guitar (' + name + (msg ? ': ' + msg : '') + '). ' +
        'Close any other tab or app using it (game tabs, gamepad testers), ' +
        'unplug/replug, then retry. On Linux, skip this button — the guitar ' +
        'already works as a normal gamepad.');
      console.log(TAG + ' claimInterface failed: ' + name + ' — ' + msg +
        ' (exclusive claim: check for other tabs/apps holding the device)');
      try { await device.close(); } catch (e2) {}
      return;
    }
    if (epInfo.alternateSetting) {
      try { await device.selectAlternateInterface(epInfo.interfaceNumber, epInfo.alternateSetting); }
      catch (e) { /* many guitars have only alt 0 */ }
    }

    currentDevice = device;
    readRunning = true;
    console.log(TAG + ' X-plorer claimed via WebUSB (iface ' + epInfo.interfaceNumber +
      ', EP ' + epInfo.endpointNumber + ', ' + epInfo.packetSize + 'B) — synthetic pad live');
    activatePad();

    // Read loop.
    while (readRunning && device.opened) {
      var result;
      try {
        result = await device.transferIn(epInfo.endpointNumber, epInfo.packetSize);
      } catch (e) {
        // Device unplugged / transfer aborted.
        console.log(TAG + ' transferIn ended: ' + (e && e.message ? e.message : e));
        break;
      }
      if (result.status === 'stall') {
        try { await device.clearHalt('in', epInfo.endpointNumber); } catch (e) {}
        continue;
      }
      if (result.status === 'ok' && result.data && result.data.byteLength >= 4) {
        var pkt = result.data;
        window._rb3Xplorer.lastPacket = pkt;
        decodePacket(pkt);
      }
    }

    readRunning = false;
    if (currentDevice === device) currentDevice = null;
    deactivatePad();
    try { if (device.opened) await device.close(); } catch (e) {}
  }

  function deviceMatchesFilter(device) {
    for (var i = 0; i < USB_FILTERS.length; i++) {
      if (device.vendorId === USB_FILTERS[i].vendorId) return true;
    }
    return false;
  }

  async function attachDevice(device) {
    if (!device || currentDevice) return;
    try {
      await claimAndRead(device);
    } catch (e) {
      console.log(TAG + ' attach failed: ' + (e && e.message ? e.message : e));
      showStatus('Failed to open the guitar: ' + (e && e.message ? e.message : e));
    }
  }

  // User-gesture path: prompt the Chrome device chooser.
  async function requestGuitar() {
    hideStatus();
    var device;
    try {
      device = await navigator.usb.requestDevice({ filters: USB_FILTERS });
    } catch (e) {
      // NotFoundError = user cancelled the chooser; not worth a scary message.
      if (e && e.name === 'NotFoundError') return;
      showStatus('Device selection failed: ' + (e && e.message ? e.message : e));
      return;
    }
    await attachDevice(device);
  }

  // Auto-reconnect: on load, re-attach to any previously-authorized matching
  // device WITHOUT a gesture (one-click-once, automatic thereafter).
  async function autoReconnect() {
    if (reconnectInFlight) return;
    reconnectInFlight = true;
    try {
      var devices = await navigator.usb.getDevices();
      for (var i = 0; i < devices.length; i++) {
        if (deviceMatchesFilter(devices[i])) {
          await attachDevice(devices[i]);
          break;
        }
      }
    } catch (e) {
      // getDevices can reject in some sandboxed contexts; non-fatal.
    } finally {
      reconnectInFlight = false;
    }
  }

  // Replug handling.
  navigator.usb.addEventListener('connect', function (e) {
    if (e.device && deviceMatchesFilter(e.device) && !currentDevice) {
      attachDevice(e.device);
    }
  });
  navigator.usb.addEventListener('disconnect', function (e) {
    if (e.device && e.device === currentDevice) {
      readRunning = false;   // read loop will unwind and deactivate the pad
    }
  });

  // ── Boot ─────────────────────────────────────────────────────────────────────
  buildUi();
  autoReconnect();
  // Keep the button's visibility in sync as real gamepads come and go.
  setInterval(updateUi, 1000);

  console.log(TAG + ' WebUSB X-plorer support ready (Chrome desktop; macOS XUSB path)');
})();
