#!/usr/bin/env python3
"""Flag flat `float unkN` members that are really Vector2/Vector3/Color/Transform
aggregates in the original source — the DWARF-retype lever.

THE LEVER
---------
When a Bank-8 header declares a run of flat `float unkN` scalars where the
original source declared a sub-struct aggregate (`Vector2`/`Vector3`/`Hmx::Color`/
`Transform`), MWCC **block-copies** the flattened struct with integer `lwz/stw`,
while the target **member-wise copies** the aggregate with float `lfs/stfs`.
Retyping the flat scalars back into the aggregate (using the Bank-5 DWARF as
evidence) flips our codegen to match — and deletes the `*(Vector3*)&unk0`
reinterpret casts at the use sites.

  Worked example — `RndLine::Point` (commit 34c4033c): 9 flat floats -> a
  `Vector3 cam` + three `Vector2 base/dir/delta`; `_M_fill_insert_aux<Point>`
  95.7% -> 99.6%. That was the ONLY true retype across a ~35-struct sweep; every
  other unk-float struct was already-typed, genuine scalars, or bank-divergent.
  Hence this tool: separate the buckets programmatically so nobody hand-audits
  35 structs again.

  objdiff signature: a run of `replace` diffs where target opcodes are
  `lfs/stfs` and base opcodes are `lwz/stw` (or the inverse) inside a struct-copy
  region (typically stlport `_M_fill_insert_aux` / `_M_insert_overflow_aux`
  instantiations or assignment operators). Counter-signature: `lwz/stw` on BOTH
  sides ⇒ struct already member-faithful, residual is permuter-class, retype
  changes nothing.

HONESTY MODEL — this is a CANDIDATE GENERATOR
---------------------------------------------
The only DWARF available is the **Bank-5 proto** ELF (~mid-2009); the target is
**Bank 8** (~2010). Layouts genuinely diverge — a Bank-5 `Vector3` occupies 0x10
bytes vs Bank-8's 0xc, and members are added/dropped/reordered per bank. So this
tool is a candidate generator in the exact mold of
`scripts/analysis/struct_layout_audit.py` + `scripts/analysis/struct_confirm.py`:
a RETYPE_CANDIDATE verdict must be **confirmed against Bank-8 evidence** — the
objdiff diff of the copy region, `struct_confirm.py`-style anchored deltas, and
the full-rebuild regression gate — before a header edit lands. See commit
34c4033c for the worked example.

VERDICTS
--------
  RETYPE_CANDIDATE  a whitelisted aggregate in the Bank-5 layout lines up with
                    >=2 consecutive our-side 4-byte scalars -> emit the mapping.
  DIVERGENT_CAUTION Bank-5 and our layout diverge (base-class rows, unexplained
                    tail, unresolved aggregate, bad accounting, or scalar runs
                    whose names don't exist in Bank-5) -> confirm by hand.
  ALREADY_TYPED     strong layout agreement AND the aggregates are already typed
                    correctly on our side. Nothing to do.
  GENUINE_SCALARS   strong agreement with no aggregate to retype. The floats are
                    really floats (Bank 8 flattened a sub-struct, etc.).
  NO_DWARF          no Bank-5 candidate for the leaf name; can't advise.

USAGE
-----
    scripts/analysis/flat_member_retype_scan.py --selftest          # hermetic
    scripts/analysis/flat_member_retype_scan.py                     # port scan
    scripts/analysis/flat_member_retype_scan.py --filter Point --unit system/rndobj
    scripts/analysis/flat_member_retype_scan.py --json out.json
    scripts/analysis/flat_member_retype_scan.py --refresh           # rebuild cache
    scripts/analysis/flat_member_retype_scan.py --min-run 4

The first real run streams ~11M `readelf --debug-dump=info` lines (minutes,
progress on stderr); the resolved registry is cached to
/tmp/claude/struct_dwarf/typed_layouts_v1.json (own file, does NOT touch the
audit's target_layouts.json) keyed on the ELF mtime + a cache-version int, so
subsequent runs are instant. `--refresh` rebuilds it.
"""

from __future__ import annotations

import argparse
import json
import re
import subprocess
import sys
import tempfile
import time
from collections import Counter
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
SRC = REPO / "src"

# Reuse the audit's ELF path + scope filter (and, optionally, TU match%).
sys.path.insert(0, str(Path(__file__).resolve().parent))
from struct_layout_audit import (  # type: ignore  # noqa: E402
    DEBUG_ELF,
    OUT_OF_SCOPE_PREFIXES,
    load_owner_pcts,
)

# Our-side header parser (has member TYPES, unlike the audit's offset-only pass).
sys.path.insert(0, str(REPO / "tools"))
from struct_db import parse_header, guess_type_size  # type: ignore  # noqa: E402

CACHE = Path("/tmp/claude/struct_dwarf/typed_layouts_v1.json")
CACHE_VERSION = 1

# ── classifier constants ──────────────────────────────────────────────────────

# The retype whitelist, with BANK-8 sizes (Bank-5 Vector3 is 0x10, B8 is 0xc).
AGG = {"Vector2": 8, "Vector3": 12, "Vector4": 16, "Color": 16, "Color32": 4,
       "Transform": 64, "Quat": 16, "Plane": 16}

SCALARS4 = {"float", "int", "unsigned int", "u32", "s32", "long", "unsigned long"}

# Object-system wrappers: never expanded, never a retype target.
WRAPPERS = {"ObjPtr", "ObjOwnerPtr", "ObjVector", "ObjPtrList", "ObjList",
            "ObjDirPtr", "String", "Symbol", "FilePath", "DataNode"}


def norm_type(t: str) -> str:
    t = t.replace("Hmx::", "").replace("const ", "").strip().rstrip("&").strip()
    return t


# ── DWARF extraction (typed registry) ─────────────────────────────────────────
#
# Extends struct_layout_audit's offset-only pass with DIE-offset capture, a
# global type table, and DW_AT_type ref resolution, so each member carries a
# resolved type NAME + byte_size. DW_TAG_inheritance rows are kept as `(base)`.
# All same-leaf-name class candidates are retained (leaf-name collision is real —
# `Point` has 3 distinct classes) and deduped by member signature.

DIE_RE = re.compile(
    r"^\s*<(\d+)><([0-9a-f]+)>:\s*Abbrev Number:\s*\d+(?:\s*\((DW_TAG_\w+)\))?")
ATTR_RE = re.compile(r"^\s*<[0-9a-f]+>\s+(DW_AT_\w+)\s*:\s*(.*?)\s*$")
UCONST_RE = re.compile(r"DW_OP_plus_uconst:\s*(\d+)")
REF_RE = re.compile(r"<0x([0-9a-f]+)>")
CU_RE = re.compile(r"^\s*Compilation Unit @ offset (0x[0-9a-f]+)")

TYPE_TAGS = {"DW_TAG_base_type", "DW_TAG_structure_type", "DW_TAG_class_type",
             "DW_TAG_typedef", "DW_TAG_pointer_type", "DW_TAG_reference_type",
             "DW_TAG_const_type", "DW_TAG_volatile_type", "DW_TAG_array_type",
             "DW_TAG_enumeration_type", "DW_TAG_union_type"}


def _parse_int(v: str):
    v = v.strip()
    try:
        return int(v, 16) if v.lower().startswith("0x") else int(v)
    except ValueError:
        return None


