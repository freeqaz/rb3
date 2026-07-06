#!/usr/bin/env python3
"""native_compat_census.py — census/gen/check tool for the NativeCompat flag registry.

CONTEXT (see docs/native/engine-arch-review-2026-07-05/06-arch-crosscut.md §3 and
docs/native/engine-arch-review-2026-07-05/execution/W0.6/PLAN.md — this tool implements
subtask W0.6.S1). RB3's native port scattered ~229 `getenv()` reads across the shared
engine (`milo-native-engine/src`) and the rb3 glue (`rb3/native/src`) — probes, shipped
default-ON workarounds, and value knobs, with NO uniform read semantics (some sites treat
ANY env value as a trigger, "presence" mode; others require a non-empty non-"0" value,
"truthy" mode). This tool is the read-only front half of collapsing that into one typed
`NativeCompat` registry (module itself lands in W0.6.S2):

  scan       walk all source trees (SCAN_ROOTS) for literal `getenv("NAME")` call sites, guess each
             flag's read-mode (presence / truthy / value / unknown) from local context,
             and emit a deterministic JSON inventory (census.json).
  gen        join the scan against a hand-curated classification sidecar
             (NativeCompatFlags.classification.json — class/owner/faithfulStatus) and
             emit (a) a C brace-init table (NativeCompatFlags.gen.inc, consumed by S2's
             NativeCompatFlags.cpp) and (b) a burn-down ledger Markdown doc (S3). Flags
             present in the scan but absent from the sidecar get `class=unknown`
             (a NEEDS-CLASSIFICATION marker) — gen never invents flags.
  check      re-scan and diff against the *committed* .gen.inc's flag-name set (grep, not
             compile) — exits nonzero if any live `getenv` is not yet in the registry, or
             if `gen` would produce different committed-file content (regen-not-clean).
             This is the CI-shaped gate: green ⇒ registry ⊇ every getenv in the tree AND
             is up to date with the sidecar.
  --selftest hermetic fixtures (temp dirs, no repo/dependency on the real trees) proving
             the scan/gen/check logic itself is correct. Run this first.

Usage:
    scripts/analysis/native_compat_census.py --selftest
    scripts/analysis/native_compat_census.py scan
    scripts/analysis/native_compat_census.py scan --json /tmp/census.json
    scripts/analysis/native_compat_census.py gen  [--gen-inc-out PATH] [--ledger-out PATH]
    scripts/analysis/native_compat_census.py check
"""

import argparse
import json
import re
import sys
from pathlib import Path

# ─────────────────────────────────────────────────────────────────────────────
# Paths
# ─────────────────────────────────────────────────────────────────────────────

RB3_ROOT = Path(__file__).resolve().parents[2]           # .../milohax/rb3
MILOHAX_ROOT = RB3_ROOT.parent                            # .../milohax
ENGINE_ROOT = MILOHAX_ROOT / "milo-native-engine"

SCAN_ROOTS = [
    ("engine", ENGINE_ROOT / "src"),
    ("glue", RB3_ROOT / "native" / "src"),
    # "game" = the rb3 decomp game-code tree (BandCharacter, Crowd, etc.). Added by
    # W2.6.S4 to close the coverage gap flagged by W2.2/W2.6: ~90 native `getenv`
    # flags live here and were previously invisible to the census (it only scanned
    # the shared engine + the native/src glue layer).
    ("game", RB3_ROOT / "src" / "system"),
]

CENSUS_JSON_DEFAULT = (
    RB3_ROOT / "docs" / "native" / "engine-arch-review-2026-07-05"
    / "execution" / "W0.6" / "census.json"
)
SIDECAR_DEFAULT = ENGINE_ROOT / "src" / "platform" / "NativeCompatFlags.classification.json"
GEN_INC_DEFAULT = ENGINE_ROOT / "src" / "platform" / "NativeCompatFlags.gen.inc"
LEDGER_DEFAULT = (
    RB3_ROOT / "docs" / "native" / "engine-arch-review-2026-07-05"
    / "NATIVE_COMPAT_LEDGER.md"
)

SCAN_EXTS = (".cpp", ".h", ".mm")

# ─────────────────────────────────────────────────────────────────────────────
# scan
# ─────────────────────────────────────────────────────────────────────────────

GETENV_RE = re.compile(r'(?:std::)?getenv\(\s*"([A-Za-z0-9_]+)"\s*\)')

