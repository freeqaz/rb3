# rb3-xenon identity-ingestion conventions (Scout S1 — 2026-06-11)

This document maps how `/home/free/code/milohax/rb3-xenon` stores and
consumes function identities, and proposes the exact ingest design for
the vetted ACCEPT pairs from ghidriff run 3.

---

## 1. Existing identity files — schema and provenance

All identity files live in the rb3-xenon repo **root** and are
**gitignored** (regenerable). The `.gitignore` entries are at lines 54–83
of `/home/free/code/milohax/rb3-xenon/.gitignore`.

### fingerprints.json (12.9 MB, gitignored)

Written by `tools/fingerprint_match.py`. A dict keyed by **bare uppercase
hex** (`"82260000"`), not `0x`-prefixed. Values are fingerprint records
(strings, callees, immediate constants). Only read by `fingerprint_match.py`
itself as a source of the autoid / autoreport commands. Not consumed by
fn_resolver or target_symbol_map pipeline.

### autoid.json (104 KB, gitignored)

Written by `tools/fingerprint_match.py autoid`. A list of records:

```json
{
  "fn": "fn_8273ED68",     // fn_ prefix + uppercase hex
  "size": 7964,
  "score": 36,
  "n_strings": 38,
  "src": "../rb3/src/system/obj/DataFunc.cpp",
  "matched_strings": [...]
}
```

Consumed by `fn_resolver.py` as **tier T7** (lowest confidence; gives
source-file stem only, no mangled name). `fn_resolver._get_autoid_idx()`
indexes it by integer address (stripping the `fn_` prefix).

### dc3_content_match.json (1.1 MB, gitignored)

Written by `tools/dc3_content_match.py`. A list of records:

```json
{
  "rb3_addr": "0x82260000",    // 0x-prefixed lowercase
  "dc3_name": "??1App@@QAA@XZ",  // MSVC-mangled
  "dc3_obj": "App.obj",
  "size": 20,
  "masked_sha": "4a93..."
}
```

Consumed by `fn_resolver.py` as **tier T3** (confidence 0.95, byte-identical
SHA match). Address key `rb3_addr` is `0x`-prefixed lowercase.

### unified_id.json / unified_id_vtable.json / unified_id_rtti.json /
   unified_id_rtti_low.json / unified_id_callgraph.json (all gitignored)

Written by `tools/fingerprint_match.py` BinDiff export. All follow the
`unified_id.json` schema (list of records, `rb3_addr` = `0x` lowercase):

```json
{
  "rb3_fn": "fn_82260018",
  "rb3_addr": "0x82260018",
  "size": 104,
  "source": "bindiff",
  "dc3_name": "?DrawRegular@App@@IAAXXZ",   // MSVC-mangled
  "dc3_name_demangled": "App::DrawRegular",
  "dc3_obj": "App.obj",
  "dc3_inline_only": false,
  "bindiff_src": "../dc3-decomp/src/App.cpp",
  "similarity": 1.0,
  "confidence": 0.982,
  "algorithm": "function: prime signature matching"
}
```

Consumed by `fn_resolver.py` as **tier T5** (bindiff_dc3 / vtable / rtti).
The `confidence` field is used directly as the Identity confidence (capped
to avoid inflated values in downstream consumers).

### unified_id_rb3wii.json (3.5 MB, gitignored)

Written by a cross-arch BinDiff run (DC3→RB3-Wii, then Wii names mapped
onto rb3-xenon addresses). A list of records:

```json
{
  "rb3_addr": "0x82260000",           // Xenon address, 0x lowercase
  "rb3_fn": "fn_82260000",
  "wii_addr": "0x8043e790",           // ** BANK 5 ** Wii address
  "wii_name": "TourProgress::GetTourStatus(int)_const",  // demangled
  "bindiff_src": "band3/src/tour/TourProgress.cpp",
  "similarity": 0.9222,
  "confidence": 0.9707,
  "algorithm": "function: MD index matching...",
  "size": 8,
  "source": "bindiff_rb3wii"
}
```