def _build_registry(max_lines: int | None = None) -> dict:
    """Stream readelf and return {leaf_name: [candidate, ...]} with each member
    resolved to (name, off, type, tsize, ttag)."""
    types: dict[int, list] = {}   # die_off -> [tag, name, byte_size, ref]
    classes: list[dict] = []      # {die, name, byte_size, members, cu}
    stack: list[tuple] = []       # (depth, die_off, crec)
    cur_die = None
    cu_base = 0

    proc = subprocess.Popen(
        ["readelf", "--debug-dump=info", str(DEBUG_ELF)],
        stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, text=True, bufsize=1)
    assert proc.stdout is not None
    t0 = time.time()
    n = 0
    for line in proc.stdout:
        n += 1
        if max_lines and n > max_lines:
            proc.terminate()
            break
        if n % 500000 == 0:
            print(f"  [dwarf] {n} lines, {len(classes)} classes "
                  f"({time.time() - t0:.0f}s)", file=sys.stderr)

        m = CU_RE.match(line)
        if m:
            cu_base = int(m.group(1), 16)
            continue
        dm = DIE_RE.match(line)
        if dm:
            depth = int(dm.group(1))
            off = int(dm.group(2), 16)
            tag = dm.group(3)
            while stack and stack[-1][0] >= depth:
                stack.pop()
            cur_die = None
            if tag in TYPE_TAGS:
                rec = [tag, None, None, None]
                types[off] = rec
                cur_die = ("type", rec)
                if tag in ("DW_TAG_structure_type", "DW_TAG_class_type"):
                    crec = {"die": off, "name": None, "byte_size": None,
                            "members": [], "cu": cu_base}
                    classes.append(crec)
                    stack.append((depth, off, crec))
                    cur_die = ("class", (rec, crec))
            elif tag == "DW_TAG_member" and stack and depth == stack[-1][0] + 1:
                mrec = {"name": None, "off": None, "ref": None}
                stack[-1][2]["members"].append(mrec)
                cur_die = ("member", mrec)
            elif tag == "DW_TAG_inheritance" and stack and depth == stack[-1][0] + 1:
                mrec = {"name": "(base)", "off": None, "ref": None}
                stack[-1][2]["members"].append(mrec)
                cur_die = ("member", mrec)
            continue

        am = ATTR_RE.match(line)
        if not am or cur_die is None:
            continue
        attr, val = am.group(1), am.group(2)
        kind, obj = cur_die
        if kind == "type":
            if attr == "DW_AT_name":
                obj[1] = val
            elif attr == "DW_AT_byte_size":
                obj[2] = _parse_int(val)
            elif attr == "DW_AT_type":
                r = REF_RE.search(val)
                if r:
                    obj[3] = int(r.group(1), 16)
        elif kind == "class":
            trec, crec = obj
            if attr == "DW_AT_name":
                trec[1] = val
                crec["name"] = val
            elif attr == "DW_AT_byte_size":
                v = _parse_int(val)
                trec[2] = v
                crec["byte_size"] = v
        elif kind == "member":
            if attr == "DW_AT_name":
                obj["name"] = val
            elif attr == "DW_AT_data_member_location":
                u = UCONST_RE.search(val)
                obj["off"] = int(u.group(1)) if u else _parse_int(val)
            elif attr == "DW_AT_type":
                r = REF_RE.search(val)
                if r:
                    obj["ref"] = int(r.group(1), 16)
    proc.wait()
    print(f"[dwarf] {n} lines, {len(types)} type DIEs, {len(classes)} class DIEs",
          file=sys.stderr)

    def resolve(ref, depth=0):
        seen = set()
        name = None
        while ref is not None and ref not in seen and depth < 12:
            seen.add(ref)
            depth += 1
            t = types.get(ref)
            if t is None:
                return ("?unresolved:0x%x" % ref, None, None)
            tag, tname, bs, nref = t
            if tag in ("DW_TAG_const_type", "DW_TAG_volatile_type"):
                ref = nref
                continue
            if tag == "DW_TAG_typedef":
                if name is None:
                    name = tname
                ref = nref
                continue
            if tag in ("DW_TAG_pointer_type", "DW_TAG_reference_type"):
                return (name or "ptr", 4, tag)
            return (name or tname, bs, tag)
        return ("?cycle", None, None)

    registry: dict[str, list] = {}
    for crec in classes:
        if not crec["name"] or not crec["members"]:
            continue
        mem = []
        for m in crec["members"]:
            tn, tb, tt = resolve(m["ref"])
            mem.append({"name": m["name"], "off": m["off"],
                        "type": tn, "tsize": tb, "ttag": tt})
        registry.setdefault(crec["name"], []).append(
            {"die": hex(crec["die"]), "cu": hex(crec["cu"]),
             "byte_size": crec["byte_size"], "members": mem})

    # dedup identical candidates per leaf name
    for k, v in registry.items():
        seen = set()
        ded = []
        for c in v:
            sig = json.dumps([c["byte_size"],
                              [(m["name"], m["off"], m["type"]) for m in c["members"]]])
            if sig in seen:
                continue
            seen.add(sig)
            ded.append(c)
        registry[k] = ded
    return registry


def load_registry(refresh: bool, max_lines: int | None) -> dict:
    if not DEBUG_ELF.exists():
        print(f"missing debug ELF: {DEBUG_ELF}", file=sys.stderr)
        sys.exit(2)
    elf_mtime = DEBUG_ELF.stat().st_mtime
    if not refresh and not max_lines and CACHE.exists():
        try:
            cached = json.loads(CACHE.read_text())
            if (cached.get("_elf_mtime") == elf_mtime
                    and cached.get("_cache_version") == CACHE_VERSION):
                return cached["registry"]
        except (json.JSONDecodeError, KeyError):
            pass
    print("[dwarf] parsing Bank-5 debug ELF via readelf (cached afterwards)...",
          file=sys.stderr)
    registry = _build_registry(max_lines)
    if not max_lines:
        CACHE.parent.mkdir(parents=True, exist_ok=True)
        CACHE.write_text(json.dumps({"_elf_mtime": elf_mtime,
                                     "_cache_version": CACHE_VERSION,
                                     "registry": registry}))
    print(f"[dwarf] {len(registry)} leaf names", file=sys.stderr)
    return registry


# ── classifier (ported from the validated simulate_classifier.py) ─────────────


def our_sizes(members):
    """Size of each annotated member = gap to next annotated offset (last member:
    guess_type_size, fallback 4). Sidesteps template-size ambiguity."""
    out = []
    for i, m in enumerate(members):
        if i + 1 < len(members):
            out.append(members[i + 1].offset - m.offset)
        else:
            g = guess_type_size(m.type_str)
            out.append(g if g else 4)
    return out


def b8_derived_size(tname, tsize, probe):
    """Bank-8 size for a Bank-5 type: aggregates from the AGG table; other structs
    by recursive re-layout of their probe members (4-align cap); scalars/ptrs keep
    their Bank-5 size."""
    n = norm_type(tname or "")
    if n in AGG:
        return AGG[n]
    cands = probe.get(n)
    if cands and cands[0]["members"]:
        cur = 0
        for m in sorted([m for m in cands[0]["members"]
                         if m["off"] is not None and m["name"] != "(base)"],
                        key=lambda x: x["off"]):
            s = b8_derived_size(m["type"], m["tsize"], probe)
            a = min(4, s) if s in (1, 2, 4) else 4
            cur = (cur + a - 1) // a * a
            cur += s
        return (cur + 3) // 4 * 4 if cur else (tsize or 4)
    return tsize or 4


