# W4.3-C34 — C3 (flipped hold-labels) + C4 (hub ticker overlap)

Engine pin 146fd19 (READ-ONLY on engine render code this stage). Build dir:
`native/build-agent-W4.3-C34`. Stage = C3+C4 diagnosis-first per A9/A10.

## C3 — bound facts (grounding, before any edit)

- The two flipped labels + the correct one are **three ActionElements of the SAME
  `InlineHelp` component** (`help.ihp` in `orig-assets/extracted/ui/song_select/song_select.dta`
  `update_helpbar` handler, ~:1559-1650): Confirm action gets
  `(play_song|enqueue_song) (hold_make_setlist|hold_finish_setlist)` (primary+secondary),
  a details/option action gets `("" "VIEW MORE INFO"-ish "details")` — actually `$option` =
  `details` maps to a DIFFERENT locale key with NO secondary (primary-only, confirmed by
  `orig-assets/extracted/ui/locale/eng/locale_keep.dta:11365` `hold_for_shortcuts` existing
  only as a bare secondary-style string, used by a THIRD ActionElement/shortcuts action not
  present in song_select.dta's visible source — likely wired via a shared shortcut-panel
  config elsewhere).
- `src/system/bandobj/InlineHelp.h`/`.cpp` (RB3 shared game code, NOT engine — editable):
  `InlineHelp::ActionElement` holds `mPrimaryStr`/`mSecondaryStr`; `DrawShowing()`
  (`InlineHelp.cpp:307-344`) applies a STATIC (class-wide, shared across ALL InlineHelp
  instances) rotation `rotXfm` built from `sLabelRot` degrees about the local X axis
  ONLY to labels whose `mSecondaryStr` is non-empty (`:338-340`). `Poll()` (`:281-305`)
  and `SetLabelRotationPcts()` (`:490-495`) implement a "flip" animation: 5s static at
  `sLabelRot==0` (readable), then over the next 1s `f1 in [0,1)`:
  `f<0.5: rot=f*-240` (0 to -120), `f>=0.5: rot=f*-240-120` (-240 to -360, i.e. mod 360
  == 120 to 0). **By construction this formula's visited-angle set (mod 360) is only
  `[240,360]` union `[0,120]` — it can NEVER reach 180 (the value that would render as a
  clean, fully-legible, vertically-mirrored-only quad — see geometry note below).**
  Content swap (`sRotated` toggle) happens at the `f>=0.5` boundary, mid-animation.
- Geometry note: a quad lying in the camera-facing plane, rotated exactly 180 deg about
  the local X axis, has Y negated and Z unchanged (Z was 0) — i.e. it stays fully
  face-on to the camera (no foreshortening, no backface-cull ambiguity) but reads
  top-to-bottom flipped, NOT left-right mirrored. That is EXACTLY what the screenshot
  shows (`/tmp/wave12-current-state/song_select_list2.png`: "HOLD TO MAKE SETLIST" /
  "HOLD FOR SHORTCUTS" fully legible, vertically flipped only). So the observable bug
  is consistent with `sLabelRot` being stuck at (or reaching) +-180 mod 360, which the
  shared formula should never produce — a native-side STATE/TIMING divergence, not a
  renderer determinant/backface issue (rules out the acceptance's escalation branch
  unless the probe proves otherwise).