**CRITICAL: `wii_addr` here is a Bank 5 address, NOT Bank 8.** This
was confirmed in Round 1 (T4 notes in
`xenon-precision-hardening-2026-06-10.md`): `0x8013cd10` maps to
"SetInCoda" in Bank 5 but to RebuildBeats in Bank 8. The eval tool's
T4 seed-ingestion re-resolves every `wii_name` → Bank 8 address via
a 1:1-unique normalize-join to avoid this drift.

Consumed by `fn_resolver.py` as **tier T6** (`rb3wii_bindiff`), capped
at confidence 0.85 ("cross-arch = noisier"). Also consumed by:
- `tools/gen_game_target_map.py` — derives MSVC mangled names for game
  TUs from `wii_name` (parsed by class/method/arity), then writes into
  `scripts/target_symbol_map.json` so `obj_target_symbol_renamer.py`
  can rename the dtk-split target .obj symbols.
- `tools/game_splits.py` — derives `.text` split spans from oracle entries.

### game_content_match.json (gitignored, separate line)

Byte-SHA matches from game-TU compiled objs. Same schema as
`dc3_content_match.json` with `mangled_name` / `unit` fields instead
of `dc3_name` / `dc3_obj`. Consumed as T3 in fn_resolver.

---

## 2. Symbol-application flows

The pipeline from identity → applied Ghidra / objdiff name has two
parallel tracks.

### Track A: target_symbol_map.json (for objdiff matching)

`scripts/target_symbol_map.json` is a **tracked** dict `{"0xADDR": "mangled"}`.
Keys are `0x`-prefixed UPPERCASE hex (e.g. `"0x82260018"`). Values are
MSVC-mangled names.

The build step `scripts/obj_target_symbol_renamer.py` reads this map and
rewrites dtk-split target `.obj` symbol tables so that anonymous `fn_<addr>`
symbols become the MSVC-mangled name, allowing objdiff to pair target↔base
by name and register a match percentage.

`safe_name_merge.py` is the gate tool that validates candidates before
writing into target_symbol_map.json. It enforces uniqueness, Ham→Band
normalization, and ICF-family dedup rules.

`gen_game_target_map.py` is the main bulk-population tool: it reads
`unified_id_rb3wii.json`, parses `wii_name` into class/method/arity, and
matches against the compiled `.obj`'s defined MSVC symbols to pick the
exact mangled form.

### Track B: fn_resolver.py (for general identity lookups)

`tools/fn_resolver.py` aggregates all identity tiers into a ranked
Identity list per address. It is the lookup oracle for the orchestrator
(`lookup_merged_symbol` MCP tool) and can build a full `fn_resolver_index.json`.

It does NOT write to target_symbol_map.json; it is a read-only query tool
that exposes what is known about any `fn_` address.

### Track C: Ghidra project (port 8002)

The rb3-xenon Ghidra project at `ghidra_projects/RB3Xenon/` has no
automatic script to apply external names. Names arrive only via dtk's
`obj_target_symbol_renamer` or manual batch rename. The Ghidra MCP on port
8002 is a read-only query tool.

---

## 3. Address-format pitfalls

Three address spaces are in play:

| Context | Format | Example | Notes |
|---|---|---|---|
| vetted_identities.json `xenon_addr` | `0x` + lowercase | `0x82260018` | Xenon .text |
| vetted_identities.json `wii_addr` | `0x` + lowercase | `0x8000fb10` | **Bank 8** CW map |
| matches.json `p1_addr` / `p2_addr` | bare lowercase hex (no `0x`) | `8000fb10` | Bank 8 Wii / Xenon |
| unified_id_rb3wii.json `wii_addr` | `0x` + lowercase | `0x8043e790` | **Bank 5** (DIFFERENT!) |
| target_symbol_map.json keys | `0x` + UPPERCASE | `0x82260018` | MSVC rename map |
| fingerprints.json keys | bare UPPERCASE hex | `82260000` | No `0x` |

