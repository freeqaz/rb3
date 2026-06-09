#!/usr/bin/env python3
"""Unit tests for eval_xenon_matches.py joining/metric logic.

Synthetic fixtures only — no Ghidra, no demangler binaries (demangled maps are
injected). Run:
    python3 tools/ghidra/test_eval_xenon_matches.py
"""
from __future__ import annotations

import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from eval_xenon_matches import (  # noqa: E402
    categorize_tu,
    evaluate,
    judge_agreement,
    parse_addr,
    parse_wii_map_index,
    tu_stem,
)

BAND3_TU = "lib.a C:\\hproj\\band3_wii\\band3\\src\\game\\wii_release\\Game.o"
BAND3_TU2 = "lib.a C:\\hproj\\band3_wii\\band3\\src\\game\\wii_release\\GamePanel.o"
ACCOMP_TU = "lib.a C:\\hproj\\band3_wii\\band3\\src\\meta_band\\wii_release\\Accomplishment.o"
SYSTEM_TU = "lib.a C:\\hproj\\band3_wii\\system\\src\\rndobj\\wii_release\\Mesh.o"
SYSTEM_TU2 = "lib.a C:\\hproj\\band3_wii\\system\\src\\obj\\wii_release\\Object.o"

MINI_MAP = (
    ".text section layout\n"
    "  Starting        Virtual  File\n"
    "  address  Size   address  offset\n"
    "  ---------------------------------\n"
    "  00000000 002fc4 80010000 00010000  1 .text \tApp.o \n"          # section row: skipped
    f"  00000000 000100 80010000 00010000 16 GameFn__4GameFv \t{BAND3_TU} \n"
    f"  00000100 000100 80010100 00010100 16 SysFn__7RndMeshFv \t{SYSTEM_TU} \n"
    "  00000200 000100 80010200 00010200 16 AppFn__3AppFv \tApp.o \n"
    "  00000300 000100 80010300 00010300 16 OSReport \tos.a OS.o \n"
    f"  00000400 000100 80010400 00010400 16 X__3FooFv \t{ACCOMP_TU} \n"
    f"  00000500 000100 80010500 00010500 16 Y__3BarFv \t{SYSTEM_TU2} \n"
    "  00000600 000000 80010600 00010600 16 ZeroSize__Fv \tApp.o \n"   # size 0: skipped
    f"  00000700 000100 80010700 00010700 16 NewFn__4GameFv \t{BAND3_TU2} \n"
    f"  00000800 000100 80010800 00010800 16 SysNew__7RndMeshFv \t{SYSTEM_TU} \n"
    "  00000900 000100 80010900 00010900 16 AppFn2__3AppFv \tApp.o \n"
    f"  00000a00 000100 80010a00 00010a00 16 W__3QuxFv \t{SYSTEM_TU2} \n"
    f"  00000b00 000100 80010b00 00010b00 16 SysFn2__7RndMeshFv \t{SYSTEM_TU} \n"
    ".data section layout\n"
    "  00000000 000100 80a00000 00900000 16 NotCode__Fv \tApp.o \n"    # not .text: skipped
)


def write_mini_map(tmpdir: Path) -> Path:
    p = tmpdir / "mini.map"
    p.write_text(MINI_MAP)
    return p


# Injected demangle fixtures (what the real batch demanglers would return).
WII_DEM = {
    "X__3FooFv": "Foo::X(void)",
    "Y__3BarFv": "Bar::Y(void)",
    "W__3QuxFv": "Qux::W(void)",
}
MSVC_DEM = {
    "?X@Foo@@QAAXXZ": "public: void __cdecl Foo::X(void)",
    "?Z@Baz@@QAAXXZ": "public: void __cdecl Baz::Z(void)",
    "?W@Qux@@QAAXXZ": "public: void __cdecl Qux::W(void)",
}

SEEDS = [{"p1_addr": "0x80010200", "p2_addr": "0x82990000"}]