def classify(cls_name, ours, b5, probe, min_run=3):
    """ours: ClassInfo-like (has .members: [Member(name,type_str,offset)]);
    b5: chosen probe candidate dict. Returns (verdict, evidence_dict)."""
    # -- expand B5 members (skip bases, expand one level of non-aggregate structs)
    has_bases = any(m["name"] == "(base)" for m in b5["members"])
    our_type_leafs = {norm_type(om.type_str).split("<")[0].split("::")[-1]
                      for om in ours.members}
    exp = []
    for m in sorted([m for m in b5["members"]
                     if m["name"] != "(base)" and m["off"] is not None],
                    key=lambda x: x["off"]):
        n = norm_type(m["type"] or "")
        if (m["ttag"] in ("DW_TAG_class_type", "DW_TAG_structure_type")
                and n not in AGG
                and n.split("<")[0] not in WRAPPERS
                and n not in our_type_leafs):
            sub = probe.get(n)
            if sub and sub[0]["members"]:
                for sm in sorted([x for x in sub[0]["members"]
                                  if x["off"] is not None and x["name"] != "(base)"],
                                 key=lambda x: x["off"]):
                    exp.append({"name": sm["name"], "off": m["off"] + sm["off"],
                                "type": sm["type"], "tsize": sm["tsize"],
                                "ttag": sm["ttag"], "via": m["name"]})
                continue
        exp.append(dict(m, via=None))

    unexplained_tail = 0
    if exp and b5["byte_size"]:
        end = max(m["off"] + (m["tsize"] or 4) for m in exp)
        end16 = (end + 15) // 16 * 16
        unexplained_tail = max(0, b5["byte_size"] - end16)

    osize = our_sizes(ours.members)
    our_by_name = {m.name: (i, m) for i, m in enumerate(ours.members)}

    # -- phase 1: name pairs
    pairs = []  # (b5_member, our_idx)
    for m in exp:
        hit = our_by_name.get(m["name"])
        if hit:
            pairs.append((m, hit[0]))
    name_paired_b5 = {id(m) for m, _ in pairs}
    name_paired_ours = {i for _, i in pairs}

    # -- phase 2: positional pairs between anchors (greedy, size/family-compatible)
    b5_left = [m for m in exp if id(m) not in name_paired_b5]
    ours_left = [i for i in range(len(ours.members)) if i not in name_paired_ours]
    pos_pairs = []
    for m in b5_left:
        want = b8_derived_size(m["type"], m["tsize"], probe)
        for i in list(ours_left):
            osz = osize[i]
            on = norm_type(ours.members[i].type_str)
            mn = norm_type(m["type"] or "")
            compat = (osz == want) or ({on, mn} <= {"Color", "Color32"})
            if compat:
                pos_pairs.append((m, i))
                ours_left.remove(i)
                break
    pos_paired_b5 = {id(m) for m, _ in pos_pairs}
    b5_deletions = [m for m in b5_left if id(m) not in pos_paired_b5]
    ours_insertions = [ours.members[i].name for i in ours_left]

    # -- STRONG_MAP
    np_sorted = sorted(pairs, key=lambda p: p[0]["off"])
    our_offs = [ours.members[i].offset for _, i in np_sorted]
    monotone = all(our_offs[i] < our_offs[i + 1] for i in range(len(our_offs) - 1))
    strong = len(pairs) >= 3 and monotone and not b5_deletions

    # -- aggregate verdicts + retype mappings
    agg_findings = []
    retype = []
    for m in exp:
        n = norm_type(m["type"] or "")
        if n not in AGG:
            continue
        b8sz = AGG[n]
        hit = our_by_name.get(m["name"])
        if hit:
            i, om = hit
            on = norm_type(om.type_str)
            if on in AGG:
                agg_findings.append((m["name"], "TYPED"))
            elif n == "Color" and osize[i] == 4:
                # COLOR-PACKING INVERSE (not a retype): the Bank-5 source used a
                # 16-byte Hmx::Color here, but Bank 8 deliberately PACKED it to a
                # 4-byte Color32 int at the same name/offset (e.g.
                # LightPreset::EnvLightEntry::mColor). Re-expanding to Hmx::Color
                # would GROW the field 0x10 and shift every later member — the
                # opposite of the RndLine::Point lever. Treat as already-correct.
                agg_findings.append((m["name"], "COLOR_PACKED"))
            else:
                # name-paired to a scalar: consume the following 4-byte scalars
                span = [j for j in range(i, len(ours.members))
                        if ours.members[j].offset < om.offset + b8sz]
                sc = [j for j in span
                      if norm_type(ours.members[j].type_str) in SCALARS4]
                if len(sc) >= 2:
                    retype.append({
                        "b5_member": m["name"], "b5_type": n, "b5_off": m["off"],
                        "our_members": [ours.members[j].name for j in sc],
                        "our_offset": ours.members[sc[0]].offset,
                        "mode": "name-anchored"})
                else:
                    agg_findings.append((m["name"], "UNRESOLVED"))
        else:
            # positional: relayout cursor with our-side snapping
            cur = 0
            adj = None
            for e in exp:
                en = norm_type(e["type"] or "")
                esz = b8_derived_size(e["type"], e["tsize"], probe)
                snap = None
                for j, om2 in enumerate(ours.members):
                    if om2.offset == cur:
                        on2 = norm_type(om2.type_str)
                        if (om2.name == e["name"] or osize[j] == esz
                                or {on2, en} <= {"Color", "Color32"}):
                            snap = j
                        break
                a = min(4, esz) if esz in (1, 2) else 4
                cur = (cur + a - 1) // a * a
                if e is m:
                    adj = cur
                    break
                cur += osize[snap] if snap is not None else esz
            if adj is None:
                agg_findings.append((m["name"], "UNRESOLVED"))
                continue
            span = [j for j in range(len(ours.members))
                    if adj <= ours.members[j].offset < adj + b8sz]
            types_ = [norm_type(ours.members[j].type_str) for j in span]
            if types_ and all(t in SCALARS4 for t in types_) and len(span) >= 2:
                retype.append({
                    "b5_member": m["name"], "b5_type": n, "b5_off": m["off"],
                    "our_members": [ours.members[j].name for j in span],
                    "our_offset": ours.members[span[0]].offset,
                    "mode": "positional"})
            elif types_ and types_[0] in AGG:
                agg_findings.append((m["name"], "TYPED(pos)"))
            else:
                agg_findings.append((m["name"], "UNRESOLVED"))

    # -- scalar runs (>= min_run consecutive our-side 4-byte scalars)
    runs = []
    run = []
    for i, m in enumerate(ours.members):
        if (norm_type(m.type_str) in SCALARS4
                and (not run or ours.members[run[-1]].offset + 4 == m.offset)):
            run.append(i)
        else:
            if len(run) >= min_run:
                runs.append(run)
            run = [i] if norm_type(m.type_str) in SCALARS4 else []
    if len(run) >= min_run:
        runs.append(run)
    b5_scalar_names = {m["name"] for m in exp
                       if m["ttag"] in ("DW_TAG_base_type", "DW_TAG_enumeration_type")}
    runs_confirmed = (all(all(ours.members[j].name in b5_scalar_names for j in r)
                          for r in runs) if runs else True)

    unresolved = [f for f in agg_findings if "UNRESOLVED" in f[1]]

    # -- aggregate accounting (per AGG type, Color family pooled)
    cb5 = Counter(norm_type(m["type"]) for m in exp
                  if norm_type(m["type"] or "") in AGG)
    cours = Counter(norm_type(m.type_str) for m in ours.members
                    if norm_type(m.type_str) in AGG)

    def acct_ok():
        for t, c in cb5.items():
            have = cours.get(t, 0)
            if t in ("Color", "Color32"):
                have = cours.get("Color", 0) + cours.get("Color32", 0)
            if have < c:
                return False
        return True

    account_ok = acct_ok()

    # A POSITIONAL retype anchor (weakest kind) is the false-alignment trap when
    # the struct ALREADY has that aggregate family fully typed (`account_ok`): the
    # Bank-5 member lands on an unrelated run of our scalars purely because earlier
    # members shifted between banks, while the real aggregate is typed elsewhere
    # (e.g. RndPostProc's B5 mMotionBlurBlendWeight Color aligning onto our timing
    # scalars though mMotionBlurWeight is already Hmx::Color; CharLookAt's
    # mSourceFilter onto angle scalars). Demote those to a caution finding rather
    # than assert a retype. Name-anchored retypes, and positional retypes where the
    # aggregate is genuinely MISSING from our layout (RndLine::Point pre-retype:
    # account_ok is False — cam/base/dir/delta are not yet typed), are kept.
    demoted_positional = [r for r in retype
                          if r.get("mode") == "positional" and account_ok]
    if demoted_positional:
        retype = [r for r in retype if r not in demoted_positional]
        for r in demoted_positional:
            agg_findings.append((r["b5_member"], "POSITIONAL_REDUNDANT"))

    # -- headline verdict (validated order)
    if retype:
        verdict = "RETYPE_CANDIDATE"
    elif not strong and (has_bases or unexplained_tail > 0 or unresolved
                         or not account_ok or not runs_confirmed):
        verdict = "DIVERGENT_CAUTION"
    elif strong:
        verdict = "ALREADY_TYPED" if agg_findings else "GENUINE_SCALARS"
    else:
        verdict = "GENUINE_SCALARS"

    ev = dict(strong=strong, pairs=len(pairs), pos_pairs=len(pos_pairs),
              deletions=[m["name"] for m in b5_deletions],
              insertions=ours_insertions, has_bases=has_bases,
              unexplained_tail=unexplained_tail, retype=retype,
              aggregate_findings=agg_findings, runs=len(runs),
              runs_confirmed=runs_confirmed, acct_ok=account_ok)
    return verdict, ev


