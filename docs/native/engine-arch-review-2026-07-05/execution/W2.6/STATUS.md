# W2.6 — Foot/shoe rest-capture coverage + flag-registry cleanup — STATUS

Append-only log. Update under `flock /tmp/rb3-docs.lock`. One `## <subtask-id> — done|partial|blocked`
section per subtask with commit SHAs + blockers. Re-runs read this + `git log --grep=W2.6` and skip
done work.

Lane C (rb3-only, parallel). Engine RENDER/skinning seams READ-ONLY (same seams as W2.2:
`mNativeBonesRebound` + `RB3_GUARD_EXEMPT_REBOUND`; NO `Rnd_Wgpu_RB3.cpp` / `DrawMesh` edits).
Engine pin `6221a56` — do NOT bump (coordinator bumps once per wave).
Build dir: `native/build-agent-W2.6` (Clang). Targets `rb3-native` / `rb3-tests`.

DEFAULT-OFF for the foot/shoe fix; separate coordinator-gated flip. STOP-TRIPWIRE (PART 1): a
200-460u smear or >92u skinpos on a rebound appendage reproduces the rotation-basis failure — STOP.

## planning — done
- PLAN.md written (Opus planner). Subtasks W2.6.S1..W2.6.S4. See PLAN.md `## Subtasks`.

## W2.6.S1 — done (BRANCH = STOP / diagnosis-only)

**Build:** `rb3-native` @ engine pin `6221a56`, `native/build-agent-W2.6` (Clang) — BUILD_OK.
**Harness:** `scripts/native/hands_bind_characterize.py` x4 passes on HEAD (RELOAD/HEAD_REBIND/
SHARD_RATIO/REBIND_DRAW_SKINPOS/REBIND_DRAW_FLING/SKIN_CLAMP probes).

**Fail-red baseline reproduces (lineup-dependent).** Band lineup is randomized per boot; the
`saddleshoe_skin.2` guard-DROP appeared in 1 of 4 passes (rep3) at **4.37-4.56x DROP** — matches
W2.2's 4.73x. Canonical drop baseline committed: `W2.6/char/parsed-drop.json` (raw-drop.log local,
gitignored). No-drop lineup: `parsed-default.json`.

**PLAN's PART-1 mechanism REFUTED.** The stated cause (late lower-body bones lack a clip-free seed →
mid-clip first-resolve `why=clipPlaying` → V24 drop) does NOT occur: **0 `clipPlaying` pendings** on
HEAD AND in the W2.2 baseline's own log (both: 91 `unresolvable` + 24 `boundRebakeOff`, 0 clipPlaying).
`[REST_SEED] poll=0` already seeds 105-109 bones/member clip-free; later seeds are `added=0`. The shoe
mesh resolves distinct clip-free and **completes its rebind** (`[CHAR_MESH] rebound=1`). The DROP is a
**2/155-frame count-in transient** (W2.2: 5/190) the guard already hides — max SKINPOS 69.5u overall,
**0 fling, no ≥92u, no 200-460u tripwire**.

**Why S2 is a no-op:** the seed already covers these bones; the `RB3_HANDS_BIND_FIX`-style completion
branch is gated on `clipPlaying` which never fires; the only bind-side lever for the transient frames
is `own==bound` `RB3_BOUND_REBAKE` = the proven 200-460u rotation-basis STOP-TRIPWIRE (engine-side
draw-order/guard fix is out of PART-1's READ-ONLY scope).

**Consequence:** S2/S3 (PART-1 behavior) become NO-OPs. W2.6 lands **PART 2 (S4 flag-registry) + the
diagnosis memo** only — NO PART-1 behavior change (exit criteria 1/5 hold trivially: no flag, no code
path). **S4 should NOT introduce `RB3_FOOT_REST_CAPTURE`** (never implemented) — register only
`RB3_HANDS_BIND_FIX` + `RB3_SKEL_REBIND_FULL` + the 90 `rb3/src/system` scan-root flags.

**Deviation vs plan:** the shard is BELOW the rotation-basis tripwire (2-frame guard-hidden transient,
not >92u/200-460u); STOP holds because the targeted mechanism (clipPlaying missing-seed) is absent.
Memo: `W2.6/char/S1_DIAGNOSIS.md`. Files touched: `W2.6/char/*` artifacts only (no source).
