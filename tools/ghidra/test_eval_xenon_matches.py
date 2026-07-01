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
    _class_tokens_alias,
    _credit_aliased,
    categorize_tu,
    evaluate,
    is_agree,
    judge_agreement,
    parse_addr,
    parse_sweep,
    parse_wii_map_index,
    tu_stem,
    verdict_bucket,
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


# ---------------------------------------------------------------------------
# T4: platform-alias + arity-tolerance crediting
# ---------------------------------------------------------------------------
class TestPlatformAliasCrediting(unittest.TestCase):
    def test_class_token_alias_table(self):
        # explicit twins
        self.assertTrue(_class_tokens_alias("WiiMovie", "DxMovie"))
        self.assertTrue(_class_tokens_alias("BandCamShot", "HamCamShot"))
        # prefix-swap forms
        self.assertTrue(_class_tokens_alias("WiiPostProc", "NgPostProc"))
        self.assertTrue(_class_tokens_alias("BandSongMetadata", "HamSongMetadata"))
        self.assertTrue(_class_tokens_alias("WiiDOFProc", "NgDOFProc"))
        # identical
        self.assertTrue(_class_tokens_alias("Foo", "Foo"))
        # NOT aliases — genuinely different classes
        self.assertFalse(_class_tokens_alias("Foo", "Bar"))
        self.assertFalse(_class_tokens_alias("CamShot", "RndMovie"))

    def test_credit_aliased_platform_twin(self):
        # WiiMovie::SetTex(RndTex*) <-> DxMovie::SetTex(RndTex*) — same arity
        k1 = (("WiiMovie",), "SetTex", 1, False)
        k2 = (("DxMovie",), "SetTex", 1, False)
        self.assertEqual(_credit_aliased(k1, k2), "platform-alias")

    def test_credit_aliased_arity_tolerant(self):
        # QuatKeys::SetFrame: Wii (float,float)=2  vs  MSVC MMM (float,float,
        # float)=3 — the EXACT documented Fff/MMM arity case (forensics §1D V2)
        k1 = (("QuatKeys",), "SetFrame", 2, False)
        k2 = (("QuatKeys",), "SetFrame", 3, False)
        self.assertEqual(_credit_aliased(k1, k2), "arity-tolerant")

    def test_credit_aliased_both(self):
        k1 = (("BandCamShot",), "Foo", 1, False)
        k2 = (("HamCamShot",), "Foo", 2, False)
        self.assertEqual(_credit_aliased(k1, k2), "platform-alias+arity-tolerant")

    def test_credit_aliased_rejects_different_template_instantiation(self):
        # KeylessHash<char,char> vs KeylessHash<AllocInfo,void> — NOT a rename;
        # the eval must keep this 'disagree'.
        k1 = (("KeylessHash<*,*,char,char,const,const>",), "#ctor", 4, False)
        k2 = (("KeylessHash<*,*,AllocInfo,void>",), "#ctor", 4, False)
        self.assertIsNone(_credit_aliased(k1, k2))

    def test_credit_aliased_rejects_large_arity_gap(self):
        k1 = (("Foo",), "Bar", 1, False)
        k2 = (("Foo",), "Bar", 5, False)
        self.assertIsNone(_credit_aliased(k1, k2))

    def test_credit_aliased_rejects_constness_mismatch(self):
        k1 = (("Foo",), "Bar", 1, False)
        k2 = (("Foo",), "Bar", 1, True)
        self.assertIsNone(_credit_aliased(k1, k2))

    def test_credit_aliased_rejects_method_mismatch(self):
        k1 = (("WiiMovie",), "SetTex", 1, False)
        k2 = (("DxMovie",), "GetTex", 1, False)
        self.assertIsNone(_credit_aliased(k1, k2))

    def test_judge_agreement_gated_off_by_default(self):
        # Wii QuatKeys::SetFrame(float,float) vs DC3 (float,float,float): with
        # crediting OFF (the default) this stays 'disagree'.
        wd = {"SetFrame__8QuatKeysFff": "QuatKeys::SetFrame(float, float)"}
        md = {"?SetFrame@QuatKeys@@UAAXMMM@Z":
              "public: virtual void __cdecl QuatKeys::SetFrame(float, float, float)"}
        v, _ = judge_agreement("SetFrame__8QuatKeysFff",
                               "?SetFrame@QuatKeys@@UAAXMMM@Z", wd, md)
        self.assertEqual(v, "disagree")
        # ...and 'agree (arity-tolerant)' when credited.
        v2, _ = judge_agreement("SetFrame__8QuatKeysFff",
                                "?SetFrame@QuatKeys@@UAAXMMM@Z", wd, md,
                                credit_aliases=True)
        self.assertEqual(v2, "agree (arity-tolerant)")
        self.assertTrue(is_agree(v2))

    def test_judge_agreement_platform_alias_credited(self):
        wd = {"SetTex__8WiiMovieFP6RndTex": "WiiMovie::SetTex(RndTex*)"}
        md = {"?SetTex@DxMovie@@UAAXPAVRndTex@@@Z":
              "public: virtual void __cdecl DxMovie::SetTex(RndTex*)"}
        v, _ = judge_agreement("SetTex__8WiiMovieFP6RndTex",
                               "?SetTex@DxMovie@@UAAXPAVRndTex@@@Z", wd, md,
                               credit_aliases=True)
        self.assertEqual(v, "agree (platform-alias)")

    def test_verdict_bucket(self):
        self.assertEqual(verdict_bucket("agree"), "agree")
        self.assertEqual(verdict_bucket("agree (platform-alias)"), "agree_alias")
        self.assertEqual(verdict_bucket("agree (arity-tolerant)"), "agree_alias")
        self.assertEqual(verdict_bucket("disagree"), "disagree")
        self.assertEqual(verdict_bucket("unjudgeable"), "unjudgeable")


