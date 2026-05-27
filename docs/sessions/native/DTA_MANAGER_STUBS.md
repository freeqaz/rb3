# RB3 native — DTA-manager stubs + UI-init bypasses (spec for T4)

> READ-ONLY research artifact. This is the spec for task **T4** in
> [BOOT_TO_SONG.md](BOOT_TO_SONG.md). It enumerates every DTA-referenced manager
> on the boot-to-main-menu path, classifies each as a real RB3 singleton vs a
> needed stub, gives a ready-to-adapt `App.cpp` draft, and specifies the
> UI-init bypass targets. Cites `file:line` throughout. All paths are absolute
> from the `rb3/` repo root unless noted.

---

## TL;DR — the headline finding (differs sharply from DC3)

DC3 registered ~6 **bare `Hmx::Object` smart-stubs** by name (`saveload_mgr`,
`profile_mgr`, `platform_mgr`, `content_mgr`, `challenges`, `speech_mgr`) into
`ObjectDir::Main()` because DC3's boot does *not* construct those Xbox managers
natively (`dc3 App.cpp:75-196` defs, `:505-520` registration).

**RB3 is the opposite.** On RB3, **essentially every `*_mgr` object the boot
DTAs reference is a REAL C++ singleton that registers itself by name** during the
existing `App::App` init spine (`src/App.cpp:117-307`) — via `BandUserMgrInit()`,
`MetaPanel::Init()` (the hub, `src/App.cpp:248`), `SessionMgr::Init()`,
`BandUI::Init()` (= `TheUI.Init()`, `src/App.cpp:272`), and the `MetaPanel`
ctor. None of them are platform stubs.

Therefore the RB3 native strategy is **not** "register fake named objects." It
is: **let the real singletons construct, and gate the *platform/online/Wii*
subsystems they depend on under `#ifndef HX_NATIVE` so they answer DTA with
safe single-player/offline defaults.** The real managers already produce the
right answers when net is idle — e.g. `session_mgr is_local` (the most-called
DTA message, 11×) bottoms out at `NetSession::IsLocal()`
(`src/network/net/NetSession.cpp` IsLocal) which returns **true** when
`mState==kIdle && !mOnlineEnabled` — exactly the native-disconnected state.

