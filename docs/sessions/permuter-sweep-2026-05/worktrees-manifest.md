# Permuter sweep 2026-05 — worktree manifest

Snapshot captured **2026-05-28** before bulk removal of session sweep worktrees.

54 session worktrees under `.claude/worktrees/` named `<wave><n>-<symbol-hint>`
(e.g. `q1-hiresscreen`, `w3-bandcharacter`). All have `ahead=0` against master —
no unmerged commits. Wins were cherry-picked to master via Edit-and-commit;
worktrees acted as **per-target sandboxes** for permuter runs + experimentation.

## Removal criteria

A session worktree is safe to remove when:
- `ahead=0` against master (no unique commits) — **all 54 satisfy**
- Either no `src/` diff vs master OR the diff is a rejected experiment

## Worktree status table

| wt | last commit | src diff vs master | disposition |
|---|---|---|---|
| j5-charkeyhand | 36efad2c (worktree-seed) | none | remove |
| j6-vocaltrack | 36efad2c | none | remove |
| j7-bandwardrobe | 36efad2c | BandWardrobe.cpp DIFFERS (62 lines) | rejected attempts; remove |
| j8-chordshape | 36efad2c | none | remove |
| k1-color | 26686b8d | Color.cpp SAME | remove |
| k2-trans | 26686b8d | none | remove |
| k3-vocalpart | 26686b8d | none | remove |
| k4-charsleeve | 26686b8d | none | remove |
| l2-meshdeform | 4cd8e9bd | MeshDeform.cpp SAME | remove |
| n1-pitchdetector | 2f4a0bf1 | none | remove |
| n2-rndtext | 2f4a0bf1 | Text.cpp DIFFERS (WrapText erase-swap **rejected**) | remove |
| n3-outfitconfig | 2f4a0bf1 | OutfitConfig.cpp DIFFERS | remove |
| n4-bink | 2f4a0bf1 | none | remove |
| n5-vorbisreader | 2f4a0bf1 | none | remove |
| o1-songparser | 4856a976 | none | remove |
| o2-lightpreset | 4856a976 | LightPresetManager.cpp DIFFERS (dead-code, **rejected**) | remove |
| o3-scramblexfms | d745d94f | Utl.cpp DIFFERS | remove |
| o4-band3meta | d745d94f | none | remove |
| p1-synth | 1c7947cd | none (Synth GetSPUOverhead `*0x800` drop **rejected**) | remove |
| p2-lipsync | 1c7947cd | CharLipSyncDriver.cpp DIFFERS (refelim baseline) | remove |
| p3-tourdesc | 1c7947cd | none | remove |
| p4-clipdisplay | 1c7947cd | none | remove |
| p5-trackwatcher | 1c7947cd | TrackWatcher.cpp DIFFERS | remove |
| q1-hiresscreen | b248b3ed | HiResScreen.cpp DIFFERS (4 lines, extra try) | remove |
| q2-clipdistmap | b248b3ed | ClipDistMap.cpp DIFFERS | remove |
| q3-chardriver | b248b3ed | CharDriver.cpp DIFFERS (MaxEq swap **rejected**) | remove |
| q4-bandperformer | b248b3ed | none | remove |
| q5-applabel | b248b3ed | AppLabel.cpp DIFFERS | remove |
| r1-charactertest | 4d8a0ec9 | none | remove |
| r2-uiliststate | 4d8a0ec9 | none | remove |
| r3-quatspline | 4d8a0ec9 | Key.cpp DIFFERS | remove |
| r4-tail | 4d8a0ec9 | none | remove |
| r5-gemtrack | 4d8a0ec9 | GemTrack.cpp DIFFERS | remove |
| s1-game | 86329459 | GamePanel.cpp DIFFERS | remove |
| s2-app | 86329459 | App.cpp DIFFERS | remove |
| s3-banddirector | 86329459 | BandDirector.cpp DIFFERS | remove |
| s4-storeoffer | 86329459 | none | remove |
| s5-bitmap | 86329459 | RndBitmap.cpp DIFFERS | remove |
| t1-charbonessamples | 1403a1bb | CharBonesSamples.cpp DIFFERS | remove |
| t2-charbones | 1403a1bb | CharBones.cpp DIFFERS (`fz` constant **kept** in master) | remove |
| t3-layerdir | 1403a1bb | none | remove |
| t4-crowd | 1403a1bb | Crowd.cpp DIFFERS (Draw3DChars sign-flip **rejected**) | remove |
| t5-charhair | 1403a1bb | none | remove |
| u1-memmgr | cb5d573c | none | remove |
| u2-chareyes | cb5d573c | none | remove |
| u3-spotlight | cb5d573c | SpotlightDrawer.cpp DIFFERS | remove |
| u4-charbones2 | cb5d573c | CharBones.cpp DIFFERS | remove |
| u5-songdata | cb5d573c | SongData.cpp DIFFERS (AddLanes arg-swap **rejected**) | remove |
| v1-streaktracker | 1c237eff | StreakTracker.cpp DIFFERS | remove |
| v2-bandstorepanel | 1c237eff | none | remove |
| v3-rndmesh | 1c237eff | Mesh.cpp DIFFERS | remove |
| w1-bandfacedeform | 2603b529 | none (AT_LIMIT marked) | remove |
| w2-nextsongpanel | 2603b529 | NextSongPanel.cpp SAME | remove |
| w3-bandcharacter | 7878573d | BandCharacter.cpp DIFFERS (pre-WIP-merge baseline) | remove |

### Rejected experiments captured for the record

Several worktrees retain rejected permuter/agent attempts. These were
**not** ported to master because they changed behaviour:

- **n2-rndtext** `WrapText`: agent labeled "neutral" an erase-arg-swap
  (`erase(end, begin)` clears nothing).
- **o2-lightpreset** `if (pnew)`: only matched when a dead branch was added.
- **p1-synth** `GetSPUOverhead`: permuter dropped a `* 0x800` multiply.
- **q3-chardriver** `MaxEq`: argument order swap.
- **t4-crowd** `Draw3DChars`: `-(a-b)` → `-(b-a)` sign flip.
- **u5-songdata** `AddLanes`: argument swap.

If they're useful as anti-examples for future agent prompts, they're recorded
in the relevant memory files (`feedback_*.md`).

## Non-session worktrees (NOT removed)

These pre-date this session — leave alone:

- `gemplayer-hit` (older `wt-gemplayer-hit`)
- `validate-rb3`, `validate-regorder`, `validate-regorder-v2`, `validate-regorder-v3` (validation harnesses)
- `og-rb3` (detached HEAD reference checkout)

## Agent worktrees (.claude/worktrees/agent-aXXXXX*)

129 worktrees total, 128 locked. These are managed by FleetView (the per-agent
isolation system) — not session worktrees, separate disposition. See
`agent-worktrees-audit.md` (sibling file).

## Reuse

The worktree naming convention works well; nothing about the layout needs to
change. To resume a sweep in the same shape:

```bash
tools/setup-worktree.sh <wave><n>-<hint>
```

`tools/setup-worktree.sh` is CoW-reflink fast (~1.5s) and copies build
inputs + symlinks `bin/`.
