# WAVE 32 — PRE-E1 COUNTERSIGN

Opus countersign agent, 2026-07-12. Method (W27 lesson): every headline number
re-derived from **raw committed artifacts** (git commits, gz/json evidence dumps,
committed verdict.json, engine.log, drawlog/uidump json, source, report.json) —
never from an agent's prose. Independent `batch_objdiff` run over Lane D's landed
symbol + unit siblings and Lane B's neutrality set. Flags for numbers living only
in `/tmp` or a build dir called out in §Evidence-honesty gaps.

Base SHA reconciliation: kickoff top-line reads `11d1ad1b`, but the **COORDINATOR
ACCEPTANCE** block binds "Base SHA for all lanes = this acceptance commit" =
`30546499` (verified present in git log; its message is the A1–A14 acceptance).
All five lane STATUS files correctly cite `30546499`. Not a gap — the acceptance
override supersedes the stale top-line. Engine pin `24c4f95` is an engine-repo SHA
(not resolvable in rb3 — expected).

---

## Per-lane claim → re-derived → verdict

### Lane A — W32-WEB-YELLOW (blocked; STEP-0, COORDINATOR-ACK-NEEDED)

| Claim (STATUS/checkpoint) | Re-derived from committed artifact | Verdict |
|---|---|---|
| Commit `41d52acb` landed | `git log` → "W32-WEB-YELLOW STEP-0: name hub highlight-bar orphan quad…" | MATCH |
| Quad = `highlight_main.mesh`/`highlight_pattern.mesh` hub highlight bar | Named from engine source comment `Rnd_Wgpu_RB3.cpp:3233-3237` (quoted verbatim in STATUS) | MATCH (source-comment sourced, not live dump — see gaps) |
| Divergence root: render-hook TU filtered out of web build, native/CMakeLists.txt native-only include + web exclusion | `native/CMakeLists.txt:455` = `${CMAKE_SOURCE_DIR}/src/rb3_render_hook.cpp` (native); `:819` comment "rb3_render_hook.cpp — HamRenderHook glue…; not needed for clear-frame" (web excl.) — both lines present verbatim | MATCH |
| A10 replay: focus tracks ArrowDown/Up both dirs, quad static | `evidence/web/joined_00..04_*.png` on disk (default + 2×ArrowDown + 2×ArrowUp); STATUS quotes engineState focus chain | MATCH (present) |
| Divergence = mechanism (i) → A2 STOP before fix | rb3_render_hook.cpp git-clean (Lane C exclusive TU); no fix applied | MATCH |
| Native control (drawlog-golden PASS) + after-fix web pair | **NOT captured** — STATUS admits native boot stalled at splash; legs (3)(4)(5) DEFERRED post-arbitration | GAP (see §1, legitimate A2 deferral) |

### Lane B — W32-PROP-FAN (done; branch (b) STARVED, fix landed default-OFF)

| Claim | Re-derived | Verdict |
|---|---|---|
| Commit `3ed6118a` (rb3) + `c0bc00a` (engine) | `git log` → "char/native: fix starved instrument-MIDI prop drivers (W32-PROP-FAN, F1)" | MATCH |
| Fix on owned surfaces only | `git show --stat 3ed6118a` → `CharDriverMidi.cpp` (+79/-…), `CharIKMidi.cpp` (+16), + PLAN/STATUS only | MATCH (A7 owned) |
| Baseline census: drums drops_band=1107 (ratio 5.92), guitar 843 (5.15), vocals 0 | committed `evidence/baseline/{drums,guitar,vocals}/verdict.json`: drops_band 1107/843/0, max_band_ratio 5.92/5.15/3.14 | MATCH |
| After-fix: drums 1107→2 (residual `wovensteppers`, SKEL), guitar 843→0 | committed `evidence/fix-on/{drums,guitar}/verdict.json`: drops_band 2 (wovensteppers_skin.2.mesh:2) / 0 | MATCH |
| Wii objdiff neutrality: Poll/Enter/__ct__ CharDriverMidi = 100.0 | independent batch_objdiff: `Poll__14CharDriverMidiFv` 100.0, `Enter__14CharDriverMidiFv` 100.0, `__ct__14CharDriverMidiFv` 100.0 | MATCH |
| Pre-existing residuals unchanged: OnMidiParser 99.02, NewSpot 98.41 | report.json: `OnMidiParser__14CharDriverMidiFP9DataArray` 99.02, `NewSpot__10CharIKMidiFP16RndTransformablef` 98.41 | MATCH |
| Per-prop-class verdict (A8, not aggregate) | STATUS table: sticks / kit cones / guitar neck / stick-fan guitar / vocals — 5 pointer-keyed rows | MATCH (per-class, no aggregate-only row) |
| Discriminator probe: CTOR 23/ENTER 0/POLL 17/FEED 0 → ENTER 0→21, FEED 0→173, clips 0→416, SETPLAY 26, CHARDRV_PLAY 81/80 | quoted in tracked STATUS; **source logs in `/tmp/rb3-bandcloseup-*.log`** (per verdict.json `log` field), NOT under evidence/ | GAP (see §2, quote-only mechanism counts; visual outcome corroborates) |