# Registry-ROUTED reads: once a raw `getenv("X")` site is migrated to the
# NativeCompat registry (W0.6.S2+), the literal getenv disappears but the flag
# is still referenced by name via the read-once accessors. Without counting
# these, migrating a site would DELETE its registry row (regen non-idempotent)
# and — worse — drop the flag from the runtime table, silently breaking the
# set-env behaviour it was rewired to preserve. So a routed reference keeps the
# flag in the census. Only the accessor names unique to NativeCompat are matched
# (OptOutActive / ProbeActive) to avoid capturing unrelated `Find("…")` calls;
# routed sites carry no read-mode context (that lives in the sidecar).
REGISTRY_REF_RE = re.compile(r'\b(?:OptOutActive|ProbeActive)\(\s*"([A-Za-z0-9_]+)"\s*\)')

# Read-mode heuristics, checked in this priority order against a small text
# window around each call site (its own line + a few lines of trailing
# context, which covers the codebase's two dominant multi-line idioms — see
# PLAN.md §Key-facts-4). This is a best-effort GUESS: `gen`/`check` never rely
# on it for correctness (only names matter there); it exists to pre-populate
# the ledger and flag likely mismatches for a human to confirm during S2.
_TRUTHY_RE = re.compile(r"\[0\][^\n]*!=\s*'0'|!=\s*'0'[^\n]*\[0\]")
_VALUE_RE = re.compile(r"\b(atoi|atof|atol|strtol|strtod|strtof)\s*\(")
_PRESENCE_RE = re.compile(r"!=\s*nullptr|!=\s*NULL|==\s*nullptr|==\s*NULL")

WINDOW_LINES_AFTER = 3


def _guess_read_mode(window: str) -> str:
    if _TRUTHY_RE.search(window):
        return "truthy"
    if _VALUE_RE.search(window):
        return "value"
    if _PRESENCE_RE.search(window):
        return "presence"
    # Bare boolean use with no captured pointer at all, e.g. `if (getenv("X"))`,
    # `!getenv("X")`, `getenv("X") ?` — common presence idiom with no explicit
    # nullptr comparison.
    if re.search(r'(?:if\s*\(\s*!?|!\s*|\(\s*!\s*)(?:std::)?getenv\(', window):
        return "presence"
    return "unknown"


def _guess_default(name: str) -> str:
    """Best-effort human-readable default, purely from naming convention."""
    if name.endswith("_OFF"):
        return "on"  # opt-out idiom: shipped ON, env var disables it
    if any(tok in name for tok in ("_PROBE", "_DBG", "_TRACE", "_TRACK", "DEBUG_")):
        return "off"  # opt-in debug/probe idiom
    return "unknown"


def _iter_source_files(root: Path):
    if not root.is_dir():
        return
    for path in sorted(root.rglob("*")):
        if path.is_file() and path.suffix in SCAN_EXTS:
            yield path


def scan_tree(root_label: str, root: Path):
    """Yield (name, relpath, lineno, window_text, kind) for every flag reference
    under `root`, where kind is "getenv" (raw env read — contributes a read-mode
    guess) or "routed" (a NativeCompat registry accessor — keeps the flag in the
    census after its getenv site is migrated, but contributes no read-mode).
    `relpath` is relative to MILOHAX_ROOT for readability across both repos."""
    for path in _iter_source_files(root):
        try:
            text = path.read_text(encoding="utf-8", errors="replace")
        except OSError:
            continue
        lines = text.split("\n")
        try:
            relpath = str(path.relative_to(MILOHAX_ROOT))
        except ValueError:
            relpath = str(path)
        for i, line in enumerate(lines):
            for m in GETENV_RE.finditer(line):
                name = m.group(1)
                window = "\n".join(lines[i:i + 1 + WINDOW_LINES_AFTER])
                yield name, relpath, i + 1, window, "getenv"
            for m in REGISTRY_REF_RE.finditer(line):
                yield m.group(1), relpath, i + 1, "", "routed"


