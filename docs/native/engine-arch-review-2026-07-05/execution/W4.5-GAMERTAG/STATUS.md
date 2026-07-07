# W4.5-GAMERTAG STATUS (Wave 15, Lane N)

## Outcome: FIXED, flag-first default-OFF

`PlatformMgr::GetName(int)` now has a strong native override in
`native/src/rb3_platform_native.cpp` (`#ifdef HX_NATIVE`) that ports the Wii
"not signed in" fallback body verbatim from `PlatformMgr_Wii.cpp:489-496`:

```cpp
const char *PlatformMgr::GetName(int pad) const {
    static int enabled = -1;
    if (enabled < 0)
        enabled = ::getenv("RB3_PLAYER_NAME_FALLBACK") ? 1 : 0;
    if (!enabled)
        return nullptr;
    return MakeString("%s %d", Localize(player, 0), pad + 1);
}
```

This wins over the weak NULL-returning stub in `native/src/dta_link_stubs.s`
(`_ZNK11PlatformMgr7GetNameEi`) the same way `PlatformMgr::PlatformMgr()`
already does in the same file.

## Why one fix covers every consumer

Both label routes converge on `ThePlatformMgr.GetName(pad)`:
- Overshell player plate: `OvershellSlot.cpp:96` (`user_name.lbl`, via
  `update_user_name` message).
- song_select header: `AppLabel::OnSetUserName` (`AppLabel.cpp:356-369`,
  `set_user_name` handler `:759`) — the header's `stats.grp` gamertag element
  (`SongSelectPanel.cpp:106`).
- Both go through `AppLabel::SetUserName` -> `User::UserName()` /
  `ThePlatformMgr.GetName(i)` (`AppLabel.cpp:159,161`; `LocalUser::UserName`,
  `User.cpp:107`); `SessionUsersProviders.cpp:103` and
  `Track::SetUserNameLabel` are the same call.

So the single strong `GetName` definition fixes the header, the overshell
plate, and every other consumer at once — no per-label patch needed.

## Flag semantics

`RB3_PLAYER_NAME_FALLBACK`, presence-mode, default-OFF:
- **Unset** (default): `GetName` returns `nullptr` — identical to the weak
  stub's behavior today. Byte-identical flag-OFF.
- **Set**: returns the Wii not-signed-in fallback string, `"Player N"`
  (`Localize(player, 0)` + `pad + 1`). `Localize()` degrades to the literal
  token string `"player"` when the locale table has no translation for the
  `player` symbol (`Locale.cpp`), so the "localized token if available,
  literal fallback otherwise" gate requirement is satisfied without any extra
  fallback logic in this fix.

Registered in `milo-native-engine/src/platform/NativeCompatFlags.classification.json`
(`RB3_PLAYER_NAME_FALLBACK`, class `feature`, owner `ui/profile`, default
`off`, read `presence`) — append-only edit, no `.gen.inc` regen (that's the
coordinator's end-of-wave job across all Wave 15 lanes).

## Verification (rb3-native, headless, `RB3_HTTP=1 RB3_FIXED_CLOCK=1`)

Built in an isolated build dir (`native/build-agent-W4.5-GAMERTAG`), booted to
the song_select ("Music Library") screen with a free HTTP port per run,
screenshot via `/api/screenshot`, settled by frame count, pgid-only cleanup.

- **flag-ON** (`RB3_PLAYER_NAME_FALLBACK=1`): header top-right and overshell
  bottom-left both read **"Player 1"**.
  `evidence/song_select_header_ON_player1.png`
- **flag-OFF** (unset): header and overshell both read **"(null)"**
  (unchanged from pre-fix behavior).
  `evidence/song_select_header_OFF_null.png`
- **drawlog 792**: both logs show `RB3 Native: frame 792 complete` at the
  canonical frame boundary (`/tmp/w45-gamertag-on.log`,
  `/tmp/w45-gamertag-off.log` — not checked in, ephemeral harness logs).
- Build: clean (`native/build-agent-W4.5-GAMERTAG`, `rb3-native` linked OK).

## Gates (all met)

- [x] song_select header shows "Player 1" flag-ON, "(null)" gone.
- [x] Overshell player plate checked — same fix, same result.
- [x] No other `GetName` consumer left unfixed (single provider).
- [x] flag-OFF byte-identical (returns `nullptr`, same as the stub).
- [x] drawlog 792 canonical order, both arms.
- [x] E1 capture (screenshots above).

## Files touched (this lane only)

- rb3: `native/src/rb3_platform_native.cpp` (strong `PlatformMgr::GetName`
  override + comment block).
- engine: `src/platform/NativeCompatFlags.classification.json` (append
  `RB3_PLAYER_NAME_FALLBACK` entry).

Not touched by this lane (left as found — other lanes' concurrent work in the
shared tree): `native/src/rb3_session_trace.cpp`, `src/system/ui/UIListWidget.cpp`,
`docs/native/engine-arch-review-2026-07-05/NATIVE_COMPAT_LEDGER.md` (stale
multi-lane regen, not re-run here — coordinator's job), engine
`src/platform/FxSendNative.cpp`, `.rowfix/` scratch dir.