The single most dangerous conflation is **Bank 5 vs Bank 8 Wii addresses**.
`unified_id_rb3wii.json::wii_addr` is Bank 5; `vetted_identities.json::wii_addr`
is Bank 8 (cross-checked manually: `0x8000fb10` = `DrawRegular__3AppFv` in
the Bank 8 map `orig/SZBE69_B8/files/band_r_wii.map`).

Any new file that joins against Wii addresses MUST document which bank it
uses and must NOT silently load into `fn_resolver._get_rb3wii_idx()` (which
expects Bank 5).

---

## 4. The right landing spot

### Why not extend unified_id_rb3wii.json?

`unified_id_rb3wii.json` expects Bank 5 `wii_addr`. Our data has Bank 8.
Loading our data under the same key in `fn_resolver._t6_rb3wii()` would
silently pass Bank 8 addresses to downstream consumers that expect Bank 5,
breaking the rb3wii cross-check and `gen_game_target_map.py` (which parses
`wii_name` to derive MSVC names — this works fine, but address-based joins
would be wrong).

### Why not extend dc3_content_match.json or unified_id.json?

Those expect MSVC-mangled `dc3_name` values. Our `wii_symbol` field is
CW-mangled (e.g. `ShowClothes__9ClosetMgrFv`). The fn_resolver T3/T5 tiers
pass the mangled name directly to MSVC's demangler — a CW-mangled string
will demangle incorrectly there.

### Recommended: new file `ghidriff_identities.json`

Create a NEW gitignored file at `rb3-xenon/ghidriff_identities.json`.
It is additive-only, never clobbers existing files, and carries its own
provenance. A new T4b tier in `fn_resolver.py` reads it.

**Proposed schema** (one JSON array, each entry is one ACCEPT-tier pair):

```json
[
  {
    "rb3_addr": "0x82260018",           // Xenon address (0x lowercase)
    "wii_addr_bank8": "0x8000fb10",     // Bank 8 Wii address (RENAMED to avoid collision)
    "wii_symbol": "DrawRegular__3AppFv", // CW-mangled name from Bank 8 map
    "tier": "ACCEPT",
    "match_types": ["SeedMatch"],       // ghidriff correlator(s)
    "tu": "App.o",                      // Wii CW TU from map
    "category": "main",                 // band3/system/network/sdk/main
    "bsim_simconf": 15.6,               // BSim sim×conf (null for non-BSIM)
    "source": "ghidriff-run3"           // provenance tag — increment for re-runs
  },
  ...
]
```

**Key design decisions:**

1. Use `wii_addr_bank8` (NOT `wii_addr`) to make the Bank 8 vs Bank 5
   distinction explicit at the schema level. Any future reader that
   accidentally tries to use this as a Bank 5 address will fail fast on
   the `_bank8` suffix.

2. Keep `rb3_addr` as the join key (Xenon address) — same as every other
   identity file. `fn_resolver._norm_rb3_addr()` already handles `0x`
   lowercase format.

3. Include `bsim_simconf` so consumers can apply the calibrated threshold
   (≥15 → 0.933 precision) independently. Null for non-BSIM entries.

4. The `source` field is a provenance tag. If a Round 4 run produces a
   better set of ACCEPT pairs, entries can be updated in-place or the
   field becomes `"ghidriff-run4"` to distinguish overlapping pairs.

5. **Exclude sdk-category entries** (oracle guidance from Round 1: sdk
   precision = 0.000). The vet tool currently exports them; filter them
   during ingest with `category != "sdk"`.

**What to exclude from the initial ingest:**