### Lane C — W32-HUD-F2F4 (partial; F2 real-bug engineAckNeeded, F4 faithful non-bug)

| Claim | Re-derived | Verdict |
|---|---|---|
| Commit `b0f6e3b7` (docs only, no code) | `git log` → "docs(wave-32 Lane C): F2/F4 HUD diagnosis"; rb3_render_hook.cpp git-clean | MATCH |
| F2 HEADMAT dump: sb_refract (0.35,0.35,0.35,a=0.90) blend=3; sb_bg (1,1,1,a=1) blend=3; sb_lens (0.70,0.70,1,a=0.90) blend=4; all prelit=0 useEnviron=0 | committed `evidence/headmat/engine.log` — three lines match byte-for-byte | MATCH |
| F2 pixel proof (native pill≈venue; retail opaque dark) | `evidence/native_pill_exact.png`, `retail_pill_star.png` on disk (pixel samples quoted in STATUS, not independently re-sampled) | MATCH (images present) |
| F4 A11 two-earned-count: native 0/1/3 + retail 2/4 | 5 committed PNGs: `F4_native_{0,1,3}star_*.png`, `F4_retail_{2,4}star_*.png` | MATCH (A11 over-satisfied) |
| F4 faithfulness: ResetStars/SetupStars/SyncObjects/Reset 100.0, SetNumStars 94.56 | report.json BandStarDisplay: 100.0/100.0/100.0/100.0, `SetNumStars…` 94.56 | MATCH |
| F4 source: reveal `SetShowing(true)` + hide `if(i>0)…SetShowing(false)`, 5-star loop | `BandStarDisplay.cpp:36` SetShowing(true), `:108` SetShowing(false), `:82` for i<5, `:83` MakeString("star%d") | MATCH |
| A13 lint-4 registry sweep BEFORE "engine drops X" | STATUS §"A13 lint-4 registry sweep — CLEAN" enumerates 8 flags, none touching scoreboard_refract/_bkgrnd/BandStarDisplay | MATCH (present, in-STATUS) |

### Lane D — W32-ARG-ORDER (done; class EXHAUSTED, 1 sub-100 behavioral find)

| Claim | Re-derived | Verdict |
|---|---|---|
| Commit `ad0130f4` | `git log` → "W32-ARG-ORDER: fix VocalTrackDir::SetRange SetFrame arg order…" | MATCH |
| Fix = 1-line swap `SetFrame(1.0f,0.0f)`→`SetFrame(0.0f,1.0f)` at VocalTrackDir.cpp | `git show ad0130f4` → single `-`/`+` line, exactly this swap; 2 doc files + 1 src line | MATCH |
| SetRange sub-100 after fix: raw 99.2782 (NOT 100) | **independent batch_objdiff**: `SetRange__13VocalTrackDirFffib` = raw 99.28% BORDERLINE (not COMPLETE) | MATCH (honest sub-100 disclosure) |
| A5 unit neutrality: no sibling regressed (ConfigPanels 99.860 unchanged) | **independent batch_objdiff** over unit: ConfigPanels 99.86, Copy/Save/Load/__ct__ 100.0 | MATCH |
| A4 report regen 2026-07-12 07:09:53; enumeration 1185 in-scope | timestamp + count quoted in STATUS + checkpoint (report.json regen; not independently re-timed) | MATCH (in-artifact) |
| STOP: 0/1185 clean-raw-100 arg-order landings | methodology/classifier result (checkpoint classifier_counts; evidence gzipped) — not independently re-swept | ACCEPTED (bounded claim; the one landable was found behaviorally not by sweep) |
| VocalTrackDir SetFrame retail-byte-verified at [140-147] | STATUS quotes post-fix objdiff [144-145] matching retail; consistent with 99.28 partial | MATCH |

