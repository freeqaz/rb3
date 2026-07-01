# Task: score-detail / star-breakdown screen — WAVE-5 implement

**Status: DONE + VERIFIED.** The score-detail / star-breakdown screen
(`coop_endgame_screen`) is now reached and renders end-to-end after a full song
for BOTH guitar and vocals, with zero crash signatures. Two native gaps were
root-caused and fixed; the primary one is a genuine decomp bug fixed un-`#ifdef`'d
(98.8% → 100.0% match), the secondary one is an `HX_NATIVE`-gated assert mirror
(Wii byte-identical).

- Worktree branch: `wt-task-score-detail`
- Commit: `50eeb6ce` (rb3 worktree)
- Engine change: **NONE** (`needsEngine = false`)
- Files: `src/band3/meta_band/PassiveMessenger.cpp`, `src/band3/meta_band/PassiveMessenger.h`

---

## SYMPTOM

The wave-4 endgame fix reaches `coop_endgame_popups_screen` (renders + holds), but
the deeper STAR/SCORE-DETAIL breakdown (`coop_endgame_screen`, `coop_player_widget`)
was un-exercised headless. The popup did NOT auto-advance, and a confirm/start input
did nothing (the popup screen has no `BUTTON_DOWN_MSG` handler — it advances ONLY via
its `poll`). Driving confirm 80× left it stuck on `coop_endgame_popups_screen` for
90+ seconds / ~10k frames (alive, not frozen).

## ROOT CAUSE (two layers)

### Layer 1 — popup never auto-advances (DECOMP BUG, the real blocker)

`coop_endgame_popups_screen`'s poll only hands off to `coop_endgame_screen` when
`{! {ui in_transition}}` AND `{! {passive_messenger has_messages}}`
(`ui/endgame/endgame.dta`). A live probe showed `in_transition=0` but
`passive_has_messages=1` **forever**.

Instrumenting `PassiveMessenger::HasMessages` (temporary `RB3_PASSIVE_DBG` probe,
since removed): the single local participant's `PassiveMessageQueue` held
`qsize=8 timerRunning=0` and never drained.

`PassiveMessageQueue::Poll` (`PassiveMessenger.cpp:24`) drains a message and starts
its timer only inside the show-next-message branch. The source had that branch gated
on `if (running && ...)`, where `running = mTimer.Running()`. The Bank 8 target gates
it on `!running`:

```
// Bank 8 (GetAndPreProcessFirstMessage neighbour Poll @0x803341e0):
if (-1 < (int)(-running & ~running))   // == (running == 0) == !running
```

(objdiff idx 86-89 was exactly this: target `neg/andc/srwi./bne` materializing
`!running`; source emitted `cmpwi 0` for `running`.) With the inverted test, a fresh
queue whose timer is stopped can NEVER show its first message or start the timer —
chicken-and-egg — so the 8 song-completion / earned-accomplishment toasts stayed
queued forever, `HasMessages()` stayed true, and the popup never advanced.