HOLDOUT = [
    {"addr": "0x82001000", "stem": "Accomplishment"},  # matched to a Mesh.o fn -> WRONG
    {"addr": "0x82002000", "stem": "Game"},            # matched to Game.o fn -> correct
    {"addr": "0x82003000", "stem": "Mesh"},            # never matched
    {"addr": "0x82990000", "stem": "App"},             # was a seed -> excluded
    # Xenon-only TU: stem exists in NO Wii map TU -> can never TU-agree;
    # matched below, but must be excluded from correct/wrong scoring
    {"addr": "0x8200c000", "stem": "XenonOnlyFile"},
]

BINDIFF = [
    {"rb3_addr": "0x82004000", "dc3_name": "?X@Foo@@QAAXXZ",
     "similarity": 1.0, "confidence": 0.99},   # high-conf, agree
    {"rb3_addr": "0x82005000", "dc3_name": "?Z@Baz@@QAAXXZ",
     "similarity": 1.0, "confidence": 0.99},   # high-conf, DISAGREE (make-believe)
    {"rb3_addr": "0x8200a000", "dc3_name": "?W@Qux@@QAAXXZ",
     "similarity": 0.8, "confidence": 0.5},    # low-conf, agree (all-entries only)
]

FUNCTION_MATCHES = [
    # seed (a given) — excluded everywhere
    {"p1_addr": "80010200", "p2_addr": "82990000", "match_types": ["SeedMatch"],
     "p1_name": "AppFn__3AppFv", "p2_name": "FUN_82990000"},
    # (a) holdout: correct (Game.o stem == 'Game')
    {"p1_addr": "80010000", "p2_addr": "82002000",
     "match_types": ["VTCombinedReference"]},
    # (a) holdout: WRONG (Mesh.o stem != 'Accomplishment')
    {"p1_addr": "80010100", "p2_addr": "82001000", "match_types": ["Implied Match"]},
    # (b) bindiff agree ('ram:' prefix exercises addr parsing)
    {"p1_addr": "ram:80010400", "p2_addr": "0x82004000", "match_types": ["BSIM"]},
    # (b) bindiff disagree
    {"p1_addr": "80010500", "p2_addr": "82005000",
     "match_types": ["BSIM", "StringsRefsHasher"]},
    # (b) low-conf bindiff agree — counts in all_entries, not high_conf/(d)
    {"p1_addr": "80010a00", "p2_addr": "8200a000", "match_types": ["Implied Match"]},
    # (c) new coverage: band3 game code
    {"p1_addr": "80010700", "p2_addr": "82006000",
     "match_types": ["VTCombinedReference"]},
    # (c) new coverage: system
    {"p1_addr": "80010800", "p2_addr": "82007000", "match_types": ["Implied Match"]},
    # (c) new coverage: main (game code)
    {"p1_addr": "80010900", "p2_addr": "82009000", "match_types": ["BSIM"]},
    # (c) suspect: SymbolsHash-only -> excluded from new coverage, listed
    {"p1_addr": "80010300", "p2_addr": "82008000", "match_types": ["SymbolsHash"]},
    # seed conflict: non-seed claim re-using the seeded p2
    {"p1_addr": "80010600", "p2_addr": "82990000", "match_types": ["SymbolsHash"]},
    # bad address
    {"p1_addr": "garbage", "p2_addr": "8200b000", "match_types": ["BSIM"]},
    # holdout with a Xenon-only stem: matched, but unscoreable
    {"p1_addr": "80010b00", "p2_addr": "8200c000", "match_types": ["BSIM"]},
]


