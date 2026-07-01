# T3 — Xbox oracle feasibility (rb3-xenon vs Xenia) — 2026-07-01

Goal: get a **ground-truth Xbox band-member FACE closeup** to compare against native
(native loads Xbox `.milo_xbox` assets ⇒ Xbox is the most faithful oracle). Two
candidate paths assessed. **Neither yields a quick screenshot today; Dolphin (Wii)
is the better bet.**

---

## PATH A — rb3-xenon — NOT VIABLE (premise was wrong)

The task framed rb3-xenon as "a XenonRecomp-based NATIVE x86 static recompilation of
Xbox 360 RB3 that runs without an emulator." **That is not what rb3-xenon is.**

Per `../rb3-xenon/README.md` + `CLAUDE.md`, rb3-xenon is a **from-scratch MSVC-X360
decomp-matching project** (like the Wii rb3 repo): it compiles C++ source and diffs the
resulting `.obj` against the retail XEX for byte-match %. It does **not** statically
recompile the binary and produces **no runnable x86 game**.

- Decomp match baseline: **8.14% code / 10,692 of 65,572 functions** matched
  (`build/45410914/report.json`). A matching metric, not an executable — even at 100%
  it would yield a re-buildable *Xbox* PE, not a native renderer.
- The only thing rb3-xenon *runs* is `native/` → **`rb3-dta`**, a headless **DTA text
  parser** (parses `songs.dta` into the engine's DataArray tree). `CMakeLists.txt` has a
  single `add_executable(rb3-dta ...)`. **No GPU, no audio, no `.milo` scene, no
  character rendering** — same status the READMEs state ("no GPU or audio yet").
- No XenonRecomp component exists anywhere in the tree.

⇒ **rb3-xenon cannot render a character face and is not "close" to doing so.** It is a
matching project, not an oracle. Skip.

---

## PATH B — Xenia — VIABLE but not quick (DC3-tuned fork, RB3 doesn't render O.O.B.)

Unlike the plan's assumption ("NOT built, build/bin/Linux empty"), **Xenia is fully
built** and there's a headless run log from today (`xenia-headless.log`, Jul 1 18:29).

- Built executables in `../xenia/build/bin/Linux/{Release,Checked,Debug}/`:
  - `Release/xenia` (56 MB, GTK/X11/SDL2/Vulkan GUI)
  - `Checked/xenia-headless` (222 MB) + `Debug/xenia-headless` — the frame-dump path
- Host GPU is fine: **NVIDIA RTX 3090, Vulkan 1.4.341, `/dev/dri` present** (but
  `DISPLAY` is empty — headless offscreen only; the GUI binary would need Xvfb).
- The `dump_frames_path` / `headless_capture_interval` cvars are compiled in
  (confirmed via symbols) — headless renders on the real GPU and dumps PPM frames, so
  no display is needed for the headless path.
- rb3-xenon ships a documented capture recipe: `.claude/skills/xenia-gameplay/SKILL.md`
  (`--target=…default.xex --gpu=vulkan --dump_frames_path=… --headless_capture_interval=…`).

### RB3 xex situation — CONFIRMED PRESENT
- **`../rb3-xenon/orig/45410914/default.xex`** — 15,478,784 bytes, the **vanilla retail
  RB3 Xbox 360 executable**. (Duplicate at `../rb3-sizedvec/orig/45410914/default.xex`.)
- **Title ID `45410914` = "Rock Band 3"** — confirmed two ways: rb3-xenon README, and
  Xenia's own boot log below (`BOOT: Title Name: Rock Band 3`).

### Boot test result (this session) — LOADS, does NOT render
Ran `xenia-headless --target=…45410914/default.xex --gpu=vulkan --dump_frames_path=…
--headless_capture_interval=120 --headless_timeout_ms=150000` (log:
`/home/free/tmp/c8-xenia-rb3/boot.log`):

- RB3 **module loads + kernel init succeeds**: `BOOT: Title loaded successfully / Title
  ID: 0x45410914 / Title Name: Rock Band 3 / Kernel state initialized`. Vulkan backend
  up, pipeline cache loaded, headless frame dump enabled.
- Then it **faults during early init**: one guest exception + stack-walk, unstubbed
  **`CX2SourceVoice::{Initialize,Start,Stop}`** (XAudio2), and present-pipeline probes
  `DxRnd::Present NOT COMPILED` / `D3DDevice_Swap NOT COMPILED`.
- **0 `VdSwap`s, 0 frames dumped.** Never reached a rendered frame.

Root cause: **this Xenia is a heavily DC3-decomp-specific fork** (`CLAUDE.md`:
"Xenia fork with DC3 boot hack pack"; all `docs/` are `dc3-boot/`, `dc3_render_*`, etc.).
Its hack-pack instrumentation and stub table are hardcoded to **DC3-decomp guest
addresses** (`dc3_hack_pack.cc`, `xenia_dc3_patch_manifest.json`) and fire spuriously on
retail RB3 — the `App::App()` probes, `CX2SourceVoice` stubs, and present probes are all
DC3 artifacts, not RB3-aware. RB3 crashes in audio/present init before rendering.

### What it would take to get an Xbox band-face frame
Not a quick capture — realistically hours-plus:
1. Get RB3 past init in an RB3-aware Xenia — either **stub the retail RB3 audio path** /
   disable the DC3 hack-pack probes in this fork, **or** build **stock/upstream Xenia**
   (mainline Xenia runs retail RB3 to gameplay well; this fork's DC3 patches are the
   blocker, not Xenia-vs-RB3 incompatibility).
2. Then **navigate headlessly to gameplay** (or the character creator) to make band
   characters appear — there is **no input harness** in the headless path; needs a
   scripted-input/save-state route to reach a venue.
Only step 1's "stock Xenia" sub-path avoids fighting the DC3 fork, but adds a full Xenia
build + a nav/save solution.

---

## RECOMMENDATION

**Use Dolphin (Wii), which the other agent is already handling, for the band-face
reference. Do not invest in the Xbox paths for this task.**

- **rb3-xenon**: dead end for rendering (0-runnable; DTA parser only). Ignore.
- **Xenia**: the *only* real Xbox oracle, and it's built with the retail RB3 xex on hand
  — but the local build is a DC3-specific fork that won't render RB3 out of the box, and
  reaching band characters needs a headless nav/save harness that doesn't exist yet.
  Cost is hours+, not a screenshot.
- **Dolphin/Wii is the pragmatic oracle**: it's the emulator for the exact game we
  decompile, the Wii wbfs disc is on hand, and the **shared Milo engine means the same
  intended band poses/faces**. For the retargeted question (per `FINDINGS.md`: *what
  should a band face's texture / skin-shading / eye-brightness look like*), the character
  art is shared across platforms — Wii is a faithful face oracle. The only Xbox-specific
  delta is asset byte layout (tiling/endianness), which does not change what a face
  *should look like*.

**Escalate to Xenia only if** a Dolphin/Wii face closeup proves insufficient and an
Xbox-exact render is specifically required. In that case the lower-risk route is **stock
upstream Xenia + retail `45410914/default.xex` + a save-state/scripted-input harness to
reach a gameplay venue**, not patching the DC3 fork.

### Reference paths
- RB3 retail xex: `/home/free/code/milohax/rb3-xenon/orig/45410914/default.xex`
- Xenia headless: `/home/free/code/milohax/xenia/build/bin/Linux/Checked/xenia-headless`
- Capture recipe: `/home/free/code/milohax/rb3-xenon/.claude/skills/xenia-gameplay/SKILL.md`
- This session's boot log: `/home/free/tmp/c8-xenia-rb3/boot.log`
