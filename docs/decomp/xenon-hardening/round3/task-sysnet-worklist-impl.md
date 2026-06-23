# Round 3 — TASK #2 IMPL: system/network porting-worklist handoff

**Status:** DONE. The 516 net-new system/network Wii→Xenon identities are emitted
as a porting worklist + per-fn identity oracle, mirroring the just-shipped band3
worklist. These were human-validated at **0.967 precision** (system 14/15 = 0.933,
network 15/15 = 1.000; HIGH+BSim≥30 core 11/11 = 1.000 in-sample), clearing the
**≥0.85 handoff bar** — so, like band3, they get a worklist (SYNTHESIS §3).

**Constraints honored:** no ghidriff / Ghidra / JVM session (pure data transform —
consumes existing artifacts only); NOT a `target_symbol_map.json` injection
(additive worklist/oracle); `fn_resolver.py` untouched; rb3-xenon commit additive +
branch-checked.

This is the **second-priority** lever behind band3: much of system/network is
shared Milo engine + Quazal netcode where DC3 BinDiff also helps, so marginal value
is lower even though precision is higher.

---

## TL;DR — what shipped

| Artifact | Repo / path | Tracked? | What |
|---|---|---|---|
| Generator (canonical) | `rb3-xenon/tools/gen_sysnet_port_worklist.py` | yes | Self-verifying; sibling of `gen_band3_port_worklist.py`, generalized to `category ∈ {system, network}` |
| CW demangler (reused) | `rb3-xenon/tools/demangle_cw.py` | already tracked | Same demangler band3 uses (not re-added) |
| Data feed | `rb3-xenon/sysnet_port_worklist.json` | no (gitignored) | 516 rows + `tu_summary` + `ranked_tus` |
| Human checklist | `rb3-xenon/docs/plans/sysnet-port-worklist.md` | yes | TU-ranked, safe-first subset first, per-TU rosters (2336 lines) |
| `.gitignore` entry | `rb3-xenon/.gitignore` | yes | `/sysnet_port_worklist.json` (sibling of `/band3_port_worklist.json`) |
| This handoff doc | `rb3/docs/decomp/xenon-hardening/round3/task-sysnet-worklist-impl.md` | yes (rb3) | — |

Commit: see `## Commit + branch`.

---

## 1. Re-derived net-new counts (against the LIVE map)