def run_scan(roots=None):
    """Returns the deterministic scan result dict (see module docstring)."""
    roots = roots or SCAN_ROOTS
    flags = {}  # name -> {sites: [...], read_modes: Counter-ish list}
    root_labels = {}  # name -> set of root labels it appears under
    for label, root in roots:
        for name, relpath, lineno, window, kind in scan_tree(label, root):
            entry = flags.setdefault(name, {"sites": [], "read_modes": []})
            entry["sites"].append(f"{relpath}:{lineno}")
            # Routed (registry-accessor) sites carry no surrounding read-mode
            # context — the flag's read mode is authoritative in the sidecar/
            # committed table, not re-guessed from the migrated call site.
            if kind == "getenv":
                entry["read_modes"].append(_guess_read_mode(window))
            root_labels.setdefault(name, set()).add(label)

    _MODE_PRIORITY = {"presence": 0, "truthy": 1, "value": 2, "unknown": 3}

    result_flags = []
    for name in sorted(flags):
        entry = flags[name]
        sites = sorted(set(entry["sites"]))
        modes = entry["read_modes"]
        # Majority vote, tie-broken deterministically by _MODE_PRIORITY.
        counts = {}
        for m in modes:
            counts[m] = counts.get(m, 0) + 1
        # A routed-only flag (all sites migrated to the registry) has no read-mode
        # votes; its authoritative mode comes from the sidecar in `gen`.
        if counts:
            best = sorted(counts.items(), key=lambda kv: (-kv[1], _MODE_PRIORITY[kv[0]]))[0][0]
        else:
            best = "unknown"
        result_flags.append({
            "name": name,
            "sites": len(entry["sites"]),
            "files": sites,
            "readModeGuess": best,
            "defaultGuess": _guess_default(name),
            "roots": sorted(root_labels[name]),
        })

    engine_only = sum(1 for f in result_flags if f["roots"] == ["engine"])
    glue_only = sum(1 for f in result_flags if f["roots"] == ["glue"])
    game_only = sum(1 for f in result_flags if f["roots"] == ["game"])
    shared = sum(1 for f in result_flags if len(f["roots"]) > 1)

    return {
        "flags": result_flags,
        "summary": {
            "totalFlags": len(result_flags),
            "totalSites": sum(f["sites"] for f in result_flags),
            "engineOnly": engine_only,
            "glueOnly": glue_only,
            "gameOnly": game_only,
            "sharedAcrossRoots": shared,
        },
    }


def cmd_scan(args):
    result = run_scan()
    out_path = Path(args.json) if args.json else CENSUS_JSON_DEFAULT
    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_text(json.dumps(result, indent=2, sort_keys=False) + "\n")
    s = result["summary"]
    print(f"scan: {s['totalFlags']} distinct flags, {s['totalSites']} call sites "
          f"({s['engineOnly']} engine-only, {s['glueOnly']} glue-only, "
          f"{s['gameOnly']} game-only, {s['sharedAcrossRoots']} shared) -> {out_path}")
    return 0


# ─────────────────────────────────────────────────────────────────────────────
# sidecar (curated classification)
# ─────────────────────────────────────────────────────────────────────────────

def load_sidecar(path: Path) -> dict:
    if not path.is_file():
        return {}
    return json.loads(path.read_text())


# ─────────────────────────────────────────────────────────────────────────────
# gen — join scan ∪ sidecar -> .gen.inc (C rows) + ledger Markdown
# ─────────────────────────────────────────────────────────────────────────────

_FLAG_CLASS_ENUM = {
    "probe": "FlagClass::Probe",
    "workaround": "FlagClass::Workaround",
    "feature": "FlagClass::Feature",
    "perf": "FlagClass::Perf",
    "unknown": "FlagClass::Unknown",
}
_FLAG_READ_ENUM = {
    "presence": "FlagRead::Presence",
    "truthy": "FlagRead::Truthy",
    "value": "FlagRead::Value",
    "unknown": "FlagRead::Truthy",  # safest fallback: never accidentally widen triggering
}


def _c_escape(s: str) -> str:
    return s.replace("\\", "\\\\").replace('"', '\\"')


def join_scan_and_sidecar(scan_result: dict, sidecar: dict):
    """Returns a list of row dicts: name, cls, default, read, owner, status."""
    rows = []
    for f in scan_result["flags"]:
        name = f["name"]
        curated = sidecar.get(name, {})
        cls = curated.get("class", "unknown")
        owner = curated.get("owner", "unclassified")
        status = curated.get("faithfulStatus", "n/a")
        default = curated.get("default", f["defaultGuess"])
        # The sidecar may pin a flag's read mode (REQUIRED for flags whose getenv
        # sites have all been migrated to the registry, since the scan can no
        # longer guess it from context — see REGISTRY_REF_RE). Otherwise the
        # scan's contextual guess stands.
        read = curated.get("read", f["readModeGuess"])
        rows.append({
            "name": name,
            "class": cls,
            "default": default,
            "read": read,
            "owner": owner,
            "status": status,
            "sites": f["sites"],
        })
    return rows


