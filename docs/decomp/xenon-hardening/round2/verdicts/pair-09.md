# Pair 09 verdict — SetGameOver__4GameFb ↔ Xenon 0x8265a168

**Verdict: CORRECT — confidence HIGH**

## Claim
Wii `SetGameOver__4GameFb` (Game::SetGameOver(bool), Bank8 0x80181720, TU Game.o)
== Xenon `Function_8265A168` (0x8265a168). Match type BSIM, sim×conf 28.524
(similarity 0.898 / confidence 31.764).

## Decisive evidence — 3/3 callees agree, in order
The Xenon body calls exactly three functions, and all three resolve (via
matches.json) to the three callees the Wii body calls, in the **same call order**:

| order | Xenon callee | resolved Wii symbol | confirmed in Wii asm |
|---|---|---|---|
| 1 | `?SetCollectStats@AutoTimer@@SAX_N0@Z` (0x824fedf8) | `SetCollectStats__9AutoTimerFbb` | Game.s:6851 |
| 2 | `Function_82659EC0` (0x82659ec0) | `GetResult__4GameFb` | Game.s:6854 |
| 3 | `Function_823D2A58` (0x823d2a58) | `EndGame__10NetSessionFibf` | Game.s:6860 |

Three-callee agreement in order is highly distinctive and independent of the BSim
score that nominated the pair.

## Independent (non-graph) corroboration
- **MSVC-demangled callee name survives in the Xenon binary**:
  `?SetCollectStats@AutoTimer@@SAX_N0@Z` → `AutoTimer::SetCollectStats(bool,bool)`
  (`SA` static, `_N0` = bool,bool). This matches the Wii `SetCollectStats__9AutoTimerFbb`
  (class AutoTimer, args `bb`) **without** relying on our matches.json graph — it is
  a real exported/RTTI name, not a `Function_<addr>` placeholder. Strongest single
  signal.

## Control-flow / structural match
Both sides have the identical skeleton:
1. early-out: `if (field@panel != 3) { ... }`  (Xenon `*(DAT+0x74)`, Wii `TheGamePanel->0x90`)
2. inner cond on the bool arg: `if ((arg & 0xff) == 0) field_A = field_B;`
   (Xenon `*(p1+0x128)=*(p1+0xb4)`, Wii `this->0x124 = this->0xac`)
3. `SetCollectStats(0, <Rnd field>)`
4. `r = GetResult(this, arg)`  — return value flows into:
5. `EndGame(TheNetSession, r, 0, field_0x124)`

The `& 0xff` on the bool param is expected Xenon 64-bit-register narrowing.

## Divergences (all benign toolchain noise)
- Field offsets differ (panel 0x90/0x74, score 0xac/0xb4, 0x124/0x128) — expected
  MWCC-vs-MSVC struct layout difference, explicitly flagged as non-evidence in the
  substrate caveats.
- Size: Wii ~164 B (41 asm lines) vs Xenon 144 B — ~1:1, well inside the 1.0–2.5x band.

## No contradicting signals
- Neither side has referenced strings (so no string set to conflict).
- Semantics ("end the game": calls NetSession::EndGame with a result code, snapshots
  a score field, stops a stats timer) are fully consistent with the demangled Wii
  name Game::SetGameOver(bool).

## For the next agent
This is one of the cleaner ACCEPT pairs: 3/3 ordered callee agreement plus an
independently-demangled MSVC callee name (`AutoTimer::SetCollectStats(bool,bool)`)
that corroborates outside the matches.json graph. Treat as a true positive with
high confidence. No further Ghidra dig needed.