Net-new = ghidriff ACCEPT identity (`ghidriff_identities.json`) with
`category ∈ {system, network}` whose normalized Xenon `rb3_addr` is NOT a key in
the **live** production map (`scripts/target_symbol_map.json`). Address join =
`lower(); strip "0x"; lstrip("0"); zfill(8)`. The map has **drifted to 13,199**
entries (up from recon's ~13,023) as porters land identities — re-derived against
that live map, not a stale count.

```
LIVE target_symbol_map: 13,199 entries
system : total 438 | in-map 127 | NET-NEW 311 | TUs 151
network: total 216 | in-map  11 | NET-NEW 205 | TUs 125
TOTAL net-new system+network = 516 across 276 TUs
```

These match the SYNTHESIS estimate (system ~311 + network ~205) **to the entry**.

### By confidence stratum

| Stratum | system | network | combined |
|---|---|---|---|
| high (Exact/SwitchSig/Implied/SymbolsHash) | 22 | 0 | **22** |
| BSim ≥ 30 | 57 | 32 | **89** |
| BSim 20–30 | 96 | 89 | **185** |
| BSim 15–20 | 136 | 84 | **220** |
| **total** | **311** | **205** | **516** |

- **Safe-first core (HIGH + BSim≥30): 111** = system 79 (22 high + 57 ≥30) +
  network 32 (0 high + 32 ≥30). Network has **0 high** (no exact/switch/implied
  feeders — all BSim), exactly as SYNTHESIS §3 noted.
- The strata counts exactly reproduce SYNTHESIS §3's "system 22 high + 57 ≥30;
  network 32 ≥30".

Confidence label rule (same as band3): **high** = match_type ∈
{ExactInstructionsFunctionHasher, SwitchSigHasher, Implied Match, SymbolsHash} OR
BSim `simconf ≥ 30`; **bsim≥30 / bsim20-30 / bsim15-20** = BSim `simconf` bands.

---

## 2. The two coupled outputs (form per SYNTHESIS §3 — additive, NOT a map injection)

**(1) `rb3-xenon/sysnet_port_worklist.json`** — gitignored data feed (sibling of
`band3_port_worklist.json` / `ghidriff_identities.json`), one row per identity,
grouped by TU.

**(2) `rb3-xenon/docs/plans/sysnet-port-worklist.md`** — tracked, beside
`band3-port-worklist.md`. Sections: Safe-first subset (HIGH+BSim≥30, 111 rows) →
TU ranking (276 TUs, `(#high + #bsim≥30) desc`, then total desc) → per-TU rosters.
The BSim 15–20 tier is **clearly flagged "confirm-on-consume"** in the safe-first
section, the strata legend, and the per-TU rosters.

Both are **additive + reversible**: delete two files (one gitignored, one doc) +
the gitignore line; zero effect on the build, the map, objdiff, report.json, or
`fn_resolver`.

### Per-entry fields (the JSON feed row)

```json
{
  "rb3_addr": "0x827bb4f0",
  "wii_addr_bank8": "0x807995c0",
  "wii_symbol": "Init__11TrackWidgetFv",
  "wii_demangled": "TrackWidget::Init(...)",
  "tu": "TrackWidget.o",
  "category": "system",                            // system | network
  "src_path": "src/system/track/TrackWidget.cpp",  // derived from CW .o path
  "src_exists": true,                              // rb3 source present?
  "match_type": "ExactInstructionsFunctionHasher",
  "match_types": ["ExactInstructionsFunctionHasher"],
  "confidence_label": "high",                      // high | bsim>=30 | bsim20-30 | bsim15-20
  "simconf": null,
  "dc3_cannot_provide": false                      // see §4
}
```

Top-level: `_meta` (counts by category + stratum, safe-first count,
DC3-unreachable count, precision prior, failure-mode note), `tu_summary`,
`ranked_tus`, `worklist`.

### src_path derivation (generalized from band3)

The band3 generator hardcoded `src/band3/<sub>/`. The CW `.o` path is
`...\band3_wii\<top>\src\<sub>\wii_release\<File>.o` with `<top> ∈ {system,
network}`. The rb3 tree mirrors this exactly — `src/system/<sub>/` (lowercase subs:
beatmatch, bandobj, …) and `src/network/<sub>/` (CW-cased subs: Core, Plugins,
ObjDup, …). The generator extracts `<top>/<sub>` and emits `src/<top>/<sub>/<File>.cpp`.
**370/516 derived src_paths exist on disk**; the 146 absent are undecompiled TUs
(overwhelmingly Quazal network — see §4).

---

## 3. Safe-first slice + the confirm-on-consume tier (SYNTHESIS §3 flags)

- **Safe-first = HIGH + BSim≥30: 111 rows** (system 79 + network 32). Human-judged
  1.000 on the in-sample core (11/11). Surfaced first in the markdown.
- **BSim 15–20 = "confirm-on-consume"** (220 rows). That band holds the **lone
  measured miss** across the 30-pair sample: **`TrackWidget::Init` aliased to its
  sibling `TrackWidget::Empty`** — two 20-byte `mImp->virtual()` forwarders
  differing ONLY in the vtable-slot immediate (Init forwards through slot `0x44`,
  Empty through `0xc`; both confirmed from `build/SZBE69_B8/asm/system/track/
  TrackWidget.s`). The worklist carries the `Init` BSim-15.142 row in the 15–20
  tier (and a separate `high` `Init` row from the exact feeder), so a porter
  consuming the BSim row hits exactly the "verify per-fn" gate.

---

## 4. DC3 reachability — `dc3_cannot_provide`

band3 set this `True` always (DC3 has no Rock Band gameplay). For system/network it
**defaults False** — most is shared Milo engine that DC3 (same engine, same Xbox
360 toolchain) supplies. A row is flagged **`dc3_cannot_provide=true` only when
genuinely DC3-unreachable**: its rb3 source file is absent (undecompiled TU) AND no
same-named `.cpp` exists anywhere under the DC3 `src/system` / `src/network` tree.

```
144 / 516 flagged genuinely DC3-unreachable  (network 143, system 1)
```

This cleanly isolates the **Quazal / ObjDup netcode** proprietary to RB3's Wii
build with no DC3 twin (DuplicationSpace, DOCore, Station, RMC, RootDO, Session…) —
which is precisely where this worklist's marginal value over DC3 BinDiff is
highest. The rosters mark each TU `DC3 cannot-provide` vs `DC3 shared`.

---

## 5. VERIFY (task #3 — gate passed)

The generator's VERIFY pass runs on every regen and **exits non-zero on any
failure**. Independently reproduced:

1. **Every `wii_symbol` resolves in the CW map (`rb3/orig/SZBE69_B8/files/
   band_r_wii.map`) to its claimed `wii_addr_bank8`** — **516/516, 0 missing, 0
   addr-mismatch** (generator exit 0). The CW map is `secoff size VA fileoff align
   symbol … path.o`; the VA is column 2 (heeding SYNTHESIS's parse-column warning).
2. **0 entries already in `target_symbol_map.json`** — re-checked independently:
   **0** (by construction of the net-new filter, against the live 13,199-entry map).
3. **Demangle rate: 499/516 = 96.7%** resolve to `Class::Method(...)` (Quazal `Q2`
   scopes handled). **17 fallbacks** carry the raw CW name — all operators
   (`__as__`, `__eq__`, `__ls__`, `__apl__`, `__amu__`), one C function
   (`deflateInit_`), and STL templated free functions (`__median`, `__adjust_heap`,
   `__unguarded_partition`). Acceptable: the raw `wii_symbol` is in every row and a
   porter reads the real body via `bin/analyze-function <wii_symbol>`.
4. **6 signatures spot-checked by hand** against the CW map (addr + .o path):
   - `AddPhrase__20PhraseListCollectionF…` → `8063eb90`, PhraseList.o ✓
   - `__ct__Q26Quazal22SingleThreadCallPolicyFv` → `80034a80`, SingleThreadCallPolicy.o ✓
   - `Init__11TrackWidgetFv` → `807995c0`, TrackWidget.o ✓ (the lone-miss anchor)
   - `Poll__11TrackWidgetFv` → `80799690`, TrackWidget.o ✓
   - `Authenticate__Q26Quazal21ProcessAuthenticationF…` → `80072500`, ProcessAuthentication.o ✓ (network)
   - `Units__13RndAnimatableCFv` → `8087a120`, Animatable.o ✓ (system high)
5. **`target_symbol_map.json` and `fn_resolver.py` untouched** — additive only
   (generator + doc + gitignore line). Confirmed by the commit file-list (§Commit).

**VERIFY: PASS.**

---

## 6. Regenerate / consume

```bash
# Regenerate both outputs (cwd-independent; VERIFY pass gates it):
python3 /home/free/code/milohax/rb3-xenon/tools/gen_sysnet_port_worklist.py

# Consume one row (real Wii body + arg shape), from the rb3 repo:
cd /home/free/code/milohax/rb3 && bin/analyze-function Init__11TrackWidgetFv
```

---

## Commit + branch

- **rb3-xenon branch at commit time:** checked via `git branch --show-current`
  immediately before committing (was `main`; see StructuredOutput for the actual
  branch at commit — porters may have switched it). Additive only — 3 staged files
  (generator + doc + gitignore line); **no `target_symbol_map.json` / `fn_resolver`
  / report.json / build touched**.
- Files staged in rb3-xenon: `tools/gen_sysnet_port_worklist.py`,
  `docs/plans/sysnet-port-worklist.md`, `.gitignore`.
  (`sysnet_port_worklist.json` is gitignored/regenerable, intentionally unstaged;
  `tools/demangle_cw.py` is reused, already tracked.)
- Files in rb3: this doc (committed on `xenon-round3-recon`).

---

## For the next agent

- **The worklist IS the deliverable** — `rb3-xenon/docs/plans/sysnet-port-worklist.md`
  (human) + `sysnet_port_worklist.json` (machine). Port the **safe-first core
  (HIGH+BSim≥30, 111 rows)** first, then BSim 20–30, then **confirm-on-consume each
  BSim 15–20 row** before trusting its name.
- **DC3 first for `DC3?=shared` rows.** For shared engine TUs, DC3's already-decomp'd
  body (`/dc3-pair`, BinDiff) is the faster base; this worklist's value concentrates
  on the **144 `DC3?=cannot-provide` Quazal/ObjDup netcode rows**.
- **Do NOT inject these into `target_symbol_map.json`** — CW≠MSVC mangling, TUs
  uncompiled; wrong key mis-pairs objdiff. Confirm each name when the TU is actually
  ported (`gen_game_target_map.py --tu <TU>`).
- **Dominant (only) failure mode = same-TU sibling aliasing**, concentrated in
  BSim 15–20 (the `TrackWidget::Init` vs `::Empty` miss). The round-3 priority-3
  sibling-aliasing vet check (diff vtable-slot / type-tag / node-size immediates on
  near-identical same-TU bodies) is still NOT built — it would have caught this and
  every prior wrong pair. Highest-leverage next investment; run it over BSim 15–20
  first.