def render_gen_inc(rows) -> str:
    lines = [
        "// GENERATED by scripts/analysis/native_compat_census.py — do not edit by hand.",
        "// Regenerate: python3 scripts/analysis/native_compat_census.py gen",
        "// Row shape: { name, default, class, read, owner, faithfulStatus, docAnchor },",
        "",
    ]
    for r in rows:
        cls = _FLAG_CLASS_ENUM.get(r["class"], _FLAG_CLASS_ENUM["unknown"])
        read = _FLAG_READ_ENUM.get(r["read"], _FLAG_READ_ENUM["unknown"])
        lines.append(
            '{{ "{name}", "{default}", {cls}, {read}, "{owner}", "{status}", "{name}" }},'.format(
                name=_c_escape(r["name"]),
                default=_c_escape(r["default"]),
                cls=cls,
                read=read,
                owner=_c_escape(r["owner"]),
                status=_c_escape(r["status"]),
            )
        )
    return "\n".join(lines) + "\n"


def render_ledger_md(rows) -> str:
    by_class = {}
    for r in rows:
        by_class.setdefault(r["class"], 0)
        by_class[r["class"]] += 1
    default_on_workarounds = sum(
        1 for r in rows if r["class"] == "workaround" and r["default"] == "on"
    )
    lines = [
        "<!-- GENERATED by scripts/analysis/native_compat_census.py — do not edit by hand. -->",
        "<!-- Regenerate: python3 scripts/analysis/native_compat_census.py gen -->",
        "",
        "# Native Compat Flag Ledger",
        "",
        "One row per `getenv()`-backed native-compat flag found under `milo-native-engine/src`"
        " + `rb3/native/src`. See `docs/native/engine-arch-review-2026-07-05/06-arch-crosscut.md`"
        " §3 and `execution/W0.6/PLAN.md` for the design this is generated from.",
        "",
        f"**Total flags:** {len(rows)}  ",
        "**By class:** " + ", ".join(f"{k}={v}" for k, v in sorted(by_class.items())) + "  ",
        f"**Default-ON workarounds (the number §W5.3 must drive to 0):** {default_on_workarounds}",
        "",
        "| name | class | default | owner | faithful-status | sites |",
        "|---|---|---|---|---|---|",
    ]
    for r in sorted(rows, key=lambda r: r["name"]):
        lines.append(
            f"| `{r['name']}` | {r['class']} | {r['default']} | {r['owner']} "
            f"| {r['status']} | {r['sites']} |"
        )
    return "\n".join(lines) + "\n"


def cmd_gen(args):
    sidecar_path = Path(args.sidecar) if args.sidecar else SIDECAR_DEFAULT
    gen_inc_path = Path(args.gen_inc_out) if args.gen_inc_out else GEN_INC_DEFAULT
    ledger_path = Path(args.ledger_out) if args.ledger_out else LEDGER_DEFAULT

    scan_result = run_scan()
    sidecar = load_sidecar(sidecar_path)
    rows = join_scan_and_sidecar(scan_result, sidecar)

    gen_inc_path.parent.mkdir(parents=True, exist_ok=True)
    ledger_path.parent.mkdir(parents=True, exist_ok=True)
    gen_inc_path.write_text(render_gen_inc(rows))
    ledger_path.write_text(render_ledger_md(rows))

    unknown_count = sum(1 for r in rows if r["class"] == "unknown")
    print(f"gen: {len(rows)} rows ({unknown_count} unclassified) -> {gen_inc_path}")
    print(f"gen: ledger -> {ledger_path}")
    return 0


# ─────────────────────────────────────────────────────────────────────────────
# check — registry coverage + regen-clean gate
# ─────────────────────────────────────────────────────────────────────────────

_GEN_ROW_NAME_RE = re.compile(r'^\{\s*"([A-Za-z0-9_]+)"')


def names_in_gen_inc(gen_inc_path: Path):
    if not gen_inc_path.is_file():
        return set()
    names = set()
    for line in gen_inc_path.read_text().splitlines():
        m = _GEN_ROW_NAME_RE.match(line.strip())
        if m:
            names.add(m.group(1))
    return names