- `tier != "ACCEPT"` — CAUTION / FILTERED_VT / REJECT not yet ready.
- `category == "sdk"` — known-bad precision.
- `wii_symbol is null` — 7 entries where the Bank 8 map lookup failed.
- Optionally: `match_types == ["SeedMatch"]` — seeds are already in
  `scripts/target_symbol_map.json` via the T4 eval tool; including them
  is harmless redundancy (they will be skipped by safe_name_merge as
  already-mapped) but inflates the file.

After filtering: approximately **997 non-seed ACCEPT** entries (922 BSIM +
61 ExactInstr + 8 Implied + 5 SwitchSig + 1 SymbolsHash), minus ~12 sdk =
**~985 new entries**.

---

## 5. How fn_resolver.py should wire the new tier

Add a new **tier T4b** between T4 (fuzzy_pairs, 0.70–0.90) and T5
(unified_id / bindiff, up to 0.93). Placement rationale: BSIM ACCEPT at
simconf≥15 has measured 0.933 precision, which is comparable to T5 bindiff.
ExactInstr/Implied/SwitchSig at 0.93–0.96 approach T3 byte-identical.

```python
GHIDRIFF_PATH = _repo_path("ghidriff_identities.json")

def _get_ghidriff_idx() -> dict[int, list[dict]]:
    """addr → list[entry] from ghidriff_identities.json (Bank 8 Wii, ACCEPT tier)."""
    if "ghidriff_idx" not in _cache:
        data = _load_json(GHIDRIFF_PATH)
        idx: dict[int, list[dict]] = defaultdict(list)
        if data:
            for entry in data:
                a = _norm_rb3_addr(entry.get("rb3_addr", ""))
                if a:
                    idx[a].append(entry)
        _cache["ghidriff_idx"] = dict(idx)
    return _cache["ghidriff_idx"]


def _t4b_ghidriff(addr: int) -> list[Identity]:
    """T4b: ghidriff_identities.json — Wii↔Xenon ACCEPT-tier ghidriff pairs."""
    entries = _get_ghidriff_idx().get(addr, [])
    results = []
    for entry in entries:
        wii_sym = entry.get("wii_symbol", "")
        if not wii_sym or wii_sym.startswith("fn_"):
            continue
        match_types = entry.get("match_types", [])
        simconf = entry.get("bsim_simconf")
        # Calibrated confidence: ExactInstr/Implied/SwitchSig ~0.94,
        # BSIM simconf>=15 ~0.93. Cross-arch, so cap below T3 byte-SHA.
        if "ExactInstructionsFunctionHasher" in match_types or "Implied Match" in match_types:
            conf = 0.94
        elif "BSIM" in match_types and simconf and simconf >= 15:
            conf = 0.93
        else:
            conf = 0.88  # BSIM below threshold in the file (shouldn't happen if filtered)
        results.append(Identity(
            mangled=wii_sym,  # CW-mangled; demangled field carries human form
            demangled=wii_sym,  # no MSVC demangler available for CW names
            source="ghidriff_wii_b8",
            confidence=conf,
            extra={
                "match_types": match_types,
                "tu": entry.get("tu"),
                "category": entry.get("category"),
                "bsim_simconf": simconf,
                "source_tag": entry.get("source", "ghidriff-run3"),
                "wii_addr_bank8": entry.get("wii_addr_bank8"),
            },
        ))
    return results
```

Insert `"ghidriff_wii_b8"` into `TIER_ORDER` between `"fuzzy_pairs"` and
`"bindiff_dc3"` (roughly T4.5 — better than fuzzy Jaccard, tied with
bindiff on precision):

```python
TIER_ORDER = ["decomp_db_named", "target_symbol_map", "dc3_content_match",
              "game_content_match", "gameid_crossval", "fuzzy_pairs",
              "ghidriff_wii_b8",   # NEW: T4b
              "bindiff_dc3", "vtable", "rtti", "rtti_low",
              "rb3wii_bindiff", "autoid_fingerprint", "decomp_db_unit"]
```

And add to `resolve_all`:

```python
candidates.extend(_t4_fuzzy_pairs(addr))
candidates.extend(_t4b_ghidriff(addr))   # NEW
candidates.extend(_t5_unified_id(addr))
```

