# W31-SET-PLAY-DISPATCH — STATUS

**Outcome: DONE (faithful fix landed).** The on-stage band now performs in-song.
Root cause was a **source-level decomp arg-order bug** in
`BandDirector::SyncProperty`, not a missing native feature. The fix is
**UNCONDITIONAL** (no flag, NOT HX_NATIVE-gated): it brings `SyncProperty` from
99.96% to **100.0% match (all equal, Complete)** AND fixes the native band-mood
routing. Base SHA `fd119705`. Single owned TU touched: `BandDirector.cpp`.

## COORDINATOR-ACK-NEEDED (deviation from kickoff)
The kickoff expected an HX_NATIVE-gated, default-OFF new flag. The actual root
cause is a **decomp bug**; the faithful fix is to correct it unconditionally,
which also improves the Wii `.o` toward the target (100%). No new flag, no
class.json append, no default flip, no pin bump. This is strictly better than a
lever (compare the W30 `RB3_BAND_PERF_FORCE_PLAY` demo lever, which caused sit
churn 16->4373; this fix causes ZERO sit churn). Requesting coordinator ack that
an unconditional decomp-correctness fix is the right disposition here.

## STEP-0 discriminators (checkpointed before fix code)

### (i) Does the venue-mood event data exist parsed natively? — **YES**
- The per-song `.milo_xbox` carries a `song.anim` `RndPropAnim` whose SymbolKeys
  hold props `{guitar,bass,drum,mic,key}_intensity` (target = BandDirector) with
  mood symbol VALUES (`idle`/`play`/`intense`/`mellow`/`idle_realtime`/...).
  strings-confirmed in `songs/20thcenturyboy/gen/20thcenturyboy.milo_xbox`:
  `PropAnim`, `song.anim`, `intense`, `idle_intense`, `mellow`, `crowd_intense`.
- Native LOADS it: `BandDirector::OnFileLoaded` binds
  `mPropAnim = dir->Find<RndPropAnim>("song.anim")` (BandDirector.cpp:1303).
- Empirically confirmed resident with real keyframes (probe `[SETPLAY_KEYS]`):
  `guitar_intensity`=4, `bass_intensity`=4, `drum_intensity`=7, `mic_intensity`=35
  keyframes (`key_intensity` absent for this song).
- The `.mid` authoring source carries 107 mood text events
  (`[play]`x37, `[intense]`x20, `[idle]`x38, `[idle_realtime]`x6,
  `[idle_intense]`x4, `[mellow]`x2).

### (ii) Who should send set_play? — the song.anim intensity keys (data-driven)
Faithful chain, entirely present + matched in-tree (NO C++ code writes `set_play`):
```
song.anim <inst>_intensity SymbolKey fires at frame
 -> BandDirector::SyncProperty SYNC_PROP_SET(<inst>_intensity, .., SendMessage(<inst>, mood))   [100% after fix]
  -> BandDirector::SendMessage -> TheBandWardrobe->SendMessage(inst, mood)                        [BandWardrobe 100%]
   -> matches "<inst>" against player_<inst>0 venue names, sends the MOOD message to that char
    -> char CHAR_COMMON DTA handler (char/char_objects.dta:1635-1647): e.g. intense -> {$this set_play kPlayIntense}
     -> BandCharacter::OnSetPlay (BandCharacter.cpp:4512): mPlayFlags = mPlayFlags & 0xFFF80FFF | newmask
      -> PlayMainClip GetClip(mask) resolves stand_rhythm_* / stand_solo_* (P/PI/PS classes)
```
The in-song pump ALREADY works: `WorldDir::Poll` -> `select_camera` ->
`BandDirector::OnSelectCamera` -> `mPropAnim->SetFrame` advances the anim
(camshots fire from the same tick). So the pump was NEVER the gap.

### (iii) A5 perf-clip prop-tip enumeration — cones/fans leg **RE-SCOPED (pre-agreed)**
The performance clips (`stand_rhythm_*`, `stand_solo_*`) are **body** clips
(torso/arms/head). Drumstick-tip / guitar-neck / mic PROP-tip bones are driven by
the separate instrument-MIDI drivers (`strum.dmidi`, `fret.ikmidi`,
`right_hand.dmidi`, CharDriverMidi/CharIKMidi), NOT by the mood/`set_play` stream.
Per A5 ("if perf clips carry NO prop-tip tracks, that leg is pre-agreed
re-scoped"): the cones/fans prop-tip stick-fans are the **F1 undriven-prop-bone
family**, a separate charter. The kickoff itself frames the floating-legs report
as "dead set_play stream (Lane A) + undriven prop bones (F1)"; this lane fixes the
set_play half (body performance), F1 owns the prop-tip half.

## Root cause (the bug)
`BandDirector::SyncProperty` intensity handlers were decompiled with the
`SendMessage` arguments swapped:
- **was (buggy):** `SendMessage(_val.Sym(), "guitar")`  ==  `SendMessage(mood, inst)`
- **now (fixed):** `SendMessage("guitar", _val.Sym())`  ==  `SendMessage(inst, mood)`

`BandWardrobe::SendMessage(s1, s2)` (100% matched) matches **s1** as a substring of
the `player_<inst>0` venue names and sends **s2** as the message. With the buggy
order, the mood was matched against char names (only `play` ever matched, via the
`play` substring of `player_`) and the *instrument name* was sent as the message —
which no `CHAR_COMMON` mood handler catches, so `set_play` never fired. The
sibling `part{2,3,4}_sing` handlers already used the correct `(inst, mood)` order,
corroborating the intended arg order. Retail asm confirms it: at all 5 intensity
call sites the mood (`_val.Sym()`) lands in **s2** and the instrument literal in
**s1**. Swapping the source removed the sole 99.96% residual (an r4<->r5
REGISTER_SWAP at exactly the 5 `SendMessage` sites) -> **100.0% all equal**.

## Fix
`src/system/bandobj/BandDirector.cpp` — 5 `SYNC_PROP_SET` intensity lines
(bass/drum/guitar/mic/keyboard) swapped to `SendMessage("<inst>", _val.Sym())`.
Plus a read-only, default-OFF HX_NATIVE diagnostic (`RB3_SETPLAY_PROBE` ->
`[SETPLAY_KEYS]` one-shot key census; `[SETPLAY_SEND]` per-dispatch log) as the
mechanism's acceptance instrument (Wii `.o` unaffected — HX_NATIVE never defined
for MWCC). No new persistent flag.

