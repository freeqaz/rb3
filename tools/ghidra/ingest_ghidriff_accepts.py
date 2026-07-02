#!/usr/bin/env python3
"""ingest_ghidriff_accepts.py — Export vetted ACCEPT identities to rb3-xenon.

Reads run-3 archive artifacts and emits ghidriff_identities.json in the
rb3-xenon repo, gated on the judged BSim-stratum precision.

GATE (--gate argument):
  full         BSim-sample precision >= 0.85 → ingest all non-seed ACCEPT
               (sdk excluded, judged-WRONG excluded)
  conservative BSim-sample precision 0.70–0.85 → only BSim simconf>=20
               plus ExactInstr/SwitchSig/Implied/SymbolsHash
  blocked      precision <0.70 → write blocker doc, no output

Sources (immutable run-3 archive — never reads the live run dir):
  build/SZBE69_B8/ghidra/ghidriff-xenon/run3-archive/vetted_identities.json
  build/SZBE69_B8/ghidra/ghidriff-xenon/run3-archive/json/*.matches.json

Output:
  ../rb3-xenon/ghidriff_identities.json (gitignored, regenerable)

Round-2 judged-WRONG xenon addresses (always excluded regardless of gate):
  0x82518de0  pair-13  BSIM  clear__List_base<pair<Symbol,Symbol>>
  0x824e51e0  pair-16  BSIM  __ct<PCc,i> pair<Symbol,DataNode> float sibling
  0x8233afb0  pair-29  SwitchSig  ActiveScoreType__12MusicLibraryCFv

Usage:
  python3 tools/ghidra/ingest_ghidriff_accepts.py [--gate full|conservative|blocked]
  python3 tools/ghidra/ingest_ghidriff_accepts.py --gate full --dry-run
  python3 tools/ghidra/ingest_ghidriff_accepts.py --gate full --out /tmp/test.json
"""

import argparse
import json
import sys
from collections import Counter, defaultdict
from pathlib import Path

# ---------------------------------------------------------------------------
# Paths
# ---------------------------------------------------------------------------
SCRIPT_DIR = Path(__file__).resolve().parent
RB3_ROOT = SCRIPT_DIR.parent.parent  # rb3 repo root

ARCHIVE_DIR = (
    RB3_ROOT
    / "build/SZBE69_B8/ghidra/ghidriff-xenon/run3-archive"
)
VETTED_IDENTITIES = ARCHIVE_DIR / "vetted_identities.json"
MATCHES_DIR = ARCHIVE_DIR / "json"

RB3_XENON_ROOT = RB3_ROOT.parent / "rb3-xenon"
OUTPUT_PATH = RB3_XENON_ROOT / "ghidriff_identities.json"

# Default BSIM floor (holdout-calibrated 0.933 @ simconf>=15). Overridable
# via --bsim-floor for the loose-band (ws2) regen (10.0 -> ~0.887 calibrated).
DEFAULT_BSIM_FLOOR = 15.0

# ---------------------------------------------------------------------------
# Round-2 judged-WRONG xenon addresses (always excluded)
# ---------------------------------------------------------------------------
JUDGED_WRONG_XENON = frozenset({
    "0x82518de0",   # pair-13 BSIM  clear__List_base
    "0x824e51e0",   # pair-16 BSIM  __ct<PCc,i> pair float-ctor wrong
    "0x8233afb0",   # pair-29 SwitchSig  ActiveScoreType (is BandTrack::SetInstrument)
})

# ---------------------------------------------------------------------------
# Source tag
# ---------------------------------------------------------------------------
SOURCE_TAG = "ghidriff-run3"


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def find_matches_json(matches_dir: Path) -> Path | None:
    """Find the matches.json file in the archive json/ directory."""
    candidates = list(matches_dir.glob("*.matches.json"))
    if not candidates:
        return None
    return candidates[0]