def pick(cands, ours):
    """Choose the DWARF candidate maximizing shared member names with ours."""
    onames = {m.name for m in ours.members}
    scored = sorted(cands,
                    key=lambda c: sum(1 for m in c["members"] if m["name"] in onames),
                    reverse=True)
    best = scored[0]
    ambiguity = len(cands) - 1  # runner-up count
    return best, ambiguity


# ── scan orchestration ────────────────────────────────────────────────────────


def rel_unit(path: Path) -> str:
    try:
        return str(path.resolve().relative_to(SRC.resolve()))
    except ValueError:
        return str(path)


def in_scope(rel: str, include_all: bool) -> bool:
    if include_all:
        return True
    return not any(rel.startswith(p) for p in OUT_OF_SCOPE_PREFIXES)


def has_scalar_run(ci, min_run):
    run = []
    for i, m in enumerate(ci.members):
        if (norm_type(m.type_str) in SCALARS4
                and (not run or ci.members[run[-1]].offset + 4 == m.offset)):
            run.append(i)
            if len(run) >= min_run:
                return True
        else:
            run = [i] if norm_type(m.type_str) in SCALARS4 else []
    return False


def cand_has_agg(cand):
    return any(norm_type(m["type"] or "") in AGG for m in cand["members"])


def our_total_size(ci):
    if not ci.members:
        return 0
    osz = our_sizes(ci.members)
    return ci.members[-1].offset + osz[-1]


def scan(registry, args, owner_pcts):
    """Yield evaluation records for in-scope annotated structs of interest."""
    results = []
    for header in SRC.rglob("*.h"):
        rel = rel_unit(header)
        if args.unit and not rel.startswith(args.unit):
            continue
        if not in_scope(rel, args.all):
            continue
        try:
            cis = parse_header(header)
        except Exception:
            continue
        for ci in cis:
            if not ci.members:
                continue
            explicit = bool(args.filter and args.filter in ci.name)
            if args.filter and not explicit:
                continue
            leaf = ci.name.split("::")[-1]
            cands = registry.get(leaf)
            record = {
                "cls": ci.name, "leaf": leaf, "header": rel,
                "owner_pct": owner_pcts.get(rel[:-2] if rel.endswith(".h") else rel),
                "our_size": our_total_size(ci),
            }
            if not cands:
                # only surface a NO_DWARF struct when it is of interest
                if not (explicit or has_scalar_run(ci, args.min_run)):
                    continue
                record.update(verdict="NO_DWARF", b5_die=None, b5_byte_size=None,
                              ambiguity=0)
                results.append(record)
                continue
            cand, ambiguity = pick(cands, ci)
            interesting = (explicit or has_scalar_run(ci, args.min_run)
                           or cand_has_agg(cand))
            if not interesting:
                continue
            verdict, ev = classify(ci.name, ci, cand, registry, args.min_run)
            record.update(verdict=verdict, b5_die=cand["die"],
                          b5_byte_size=cand["byte_size"], ambiguity=ambiguity)
            record.update(ev)
            results.append(record)
    return results


# ── output ─────────────────────────────────────────────────────────────────────

BANNER = """\
================================================================================
 flat_member_retype_scan — CANDIDATE GENERATOR (not a verdict)
--------------------------------------------------------------------------------
 The lever: flat `float unkN` members that are really Vector2/Vector3/Color/
 Transform aggregates in the original. MWCC block-copies our flattened struct
 (lwz/stw) where the target member-copies floats (lfs/stfs); retyping from the
 Bank-5 DWARF flips our codegen to match. See commit 34c4033c (RndLine::Point,
 9 flat floats -> Vector3 cam + 3x Vector2, 95.7% -> 99.6%).

 HONESTY: the only DWARF is the Bank-5 proto (~2009); the target is Bank 8
 (~2010). Layouts diverge (B5 Vector3 = 0x10 vs B8 0xc; members added/dropped
 per bank). A RETYPE_CANDIDATE is a hypothesis — CONFIRM it against Bank-8
 evidence (objdiff diff of the copy region, scripts/analysis/struct_confirm.py,
 the full-rebuild regression gate) BEFORE editing a header.
================================================================================"""


def _hx(v):
    return f"0x{v:x}" if isinstance(v, int) else str(v)