## Acceptance (songMs-matched --fixed-clock A/B; verbatim-quoted criteria)

> "with the DEMO LEVER OFF, sustained rhythm/solo CHARDRV_PLAY census (Lane-1 A/B
> rerun, OFF=W29-idle baseline); no sit-group churn (E3 bound); F1 gameplay retest
> — drumstick/prop-tip bones driven, cones/fans gone or explicitly re-scoped."

| metric | BASELINE (buggy args) | FIX (correct args) | verdict |
|---|---|---|---|
| perf plays (`stand_rhythm_*`/`stand_solo_*`, CHARDRV_PLAY) | **3** (only `stand_rhythm_cam_s_04`, camera-triggered) | **80** | PASS |
| perf plays per songMs quartile | idle-dominated | **[25,16,20,19]** (4/4, >=3 required) | PASS (A3) |
| distinct intensities dispatched (`[SETPLAY_SEND]`) | mood mis-routed | **play, idle, intense (3)** | PASS (A3: >=2, not constant) |
| drummer `idle_play_*` performance idles | 0 | 14 | PASS |
| sit-group `BANDPERF_STATE` (E3) | 24 | **26** (<= 10x OFF=160; ~flat) | PASS (A4) |
| total `BANDPERF_STATE` (E3) | 119 | 131 (~1.1x OFF, <= 2x) | PASS (A4) |
| `SyncProperty` objdiff | 99.96% | **100.0% all equal** | PASS |

- **>=2 distinct intensities tracking authored events, NOT a constant** — met
  (play/idle/intense), so this is the faithful dispatch, not the lever re-badged.
- **No sit-group churn** — met emphatically (24 vs 26; contrast the W30 demo
  lever's 16->4373). The fix does NOT retrigger the sit group.
- **cones/fans** — pre-agreed RE-SCOPED to F1 (A5): the set_play stream drives
  body performance clips (torso/arms/head); prop-tip fans are a separate
  instrument-MIDI-driver family.

### A7 probe-count table (grep -c on committed gz)
| tag | base | fix |
|---|---|---|
| BANDPERF_STATE | 119 | 131 |
| BANDPERF_CLIP | 171 | 156 |
| CHARDRV_PLAY | 349 | 327 |
| SETPLAY_KEYS | 5 | 5 |
| SETPLAY_SEND | 26 | 26 |
| stand_rhythm/solo plays | 3 | 80 |

## Evidence (on-disk, gitignored)
- `evidence/raw/{fix,base}_census.log.gz` — songMs-matched A/B census (--fixed-clock).
- `evidence/raw/probe_localization.log.gz` — pre-fix localization probe.
- `evidence/shots/{fix,base}_gp0{05,10}.png` — matched-songMs gameplay shots.
  (Camera frames the track, not the full band; the census is the validated oracle.)

## Boot budget: 3 boots (probe localization, FIX A/B, BASE A/B).

## Flags / class.json / pin
NONE. No new flag, no class.json append, no default flip, no pin bump. The
`RB3_SETPLAY_PROBE` diagnostic is read-only and env-presence-gated (not a
persistent behavior flag). `RB3_BAND_PERF_FORCE_PLAY` retirement remains
COORDINATOR-executed at close-out (not touched by this lane).

## COORDINATOR ACK (close-out, 2026-07-12)

UNCONDITIONAL-fix disposition ACKED as correct: the root cause was a decomp
source bug (SyncProperty intensity SendMessage arg order), not a missing native
feature — fixing it moves the Wii .o to 100.0% (Complete) AND restores in-song
band performance natively. An HX_NATIVE gate would have been wrong (it would
fork faithful behavior). RB3_BAND_PERF_FORCE_PLAY demo lever retired this
close-out: lever block deleted from BandCharacter.cpp (HX_NATIVE-only, Wii .o
untouched) + registry row removed (engine 24c4f95), per the E-C2 zombie-lever
precedent.