def build_matches_index(matches_path: Path) -> dict:
    """Build addr→entry index from matches.json.

    matches.json uses bare lowercase hex addresses (no 0x prefix).
    We store keyed by stripped bare-hex (lowercase, no 0x) for join.

    Returns:
        { wii_addr_bare: {p1_name, p2_addr_bare, bsim_simconf, ...} }
        and
        { xenon_addr_bare: same }
    Both indexes cover the same entries.
    """
    with open(matches_path) as f:
        data = json.load(f)

    # We need to join on the Wii address (p1) to get p1_name (demangled)
    # and scores.BSIM for simconf. We also need to join on xenon (p2) address.
    wii_idx: dict[str, dict] = {}     # p1_addr (bare hex) → entry
    xenon_idx: dict[str, dict] = {}   # p2_addr (bare hex) → entry

    for entry in data.get("function_matches", []):
        p1 = entry.get("p1_addr", "").lower().strip()
        p2 = entry.get("p2_addr", "").lower().strip()
        scores = entry.get("scores", {})
        bsim = scores.get("BSIM", {})
        simconf = None
        if bsim:
            sim = bsim.get("similarity", 0.0)
            conf = bsim.get("confidence", 0.0)
            simconf = float(sim) * float(conf)

        rec = {
            "p1_addr": p1,
            "p2_addr": p2,
            "p1_name": entry.get("p1_name", ""),
            "p2_name": entry.get("p2_name", ""),
            "match_types": entry.get("match_types", []),
            "bsim_simconf": simconf,
        }
        if p1:
            wii_idx[p1] = rec
        if p2:
            xenon_idx[p2] = rec

    return wii_idx, xenon_idx


def strip_0x(addr: str) -> str:
    """Strip 0x prefix and lowercase."""
    if addr.lower().startswith("0x"):
        return addr[2:].lower()
    return addr.lower()


def add_0x_lower(addr: str) -> str:
    """Ensure 0x-prefixed lowercase format."""
    bare = strip_0x(addr)
    return f"0x{bare}"


def bsim_passes_gate(entry: dict, gate: str, xenon_idx: dict) -> bool:
    """Check whether a BSIM entry meets the current gate threshold."""
    if gate == "full":
        # Accept any BSIM that was in the vetted ACCEPT tier (already >=15)
        return True
    if gate == "conservative":
        # Need simconf >= 20
        xenon_bare = strip_0x(entry.get("xenon_addr", ""))
        match_rec = xenon_idx.get(xenon_bare, {})
        simconf = match_rec.get("bsim_simconf")
        return simconf is not None and simconf >= 20.0
    return False


# ---------------------------------------------------------------------------
# Main ingest logic
# ---------------------------------------------------------------------------