def fmt_mapping_rows(rec):
    rows = []
    for r in rec.get("retype", []):
        ours = "/".join(r["our_members"])
        rows.append(f"      our {ours} @{_hx(r['our_offset'])} -> "
                    f"{r['b5_type']} {r['b5_member']} (B5 @{_hx(r['b5_off'])})"
                    f"  [{r['mode']}]")
    return rows


def render(results):
    print(BANNER)
    order = ["RETYPE_CANDIDATE", "DIVERGENT_CAUTION", "ALREADY_TYPED",
             "GENUINE_SCALARS", "NO_DWARF"]
    by_v = {v: [] for v in order}
    for r in results:
        by_v.setdefault(r["verdict"], []).append(r)

    def owner_str(r):
        p = r.get("owner_pct")
        return f"{p:.1f}%" if p is not None else "??"

    # full detail: RETYPE_CANDIDATE, DIVERGENT_CAUTION
    for v in ("RETYPE_CANDIDATE", "DIVERGENT_CAUTION"):
        group = sorted(by_v[v], key=lambda r: r["cls"])
        if not group:
            continue
        print(f"\n########## {v} — {len(group)} struct(s) ##########")
        for r in group:
            amb = f"  (+{r['ambiguity']} other leaf-name candidate(s))" if r.get("ambiguity") else ""
            print(f"\n  {r['cls']}   {r['header']} ({owner_str(r)})   "
                  f"B5 die={r['b5_die']} size={_hx(r['b5_byte_size'])}{amb}")
            print(f"      strong={r.get('strong')} pairs={r.get('pairs')} "
                  f"pos={r.get('pos_pairs')} runs={r.get('runs')} "
                  f"runs_ok={r.get('runs_confirmed')} bases={r.get('has_bases')} "
                  f"tail={r.get('unexplained_tail')} acct_ok={r.get('acct_ok')}")
            if r.get("deletions"):
                print(f"      B5-only (deletions): {r['deletions']}")
            if r.get("insertions"):
                print(f"      ours-only (insertions): {r['insertions']}")
            if r.get("aggregate_findings"):
                print(f"      aggregates: {r['aggregate_findings']}")
            for row in fmt_mapping_rows(r):
                print(row)

    # one-liners: ALREADY_TYPED, GENUINE_SCALARS, NO_DWARF
    for v in ("ALREADY_TYPED", "GENUINE_SCALARS", "NO_DWARF"):
        group = sorted(by_v[v], key=lambda r: r["cls"])
        if not group:
            continue
        print(f"\n########## {v} — {len(group)} struct(s) ##########")
        for r in group:
            extra = ""
            if v != "NO_DWARF":
                extra = (f"  pairs={r.get('pairs')} dels={r.get('deletions')} "
                         f"runs={r.get('runs')}")
            print(f"  {r['cls']:<48} {r['header']:<40} ({owner_str(r)})  {v}{extra}")

    print("\n=== summary ===")
    for v in order:
        print(f"  {v:<18}: {len(by_v[v])}")
    print("  Confirm every RETYPE_CANDIDATE against Bank-8 evidence before "
          "editing a header (see banner).")


# ── hermetic selftest ──────────────────────────────────────────────────────────

# Pre-retype RndLine::Point header (from `git show 34c4033c^:src/system/rndobj/Line.h`).
_SELFTEST_PRE_POINT_HDR = """\
namespace Hmx { class Color32; }
class Vector3;
class RndLine {
public:
    // size 0x34
    class Point {
    public:
        Vector3 v; // 0x0
        Hmx::Color32 c; // 0xc
        float unk0; // 0x10
        float unk1; // 0x14
        float unk2; // 0x18
        float unk3; // 0x1c
        float unk4; // 0x20
        float unk5; // 0x24
        float unk6; // 0x28
        float unk7; // 0x2c
        float unk8; // 0x30
    };
};
"""

# Real current-header member lists (name, type_str, offset), embedded so the
# selftest never touches the live tree.
_SELFTEST_OURS = {
    "FingerDesc": [  # ALREADY_TYPED (mDestPos Vector3 already typed)
        ["mIsControlled", "bool", 0], ["mLength", "float", 4],
        ["mDestPos", "Vector3", 8], ["mAltDestPos", "Vector3", 20],
        ["mBone1", "ObjPtr<RndTransformable>", 32],
        ["mBone2", "ObjPtr<RndTransformable>", 44],
        ["mBone3", "ObjPtr<RndTransformable>", 56],
        ["mBoneTip", "ObjPtr<RndTransformable>", 68],
        ["mBone2DestAngle", "float", 80], ["mBone3DestAngle", "float", 84],
        ["mBone2CurAngle", "float", 88], ["mBone3CurAngle", "float", 92],
        ["mFBlendInFrames", "int", 96], ["mFBlendOutFrames", "int", 100],
        ["mResetFingerRots", "bool", 104], ["mDestForwardVector", "Vector3", 108],
        ["mCurForwardVector", "Vector3", 120], ["mDestChanged", "bool", 132],
    ],
    "LightParams_Spot": [  # GENUINE_SCALARS (B8 flattened CachedData)
        ["mDirection", "Vector3", 0], ["mColor", "Hmx::Color", 12],
        ["mTipPosition", "Vector3", 28], ["mCosTheta", "float", 40],
        ["mOneOverOneSubCos", "float", 44], ["mOneOverRange2x", "float", 48],
        ["mTipOverRange2x", "float", 52], ["mLightPos", "Vector3", 56],
        ["mRange", "float", 68], ["mRadiusTop", "float", 72],
        ["mRadiusBottom", "float", 76],
    ],
    "CharEyes": [  # DIVERGENT_CAUTION (B5 0x1d0 vs B8 ~0x160, added/dropped members)
        ["mEyes", "ObjVector<EyeDesc>", 40], ["mInterests", "ObjVector<CharInterestState>", 52],
        ["mFaceServo", "ObjPtr<CharFaceServo>", 64], ["mCamWeight", "ObjPtr<CharWeightSetter>", 76],
        ["mTarget", "Vector3", 88], ["mDefaultFilterFlags", "int", 100],
        ["mViewDirection", "ObjPtr<RndTransformable>", 104], ["mHeadLookAt", "ObjPtr<CharLookAt>", 116],
        ["mMaxExtrapolation", "float", 128], ["mMinTargetDist", "float", 132],
        ["mUpperLidTrackUp", "float", 136], ["mUpperLidTrackDown", "float", 140],
        ["mLowerLidTrackUp", "float", 144], ["mLowerLidTrackDown", "float", 148],
        ["mLowerLidTrackRotate", "bool", 152], ["mOverlay", "RndOverlay *", 156],
        ["mInterestFilterFlags", "int", 160], ["mLastFacing", "Vector3", 164],
        ["mLastCang", "float", 176], ["mLastLook", "float", 180],
        ["mMaxEyeCang", "float", 184], ["mAvDelta", "float", 188],
        ["mLastBlinkWeight", "float", 192], ["mBlinkDetect", "bool", 196],
        ["mTargetTooClose", "bool", 197], ["mCurInterest", "ObjPtr<CharInterest>", 200],
        ["mFocusInterest", "ObjPtr<CharInterest>", 212], ["mCurFocusPriorityClass", "int", 224],
        ["mNewFocusInterest", "bool", 228], ["mLastExtrapolatedDir", "Vector3", 232],
        ["mLastHeadIKWeight", "float", 244],
        ["mEyeDartRulesetData", "CharEyeDartRuleset::EyeDartRulesetData", 248],
        ["mDarting", "bool", 292], ["mDartNextEventTime", "float", 296],
        ["mDartsRemaining", "int", 300], ["mCurDartOffset", "Vector3", 304],
        ["mProceduralBlink", "bool", 316], ["mBlinkTimestamp", "float", 320],
        ["mBlinkWindowCount", "int", 324], ["mBlinkWindowSecsRemaining", "float", 328],
        ["mLastBlinkDetectTime", "float", 332], ["mDelayedTarget", "Vector3", 336],
        ["mInterestFiltersChanged", "bool", 348], ["mBlinksEnabled", "bool", 349],
    ],
}