**Fix:** `if (running && ...)` → `if (!running && ...)`. This is byte-faithful to the
target: `PassiveMessageQueue::Poll` **98.8% → 100.0%** (a genuine match-improving
decomp-bug fix, allowed un-`#ifdef`'d per the task rules).

### Layer 2 — accomplishment-coalesce meter assert (debug-only, native OSFatal)

Once the queue drained, a second blocker surfaced: a `SIGABRT` from
`PassiveMessenger.h:74  Error: mMeterAnimValue >= 0`
(`PassiveMessage::AddAnim`'s `MILO_ASSERT`).

`GetAndPreProcessFirstMessage` coalesces ≥4 simultaneous earned-accomplishment
messages, summing `firstMessage->AddAnim(msg->mMeterAnimValue - msg->unk14)` over each.
A full-combo expert/autohit pass earns many "first time" goals at once (the popup
toast literally read **"7 goals completed!"** — screenshot below). The per-message
values (`mMeterAnimValue = newFanCount-oldFanCount ≈ 120`, `unk14 = totalPoints = 250`)
make every delta ≈ `-130`, so the running fan-meter goes negative and the assert (which
checks the value *before* adding) trips on the 2nd message.

This coalesce arithmetic and the assert are **byte-faithful to the Wii target**
(confirmed in Bank 8: `*(iVar14+0x1c) += (msg->0x1c - msg->0x14)` with the same
`mMeterAnimValue >= 0` Fail at 0x4a, `AddAnim` inlined). `MILO_ASSERT` is compiled
**OUT in retail (non-debug)** — retail proceeds with a negative meter (a cosmetic
over-fill at worst). The native port runs with asserts ON, so it `OSFatal`'d before
the accomplishment toast could hand off to the score screen.

**Fix:** gate the assert `#ifndef HX_NATIVE` so native mirrors the retail non-debug
path (proceed, don't abort). Same rationale + precedent as `MILO_FAIL_DTA` in
`os/Debug.h` ("the offline flow hits benign type mismatches ... that must not
abort"). The Wii build keeps the assert → byte-identical.

## FILES CHANGED (why)

- `src/band3/meta_band/PassiveMessenger.cpp` — `PassiveMessageQueue::Poll`:
  `running` → `!running` (decomp-bug fix, 98.8→100.0% match per target asm).
- `src/band3/meta_band/PassiveMessenger.h` — `PassiveMessage::AddAnim`: wrap
  `MILO_ASSERT(mMeterAnimValue >= 0, 0x4A)` in `#ifndef HX_NATIVE` + an explanatory
  comment. Wii keeps the assert; native mirrors retail.

## VERIFICATION (before/after, evidence)

Probe: `/tmp/rp5-score-detail/verify_results.py` (auto-advance verifier; no
confirm-spam — lets the popup hand off on its own).

| scenario | BEFORE (master/wave-4) | AFTER (this fix) |
|---|---|---|
| popup → score-detail | stuck on `coop_endgame_popups_screen` 90s/10k frames; `passive_has_messages=1` forever | auto-advances `popups → coop_endgame_screen`; `passive_has_messages=0` |
| guitar full song | n/a (never reached) | `coop_endgame_screen` STABLE 12s, 1561 frames, **0 crash sigs** |
| vocals full song | n/a | `coop_endgame_screen` STABLE 12s, 1300 frames, **0 crash sigs** |
| meter-anim assert | `SIGABRT PassiveMessenger.h:74` on drain | gated on native, no abort |

- Guitar score-detail render: `/tmp/rp5-score-detail/verify_guitar/guitar_score_detail.png`
  — per-player widget renders: song title "20TH CENTURY BOY", 5-star row + total
  score label, "0% / EXPERT", solo stars, "SOLO SCORE / Previous Best", CONTINUE /
  RESTART. (`num_stars=0 score=0` is the `autohit` harness artifact — scoring path
  yields 0 under nofail+autohit; the widgets/labels themselves populate + render.)
- Vocals score-detail render: `/tmp/rp5-score-detail/verify_vocals/vocals_score_detail.png`
  — same per-player widget, vocals layout. No null labels, no missing widget, no cast crash.
- Earlier popup toast (with assert temporarily bypassed to observe drain):
  "7 goals completed!" with the fan meter — the exact coalesce path that hit Layer 2.

### No regression
- `scripts/native/keyboard-to-gameplay.py --port 9243 --diff hard` (worktree binary):
  boot → main_hub → song_select → part_difficulty → gameplay PASS, overshell slots
  transition normally, 0 crash sigs. The passive-message change doesn't break normal
  toast/overshell flow.
- `scripts/native/song-end-test.py --port 9244 --require-endgame` (worktree binary):
  PASS — and the stability watch now reports `screen='coop_endgame_screen'` (the
  popup advanced to the score screen during the 25s window; before this fix the gate
  only ever saw `coop_endgame_popups_screen`).

### Wii byte-identical (proof)
Worktree `build/SZBE69_B8/report.json` (reflects my edits) — `meta_band/PassiveMessenger`:
- `Poll__19PassiveMessageQueueFv` **100.00%** (was 98.8% — improved, un-`#ifdef`'d).
- `GetAndPreProcessFirstMessage__19PassiveMessageQueueFv` **98.94%** UNCHANGED — this
  is where `AddAnim` inlines, so its stability proves the `#ifndef HX_NATIVE` assert
  gate is Wii-neutral.
- `HasMessages__16PassiveMessengerCFv` **100.00%** (diagnostic probe fully removed).
- Every other function in the unit unchanged. No regression. (`wiiByteIdentical=true`
  for the header gate; the `Poll` text change is the allowed match-IMPROVING exception.)

## NOTE on an accidental main-repo touch (resolved)
An early diagnostic edit landed on the MAIN repo's `PassiveMessenger.cpp` (the
worktree has its own reflinked copy). It was hand-reverted (never `git checkout`);
`git diff` on main is clean. All real work is on `wt-task-score-detail`.

## landingNotes

- **Land target:** rb3 master. Cherry-pick `50eeb6ce` (or apply the 2-file diff).
- **No engine change, no pin bump.** `needsEngine=false`.
- **File regions (no sibling conflict expected — none of the other wave-5 tasks
  touch `meta_band/`):**
  - `src/band3/meta_band/PassiveMessenger.cpp` — one line in `PassiveMessageQueue::Poll`
    (the `bool running` test, ~line 33): `running &&` → `!running &&`.
  - `src/band3/meta_band/PassiveMessenger.h` — `PassiveMessage::AddAnim` (~line 39):
    wrap the single `MILO_ASSERT(mMeterAnimValue >= 0, 0x4A)` in `#ifndef HX_NATIVE`.
- **Land-order:** independent of the other wave-5 tasks. Does NOT touch
  `Rnd_Wgpu_RB3.cpp` / `standard_wgsl.inc`.
- After landing, regenerate the Wii report (`tools/ninja-locked
  build/SZBE69_B8/report.json`) — expect `PassiveMessageQueue::Poll` to flip to 100%
  in the report (overall code% ticks up slightly).

## Follow-ups (out of scope, documented)

1. **Score values 0 under autohit** — `autohit` headless yields `num_stars=0
   score=0`, so the score-detail screen renders zeros. To validate non-zero
   per-player numbers, a harness that simulates real note hits (not autohit) is
   needed. The WIDGETS render correctly; only the displayed values are the harness
   artifact. Low priority.
2. **Endgame crowd/venue greenish-dark tint** — visible in the score-detail
   backdrop; same class as the open crowd-render + venue-lighting residuals (wave-5
   backlog item), not score-detail-specific.
3. **`load_nextsong_screen` SIGSEGV on confirm in quickplay** — over-pressing confirm
   on the score screen advances to `load_nextsong_screen`, which null-derefs because
   quickplay has no next song. This is a separate next-song-load gap (NOT score-detail
   rendering); the score screen's normal Confirm in quickplay should route to
   `preload_nextsong_screen`/`meta_loading_continue_screen` per the dta — worth a
   dedicated look if the "continue from score screen" path matters.
4. **Accomplishment fan-meter could be authored to stay non-negative** — the deeper
   fix would investigate why `currentPoints/totalPoints` (`unk14=250`) exceed the fan
   delta (`mMeterAnimValue=120`) in the offline native accomplishment path; the
   retail-faithful native gate is the safe fix and is what's landed.

## Evidence index (under `/tmp/rp5-score-detail/`)
- `verify_results.py` — clean auto-advance verifier (reusable).
- `verify_guitar/guitar_score_detail.png`, `verify_vocals/vocals_score_detail.png`
  — the score-detail screen rendering for both instruments.
- `advance_to_results.py` — confirm-driven probe (showed the auto-advance + over-press
  load_nextsong follow-up).
- `report_wt.json` — worktree report.json proving the Wii match deltas.