---

## 6. Downstream effects on gen_game_target_map.py

`gen_game_target_map.py` already parses `wii_name` (demangled form) from
`unified_id_rb3wii.json` into `(class, method, arity)` and matches against
the compiled `.obj`'s MSVC symbols. It can accept ghidriff identities
directly once the CW `wii_symbol` field is parsed with the same logic.

However, the `wii_symbol` in `ghidriff_identities.json` is **CW-mangled**
(e.g. `ShowClothes__9ClosetMgrFv`), not the `Ghidra-demangled` form stored
in `unified_id_rb3wii.json`. The `parse_wii_name` function in
`gen_game_target_map.py` (lines 184–230) expects demangled `Class::Method()`
form, not CW mangling.

**Options (choose one):**

A. **Store demangled form alongside CW-mangled** in
   `ghidriff_identities.json`. The `vet_xenon_identities.py` tool already
   has access to the Ghidra-demangled name via the matches.json `p1_name`
   field (which uses Ghidra's `getDefaultLabelText()`, effectively
   demangling for us). Add `wii_symbol_demangled` to the ingest schema.

B. **Write a lightweight CW demangler** for the `Class__N<Name>F<Args>`
   pattern. Many simple cases are parseable with a regex (the `__<N><Name>F`
   pattern for member functions).

C. **Adapt gen_game_target_map.py** to accept ghidriff entries with
   CW-mangled names via a parallel parser (add as `--ghidriff` input).

**Recommendation: Option A** — carry both forms in the file. The matches.json
`p1_name` field already carries the Ghidra-demangled (human-readable) version
of the CW-mangled symbol (e.g. `ShowClothes__9ClosetMgrFv` → `ClosetMgr::ShowClothes`).
The ingest script should join on `p1_addr == wii_addr_bank8.strip("0x")` and
pull `p1_name` as the demangled form.

---

## 7. Ingest script location and gitignore

Place the ingest script at `tools/ghidra/ingest_ghidriff_accepts.py` in the
rb3 repo (not rb3-xenon — it reads from the rb3 build artifacts and writes
into rb3-xenon). Add `ghidriff_identities.json` to rb3-xenon's `.gitignore`
on the `# fingerprint_match.py generated indexes` block at line 54.

The gitignore pattern to add:

```
/ghidriff_identities.json
```

---

## 8. Summary of address formats consumed by each tool

| Tool | Input key field | Expected format | Bank |
|---|---|---|---|
| fn_resolver T6 `_get_rb3wii_idx` | `rb3_addr` | `0x` lowercase | Xenon |
| fn_resolver T6 `_get_rb3wii_idx` | `wii_addr` | `0x` lowercase | **Bank 5** |
| gen_game_target_map `load_oracle` | `rb3_addr` | `0x` lowercase | Xenon |
| gen_game_target_map `build_tu_entries` | `wii_name` | demangled string | n/a |
| safe_name_merge `--gate` | `rb3_addr` | `0x` any | Xenon |
| target_symbol_map keys | n/a | `0x` UPPERCASE | Xenon |
| vetted_identities entries | `xenon_addr` | `0x` lowercase | Xenon |
| vetted_identities entries | `wii_addr` | `0x` lowercase | **Bank 8** |
| matches.json function_matches | `p1_addr` | bare lowercase (no `0x`) | Bank 8 Wii |
| matches.json function_matches | `p2_addr` | bare lowercase (no `0x`) | Xenon |
| **proposed ghidriff_identities** | `rb3_addr` | `0x` lowercase | Xenon |
| **proposed ghidriff_identities** | `wii_addr_bank8` | `0x` lowercase | **Bank 8** |

---

## 9. For the next agent

### What to implement

