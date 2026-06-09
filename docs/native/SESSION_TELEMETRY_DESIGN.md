# Session Telemetry & Replay — Design

**Status:** **v1 CONTRACT LOCKED** (2026-06-09). §3–§9 are the multi-agent deep-dive (design provenance); the 6 reconciliations + OQ register are resolved into the authoritative **[Locked v1 contract](#locked-v1-contract-authoritative)** below — that section wins wherever §3–§9 conflict. **Ready for M0.** Capture scope: local now, remote-ready.
**Author:** skeleton by Claude; §3–§9 by a multi-agent code-grounded design deep-dive, 2026-06-09
**Audience:** RB3 native+web port maintainers.

> **Read order:** the **[Reconciliation](#-reconciliation-required-before-m0-read-first)**
> section and the **[Open Questions register](#open-questions-register-post-deep-dive)**
> are the actionable parts — every claim in §3–§9 is grounded in real code (file:line),
> but the cross-section integration calls (wire-format identity, `sid` ownership, the
> both-builds frame tap) are what actually gate M0. Resolve the 🔴 items first.

---

## 1. Goal

Whenever a human plays the RB3 port — **web or native** — capture a **session
trace** rich enough to:

1. **Measure performance** — frame rate, frame-time tail (p95/p99), load-stutter
   attribution, boot timeline, audio underruns.
2. **See what the player did** — every key/button press + analog (whammy/tilt),
   the nav path through screens, song/difficulty chosen.
3. **Replay the session** to reproduce bugs — re-drive the same inputs against
   the same build + assets and observe the same failure.

One shared implementation, two transports (native = file/HTTP, web = upload to
the dev server). Telemetry is **always-on but toggleable**, with a bounded
in-memory footprint so it costs ~nothing during normal play.

### Locked decisions (from design Q&A, 2026-06-09)

| Decision | Choice |
|---|---|
| Replay fidelity | **Tier 1 first** (diagnostic trace + scenario re-drive), **Tier 2 after** (deterministic frame-perfect), **then testing** to measure how deterministic we actually are. |
| Capture default | **Always-on, toggleable** (env var / URL param to disable). |
| Trace storage | server.py ingests uploads and writes to **SQLite** (not flat JSONL files). |
| Capture scope | **Local dev now, remote-ready** — loopback bind by default; one opt-in flag (+ shared-secret) flips to a network bind for remote playtesters. Schema/egress unchanged when remote is enabled. |
| Post-M0 priority | **Perf reporting before replay** — after M0: report tooling (M3), *then* bug-repro replay (M1). |
| Report delivery | **CLI/markdown first** (`trace-report.py`); browser session-dashboard is a later add. |
| Doc workflow | Skeleton → multi-agent deep dive (§3–§9) → synthesis reconciliation → **Locked v1 contract** (authoritative). |

---

## Overview — how the pieces fit

The seven deep-dives describe one coherent pipeline: a shared C++ recorder (D1) emits a versioned NDJSON event stream (D2), which the web build batches over an EM_JS bridge to a new `POST /api/telemetry/<sid>` (D5) that a single-writer thread ingests into SQLite (D3); two reader tools — a report generator and a Playwright replay harness (D7) — plus an in-engine replay driver (D6) consume sessions back via `GET /api/telemetry/<sid>`. The architecture is sound and each section is individually well-grounded in real file:line seams. The friction is entirely at the **contracts between sections**: the wire schema (D2) and the in-memory POD (D1) disagree on input axes; the SQLite idempotency model (D3) depends on a per-line `client_seq` that no emitter section actually adds; the nav source-of-truth was upgraded by D4 (UIScreenChangeMsg, +wentBack) but D1/D2 still describe the old PublishCurrentScreen tap and omit `wentBack`; and the denormalized `frames` hot table silently drops the `ld`/`st` columns the report's marquee asset-correlation metric needs. None of these are architectural — they are field-list and ownership reconciliations — but several (client_seq, sid ownership, the App.cpp frame-tap relocation) are hard prerequisites for M0 to produce a single valid native trace.

## ⚠️ Reconciliation required before M0 (read first)

The 7 deep dives below are each internally grounded in real code, but the synthesis pass found **6 contradictions between sections** and several **unowned gaps**. None are architectural — the pipeline (recorder → NDJSON → EM_JS/beacon → POST → SQLite → report/replay) holds end-to-end — but three block M0 from emitting a single valid trace. Resolve these (they map to the OQ register at the bottom) before writing code.

### Synthesis verdict

> COHERENT AND BUILDABLE END-TO-END, with reconciliation required before coding. The pipeline (recorder → NDJSON → EM_JS/beacon → POST → SQLite → report/replay) is architecturally sound and every seam is grounded in verified file:line; no section's approach is fundamentally incompatible with another's. But there are 6 concrete cross-section contradictions and several unowned gaps that must be closed first — three of them block M0 from emitting even one valid trace: (1) NO emitter section adds the per-line `client_seq` that D3's PK/idempotency requires; (2) the `in` event axis set is 2 fields in D1's POD vs 9 in D2's wire schema; (3) `sid` has no defined source on native. 
> 
> M0 SHOULD START WITH (in order): (a) Decide sid ownership — make C++ the sid owner on both platforms (web pre-js reads it back via EM_ASM for the route) so the recorder, hdr line, and POST route key agree [OQ3]. (b) Add `client_seq` (and a settled minimal `ax` axis set) to D1's POD + D2's §4.2 envelope so the wire format is final before any ingest exists [OQ1, OQ2]. (c) Build the shared recorder TU rb3_session_trace.{h,cpp} subsuming rb3_frame_trace.cpp, with the toggle gate, ring, and string interner. (d) Relocate the frame tap into App::RunOneFrame (gate the Timer.Split behind gRB3TraceActive) so frame capture covers BOTH builds, and assign the band3 App.cpp edit owner [OQ4]. (e) Land the UIScreenChangeMsg nav sink (D4's model, with `wentBack` added to the POD/schema) rather than D1's stale PublishCurrentScreen detector [OQ6]. 
> 
> This yields M0's acceptance — a real native .jsonl with hdr+fr+nav(+boot if native BootMark is in scope) and RB3_FRAME_TRACE back-compat — while front-loading exactly the schema fields (client_seq, sid, axis set, wentBack, ld/st-in-frames) whose omission would otherwise force a breaking format change once D3 ingest and D7 reporting are built. Defer Tier-2 determinism (D6 §Tier2) and `au` underrun capture to their respective milestones; both are correctly gated and neither blocks M0–M3.

### Cross-section contradictions

1. INPUT AXIS SCHEMA MISMATCH (D1 ↔ D2). D1's in-memory POD `in` struct (rb3_session_trace.h event model) carries only `int16_t whammy, tilt` — two axes. D2's wire schema §4.4 defines `ax{lx,ly,rx,ry,lt,rt,sx,sy,sz}` — NINE axes (sticks ×4, triggers ×2, sensors ×3). The D1 fixed-size POD physically cannot hold what D2 serializes, and D6's replay writes back only `d->mSticks[0][1]`/`[1][0]` (LY/RX), ignoring lt/rt/sx/sy/sz. Either D1's POD must widen to the full axis set (breaking the ~48B/~32B union budget) or D2 must narrow `ax` to the 2 axes actually captured. D6 replay must also be reconciled to whatever axis set survives.

2. NAV SOURCE OF TRUTH CONTRADICTION (D1/D2 ↔ D4). D1 §3 and D2 §4.3 specify the nav tap at `PublishCurrentScreen()` (main_web.cpp:282) with recorder-side change detection (`static const char* sLastScr`). D4 §6.1 explicitly REFUTES this ("It does not detect the change… no `from`, no `wentBack`, can miss A→B→A in an 8-frame window") and mandates a `UIScreenChangeMsg` sink on TheUI (UI.cpp:682) instead. D4 is the lifecycle owner and is correct, but D1's API (`RB3RecordNav(from,to,focus)`) and integration sketch were never updated to the sink model — and D4's `nav{from,to,wentBack,focus}` adds a `wentBack` field that D1's `nav` POD (`fromId,toId,focusId` — no wentBack) and D2's schema (`from,to,focus,ov` — no wentBack) both omit.

3. EVENTS PK COLUMN NAME DRIFT (D3 internal + D7). D3 §5.2's canonical schema names the events PK column `client_seq` (events(sid, client_seq, …)), but D7 §9.1's query table describes the schema as `events(sid,seq,t_ms,frame,song_ms,kind,payload)` — using the OLD `seq` name from the doc's pre-D3 stub (§5 line 183). D7's SQL examples don't select that column by name so they'd run, but the documented contract is inconsistent: the column is `client_seq`, not `seq`.

4. client_seq NOT IN ANY EMITTER SCHEMA (D3 ↔ D1/D2/D5). D3's entire idempotency + ordering model is keyed on a monotonic per-session `client_seq` stamped on EVERY JSONL line. Neither D1's event POD, D1's public Record* API, D2's §4.2 common envelope (`t`,`f`,`sm`,`k` only), nor D5's egress format add a `client_seq`/`seq` field. D3 itself flags this as a cross-section dependency, but as written the recorder emits no client_seq, so the SQLite PK (sid,client_seq) would collide/NULL on ingest. This is a hard build-blocking contract gap, not just a doc nit.

5. DENORM `frames` TABLE DROPS `ld`/`st` (D2 ↔ D3 ↔ D7). D2's `fr` event carries `ld` (loader adds) and `st` (stream opens) — the spike↔asset correlation `frame_profiler.py` relies on. D3's denormalized `frames` table (§5.2) has columns dt,lp,lpu,scr,pend but NO ld/st. D7's long-frame-attribution section selects `frame,dt,lp,lpu,scr,pend FROM frames` and never gets ld/st, yet D2 §4.6 rule 2 ("always emit when ld>0||st>0") and frame_profiler's spike↔asset clustering need them. They survive only in `events.payload`, so the denorm hot-path query is lossy for exactly the asset-correlation metric the report advertises.

6. sid OWNERSHIP UNRESOLVED ACROSS PLATFORMS (D5 ↔ D1 ↔ D3). D5 §7.2 generates `sid` in JS (crypto.randomUUID, sliced to 16 chars) and has C++ read it back via EM_ASM for the hdr line. D1's API (`RB3TraceInit`) and hdr emission assume the recorder owns the header but never says where sid comes from on NATIVE (no JS). D3 keys every table on `sid` as the route + PK. D5's own open question flags this. As written, native has no defined sid source, so native→file traces and native `repro` (D7) have no session identity.


### Unowned gaps

- FRAME-TAP RELOCATION OWNERSHIP. Three sections (D1 new-Q, D2 new-Q, the doc §2/§3) independently observe that the existing frame tap at App.cpp:809 lives in the desktop `RunWithoutDebugging` loop and is BYPASSED by the web path (main_web.cpp:653 calls `sApp->RunOneFrame` directly). All three agree the tap must move into `App::RunOneFrame`, but no section OWNS the band3 App.cpp edit or resolves the cost question (always-on `Timer.Split()` on the native non-instrumented path vs gating behind gRB3TraceActive). M0 cannot ship 'frame capture on both builds' until this is assigned and the timing-cost decision made.

- NATIVE BOOT + NATIVE NAV CAPTURE. BootMark and PublishCurrentScreen live only in main_web.cpp and are not called from the native loop. D1 explicitly defers ('leave boot web-only for M0'), D4 taps BootMark (web-only) and a TheUI nav sink (shared, but needs the native loop to actually Poll UI). No section commits native boot/nav for M0, yet M0's acceptance in the doc (§10) is 'frame/boot/nav capture' from a NATIVE headless run. The native boot/nav path is unowned.

- `au` (AUDIO UNDERRUN) RECEIVER SEAM. D2/D5/D4 all note `au` is web-only and the worklet's `.port.onmessage` lives in engine-generated rb3-web.js glue (not pre-js, not native/src), so there is NO concrete seam wiring underrun postMessage into window.__rb3Trace. D5's diagram routes it but its own open question admits the receiver is engine-glue-owned. No section owns adding that hook (it needs a milo-native-engine edit or a window global the recorder polls). v1 `au` is effectively unimplemented.

- SONG `end` SCORE/PCT ACCESSOR. D4 confirms `Game::OnMsg(GameEndedMsg)` has `mResult` + end-ms but NOT a numeric score/pct local, and defers the exact score accessor to D2. D2's schema lists `song.end` fields `score,pct` but does NOT pin where they're read from (TheGamePanel export args? a Performer/stats accessor? post-hoc /api/dta/eval?). The `song.end` payload fields are specified but unsourced — no one owns the accessor.

- `log` EVENT RATE-LIMIT / FILTER POLICY. D4 taps Debug::Print (catches 100% of MILO_LOG/WARN/ASSERT) and flags that GAME_DBG/per-frame diagnostic spam also funnels through it; it defers the severity-allowlist-vs-rate-limit decision to D2. D2's schema defines the `log` event shape but its decimation section (§4.6) only covers `fr` decimation — it never specifies a `log` filter/rate-limit. Always-on capture could flood the ring with dev-only logs; the policy is unowned.

- JoypadButton BIT→NAME DECODE TABLE. D7's input/APM per-button histogram needs a canonical bit→button-name map (and which bits are analog vs digital). D7 flags it should be generated from the C++ enum (os/Joypad.cpp / rb3_joypad_native.cpp) rather than hardcoded, and assigns generation to 'D2 (schema)'. D2 never specifies this table or its generation. The report's input histogram has no defined symbol source.

- tilt/sensors CAPTURE IS DEAD IN v1. D2 §4.4 reserves sx/sy/sz but states the native port never synthesizes tilt (mSensors stay 0, no keybind). D6 replay reconstruction and D1's POD likewise don't carry sensors. So the schema reserves tilt but nothing produces it — acceptable for v1 but no section owns the 'tilt is a no-op until a Wii pad / keybind exists' caveat being surfaced to the report tool (D7 would render constant-zero sx/sy/sz if ever present).

## Locked v1 contract (authoritative)

This section freezes the v1 contract by resolving the 6 reconciliations + the OQ
register, folding in the maintainer's direction calls. **Where §3–§9 disagree
with this section, this section wins**; the deep-dive sections remain as design
provenance and detail. Each resolution cites the OQ / contradiction it closes.

### Identity & ordering
- **`sid` is owned by C++ on both platforms** (closes contradiction 6 / OQ3).
  `RB3TraceInit` mints it (monotonic clock + pid/counter; same code path on web).
  The web pre-js reads it back via `EM_ASM` into `window.__rb3Sid` so the
  `POST /api/telemetry/<sid>` route, the `hdr` line, and the SQLite PK all agree.
  Native file traces + `repro` therefore have a real session identity.
- **`client_seq`** — a monotonic per-session `uint64`, stamped by the recorder on
  **every** emitted line (incl. `hdr`) — is added to the common envelope (closes
  contradiction 4 / OQ1). It survives chunk boundaries + `sendBeacon` tail
  duplication; ingest upserts on `(sid, client_seq)`. It is *the* idempotency +
  ordering key (`t`/`f` are not unique enough). D1 POD, D1 `Record*`, D2 §4.2
  envelope, and D5 egress all carry it.

### Input axes (closes contradiction 1 / OQ2)
- v1 captures only the axes the port actually produces: **whammy** (live at the
  `JoypadPoll` tap via `mSticks` LY/RX) + a reserved **tilt** slot (always 0
  until a Wii pad / keybind exists). The C++ POD carries a fixed `int16 axes[2]`
  (whammy, tilt), values ×1000 — this is the canonical size, **not** D2's 9-axis
  set.
- The wire `ax{}` is **sparse**: only non-zero axes serialize, under keys `wh`
  (whammy) / `ti` (tilt). The `lx/ly/rx/ry/lt/rt/sx/sy/sz` namespace stays
  *reserved* (documented) but unproduced in v1. D6 replay writes back only the
  captured axes; the report renders only populated axes (no constant-zero cols).

### Nav (closes contradiction 2 / OQ6)
- Nav is captured by a real **`UIScreenChangeMsg` sink on `TheUI`** (D4's model),
  not the passive `PublishCurrentScreen` detector — exact `from`/`to`/`wentBack`,
  no missed A→B→A. **`wentBack` is added** to the nav POD + the `nav` wire schema.
  The recorder registers the sink at init, before the first `GotoScreen`.

### Frame metrics (closes contradiction 5 / OQ4, OQ10, OQ11)
- The **frame tap moves into `App::RunOneFrame`** so one tap covers native *and*
  web (web bypasses the old `RunWithoutDebugging` site). The `frameTimer.Split()`
  read is gated behind `gRB3TraceActive` → zero-cost when off. Owner: the M0
  implementer (small `src/band3/App.cpp` edit).
- The denormalized `frames` hot table gains **`ld`, `st`** (loader-adds /
  stream-opens) + `t_ms`, `song_ms`, so the long-frame ↔ asset-spike query runs
  off the hot table. The events PK column is **`client_seq`** everywhere (fixes
  D7's `seq` drift / OQ11).
- **Native boot + native nav are in M0 scope** (closes OQ5): hoist a shared
  `RB3TraceCheckNav()` + a native `BootMark` equivalent out of `main_web.cpp`, so
  a native headless run emits `hdr+fr+nav+boot` (the M0 acceptance bar).

### Capture scope — local now, remote-ready (closes OQ13; OQ12)
- Default bind stays **loopback** (`127.0.0.1`); the telemetry POST route also
  enforces a localhost client check. One opt-in flag (`--telemetry-bind <addr>` /
  env) flips to a network bind for remote playtesters, at which point a
  **shared-secret header** is required. The `hdr` already carries enough to
  attribute a remote session (build sha, asset version, UA). **No field changes
  are needed to enable remote** — it is config, not a rewrite.
- DB durability: **`synchronous=NORMAL` + WAL** (OQ12) — correct for a
  dev/playtest tool; a host crash loses at most the last unflushed tail.

### `log` flood control (closes OQ8)
- The `Debug::Print` tap applies a **severity allowlist (WARN/ASSERT/ERROR by
  default) + per-message-key rate-limit** before recording, so always-on capture
  can't be flooded by `GAME_DBG`/per-frame spam. Raisable to full verbosity for a
  focused repro.

### Reporting (closes OQ18)
- **CLI/markdown first** (`trace-report.py`, extends `frame_profiler.py`),
  pull-based. A browser session-dashboard (served by `server.py`) is a **later**
  add once data is flowing — not in the first tooling pass.

### Deferred to their milestone (not v1-blocking)
- **`au` audio-underrun capture** (OQ9) — web-only; needs a `milo-native-engine`
  glue hook on the worklet `onmessage`. Lands with **M2**.
- **`song.end` score/pct accessor** (OQ7) — pin the exact `Game`/`Performer`
  accessor when song-lifecycle capture lands (**M1/M3**).
- **Tier-2 gameplay determinism** (OQ14, OQ15, OQ19) — the `Game::Poll`
  replay-clock seam + divergence tolerance + milestone anchoring are **M4**, owned
  by the native-port maintainer (band3/game + audio).
- **`RB3CurrentFrame()` for the replay branch** (OQ16) + **JoypadButton bit→name
  table** (OQ17) — implementer details for **M1/M3**.

> **Sequencing (maintainer priority):** M0 (trace core, native traces) →
> **M3 report tooling** (runs on native traces immediately) → **M2 web egress**
> (real human web sessions into SQLite) → **M1 Tier-1 replay** → **M4 Tier-2**.
> Perf reporting is prioritized ahead of replay per the direction call; M2 slots
> in because real "human playing" perf data needs the web egress path.

---

## 2. Why this is mostly plumbing, not new engine work

The seams already exist. The exploration found:

| Capability we need | What already exists | Where (file:line) |
|---|---|---|
| Per-frame metrics + JSONL sink | `RB3FrameTraceRecord(frame, dtMs, loadPollMs, loadPollUntilMs, screen, pendingLoaders)`, env-gated by `RB3_FRAME_TRACE=<path>`, zero-cost when off | `native/src/rb3_frame_trace.cpp:56` |
| Event counters at engine chokepoints | `gFrameTraceActive` / `gFrameTraceLoaderAdds` / `gFrameTraceStreamOpens` incremented in `LoadMgr::AddLoader` + `Synth::NewStream` behind one bool | `native/src/rb3_frame_trace.cpp:44`; defined in `src/system/utl/Loader.cpp` |
| Natural per-frame hook (timing already measured) | `frameTimer` + `Timer::CyclesToMs`; `RB3FrameTraceRecord` already called here | `src/App.cpp:778-815` (loop), `App::RunOneFrame` `src/App.cpp:504` |
| Single input chokepoint | `JoypadPoll() → SendButtonMessages(0, btns)` — diffs to pressed/released, broadcasts to menus + gameplay | `os/Joypad.cpp:321`, `native/src/rb3_joypad_native.cpp:355` |
| Input as a 32-bit `JoypadButton` bitmask; web mirror | `window._rb3Keys` bitmask kept by keydown/keyup listeners; read by `RB3GameInputPoll` | `native/src/rb3_game_input.cpp` (`InitWebInput`, `RB3GameInputPoll`) |
| Boot timeline | `BootMark(phase)` → `performance.mark` + `window.rb3BootPhaseLog=[[phase,ms]]`, 6 marks | `native/src/main_web.cpp:430` (marks at fetch_start/done, engine_init_done, gpu_ready, appctor_start/done) |
| Live state published per frame | `window.rb3CurrentScreen / rb3FocusButton / rb3FrameCount / rb3SongCount / rb3OvershellView/Track/Diff / rb3HighlightedSong` | `native/src/main_web.cpp:235` (`PublishCurrentScreen` etc.) |
| Audio health | worklet underrun stats (`underrunEvents`, `underrunFrames`) via `postMessage` | `native/web/build/audio-worklet.js:84` |
| Existing frame-gated input injection (replay precursor) | `RB3_GAME_INPUT="@30:start,@90:confirm"`, `RB3_JOYPAD_SEQ="f:bit,..."` | `native/src/rb3_game_input.cpp:213`, `native/src/rb3_joypad_native.cpp:394` |
| Headless drive + main-thread command queue | `/api/health|screenshot|dta/eval|input`; queue drained on main thread post-frame | `native/src/rb3_http_server.cpp`, `native/src/rb3_http_handlers.cpp` |
| Clocks | `OSGetTick()` (monotonic), `OSGetTime()` (realtime), song time via `MasterAudio::GetTime()` | `native/src/rvl_shims.cpp:126-140`, `rb3_http_handlers.cpp:423` |

**The actual gaps:**
1. A unified **session recorder** that folds input + frame metrics + lifecycle
   + boot into one time-ordered, versioned stream (today `rb3_frame_trace.cpp`
   only does frame metrics; input/nav/boot are scattered across globals).
2. A **web egress path** — there is *no* POST/upload endpoint on either
   `server.py` (`native/web/server.py:345`, GET-only dispatch) or the native
   HTTP server. Web traces currently can't leave the browser.
3. A **replay driver** that injects recorded input by the *engine clock / frame
   index* rather than wall time, and (Tier 2) overrides the frame dt.
4. **SQLite ingest + schema** on the dev server.

---

## 3. Architecture — Trace core C++ API & integration seams (D1)

The recorder is **one sibling TU, `native/src/rb3_session_trace.{h,cpp}`**, compiled into both builds under `HX_NATIVE`. It **subsumes** `rb3_frame_trace.cpp` (see Q1) rather than extending it in place.

### Event model (in-memory; D2 owns the wire schema)
A single fixed-size POD so the ring is a flat array (no per-event alloc, trivially `memcpy`-serializable):
```c++
enum RB3TraceKind : uint8_t { TK_HDR, TK_BOOT, TK_FRAME, TK_INPUT, TK_NAV, TK_SONG, TK_AU, TK_LOG, TK_MARK };
struct RB3TraceEvent {            // ~48 bytes
  double   t;                     // monotonic ms (OSGetTick→ms; rvl_shims.cpp:135)
  int32_t  f;                     // frame index (see gRB3TraceFrame below)
  float    sm;                    // song ms, -1 if not in a song
  uint8_t  kind; uint8_t pad;     // pad = joypad index (input) / phase id (boot)
  union {                         // kind-specific, ≤32B; strings (scr/nav/log) are interned ids
    struct { float dt,lp,lpu; uint16_t pend; uint16_t scrId; uint16_t ld,st; } fr;
    struct { uint32_t bits,dn,up; int16_t whammy,tilt; } in;   // axes ×1000, D2 finalizes
    struct { uint16_t fromId,toId,focusId; } nav;
    struct { uint8_t phaseId; } boot;
    struct { uint16_t aId; uint32_t u32; float val; } gen;     // song/au/log/mark
  };
};
```
Variable-length strings (screen/nav/focus/log) go through a **string interner** (append-only `std::vector<std::string>` + hash map → uint16 id) so events stay fixed-size; the interner table is emitted in the header line on flush.

### Toggle gate — single predicted branch when OFF
Mirror the existing pattern (`gFrameTraceActive`, Loader.cpp:42; per-callsite `static int sFrameTrace` cache, App.cpp:770-772). One exported global, set once at init:
```c++
extern bool gRB3TraceActive;     // defined in rb3_session_trace.cpp, false by default
extern int  gRB3TraceFrame;      // current frame index, written by the frame tap
inline void RB3RecordInput(...)  { if (!gRB3TraceActive) return; RB3RecordInputImpl(...); }
```
Every public `Record*` is a thin inline whose first statement is `if (!gRB3TraceActive) return;` — same not-taken branch the engine chokepoints already pay (Loader.cpp:144). The engine-side counters (`gFrameTraceLoaderAdds/StreamOpens`) stay **as-is in Loader.cpp** and are aliased into the new `fr` event, so no engine TU changes.

`gRB3TraceActive` init (lazy, first frame): native reads `getenv("RB3_SESSION_TRACE")` (path ⇒ file sink) **and** keeps `RB3_FRAME_TRACE` as a back-compat alias (Q1). Web reads a JS global published by the pre-js shim (`window.__rb3TraceOn`, default true, cleared by `?notrace=1`).

### Public API (`rb3_session_trace.h`)
```c++
void RB3TraceInit();                                  // resolve toggle + open sink, set gRB3TraceActive
void RB3TraceSetFrame(int frame);                     // frame tap calls first thing each frame
void RB3RecordFrame(float dt,float lp,float lpu,const char* scr,int pend);
void RB3RecordInput(int pad,uint32_t bits,uint32_t dn,uint32_t up,float whammy,float tilt);
void RB3RecordNav(const char* from,const char* to,const char* focus);
void RB3RecordBootMark(const char* phase);
void RB3RecordEvent(RB3TraceKind k,const char* sym,uint32_t u32,float val);  // song/au/log/mark
void RB3TraceFlush();                                 // native: append ring→file; web: hand ring→EM_JS (D5)
```

### Exact call sites (verified file:line) + 1-line integration

| Tap | Site | Sketch |
|---|---|---|
| **Frame** | `App::RunOneFrame` — at entry, **not** the `RunWithoutDebugging` loop. The web boot calls `sApp->RunOneFrame(sFrameCount)` directly (`main_web.cpp:653`), bypassing the loop's existing `RB3FrameTraceRecord` at `src/App.cpp:809`. Moving the frame tap into `RunOneFrame` is the **only way one tap covers both builds.** | Top of `RunOneFrame(int frame)` (after `src/App.cpp:504`): `RB3TraceSetFrame(frame);` then at end of frame `RB3RecordFrame(ms,gLoadPollMsThisFrame,gLoadPollUntilMsThisFrame,scrName,(int)TheLoadMgr.mLoading.size());`. The loop at `App.cpp:808` already computes `ms`/`scrName` under `wallTime`; have it set those into two file-scope globals (`gRB3FrameMs`,`gRB3FrameScr`) that `RunOneFrame` reads, OR move the `frameTimer` into `RunOneFrame` so web also gets dt (web currently has none). **Recommend the latter** — frame timing then works on web for free. |
| **Input** | `JoypadPoll()` final broadcast at `native/src/rb3_joypad_native.cpp:537` (`SendButtonMessages(0, btns)`). `JoypadPoll` has **no `frame` param** (it runs under `SystemPoll(false)`→`System.cpp:673`, before `RB3GameInputPoll`), so it reads `gRB3TraceFrame`. | Immediately before line 537: `RB3RecordInput(0, btns, btns & ~gPrevBtns, ~btns & gPrevBtns, d->mSticks[0][1], d->mSticks[1][0]); gPrevBtns = btns;` — edge-only (impl drops the call if `bits==gPrevBtns`). Axes `mSticks` (Joypad.h:218) are live here; whammy = LY/RX (set at lines 530-531). The `pad:`/`SEQ` harness early-returns above won't tap — acceptable (those are test injectors). |
| **Boot** | `BootMark(phase)` at `native/src/main_web.cpp:477`. | Add as last line of `BootMark`: `RB3RecordBootMark(phase);`. Native has no BootMark today; add equivalent calls or leave boot web-only for M0. |
| **Nav** | `PublishCurrentScreen()` at `native/src/main_web.cpp:282`. It already computes `name`/`focus`. Add a `static const char* sLastScr/sLastFocus`; on change emit. | After line 293: `if (strcmp(name,sLastScr)) { RB3RecordNav(sLastScr,name,focus); sLastScr=name; }`. Web calls this every 8 frames (`main_web.cpp:700`); native must call `PublishCurrentScreen()` (or a shared `RB3TraceCheckNav()`) from `RunOneFrame` so nav is captured natively too. |

### Q1 — extend in place vs new TU; RB3_FRAME_TRACE alias
**New sibling TU `rb3_session_trace.cpp` that subsumes `rb3_frame_trace.cpp`** (delete the old one once the frame tap is migrated). Rationale: the old TU is a single `fprintf`-per-frame function with no ring/interner/multi-kind structure; bolting input/nav/boot onto it would mean a rewrite anyway. The engine-side counters (`gFrameTrace*` in Loader.cpp) are reused unchanged — only the consumer moves. **`RB3_FRAME_TRACE=<path>` is kept as an alias**: `RB3TraceInit` checks `RB3_SESSION_TRACE` then falls back to `RB3_FRAME_TRACE`; when only the latter is set, the file sink emits the same `fr` rows (a strict superset of today's format — `scripts/native/frame_profiler.py` keeps working since `f/dt/lp/lpu/scr/ld/st/pend` are preserved as JSON keys). `RB3FrameTraceRecord` becomes a back-compat shim forwarding to `RB3RecordFrame`.

### Q2 — ring size + overflow policy
**Ring: 16,384 events** (`RB3_TRACE_RING` env / `?tracering=` override). At ~48B that's ~768KB — negligible vs the engine's heaps, and the native path **also streams to the file each flush** so the ring is just the web batch buffer / native crash-tail. Web flushes (D5) every ~5s, well inside 16k events.

**Overflow = tiered, not naive drop-oldest.** When full and the sink can't drain (web offline / native between flushes):
1. **Protect `in`/`nav`/`song`/`boot`/`mark`/`log`** — never dropped (they're the replay + diagnostic backbone, and rare).
2. **Decimate `fr` first** — on pressure, drop every other `fr` (then 3-of-4…) keeping long frames (`dt > sLongFrameThreshMs`, App.cpp:800) always. Frame metrics are statistical; losing fast frames costs nothing.
3. **Only if still full after frame decimation, drop-oldest** of the protected classes (last resort), and emit a single `{"k":"log","drop":N}` gap marker so the report tool knows the stream is lossy.

This keeps always-on cost flat (one `if` + one array write when on, one predicted branch when off) and bounds memory hard regardless of session length — a 30-minute idle session stays ≤768KB because `fr` decimates and the native file sink truncates the ring each flush.

## 4. Trace format — v1 schema (LOCKED)

**Wire/file format: NDJSON (JSONL).** Line 1 is a `hdr`; the rest is a time-ordered event stream. Every non-header line carries the time triple `t` (monotonic ms), `f` (frame index), and — only inside a song — `sm` (song ms). Lines beginning with `#` are comments (the existing tracer writes one; the parser must skip them, matching `rb3_frame_trace.cpp:68`).

### 4.1 Versioning rule
- `hdr.v` is the **schema version** (`1`). Bump only on a **breaking** change (field removed/retyped/renamed). **Additive** fields never bump `v` — readers MUST ignore unknown keys (forward-compat) and treat absent optional keys as their documented default. The SQLite `schema_version` (D3) is independent of `hdr.v`.
- A reader that sees `hdr.v > N_known` parses best-effort and flags the trace `version_ahead`.

### 4.2 Common envelope (every event line except `hdr`)
| key | type | meaning |
|---|---|---|
| `t` | number | monotonic ms since process start (`OSGetTick`/`CyclesToMs`, same clock the loop already uses at `src/App.cpp:796`). 1 decimal. |
| `f` | int | frame index (the loop counter, `src/App.cpp:778`) |
| `sm` | number | song ms — **emitted only when ≥ 0**; omitted entirely in menus (see 4.5). 1 decimal. |
| `k` | string | event kind |
| `cs` | int | **client_seq** — monotonic per-session counter on **every** line incl `hdr` (`hdr`=0, events 1…). The `(sid, cs)` idempotency + ordering key for SQLite ingest. **Canonical wire key = `cs`** (prose elsewhere says `client_seq`; ingest maps `cs`→column `client_seq`). |

### 4.3 Per-kind field set
| `k` | fields (beyond envelope) | source / notes |
|---|---|---|
| `hdr` | `v`,`sid`,`platform`(`"web"`/`"native"`),`started`(ISO-8601 UTC),`build`{`wasm_sha`,`git`},`asset_version`,`ua`,`viewport`[w,h],`flags`{…} | one per session; no `t/f/sm` |
| `boot` | `ph` (phase string) | the 6 `BootMark()` phases at `main_web.cpp:477` (`fetch_start`,`fetch_done`,`engine_init_done`,`gpu_ready`,`appctor_start`,`appctor_done`). Native synthesizes equivalents. |
| `fr` | `dt`,`lp`,`lpu`,`scr`,`ld`,`st`,`pend` | exact fields the tracer already writes (`rb3_frame_trace.cpp:88`): `dt`=frame ms, `lp`=LoadMgr.Poll ms, `lpu`=PollUntil ms, `scr`=screen, `ld`=loader adds, `st`=stream opens, `pend`=pending loaders. **Decimated** (4.6). |
| `in` | `pad`,`b`,`dn`,`up`,`ax`(optional) | edge-only input (4.4) |
| `nav` | `from`,`to`,`focus`,`wb`(optional, `true` on a back/cancel transition),`ov`(optional `{view,track,diff}`) | emitted by the `UIScreenChangeMsg` sink (Locked v1 contract) — exact `from`/`to`/`wentBack`. **Canonical wire key = `wb`** (prose says `wentBack`). |
| `song` | `ev`(`load`/`start`/`end`),`id`,`track`,`diff`,`score`(end),`pct`(end) | lifecycle hooks pinned by D4 |
| `au` | `under`,`frames` | worklet underrun postMessage (web); native may omit |
| `log` | `lvl`(`assert`/`warn`/`info`),`msg`,`src`(optional file:line) | chokepoint pinned by D4 |
| `mark` | `tag`,`note`(optional) | user/bug marker |

### 4.4 `in` — input edge + analog axes (Q3 ANSWERED)
Recorded at the `JoypadPoll` tap (`rb3_joypad_native.cpp:355`), **edge-only**: emit only when `b` changes vs the last `in`.
- `pad` — pad index (0 today).
- `b` — full 24-bit `JoypadButton` bitmask after this poll (`mButtons`; `kPad_NumButtons=24`, `Joypad.h:55`).
- `dn` / `up` — newly-pressed / newly-released masks for this edge (`mNewPressed`/`mNewReleased`, `Joypad.h:216-217`), so a reader needn't diff adjacent rows.
- `ax` — **analog axes, present only when any is non-zero** (keeps menu input rows tiny). Object of fixed-point ints = `round(value*1000)` (axes are normalized −1..1; the recorder reads the live floats, no FP in the wire if we prefer `Math.round`). Keys map 1:1 to `JoypadData` accessors confirmed at `Joypad.h:267-275`:

  | key | accessor | offset | use |
  |---|---|---|---|
  | `lx`,`ly` | `GetLX/GetLY` → `mSticks[0][*]` | 0xC,0x10 | whammy (`ly_whammy`) |
  | `rx`,`ry` | `GetRX/GetRY` → `mSticks[1][*]` | 0x14,0x18 | whammy (`traditional`/`neg_rx`) |
  | `lt`,`rt` | `GetLT/GetRT` → `mTriggers` | 0x1C,0x20 | trigger whammy |
  | `sx`,`sy`,`sz` | `GetSX/GetSY/GetSZ` → `mSensors` | 0x24-0x2C | guitar **tilt** (accelerometer) |

**Availability: YES.** The `JoypadData*` is in scope at the tap (`d = JoypadGetPadData(0)`, `rb3_joypad_native.cpp:362`), and the native glue already *writes* whammy into `mSticks[0][1]`/`mSticks[1][0]` (`rb3_joypad_native.cpp:530-531`) before `SendButtonMessages` — so the recorder reads the same struct the engine consumes (`GuitarController::GetWhammyBar` reads `GetLY`/`GetRX`). **Tilt caveat:** the native port does not yet synthesize tilt (`mSensors` stay 0; no keybind today), so `sx/sy/sz` will be absent until a tilt input is added — the schema reserves them, capture is free when a real Wii pad or future keybind drives them. `ax` is captured on **any** change to `b` OR to an axis crossing a deadband (|Δ|≥0.02) to avoid per-frame analog spam.

### 4.5 `sm` (song ms) population (CONFIRMED)
Computed by the exact null-guarded chain already in `rb3_http_handlers.cpp:427-432`: `TheGame->GetBeatMaster()->GetAudio()->GetTime()`, which returns **−1 in menus** (master audio NULL before a song loads). Rule: the recorder calls this each frame; **if result < 0, omit `sm` entirely** (don't write `"sm":-1`). Presence of `sm` is itself the "in a song" signal for the report tool.

### 4.6 NDJSON string escaping (`nav`/`log`/`mark`/header strings)
Screen/component/song-id/`msg`/`ua` strings are engine/asset-derived and may contain `"`, `\`, control chars, or newlines (`MILO_LOG` messages especially). The recorder MUST JSON-escape on write: `\"` `\\` `\b\f\n\r\t` and `\u00XX` for other `<0x20`. **Critically, `\n` must become `
`/`\n` so no event spans two lines** — NDJSON integrity depends on one event = one physical line. Native: a small `json_escape(dst, src)` helper (the current `fprintf("%s")` at `rb3_frame_trace.cpp:90` is unsafe for arbitrary screen names and must be replaced). Web/EM_JS side hands raw bytes to JS, which uses `JSON.stringify` per field (already escapes). The `hdr` is built with the same escaper.

### 4.7 Frame decimation policy + config knob
`fr` is the highest-volume kind (a 10-min session ≈ 36k frames). Policy (recorder-side, frame-loop-agnostic so it works for both the native loop at `src/App.cpp:808` and the web `RunOneFrame` driver which does **not** hit that block):
1. **Always emit** a `fr` for a **long frame**: `dt > RB3_TRACE_FRAME_MS` (default **20 ms** ≈ below-60fps). These are the stutters the report attributes to `lp`/`lpu`.
2. **Always emit** when a load/stream event fired this frame (`ld>0 || st>0`) — keeps spike↔asset correlation that `frame_profiler.py` relies on.
3. **Otherwise sample 1/N**: emit when `f % RB3_TRACE_FRAME_DECIMATE == 0` (default **N=30**, ≈1 Hz at 60fps) so the FPS baseline/percentiles stay representable, plus a rolling `dt` min/max could be folded into the sampled row later (v1: just the sampled instantaneous row).

**Config knobs (env on native, URL param on web, parsed once like the existing `getenv` gates):**
- `RB3_SESSION_TRACE=<path|1>` — master toggle (native sink); web uses `?trace=0` to disable (always-on default).
- `RB3_TRACE_FRAME_MS` (default 20.0) — long-frame "always emit" threshold.
- `RB3_TRACE_FRAME_DECIMATE` (default 30) — baseline 1/N sample; `1` = every frame (back-compat with today's `RB3_FRAME_TRACE` full stream), `0` = long-frames-only.

**`RB3_FRAME_TRACE` back-compat (defer to D1):** keep it as an alias that sets `RB3_SESSION_TRACE` + `RB3_TRACE_FRAME_DECIMATE=1` so the existing `frame_profiler.py` full-frame expectation is preserved. (Exact aliasing wiring is D1's.)

## 5. SQLite storage (server-side ingest)

`server.py` gains the **first non-GET routes** on this server: `POST /api/telemetry/<sid>` (ingest) and `GET /api/telemetry/<sid>` (fetch-back). The handler parses the JSONL body and upserts into SQLite at `native/web/telemetry/sessions.db`. `sqlite3` is stdlib and the repo already has a working WAL + migrations precedent in `scripts/orchestrator/database.py:115-156`.

### 5.1 Server reality check (verified)
- Class is **`http.server.ThreadingHTTPServer`** (`native/web/server.py:974`) → each request runs on its own thread, so **two POSTs can write concurrently**. SQLite must be made write-safe (§5.4).
- Dispatch is GET-only via `do_GET`→`_handle_api` (`server.py:152,345`). There is **no `do_POST`** today — it must be added, mirroring `do_GET`'s `/api/` short-circuit.
- The server binds **`0.0.0.0`** (`server.py:974`), *not* loopback — the doc's "localhost-only like the rest of the dev server" is **aspirational, not current**. So localhost-only must be **enforced in the new handler** (§5.3); the GET endpoints stay open as before.

### 5.2 Schema (final)
A dedicated module `native/web/telemetry/db.py` owns the connection + schema (keeps `server.py` thin; importable by `trace-report.py`/D7). `_apply_schema()` is idempotent (`CREATE TABLE IF NOT EXISTS`) and version-gated:

```sql
CREATE TABLE schema_version (version INTEGER NOT NULL);   -- single row; D3 ships v=1

CREATE TABLE sessions (
  sid           TEXT PRIMARY KEY,
  started_utc   TEXT, platform TEXT, build_sha TEXT, asset_version TEXT,
  ua TEXT, viewport_w INTEGER, viewport_h INTEGER, flags_json TEXT,
  ended_utc     TEXT, last_frame INTEGER,
  client_seq_hi INTEGER DEFAULT -1,    -- highest contiguous client_seq ingested
  bytes_total   INTEGER DEFAULT 0,
  first_seen_utc TEXT, last_seen_utc TEXT);

-- Canonical event log. (sid,client_seq) is the idempotency key: the C++ recorder
-- stamps a monotonic per-session client_seq on EVERY line it emits (D1/D2 add it
-- to the JSONL). Chunk re-sends and the sendBeacon tail re-POST the same lines →
-- INSERT OR IGNORE makes re-ingest a no-op.
CREATE TABLE events (
  sid TEXT NOT NULL, client_seq INTEGER NOT NULL,
  t_ms REAL, frame INTEGER, song_ms REAL, kind TEXT,
  payload TEXT,                         -- the kind-specific JSON object, verbatim
  PRIMARY KEY (sid, client_seq)) WITHOUT ROWID;

-- Denormalized hot tables, populated in the SAME txn from kind='fr'/'in'. Keyed
-- on client_seq too so they're equally idempotent; frame is NOT unique (decimated
-- frames can repeat / inputs are multi-per-frame).
CREATE TABLE frames (sid TEXT, client_seq INTEGER, frame INTEGER, dt REAL,
  lp REAL, lpu REAL, scr TEXT, pend INTEGER, PRIMARY KEY(sid,client_seq)) WITHOUT ROWID;
CREATE TABLE inputs (sid TEXT, client_seq INTEGER, frame INTEGER, t_ms REAL,
  pad INTEGER, bits INTEGER, dn INTEGER, up INTEGER, axes TEXT,
  PRIMARY KEY(sid,client_seq)) WITHOUT ROWID;
CREATE INDEX ix_frames_sf ON frames(sid,frame);
CREATE INDEX ix_inputs_sf ON inputs(sid,frame);
CREATE INDEX ix_events_sk ON events(sid,kind);
```

`fr`/`in` rows are mirrored from `frames`/`inputs` field names already in `rb3_frame_trace.cpp` (`dt/lp/lpu/scr/pend`, `RB3FrameTraceRecord` at `native/src/rb3_frame_trace.cpp:56`) so the denorm columns map 1:1. `axes` (whammy/tilt, D2/Q3) is stored as a small JSON array; NULL when absent.

**Why `client_seq` not server append-order** (supersedes the stub's `seq`): the recorder owns ordering; the server append order is non-deterministic under `ThreadingHTTPServer` + duplicate beacons. The C++-stamped `client_seq` gives a stable PK, free dedup, and gap detection (`client_seq_hi`).

### 5.3 POST ingest handler
Add `do_POST(self)` to `RB3Handler` (next to `do_GET`, `server.py:152`); route `/api/telemetry/<sid>` → `_handle_telemetry_post(sid)`, else 404.

- **Localhost-only gate** (enforced here since the server binds 0.0.0.0): reject unless `self.client_address[0]` is in `{127.0.0.1, ::1, ::ffff:127.0.0.1}` → `403`. A `--telemetry-bind-any` flag (default off) opens it for remote-device capture. GET fetch-back uses the same gate.
- **Body read / max size**: require `Content-Length`; cap at `TELEMETRY_MAX_BODY = 8 MiB` per request (`413` over cap). `sendBeacon` and `fetch keepalive` payloads are small (D5 batches ~64 KB), so 8 MiB is generous for a coalesced flush. Read exactly `Content-Length` bytes (no chunked-transfer parsing — `fetch`/`sendBeacon` always set Content-Length).
- **Chunked-upload semantics**: each POST is one independent JSONL fragment. Parse line-by-line; **tolerate a truncated final line** (drop it — the next chunk re-sends it because `client_seq` is contiguous). The `hdr` line (first ever) upserts `sessions`; all lines upsert `events` (+ `frames`/`inputs` for `fr`/`in`) under `INSERT OR IGNORE`. One DB transaction per POST.
- **Response**: `200 {"ok":true,"sid":...,"ingested":N,"dup":M,"client_seq_hi":H}`. The client can use `client_seq_hi` to drop already-acked chunks (backpressure handshake for D5).
- **Malformed JSON line** → skip that line, count it, keep going (never 500 a whole upload for one bad line); `400` only if the body has *no* parseable lines.

### GET fetch-back
`GET /api/telemetry/<sid>` reconstructs the JSONL stream (`hdr` from `sessions`, then `events` ordered by `client_seq`) and streams it back as `application/x-ndjson` for replay (`?replay=<sid>`, D6) and `trace-report.py` (D7). `?format=json` returns a single summary object (`sessions` row + counts) for the report tool. `GET /api/telemetry` (no sid) lists sessions (sid, started, platform, last_frame, bytes_total).

### 5.4 Concurrency — Q4 (write safety under `ThreadingHTTPServer`)
**Answer: single-writer queue + a dedicated writer thread; readers use WAL.** Under `ThreadingHTTPServer` (`server.py:974`) any request thread can write, and `sqlite3` connections are not shareable across threads (`check_same_thread`). Two viable models; **choose the queue**:

1. **(Chosen) Serialized writer thread.** A module-global `queue.Queue` + one long-lived writer thread owning the *only* write-connection (opened once with `PRAGMA journal_mode=WAL; busy_timeout=5000; synchronous=NORMAL`). `do_POST` parses + validates on the request thread, then enqueues `(sid, rows)` and blocks on a per-item `threading.Event` for the ack (so it can still return `ingested/dup` counts). This eliminates writer contention entirely (one txn at a time, no `SQLITE_BUSY`), bounds memory (queue maxsize → `503` backpressure if the writer falls behind), and keeps the hot path (D5 flush) cheap. GET readers open their **own** short-lived read-only WAL connection per request (WAL lets reads proceed during a write).
2. (Rejected as primary) Per-thread connection + `WAL` + `busy_timeout=5000` + retry-on-`SQLITE_BUSY`. Simpler, but two concurrent POSTs serialize via busy-wait and the idempotent `INSERT OR IGNORE` upserts can still trip `SQLITE_BUSY` spikes under multi-tab capture. WAL is still enabled in model 1, so this is the fallback if the queue proves unnecessary.

Net: **WAL on for read concurrency + a single writer thread for write serialization.** Mirrors the engine constraint that all our other main-thread command queues use (the native HTTP server already funnels commands to one thread, `native/src/rb3_http_handlers.cpp`).

### 5.5 `raw_jsonl` + retention — Q5
**Answer: drop the `raw_jsonl` blob from the stub schema; reconstruct from `events`.** The fetch-back (§GET) regenerates a byte-faithful JSONL stream from `events.payload` (stored verbatim, in `client_seq` order) — exact-replay needs *content* fidelity (every line, in order), not the original chunk framing, and `client_seq` guarantees order + completeness. Keeping a gz blob doubles storage and creates a second source of truth that can disagree with `events` after a partial/duplicate upload. (If a future need for byte-identical raw archival appears, add an `append`-mode sidecar file `telemetry/raw/<sid>.jsonl` written by the writer thread — out of DB, trivially prunable — rather than a BLOB column.)

**Retention/pruning** (this is a *local dev* tool, confirms §11 privacy non-goal — no external upload, localhost gate): a `prune_sessions(db, keep_days=14, max_sessions=200, max_bytes=500MB)` helper in `db.py`, invoked (a) lazily once per server start, and (b) via `POST /api/telemetry/prune` (localhost-gated). Eviction is oldest-`first_seen_utc` first until under both caps; `DELETE` cascades to `events/frames/inputs` by `sid` (explicit multi-table delete in one txn — no FK cascade since we avoid cross-table FKs for write speed). `VACUUM` is opt-in (manual), not per-prune.

### 5.6 Migrations
`schema_version` single-row table seeded to `1`. `db.py:ensure_schema(conn)` reads the version and runs ordered `_MIGRATIONS[v]` steps up to `CURRENT_SCHEMA_VERSION`, exactly like `scripts/orchestrator/database.py:_run_migrations` (`:156`). v1 = the §5.2 tables. Each migration is a function taking the connection; bump `CURRENT_SCHEMA_VERSION` + append a step for any later column add. Schema creation runs once on first writer-thread connect (under its single-writer lock, so no migration race).

### New open questions
- Should the writer thread `fsync` (`synchronous=FULL`) per flush, or is `NORMAL`+WAL acceptable for a dev tool? (Default NORMAL; revisit if crash-during-write loses a tail.)
- D5 must add the per-line `client_seq` to the JSONL the recorder emits — this schema's idempotency depends on it. Flagged to D1/D2/D5.

## 6 (D4) — Capture taps: lifecycle, assert, nav (verified)

All taps below are HX_NATIVE-shared (one edit serves native + web). `MILO_DEBUG=1`
is defined for both targets (`native/CMakeLists.txt:371`), so the `MILO_*` macros
are live, not stripped.

### 6.1 Nav — source of truth = `UIScreenChangeMsg`, NOT `PublishCurrentScreen`

**Correction to the doc claim.** §6 says `PublishCurrentScreen` "already detects
the change." It does **not** — it is called unconditionally every 8 frames
(`native/src/main_web.cpp:700-701`) and re-publishes the current name each time
(`main_web.cpp:282-323`); it has no `from`, no `wentBack`, and can miss A→B→A
transitions inside an 8-frame window.

**The real seam is the engine's own transition broadcast.**
`UIManager::GotoScreenImpl` fires `UIScreenChangeMsg(newScr, mCurrentScreen,
mWentBack)` exactly once per transition at `src/system/ui/UI.cpp:682`
(`Handle(msg, false)` → all sinks). The msg already carries **to** (`Object(0)`),
**from** (`Object(1)`), and **wentBack** (`Bool(2)`) — declared
`src/system/ui/UIScreen.h:90-91`. `Game` itself sinks it (`Game.cpp:193`,
`OnMsg` at `Game.cpp:925`), proving it's the canonical nav event.

**Recommended tap:** a tiny sink object (or the recorder registering as a
`MsgSource` sink on `TheUI`) that records `nav{from,to,wentBack,focus}` on
`UIScreenChangeMsg`. `focus` can be lifted at record time from
`scr->FocusPanel()->FocusComponent()->Name()` (the chain
`main_web.cpp:291-293` already uses). This is push-based, zero-poll, and exact.
*Fallback if wiring a sink is undesirable:* keep the `PublishCurrentScreen` call
site but add recorder-side change-detection (`static Symbol sLast`) — strictly
worse (no `from`, frame-granular). **Pick the `UIScreenChangeMsg` sink.**

### 6.2 Song lifecycle — exact `Game.cpp` hooks

The lifecycle is entirely inside `src/band3/game/Game.cpp` (the NetSession path
only fires the *end* message). One `RecordEvent("song", …)` per row:

| Phase | Hook (file:line) | Payload source |
|---|---|---|
| **load start** | `Game::LoadSong()` entry — `src/band3/game/Game.cpp:242-243` (`gSongLoadTimer.Restart()`) | `MetaPerformer::Current()->Song()` (`Game.cpp:244`); track/diff via `TheSongMgr` / overshell |
| **load done** | `Game::Go()` entry — `Game.cpp:389` (load gate `HandleAudioLoad()` just passed at `Game.cpp:1586`) | `gSongLoadTimer` elapsed = load duration |
| **play start** | inside `Game::Go()` after `mMaster->GetAudio()->Play()` — `Game.cpp:403`, `unk148=true` `Game.cpp:409` | song time begins at `MasterAudio::GetTime()` |
| **end + score** | `Game::OnMsg(const GameEndedMsg&)` — `Game.cpp:855` | `mResult` (`Game.cpp:868`, kWon/kLost/kSkip…), end-ms `unk124`=`msg->Float(3)` (`Game.cpp:869`); score via `TheGamePanel` export path |

The end chain is the one `scripts/native/song-end-test.py` exercises:
`Game::SetGameOver` (`Game.cpp:845`) → `TheNetSession->EndGame(...)`
(`Game.cpp:851`) → native `NetSession::EndGame`
(`native/src/rb3_netsession_native.cpp:167`) re-Handles `GameEndedMsg` →
`Game::OnMsg` (`Game.cpp:855`). **Tap `Game::OnMsg(GameEndedMsg)`, not
`SetGameOver`** — `OnMsg` is where result + end-ms + the `game_won`/`game_lost`/
`game_over` exports (`Game.cpp:872-895`) are known, and it fires exactly once
(guarded by `!IsGameOver()`). Numeric **score** is not a local in `OnMsg`; pull
it from the score/stats object at export time (the report tool can also derive it
from a later `/api/dta/eval {game ...}` — defer the exact score accessor to D2's
field-finalization, see new Q11).

### 6.3 Assert / error chokepoint — single sink exists: `Debug`

Every diagnostic path funnels through the one global `Debug TheDebug`
(`src/system/os/Debug.cpp:23`), a `Debug : public TextStream`:
- `MILO_LOG`/`MILO_FAIL`/`MILO_WARN`/`MILO_ASSERT`/`MILO_FAIL_DTA` all expand to
  `TheDebug << …` / `TheDebugNotifier`/`TheDebugFailer` (`src/system/os/Debug.h:86-102`).
- `TheDebugNotifier::operator<<` → `Debug::Notify` (`Debug.cpp:110`) → `MILO_LOG`.
- `TheDebugFailer::operator<<` → `Debug::Fail` (`Debug.cpp:121`; on HX_WEB it
  log-and-returns, `Debug.cpp:122-154`).
- **All of them ultimately reach `Debug::Print(const char*)`** — `Debug.cpp:334` —
  the single text sink (writes `mLog`, `mReflect`, Holmes, `OSReport`).

**Tap = `Debug::Print`** (one `RecordEvent("log", msg)` near `Debug.cpp:334`,
classify severity by an in-string prefix the engine already emits: `NOTIFY:`/
`FAIL:`/`FAIL-MSG:`/`APP FAILED`). This catches 100% of asserts, warnings, fails,
and plain logs with one edit, in both builds.

**Do NOT hijack `mReflect`/`SetReflect`.** It looks tempting (`Debug.h:53`,
consumed at `Debug.cpp:344`) but it's a *single* save/restore slot already used
transiently by `TextFile.cpp:76-80` and `BandDirector.cpp:296-307`; a permanent
recorder reflect would be clobbered/leaked by those. Tap `Print` directly
instead. (Severity-only filter: skip the per-frame `GAME_DBG` spam by gating on
prefix or a recorder-side rate-limiter — `Debug::Fail` already rate-limits on web
at `Debug.cpp:133-152`.)

### 6.4 Boot — already centralized

`BootMark(phase)` is the single boot-phase emitter (`native/src/main_web.cpp:477`,
6 marks `main_web.cpp:491-613`). Add `RecordEvent("boot", phase)` inside it — one
edit, all phases.

### Summary of tap edits (file:line)
- `src/system/os/Debug.cpp:334` — `Debug::Print` → `log` event (assert/warn/fail/log).
- `src/band3/game/Game.cpp:242` — `LoadSong` → `song{ev:"load_start"}`.
- `src/band3/game/Game.cpp:403` — `Go` → `song{ev:"start"}` (+ `load_done`).
- `src/band3/game/Game.cpp:855` — `OnMsg(GameEndedMsg)` → `song{ev:"end",result,ms}`.
- `src/system/ui/UI.cpp:682` (or a `TheUI` sink) — `nav{from,to,wentBack}`.
- `native/src/main_web.cpp:477` — `BootMark` → `boot` event.

## 7. Web egress — EM_JS bridge + JS flusher + beacon

```
C++ recorder ──EM_JS rb3_trace_emit(ptr,len)──► window.__rb3Trace.q (string[] batch)
                                                   │
   flush timer (~5s) / size hi-watermark ──► fetch POST /api/telemetry/<sid>  (keepalive)
   pagehide / visibilitychange:hidden ──────► navigator.sendBeacon(/api/telemetry/<sid>, blob)
```

The C++ recorder owns serialization (it produces NDJSON bytes); JS owns batching + network only. All web-egress JS lives in **`native/web/rb3_pre.js`** (a self-contained IIFE block, mirroring the asset-cache and save blocks already there, e.g. `rb3_pre.js:139-277`), so it exists *before* the wasm runs and `__rb3Trace` is always present when the first `rb3_trace_emit` fires.

### 7.1 EM_JS bridge (C++ side, in `rb3_session_trace.cpp`, `#ifdef __EMSCRIPTEN__`)

```cpp
EM_JS(void, rb3_trace_emit, (const char* ptr, int len), {
    try {
        var t = window.__rb3Trace; if (!t) return;
        // Zero-copy view → decode to a string line (already \n-terminated NDJSON).
        t.push(UTF8ToString($0, $1));   // ($1 = byte len; avoids a NUL scan)
    } catch (e) {}
});
```

`UTF8ToString(ptr,len)` is the exact idiom already used for buffer→JS handoff (`native_file.cpp:71`, `rb3_save_web.cpp:80`). I use a **`push`-wrapper** (`window.__rb3Trace.push`) defined in pre-js rather than `t.q.push` directly so backpressure/cap logic lives in one place. The recorder calls `rb3_trace_emit` once per *flush of its C++ ring* (not per event) — it accumulates events into its bounded ring (D1) and emits a multi-line chunk on a cadence (e.g. every ~64 events or when the ring half-fills), so the EM_JS crossing is amortized, not per-event. Native compiles the same TU but `rb3_trace_emit` is `#else`-stubbed to a file/HTTP sink (D1/§3) — one recorder, two sinks.

### 7.2 `window.__rb3Trace` (pre-js)

Defined in a new pre-js IIFE block alongside the existing ones. Shape:

```js
window.__rb3Trace = {
    sid: (crypto.randomUUID ? crypto.randomUUID() : (''+Date.now()+Math.random())).slice(0,16),
    q: [],            // pending NDJSON lines
    bytes: 0,         // approx pending byte count
    posting: false,   // single in-flight POST guard
    dropped: 0,       // lines dropped under backpressure
    enabled: (location.search.indexOf('notrace') < 0),  // URL opt-out
    push: function(line){
        if (!this.enabled) return;
        if (this.bytes > MAX_BUFFER) { this.dropped++; return; }  // bounded
        this.q.push(line); this.bytes += line.length + 1;
    }
};
```

- **sid** is generated once in JS at page load (`crypto.randomUUID` available under secure context / localhost). It is the route key for `POST /api/telemetry/<sid>`. The C++ recorder reads it back via `EM_ASM_INT`/`EM_ASM_PTR` to stamp the `hdr` line's `sid` (so server route and header agree).
- **MAX_BUFFER** ≈ 2 MB — the in-browser retention cap. Backpressure policy below.

### 7.3 JS flusher

```js
var FLUSH_MS = 5000, BATCH_HI = 256*1024, MAX_BUFFER = 2*1024*1024, MAX_INFLIGHT_RETRY = 3;
function drainTo(blob){ /* build one body from q, clear q/bytes */ }
function flush(useKeepalive){
    var T = window.__rb3Trace;
    if (T.posting || T.q.length === 0) return;
    var body = T.q.join('\n') + '\n';        // NDJSON
    var n = T.q.length, b = T.bytes;
    T.q = []; T.bytes = 0; T.posting = true;
    fetch('/api/telemetry/' + T.sid, {
        method:'POST', body:body, keepalive:!!useKeepalive,
        headers:{'Content-Type':'application/x-ndjson'}
    }).then(function(r){ T.posting=false; if(!r.ok) requeue(body,n,b); })
      .catch(function(){ T.posting=false; requeue(body,n,b); });
}
setInterval(function(){ flush(false); }, FLUSH_MS);
// also flush eagerly when a single batch crosses BATCH_HI between timer ticks.
```

- **Batch size**: time-driven (5 s) plus a `BATCH_HI` (256 KB) early-flush so a burst (e.g. dense input + frame rows) doesn't sit for 5 s. Each timer POST carries one batch; bodies are unbounded (server cap is generous, D3) — only the *beacon* path is size-limited.
- **Single in-flight** (`posting` guard) keeps ordering simple and avoids hammering the stdlib `ThreadingHTTPServer` (`server.py:974`) with parallel writers (D3 SQLite-writer concern). New events accumulate in `q` during the in-flight POST and ship on the next tick.
- **Backpressure (server down / failing)**: on a failed/`!ok` POST, `requeue()` prepends the body's lines back to `q` (preserving order) **only if** `bytes < MAX_BUFFER`; past the cap it **drops oldest** and bumps `T.dropped` (surfaced in the next `hdr`/`mark` so the report flags loss). This is *retain-bounded then drop-oldest* — a long offline session can't OOM the tab, and recent events (the crash window) are preferred over stale ones. No exponential backoff needed; the 5 s timer is the natural retry.

### 7.4 Beacon on teardown (Q7 — size cap)

```js
function beaconFlush(){
    var T = window.__rb3Trace;
    if (!T || T.q.length === 0) return;
    var body = T.q.join('\n') + '\n', url = '/api/telemetry/' + T.sid;
    // sendBeacon hard-rejects bodies over the UA's cap (spec ~64KB) by returning
    // false. Send the TAIL (most-recent lines fit in one beacon) and let the
    // server treat a beacon-flagged body as the session terminator.
    var BEACON_CAP = 60*1024;            // headroom under the 64KB spec limit
    while (T.q.length && body.length > BEACON_CAP) { T.q.shift(); body = T.q.join('\n')+'\n'; }
    var ok = navigator.sendBeacon ? navigator.sendBeacon(url + '?fin=1',
                 new Blob([body], {type:'application/x-ndjson'})) : false;
    if (!ok) {                            // beacon refused (too big / unsupported)
        try { fetch(url+'?fin=1',{method:'POST',body:body,keepalive:true}); } catch(e){}
    }
    T.q = []; T.bytes = 0;
}
document.addEventListener('visibilitychange', function(){
    if (document.visibilityState === 'hidden') beaconFlush();
});
window.addEventListener('pagehide', beaconFlush);
```

**Q7 (sendBeacon size cap):** `visibilitychange:hidden` is the *primary* tail flush — it fires on every tab-hide/close reliably, while the page still has time, and is the same lifecycle event the save path already uses (`rb3_pre.js:384`). On hidden we have usually just done a ≤5 s-old timer flush, so the tail in `q` is small; if it still exceeds the **~64 KB** UA cap we **trim the oldest lines down to a 60 KB beacon** (keeping the most-recent, crash-relevant tail) — `sendBeacon` returns `false` rather than throwing if over-cap, so the `if(!ok)` falls back to a `keepalive:true` fetch (no size cap) which the browser still permits during an unload that originated from a visibility change. The `?fin=1` marker tells the server this is the terminating chunk (sets `ended_utc`, D3). To structurally avoid the cap, the **periodic timer keeps `q` small**, so the beacon almost always fits.

### 7.5 COOP/COEP compatibility

`server.py:107-110` sets `Cross-Origin-Opener-Policy: same-origin` + `Cross-Origin-Embedder-Policy: require-corp` (for SAB). These constrain **embedding/credentialed cross-origin loads**, not same-origin requests. `POST /api/telemetry/<sid>` and `sendBeacon(/api/telemetry/<sid>)` are **same-origin** (relative URLs → the page's own origin), so COOP/COEP impose **no extra requirement** — no CORP/CORS header is needed on the response. There is **no CSP** on the server (confirmed: no `Content-Security-Policy` header in `server.py`), so `connect-src` does not block the POST/beacon. Action item for D3: the POST response should still pass through `end_headers()` so it carries the same security headers consistently (harmless), and must send `Access-Control-Allow-Origin` only if we ever post cross-origin (we don't). No change to `instantiateWasm`/SAB path.

### 7.6 `wasm_sha` source (Q7 — recommendation: build-time inject)

**Recommend build-time injection, not deriving from `/api/version`.** `/api/version` (`server.py:374-405`) returns an **opaque mtime composite** (`<assets_mtime>-<wasm_mtime>`), *not* a content hash of the wasm — two different builds with the same mtime, or the same build copied at a new mtime, would mislabel a trace, defeating the "self-describing repro bundle" goal. Instead:

- CMake already computes the engine SHA via `git rev-parse HEAD` (`native/CMakeLists.txt:77`). Add a matching `RB3_BUILD_GIT_SHA` compile define from `git rev-parse --short HEAD` at configure time, baked into the recorder's `hdr` line as `build.git`.
- For `build.wasm_sha`, hash the linked `rb3-web.wasm` as a POST_BUILD step in `scripts/web/build.sh` (it already deploys the wasm) and emit it into a tiny generated `native/web/build/<cfg>/rb3-build.json` (or a `window.__rb3BuildSha` in a generated pre-include). pre-js reads it into `__rb3Trace.buildSha`; the recorder stamps it on `hdr`. This makes the wasm hash **content-true** and available without a network round-trip.
- Keep `/api/version`'s token in the `hdr` as `asset_version` (its actual purpose — asset-set identity for the IDB cache), so the header carries *both*: content-true `wasm_sha` + git sha (build identity) and the asset-version token (asset identity). Replay's build-hash matching (D7) compares `wasm_sha`.

Net: `hdr.build = {wasm_sha: <content hash>, git: <short sha>}`, `hdr.asset_version = <api/version token>` — fully self-describing, no runtime fetch dependency for the build identity.

## 8. Replay

### Tier 1 — Diagnostic trace + scenario re-drive (build first)

**Injection point: feed `SendButtonMessages(0, bits)` directly — NOT the verb executor.**
`SendButtonMessages(pad, btns)` (`src/system/os/Joypad.cpp:338`) takes the *full*
held bitmask and internally diffs it against `theData->mButtons` to compute
`mNewPressed`/`mNewReleased`, then broadcasts `ButtonDownMsg`/`ButtonUpMsg` through
`gJoypadMsgSource` to **both** menu (focused UIScreen) and gameplay
`GuitarController`. This is the one true chokepoint that `JoypadPoll`
(`native/src/rb3_joypad_native.cpp:537`) already drives every frame.

Do **not** reuse `RB3GameInputExecVerbMainThread` / the `RB3_GAME_INPUT` verb queue
for replay input. That path is `ExecButton → TheUI.Handle(ButtonDownMsg)`
(`native/src/rb3_game_input.cpp:876`), which (a) injects only into `TheUI`, missing
the gameplay `GuitarController` (you can't strum a gem), and (b) would double-fire
against `JoypadPoll`'s `SendButtonMessages` for the same key — the exact reason the
web path's `DOUBLE-FIRE GUARD` stopped raw-injecting menu keys
(`rb3_game_input.cpp:1484-1496`). The verb queue stays for the *scenario scaffolding*
(track/difficulty/nofail/overshell — the non-input directives), but recorded button
input replays through the raw joypad feed.

**Mechanism (new replay source inside `JoypadPoll`):** add an `RB3_REPLAY` branch
in `rb3_joypad_native.cpp:355`, ordered with the existing
`RB3_JOYPAD_TEST_BTN`/`RB3_JOYPAD_SEQ`/pad-queue early-returns (lines 382-473). When
`RB3_REPLAY=<path|sid>` is set, parse the trace's `in` events once into a
frame-indexed table, then each poll:
```
bits = HeldBitsForFrame(frame);            // see edge reconstruction
d->mSticks[0][1] = axisLY; d->mSticks[1][0] = axisRX;  // whammy/tilt (D2)
SendButtonMessages(0, bits);
return;                                     // skip the live keyboard read
```
`frame` is the loop counter already threaded through `RunOneFrame(frame)` →
`RB3GameInputPoll(frame)`; expose it to the joypad TU via a tiny
`RB3CurrentFrame()` accessor (or pass it in — `JoypadPoll()` is called from
`SystemPoll(false)` which has no frame arg, so a file-static set by
`RB3GameInputPoll` is simplest).

**Edge reconstruction from the `in` stream.** Input is recorded **edge-only** (D2:
`in` fires on bitmask change, carrying `b` = the *full held bitmask after* the edge,
plus `dn`/`up` deltas). To replay, reconstruct the per-frame held state:
`HeldBitsForFrame(f)` = the `b` of the last `in` event with `f_event ≤ f`, defaulting
to 0 before the first event. Because `SendButtonMessages` re-derives `mNewPressed`/
`mNewReleased` from `mButtons ^ bits`, simply re-asserting the held bitmask each
frame regenerates identical down/up edges — no need to replay `dn`/`up` directly.
Analog axes (`ax` in `in`, D2) are interpolated/held the same way and written to
`d->mSticks` before the broadcast, mirroring the live whammy write at
`rb3_joypad_native.cpp:530-531`.

**Web sources the trace via the existing URL→env bridge.** `?replay=<sid>` is mapped
to `setenv("RB3_REPLAY", sid)` by extending the `kPairs` table in
`main_web.cpp:171-179` (`ApplyUrlLoaderEnv`, runs at BOOT_INIT before the first
frame). The recorder/replay loader then `fetch`es `GET /api/telemetry/<sid>` (D3)
for the JSONL and parses `in`/`au` into the frame table — same data shape native
reads from a file. Native takes `RB3_REPLAY=<path.jsonl>` directly.

**Tier-1 verification.** Replay with capture *also on*, then diff the replay's event
stream against the recording: (1) `nav` transitions must match in order
(same `from`→`to` at comparable frames); (2) `song` lifecycle (`start`/`end` + final
`score`/`pct`) must match; (3) checkpoint screenshots via the HTTP
`/api/screenshot` (`rb3_http_handlers.cpp`) at recorded nav-transition frames,
pixel-/perceptual-diffed against recording-time captures. This is the same harness
shape as `scripts/native/song-end-test.py` / `song-select-capture.py`. Scenario
fidelity (build+assets+nav+inputs+score), not frame-perfect, is the Tier-1 bar.

**Q8 answer:** feed `SendButtonMessages(0, bits)` directly (reuse the joypad
chokepoint), not the verb executor — the verb path misses gameplay sinks and
double-fires. Held bitmask is reconstructed as "last `in.b` at-or-before frame f";
`SendButtonMessages` regenerates edges internally.

### Tier 2 — Deterministic frame-perfect replay (determinism audit)

**The engine clock IS injectable — the seam already exists and is partly used.**
`TaskMgr::Poll()` (`src/system/obj/Task.cpp:369-380`) is the master clock: it does
`mTime.Split()` (a `Timer` reading `HxNativeMftb()` = `std::chrono` monotonic µs,
`os/Timer.cpp:17` / `OSGetTick` CLOCK_MONOTONIC `native/src/rvl_shims.cpp:135`), and
in `mAutoSecondsBeats` mode derives `kTaskSeconds`/`kTaskBeats` from
`Timer::CyclesToMs(mTime.mCycles)` (wall clock). Public setters to *override* every
timeline already exist: `SetSeconds`, `SetSecondsAndBeat`, `SetTimeAndDelta`,
`SetUISeconds` (Task.cpp:316-345). **Precedent:** `UIManager::Poll` already injects a
fixed-step clock under `MILO_HEADLESS` — `sHeadlessFakeUISeconds += 1/30` per Poll →
`SetUISeconds` (`src/system/ui/UI.cpp:518-525`). The Tier-2 mechanism is to generalize
this: a `DtForFrame()`-fed fixed-timeline driver that, before `TaskMgr::Poll`, sets
`kTaskUISeconds`/`kTaskSeconds`/`kTaskBeats` from the *recorded* per-frame dt instead
of wall clock (clear `mAutoSecondsBeats` so the wall-clock path doesn't clobber it).

**Non-determinism risks (what I actually found):**
1. **Audio-driven song clock (the hard one).** During gameplay `Game::Poll` overrides
   `kTaskSeconds` with `mMaster->GetAudio()->GetTime()` — the *audio device's* play
   position — run through a **de-jitter filter** (`mDeJitter.Apply`), then
   `SetSeconds(songMs/1000, false)` (`src/band3/game/Game.cpp:1597-1615`). So in-song,
   wall clock is bypassed but replaced by an even *less* deterministic source (audio
   ring position + DeJitter state, `system/meta/DeJitterPanel.h`). To make gameplay
   deterministic the replay must take the **`mRealtime` branch** (Game.cpp:1602-1604,
   `audioMs = mTimeOffset + Timer::CyclesToMs(mTime.mCycles)`) or inject `SetSeconds`
   itself, and feed the *recorded* song-ms timeline — bypassing both audio position and
   DeJitter. Feasible but invasive; needs a replay-only hook in Game::Poll.
2. **Gameplay RNG: low risk.** No time-seeded gameplay RNG found in the native shim —
   gem layout is fixed by song MIDI data, and the keygen LCG
   (`native/src/rb3_keychain_native.cpp:83`, `s_seed`) is audio-decrypt-only, never on
   the scoring path. (Confirmed: it's a file-static seeded `0xEB`/by key, used by
   shuffle1-3 for mogg keys.)
3. **Async load timing → frame-skew.** Loads are budgeted/async (`LoadMgr::Poll`,
   `RB3_LOADER_BUDGET_MS`); a slower host spreads a load over more frames, shifting the
   frame index at which a screen/song becomes ready. Inputs keyed to *frame index* then
   land on a different UI state. Mitigation: key replay to **logical milestones**
   (nav-transition reached) like the existing readiness-gated verb queue
   (`rb3_game_input.cpp:1001` `VerbReady`), not raw frame counts — i.e. Tier-2 still
   needs the Tier-1 readiness gating as a backstop.
4. **Float / thread order.** Host x86/wasm float vs Wii paired-single won't bit-match
   the target, but native-vs-native replay on the same build is reproducible to float
   tolerance. Audio runs on a separate RT thread (miniaudio/worklet) whose interleaving
   with the main thread is non-deterministic — another reason to detach the sim clock
   from audio position for Tier 2.

**Feasibility verdict.** Tier 2 is **feasible and low-risk for menu/nav/UI timelines
and load-stutter repro** (the UI clock is already injectable and headless-fixed-step
proven). It is **feasible-but-uncertain for in-song frame-perfect scoring**: the
audio-driven + de-jittered `kTaskSeconds` and async-load frame-skew must both be
defeated (replay via the `mRealtime`/injected-`SetSeconds` path + milestone-keyed
input). Recommend: ship Tier 2 for UI/nav determinism first, treat gameplay frame-
perfect as the *acceptance experiment* (M4) — record N sessions, replay, measure
divergence — rather than a guarantee.

**Checkpoint-hash scheme.** At each nav-transition (and every K frames in-song),
hash a stable state vector and emit it as a `mark{hash}` event in both record and
replay; divergence = first frame the hashes differ. Hash inputs (all already
reachable): current screen name + focus button (`PublishCurrentScreen`,
`main_web.cpp:235`), `TheTaskMgr.Seconds()`/`Beat()` quantized, in-song
`mMaster->GetAudio()->GetTime()` quantized + `mSongPos` (Game.cpp:1619-1620), live
score/`pct` (the `song`-end fields), and active-player count. FNV-1a over that tuple,
quantizing floats to e.g. 1ms / 0.01 to absorb benign float drift. The hash stream is
the M4 divergence metric.

**Q9 answer:** Yes — the clock is injectable (`SetSeconds`/`SetUISeconds`/
`SetTimeAndDelta` setters + `mAutoSecondsBeats` flag; UI already does fixed-step
under `MILO_HEADLESS`). Dominant non-determinism is the **audio-driven, de-jittered
in-song `kTaskSeconds`** and **async-load frame-skew**, not RNG (keygen LCG is
audio-only) or uninitialized gameplay seed. Verdict: Tier 2 robust for UI/nav,
uncertain for frame-perfect gameplay → gate it behind the M4 measurement.

## 9. Analysis & repro tooling

Three tools, all reading a session by `sid` from SQLite (or a `.jsonl` for offline). They share one Python module `scripts/telemetry/trace_db.py` (open DB read-only, `fetch_session(sid)`, `fetch_events(sid, kinds=…)`), so `trace-report.py` and `repro` never duplicate query logic.

### 9.1 `scripts/telemetry/trace-report.py` — EXTEND, don't fork

**Decision (Q10):** Reuse the proven `report()` body of `scripts/native/frame_profiler.py:133-199` (percentiles `pct()` :119, histogram :146-153, worst-N :156-160, per-screen :163-171, spike-clustering lp/lpu/draw attribution :174-190, transition cost :192-198) but split it: lift that frame-health renderer into `trace_report_frames.py` and have **both** `frame_profiler.py` and the new `trace-report.py` import it. `frame_profiler.py` stays as-is (a *driver* that boots+navs+writes a flat `RB3_FRAME_TRACE` jsonl); `trace-report.py` is a *pure reader* over the richer multi-kind session (boot/in/nav/song/au, not just frame rows). Reasons not to bolt onto `frame_profiler.py`: it owns the headless launch/nav lifecycle and only understands one event kind; the report tool must (a) read SQLite, (b) handle 6 kinds, (c) never launch the engine. New file, shared renderer.

**Report sections + the query behind each** (against the D3 schema: `sessions`, `events(sid,seq,t_ms,frame,song_ms,kind,payload)`, denormalized `frames`, `inputs`):

| Section | Source | Query / computation |
|---|---|---|
| Header | `sessions` | `SELECT * FROM sessions WHERE sid=?` → platform, build_sha, asset_version, viewport, duration (`ended_utc`−`started_utc`), last_frame |
| **Boot timeline** | `events k='boot'` | `SELECT json_extract(payload,'$.ph') ph, t_ms FROM events WHERE sid=? AND kind='boot' ORDER BY t_ms` → adjacent-diff = phase durations + total to last mark |
| **Frame health** (FPS, p50/p95/p99, histogram, long-frame list) | `frames` | `SELECT frame,dt,lp,lpu,scr,pend FROM frames WHERE sid=? ORDER BY frame` → feed straight into the lifted renderer; FPS = `count/(Σdt/1000)` overall + rolling per-10s window |
| **Long-frame attribution** (lp vs lpu vs draw) | `frames` | reuse :174-190 verbatim — `long_thresh=max(33,p95)`; per long frame split `dt` into `lp` (bg LoadMgr.Poll), `lpu` (sync PollUntil drain), `dt-lp-lpu` (draw/poll/gpu). Add a 4th column: % of long frames whose `frame` is within ±2 of an `in`/`nav`/`song` event (input-induced stalls) |
| **Input timeline / APM** | `inputs` | `SELECT t_ms,frame,bits,dn,up FROM inputs WHERE sid=? ORDER BY t_ms`; APM = `count(dn edges)/(span_min)`; per-button histogram via `popcount(dn)` per row (decode bits against the `JoypadButton` names) |
| **Per-screen dwell** | `nav` + `frames` | `SELECT t_ms, json_extract(payload,'$.to') scr FROM events WHERE sid=? AND kind='nav' ORDER BY t_ms`; dwell = next-nav `t_ms` − this `t_ms`; also join to `frames` for per-screen p95 (already in renderer at :163-171, keyed on `scr`) |
| **Session summary** | `events k IN('song','au','log')` | songs played + scores: `kind='song' AND json_extract(payload,'$.ev')='end'` → id/score/pct; underruns: `SUM(json_extract(payload,'$.under'))` over `kind='au'`; errors: `COUNT(*) kind='log'` |

Output: markdown to stdout (default) + `--json` blob + `--png` (matplotlib, lazy-imported, optional) for the frame-time histogram + boot waterfall. `--jsonl <path>` bypasses SQLite (parse like `frame_profiler.parse_trace` :106, then bucket rows by `kind`) so a raw native trace reports without a server. CLI: `trace-report.py --sid <sid> [--db PATH] [--json] [--png] | --jsonl PATH`.

### 9.2 `scripts/web/trace-replay.mjs` — Playwright checkpoint harness

Reuses `scripts/web/lib/core.mjs` wholesale — no new browser plumbing:
- `waitForServer(port)`, then `launchBrowser(port, {query: 'replay='+sid, viewport})` (the `query` param at core.mjs:106-127 appends `?replay=<sid>`; D6 owns the in-engine consumer that fetches the trace via `GET /api/telemetry/<sid>`).
- `createCapture(page)` for console/error/crash; `waitForBoot` then `engineState(page)` (core.mjs:210) to poll `rb3CurrentScreen`/`rb3FrameCount`.
- **Checkpoints** = the recorded `nav` + `song` events (fetched from `GET /api/telemetry/<sid>` over `http`, same module pattern). At each recorded screen-transition frame, `waitScreen({targets:[expectedScreen]})` and `captureCanvasStats(page, dir, 'cp_'+frame)` (core.mjs:431, gives `paintedPct` so a black/frozen frame is auto-flagged) + `screenshot`.
- Diff: assert replay's observed `nav`/`song`/score sequence == recorded (the Tier-1 acceptance from §8); write `saveJson({recorded, observed, diverged})` + `saveLogs`. Exit non-zero on first divergence or any captured `pageerror`/crash.
- Because replay is **input-injected by frame index** (D6), the harness does NOT press keys — it only navigates the page to `?replay=` and observes. CLI: `node trace-replay.mjs --sid <sid> [--port 8421] [--out DIR]`.

### 9.3 `scripts/telemetry/repro` — one-command relaunch in matching build

A thin Python launcher (mirrors `song-end-test.py`/`frame_profiler.py` boot harness: `free_port`, `subprocess.Popen` with `start_new_session=True`, `RB3_GAME/RB3_HTTP/MILO_HEADLESS/RB3_DATA` env, SIGTERM teardown).

`repro <sid|trace.jsonl> [--web]`:
1. Resolve the session header (`sessions` row by sid, or hdr line `k='hdr'` of the jsonl) → `build.git`, `build.wasm_sha`, `asset_version`, `flags`.
2. **Build-hash match/warn:** compute the current build identity — native: `git rev-parse --short HEAD` (cwd REPO) for git + binary mtime; web: `GET /api/version` (server.py:374, the asset+wasm token). Compare to the header. **Exact → proceed silently; mismatch → print a loud `WARN: trace built on <X>, current is <Y> — replay may diverge` and continue** (per §9 spec: warn, don't block). Add `--strict` to abort on mismatch.
3. Relaunch in replay mode:
   - **native** (default): `Popen([rb3-native], env={…, 'RB3_REPLAY': sid_or_path, 'RB3_HTTP':'1'})` (D6 defines `RB3_REPLAY`), then optionally invoke `trace-report.py` on the *new* trace and diff vs the source (regression detection).
   - **`--web`**: print the URL `http://localhost:<port>/?replay=<sid>` and shell out to `node scripts/web/trace-replay.mjs --sid <sid>` for the screenshotted checkpoint run.

This is the "bug-repro bundle": the trace is self-describing (build+asset+flags from its `hdr`), so `repro <sid>` turns any report into a runnable repro.

**Dedup verdict:** `loadperf-frametail.py` (parses `RB3_FRAME_INSTRUMENT` *log lines*, not the trace) and the web `loadperf-*.mjs` (CDP/PerformanceObserver, not the trace) are a **different data source** and stay independent — `trace-report.py` does not replace them; it consumes the unified session stream. The only shared code is the frame-health renderer lifted out of `frame_profiler.py`.

**Q10 — ANSWERED:** extend (lift+share `frame_profiler.report()`), don't fork; new `trace-report.py` is a SQLite/jsonl reader; report format + per-metric queries specified above.

## M4 (Tier-2) — feasibility + implementation plan

**Verdict: CONDITIONAL GO.** Frame-perfect replay is feasible. The gameplay sim is a deterministic function of `(inputs + injected song-ms)` for normal human play — the entire scoring/hit-detection path reads **no RNG** (ScoreTracker/ScoreUtl/StatCollector/GemManager grep-confirmed 0 calls; gem layout is fixed MIDI), and all in-song polls consume the single `songMs` that `Game::Poll` pushes into `TheTaskMgr`. The conditions are bounded and named below; none is blocking.

### Clock injection — two seams, both `HX_NATIVE` + `RB3_REPLAY_FIXED_CLOCK`

1. **Menu/UI (one edit):** `TaskMgr::Poll` (`src/system/obj/Task.cpp:369-380`) is the single per-frame sim time-advance (called at `App.cpp:561`, right after `RB3GameInputPoll(frame)`). Today its `mAutoSecondsBeats` branch derives seconds from accumulated wall-clock cycles. Under the gate, advance a new accumulator (`mReplaySeconds += recordedDt`) and drive `mTimelines[kTaskSeconds]/[kTaskBeats].SetTime(...)` from it instead of `mTime.Split()`. The native loop has **no frame pacing/vsync/sleep** to undo (free-running today), so this strictly removes nondeterminism.

2. **Gameplay (the hard half):** `Game::Poll` re-overrides `kTaskSeconds` every in-song frame at `Game.cpp:1606-1615`: `audioMs = mMaster->GetAudio()->GetTime()` → `+ GetSongToTaskMgrMs()` → `mDeJitter.Apply(...)` → `TheTaskMgr.SetSeconds(songMs/1000, false)`. Under the gate, feed the **recorded post-DeJitter `songMs`** directly into `SetSeconds`, bypassing `GetTime()` (kills audio-thread dependence), `DeJitter.Apply` (kills the stateful 32-sample median ring), and PitchMucker's `rand()`-driven `SetSpeed` perturbation. After this, `mMaster/mBand/mTrackerManager` polls (`Game.cpp:1632-1636`) and `mSongPos` (`:1618-1620`) are pure functions of the injected `songMs`.

The audio **mixer** still free-runs on miniaudio's callback thread (`rb3_stream_receiver_native.cpp:187-208`); in replay the sim reads recorded `songMs` and never reads the live position back, so audible playback may drift/mute without affecting the trace.

### Capture additions (on top of Tier-1's nav/song/input + frame index)

- **Per-frame:** `dt` (drives seam 1) and `songMs` (drives seam 2; record the value actually fed to `SetSeconds`, `-1`/omit in menus). For plain human play, **this is the only new capture.**
- **Header (conditional):** `gRand` boot seed *only* if cheat/autoplay scoring is in scope (`TrackWatcherImpl` autoplay reads RNG). `mPartResolverSeed` is **already serialized** (free) for instrument-contention. `mNetRandomSeed` is network-only → out of scope. No per-frame seed needed; gem/camera/context RNG are deterministic or cosmetic-only on the in-song path.

### Verification — `chk` event + milestone anchoring + tiered tolerance

Additive `chk` event (no `hdr.v` bump), computed at the proven per-frame hook `rb3_http_handlers.cpp:427-445` (the site `/api/health` already samples safely boot-through-gameplay). Emit every `RB3_TRACE_CHK_EVERY` (default 30 in-song, +1 per nav). Carry **both** a fast-path hash and raw fields:

- `h` = FNV-1a over the quantized tuple `{scr, focus, q(taskSec,1ms), q(beat,0.01), q(songMs,1ms), q(mSongPos.mBeat,0.01), Σ Player(Performer)::GetScore() exact, nPlayers}`. Accessors: `TheUI.CurrentScreen()->Name()`, `TaskMgr::Seconds(kRealTime)`/`Beat()`, `Game::GetBand()->GetActivePlayers()->Player::GetScore()`, `Performer::GetPercentComplete()` — all verified present/reachable.
- Raw fields alongside `h` (`scr, focus, sec, beat, sm, score[], pct`) for field-level ε classification on hash mismatch.
- Quantize floats **before** hashing (1ms / 0.01) to absorb benign x86-vs-x86 drift.

**Anchoring (required):** the replay clock is milestone-relative to defeat async-load frame-skew (`Loader.cpp:56` wall-clock budget). Milestones = recorded `nav` transitions + `song` lifecycle. Input frame indices stored relative-to-last-milestone; at each milestone the replay **blocks** (reusing the `VerbReady` readiness gate) until the host reaches the same logical state, re-zeroes the offset, then advances by recorded `dt`. `trace-diff` aligns by segment, not absolute frame.

**Tiered tolerance:** nav sequence EXACT (hard fail on reorder/miss); final score EXACT integer, pct EXACT or ±1; screen-ready within ±K frames (K=8); clocks/positions within ε after quantization (`chk.h` fast equality, raw-field ε fallback classifies benign-drift vs real). First true divergence = first segment with a hard-fail OR ≥M consecutive `chk` ε-breaches.

### CI gate (greenfield — neither file exists yet)

- `trace-diff.py`: pure reader — align by milestone, apply tiered tolerance, exit non-zero on first hard-fail or sustained ε breach.
- `trace-replay.mjs`: reuses `scripts/web/lib/core.mjs` wholesale (`engineState`/`waitScreen`/`captureCanvasStats` for black-frame auto-flag).
- Harness shape proven by `song-end-test.py` / `song-select-capture.py`: boot headless, `RB3_REPLAY` a recorded trace with capture on, diff the new `chk`/`nav`/`song` stream vs source.

### Conditions & risks (acceptance experiment)

1. Audit that no in-song sim consumer reads the **live audio position** instead of the injected `songMs` (the one truly thread-independent component). 2. Confirm the `GetTime()`+`DeJitter`+`PitchMucker` bypass is unconditional under the replay gate. 3. Milestone anchoring must hold across load-skew (the ±8 screen-ready tolerance is the backstop). 4. Scope cheat/autoplay/multiplayer explicitly (those need the `gRand` seed); default to single-attempt human play. 5. The gate measures **run-to-run** reproducibility on the same build, **not** Wii paired-single parity — hence the quantization tolerance.

### Recommendation

**Build a thin prototype first**, then the full design. Land seam 1 + seam 2 behind `RB3_REPLAY_FIXED_CLOCK`, capture `{dt, songMs}`, add the `chk` event + a minimal `trace-diff.py`, and validate replay of **one real song end-to-end** (record → replay → diff: nav exact, final score exact, `chk` within ε). The DeJitter-bypass × milestone-anchoring interaction is the highest-uncertainty piece and is the right go/no-go signal before committing the rest of M4. Tier-1 infra (`rb3_frame_trace.cpp`, the readiness-gated verb queue) already exists, so the prototype is small.

> _Audit 2026-06-09 (3-agent read-only determinism audit + synthesis). Recommendation: land the two seams + `{dt,songMs}` capture + a `chk` event + minimal `trace-diff` chk-mode, then prove ONE real song end-to-end (nav exact, final score exact, chk within ε) as the go/no-go before the full CI gate._

---

## 10. Milestones

| # | Milestone | Deliverable / acceptance |
|---|---|---|
| M0 | Trace core + frame/boot/nav capture (shared C++) | a real `.jsonl` session from a native headless run; `RB3_FRAME_TRACE` back-compat preserved |
| M1 | Input capture + **Tier-1 replay** | record a native session, replay it, matching `nav`/`song`/score |
| M2 | Web egress (always-on) + SQLite ingest | play in a browser → session lands in `sessions.db`; tail survives tab close (beacon) |
| M3 | Analysis + replay tooling | `trace-report.py`, `trace-replay.mjs`, `repro` |
| M4 | **Tier-2** deterministic replay + determinism testing | ✅ audit done (CONDITIONAL GO, see the M4 section above): two HX_NATIVE clock seams + {dt,songMs} capture + `chk` event; acceptance = one real song reproduces (nav+score exact, chk within ε) |

---

## 11. Risks & non-goals

- **Determinism is unproven** — Tier 1 is robust; Tier 2 is gated on the D6 audit.
- **Always-on overhead** — must stay ~zero when nothing's wrong; the ring +
  edge-only input + frame decimation are the levers (validate in D1).
- **Privacy/scope** — **local dev now, remote-ready**: loopback by default, no
  external upload; an opt-in network bind (+ shared-secret) enables remote
  playtester capture without schema changes. Not a production analytics pipeline.
- **Trace size** — a long session must not blow memory or the DB; bound + prune.

---

## Open Questions register (post-deep-dive)

**Resolution status:** OQ1–OQ6, OQ8, OQ10–OQ13, OQ18 are **RESOLVED** in the
**[Locked v1 contract](#locked-v1-contract-authoritative)** above (the 🔴/🟠
build-blockers are all closed). OQ7, OQ9, OQ14–OQ17, OQ19 are **DEFERRED** to
their milestone (M1/M2/M3/M4) as noted there. The full table is retained below
for traceability — read the Locked v1 contract for the decision on each.

The skeleton's original Q1–Q10 were **answered inline** in the §3–§9 sections (see each section's resolved-questions). The deep dive surfaced these consolidated questions; priority tags: 🔴 blocks M0 trace emission · 🟠 blocks M0 acceptance · 🟡 blocks a later milestone · 🔵 human/maintainer judgment · ⚪ implementer detail.

| OQ | Priority | Question | Owner |
|---|---|---|---|
| OQ1 | 🔴 M0 build-blocker (wire format) | client_seq plumbing: the C++ recorder must stamp a monotonic per-session client_seq on EVERY emitted JSONL line; D1's event POD / D1's Record* API / D2's §4.2 common envelope / D5's egress must all add this field, and it must survive chunk boundaries + sendBeacon tail duplication. (Build-blocking for D3 ingest.) | D1/D2/D5 (emit) + D3 (consume) |
| OQ2 | 🔴 M0 build-blocker (wire format) | Reconcile the input-axis set: does the captured `in` event carry 2 axes (D1 POD whammy/tilt) or the 9-axis `ax` object (D2 §4.4)? Pick one and propagate to D1's POD, D2's wire schema, D3's `inputs.axes` JSON, D6's replay writeback, and D7's histogram. | D1 (POD) + D2 (schema authority) |
| OQ3 | 🔴 M0 build-blocker (identity) | Cross-platform sid scheme: unify so C++ owns sid always (web pre-js reads it via EM_ASM at hdr time) OR JS owns it on web and C++ owns it on native. Resolve so native traces + native `repro` have a session identity and the POST route key matches the hdr. | D1 (trace core API) |
| OQ4 | 🟠 M0 acceptance (both-build frame tap) | Frame tap relocation into App::RunOneFrame (src/App.cpp:504): confirm the always-on per-frame Timer.Split() cost is acceptable, or gate the timing read behind gRB3TraceActive. Assign the band3 App.cpp edit owner. (Single tap must cover web, which bypasses the App.cpp:809 loop site.) | D1 implementer / native-port maintainer |
| OQ5 | 🟠 M0 acceptance (native boot/nav) | Native boot + native nav capture for M0: ship a shared RB3TraceCheckNav() + native BootMark equivalents hoisted out of main_web.cpp, or defer native boot/nav to a later milestone? (M0 acceptance §10 implies native, D1 leans web-only.) | D1 / D4 |
| OQ6 | 🟠 M0 acceptance (nav sink) | Nav tap implementation: register a real MsgSource sink on TheUI for UIScreenChangeMsg (push, exact, carries from/to/wentBack) vs a passive PublishCurrentScreen change-detector (no engine wiring, loses from/wentBack). D4 recommends the sink; confirm recorder ownership/registration timing (before first GotoScreen) against D1's init order. Also add `wentBack` to D1's nav POD + D2's nav schema. | D1 (init order) + D4 (recommends sink) |
| OQ7 | 🟡 M1 (song.end fields) | song.end numeric score/pct accessor: where is the final score/percent read (TheGamePanel export args, a Performer/score accessor, or derived post-hoc via /api/dta/eval)? Pin the exact accessor so D2's song.end fields are sourced. | D2 (field finalization, with D4 input) |
| OQ8 | 🟡 M0–M2 (log flood) | `log` event filter policy: severity-prefix allowlist vs recorder-side rate-limit, so always-on capture doesn't flood the ring with GAME_DBG/per-frame dev spam funneling through Debug::Print. Fold into D2's decimation/schema policy. | D2 (schema/decimation) |
| OQ9 | 🟡 M2 (au receiver) | `au` underrun receiver seam: the worklet `.port.onmessage` lives in engine-generated rb3-web.js glue, not pre-js or native/src. Needs an engine-side hook or a window global the worklet handler writes that the C++ recorder polls. Is `au` web-only in v1, or is there a native MasterAudio/Synth underrun counter to mirror? | D4 + milo-native-engine owner |
| OQ10 | 🟡 M3 (denorm vs payload) | Denorm `frames` table: add `ld`/`st` columns (and `t_ms`/`song_ms`?) so the long-frame asset-correlation query D7 advertises works off the hot table, or have D7 read those from events.payload? Resolve the denorm vs payload split for the asset-spike metric. | D3 (schema) + D7 (consumer) |
| OQ11 | 🟡 M3 (column-name drift) | Fix the `events.seq` vs `events.client_seq` column-name drift between D3 §5.2 (canonical: client_seq) and D7 §9.1's documented schema (seq). Make D7's query table cite client_seq. | D7 |
| OQ12 | 🔵 maintainer (durability) | Writer-thread durability: synchronous=NORMAL+WAL (tail-loss window on host crash mid-write) vs synchronous=FULL per flush. Confirm NORMAL is acceptable for a dev tool. | D3 impl / maintainer |
| OQ13 | 🔵 maintainer (bind scope) | Server bind scope: the dev server binds 0.0.0.0 (server.py:974), so localhost-only is enforced only in the new telemetry route via client_address checks. Move the whole server to loopback by default, or keep the per-route gate + --telemetry-bind-any opt-in? | maintainer |
| OQ14 | 🔵 maintainer (Tier-2 ownership) | Tier-2 gameplay determinism: who owns the HX_NATIVE-gated replay-clock seam inside Game::Poll (Game.cpp:1597-1615) to bypass the audio-driven + de-jittered kTaskSeconds (env RB3_REPLAY_FIXED_CLOCK), and confirm it doesn't desync the audio mixer? Or leave gameplay Tier-1-only? | native-port maintainer (band3/game + audio) |
| OQ15 | ⚪ M1/M4 (replay anchoring) | Replay frame-index vs logical-milestone anchoring: async-load frame-skew means a screen/song becomes ready at a different frame on the replay host. Should replay re-anchor to nav-transition milestones (like the VerbReady gate) — a hybrid frame+milestone clock? | D6 implementer |
| OQ16 | ⚪ M1 (frame accessor) | RB3CurrentFrame() plumbing for the replay branch in JoypadPoll (no frame arg under SystemPoll(false)): file-static set by RB3GameInputPoll(frame) each frame vs threading frame through SystemPoll. Same accessor also serves the live-input tap's gRB3TraceFrame. Nail down so replay + live-input frame indexing are identical. | D1 + D6 implementer |
| OQ17 | ⚪ M3 (bit→name table) | JoypadButton bit→name decode table for D7's per-button histogram: generate a single canonical Python table from the C++ JoypadButton enum (os/Joypad.cpp / rb3_joypad_native.cpp) rather than hardcode. Who owns generating it? | D2 (schema) / D7 (consumer) |
| OQ18 | 🔵 maintainer (report auto-run) | trace-report.py auto-run on ingest (server caches a rendered report per session) vs strictly pull-based (run on demand)? Defaulting pull-based unless a session-browser UI is wanted. | maintainer / D3 |
| OQ19 | ⚪ M3/M4 (divergence tolerance) | Replay checkpoint divergence tolerance: exact nav/song/score equality may be too strict given Tier-1 frame-index jitter (transitions settle ±N frames). Agree a tolerance window (screen matches within ±K frames) before trace-replay.mjs can be a CI gate. | D6 (determinism) / D7 (harness) |

## Deep-dive task index — ✅ COMPLETE (2026-06-09)

All 7 deep dives ran (multi-agent, code-grounded) and filled §3–§9 above. Provenance:

| Task | Section | One-line scope |
|---|---|---|
| D1 | §3 | Trace core C++ API, ring sizing, toggle, exact integration diffs, cost-when-off |
| D2 | §4 | Finalize JSONL schema per event kind + decimation policy |
| D3 | §5 | SQLite schema + POST ingest (idempotency, concurrency, fetch-back) |
| D4 | §6 | Lifecycle/assert/nav hook inventory (file:line) |
| D5 | §7 | Web egress: EM_JS bridge + JS flusher + beacon |
| D6 | §8 | Replay mechanics (Tier 1) + determinism audit (Tier 2) |
| D7 | §9 | Analysis + repro tooling spec, dedup vs existing |

Each deep dive must: (a) read the real code and cite file:line; (b) replace its
🔬 stub with a concrete design; (c) answer or defer its `Qn`; (d) flag any *new*
open questions it surfaces.