# Real Bank-5 candidates (lifted verbatim from the validated probe_out.json).
_SELFTEST_PROBE_JSON = r"""{"Vector3":[{"die":"0x13c0","cu":"0x0","byte_size":16,"members":[{"name":"x","off":0,"type":"float","tsize":4,"ttag":"DW_TAG_base_type"},{"name":"y","off":4,"type":"float","tsize":4,"ttag":"DW_TAG_base_type"},{"name":"z","off":8,"type":"float","tsize":4,"ttag":"DW_TAG_base_type"}]}],"Color":[{"die":"0x365a","cu":"0x0","byte_size":16,"members":[{"name":"r","off":0,"type":"float","tsize":4,"ttag":"DW_TAG_base_type"},{"name":"g","off":4,"type":"float","tsize":4,"ttag":"DW_TAG_base_type"},{"name":"b","off":8,"type":"float","tsize":4,"ttag":"DW_TAG_base_type"},{"name":"a","off":12,"type":"float","tsize":4,"ttag":"DW_TAG_base_type"}]}],"ObjPtr":[{"die":"0x37ea","cu":"0x0","byte_size":12,"members":[{"name":"(base)","off":0,"type":"ObjRef","tsize":4,"ttag":"DW_TAG_class_type"},{"name":"mOwner","off":4,"type":"ptr","tsize":4,"ttag":"DW_TAG_pointer_type"},{"name":"mPtr","off":8,"type":"ptr","tsize":4,"ttag":"DW_TAG_pointer_type"}]}],"ObjOwnerPtr":[{"die":"0x3f91","cu":"0x0","byte_size":12,"members":[{"name":"(base)","off":0,"type":"ObjRef","tsize":4,"ttag":"DW_TAG_class_type"},{"name":"mOwner","off":4,"type":"ptr","tsize":4,"ttag":"DW_TAG_pointer_type"},{"name":"mPtr","off":8,"type":"ptr","tsize":4,"ttag":"DW_TAG_pointer_type"}]}],"Vector2":[{"die":"0xe075","cu":"0x0","byte_size":8,"members":[{"name":"x","off":0,"type":"float","tsize":4,"ttag":"DW_TAG_base_type"},{"name":"y","off":4,"type":"float","tsize":4,"ttag":"DW_TAG_base_type"}]}],"ObjVector":[{"die":"0x11e59","cu":"0x10f69","byte_size":16,"members":[{"name":"(base)","off":0,"type":"vector","tsize":12,"ttag":"DW_TAG_class_type"},{"name":"mOwner","off":12,"type":"ptr","tsize":4,"ttag":"DW_TAG_pointer_type"}]}],"CharEyes":[{"die":"0x1995d","cu":"0x16c47","byte_size":464,"members":[{"name":"(base)","off":0,"type":"RndHighlightable","tsize":52,"ttag":"DW_TAG_class_type"},{"name":"(base)","off":8,"type":"CharWeightable","tsize":68,"ttag":"DW_TAG_class_type"},{"name":"(base)","off":32,"type":"CharPollable","tsize":52,"ttag":"DW_TAG_class_type"},{"name":"mEyes","off":40,"type":"ObjVector","tsize":16,"ttag":"DW_TAG_class_type"},{"name":"mInterests","off":56,"type":"ObjVector","tsize":16,"ttag":"DW_TAG_class_type"},{"name":"mFaceServo","off":72,"type":"ObjPtr","tsize":12,"ttag":"DW_TAG_class_type"},{"name":"mCamWeight","off":84,"type":"ObjPtr","tsize":12,"ttag":"DW_TAG_class_type"},{"name":"mTarget","off":96,"type":"Vector3","tsize":16,"ttag":"DW_TAG_class_type"},{"name":"mDefaultInterestFilterFlags","off":112,"type":"int","tsize":4,"ttag":"DW_TAG_base_type"},{"name":"mViewDirBone","off":116,"type":"ObjPtr","tsize":12,"ttag":"DW_TAG_class_type"},{"name":"mHeadLookAt","off":128,"type":"ObjPtr","tsize":12,"ttag":"DW_TAG_class_type"},{"name":"mMaxExtrapolationAngle","off":140,"type":"float","tsize":4,"ttag":"DW_TAG_base_type"},{"name":"mMinTargetDistance","off":144,"type":"float","tsize":4,"ttag":"DW_TAG_base_type"},{"name":"mUpperLidTrackUpScale","off":148,"type":"float","tsize":4,"ttag":"DW_TAG_base_type"},{"name":"mUpperLidTrackDownScale","off":152,"type":"float","tsize":4,"ttag":"DW_TAG_base_type"},{"name":"mLowerLidTrackUpScale","off":156,"type":"float","tsize":4,"ttag":"DW_TAG_base_type"},{"name":"mLowerLidTrackUpOutScale","off":160,"type":"float","tsize":4,"ttag":"DW_TAG_base_type"},{"name":"mLowerLidTrackDownScale","off":164,"type":"float","tsize":4,"ttag":"DW_TAG_base_type"},{"name":"mLowerLidTrackDownOutScale","off":168,"type":"float","tsize":4,"ttag":"DW_TAG_base_type"},{"name":"mOverlay","off":172,"type":"ptr","tsize":4,"ttag":"DW_TAG_pointer_type"},{"name":"mCurInterestFilterFlags","off":176,"type":"int","tsize":4,"ttag":"DW_TAG_base_type"},{"name":"mLastFacing","off":192,"type":"Vector3","tsize":16,"ttag":"DW_TAG_class_type"},{"name":"mLastCang","off":208,"type":"float","tsize":4,"ttag":"DW_TAG_base_type"},{"name":"mLastLook","off":212,"type":"float","tsize":4,"ttag":"DW_TAG_base_type"},{"name":"mMaxEyeCang","off":216,"type":"float","tsize":4,"ttag":"DW_TAG_base_type"},{"name":"mAvDelta","off":220,"type":"float","tsize":4,"ttag":"DW_TAG_base_type"},{"name":"mLastBlinkWeight","off":224,"type":"float","tsize":4,"ttag":"DW_TAG_base_type"},{"name":"mBlinkDetect","off":228,"type":"bool","tsize":1,"ttag":"DW_TAG_base_type"},{"name":"mTargetTooClose","off":229,"type":"bool","tsize":1,"ttag":"DW_TAG_base_type"},{"name":"mCurInterest","off":232,"type":"ObjPtr","tsize":12,"ttag":"DW_TAG_class_type"},{"name":"mFocusInterest","off":244,"type":"ObjPtr","tsize":12,"ttag":"DW_TAG_class_type"},{"name":"mCurFocusPriorityClass","off":256,"type":"int","tsize":4,"ttag":"DW_TAG_base_type"},{"name":"mNewFocusInterest","off":260,"type":"bool","tsize":1,"ttag":"DW_TAG_base_type"},{"name":"mLastExtrapolatedDir","off":272,"type":"Vector3","tsize":16,"ttag":"DW_TAG_class_type"},{"name":"mLastHeadIKWeight","off":288,"type":"float","tsize":4,"ttag":"DW_TAG_base_type"},{"name":"mCurDartRuleset","off":292,"type":"EyeDartRulesetData","tsize":44,"ttag":"DW_TAG_class_type"},{"name":"mDarting","off":336,"type":"bool","tsize":1,"ttag":"DW_TAG_base_type"},{"name":"mDartNextEventTime","off":340,"type":"float","tsize":4,"ttag":"DW_TAG_base_type"},{"name":"mDartsRemaining","off":344,"type":"int","tsize":4,"ttag":"DW_TAG_base_type"},{"name":"mCurDartOffset","off":352,"type":"Vector3","tsize":16,"ttag":"DW_TAG_class_type"},{"name":"mProceduralBlink","off":368,"type":"bool","tsize":1,"ttag":"DW_TAG_base_type"},{"name":"mBlinkTimestamp","off":372,"type":"float","tsize":4,"ttag":"DW_TAG_base_type"},{"name":"mBlinkWindowCount","off":376,"type":"int","tsize":4,"ttag":"DW_TAG_base_type"},{"name":"mBlinkWindowSecsRemaining","off":380,"type":"float","tsize":4,"ttag":"DW_TAG_base_type"},{"name":"mLastBlinkDetectTime","off":384,"type":"float","tsize":4,"ttag":"DW_TAG_base_type"},{"name":"mDelayedTarget","off":400,"type":"Vector3","tsize":16,"ttag":"DW_TAG_class_type"},{"name":"mInterestFiltersChanged","off":416,"type":"bool","tsize":1,"ttag":"DW_TAG_base_type"}]}],"EyeDartRulesetData":[{"die":"0x1a313","cu":"0x16c47","byte_size":44,"members":[{"name":"mMinRadius","off":0,"type":"float","tsize":4,"ttag":"DW_TAG_base_type"},{"name":"mMaxRadius","off":4,"type":"float","tsize":4,"ttag":"DW_TAG_base_type"},{"name":"mOnTargetAngleThresh","off":8,"type":"float","tsize":4,"ttag":"DW_TAG_base_type"},{"name":"mMinDartsPerSequence","off":12,"type":"int","tsize":4,"ttag":"DW_TAG_base_type"},{"name":"mMaxDartsPerSequence","off":16,"type":"int","tsize":4,"ttag":"DW_TAG_base_type"},{"name":"mMinSecsBetweenDarts","off":20,"type":"float","tsize":4,"ttag":"DW_TAG_base_type"},{"name":"mMaxSecsBetweenDarts","off":24,"type":"float","tsize":4,"ttag":"DW_TAG_base_type"},{"name":"mMinSecsBetweenSequences","off":28,"type":"float","tsize":4,"ttag":"DW_TAG_base_type"},{"name":"mMaxSecsBetweenSequences","off":32,"type":"float","tsize":4,"ttag":"DW_TAG_base_type"},{"name":"mScaleWithDistance","off":36,"type":"bool","tsize":1,"ttag":"DW_TAG_base_type"},{"name":"mReferenceDistance","off":40,"type":"float","tsize":4,"ttag":"DW_TAG_base_type"}]}],"FingerDesc":[{"die":"0x35fc72","cu":"0x35f54b","byte_size":160,"members":[{"name":"mIsControlled","off":0,"type":"bool","tsize":1,"ttag":"DW_TAG_base_type"},{"name":"mLength","off":4,"type":"float","tsize":4,"ttag":"DW_TAG_base_type"},{"name":"mDestPos","off":16,"type":"Vector3","tsize":16,"ttag":"DW_TAG_class_type"},{"name":"mBone1","off":32,"type":"ObjPtr","tsize":12,"ttag":"DW_TAG_class_type"},{"name":"mBone2","off":44,"type":"ObjPtr","tsize":12,"ttag":"DW_TAG_class_type"},{"name":"mBone3","off":56,"type":"ObjPtr","tsize":12,"ttag":"DW_TAG_class_type"},{"name":"mBoneTip","off":68,"type":"ObjPtr","tsize":12,"ttag":"DW_TAG_class_type"},{"name":"mBone2DestAngle","off":80,"type":"float","tsize":4,"ttag":"DW_TAG_base_type"},{"name":"mBone3DestAngle","off":84,"type":"float","tsize":4,"ttag":"DW_TAG_base_type"},{"name":"mBone2CurAngle","off":88,"type":"float","tsize":4,"ttag":"DW_TAG_base_type"},{"name":"mBone3CurAngle","off":92,"type":"float","tsize":4,"ttag":"DW_TAG_base_type"},{"name":"mFBlendInFrames","off":96,"type":"int","tsize":4,"ttag":"DW_TAG_base_type"},{"name":"mFBlendOutFrames","off":100,"type":"int","tsize":4,"ttag":"DW_TAG_base_type"},{"name":"mResetFingerRots","off":104,"type":"bool","tsize":1,"ttag":"DW_TAG_base_type"},{"name":"mDestForwardVector","off":112,"type":"Vector3","tsize":16,"ttag":"DW_TAG_class_type"},{"name":"mCurForwardVector","off":128,"type":"Vector3","tsize":16,"ttag":"DW_TAG_class_type"},{"name":"mDestChanged","off":144,"type":"bool","tsize":1,"ttag":"DW_TAG_base_type"}]}],"Point":[{"die":"0x3ff977","cu":"0x3ff34d","byte_size":144,"members":[{"name":"pos","off":0,"type":"Vector3","tsize":16,"ttag":"DW_TAG_class_type"},{"name":"force","off":16,"type":"Vector3","tsize":16,"ttag":"DW_TAG_class_type"},{"name":"lastFriction","off":32,"type":"Vector3","tsize":16,"ttag":"DW_TAG_class_type"},{"name":"lastZ","off":48,"type":"Vector3","tsize":16,"ttag":"DW_TAG_class_type"},{"name":"bone","off":64,"type":"ObjPtr","tsize":12,"ttag":"DW_TAG_class_type"},{"name":"length","off":76,"type":"float","tsize":4,"ttag":"DW_TAG_base_type"},{"name":"collides","off":80,"type":"ObjPtrList","tsize":20,"ttag":"DW_TAG_class_type"},{"name":"radius","off":100,"type":"float","tsize":4,"ttag":"DW_TAG_base_type"},{"name":"outerRadius","off":104,"type":"float","tsize":4,"ttag":"DW_TAG_base_type"},{"name":"sideLength","off":108,"type":"float","tsize":4,"ttag":"DW_TAG_base_type"},{"name":"pose","off":112,"type":"Vector3","tsize":16,"ttag":"DW_TAG_class_type"},{"name":"collide","off":128,"type":"float","tsize":4,"ttag":"DW_TAG_base_type"},{"name":"sideCollide","off":132,"type":"bool","tsize":1,"ttag":"DW_TAG_base_type"}]},{"die":"0x40a80c","cu":"0x40a3eb","byte_size":64,"members":[{"name":"trans","off":0,"type":"ObjPtr","tsize":12,"ttag":"DW_TAG_class_type"},{"name":"pos","off":16,"type":"Vector3","tsize":16,"ttag":"DW_TAG_class_type"},{"name":"dist","off":32,"type":"float","tsize":4,"ttag":"DW_TAG_base_type"},{"name":"weight","off":36,"type":"float","tsize":4,"ttag":"DW_TAG_base_type"},{"name":"debugPos","off":48,"type":"Vector3","tsize":16,"ttag":"DW_TAG_class_type"}]},{"die":"0x4d7386","cu":"0x4d701a","byte_size":80,"members":[{"name":"v","off":0,"type":"Vector3","tsize":16,"ttag":"DW_TAG_class_type"},{"name":"c","off":16,"type":"Color","tsize":16,"ttag":"DW_TAG_class_type"},{"name":"cam","off":32,"type":"Vector3","tsize":16,"ttag":"DW_TAG_class_type"},{"name":"base","off":48,"type":"Vector2","tsize":8,"ttag":"DW_TAG_class_type"},{"name":"dir","off":56,"type":"Vector2","tsize":8,"ttag":"DW_TAG_class_type"},{"name":"delta","off":64,"type":"Vector2","tsize":8,"ttag":"DW_TAG_class_type"}]}],"LightParams_Spot":[{"die":"0x4a5f1f","cu":"0x4a5577","byte_size":112,"members":[{"name":"mDirection","off":0,"type":"Vector3","tsize":16,"ttag":"DW_TAG_class_type"},{"name":"mLightPos","off":16,"type":"Vector3","tsize":16,"ttag":"DW_TAG_class_type"},{"name":"mColor","off":32,"type":"Color","tsize":16,"ttag":"DW_TAG_class_type"},{"name":"mRange","off":48,"type":"float","tsize":4,"ttag":"DW_TAG_base_type"},{"name":"mRadiusTop","off":52,"type":"float","tsize":4,"ttag":"DW_TAG_base_type"},{"name":"mRadiusBottom","off":56,"type":"float","tsize":4,"ttag":"DW_TAG_base_type"},{"name":"mCachedData","off":64,"type":"CachedData","tsize":48,"ttag":"DW_TAG_class_type"}]}],"CachedData":[{"die":"0x4a5fc5","cu":"0x4a5577","byte_size":48,"members":[{"name":"mTipPosition","off":0,"type":"Vector3","tsize":16,"ttag":"DW_TAG_class_type"},{"name":"mTipOffsetSqr","off":16,"type":"float","tsize":4,"ttag":"DW_TAG_base_type"},{"name":"mCosTheta","off":20,"type":"float","tsize":4,"ttag":"DW_TAG_base_type"},{"name":"mOneOverOneSubCos","off":24,"type":"float","tsize":4,"ttag":"DW_TAG_base_type"},{"name":"mOneOverRange2x","off":28,"type":"float","tsize":4,"ttag":"DW_TAG_base_type"},{"name":"mTipOverRange2x","off":32,"type":"float","tsize":4,"ttag":"DW_TAG_base_type"}]}]}"""