1. **Write `tools/ghidra/ingest_ghidriff_accepts.py`** in the rb3 repo.
   Input: `build/SZBE69_B8/ghidra/ghidriff-xenon/vetted_identities.json`
   + `build/SZBE69_B8/ghidra/ghidriff-xenon/json/*.matches.json` (for
   `bsim_simconf` and `p1_name` demangled form).
   Output: `rb3-xenon/ghidriff_identities.json`.

   Filters to apply:
   - `tier == "ACCEPT"` only
   - `category != "sdk"` (known 0.000 precision)
   - `wii_symbol is not null` (skip the 7 null-map entries)
   - Optionally omit SeedMatch entries (already in target_symbol_map)

   Schema per entry (final recommendation):
   ```json
   {
     "rb3_addr": "0x82XXXXXX",
     "wii_addr_bank8": "0x80XXXXXX",
     "wii_symbol": "CwMangledName__9ClassFv",
     "wii_symbol_demangled": "Class::Method()",
     "tier": "ACCEPT",
     "match_types": ["BSIM"],
     "tu": "Class.o",
     "category": "band3",
     "bsim_simconf": 15.6,
     "source": "ghidriff-run3"
   }
   ```

2. **Add T4b tier to `fn_resolver.py`** as described in §5 above.

3. **Add `ghidriff_identities.json` to rb3-xenon `.gitignore`** at the
   line-54 block.

4. **Verify the ingest does not clobber** any existing `fn_resolver.py`
   or `target_symbol_map.json` data — the file is pure additive.

### What NOT to do

- Do NOT extend `unified_id_rb3wii.json` — Bank 5 address space.
- Do NOT feed raw CW-mangled names into `gen_game_target_map.py` without
  the demangled form — it will silently fail the `parse_wii_name` step.
- Do NOT use `0x`-UPPERCASE format for `rb3_addr` in the new file —
  fn_resolver's `_norm_rb3_addr` handles lowercase but not the existing
  consumers of target_symbol_map expect uppercase.
- Do NOT include sdk entries.
- Do NOT include CAUTION/FILTERED_VT/REJECT entries in this first wave.

### Key numbers

- Total vetted_identities.json entries: 8,527
- ACCEPT tier: 2,207 (of which 1,210 are SeedMatch / already in tsm)
- Non-seed ACCEPT: 997 (922 BSIM + 61 ExactInstr + 8 Implied + 5 SwitchSig + 1 SymbolsHash)
- band3 non-seed ACCEPT: 309 (280 BSIM + 25 ExactInstr + 3 SwitchSig + 1 Implied)
- Null wii_symbol: 7 (skip)
- sdk category: ~12 in non-seed ACCEPT (skip)
- Final ingest estimate: ~978 new entries

### Key file paths

- Source: `/home/free/code/milohax/rb3/build/SZBE69_B8/ghidra/ghidriff-xenon/vetted_identities.json`
- Source scores: `/home/free/code/milohax/rb3/build/SZBE69_B8/ghidra/ghidriff-xenon/json/bank8_target.elf-42264e.gzf-rb3_xenon_default_xex.gzf.ghidriff.matches.json`
- Target: `/home/free/code/milohax/rb3-xenon/ghidriff_identities.json` (new, gitignored)
- Consumer: `/home/free/code/milohax/rb3-xenon/tools/fn_resolver.py` (add T4b tier)
- gitignore: `/home/free/code/milohax/rb3-xenon/.gitignore` (add `/ghidriff_identities.json`)

### Calibrated confidence values to use in T4b

| match_type | measured precision | recommended conf |
|---|---|---|
| ExactInstructionsFunctionHasher | 0.935–0.958 (run 3 holdout) | 0.94 |
| Implied Match | 0.75–1.0 (small n) | 0.90 |
| SwitchSigHasher | unmeasured but structural | 0.90 |
| BSIM simconf≥15 | 0.933 (holdout, 45 kept) | 0.93 |
| SeedMatch | ~1.0 | 0.97 (but skip — already in tsm) |
| SymbolsHash | 1 match total, skip if sdk | 0.95 |
