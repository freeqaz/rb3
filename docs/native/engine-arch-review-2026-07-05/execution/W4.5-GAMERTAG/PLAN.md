# W4.5-GAMERTAG PLAN (Wave 15, Lane N)

**Acceptance (A5/A6, binding):** the song_select header "(null)" is the
`PlatformMgr::GetName` weak NULL stub (`native/src/dta_link_stubs.s`) consumed
via `AppLabel::SetUserName`/`user_name.lbl`. Wii `PlatformMgr_Wii.cpp:489-496`
falls back to localized "Player N" when not signed in. Implement THAT fallback
in `native/src/rb3_platform_native.cpp` (one strong override fixes every
consumer). Hiding the label is REJECTED (unfaithful).

## Trace (confirmed against source, matches WAVE15_REVIEW.md A6)

- Consumers: overshell plate `user_name.lbl` (`OvershellSlot.cpp:96`, via
  `update_user_name` msg) and the DTA header route (`AppLabel::OnSetUserName`
  -> `AppLabel.cpp:356-369`, `set_user_name` handler `:759` — the song_select
  header stats.grp gamertag element, `SongSelectPanel.cpp:106`).
- Both converge on `AppLabel::SetUserName` -> `User::UserName()` /
  `ThePlatformMgr.GetName(i)` (`AppLabel.cpp:159,161`; `LocalUser::UserName`,
  `User.cpp:107`); also `SessionUsersProviders.cpp:103`,
  `Track::SetUserNameLabel`.
- Native: `PlatformMgr::GetName(int)` resolves to the weak no-op stub in
  `dta_link_stubs.s` (`_ZNK11PlatformMgr7GetNameEi`, returns NULL) because the
  real Wii-SDK-bound impl (`PlatformMgr_Wii.cpp`) isn't compiled natively.
  `SetDisplayText(NULL, true)` formats the glibc `"(null)"` string.
- Wii fallback (`PlatformMgr_Wii.cpp:489-496`): when not signed in (or no
  profile name — always true natively, no profile subsystem), returns
  `MakeString("%s %d", Localize(player, 0), pad + 1)` = localized "Player N".

## Plan

1. Add a strong `PlatformMgr::GetName(int)` definition in
   `native/src/rb3_platform_native.cpp` (`#ifdef HX_NATIVE`) that ports the Wii
   fallback body verbatim (it wins the weak/strong link — same mechanism as
   `PlatformMgr::PlatformMgr()` already in that file).
2. Flag-gate `RB3_PLAYER_NAME_FALLBACK` (presence-mode, default-OFF): unset ->
   `nullptr` (byte-identical to today's stub); set -> the Wii fallback body.
3. Register the flag in `NativeCompatFlags.classification.json` (engine repo,
   append-only).
4. Verify: song_select header + overshell plate both read "Player 1" flag-ON;
   both read "(null)" flag-OFF (unchanged); drawlog 792 canonical order in
   both arms; E1 screenshot capture.