class _FakeCI:
    def __init__(self, name, rows):
        from struct_db import Member  # type: ignore
        self.name = name
        self.members = [Member(name=r[0], type_str=r[1], offset=r[2],
                               line_number=0, raw_line="") for r in rows]


def selftest() -> int:
    probe = json.loads(_SELFTEST_PROBE_JSON)
    ok = True

    def check(label, name, ci, expect):
        nonlocal ok
        cand, _ = pick(probe[name], ci)
        got, ev = classify(ci.name, ci, cand, probe)
        good = got == expect
        ok = ok and good
        print(f"[{'OK ' if good else 'FAIL'}] {label:<40} expect={expect:<18} got={got}")
        return got, ev

    # 1) pre-retype RndLine::Point -> RETYPE_CANDIDATE with the exact mapping.
    tf = Path(tempfile.mkstemp(suffix=".h")[1])
    tf.write_text(_SELFTEST_PRE_POINT_HDR)
    try:
        cis = [c for c in parse_header(tf)
               if c.name.split("::")[-1] == "Point" and c.members]
    finally:
        tf.unlink(missing_ok=True)
    assert cis, "selftest: failed to parse embedded pre-retype Point header"
    got, ev = check("RndLine::Point (pre-retype)", "Point", cis[0], "RETYPE_CANDIDATE")
    mapping = {r["b5_member"]: r["our_members"] for r in ev["retype"]}
    expected = {"cam": ["unk0", "unk1", "unk2"], "base": ["unk3", "unk4"],
                "dir": ["unk5", "unk6"], "delta": ["unk7", "unk8"]}
    if mapping != expected:
        ok = False
        print(f"       FAIL mapping: got={mapping}\n                   want={expected}")
    else:
        print(f"       mapping OK: {mapping}")

    # 2) ALREADY_TYPED, 3) GENUINE_SCALARS, 4) DIVERGENT_CAUTION (real headers)
    check("CharIKFingers::FingerDesc", "FingerDesc",
          _FakeCI("CharIKFingers::FingerDesc", _SELFTEST_OURS["FingerDesc"]),
          "ALREADY_TYPED")
    check("BoxMapLighting::LightParams_Spot", "LightParams_Spot",
          _FakeCI("BoxMapLighting::LightParams_Spot", _SELFTEST_OURS["LightParams_Spot"]),
          "GENUINE_SCALARS")
    check("CharEyes", "CharEyes",
          _FakeCI("CharEyes", _SELFTEST_OURS["CharEyes"]),
          "DIVERGENT_CAUTION")

    print("\nselftest:", "PASS" if ok else "FAIL")
    return 0 if ok else 1