The only objects in this set that have **no constructing call on the current
native link** are the ones inside the `#ifndef HX_NATIVE`-gated subsystems
(`session`/`NetSession`, `TheNet`, `TheRockCentral`, `TheEntityUploader`,
Wii `content_mgr`, `WiiProfileMgr`, `GameMicManager`). Those become **either**
(a) brought-up-real once their subsystem is gated-but-still-constructed, **or**
(b) a thin native stub if the subsystem is excluded from the link. See
[§4 stub decision](#4-the-handful-of-genuine-stub-candidates) — the candidate
list is far smaller than DC3's.

---

## 1. Boot path scanned

The `intro_movie → splash → main_hub` path. DTAs scanned (all under
`orig-assets/extracted/`):

| DTA | role |
|-----|------|
| `ui/ui.dta` | top-level `(init …)` script + `goto_screen $first_screen` |
| `ui/init.dta` | `#include` chain of every screen DTA + `net_cache_mgr init` + `platform_mgr set_notify_ui_location` |
| `ui/band_ui.dta` | `ui_event_mgr` dialog/transition event table (error dialogs, saveload dialog, sign-in) |
| `ui/splash/splash.dta` | `intro_movie_screen` + `splash_screen` boot state machine (saveload/overshell/profile handshake) |
| `ui/main/main.dta` | credits screens (off boot path, scanned for completeness) |
| `ui/main/main_hub.dta` | `main_hub_panel` — the destination; heaviest manager user |
| `config/band_*.dta` | `band_preinit_keep.dta` / `band_keep.dta` — system config, no manager *message* refs (object typedefs) |

Boot ordering of manager references that actually fire before/at main hub:

1. `ui.dta:37-39` `{net_cache_mgr init {find $syscfg store netcache_init}}`
2. `splash.dta` (intro→splash): `{platform_mgr disable_xmp}`, `{session clear}`,
   `{saveload_mgr activate}`/`{saveload_mgr is_idle}`, `{overshell …}`,
   `{profile_mgr set_primary_profile_by_user …}`,
   `{profile_mgr get_has_seen_first_time_calibration}`, `{input_mgr set_user}`,
   `{critical_user_listener set_critical_user}`
3. `main_hub.dta enter` (`main_hub.dta:71-103`): `{profile_mgr set_primary_profile_by_user}`,
   `{input_mgr clear_user}`, `{closet_mgr clear_user}`,
   `{critical_user_listener clear_critical_user}`, `{gamemode set_mode init}`,
   `{net_sync set_ui_state kNetUI_MainMenu}`, `{platform_mgr add_sink $this}`,
   `{input_mgr set_auto_vocals_confirm_allowed}`, `{overshell set_allow_real_guitar_flow}`,
   `{overshell update_all}`, `{content_mgr start_refresh}`, `{campaign clear_current_goal}`

---

## 2. Manager reference table (full)

**Legend**
- **Kind**: `REAL` = RB3 C++ singleton that `SetName`s itself; `REAL†` = real but
  lives inside a `#ifndef HX_NATIVE`-gated subsystem (constructs only if subsystem
  kept); `STUB?` = stub candidate iff its subsystem is excluded from the link.
- **Reg site** = where it gets its name (`SetName(...)`), and what App.cpp call
  reaches it.

| DTA name | Kind | Reg site (`SetName`) — reached from | Boot-path messages it must answer | Native smart-default |
|----------|------|--------------------------------------|------------------------------------|----------------------|
| `net_cache_mgr` | REAL† | `src/system/utl/NetCacheMgr.cpp:35` (ctor `SetName("net_cache_mgr", sMainDir)`); Wii subclass `NetCacheMgrWii`. Constructed by the net/store subsystem. | `init {…netcache_init}` (`ui.dta:37`); handler `NetCacheMgr.cpp:97 HANDLE_ACTION(init, OnInit(...))` | `init`→no-op OK (offline). `IsReady`/`IsDoneLoading`→ already returns `1` (`NetCacheMgr.cpp:62`). |
| `platform_mgr` | REAL | `src/system/os/PlatformMgr_Wii.cpp:169` `SetName("platform_mgr", sMainDir)`. `ThePlatformMgr` is a static; alive on native. | `disable_xmp`, `set_notify_ui_location`, `add_sink`/`remove_sink`, `guide_showing`, `signin`, `is_user_signed_into_live`, `is_user_a_guest` | Real PlatformMgr_Wii answers; `guide_showing`→0, `is_user_signed_into_live`→0 offline. `add_sink`/`remove_sink` real (MsgSource). |
| `saveload_mgr` | REAL | `src/band3/meta_band/SaveLoadManager.cpp:65` `SetName("saveload_mgr", sMainDir)`; `SaveLoadManager::Init()` @ `src/App.cpp:236` | `activate`, `is_idle`, `is_initial_load_done`, `autosave`, `is_autosave_enabled`, `get_dialog_*`, `handle_eventresponse*`, `add_sink`/`remove_sink` | Real mgr. Native concern: `activate`→`is_idle` must reach idle so `splash.dta:99-102` advances. Verify SaveLoadManager reaches idle with no Wii memcard (see §4). |
| `profile_mgr` | REAL | `src/band3/meta_band/ProfileMgr.cpp:95` `SetName("profile_mgr", Main())`; `TheProfileMgr.Init()` @ `MetaPanel.cpp:249` (← `App.cpp:248`) | `set_primary_profile_by_user`, `has_primary_profile`, `get_primary_profile`, `get_profile`, `get_has_seen_first_time_calibration`, `get_*_volume`, audio/option getters | Real mgr (`ProfileMgr.cpp:1486` handlers). With no signed-in profile: `has_primary_profile`→false, `get_profile`→0 — DTA already guards on these (`main_hub.dta:122,154,332`). |
| `session` | REAL† | `src/network/net/NetSession.cpp:94` `SetName("session", Main())`; `TheNetSession` set @ `NetSession.cpp:118`. Constructed by `TheNet.Init()` (`App.cpp:216`, gated). | `clear`, `is_local`, `is_in_game`, `num_users`, `end_game`, `disconnect`, `add_sink`/`remove_sink` | Real NetSession. Offline: `is_local`→**true** (`NetSession.cpp` IsLocal, `mState==kIdle && !mOnlineEnabled`). Keep this subsystem constructing (see §4). |
| `session_mgr` | REAL | `src/band3/meta_band/SessionMgr.cpp:51` `SetName("session_mgr", Main())`; `SessionMgr::Init()` @ `MetaPanel.cpp:247`. Ctor also creates `critical_user_listener` + `machine_mgr`. | `is_local` (11×), `is_leader_local` (6×), `disconnect`, `default_ranked_match`, `add_local_user`, `set_active_roster`, `get_leader_user`, `is_busy` | Real mgr; `IsLocal`→`mSession->IsLocal()`→true offline (`SessionMgr.cpp:146`). `is_leader_local`→true single-machine (`SessionMgr.cpp:218`). |
| `overshell` | REAL | `src/band3/meta_band/OvershellPanel.cpp:69` `Synchronizable("overshell")`; created via DTA `{overshell load TRUE}` (`ui.dta:18`) and grabbed `OvershellPanel.cpp:119` / `BandUI.cpp:129`. | `set_active_status`, `update_all`, `attempt_to_add_user`, `set_allow_real_guitar_flow`, `is_full`, `add_sink`/`remove_sink`, `get_slot` | Real panel (`OvershellPanel.cpp:1545-1582` handlers). Drives the splash add-user handshake — must work; see §5 splash flow. |
| `profile`/`get_primary_profile` returns a `Profile` obj | REAL | `BandProfile`; only reachable when a profile exists | (used by main_hub for char counts) | n/a offline — guarded. |
| `input_mgr` | REAL | `src/band3/meta_band/InputMgr.cpp:32` `SetName("input_mgr", Main())`; `InputMgr::Init()` @ `BandUI.cpp:73` (← `App.cpp:272`). | `set_user`, `clear_user`, `set_auto_vocals_confirm_allowed`, `check_trigger_auto_vocals_confirm` | Real mgr. No special native default. |
| `ui_event_mgr` | REAL | `src/band3/meta_band/UIEventMgr.cpp:80` `SetName`; `UIEventMgr::Init()` @ `BandUI.cpp:72`. Also `SetTypeDef(SystemConfig("ui","ui_event_mgr"))` (`UIEventMgr.cpp:81`) — its dialog/event table comes from `band_ui.dta:29`. | `trigger_event`, `dismiss_dialog_event`, `has_transition_event`, `has_active_dialog_event`, `current_*_event` | Real mgr. Boot enter checks `has_transition_event go_to_charactercreator` (`main_hub.dta:82`) → false initially. |
| `net_sync` | REAL | `src/band3/meta_band/NetSync.cpp:54` `SetName("net_sync", Main())`; `NetSync::Init()` @ `BandUI.cpp:71`. | `set_ui_state`, `disable`, `enable`, `is_enabled`, `get_ui_state` | Real mgr (`NetSync.cpp:338-344`). `set_ui_state kNetUI_MainMenu` is a plain state set — works offline. |
| `critical_user_listener` | REAL | `src/band3/meta_band/CriticalUserListener.cpp:16` `SetName`; constructed by `SessionMgr` ctor `SessionMgr.cpp:47`. | `set_critical_user`, `clear_critical_user`, `get_critical_user` | Real mgr. |
| `user_mgr` | REAL | `src/system/os/UserMgr.cpp:13` `UserMgr()` ctor `SetName("user_mgr", sMainDir)`; `TheBandUserMgr` is a `BandUserMgr : UserMgr`, created in `BandUserMgrInit()` @ `App.cpp:214` (`BandUserMgr.cpp:34`). | `get_user_from_pad_num`, `get_user_from_slot`, `get_num_participants`, `get_num_local_participants` | Real mgr; pad→user resolution from real Joypad. |
| `gamemode` | REAL | `src/band3/game/GameMode.cpp:16` `SetName("gamemode", sMainDir)`; `GameModeInit()` @ `MetaPanel.cpp:244`. | `set_mode` (e.g. `init`, `practice`, `tour`, `campaign`) | Real mgr; pure state machine over `config/modes.dta`. |
| `content_mgr` | REAL† | `src/system/os/ContentMgr.cpp:14` `SetName("content_mgr", Main())`; `TheContentMgr=&TheWiiContentMgr` static initer (`ContentMgr_Wii.cpp:72-74`), `WiiContentMgr::Init()` (`ContentMgr_Wii.cpp:538`). | `start_refresh`, `refresh_done`, `never_refreshed`, `add_sink` | Real mgr (`ContentMgr.cpp:277-286`). `start_refresh`→`StartRefresh()`; `refresh_done`→`RefreshDone()`. Native: WiiContentMgr enumerates installed content; with no NAND content it should report refresh-done. Confirm offline path (§4). |
| `acc_mgr` | REAL | `src/band3/meta_band/AccomplishmentManager.cpp:114` `SetName("acc_mgr", Main())`; `new AccomplishmentManager()` from `Campaign` ctor (`Campaign.cpp:44`); `Init()` @ `AccomplishmentManager.cpp:260`. | `has_new_reward_vignettes`, `has_new_awards`, `has_completed_goal`, `get_name_for_first_new_reward_vignette`, `clear_*` | Real mgr (`AccomplishmentManager.cpp:1896-1904`). With no profile: `has_new_*`→false; main_hub `check_rewards_and_hints` (`main_hub.dta:708`) falls through to `show_hint hint_rb3_welcome_screen`. |
| `modifier_mgr` | REAL | `src/band3/meta_band/ModifierMgr.cpp:29` `SetName("modifier_mgr", sMainDir)`; `ModifierMgr::Init()` @ `MetaPanel.cpp:245`. | `is_modifier_active`, `toggle_modifier_enabled`, `is_modifier_delayed_effect` | Real mgr. |
| `machine_mgr` | REAL | `src/band3/meta_band/BandMachineMgr.cpp:142` `SetName("machine_mgr", Main())`; `BandMachineMgr::Init()` @ `SessionMgr.cpp:38`; also instance in `SessionMgr.cpp:46`. | `all_machines_have_same_net_ui_state` | Real mgr; single-machine→true. |
| `closet_mgr` | REAL | `src/band3/meta_band/ClosetMgr.cpp:34` `SetName`; `ClosetMgr::Init()` @ `App.cpp:267`. | `set_user`, `clear_user`, `set_return_screen` | Real mgr. |
| `training_mgr` | REAL | `src/band3/meta_band/TrainingMgr.cpp:24` `SetName`; `TrainingMgr::Init()` @ `App.cpp:268`. | `set_user`, `set_return_info`, `set_minimum_difficulty`, `participate_users` | Real mgr. Off main-hub-boot path (training entry only). |
| `song_mgr` | REAL | `src/band3/meta_band/BandSongMgr.cpp:62` `SetName("song_mgr", sMainDir)`; `TheSongMgr.Init()` @ `App.cpp:247`. | `get_max_song_count` (only on boot path) | Real mgr. |
| `prefab_mgr` | REAL | `src/band3/meta_band/PrefabMgr.cpp:113` `SetName`; `PrefabMgr::Init()` @ `App.cpp:263`. | `load_portraits`, `unload_portraits` | Real mgr (off boot path). |
| `campaign` | REAL | `src/band3/meta_band/Campaign.cpp:48` `SetName("campaign", Main())`; `new Campaign(...)` in `MetaPanel` ctor (`MetaPanel.cpp:269`). | `get_campaign_level`, `is_last_campaign_level`, `clear_current_goal`, `has_hints_to_show` | Real mgr (config-driven from `config/campaign.dta`). |
| `music_library` | REAL | `src/band3/meta_band/MusicLibrary.cpp:182` `SetName`; `MusicLibrary::Init()` in `MetaPanel` ctor (`MetaPanel.cpp:274`). | `start_in_setlist_browser` (song-select entry, not boot) | Real mgr (relevant to T8). |
| `meta_performer` | REAL | `src/band3/meta_band/MetaPerformer.cpp:158` `SetName(cc, sMainDir)`; `MetaPerformer::Init()` @ `MetaPanel.cpp:250`. | `are_credits_pending` (via `check_rewards_and_hints`) | Real mgr. |

**Not a manager but referenced** (engine globals, already real): `ui` =
`TheBandUI` (`BandUI.cpp:45`), `taskmgr`, `synth`, `find`/`elem`/`switch` DTA
funcs, `$syscfg` = the system config DataArray.

### What changed vs DC3
DC3's 6 named smart-stubs map onto RB3 as: `saveload_mgr`→REAL (SaveLoadManager),
`profile_mgr`→REAL (ProfileMgr), `platform_mgr`→REAL (PlatformMgr_Wii),
`content_mgr`→REAL (WiiContentMgr), `challenges`→**not referenced on RB3 boot
path** (RB3 uses `acc_mgr`/`campaign` instead), `speech_mgr`→**no RB3
equivalent** (no Kinect/voice on Wii boot). So the DC3 registration block does
**not** port 1:1 — most of it disappears because the real RB3 singletons exist.

---

## 3. The registration is already done — no new `ObjectDir::Main()` block needed (default case)

Because every name above is bound by an existing `Init()` on the App spine, the
**preferred** T4 implementation registers **zero** new named objects. The work is
in T2 (`#ifndef HX_NATIVE` gating in `App.cpp`) keeping those `Init()` calls
reachable. Confirm at runtime, after boot, that each name resolves:

```cpp
// Native sanity check (TEMP, remove after verification) — drop after TheUI.Init().
#ifdef HX_NATIVE
for (const char *n : {"net_cache_mgr","platform_mgr","saveload_mgr","profile_mgr",
                      "session","session_mgr","overshell","input_mgr","ui_event_mgr",
                      "net_sync","critical_user_listener","user_mgr","gamemode",
                      "content_mgr","acc_mgr","modifier_mgr","machine_mgr",
                      "closet_mgr","training_mgr","song_mgr","prefab_mgr",
                      "campaign","music_library"}) {
    if (!ObjectDir::Main()->FindObject(n, false, false))
        MILO_WARN("RB3 native: DTA manager '%s' NOT registered after init", n);
}
#endif
```

Any name this flags is one whose constructing `Init()` got excluded by a T1/T2
link/gate decision → that one becomes a §4 stub.

---

## 4. The handful of genuine stub candidates (only if their subsystem is excluded)

These are the `REAL†` rows: real classes that only construct if their
**platform/net subsystem** stays on the link. T1/T2 decide that. Two outcomes:

- **(A) Keep the subsystem constructing (preferred — least divergence).** Gate
  only the *Wii/online side-effects* inside the class under `#ifndef HX_NATIVE`,
  leave the object + its name + offline defaults. `session`/`NetSession` already
  returns the right offline answers; `net_cache_mgr` already returns
  ready/done; `content_mgr` (WiiContentMgr) needs its NAND-content enumeration
  to short-circuit to "refresh done" on native.

- **(B) Exclude the subsystem → register a thin stub.** Only if `TheNet`/
  `TheRockCentral`/`TheEntityUploader`/`NetSession`/`WiiContentMgr`/`WiiProfileMgr`
  are dropped from the native link entirely. Then mirror DC3's pattern.

### Verification items for outcome (A) (do during T4 bring-up)
1. `saveload_mgr`: `splash.dta:96-102` polls `{saveload_mgr is_idle}` after
   `{saveload_mgr activate}`; the splash state machine **blocks** at
   `kSplashScreen_ActivateSaveLoad` until idle. Confirm `SaveLoadManager` reaches
   idle with no Wii memcard. (`SaveLoadManager.cpp` Poll/state.) If it stalls,
   the HX_NATIVE fix is to force idle, mirroring DC3 `NativeSaveLoadStub`
   `is_idle→1` (`dc3 App.cpp:83`).
2. `content_mgr`: `main_hub.dta:102` `{content_mgr start_refresh}`. Confirm
   `WiiContentMgr::StartRefresh()` completes / `refresh_done`→true with no NAND
   content (`ContentMgr_Wii.cpp:538+`). Boot does not block on this (fire-and-
   forget sink), but a hang in refresh would stall later content.
3. `session`/`session_mgr`: confirm `IsLocal`→true offline (already verified:
   `NetSession::IsLocal` returns true at `mState==kIdle`). No fix expected.

### Stub draft for outcome (B) — adapt only the names that §3's check flags

If (and only if) a subsystem is excluded, paste an adapted version of the DC3
block. This mirrors `dc3 App.cpp:75-196` + `:505-520` but with RB3 names/
messages. **Do not register names that §3 confirmed are already bound** —
`registerStub` no-ops on collision, but keep the list minimal.

```cpp
// === rb3/src/App.cpp — only if a REAL† subsystem is excluded from the link ===
// Place class defs near the top of App.cpp inside an #ifdef HX_NATIVE block,
// mirroring dc3 App.cpp:75-196. Registration block goes AFTER TheUI.Init()
// (App.cpp:272), mirroring dc3 App.cpp:505-520.
#ifdef HX_NATIVE
#include "obj/Object.h"
#include "obj/Data.h"
#include "obj/Dir.h"

// session / NetSession stub — only if TheNet/NetSession excluded.
// DTA: {session clear} (splash.dta:52), {session_mgr is_local} (11x),
//      {session_mgr is_leader_local} (6x). Offline = single local machine.
class NativeSessionStub : public Hmx::Object {
public:
    virtual DataNode Handle(DataArray *msg, bool rev) {
        Symbol s = msg->Sym(1);
        if (s == "is_local")        return DataNode(1);   // single machine
        if (s == "is_leader_local") return DataNode(1);
        if (s == "is_in_game")      return DataNode(0);
        if (s == "is_busy")         return DataNode(0);
        if (s == "num_users")       return DataNode(1);
        if (s == "clear" || s == "disconnect" || s == "end_game") return DataNode(0);
        return Hmx::Object::Handle(msg, rev); // add_sink/remove_sink → bare-Object
    }
};

// content_mgr stub — only if WiiContentMgr excluded.
// DTA: {content_mgr start_refresh} (main_hub.dta:102), {content_mgr refresh_done}.
class NativeContentMgrStub : public Hmx::Object {
public:
    virtual DataNode Handle(DataArray *msg, bool rev) {
        Symbol s = msg->Sym(1);
        if (s == "start_refresh")  return DataNode(0);
        if (s == "refresh_done")   return DataNode(1);   // pretend always done
        if (s == "never_refreshed")return DataNode(0);
        return Hmx::Object::Handle(msg, rev);
    }
};

// net_cache_mgr stub — only if the net/store cache subsystem excluded.
// DTA: {net_cache_mgr init {...netcache_init}} (ui.dta:37).
class NativeNetCacheMgrStub : public Hmx::Object {
public:
    virtual DataNode Handle(DataArray *msg, bool rev) {
        Symbol s = msg->Sym(1);
        if (s == "init")            return DataNode(0);   // no-op
        if (s == "is_ready")        return DataNode(1);
        if (s == "is_done_loading") return DataNode(1);
        return Hmx::Object::Handle(msg, rev);
    }
};

// saveload_mgr stub — ONLY as a fallback if the real SaveLoadManager cannot
// reach idle natively (verification item #1). Mirrors dc3 NativeSaveLoadStub.
class NativeSaveLoadStub : public Hmx::Object {
public:
    virtual DataNode Handle(DataArray *msg, bool rev) {
        Symbol s = msg->Sym(1);
        if (s == "activate")               return DataNode(0);
        if (s == "is_idle")                return DataNode(1);
        if (s == "is_initial_load_done")   return DataNode(1);
        if (s == "is_autosave_enabled")    return DataNode(0);
        if (s == "autosave")               return DataNode(0);
        if (s == "enable_autosave" || s == "disable_autosave") return DataNode(0);
        return Hmx::Object::Handle(msg, rev);
    }
};
#endif // HX_NATIVE
```

Registration block — append **after** `TheUI.Init();` (`src/App.cpp:272`),
mirroring `dc3 App.cpp:505-520`. `registerStub` no-ops if the real singleton
already claimed the name, so it is safe to leave all candidates listed:

```cpp
#ifdef HX_NATIVE
    {
        auto registerStub = [](const char *name, Hmx::Object *obj) {
            if (!ObjectDir::Main()->FindObject(name, false, false)) {
                obj->SetName(name, ObjectDir::Main());
            } else {
                delete obj; // real singleton already registered this name
            }
        };
        // Only the REAL† subsystem objects need a fallback; everything else is REAL.
        registerStub("session",        new NativeSessionStub());
        registerStub("content_mgr",    new NativeContentMgrStub());
        registerStub("net_cache_mgr",  new NativeNetCacheMgrStub());
        // Enable ONLY if verification item #1 fails:
        // registerStub("saveload_mgr", new NativeSaveLoadStub());
    }
#endif // HX_NATIVE
```

> Note placement: `BandUI::Init()` (= `TheUI.Init()`) constructs `net_sync`,
> `ui_event_mgr`, `input_mgr` and grabs `overshell`/panels in `InitPanels`, and
> `BandUI::Init` also *dereferences* `TheNetSession`, `TheContentMgr`,
> `TheSaveLoadMgr`, `TheRockCentral` directly (`BandUI.cpp:60-70`). So if a
> subsystem is excluded (outcome B), those globals must still be non-null
> **before** `TheUI.Init()` runs — meaning the stub for `session`/`content_mgr`
> must be installed *and the corresponding `TheX` global pointed at a safe
> object* in T2's `#ifndef HX_NATIVE` gating, not just registered by name here.
> Named-object registration alone is insufficient for the C++ deref sites.
> This is the key RB3-vs-DC3 difference: RB3 holds typed `TheX` globals, DC3
> resolved by name. **Prefer outcome (A).**

---

## 5. UI-init bypass spec (exact targets)

RB3's UI architecture diverges from DC3's; the three DC3 bypasses map as follows.

### 5.1 `mSink` direct-set — NOT NEEDED on RB3
DC3 needed it because button routing went through `UIManager::mSink` set only by
a `set_sink` DTA action (`dc3 DTA_LOADING_BLOCKER.md`). **RB3 has no `set_sink`
action and no `mSink` assignment anywhere** — `grep set_sink` over `src/system/ui`,
`src/band3/meta_band`, and all boot DTAs returns nothing. RB3's `mSink` is
exposed purely as `HANDLE_MEMBER_PTR(mSink)` (`src/system/ui/UI.cpp:904`) (a
get/set passthrough that DTA never invokes). Input on RB3 routes through the
focus path: `Automator`→`ButtonDownMsg` (`UI.cpp:180,302`) → `FocusPanel()` →
`UIScreen`/`UIComponent`. `BlockHandlerDuringTransition` (`UI.cpp:859-881`)
gates button/keyboard during transitions. **No native bypass required here** —
do not port DC3's mSink direct-set.

### 5.2 Animation lifecycle / `IsAnimating()` self-delete — DIFFERENT mechanism
RB3 has **no `IsAnimating()`** on `UIScreen` and **no `FlowAnimate`/`AnimTask`**
in the UI path. The transition state machine instead gates on
`Entering()`/`Exiting()`/`CheckIsLoaded()`:

- `UIManager::Poll()` (`src/system/ui/UI.cpp:500-571`) advances
  `kTransitionTo → kTransitionFrom → kTransitionNone`. The
  `kTransitionFrom → kTransitionNone` step (and the firing of
  `SendTransitionComplete` → `UITransitionCompleteMsg`, the DTA
  `TRANSITION_COMPLETE_MSG` hook) is gated on
  `!mCurrentScreen->Entering()` (`UI.cpp:553`).
- `UIScreen::Entering()` (`src/system/ui/UIScreen.cpp:87-100`) → any active
  `UIPanel::Entering()` (`UIPanel.cpp:181-186`) → `PanelDir::Entering()`
  (`src/system/ui/PanelDir.cpp:185-195`): true while **any `UIComponent` is
  entering OR any `UITrigger::IsBlocking()`**. Triggers/anims are DTA-driven
  (`*.trg trigger`, `*.anim animate` — e.g. `main.dta:51`, `main_hub.dta:512`).
- `BandScreen::Entering()` ALSO OR's `TheBandUI.WipingIn()`
  (`src/band3/meta_band/BandScreen.cpp:18`); `WipingIn`/`WipingOut`
  (`BandUI.cpp:312-320`) query `abstract_wipe_panel` (the `{ui abstract_wipe}`
  fades, `main_hub.dta:222,258`).

**Native risk:** if a panel's enter `.anim`/`.trg` never completes (the RB3
analog of DC3's `AnimTask` that never self-deletes), `Entering()` stays true and
the transition never reaches `kTransitionNone` → `main_hub_screen` never settles
and `TRANSITION_COMPLETE_MSG` never fires. **Bypass target if this manifests:**
`UIManager::Poll()` `UI.cpp:551-553` — gate the `Entering()` check (or
`PanelDir::Entering()`/`BandScreen::Entering()`) under `#ifdef HX_NATIVE` to
force-complete after the panel is loaded, mirroring DC3's `IsAnimating()` bypass.
Diagnose first (anims may complete fine once `Load()` is byte-correct, per T5).
Do **not** add this preemptively.

### 5.3 Screen auto-advance — mostly handled by the REAL DTA flow + Movie graceful-done
RB3 has **no `GotoFirstScreen`** (BOOT_TO_SONG.md Open Q#2). First screen is
DTA: `ui.dta:45 {ui goto_screen $first_screen}` with
`first_screen=intro_movie_screen` (`ui.dta:43`), via
`UI.cpp:912 HANDLE(goto_screen, OnGotoScreen)`. **Keep this DTA path** — do not
add a C++ `GotoFirstScreen` call.

The boot screen chain that must auto-advance natively:

1. **`intro_movie_screen` → `splash_screen`** (Bink intro; no native decoder).
   Hook is `MoviePanel::Poll` (`src/system/meta/MoviePanel.cpp:146-156`): when
   `!mMovie.Poll() && !TheUI.InTransition()` it fires `movie_done_msg` → DTA
   `(movie_done {ui goto_screen splash_screen})` (`splash.dta:18-19`).
   **Two native concerns at the same call site:**
   - `MoviePanel::IsLoaded()` returns false while `!mMovie.Ready()`
     (`MoviePanel.cpp:91-94`). The transition INTO `intro_movie_screen` is gated
     on `CheckIsLoaded()` (`UI.cpp:514`) → if Bink can't open the file,
     `mMovie.Ready()` may never go true and the screen **never loads**, blocking
     boot.
   - Conversely if `Ready()` is true but `Poll()` never returns false, the movie
     never ends → no `movie_done` → no advance.
   **Bypass target:** `Movie::Ready()` (`src/system/movie/Movie.cpp` Ready →
   `mImpl->Ready()`) and `Movie::Poll()` (`Movie.cpp:206`). Under
   `#ifdef HX_NATIVE` (Bink absent), make `Ready()`→true and `Poll()`→false
   immediately so `intro_movie_screen` loads then instantly fires `movie_done`
   and advances to `splash_screen` — the graceful "no decoder, treat as ended"
   path. (Movie integration is `synth/BinkReader.h` + `utl/BinkIntegration.h`,
   `Movie.cpp:18,27-40`.) Alternatively gate the `MoviePanel::IsLoaded`
   `!mMovie.Ready()` early-return (`MoviePanel.cpp:92`) + the `Poll` condition
   (`MoviePanel.cpp:150`). Prefer fixing `Movie` once so both the splash startup
   movies (`App.cpp:158-176`) and this MoviePanel get the same graceful behavior.

2. **`splash_screen` → `main_hub_screen`** is a real DTA state machine in
   `splash_panel` (`splash.dta:42-167`), **not** a timer. Sequence:
   - `enter` (`splash.dta:54-64`): `{loading.grp set_showing FALSE}`. (The
     `{saveload_mgr activate}` on enter is `#ifdef HX_PS3` only.)
   - `SELECT_MSG` (press Start, `splash.dta:113-128`) →
     `set_splash_state kSplashScreen_ActivateSaveLoad` → `{saveload_mgr activate}`.
   - `poll` (`splash.dta:96-102`): when `{saveload_mgr is_idle}` →
     `kSplashScreen_StartOvershell`.
   - `kSplashScreen_StartOvershell` (`splash.dta:139-152`):
     `{overshell attempt_to_add_user [last_user]}`,
     `{profile_mgr set_primary_profile_by_user [last_user]}`,
     `{overshell set_active_status kOvershellInShell}`, →
     `kSplashScreen_WaitOvershell`.
   - `overshell_allowing_input` callback (`splash.dta:159-167`): when overshell
     reports input-allowed → `kSplashScreen_EndOvershell` →
     `{ui goto_screen main_hub_screen}` (`splash.dta:153-158`).

   **Native concerns (NOT a timer bypass — keep the real flow):**
   (a) Press-Start: `SELECT_MSG` needs a synthetic Confirm/Start. On a headless/
   keyboard native build, inject a `kAction_Confirm` button once on
   `splash_screen` (the native input source feeds `Automator`). This is the one
   place boot needs a synthetic input. File target: the native frame loop /
   input source added in T2 (`rb3/native/src/main_native.cpp` + the
   `Automator`/Joypad path), NOT a DTA edit.
   (b) `saveload_mgr is_idle` must become true (verification item #1, §4).
   (c) `overshell attempt_to_add_user` → `overshell_allowing_input(TRUE)` must
   fire for a local user with no Wii profile. This drives through
   `OvershellPanel::AttemptToAddUser` (`OvershellPanel.cpp:1562`) +
   `OvershellAllowingInputChangedMsg` (`BandUI.cpp:137`). If it stalls on a
   Wii/profile gate, that gate is the `#ifndef HX_NATIVE` target inside
   `OvershellPanel`/`OvershellSlot` — diagnose during T7.
   - `first_time_calibration` push (`splash.dta:154-157`) is guarded by
     `{! {profile_mgr get_has_seen_first_time_calibration}}`; with no profile,
     `get_has_seen_first_time_calibration` default determines whether boot
     detours through calibration. Confirm it returns a value that lets boot go
     straight to `main_hub_screen` (or treat calibration as a later screen to
     auto-skip). Target: `ProfileMgr` `get_has_seen_first_time_calibration`
     handler.

### 5.4 Where HX_NATIVE blocks go (summary — specify only, do not write)
| Concern | File:line target | Block shape |
|---------|------------------|-------------|
| Net/RockCentral/EntityUploader init | `src/App.cpp:216,218,220` | `#ifndef HX_NATIVE` around the 3 `Init()` calls (T2) |
| GameMic / UsbMidi | `src/App.cpp:222-224` | `#ifndef HX_NATIVE` (T2) |
| WiiProfileMgr | `src/App.cpp:271` | `#ifndef HX_NATIVE` (T2) |
| VI / CustomSplash / disc-error | `src/App.cpp:139,146-147,181-193,282` | `#ifndef HX_NATIVE` (T2) |
| MemPushHeap | `src/App.cpp:303-304` | `#ifndef HX_NATIVE` (T2) |
| Manager stub fallback (only if §3 flags) | after `src/App.cpp:272` | `#ifdef HX_NATIVE` registration block (§4) |
| Movie graceful-done | `src/system/movie/Movie.cpp` `Ready()`/`Poll()` | `#ifdef HX_NATIVE` Bink-absent path (§5.3.1) |
| Transition Entering() force-complete (IF needed) | `src/system/ui/UI.cpp:551-553` and/or `PanelDir.cpp:185` | `#ifdef HX_NATIVE`, diagnose first (§5.2) |
| Synthetic Start on splash | native input source (`rb3/native/src/main_native.cpp`) | native-only, not a DTA/src edit (§5.3.2) |

---

## 6. Open questions for the implementer

1. **T1/T2 link decision for `TheNet`/`NetSession`/`WiiContentMgr`/`WiiProfileMgr`:**
   keep-and-gate (outcome A, fewer stubs) vs exclude-and-stub (outcome B). This
   choice determines whether §4's stub block is needed at all. Recommend (A):
   the offline defaults already verified (`NetSession::IsLocal`→true,
   `NetCacheMgr::IsReady`→1) mean the real objects answer DTA correctly.
2. **`saveload_mgr` idle reachability** with no Wii memcard (verification #1) —
   the single hard gate in the splash flow.
3. **`overshell attempt_to_add_user` → `overshell_allowing_input`** for a
   profile-less local user (verification, §5.3.2(c)).
4. Whether the intro-movie graceful-done belongs in `Movie` (shared with the
   App startup splashes) or scoped to `MoviePanel` (§5.3.1) — recommend `Movie`.