- `sLabelRot`/`sRotationTime`/`sRotated`/`sHasFlippedTextThisRotation`/`sLastUpdatedTime`
  are ALL **static** (shared across every InlineHelp instance in the process, including
  the main_hub ticker's `expand_message_area.ihp`) — a global synchronized flip clock,
  intentional/shared (not itself suspicious).
- Three flat-static captures 358 and 502 frames apart (`song_select_list2.png` frame
  1649, `song_select_diffpanel.png` frame 2009, `song_select_diffpanel2.png` frame 2153)
  are reported visually IDENTICAL (`/tmp/wave12-current-state/NOTES.md:43-44`) — a
  genuinely time-varying 1s-per-6s-cycle animation should show SOME difference across
  an ~8s span unless every capture coincidentally landed in the same 5s static window
  (possible but the flipped-not-neutral value argues against "static window == rot 0").

## C3 probe plan (EDIT, HX_NATIVE + getenv-gated, no behavior change)
- `src/system/bandobj/InlineHelp.cpp`:
  - `Poll()` (`:281-305`): log `uisecs`, `sLastUpdatedTime`, `sRotationTime`, `f1`,
    `sLabelRot`, `sRotated` on every branch taken, gated `getenv("RB3_HOLDLABEL_DBG")`.
  - `DrawShowing()` (`:307-344`): per-label, log index, `mSecondaryStr` empty?,
    `sLabelRot`, and the composed `labelXfm` translation + a manual 3x3 determinant,
    same gate.
- Build in `native/build-agent-W4.3-C34`, reproduce `song_select_list2.png`'s nav
  (down x3 in music_library to a real song row), read stderr.
- Verdict branches: (a) `sLabelRot` numerically pinned near +-180/540/etc (formula
  violated) -> bug is in the TIME SOURCE (`TheTaskMgr.UISeconds()` under
  `RB3_FIXED_CLOCK`) or in a SEPARATE code path also writing `sLabelRot` -> fix
  game-side (this file or the UISeconds/TaskMgr clock feed) FLAG-FIRST; (b) `sLabelRot`
  correctly bounded in `[-120,0]`-equivalent per the formula (never reaching |180|) but
  the label STILL renders visibly upside-down -> the bug is in HOW the rotation is
  applied downstream (Multiply/composition, or the renderer's handling of the resulting
  Transform) -> if that touches `Rnd_Wgpu_RB3.cpp`, STOP, write the diff to STATUS.md,
  return ESCALATE_COORDINATOR per A9.

## C4 — bound facts (grounding)

- `orig-assets/extracted/ui/main/main_hub.dta:498-511`: the hub ticker is TWO sibling
  DTA objects handled by `MainHubPanel` (`src/band3/meta_band/MainHubPanel.cpp`):
  `message_area.lst` (a `UIList`, shows the scrolling message BODY text via
  `set_provider`) and `expand_message_area.ihp` (an `InlineHelp`, single
  Option-action label, primary text only == locale `main_hub_next_message`
  `"NEXT MESSAGE (%i/%i)"` — `locale_keep.dta:12463-12464`). `update_message_counter`
  (`main_hub.dta:505-511`) is the handler that pushes `(main_hub_next_message $current
  $max)` onto the ihp via `set_action_token` — primary-only, so C4 is NOT the same
  rotation bug as C3 (no secondary string on this ActionElement -> `DrawShowing`'s
  `:338` guard skips the rotXfm multiply for it entirely).
- The W4.1 precedent (`MainHubPanel.cpp:116-141`, "playnow.lsw" quad hide) fixed an
  UNRELATED opaque quad under `menu_buttons.grp`, explicitly noted as leaving "the
  message ticker untouched" (:125) — i.e. the ticker's own layout was never touched by
  that fix; this IS the still-open residual named in the kickoff.
- Working theory: `message_area.lst` and `expand_message_area.ihp` are positioned by
  AUTHORED local transforms in `ui/main/main_hub.milo` (binary, not directly greppable)
  under a shared parent group — retail stacks label above body (different Y). Need the
  live `WorldXfm().v` (and ideally `LocalXfm().v`) of both objects at draw time to see
  whether (a) they already differ in Y as authored but something recomputes an
  overriding world position collapsing them to the same line (engine/generic layout
  bug), or (b) they were never authored with different Y and the game code is
  expected to space them (a call site that got dropped/never HX_NATIVE-ported).

## C4 probe plan (EDIT, HX_NATIVE + getenv-gated, no behavior change)
- `src/system/bandobj/InlineHelp.cpp` `DrawShowing()`: extend the same
  `RB3_HOLDLABEL_DBG` (or a second flag `RB3_HUBTICKER_DBG`) gate to log `Name()` +
  `LocalXfm().v` + `WorldXfm().v` for instances whose `Name()` contains
  `"message_area"` or `"expand_message"`.
- `src/system/ui/UIList.cpp` `DrawShowing()` (`:455-465`): same log for
  `Name()` containing `"message_area"`.
- Build + reach main_hub, log at the frame the ticker draws (message must be
  non-empty — may need `RB3_GAME=1` demo profile with a canned ticker message, or
  force via debug injection if the provider returns 0 messages in the harness).
- Verdict branches: (a) authored Y already differs but drawn Y collapses -> engine
  layout bug (name the exact transform stage) -> escalate per A9 range rule if it's
  `Rnd_Wgpu_RB3.cpp`; (b) authored Y is genuinely identical/near-identical in both ->
  game-side data/layout issue -> fix game-side (`MainHubPanel.cpp`, e.g. a missing
  vertical offset applied to one of the two objects, mirroring the `:130` precedent
  of "re-assert every Poll").

## Process
- Checkpoint before returning: `/tmp/wave12-checkpoints/C34.json`.
- Git: `flock /tmp/rb3-git.lock git commit ...`, add ONLY files touched here
  (`src/system/bandobj/InlineHelp.cpp`, `src/system/ui/UIList.cpp` if touched, this
  PLAN.md/STATUS.md). Leave `src/system/ui/UILabel.cpp` (concurrent agent's
  uncommitted RB3_UILABEL_DBG probe) and any other unrelated dirty files untouched.
- New flags -> append-only entry in engine's `NativeCompatFlags.classification.json`
  under `flock /tmp/milo-engine-classjson.lock` (class "probe", default off).
