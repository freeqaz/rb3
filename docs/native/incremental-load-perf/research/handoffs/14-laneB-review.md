# Lane B adversarial review — chunked mogg-yielding sharpen fetch

**Reviewed:** engine `3e02cea` (../milo-native-engine), rb3 `9df9fd9b`, against
research/14 §Lane B + handoff 14-laneB-throttle.md. Reviewer: Fable, 2026-07-02.
**Verdict: FAIL — 1 blocking, 7 advisory.** Nothing was fixed here (review-only).

## BLOCKING

### B1. Range-ignoring server ⇒ unbounded append loop ⇒ wasm OOM crash mid-gameplay

`native/src/rb3_texsharpen_native.cpp` `PumpSidecarFetchWeb()`, landed-chunk
branch (~:296-315). The success path never checks `taken > gDrv.chunkReqLen`.
A 206 can never exceed the requested length — but a server that ignores Range
(RFC 9110 §14.2 explicitly allows this; `python -m http.server` does it; some
proxies/CDN edges strip Range) replies **200 with the whole 5.4 MB body**:

- chunk 0: whole file appended (benign, even self-terminating if it EOF'd here — it doesn't);
- `taken (5.4MB) < chunkReqLen (256KB)` is FALSE and `sidecarSize == -1`
  ⇒ **no terminator fires**; `chunkOffset` = 5.4 MB;
- every subsequent "chunk" (offset past EOF — the stripping server 200s anyway)
  appends **another whole 5.4 MB at the wrong offset**, forever: `assembly`
  grows ~5.4 MB every other frame (~160 MB/s at 60 fps) until the wasm heap
  exhausts (`resize` → bad_alloc → abort), while each fetch also saturates the
  wire against the mogg — the exact interference the lane exists to prevent.

The in-code comment and commit message claim the "Range-stripping proxy" case
is covered by the chunk-0-fails-×5 → legacy fallback. **It is not: a stripping
proxy SUCCEEDS (200), so the error arm never fires** — the named fallback is
unreachable for the very case it names. The one shipped topology (server.py)
honors Range (verified `_serve_range`, server.py:1300), so this does not fire
on localhost — but the lane's stated purpose is hardening for other links.

Cheap fix (for the fix pass, not applied here): in the landed branch, if
`taken > gDrv.chunkReqLen` ⇒ server ignored Range ⇒ either (offset==0) accept
the whole body and `FinalizeSidecarAssembly()`, or discard `assembly` and set
`chunkFallback = true`. Belt-and-braces: cap `assembly` at the same 64 MB
ceiling `ReadWholeFile` already uses.

## ADVISORY

1. **Truncated sidecar persists resident in MEMFS for the whole session.**
   (a) A partial `fwrite` (MEMFS OOM) leaves a truncated-but-resident file and
   `WriteAssembledSidecarToMemfs` does not `remove()` it — residency then
   short-circuits the pump, so the claimed "MEMFS write failure → legacy single
   fetch" fallback is **dead code whenever fopen succeeded** (only the
   fopen-fails case actually falls back). (b) The error-after-progress EOF arm
   deliberately finalizes a possibly-truncated assembly to MEMFS. In both
   cases the parser rejects it (matched 0, clean no-op) **but the file stays
   resident**, so every later song on that venue reads it, fails parse, and
   sharpening is silently disabled for the rest of the browser session — a
   permanence the handoff's "clean cosmetic no-op" doesn't state. Suggest
   `remove()` on write failure and after a parse-reject.
2. **ParseSidecar nameLen bounds check is u32-wrap-prone** (engine
   RB3TexSharpen.cpp:165: `if (o + nameLen > n) return false;`). A corrupt
   nameLen near 2^32 wraps the sum, passes, then `e.name.assign(d+o, d+o+nameLen)`
   is a multi-GB wild read → crash. Unreachable from pure prefix truncation
   (record headers are complete or rejected first), so the shipping short-read/
   416 terminator can't produce it — but the B1 garbage-assembly path can, and
   it weakens the "SHRP parser is the integrity gate" claim. The topmipLen
   check 6 lines below already casts to `(uint64_t)` — mirror that. Pre-existing
   line; new exposure.
