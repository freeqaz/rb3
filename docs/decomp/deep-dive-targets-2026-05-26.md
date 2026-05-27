# Deep-Dive Decomp Targets (2026-05-26)

Curated list of functions that reward a **manual semantic deep-dive** — the kind that
took `SaveLoadManager::SetState` from 97.4% → 99.6% by finding *real bugs* (wrong vtable
slot, wrong struct size, wrong call arg, behavioral erase bug) rather than register-grinding.

These are **NOT** permuter/sweep targets. The batch_auto fleet already grinds 50–99.5%
structurally; deep dives are for functions where the gap is **semantic** (wrong method,
wrong type/size, wrong control flow) and only ground-truth asm/vtable analysis cracks it.

## What makes a good deep-dive target (the SetState criteria)
- **Large** (≥ ~2 KB): more surface for hidden semantic bugs; small funcs are register-coloring.
- **In-scope**: `band3/` (game logic) or platform-agnostic `system/` (NOT `rndwii/`, `os/`, `sdk/`, `network/`, `lib/`).
- **Mid match% (50–90%)**: enough structure present to be real, enough gap to hide bugs.
- **Plateaued under sweeps**: the permuter can't move it → the gap is semantic, not regalloc.
- **Dispatchers/state-machines** (big `switch`/`Poll`/`Update`) are gold — each case is an
  independent semantic unit you can verify one at a time.

## Method (what worked for SetState)
1. `run_diff_inspect mode=clusters` → group the diff into families.
2. Map each cluster to a source case/region (read the `.s` in `build/.../asm/` for the target body).
3. **Ground-truth the semantics**, never guess:
   - vtable slot mismatch → `scripts/dump_vtable.py <Class>` (byte = `(off-8)/4` → slot).
   - wrong `new` size / struct → read the ctor `.s` in the *defining* unit; member offsets from `stw rX, OFF(rThis)`.
   - wrong arg / call → read target asm; compare arg-register setup.
   - control flow → `/compare-asm`, look for branch-polarity / missing calls.
4. Fix one family, `run_objdiff`, repeat. Commit when structure matches.
5. **Isolate from the fleet**: work in a `git worktree` (see `[[setstate-decomp-workstream]]`
   memory for the full recipe — symlink read-only inputs, own `build/.../src`).
6. When only register-coloring remains, run `hill_climber` directly; if it plateaus, it's AT-LIMIT.

---

## Tier 1 — Big dispatchers, highest value (do these first)

| Function | % | Size | Unit | Why / prior work |
|---|---|---|---|---|
| `VocalPlayer::Poll` | 74.1% | 8.7 KB | band3/game/VocalPlayer | Per-frame vocal scoring dispatcher. Partly explored (hand-rolled find_if Duff's device, 67.5→72.7 — see `[[handrolled-find_if-duff]]`); a SetState-style case-by-case pass should find more. |
| `VocalTrack::UpdateScrolling` | 80.1% | **11 KB** | band3/bandtrack/VocalTrack | Largest in-scope gap. Several prior partial wins (deque size-vs-empty, MakeString-inline — `[[deque-size-vs-empty]]`, `[[makestring-inline-constant]]`). Clearly more structural room. |
| `GemPlayer::Hit` | 87.9% | 4.8 KB | band3/game/GemPlayer | **Core gameplay** — note-hit detection. High port value. Sibling of already-done GetCodaFreestyleExtents/SetupGems. |

## Tier 2 — Fresh, low-%, unexplored

| Function | % | Size | Unit | Why |
|---|---|---|---|---|
| `BandPatchMesh::FindXfm` | 56.5% | 3.4 KB | system/bandobj/BandPatchMesh | **Lowest % of the big in-scope funcs** → most raw structural room. Not in any at-limit note. Mesh/Vector2 math + lookup. |
| `Singer::PostLoad` | 76.0% | 2.0 KB | band3/game/Singer | Load-time setup; likely struct-init / member-order semantics (the kind SetState had). |
| `Spotlight::Build{Beam,Cone,NGCone}` + `UpdateTransforms` | ~80% | ~2.4 KB ea | system/world/Spotlight | **Cluster** of 4 related geometry funcs — one insight may fix several. CAVEAT: verify it's structural, not Vec/Mtx FPR cascade, before committing. |

## Tier 3 — Worth a look, verify-first (may be FPR/SIMD at-limit)
- `MeshDeform::Reskin` (85.7%), `CharKeyHandMidi::Poll` (86.9%), `TourDescPanel::UpdateExtendedCustom` (85.7%), `Singer` family, `StorePackedMetadata::DebugDownload` (73.3%).
- **Same-file follow-up**: `SaveLoadManager::Poll` (~88%, big dispatcher) — natural next step while SetState context is fresh; same ground-truth toolkit applies.

## AVOID — documented at-limit / out-of-scope (don't burn time)
- `BandIKEffector::ApplyPosConstraints` (67.5%) — FPR cascade, dead-end (`[[applyposconstraints]]`).
- `CharSleeve::Poll` (89%), `CharEyes::Highlight`/`LidTrack`, `CharIKFingers::CalculateHandDest` — `Length()`/`Normalize()` inline-asm FPR cascade.
- `BoxMapLighting::ApplyLight` (45%) — `psq_`/`ps_` SIMD; replace in native port (`[[boxmap-psq-blocker]]`).
- `RndParticleSys::UpdateRelativeXfm` (Part.cpp), `RndMesh::OnSync` (Mesh.cpp) — `psq_`/Vec/Mtx TU-wide cascade (`[[part-cpp-at-limit]]`, `[[mesh-cpp-at-limit]]`).

## Regenerate this list
```bash
python3 - <<'PY'
import json
r=json.load(open('build/SZBE69_B8/report.json'))
SKIP=('system/rndwii','system/os')
def scope(n):
    n=n[5:] if n.startswith('main/') else n
    return n.startswith('band3/') or (n.startswith('system/') and not any(n.startswith(s) for s in SKIP))
def num(x):
    try: return float(x)
    except: return 0.0
rows=[]
for u in r['units']:
    if not scope(u['name']): continue
    for f in u.get('functions',[]):
        p=num(f.get('fuzzy_match_percent',0)); s=int(num(f.get('size',0)))
        if 45<=p<92 and s>=600: rows.append((s*(1-p/100),s,p,u['name'].replace('main/',''),f['name']))
for m,s,p,un,nm in sorted(rows,reverse=True)[:40]: print(f"{m:5.0f} {s:5d} {p:5.1f}%  {un} :: {nm[:44]}")
PY
```
