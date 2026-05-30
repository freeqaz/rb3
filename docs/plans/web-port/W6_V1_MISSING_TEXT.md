# W6 V1/V2/V3 — Missing-text root-cause analysis

**Parent plan:** [`PLAN.md`](PLAN.md) → [`W6_VISUAL_POLISH.md`](W6_VISUAL_POLISH.md) (top three P0 entries).
**Builds on:** [`W5_TEXT_RENDERING.md`](W5_TEXT_RENDERING.md) (Phase 1+2 — `useAlphaAsRGB` + `zMode=Disable` for unnamed RndText sub-meshes in the engine's `BandRnd::DrawMesh`).
**Audit anchor:** rb3 master `92ff7768`, engine main `8397fa6` (W5 fix commit).
**Investigation scope:** root-cause only. No code changes shipped; this doc proposes the concrete fix and risk.

> **NOTE on Phase 3:** A separate agent owns `W5_TEXT_RENDERING.md` Phase 3 (the "dim, hard to read" residual). V1/V2/V3 are NOT that — they are *entirely absent*, with a different root cause. The two investigations do not overlap. This doc does not touch `W5_TEXT_RENDERING.md`.

---

## Symptoms recap

| ID  | Screen                                                                     | Visual symptom                                                                                                                                                       | Reference frame                                                                                                                              |
| --- | -------------------------------------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------- |
| V1  | `song_select_screen` — `docs/sessions/web/screenshots/w5-text-fix/03_song_select.png` | Song-row title column is empty. Other text on the screen (`MUSIC LIBRARY`, `0/10`/`0/5`/`0/30`, `FRIEND RANKINGS`, `CHOOSE INSTRUMENT`) renders.                       | `images/retail-screenshots/yt_qRagnZCIMzk_song_select_list.png`                                                                              |
| V2  | `main_hub_screen` — `docs/sessions/web/screenshots/w5-text-fix/02_main_hub.png`       | The `PLAY NOW / CAREER / TRAINING / CUSTOMIZE / GET MORE SONGS` left-side menu list is invisible (no labels, no panel backdrop). Venue 3D + news ticker render OK.       | `images/retail-screenshots/yt_mhKNp9uAT48_menu_hub.png`                                                                                      |
| V3  | `game_screen` — `docs/sessions/web/screenshots/w5-text-fix/07_gameplay_t15s.png`      | HUD score digits, streak number, multiplier digits, star-power gauge, energy bar, song-progress bar — none of them render. Gem highway + "COOL" multiplier panel do.    | `images/retail-screenshots/yt_qRagnZCIMzk_gameplay_guitar.png`                                                                                |

---

## Root cause

V1 and V3 (digits) share **one** root cause. V2 and V3 (bars/meters) are separate.

### V1, V3-digits — `MusicLibrary::Text` / `AppScoreDisplay::UpdateDisplay`-class HX_NATIVE early-return on `dynamic_cast<AppLabel*>` failure

The 360-ARK extract's UI milos (`ui/song_select/gen/song_select.milo_xbox`, `ui/track/scoreboard.dta`-driven score widgets, etc.) author several `UIListLabel` slots and standalone score labels as **plain `UILabel`** objects, where the Wii original authored them as `AppLabel` (a `UILabel` subclass with format/binding helpers like `SetSongAndArtistName` / `SetScoreOrStars`).

The decompiled rb3 binders all do this:

```cpp
AppLabel *p9_label = dynamic_cast<AppLabel *>(label);
#ifdef HX_NATIVE
    if (!p9_label) return;       // <-- silent drop on web
#else
    MILO_ASSERT(p9_label, ...);  // would crash; was added to avoid that
#endif
p9_label->SetSongAndArtistName(osn);
```

When the cast yields null on web, the binder **returns without ever writing text to the plain `UILabel`**. The label's `mText` (`RndText`) keeps whatever it was last set to (typically `gNullStr` from `UIListLabel::CreateElement` → `l->SetTextToken(gNullStr);` — see `src/system/ui/UIListLabel.cpp:33`). The RndText then renders, but its mesh has zero glyphs, so nothing appears.

This is also why the row backdrops, the column `0/10` count slot (header rows go through a different branch), and the `FRIEND RANKINGS` label render — the W5 engine fix is fine for them. The *song-title* slot specifically returns early without setting text.

**Confirmed sites** (all use the identical `dynamic_cast<AppLabel*>` + `HX_NATIVE` early-return pattern):

- `src/band3/meta_band/MusicLibrary.cpp:1048-1060` — `MusicLibrary::Text` → blank song titles, sub-headers, function rows, setlist names. **This is V1's smoking gun.**
- `src/band3/meta_band/SongSetlistProvider.cpp:13-25` — `SetlistProvider::Text` → blank setlist rows.
- `src/band3/meta_band/ViewSetting.cpp:386-395` — `ViewSettingsProvider::Text` → blank "status" column on view-settings rows (cosmetic).
- `src/band3/meta_band/AppScoreDisplay.cpp:7-18` — `AppScoreDisplay::UpdateDisplay` → score panel doesn't update (this is the `mCombinedLabel`, the small score-summary tile on `song_select`, not the in-game digits).

For the in-game score, the path is **different** but the root cause is the same family:

- `ui/track/scoreboard.dta:85-96` — the milo authors `set_score` as `{score.lbl set_score_or_stars …}`, which is an `AppLabel`-only message. If `score.lbl` is a plain `UILabel`, `HandleType(set_score_or_stars_msg)` returns unhandled — silently. The `set_score_milo` branch (line 117-124) which uses `set_int` would work, but the milo's `set_score` action picks `set_score_or_stars` unconditionally.

### V2 — main-hub menu list invisible (separate cause)

Empirical evidence:

- `ui/main/main_hub.dta:744` declares `(panels meta sv3_panel main_hub_panel accomplishments_status_panel)`. The hub 3D scene (`sv3_panel`), the venue neon signs, and the news ticker all render (the ticker is *bound by* `MainHubMessageProvider::Text` which uses a **C-style cast** rather than `dynamic_cast` — `SetMessageLabel((AppLabel *)label, data)` at `src/band3/meta_band/MainHubMessageProvider.cpp:22` — so the message_text slot always receives a valid pointer regardless of the runtime type, and the static-text labels on the ticker are written).
- The menu items (`mb_playnow.btn`, `mb_career.btn`, `mb_trainers.btn`, `mb_shop.btn`, `mb_musicstore.btn`) are `UIButton`s (subclass of `UILabel`) authored inside `main_hub.milo`. They get their static text via `text_token` PROPSYNC at load time — no `AppLabel` cast involved, no per-frame binder.
- Hub state visibility is gated by `update_state_view` (line 507-601 of `main_hub.dta`). `MainHubPanel`'s constructor sets `mHubState(kMainHubState_Main)` (rb3 `src/band3/meta_band/MainHubPanel.cpp:79`). On `Enter()` it calls `UpdateStateView(mHubState=Main, prev=None, ...)` which should `none_to_main.trg trigger` to make the `kMainHubState_Main` group visible.

The W5 fix recovered the news ticker but **not** the menu items, even though both are static labels in the same milo. The most plausible failure mode is one of:

1. **`none_to_main.trg` doesn't fire on first Enter** — either because `mHubState == prev` short-circuits the switch (it does: line 519 `({== $state $old_state} TRUE)`), or because the trigger is wired so `kMainHubState_Main` is the *default* visible state in milo and the `none_to_main` trigger only handles the transition. Looking at the .dta switch: `{== $state kMainHubState_Main}` requires `$old_state` to be one of `None/PlayNow/Career/Training/Customize/Store` to fire a trigger. With `Main==Main`, no `_to_main` fires. If the milo authors `mb_playnow.btn` etc. as showing=FALSE by default, expecting `none_to_main.trg` to flip them on, the buttons never become visible.
2. **The menu-list panel host is a separate `RndDir` that fails to load** from the 360-ARK milo because of an unsupported scene type (similar to the `WiiInstance`/`World*` gaps we hit in W2). Less likely — the hub scene loads enough to render the news ticker, which is sibling geometry.
3. **The buttons are RenderTarget-textured** (the hub menu buttons in retail have a glossy 3D appearance). If their text is rendered via a custom `RenderToTexture` path that the WGPU backend doesn't fully handle, the texture would be empty and the button face would be invisible. We have known RTT gaps for character outfits (W4 era) and the recent `9f635b7` engine fix only addressed scene-cam RTT.

Most-likely (high confidence): **(1) `update_state_view` no-op when `state == old_state`**. The ctor sets `mHubState = Main`, so `Enter()`'s `UpdateStateView(Main, None, ...)` would *normally* fire `none_to_main.trg`, but the .dta switch only handles the case `state == kMainHubState_Main AND old_state != None` for the *_to_main triggers (lines 525-539). With `old_state = kMainHubState_None`, only the *_to_main switch arm matches — but `none_to_main` is INSIDE that arm at line 527. So it SHOULD fire. Need a runtime probe to confirm.

(Alternative hypothesis worth checking: the 360-ARK `main_hub.milo` may have a *different* default-visible state from the Wii original — e.g. it ships with the menu group hidden, expecting a `kMainHubState_Loading → kMainHubState_Main` transition that doesn't happen on web. Inspect the milo binary via `rb3-dta` headless to confirm initial showing-states.)

### V3 bars / star-power / energy / progress (non-text overlays)

These are **mesh-based animated UI** (StreakMeter, OverdriveMeter, GemHUD progress bar — `src/system/bandobj/StreakMeter.cpp`, `OverdriveMeter.cpp`, etc.). They don't go through `RndText` at all. The W5 fix would not have touched them.

The most likely failure mode is that these are `BandScoreboard`-style "swap mesh geometry via `SetGeomOwner`" displays where the source meshes (e.g. `num%d.mesh`, gauge fill quads) are NAMED meshes in the .milo. The W5 text-mesh heuristic skips them (correctly — they're not text). They render at whatever colour/material the milo declares. If their materials are configured for fixed-function alpha-blending or vertex-color shading that the engine's `BandRnd::DrawMesh` doesn't yet drive (e.g. they expect `mat->mIntensify` semantics, sprite alpha, or specific blend modes), the meshes draw transparently or off-screen.

For the in-game **score digits** specifically, see V1/V3-digits above — the `score.lbl set_score_or_stars` message is silently unhandled if `score.lbl` is a plain UILabel; the digits are never updated, so the BandScoreboard's `mNumMeshes` stay at initial state (showing=false from `ResetScore`).

This is **not** a single fix — it's two-three separate per-widget audits.

---

## Proposed fix

### V1 — `MusicLibrary::Text` plain-UILabel fallback (rb3-side, S)

Replace the silent early-return with a best-effort write to the plain `UILabel`:

```cpp
// src/band3/meta_band/MusicLibrary.cpp:1048
void MusicLibrary::Text(int, int idx, UIListLabel *slot, UILabel *label) const {
    AppLabel *p9_label = dynamic_cast<AppLabel *>(label);
#ifdef HX_NATIVE
    // ... (existing comment) ...
    // p9_label is null for 360-ARK milos that author song-list slots as plain
    // UILabels. Fall back to SetDisplayText/SetTextToken instead of returning
    // empty — the previous early-return left song titles invisible (W6 V1).
#else
    MILO_ASSERT(p9_label, 0x638);
#endif
    SortNode *sortNode = GetCurrentSort()->GetNode(idx);
    switch (sortNode->GetType()) {
    case kNodeSong: {
        OwnedSongSortNode *osn = dynamic_cast<OwnedSongSortNode *>(sortNode);
        if (slot->Matches("song")) {
            if (p9_label) {
                if (unkdc != 1) p9_label->SetSongAndArtistName(osn);
                else            p9_label->SetSongName(osn);
            } else {
#ifdef HX_NATIVE
                // Plain UILabel fallback: write title (and artist) directly.
                // Loses the localized "<song> by <artist>" format, but the
                // alternative is empty rows.
                const char *title = osn->GetTitle();
                const char *artist = osn->GetArtist();
                if (unkdc != 1 && artist && *artist) {
                    label->SetDisplayText(
                        MakeString("%s - %s", title, artist), true);
                } else {
                    label->SetDisplayText(title, true);
                }
#endif
            }
        }
        // ... apply the same fallback to the "difficulty" / "percentage" /
        // setlist / header / subheader / function branches.
        break;
    }
    // ... other cases ...
    }
}
```

Apply the same pattern to:

- `src/band3/meta_band/SongSetlistProvider.cpp:13-39` — setlist-builder rows.
- `src/band3/meta_band/AppScoreDisplay.cpp:6-19` — wrap `label->SetInt(...)` fallback (the data exists in `unk114/mScore/mRank/mGlobally`).

**Decomp-match risk: ZERO** (HX_NATIVE-gated; the matched Wii path is untouched and the assert still fires on Wii where the cast cannot fail).

**Visual test:** rebuild rb3-web, re-run the W4 capture script, verify song titles appear in `03_song_select.png`.

### V2 — main-hub menu visibility (rb3 + engine investigation, M-L)

Do **not** ship a fix yet — needs a runtime probe to disambiguate hypotheses (1) vs (3) above. Concrete next steps in an engine worktree:

```bash
# Engine worktree for safe experimentation
git -C /home/free/code/milohax/milo-native-engine worktree add /tmp/milo-engine-v2 main

# 1. Log every static-text label drawn on the hub screen with its parent.
#    In src/platform/Rnd_Wgpu_RB3.cpp::BandRnd::DrawMesh (~line 1665):
#      bool isTextMeshHeur = mesh->Name() && mesh->Name()[0] == '\0';
#    +  if (getenv("V2_DBG") && isTextMeshHeur) {
#    +    RndTransformable* p = mesh->TransParent();
#    +    const char* pn = p ? (p->Name() ? p->Name() : "(unnamed)") : "(null)";
#    +    fprintf(stderr, "[V2_DBG] text-mesh parent='%s' showing=%d\n",
#    +            pn, p ? (int)p->Showing() : -1);
#    +  }
#
# 2. Boot to main hub: V2_DBG=1 python3 native/web/server.py ...
#    Grep server log for parent names containing 'playnow', 'career',
#    'trainers'. If they appear with showing=0, the trigger never fired.
#    If they don't appear at all, the buttons aren't drawn (LoadFile gap).
#
# 3. If trigger-related: add a one-frame manual call to
#    `none_to_main.trg::Trigger()` from MainHubPanel::Enter() (HX_NATIVE-gated)
#    as a probe. If text appears, root cause is confirmed.
#
# 4. If RTT-related: log `RndCam::sCurrent->TargetTex()` per draw on the hub
#    screen. The hub's button bezels are likely RT-textured.
```

Defer V2 fix until probe results land.

### V3 digits — `set_score_milo` instead of `set_score` (rb3 .dta override or engine pin)

The path of least resistance is to update the binder so `set_score` calls `set_int` when the slot is plain:

- **Option A (rb3 cpp)**: there's no rb3 code calling `score.lbl set_score` — it's authored in `ui/track/scoreboard.dta:85-96`. The `set_score` action is invoked by gameplay (`Player`/`BandTrack`) sending a `set_score` message to the scoreboard. The fix would be to override `set_score`'s body in code via a HANDLE_ACTION, but the scoreboard is data-driven.
- **Option B (asset override)**: in the web build, override `scoreboard.dta:85-96` (`set_score`) to use `set_score_milo` (line 117-124) which uses `set_int`. Either by shipping a patched `scoreboard.dta` in the web assets or by detecting plain-UILabel at runtime.
- **Option C (engine shim)**: add `set_score_or_stars` as a UILabel-accepted message that falls back to `SetInt(score, true)`. Targeted, contained.

**Recommend Option C** (engine shim): add a generic `set_score_or_stars` handler in `UILabel::Handle` (HX_NATIVE-gated) that calls `SetInt(_msg->Int(3), true)` when the receiver is not an AppLabel. Reuses existing `set_int` infrastructure, single rb3 file touched (`src/system/ui/UILabel.cpp`), HX_NATIVE-gated → zero decomp impact.

### V3 bars/meters (deferred — needs per-widget audit, L)

Out of scope for this investigation. Probe needed:

- StreakMeter — `src/system/bandobj/StreakMeter.cpp`: is the bar mesh upload finishing? Does it have a valid material?
- OverdriveMeter — same question.
- BandScoreboard — already covered by V3-digits fix (numbers render when `SetScore(int)` is called).

These are likely *both* visible after V3-digits is fixed (the meters' visibility is gated by `RefreshOverdrive(energy, canDeploy)` which Track::RefreshPlayerHUD calls per frame; if the score path was broken, other Player.* state may also have been broken).

---

## Scope summary

| Fix     | Side                              | Files                                                                                   | LoC      | Cx   | Decomp risk         |
| ------- | --------------------------------- | --------------------------------------------------------------------------------------- | -------- | ---- | ------------------- |
| V1      | rb3                               | `MusicLibrary.cpp`, `SongSetlistProvider.cpp`, `AppScoreDisplay.cpp`                    | 30-60    | S    | ZERO (HX_NATIVE)    |
| V2      | engine probe → rb3 *or* engine    | `Rnd_Wgpu_RB3.cpp` (probe), then likely `MainHubPanel.cpp` or milo loader               | unknown  | L    | depends on outcome  |
| V3-num  | engine (recommended)              | `src/system/ui/UILabel.cpp` (HX_NATIVE message handler)                                 | 8-12     | S    | ZERO (HX_NATIVE)    |
| V3-bars | engine + rb3 audit (defer)        | StreakMeter / OverdriveMeter / GemHUD                                                   | unknown  | L    | likely ZERO         |

**V1 and V3-digits share a fix family** (plain-UILabel fallback in `dynamic_cast<AppLabel*>` branches and AppLabel-only messages). **V2 is independent** and needs probing before fix. **V3-bars is independent** and likely shakes out once V3-digits lands (HUD refresh callback is one path).

---

## Why the W5 heuristic doesn't cover V1/V2/V3

W5's `isTextMeshHeur = mesh->Name() && mesh->Name()[0] == '\0'` is the **correct** discriminator for un-named RndText sub-meshes. It is firing — the song-row sub-meshes ARE going through this path, get `useAlphaAsRGB=1`, `prelit=1`, `zMode=Disable`. But the **sub-meshes are empty** because the rb3 code never wrote a text string to the parent `RndText` — `RndText::UpdateMesh` builds zero vertices, `mesh->Faces().size() == 0`, `DrawMeshImmediate` early-returns at `if (nf <= 0) return;` (engine `Rnd_Wgpu_RB3.cpp:1246`).

So the engine is doing the right thing. The rb3 binders are silently dropping the text *before* it ever reaches RndText. That's a pure rb3-side miss, not a render-path miss.

---

## Risk to decomp match

**ZERO** for V1 and V3-digits. All fixes are inside `#ifdef HX_NATIVE` blocks already established by precedent (`MusicLibrary.cpp`, `SongSetlistProvider.cpp`, etc.). The matched Wii build sees the original `MILO_ASSERT(p9_label, …)` path unchanged.

**Unknown** for V2 until the probe results land. If the fix turns out to be a manual trigger-fire in `MainHubPanel::Enter`, it would be a one-line HX_NATIVE-gated addition (zero decomp risk). If it's a milo-loader / RTT gap, it's engine-side (zero rb3 decomp impact by definition).

---

## Files read during investigation

- `docs/plans/web-port/W5_TEXT_RENDERING.md`, `W6_VISUAL_POLISH.md`
- `milo-native-engine/src/platform/Rnd_Wgpu_RB3.cpp` (W5 fix, lines ~1140-1830)
- `milo-native-engine/src/platform/Mesh_Wgpu.cpp` (DC3 reference draw path, lines 120-210)
- `src/system/ui/UILabel.{cpp,h}`, `UIList.{cpp,h}`, `UIListDir.cpp`, `UIListLabel.cpp`, `UIListMesh.cpp`, `UIListSlot.cpp`, `UIListWidget.cpp`
- `src/system/rndobj/Text.cpp` (lines 1700-1820; RndText sub-mesh creation + `DrawShowing`)
- `src/system/obj/Object.h` (mName default = `gNullStr`)
- `src/band3/meta_band/MusicLibrary.cpp:1040-1278` (Text/Mat/Custom binders), `SongSetlistProvider.cpp` (all), `ViewSetting.cpp:375-399`, `AppScoreDisplay.cpp` (all), `AppLabel.{cpp,h}` (Set*-family setters), `MainHubPanel.{cpp,h}`, `MainHubMessageProvider.cpp` (ticker binder)
- `src/system/bandobj/BandScoreboard.cpp` (digit-mesh-swap), `BandTrack.cpp` (RefreshStreakMeter, RefreshOverdrive)
- `orig-assets/extracted/ui/main/main_hub.dta`, `orig-assets/extracted/ui/song_select/song_select.dta`, `orig-assets/extracted/ui/track/scoreboard.dta`

Screenshots: `docs/sessions/web/screenshots/{baseline-2026-05-29,w5-text-fix}/{02_main_hub,03_song_select,07_gameplay_t15s}.png`

---

## Unknowns / followups

- **V2 root cause is not pinned.** The two hypotheses (trigger-no-op vs RTT-textured-button) require a runtime probe (the `V2_DBG` env logging proposed above) to distinguish.
- **V3 bars/meters root cause is not pinned.** Probably collateral damage from V3-digits but needs a separate per-meter probe.
- **Audit screenshot vs W5 README discrepancy:** W5 README claims `03_song_select` shows `TOMORROW NEVER KNOWS — THE BEATLES` as the first row title after the fix. The captured screenshot (`w5-text-fix/03_song_select.png`) does not. This means either (a) the W5 README's confirmation was a different test run not captured, or (b) the captured screenshot was taken before song-list refresh completed. Worth re-running `scripts/web/w5-text-capture.mjs` once before deciding the V1 hypothesis is fully proven.
- **Other HX_NATIVE `dynamic_cast<AppLabel*>` early-returns may exist.** A grep sweep should be done to find every site and apply the same fallback uniformly — incomplete coverage will leave per-screen text gaps. Initial sweep found 4 sites; comprehensive sweep recommended before fix dispatch.