class TestHelpers(unittest.TestCase):
    def test_parse_addr(self):
        self.assertEqual(parse_addr("0x82260018"), 0x82260018)
        self.assertEqual(parse_addr("82260018"), 0x82260018)
        self.assertEqual(parse_addr("ram:80004000"), 0x80004000)
        self.assertEqual(parse_addr(0x80004000), 0x80004000)
        self.assertIsNone(parse_addr("garbage"))
        self.assertIsNone(parse_addr(None))

    def test_categorize_tu(self):
        self.assertEqual(categorize_tu(BAND3_TU), ("Game.o", "band3"))
        self.assertEqual(categorize_tu(SYSTEM_TU), ("Mesh.o", "system"))
        self.assertEqual(
            categorize_tu("lib.a C:\\hproj\\band3_wii\\network\\src\\Core\\wii_release\\Net.o"),
            ("Net.o", "network"))
        self.assertEqual(categorize_tu("App.o "), ("App.o", "main"))
        self.assertEqual(categorize_tu("os.a OS.o"), ("OS.o", "sdk"))
        self.assertEqual(categorize_tu("Linker Generated Symbol File"), (None, None))
        self.assertEqual(tu_stem("Game.o"), "Game")

    def test_parse_wii_map_index(self):
        with tempfile.TemporaryDirectory() as td:
            idx = parse_wii_map_index(write_mini_map(Path(td)))
        self.assertEqual(idx[0x80010000]["symbol"], "GameFn__4GameFv")
        self.assertEqual(idx[0x80010000]["category"], "band3")
        self.assertEqual(idx[0x80010100]["category"], "system")
        self.assertEqual(idx[0x80010200]["category"], "main")
        self.assertEqual(idx[0x80010300]["category"], "sdk")
        self.assertNotIn(0x80010600, idx)       # zero-size dropped
        self.assertNotIn(0x80A00000, idx)       # .data dropped
        # first symbol at 0x80010000 wins over the '.text' section row (skipped anyway)
        self.assertEqual(idx[0x80010000]["tu"], "Game.o")


class TestJudgeAgreement(unittest.TestCase):
    def test_mangled_agree(self):
        v, _ = judge_agreement("X__3FooFv", "?X@Foo@@QAAXXZ", WII_DEM, MSVC_DEM)
        self.assertEqual(v, "agree")

    def test_mangled_disagree(self):
        v, _ = judge_agreement("Y__3BarFv", "?Z@Baz@@QAAXXZ", WII_DEM, MSVC_DEM)
        self.assertEqual(v, "disagree")

    def test_plain_agree(self):
        v, _ = judge_agreement("OSReport", "OSReport", {}, {})
        self.assertEqual(v, "agree")

    def test_plain_crt_blocklist(self):
        v, d = judge_agreement("memcpy", "memcpy", {}, {})
        self.assertEqual(v, "unjudgeable")
        self.assertEqual(d, "crt_blocklist_name")

    def test_plain_disagree(self):
        v, _ = judge_agreement("OSReport", "DbgPrint", {}, {})
        self.assertEqual(v, "disagree")

    def test_mangled_vs_plain(self):
        v, _ = judge_agreement("X__3FooFv", "DbgPrint", WII_DEM, {})
        self.assertEqual(v, "unjudgeable")

    def test_undemangleable(self):
        v, d = judge_agreement("Nope__3ZzzFv", "?X@Foo@@QAAXXZ", {}, MSVC_DEM)
        self.assertEqual(v, "unjudgeable")
        self.assertEqual(d, "wii_undemangleable")