def cmd_check(args):
    sidecar_path = Path(args.sidecar) if args.sidecar else SIDECAR_DEFAULT
    gen_inc_path = Path(args.gen_inc_out) if args.gen_inc_out else GEN_INC_DEFAULT
    ledger_path = Path(args.ledger_out) if args.ledger_out else LEDGER_DEFAULT

    scan_result = run_scan()
    scanned_names = {f["name"] for f in scan_result["flags"]}
    registered_names = names_in_gen_inc(gen_inc_path)

    missing = sorted(scanned_names - registered_names)
    ok = True

    if not gen_inc_path.is_file():
        print(f"check: FAIL — registry file not found: {gen_inc_path}")
        ok = False
    elif missing:
        print(f"check: FAIL — {len(missing)} getenv flag(s) not in registry ({gen_inc_path}):")
        for name in missing:
            print(f"  - {name}")
        ok = False

    # Regen-clean gate: what `gen` would produce right now must match the
    # committed files byte-for-byte.
    sidecar = load_sidecar(sidecar_path)
    rows = join_scan_and_sidecar(scan_result, sidecar)
    fresh_gen_inc = render_gen_inc(rows)
    fresh_ledger = render_ledger_md(rows)

    if gen_inc_path.is_file() and gen_inc_path.read_text() != fresh_gen_inc:
        print(f"check: FAIL — {gen_inc_path} is stale (regen would differ). Run `gen`.")
        ok = False
    if ledger_path.is_file() and ledger_path.read_text() != fresh_ledger:
        print(f"check: FAIL — {ledger_path} is stale (regen would differ). Run `gen`.")
        ok = False

    if ok:
        print(f"check: OK — {len(scanned_names)} scanned flags all present in registry, "
              f"regen clean.")
        return 0
    return 1


# ─────────────────────────────────────────────────────────────────────────────
# --selftest — hermetic, no dependency on the real repo trees
# ─────────────────────────────────────────────────────────────────────────────

def _write(path: Path, content: str):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(content)