# ---------------------------------------------------------------------------
# T4: exclude-match-types, optional scores field, min-vt-score, low-trust-stub
# ---------------------------------------------------------------------------
# A focused fixture: a Wii map with one Mesh fn, and matches that carry the
# optional `scores` field for the min-vt-score path.
SCORE_MAP = (
    ".text section layout\n"
    f"  00000000 000100 80010000 00010000 16 A__7RndMeshFv \t{SYSTEM_TU} \n"   # 256B
    f"  00000100 000040 80010100 00010100 16 B__7RndMeshFv \t{SYSTEM_TU} \n"   # 64B (stub-ish)
    f"  00000200 000100 80010200 00010200 16 C__7RndMeshFv \t{SYSTEM_TU} \n"
)


def _score_map(tmpdir: Path) -> Path:
    p = tmpdir / "score.map"
    p.write_text(SCORE_MAP)
    return p


class TestExcludeAndScoreFilters(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        with tempfile.TemporaryDirectory() as td:
            cls.idx = parse_wii_map_index(_score_map(Path(td)))

    def test_map_carries_size(self):
        self.assertEqual(self.idx[0x80010000]["size"], 0x100)
        self.assertEqual(self.idx[0x80010100]["size"], 0x40)

    def test_exclude_match_types_drops_pure_noise_pairs(self):
        fm = [
            {"p1_addr": "80010000", "p2_addr": "82000000",
             "match_types": ["StringsRefsHasher"]},                  # dropped
            {"p1_addr": "80010100", "p2_addr": "82000100",
             "match_types": ["VTCombinedReference", "StringsRefsHasher"]},  # survives via VT
            {"p1_addr": "80010200", "p2_addr": "82000200",
             "match_types": ["VTCombinedReference"]},                # survives
        ]
        rep = evaluate(fm, [], [], [], self.idx,
                       exclude_match_types={"StringsRefsHasher"})
        self.assertEqual(rep["totals"]["scored_pairs"], 2)
        self.assertEqual(rep["totals"]["filtered_excluded_match_types"], 1)
        # the surviving mixed pair lost the excluded type
        nc = {r["xenon_addr"]: r for r in rep["lists"]["new_coverage"]}
        self.assertEqual(nc["0x82000100"]["match_types"], ["VTCombinedReference"])

    def test_scores_field_optional_backward_compat(self):
        # No `scores` field + a min_vt_score floor => filter inactive (the pair
        # is KEPT). This is the old-artifact backward-compat guarantee.
        fm = [{"p1_addr": "80010000", "p2_addr": "82000000",
               "match_types": ["VTCombinedReference"]}]
        rep = evaluate(fm, [], [], [], self.idx, min_vt_score=99.0)
        self.assertEqual(rep["totals"]["scored_pairs"], 1)
        self.assertEqual(rep["totals"]["vt_below_min_score_culled"], 0)

    def test_min_vt_score_culls_when_scores_present(self):
        fm = [
            {"p1_addr": "80010000", "p2_addr": "82000000",
             "match_types": ["VTCombinedReference"],
             "scores": {"VTCombinedReference": {"product": 8.0}}},     # below 9.5 -> culled
            {"p1_addr": "80010100", "p2_addr": "82000100",
             "match_types": ["VTCombinedReference"],
             "scores": {"VTCombinedReference": {"product": 12.0}}},    # above -> kept
            {"p1_addr": "80010200", "p2_addr": "82000200",
             "match_types": ["VTCombinedReference", "Implied Match"],
             "scores": {"VTCombinedReference": {"product": 1.0}}},     # VT culled, pair survives via Implied
        ]
        rep = evaluate(fm, [], [], [], self.idx, min_vt_score=9.5)
        self.assertEqual(rep["totals"]["vt_below_min_score_culled"], 2)
        # pair 0 dropped entirely (VT was its only type); pairs 1 & 2 survive
        self.assertEqual(rep["totals"]["scored_pairs"], 2)
        nc = {r["xenon_addr"]: r for r in rep["lists"]["new_coverage"]}
        self.assertNotIn("0x82000000", nc)
        self.assertEqual(nc["0x82000200"]["match_types"], ["Implied Match"])

    def test_low_trust_stub_buckets_tiny_disagrees(self):
        # A bindiff DISAGREE on a 64-byte Wii fn => low-trust (not hard-wrong).
        wd = {"B__7RndMeshFv": "RndMesh::B(void)"}
        md = {"?Z@Other@@QAAXXZ": "public: void __cdecl Other::Z(void)"}
        fm = [{"p1_addr": "80010100", "p2_addr": "82000100",
               "match_types": ["VTCombinedReference"]}]
        bindiff = [{"rb3_addr": "0x82000100", "dc3_name": "?Z@Other@@QAAXXZ",
                    "similarity": 1.0, "confidence": 0.99}]
        rep = evaluate(fm, [], [], bindiff, self.idx, wd, md,
                       low_trust_stub=True, stub_size_max=88)
        d = rep["dc3_agreement"]["high_conf"]
        self.assertEqual(d["low_trust_stub"], 1)
        self.assertEqual(d["disagree"], 0)         # NOT counted as hard-wrong
        # and it is excluded from the judged precision proxy
        self.assertNotIn("VTCombinedReference", rep["precision_by_match_type"])

    def test_stratify_emits_per_category(self):
        fm = [{"p1_addr": "80010000", "p2_addr": "82000000",
               "match_types": ["VTCombinedReference"]}]
        # make 0x82000000 a holdout that matches correctly (system/Mesh stem)
        holdout = [{"addr": "0x82000000", "stem": "Mesh"}]
        rep = evaluate(fm, [], holdout, [], self.idx, stratify=True)
        self.assertIn("precision_by_match_type_by_category", rep)
        self.assertIn("system", rep["precision_by_match_type_by_category"])


# ---------------------------------------------------------------------------
# T3 (round 2): exact-addr holdout scoring + known-negatives oracle
# ---------------------------------------------------------------------------
class TestExactAddrHoldout(unittest.TestCase):
    """A holdout entry carrying `wii_addr_bank8` is scored by EXACT Wii p1
    agreement, not the TU-stem heuristic. The original 146 (no wii_addr_bank8)
    keep the stem path and stay byte-identical."""

    @classmethod
    def setUpClass(cls):
        with tempfile.TemporaryDirectory() as td:
            cls.idx = parse_wii_map_index(write_mini_map(Path(td)))

    def test_exact_addr_correct(self):
        # round-2 holdout: xenon 0x82002000 must map to Wii 0x80010000 exactly.
        holdout = [{"addr": "0x82002000", "stem": "Game",
                    "wii_addr_bank8": "0x80010000",
                    "wii_symbol": "GameFn__4GameFv",
                    "source": "judged-round2-correct"}]
        fm = [{"p1_addr": "80010000", "p2_addr": "82002000",
               "match_types": ["BSIM"]}]
        rep = evaluate(fm, [], holdout, [], self.idx)
        h = rep["holdout_recovery"]
        self.assertEqual(h["recovered_correct"], 1)
        self.assertEqual(h["recovered_wrong"], 0)
        rows = {r["xenon_addr"]: r for r in rep["lists"]["holdout"]}
        self.assertEqual(rows["0x82002000"]["score_mode"], "exact")

    def test_exact_addr_wrong_even_when_stem_agrees(self):
        # The matched Wii fn (NewFn__4GameFv @0x80010700) is ALSO Game.o, so the
        # stem heuristic would call it CORRECT — but the exact addr differs, so
        # exact scoring correctly marks it WRONG. This is the strength of exact.
        holdout = [{"addr": "0x82002000", "stem": "Game",
                    "wii_addr_bank8": "0x80010000",
                    "wii_symbol": "GameFn__4GameFv",
                    "source": "judged-round2-correct"}]
        fm = [{"p1_addr": "80010700", "p2_addr": "82002000",   # different Game.o fn
               "match_types": ["BSIM"]}]
        rep = evaluate(fm, [], holdout, [], self.idx)
        h = rep["holdout_recovery"]
        self.assertEqual(h["recovered_correct"], 0)
        self.assertEqual(h["recovered_wrong"], 1)

    def test_stem_path_unchanged_without_field(self):
        # No wii_addr_bank8 => legacy stem scoring, and NO score_mode key in the
        # row (byte-identical to the legacy report).
        holdout = [{"addr": "0x82002000", "stem": "Game"}]
        fm = [{"p1_addr": "80010000", "p2_addr": "82002000",
               "match_types": ["BSIM"]}]
        rep = evaluate(fm, [], holdout, [], self.idx)
        self.assertEqual(rep["holdout_recovery"]["recovered_correct"], 1)
        row = rep["lists"]["holdout"][0]
        self.assertNotIn("score_mode", row)

    def test_exact_addr_unmatched(self):
        holdout = [{"addr": "0x82002000", "stem": "Game",
                    "wii_addr_bank8": "0x80010000",
                    "wii_symbol": "GameFn__4GameFv",
                    "source": "judged-round2-correct"}]
        rep = evaluate([], [], holdout, [], self.idx)
        h = rep["holdout_recovery"]
        self.assertEqual(h["unmatched"], 1)
        # exact entries are always scoreable (a known Wii addr exists)
        self.assertEqual(h["scoreable"], 1)


class TestKnownNegatives(unittest.TestCase):
    """A scored match recurring an EXACT judged-WRONG (p2,p1) pair counts WRONG;
    the same p2 matched to a DIFFERENT p1 is NOT penalized."""

    @classmethod
    def setUpClass(cls):
        with tempfile.TemporaryDirectory() as td:
            cls.idx = parse_wii_map_index(write_mini_map(Path(td)))

    def test_exact_pair_recurrence_flagged_wrong(self):
        kn = [{"xenon_addr": "0x82006000", "wii_addr_bank8": "0x80010700",
               "wii_symbol": "NewFn__4GameFv", "source": "judged-round2-wrong"}]
        fm = [{"p1_addr": "80010700", "p2_addr": "82006000",   # the WRONG pair
               "match_types": ["BSIM"]}]
        rep = evaluate(fm, [], [], [], self.idx, known_negatives=kn)
        self.assertIn("known_negatives", rep)
        self.assertEqual(rep["known_negatives"]["recurred_exact"], 1)
        # it is counted WRONG in per-type precision
        self.assertEqual(rep["precision_by_match_type"]["BSIM"]["wrong"], 1)
        self.assertEqual(rep["precision_by_match_type"]["BSIM"]["correct"], 0)
        recs = rep["lists"]["known_negative_recurrences"]
        self.assertEqual(recs[0]["xenon_addr"], "0x82006000")
        self.assertEqual(recs[0]["wii_addr"], "0x80010700")

    def test_same_p2_different_p1_not_flagged(self):
        # Known-negative says (0x82006000 -> 0x80010700) is wrong. A match of the
        # SAME p2 to a DIFFERENT p1 (0x80010000) must NOT be penalized — it may
        # be the true identity.
        kn = [{"xenon_addr": "0x82006000", "wii_addr_bank8": "0x80010700",
               "wii_symbol": "NewFn__4GameFv", "source": "judged-round2-wrong"}]
        fm = [{"p1_addr": "80010000", "p2_addr": "82006000",   # DIFFERENT p1
               "match_types": ["BSIM"]}]
        rep = evaluate(fm, [], [], [], self.idx, known_negatives=kn)
        self.assertEqual(rep["known_negatives"]["recurred_exact"], 0)
        # not in the judged set at all (no holdout/bindiff), so no precision row
        self.assertNotIn("BSIM", rep["precision_by_match_type"])
        self.assertEqual(rep["lists"]["known_negative_recurrences"], [])

    def test_absent_oracle_byte_identical(self):
        # No known_negatives => no 'known_negatives' section, no extra list key.
        fm = [{"p1_addr": "80010700", "p2_addr": "82006000",
               "match_types": ["BSIM"]}]
        rep_off = evaluate(fm, [], [], [], self.idx)
        rep_empty = evaluate(fm, [], [], [], self.idx, known_negatives=[])
        self.assertNotIn("known_negatives", rep_off)
        self.assertNotIn("known_negatives", rep_empty)
        self.assertNotIn("known_negative_recurrences", rep_off["lists"])


class TestNoRegressionReplay(unittest.TestCase):
    """The default report (new flags off) is byte-identical to one produced with
    the new parameters explicitly disabled — guards round-1 T4 discipline."""

    @classmethod
    def setUpClass(cls):
        with tempfile.TemporaryDirectory() as td:
            cls.idx = parse_wii_map_index(write_mini_map(Path(td)))

    def test_default_equals_explicit_off(self):
        import json as _json
        base = evaluate(FUNCTION_MATCHES, SEEDS, HOLDOUT, BINDIFF,
                        self.idx, WII_DEM, MSVC_DEM)
        explicit = evaluate(FUNCTION_MATCHES, SEEDS, HOLDOUT, BINDIFF,
                            self.idx, WII_DEM, MSVC_DEM,
                            known_negatives=None)
        self.assertEqual(_json.dumps(base, sort_keys=True),
                         _json.dumps(explicit, sort_keys=True))


class TestParseSweep(unittest.TestCase):
    def test_empty(self):
        self.assertIsNone(parse_sweep(""))

    def test_comma_list(self):
        self.assertEqual(parse_sweep("9.5,11,13"), [9.5, 11.0, 13.0])

    def test_range(self):
        self.assertEqual(parse_sweep("9.5:12:1.25"), [9.5, 10.75, 12.0])


if __name__ == "__main__":
    unittest.main(verbosity=2)
