# Pair 27 verdict — DifficultySortPart__12MusicLibraryCFv

**Verdict: CORRECT (confidence HIGH)**

- Pair: 27
- Wii: `DifficultySortPart__12MusicLibraryCFv` @ `0x80300e10` (Bank 8; the
  evidence-pack header's `0x802ff430`/`0x80300e10` confusion is cosmetic — CW map
  line 14600 puts the symbol at `80300e10`, size `0xec`, in `MusicLibrary.o`).
- Xenon: `Function_8252C728` @ `0x8252c728` (stripped XEX, auto-name).
- Match type: `SwitchSigHasher` (non-BSim, simconf n/a).
- Evidence pack: `docs/decomp/xenon-hardening/round2/evidence/pair-27.md`

## Decisive evidence — switch-case equivalence-class fingerprint is identical

`DifficultySortPart` reads a score-type enum (0..10) and maps it to a per-type
`Symbol`, with a `Fail`/null-Symbol default. The distinctive fingerprint is the
**case-merge pattern**: of the 11 cases, two pairs collapse to the same result
and all others are distinct.

Wii jump table `@72368[0xB]` (from the Bank-8 asm) →
`{0:drum, 1:bass, 2:guitar, 3:vocals, 4:vocals, 5:keys, 6:drum, 7:real_guitar,
8:real_bass, 9:real_keys, 10:band}`.

Xenon `switch(uVar2)` →
`{0:d8, 6:d8, 1:dc, 2:e0, 3:d4, 4:d4, 5:d0, 7:cc, 8:c8, 9:c4, 10:e4}`.

Reduced to equivalence classes (cases sharing the same result), BOTH sides give:

```
[(0,6), (1,), (2,), (3,4), (5,), (7,), (8,), (9,), (10,)]   # MATCH = True
```

11 cases, the **same two specific merges** `{0,6}` (drum) and `{3,4}` (vocals),
every other case distinct, plus a default-fail path. A coincidental match of this
exact partition between two unrelated functions is astronomically unlikely. This
is exactly what `SwitchSigHasher` keys on, and it is a true positive here.

## Corroborating signals

1. **Skeleton/arity match.** Both: call one function returning a score-type enum
   → switch over it → fail on default. Wii callee = `ActiveScoreType__12MusicLibraryCFv`
   (CW map line 14601, the function immediately after DifficultySortPart in
   `MusicLibrary.o`). Xenon callee = `Function_8252B900`.
2. **String domain agrees.** Xenon references `'guitar' 'vocals' 'real_guitar'
   'real_bass' 'real_keys'` — precisely the score-type Symbol names the Wii body
   loads (band/guitar/bass/drum/vocals/keys/real_guitar/real_bass/real_keys).
3. **Same Symbol ctor callee (cross-mangling).** Xenon builds the per-type
   `Symbol` statics via `__0Symbol__QAA_PBD_Z` (MSVC `Symbol::Symbol(const char*)`);
   Wii uses `__ct__6SymbolFPCc` (MWCC `Symbol::Symbol(const char*)`). Same ctor.
4. **Default semantics agree.** Both default branches construct a Symbol from a
   fail/null string (Wii: `Fail__5DebugFPCc` + Symbol(gNullStr); Xenon: Symbol ctor
   on `PTR_DAT_82c411b0`), consistent with the Wii assert literal
   `"Bad ScoreType in MusicLibrary::DifficultySortPart!"`.
5. **Size ratio in band.** Wii 236 B vs Xenon 584 B = 2.47x. Slightly above the
   1.0–2.5x guideline, fully explained by the Xenon side inlining the lazy
   per-Symbol init (the 9 `if((flags&bit)==0){ flags|=bit; Symbol::Symbol(...) }`
   guards) that the Wii build factors into static-init / `ActiveScoreType`.

## The one blemish (does not change the verdict)

The evidence pack resolves the Xenon callee `0x8252b900` (via `matches.json`) to
Wii `Save__...OpenWaitingGateMsg`, which disagrees with the Wii callee
`ActiveScoreType` (separately matched to Xenon `0x8233afb0`). This is an
**unverified secondary match** — exactly the "Function_<addr> placeholder / wrong
resolution" artifact called out in substrate caveat #4. It is one weak, unvetted
edge against the overwhelming primary evidence (identical 11-case partition +
identical string domain + same Symbol ctor + matching fail semantics). It lowers
nothing below HIGH.

## For the next agent

- Pair 27 is a clean SwitchSigHasher **true positive**. The switch-partition
  fingerprint method is doing real work here; trust SwitchSig on band3 enum
  dispatchers like this.
- Note the callee-resolution caveat: `matches.json` resolved `0x8252b900` to the
  wrong Wii symbol. If you build a "callee-agreement" feature, treat single
  unvetted callee edges as low-weight; the switch/string evidence dominated.
- Companion pairs in the same family worth a glance: pair-29
  `ActiveScoreType__12MusicLibraryCFv` (the enum producer, also SwitchSig) and
  pair-28 `TrackTypeToScoreType` — same `MusicLibrary` score-type machinery.