def run_ingest(gate: str, dry_run: bool, out_path: Path, verbose: bool,
               vetted_path: Path | None = None,
               matches_path: Path | None = None,
               bsim_floor: float = DEFAULT_BSIM_FLOOR,
               source_tag: str = SOURCE_TAG,
               only_band: tuple[float, float] | None = None) -> int:
    """Run the ingest; return exit code.

    vetted_path / matches_path override the hardcoded run-3 archive inputs.
    bsim_floor replaces the hard 15.0 Gate-7 floor (ws2 loose band uses 10.0).
    only_band=(LO,HI) keeps only BSIM rows with LO <= simconf < HI (and drops
    all non-BSIM ACCEPT types), so the output is the pure incremental tranche
    that the 0.900-tier ghidriff_identities.json does not already carry.
    """

    vetted_path = vetted_path or VETTED_IDENTITIES

    print(f"Ingest mode: gate={gate!r}  dry_run={dry_run}")
    print(f"Vetted identities: {vetted_path}")
    print(f"BSim floor: {bsim_floor}   source_tag: {source_tag!r}   "
          f"only_band: {only_band}")
    print(f"Output path: {out_path}")
    print()

    if not vetted_path.exists():
        print(f"ERROR: vetted_identities.json not found at {vetted_path}",
              file=sys.stderr)
        return 1

    if matches_path is None:
        matches_path = find_matches_json(MATCHES_DIR)
    if not matches_path or not Path(matches_path).exists():
        print(f"ERROR: No *.matches.json found (looked under {MATCHES_DIR} / "
              f"{matches_path})", file=sys.stderr)
        return 1
    matches_path = Path(matches_path)
    print(f"Matches file: {matches_path}")

    # Load inputs
    with open(vetted_path) as f:
        raw = json.load(f)
    entries = raw["entries"]
    print(f"Total vetted entries: {len(entries)}")

    print("Building matches index…")
    wii_idx, xenon_idx = build_matches_index(matches_path)
    print(f"  Matches: {len(xenon_idx)} (by xenon addr)")

    # -----------------------------------------------------------------------
    # Filter pipeline
    # -----------------------------------------------------------------------
    stats = Counter()
    output = []

    for entry in entries:
        xenon_addr = entry.get("xenon_addr", "")
        wii_addr = entry.get("wii_addr", "")
        wii_symbol = entry.get("wii_symbol")
        tier = entry.get("tier", "")
        match_types = entry.get("match_types", [])
        category = entry.get("category")
        tu = entry.get("tu")

        # --- Gate 1: ACCEPT only ---
        if tier != "ACCEPT":
            stats["skip_tier"] += 1
            continue

        # --- Gate 2: exclude SeedMatch-only (already in target_symbol_map) ---
        if match_types == ["SeedMatch"]:
            stats["skip_seed"] += 1
            continue

        # --- Gate 3: exclude sdk (oracle: precision=0.000) ---
        if category == "sdk":
            stats["skip_sdk"] += 1
            continue

        # --- Gate 4: exclude null wii_symbol ---
        if not wii_symbol:
            stats["skip_null_sym"] += 1
            continue

        # --- Gate 5: exclude judged-WRONG addresses ---
        xenon_norm = add_0x_lower(xenon_addr)
        if xenon_norm in JUDGED_WRONG_XENON:
            stats["skip_judged_wrong"] += 1
            if verbose:
                print(f"  Excluding judged-WRONG: {xenon_addr} {wii_symbol}")
            continue

        # --- Gate 6: BSim gate (full vs conservative) ---
        xenon_bare = strip_0x(xenon_addr)
        match_rec = xenon_idx.get(xenon_bare, {})
        bsim_simconf = match_rec.get("bsim_simconf")

        is_bsim = "BSIM" in match_types
        is_exact = "ExactInstructionsFunctionHasher" in match_types
        is_switch = "SwitchSigHasher" in match_types
        is_implied = "Implied Match" in match_types
        is_symbols = "SymbolsHash" in match_types

        if gate == "conservative":
            if is_bsim:
                if not bsim_passes_gate(entry, gate, xenon_idx):
                    stats["skip_conservative_bsim_low"] += 1
                    continue
            # ExactInstr/Switch/Implied/SymbolsHash pass conservative gate unconditionally

        # --- Gate 7: BSIM entries must have simconf >= bsim_floor (should
        #     already be true from vet tool, but assert as a hard check) ---
        if is_bsim and bsim_simconf is not None and bsim_simconf < bsim_floor:
            stats["skip_bsim_below_floor"] += 1
            if verbose:
                print(f"  WARNING: BSIM simconf={bsim_simconf:.2f} < {bsim_floor} "
                      f"for {xenon_addr}")
            continue

        # --- Gate 8 (--only-band): keep ONLY the incremental BSIM tranche
        #     LO <= simconf < HI; drop non-BSIM (already emitted at strict
        #     tier) and out-of-band BSIM. Makes the loose file purely additive. ---
        if only_band is not None:
            lo, hi = only_band
            if not is_bsim:
                stats["skip_only_band_nonbsim"] += 1
                continue
            if bsim_simconf is None or not (lo <= bsim_simconf < hi):
                stats["skip_only_band_out"] += 1
                continue

        # --- Collect p1_name (demangled) from matches.json ---
        wii_addr_bare = strip_0x(wii_addr)
        # Join on xenon address (p2) since vetted_identities maps xenon→wii
        p1_name = match_rec.get("p1_name", "")
        # If not found via xenon addr, try via wii addr (p1)
        if not p1_name:
            wii_rec = wii_idx.get(wii_addr_bare, {})
            p1_name = wii_rec.get("p1_name", "")

        # wii_symbol_demangled: prefer matches.json p1_name (Ghidra demangled)
        # Fall back to the CW-mangled form if not available
        wii_symbol_demangled = p1_name if p1_name else wii_symbol

        # Build output record
        record = {
            "rb3_addr": add_0x_lower(xenon_addr),           # Xenon addr
            "wii_addr_bank8": add_0x_lower(wii_addr),       # Bank 8 Wii addr
            "wii_symbol": wii_symbol,                        # CW-mangled
            "wii_symbol_demangled": wii_symbol_demangled,   # Ghidra demangled
            "tier": tier,
            "match_types": match_types,
            "tu": tu,
            "category": category,
            "bsim_simconf": round(bsim_simconf, 4) if bsim_simconf is not None else None,
            "rb3wii_check": entry.get("rb3wii_check"),       # absent/confirmed/contradicted
            "source": source_tag,
        }

        output.append(record)
        stats[f"kept_{category}"] += 1
        for mt in match_types:
            stats[f"mt_{mt}"] += 1

    # -----------------------------------------------------------------------
    # Summary
    # -----------------------------------------------------------------------
    print("\n--- Ingest summary ---")
    print(f"  Gate: {gate}")
    print(f"  Skipped (non-ACCEPT tier):    {stats['skip_tier']}")
    print(f"  Skipped (SeedMatch-only):     {stats['skip_seed']}")
    print(f"  Skipped (sdk category):       {stats['skip_sdk']}")
    print(f"  Skipped (null wii_symbol):    {stats['skip_null_sym']}")
    print(f"  Skipped (judged-WRONG):       {stats['skip_judged_wrong']}")
    if gate == "conservative":
        print(f"  Skipped (BSIM simconf<20):   {stats['skip_conservative_bsim_low']}")
    print(f"  Skipped (BSIM simconf<floor): {stats['skip_bsim_below_floor']}")
    if only_band is not None:
        print(f"  Skipped (only-band non-BSIM): {stats['skip_only_band_nonbsim']}")
        print(f"  Skipped (only-band out):      {stats['skip_only_band_out']}")
    print()
    print(f"  Ingested: {len(output)}")
    for key in sorted(stats):
        if key.startswith("kept_"):
            cat = key[5:]
            print(f"    category {cat:12s}: {stats[key]}")
    print()
    print("  By match_type:")
    for key in sorted(stats):
        if key.startswith("mt_"):
            mt = key[3:]
            print(f"    {mt:40s}: {stats[key]}")

    # -----------------------------------------------------------------------
    # Assertions
    # -----------------------------------------------------------------------
    sdk_count = sum(1 for r in output if r.get("category") == "sdk")
    assert sdk_count == 0, f"BUG: {sdk_count} sdk entries in output"

    seed_count = sum(1 for r in output if r.get("match_types") == ["SeedMatch"])
    assert seed_count == 0, f"BUG: {seed_count} SeedMatch-only entries in output"

    wrong_count = sum(1 for r in output if r.get("rb3_addr") in JUDGED_WRONG_XENON)
    assert wrong_count == 0, f"BUG: {wrong_count} judged-WRONG entries in output"

    bsim_below_floor = sum(
        1 for r in output
        if "BSIM" in r.get("match_types", [])
        and r.get("bsim_simconf") is not None
        and r["bsim_simconf"] < bsim_floor
    )
    assert bsim_below_floor == 0, (
        f"BUG: {bsim_below_floor} BSIM entries with simconf<{bsim_floor}")

    if only_band is not None:
        lo, hi = only_band
        out_of_band = sum(
            1 for r in output
            if "BSIM" not in r.get("match_types", [])
            or r.get("bsim_simconf") is None
            or not (lo <= r["bsim_simconf"] < hi)
        )
        assert out_of_band == 0, (
            f"BUG: {out_of_band} rows outside --only-band {lo},{hi}")

    null_sym_count = sum(1 for r in output if not r.get("wii_symbol"))
    assert null_sym_count == 0, f"BUG: {null_sym_count} null wii_symbol in output"

    print(f"  Assertions: all passed (sdk=0, seed=0, wrong=0, bsim<{bsim_floor}=0, "
          "null_sym=0)")

    if dry_run:
        print(f"\n[dry-run] Would write {len(output)} entries to {out_path}")
        return 0

    # -----------------------------------------------------------------------
    # Write output
    # -----------------------------------------------------------------------
    out_path.parent.mkdir(parents=True, exist_ok=True)
    with open(out_path, "w") as f:
        json.dump(output, f, indent=1)
    print(f"\nWrote {len(output)} entries → {out_path}")
    return 0


# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------

def main():
    parser = argparse.ArgumentParser(
        description="Export ghidriff run-3 ACCEPT identities to rb3-xenon.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__,
    )
    parser.add_argument(
        "--gate", choices=["full", "conservative", "blocked"],
        default="full",
        help="Ingest gate tier (default: full). Computed from judged BSim precision.",
    )
    parser.add_argument(
        "--dry-run", action="store_true",
        help="Print summary without writing output file.",
    )
    parser.add_argument(
        "--out", default=None,
        help=f"Output path (default: {OUTPUT_PATH})",
    )
    parser.add_argument(
        "--vetted", default=None,
        help="Override vetted_identities.json path (default: run-3 archive).",
    )
    parser.add_argument(
        "--matches", default=None,
        help="Override *.matches.json path (default: run-3 archive json/).",
    )
    parser.add_argument(
        "--bsim-floor", type=float, default=DEFAULT_BSIM_FLOOR,
        help=f"Hard BSIM simconf floor (default {DEFAULT_BSIM_FLOOR}). "
             "ws2 loose band uses 10.",
    )
    parser.add_argument(
        "--source-tag", default=SOURCE_TAG,
        help=f"Provenance tag stamped on each row (default {SOURCE_TAG!r}).",
    )
    parser.add_argument(
        "--only-band", default=None,
        help="LO,HI — keep only BSIM rows with LO<=simconf<HI (drops non-BSIM "
             "and out-of-band). Makes output the pure incremental tranche.",
    )
    parser.add_argument(
        "--verbose", "-v", action="store_true",
        help="Print per-entry exclusion messages.",
    )
    args = parser.parse_args()

    if args.gate == "blocked":
        print("Gate is BLOCKED — no ingest. Write a blocker doc with precision numbers.")
        print("BSim-stratum precision was below 0.70.")
        return

    only_band = None
    if args.only_band:
        parts = args.only_band.split(",")
        if len(parts) != 2:
            parser.error("--only-band must be LO,HI")
        only_band = (float(parts[0]), float(parts[1]))

    out_path = Path(args.out) if args.out else OUTPUT_PATH
    rc = run_ingest(
        gate=args.gate,
        dry_run=args.dry_run,
        out_path=out_path,
        verbose=args.verbose,
        vetted_path=Path(args.vetted) if args.vetted else None,
        matches_path=Path(args.matches) if args.matches else None,
        bsim_floor=args.bsim_floor,
        source_tag=args.source_tag,
        only_band=only_band,
    )
    sys.exit(rc)


if __name__ == "__main__":
    main()