3. **RejectsGarbageSidecar does NOT cover the shipping failure product.** It
   tests bad-magic / null / short-header only. The actual artifact of the
   short-read/416/error-after-progress design — a valid-header **prefix-truncated**
   blob (cut mid-entry-header / mid-name / mid-topmip) — is untested. By
   inspection ParseSidecar handles prefix truncation correctly (o+kRec check;
   64-bit topmip check), but the safety claim rides on untested code. Suggest a
   truncation-sweep test (valid 2-entry blob, truncate at N offsets, assert 0).
4. **Retry-cap accounting nits (engine RB3SharpenStep):** the give-up path adds
   `fullBytes` to `bytesUpgraded` though no GPU upload happened (status/debug
   overcount; defensible since the CPU bitmap IS full-res). The cap is
   per-entry with head-of-line blocking: a permanently not-ready GPU with 15
   matches takes 15×120 ≈ 1800 frames (~30 s), not the comment's "~2 s" —
   bounded, cosmetic. Verified NO `matchedIdx[-1]` OOB on the cursor rewind
   (the post-increment precedes the decrement, so the rewind lands on the same
   index ≥ 0) and `RB3SharpenComplete` is cursor-based, so capped/OOM entries
   count as consumed — no COMPLETE wedge. Both new gtests pass locally (5/5
   TexSharpenManagerTest).
5. **Hung-fetch wedge (pre-existing class):** Range fetches set no emscripten
   timeout; a never-completing chunk keeps `chunkReqId != 0` forever → the pump
   (and sharpening) wedges for the session, and after Reset the abandoned entry
   is reclaimed only if its callback eventually fires. Same exposure as the
   mogg slots — noted, not Lane-B-introduced.
6. **Starvation both ways — verified as designed and stated.** Sharpen can be
   starved indefinitely by continuous mogg streaming (strict yield); the
   handoff states this explicitly (acceptable-by-design). Self-deadlock is
   impossible: the count is only read when `chunkReqId == 0`, own done entries
   are excluded (`!done`), Reset's Drop abandons (`!abandoned`), and RangeTake/
   Drop always detach before the next yield check. Mogg-side never checks the
   count, so worst-case interference really is one chunk. Note chunk-0 retries
   also yield, so a no-sidecar venue under heavy streaming just takes longer to
   reach its clean no-op (fine).
7. **Flag nit:** a non-numeric `RB3_SHARPEN_CHUNK_KB` atoi's to 0 and silently
   selects the LEGACY path instead of the chunked default. Defaults otherwise
   sane (256 KB, floor 16, cap 8192, negative → legacy); CHUNK_KB=0 legacy arm
   verified working and actually improved (EnsureStatus==2 dead-fetch no-op
   fixes the old poll-forever on venues without sidecars).

## Verified clean (the hunted classes that did NOT reproduce)

- **Mid-chunk song end / RB3GameWarmReset:** `RB3TexSharpenReset` (called from
  `RB3GameWarmReset` ← GamePanel::Unload:270) RangeDrops the in-flight chunk via
  the abandon/detach pattern (WebAssets.cpp:829 — detached from the registry,
  callback self-reclaims, excluded from InFlightCount), the assembly vector is
  freed by the struct reset, and **nothing is written to MEMFS** (Finalize only
  runs from the pump, which is dead after reset). No truncated sidecar, no
  registry leak, no UAF. Step also never runs during teardown (the
  `songMs>0 && !isGameOver` gate at Game.cpp:1793 is false by then), so the
  session's raw `RndTex*` are never dereferenced against a torn-down venue.
- **Partial-file MEMFS visibility:** during assembly nothing touches MEMFS; the
  finalize write is synchronous fopen/fwrite/fclose within one frame on the
  single wasm thread — no torn-residency window on the normal path (the failed-
  write residue is advisory #1).
- **Short-read-as-EOF soundness:** emscripten_fetch errors when the body is
  shorter than Content-Length, so an onsuccess short read genuinely means the
  server clamped at EOF; server.py `_serve_range` clamps (206/short) and 416s
  past-EOF exactly as the handoff measured.
- Engine additions are `__EMSCRIPTEN__`-guarded (WebAssets) / native-only TU
  (RB3TexSharpen); no matched Wii TU touched; no pin bump (correctly left to
  the integrator).