# ── main ───────────────────────────────────────────────────────────────────────


def main() -> int:
    ap = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--all", action="store_true",
                    help="include sdk/network/rndwii/os/lib headers")
    ap.add_argument("--filter", help="substring match on class name (case-sensitive)")
    ap.add_argument("--unit", help="restrict to a src/ subtree, e.g. system/char")
    ap.add_argument("--refresh", action="store_true",
                    help="rebuild the cached Bank-5 typed-layout registry")
    ap.add_argument("--min-run", type=int, default=3,
                    help="minimum consecutive our-side 4-byte scalars to count as "
                         "a scalar run (default 3)")
    ap.add_argument("--max-lines", type=int,
                    help="parse only the first N readelf lines (fast dev; not cached)")
    ap.add_argument("--json", metavar="FILE",
                    help="write ALL evaluated structs + full evidence to FILE")
    ap.add_argument("--selftest", action="store_true",
                    help="run the hermetic selftest (no ELF/git) and exit")
    args = ap.parse_args()

    if args.selftest:
        return selftest()

    registry = load_registry(args.refresh, args.max_lines)
    owner_pcts = load_owner_pcts()
    results = scan(registry, args, owner_pcts)
    render(results)

    if args.json:
        Path(args.json).write_text(json.dumps(results, indent=2))
        print(f"\nwrote {args.json} ({len(results)} structs)", file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main())
