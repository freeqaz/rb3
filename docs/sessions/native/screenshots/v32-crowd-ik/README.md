# v32-crowd-ik screenshots — INTENTIONALLY EMPTY

## Why no screenshots

This session's build state could not reach in-song crowd-cinematic rendering
(the only state where the dispatch's "crowd hand-IK target ~300u from hand"
residual is observable). Three runs (one direct, one `gdb -batch -ex run`
slowed-harness, one extended 12000-frame) all stalled the same way: the
synthetic `RB3_GAME_INPUT` clicks through to `game_screen` cleanly by
frame 456 (per `RB3 screen:` log lines), `Game::mLoadState` reaches
`kReady`, audio/midi/anim load successfully — but `BandDirector::Enter` /
`EnterVenue` never fires, the venue 3D background never engages, and mesh
count plateaus at 50-65/frame (gameplay should be 200+/frame per the V26
session). This matches the pre-existing menu→gameplay reach FLAKE
explicitly documented in V24's regression notes and V26's "known
menu→gameplay reach FLAKE persists."

Additionally, the `BandRnd::EndDrawing` auto-screenshot defer-loop has a
quirk where `mFirstSceneCamFrame` is never set in this stalled state (no
scene cam ever calls `Select()`), so once `mFrameCount > target + 200`
the shot is permanently skipped without advancing `mShotIndex`. Even if I
had reached gameplay, the deferral logic would have to fire on a frame
where a real scene cam was current. The V26 doc was written when this
flow worked; V32 inherited the broken-flow state.

See `../../VENUE_RENDER.md` § V32 for the full root cause + V21/V26
reapply notes + the concrete recommendation for the next agent
(restore gameplay-reach BEFORE attempting any crowd-IK target-proxy
debugging — `IK_TGT_DBG=1` is wired up and ready to fire the moment
`CharIKHand::Poll` runs, which only happens in-song).