class TestEvaluate(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        with tempfile.TemporaryDirectory() as td:
            wii_index = parse_wii_map_index(write_mini_map(Path(td)))
        cls.rep = evaluate(FUNCTION_MATCHES, SEEDS, HOLDOUT, BINDIFF,
                           wii_index, WII_DEM, MSVC_DEM)

    def test_totals(self):
        t = self.rep["totals"]
        self.assertEqual(t["function_matches"], len(FUNCTION_MATCHES))
        self.assertEqual(t["seed_pairs_in_output"], 1)
        self.assertEqual(t["seed_conflicts"], 1)
        self.assertEqual(t["bad_addresses"], 1)
        # 13 - seed(1) - conflict(1) - bad(1) = 10 scored
        self.assertEqual(t["scored_pairs"], 10)

    def test_holdout_recovery(self):
        h = self.rep["holdout_recovery"]
        self.assertEqual(h["eligible"], 4)
        self.assertEqual(h["excluded_as_seeds"], 1)
        self.assertEqual(h["scoreable"], 3)
        self.assertEqual(h["recovered_correct"], 1)
        self.assertEqual(h["recovered_wrong"], 1)
        self.assertEqual(h["unmatched"], 1)
        self.assertEqual(h["recovered_unscoreable_stem"], 1)
        self.assertAlmostEqual(h["recovery_rate"], 1 / 3)
        self.assertAlmostEqual(h["precision_on_recovered"], 0.5)
        rows = {r["xenon_addr"]: r for r in self.rep["lists"]["holdout"]}
        self.assertEqual(rows["0x82002000"]["result"], "recovered_correct")
        self.assertEqual(rows["0x82002000"]["wii_symbol"], "GameFn__4GameFv")
        self.assertEqual(rows["0x82001000"]["result"], "recovered_wrong")
        self.assertEqual(rows["0x82003000"]["result"], "unmatched")
        self.assertEqual(rows["0x8200c000"]["result"], "recovered_unscoreable_stem")

    def test_dc3_agreement(self):
        d = self.rep["dc3_agreement"]
        self.assertEqual(d["matched_in_bindiff"], 3)
        self.assertEqual(d["all_entries"]["agree"], 2)       # high-conf agree + low-conf agree
        self.assertEqual(d["all_entries"]["disagree"], 1)
        self.assertAlmostEqual(d["all_entries"]["agreement_rate"], 2 / 3)
        self.assertEqual(d["high_conf"]["agree"], 1)
        self.assertEqual(d["high_conf"]["disagree"], 1)
        self.assertAlmostEqual(d["high_conf"]["agreement_rate"], 0.5)
        rows = {r["xenon_addr"]: r for r in self.rep["lists"]["dc3"]}
        self.assertEqual(rows["0x82004000"]["verdict"], "agree")
        self.assertEqual(rows["0x82005000"]["verdict"], "disagree")
        self.assertFalse(rows["0x8200a000"]["high_conf"])

    def test_new_coverage(self):
        n = self.rep["new_coverage"]
        self.assertEqual(n["total"], 3)
        self.assertEqual(n["game_code"], 2)                  # band3 + main
        self.assertEqual(n["by_category"],
                         {"band3": 1, "system": 1, "main": 1})
        self.assertEqual(n["by_match_type"],
                         {"VTCombinedReference": 1, "Implied Match": 1, "BSIM": 1})
        self.assertEqual(n["suspect_symbolshash_only"], 1)
        suspects = self.rep["lists"]["suspect_symbolshash_only"]
        self.assertEqual(suspects[0]["xenon_addr"], "0x82008000")
        new_addrs = {r["xenon_addr"] for r in self.rep["lists"]["new_coverage"]}
        self.assertEqual(new_addrs, {"0x82006000", "0x82007000", "0x82009000"})

    def test_precision_by_match_type(self):
        p = self.rep["precision_by_match_type"]
        # judged: holdout-correct (VTCombinedReference), holdout-wrong (Implied),
        # bindiff-agree (BSIM), bindiff-disagree (BSIM+StringsRefs). Low-conf NOT judged.
        self.assertEqual(p["OVERALL"]["judged"], 4)
        self.assertEqual(p["OVERALL"]["correct"], 2)
        self.assertAlmostEqual(p["OVERALL"]["precision"], 0.5)
        self.assertEqual(p["VTCombinedReference"],
                         {"judged": 1, "correct": 1, "wrong": 0, "precision": 1.0})
        self.assertEqual(p["Implied Match"],
                         {"judged": 1, "correct": 0, "wrong": 1, "precision": 0.0})
        self.assertEqual(p["BSIM"]["judged"], 2)
        self.assertEqual(p["BSIM"]["correct"], 1)
        self.assertEqual(p["StringsRefsHasher"],
                         {"judged": 1, "correct": 0, "wrong": 1, "precision": 0.0})

    def test_seed_conflict_listed(self):
        sc = self.rep["lists"]["seed_conflicts"]
        self.assertEqual(len(sc), 1)
        self.assertEqual(sc[0]["p2_addr"], "0x82990000")


if __name__ == "__main__":
    unittest.main(verbosity=2)