### Rider — W32-F7-CLIP (done; diagnosis-only, no code)

| Claim | Re-derived | Verdict |
|---|---|---|
| Commit `d07738cb` (docs only) | `git log` → "docs(wave-32 F7-CLIP): song-select sidebar bleed-through mechanism memo"; claims file empty (owns no TU) | MATCH |
| `header_list_bg.mesh` full-screen 50% dimmer, matColor==boundColor==[0,0,0,0.5], rect [0,0,1280,720], pass 1 | committed `evidence/c_drawlog_full.json`: exact record `matColor:[0,0,0,0.5] boundColor:[0,0,0,0.5] rect:[0,0,1280,720] pass:1` | MATCH |
| Backing meshes draws=0 (difficulty_bg*, raitings_bg, leaderboards_bg, live_diffs.grp) | committed `evidence/c_uidump_song_select.json` contains all named meshes (census re-confirms W4.3-C2a) | MATCH |
| Not camera/scissor/viewport clip; missing-panel asset gap | drawlog character rects all inside [0,1280]×[0,720] (STATUS-quoted from committed drawlog) | MATCH |

---

## Evidence-honesty gaps

**§1 — Lane A: native CONTROL never captured; quad name is source-comment-sourced,
not live-dump-sourced (LOW severity, legitimate A2 deferral).** STATUS transparently
records that the headless native boot stalled at `splash_screen`/`intro_movie_screen`
inside 180s, so the mandated native `/api/uidump` + `/api/drawlog` control (the
"expected hub highlight/overshell mesh set" cross-name, and the countersign's
"drawlog-golden PASS") were **not** produced. The quad's identity rests on the engine
source comment at `Rnd_Wgpu_RB3.cpp:3233-3237` (authoritative but a comment, not a
runtime observation). Acceptance legs (3) after-fix web pair, (4) native control
unchanged, (5) B8 non-regression are all **DEFERRED** to the post-arbitration fix leg.
This is correct under A2 (STOP before touching the render-hook family, Lane C
exclusive) — it is a coverage deferral, not a fabricated number — but the countersign
cannot confirm the native-side control or any after-fix evidence because none exists
yet. The A10 replay evidence (both-direction focus travel, quad static) **is** present.

**§2 — Lane B: mechanism probe counts live only in /tmp (LOW-MEDIUM severity, outcome
corroborated).** The decisive discriminator numbers — `CTOR 23 / ENTER 0 / POLL 17 /
FEED 0`, and post-fix `ENTER 0→21`, `OnMidiParser FEED 0→173`, `drum-hit clips 0→416`,
`SETPLAY_SEND 26`, `CHARDRV_PLAY 81/80` — are quoted in tracked STATUS (satisfying the
E4/lint-7 quote convention) but their **source logs are `/tmp/rb3-bandcloseup-*.log`**
(named in the committed verdict.json `log` fields), i.e. ephemeral and NOT committed
under `evidence/`. This is the W31-Lane-C locality class. **Severity is bounded** because
the *visual outcome* those counts explain — the shard census (drums drops_band 1107→2,
guitar 843→0, vocals 0) — IS committed in `verdict.json` and was independently
re-derived here, and the Wii objdiff neutrality (Poll/Enter/ctor 100.0) is independently
re-run. So the fix's effect and Wii-safety are fully verifiable; only the internal
event-count mechanism trace is quote-only. Recommend the coordinator, before the
default-ON flip, either re-home one MIDIDRV_PROBE ON/OFF log pair under `evidence/` or
accept the STATUS quote as sufficient (the census outcome already carries the flip's
ON-vs-OFF earn).

**No fabrication or build-dir-only headline number found.** Every landed source edit is
committed (working tree clean for all lane TUs). Lane C wrote zero code (rb3_render_hook.cpp
git-clean, as claimed). Lane D's sub-100 result is honestly disclosed and independently
reproduced (99.28%, not 100). All Lane B/C/D/F7 census, objdiff, HEADMAT, and drawlog
numbers re-derive cleanly from committed artifacts.

## Countersign verdict

**PASS with two coverage flags (§1 Lane A native control deferred per A2; §2 Lane B
probe counts /tmp-only, outcome committed).** No headline number contradicts its raw
artifact. Lane D's 100.0-gate honesty and A5 unit neutrality independently confirmed.
Ready for E1 close-out; coordinator should note §1/§2 when adjudicating the Lane A
arbitration and the Lane B default-ON flip.