def selftest() -> int:
    import tempfile

    results = []  # (name, ok)

    def check(name, ok):
        results.append((name, ok))
        print(f"  [{'PASS' if ok else 'FAIL'}] {name}")

    with tempfile.TemporaryDirectory(prefix="native_compat_census_selftest_") as td:
        td = Path(td)
        engine_src = td / "milo-native-engine" / "src"
        glue_src = td / "rb3" / "native" / "src"

        # --- Fixture 1: presence-mode flag (engine root) ---
        _write(engine_src / "platform" / "Fixture_Presence.cpp", """
static bool Enabled() {
    static int s = -1;
    if (s < 0) s = (getenv("FIXTURE_PRESENCE_OFF") != nullptr) ? 0 : 1;
    return s != 0;
}
""")

        # --- Fixture 2: truthy-mode flag (glue root) ---
        _write(glue_src / "fixture_truthy.cpp", """
void Init() {
    const char *e = ::getenv("FIXTURE_TRUTHY_OFF");
    if (e && e[0] && e[0] != '0')
        gDisabled = true;
}
""")

        # --- Fixture 3: value-mode flag ---
        _write(engine_src / "platform" / "Fixture_Value.cpp", """
int Period() {
    if (const char *p = getenv("FIXTURE_PERIOD_MS")) {
        int v = atoi(p);
        return v;
    }
    return 240;
}
""")

        roots = [("engine", engine_src), ("glue", glue_src)]
        scan_result = run_scan(roots)
        by_name = {f["name"]: f for f in scan_result["flags"]}

        check("scan finds FIXTURE_PRESENCE_OFF", "FIXTURE_PRESENCE_OFF" in by_name)
        check("scan classifies FIXTURE_PRESENCE_OFF as presence",
              by_name.get("FIXTURE_PRESENCE_OFF", {}).get("readModeGuess") == "presence")
        check("scan finds FIXTURE_TRUTHY_OFF", "FIXTURE_TRUTHY_OFF" in by_name)
        check("scan classifies FIXTURE_TRUTHY_OFF as truthy",
              by_name.get("FIXTURE_TRUTHY_OFF", {}).get("readModeGuess") == "truthy")
        check("scan classifies FIXTURE_PERIOD_MS as value",
              by_name.get("FIXTURE_PERIOD_MS", {}).get("readModeGuess") == "value")
        check("scan totalFlags == 3", scan_result["summary"]["totalFlags"] == 3)

        # --- gen: sidecar classifies one flag, leaves the rest Unknown ---
        sidecar = {
            "FIXTURE_PRESENCE_OFF": {
                "class": "workaround", "owner": "test", "faithfulStatus": "n/a",
                "default": "on",
            }
        }
        rows = join_scan_and_sidecar(scan_result, sidecar)
        rows_by_name = {r["name"]: r for r in rows}
        check("gen: sidecar-classified flag keeps class=workaround",
              rows_by_name["FIXTURE_PRESENCE_OFF"]["class"] == "workaround")
        check("gen: unclassified flag falls back to class=unknown",
              rows_by_name["FIXTURE_TRUTHY_OFF"]["class"] == "unknown")

        gen_inc_text = render_gen_inc(rows)
        check("gen: emits one brace row per flag",
              gen_inc_text.count("{") - gen_inc_text.count("// Row shape") >= len(rows))
        check("gen: emits FlagClass::Workaround for classified flag",
              'FlagClass::Workaround' in gen_inc_text and 'FIXTURE_PRESENCE_OFF' in gen_inc_text)
        check("gen: emits FlagClass::Unknown for unclassified flags",
              gen_inc_text.count("FlagClass::Unknown") == 2)

        ledger_text = render_ledger_md(rows)
        check("ledger: has one table row per flag",
              sum(1 for line in ledger_text.splitlines() if line.startswith("| `FIXTURE"))
              == len(rows))

        # --- check: green when registry == scan, gen output matches committed files ---
        gen_inc_path = td / "NativeCompatFlags.gen.inc"
        ledger_path = td / "LEDGER.md"
        gen_inc_path.write_text(gen_inc_text)
        ledger_path.write_text(ledger_text)

        class Args:
            pass
        args = Args()
        args.sidecar = None
        args.gen_inc_out = str(gen_inc_path)
        args.ledger_out = str(ledger_path)

        # monkeypatch run_scan for this call via closures below instead of args
        orig_run_scan = globals()["run_scan"]
        orig_load_sidecar = globals()["load_sidecar"]

        def fake_run_scan(_roots=None):
            return orig_run_scan(roots)

        def fake_load_sidecar(path):
            return sidecar if str(path) == str(SIDECAR_DEFAULT) or True else {}

        globals()["run_scan"] = fake_run_scan
        globals()["load_sidecar"] = fake_load_sidecar
        try:
            rc_green = cmd_check(args)
            check("check: exits 0 when registry covers scan and regen is clean",
                  rc_green == 0)

            # --- Fail-red: inject an unregistered getenv, check must go nonzero ---
            _write(glue_src / "fixture_unregistered.cpp", """
void Rogue() {
    if (getenv("FIXTURE_UNREGISTERED_DEMO"))
        DoRogueThing();
}
""")
            rc_red = cmd_check(args)
            check("check: exits nonzero when an unregistered getenv is present",
                  rc_red != 0)
        finally:
            globals()["run_scan"] = orig_run_scan
            globals()["load_sidecar"] = orig_load_sidecar

    n_pass = sum(1 for _, ok in results if ok)
    n_total = len(results)
    print(f"\nselftest: {n_pass}/{n_total} " + ("PASS" if n_pass == n_total else "FAIL"))
    return 0 if n_pass == n_total else 1


# ─────────────────────────────────────────────────────────────────────────────
# main
# ─────────────────────────────────────────────────────────────────────────────

def main() -> int:
    ap = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="cmd")

    p_scan = sub.add_parser("scan", help="scan both source trees, emit census.json")
    p_scan.add_argument("--json", help="override output path for census.json")
    p_scan.set_defaults(func=cmd_scan)

    p_gen = sub.add_parser("gen", help="join scan + sidecar, emit .gen.inc + ledger.md")
    p_gen.add_argument("--sidecar", help="override sidecar JSON path")
    p_gen.add_argument("--gen-inc-out", help="override NativeCompatFlags.gen.inc output path")
    p_gen.add_argument("--ledger-out", help="override NATIVE_COMPAT_LEDGER.md output path")
    p_gen.set_defaults(func=cmd_gen)

    p_check = sub.add_parser("check", help="scan vs committed registry; exit nonzero on gap")
    p_check.add_argument("--sidecar", help="override sidecar JSON path")
    p_check.add_argument("--gen-inc-out", help="override NativeCompatFlags.gen.inc path to check against")
    p_check.add_argument("--ledger-out", help="override NATIVE_COMPAT_LEDGER.md path to check against")
    p_check.set_defaults(func=cmd_check)

    ap.add_argument("--selftest", action="store_true",
                     help="run the hermetic selftest (no real repo dependency) and exit")

    args = ap.parse_args()

    if args.selftest:
        return selftest()

    if not getattr(args, "cmd", None):
        ap.print_help()
        return 2

    return args.func(args)


if __name__ == "__main__":
    sys.exit(main())
