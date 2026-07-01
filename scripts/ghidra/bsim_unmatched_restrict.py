#!/usr/bin/env python3
"""
bsim_unmatched_restrict.py — zero-Java-rebuild tuning sidestep for the ghidriff
BSim correlator stall (docs/decomp/ghidra-bsim-perf-investigation-2026-06-10.md
sections 5.1 + 5.4).

Standalone pyghidra/jpype helper module, written in the same conventions as
ghidriff/bsim.py.  It restricts the BSim correlator's source/destination address
sets so the O(n_src x n_dest) pair blowup operates on a smaller universe, and it
tunes the few thresholds that are actually exposed as VTOptions.

================================================================================
VERIFIED FACTS (read against /home/free/code/milohax/ghidra, Ghidra 12.2_DEV fork)
================================================================================

Option-name strings (BSimProgramCorrelatorFactory.java — use the class constants,
never retype the strings):

  MEMORY_MODEL                   = "Memory Model"                        default LSHMemoryModel.LARGE
  SEED_CONF_THRESHOLD            = "Confidence Threshold for a Seed"     default 10.0   (doc's guess: correct)
  IMPLICATION_THRESHOLD          = "Confidence Threshold for a Match"    default 0.0
  USE_ACCEPTED_MATCHES_AS_SEEDS  = "Use Accepted Matches as Seeds"       default True

NOT an option:
  BSimProgramCorrelator.SIMILARITY_THRESHOLD = 0.5 (BSimProgramCorrelator.java:62)
  is a `public static final double` with a constant initializer.  It is a Java
  compile-time constant, INLINED into doCorrelate's bytecode at javac time —
  it cannot be changed by reflection or jpype field writes.  Raising the
  similarity floor genuinely requires a Java rebuild (or driving the public
  BSimProgramCorrelatorMatching class directly with your own node generation,
  which means reimplementing the decompiler-signature phase — not done here).

Address sets DO pass through:
  BSimProgramCorrelatorFactory.getAddressRestrictionPreference() returns
  RESTRICTION_NOT_ALLOWED, but that enum only gates the VT *GUI wizard*.
  The programmatic path (VTAbstractProgramCorrelatorFactory.createCorrelator,
  VTAbstractProgramCorrelatorFactory.java:71-78) hands sourceAddressSet /
  destinationAddressSet to the correlator verbatim, and BSimProgramCorrelator
  uses them directly in generateNodes() ->
  Listing.getFunctions(addrSet, true)  — i.e. a function participates iff its
  ENTRY POINT lies inside the given address set.

ghidriff wiring (ghidriff/bsim.py — do NOT edit it; it already has the hooks):
  correlate_bsim(matches, p1, p2, p1_matches, p2_matches, monitor, logger=None,
                 seed_match_types=None, p1_addr_set=None, p2_addr_set=None,
                 enabled=True)
  p1_addr_set / p2_addr_set are accepted parameters (bsim.py:11) and default to
  the full loadedAndInitializedAddressSet (bsim.py:108-111).  So the address-set
  restriction needs NO monkeypatching: compute the sets with this module and
  pass them in.

  Options, however, CANNOT be injected: correlate_bsim builds
  `options = bsim_factory.createDefaultOptions()` internally (bsim.py:57) and
  never exposes it.  jpype Java classes are not monkeypatchable, so
  apply_perf_tuning() is only usable if you (a) drive
  BSimProgramCorrelatorFactory yourself in a custom script, or (b) vendor a
  one-line edit into a private copy of correlate_bsim:
      options = bsim_factory.createDefaultOptions()
      from bsim_unmatched_restrict import apply_perf_tuning   # <- add
      apply_perf_tuning(options)                              # <- add

================================================================================
RECALL ANALYSIS — why the naive "unmatched-only" restriction (doc section 5.4)
is HAZARDOUS, and what shape is safe
================================================================================

Two load-bearing mechanisms break if already-matched functions are simply
removed from the address sets:

1. The accepted-seed mechanism dies COMPLETELY.
   findAcceptedSeeds (BSimProgramCorrelatorMatching.java:558-595) turns an
   ACCEPTED VT association into a seed only if
       sourceNodes.get(srcAddr) != null      # FunctionNode exists in src set
   AND destNodes.get(dstAddr) != null        # FunctionNode exists in dst set
   AND sn.findEdge(dn) != null               # the pair was ALSO re-discovered
                                             # by binning + similarity >= 0.5
   Every accepted association is, by construction, a (matched-src, matched-dst)
   pair.  Excluding matched functions from EITHER side nulls one lookup for
   EVERY association -> zero accepted seeds.  This is exactly the mechanism
   ghidriff's bsim.py exists to feed (it injects SeedMatch/SymbolsHash/
   ExactBytes... matches as accepted associations).  BSim then falls back to
   chooseSeeds() self-discovery, which for a stripped cross-ISA binary is the
   weak path — and chooseSeeds even LOWERS confThreshold to the best available
   pair when nothing meets it (java:361-365), so low-quality self-seeds can
   propagate through the implication rounds.  Consequence: restricting "only
   the dest set" is NOT a safe middle ground — it kills accepted seeding just
   as dead as restricting both sides.

2. Call-graph edges through matched functions are silently severed.
   FunctionNodeContainer.generateCallGraph (FunctionNodeContainer.java:71-99)
   resolves each decompiler-reported call address through addrToNode (the node
   set = functions in the address set).  A call target with no node that is not
   a thunk hits `break` with kid == null and the edge is DROPPED — there is no
   stub vertex.  All NeighborGenerator fan-out (Children/Parents/GrandChildren/
   Siblings/Spouses) walks these parent/child sets, so any implication path
   running THROUGH an excluded matched function disappears.

Safe shape — "1-hop matched frontier" (build_frontier_addr_sets):
   keep every UNMATCHED function, plus every MATCHED function that is a direct
   caller or callee of an unmatched function (the frontier), lifted PAIRWISE
   (if either endpoint of a matched pair is frontier, keep both endpoints, so
   findAcceptedSeeds can resolve both sides).
   Why this is near-lossless: a generator path is only useful if it ends at an
   UNMATCHED relative (calculateBestNeighbor skips relatives with
   isAcceptedMatch).  On any useful 1- or 2-hop path anchor->R or
   anchor->Q->R with R unmatched, the intermediate Q is adjacent to R, hence
   frontier-or-unmatched, hence in-set; the edge Q-R and anchor-Q both survive.
   Interior matched functions (all 1-hop neighbors matched) are excluded, but
   any unmatched function 2 hops from such an interior anchor M via Q is still
   reached in round 0 from Q's OWN accepted seed (Q is frontier).  The only
   residual loss: cases where Q's pair fails the findEdge re-discovery (null
   vector / decompile timeout / cross-ISA similarity < 0.5) while M's would
   have succeeded — an edge case, not a structural loss.

Threshold tuning verdict (apply_perf_tuning):
   * SEED_CONF_THRESHOLD (10.0 -> 15.0) does NOT reduce the O(n x m) pair
     blowup: it is applied AFTER chooseSeeds' round loop, as a cutoff over the
     already-sorted finalPairs (java:366).  It only trims the seed list ->
     slightly less round-0 fan-out, higher seed precision.  The doc's section-5.1
     "2-5x fewer pairs" estimate belongs to SIMILARITY_THRESHOLD, which (see
     above) is not reachable without a Java rebuild.
   * IMPLICATION_THRESHOLD (0.0 -> e.g. 5.0) is the real exposed perf knob for
     the doMatching phase: it gates NeighborGenerator.searchForNewMatches pair
     creation (confidence < impThreshold -> skip) and terminates the greedy
     implication loop earlier (java:513).  It does nothing for the
     discoverPotentialMatches stall, which is upstream.
   => The address-set restriction is the ONLY zero-rebuild lever on the actual
      stall; the Tier-1 candidate cap (branch bsim-perf-candidatecap) remains
      the correct fix for the degenerate-bin case.

Expected speedup arithmetic (Bank8 40k vs Xenon 65k), pair-phase work ~ s1*s2:
   naive unmatched-only @70% matched (u1=12k, u2=20k):
       (12*20)/(40*65) = 0.092  -> ~10.8x  on discovery/aggregation/chooseSeeds
       plus ~3.3x on each decompile pass — but recall-HAZARDOUS (see above).
   frontier shape, frontier ~40% of matched: s1=23.2k, s2=38k -> 0.34 -> ~2.9x
   frontier shape, well-clustered match regions (~15%): -> 0.17 -> ~6x
   CAVEAT for this cross-ISA case: Xenon is stripped and differently compiled,
   so SymbolsHash/ExactBytes likely matched far LESS than 60-70%; the
   restriction win shrinks proportionally.  Measure with estimate_speedup().

Usage sketch (inside a pyghidra session, programs already open):

    from bsim_unmatched_restrict import (
        build_frontier_addr_sets, build_unmatched_addr_sets, estimate_speedup)
    from ghidriff.bsim import correlate_bsim

    p1_set, p2_set, stats = build_frontier_addr_sets(
        p1, p2, p1_matches, p2_matches,
        match_pairs=matches.keys(),    # pairwise frontier lift (recommended)
        logger=logger)
    logger.info(estimate_speedup(stats))
    correlate_bsim(matches, p1, p2, p1_matches, p2_matches, monitor,
                   logger=logger, p1_addr_set=p1_set, p2_addr_set=p2_set)

DO NOT use build_unmatched_addr_sets() (the literal doc-5.4 shape) unless you
explicitly accept running BSim with zero accepted seeds.
"""

from collections import namedtuple

RestrictStats = namedtuple(
    "RestrictStats",
    [
        "p1_total", "p1_kept", "p1_unmatched", "p1_frontier",
        "p2_total", "p2_kept", "p2_unmatched", "p2_frontier",
    ],
)


def _dummy_monitor():
    from ghidra.util.task import TaskMonitor
    return TaskMonitor.DUMMY


def _matched_offsets(p_matches):
    """ghidriff keeps p1_matches/p2_matches as sets of Ghidra Address objects
    (entry points).  Normalize to long offsets for robust membership tests."""
    return {int(a.getOffset()) for a in p_matches}


def _iter_real_functions(program):
    """All non-external functions.  Thunks are kept here: BSim's own
    minimumSizeFunctionFilter strips thunk bodies anyway, but a thunk's BODY in
    the set is harmless and its target matters for frontier discovery."""
    it = program.getFunctionManager().getFunctions(True)  # forward, non-external
    while it.hasNext():
        yield it.next()


def _addr_set_from_functions(funcs):
    """AddressSet covering each function's body.  BSim selects participants via
    Listing.getFunctions(addrSet, true) == entry-point-in-set; entry is always
    inside the body, and full bodies keep VTFunctionSizeUtil's range-delete
    semantics well-defined."""
    from ghidra.program.model.address import AddressSet
    s = AddressSet()
    for f in funcs:
        s.add(f.getBody())
    return s


def _neighbor_functions(func, monitor):
    """Direct callers + callees via the reference DB.  NOTE: BSim's own call
    graph comes from DECOMPILER call lists, so this is an approximation; it
    also resolves thunks one level so the frontier matches generateCallGraph's
    thunk-chasing behaviour."""
    out = set()
    for g in func.getCalledFunctions(monitor):
        if g.isThunk():
            t = g.getThunkedFunction(True)
            if t is not None:
                out.add(t)
        out.add(g)
    for g in func.getCallingFunctions(monitor):
        out.add(g)
    return out


def build_unmatched_addr_sets(p1, p2, p1_matches, p2_matches, logger=None):
    """The LITERAL doc-section-5.4 shape: keep only functions NOT already
    matched, on both sides.

    !!! RECALL HAZARD (see module docstring): this zeroes findAcceptedSeeds —
    every ghidriff-injected exact/seed match is dropped as a BSim seed, and
    call-graph fan-out through matched anchors is severed.  BSim degenerates to
    self-seeded chooseSeeds.  Maximum speed (~10x at 70%-matched), minimum
    recall.  Provided for A/B measurement, not for production use.

    Returns (p1_addr_set, p2_addr_set, RestrictStats).
    """
    stats = {}
    sets = []
    for tag, program, matched in (
            ("p1", p1, _matched_offsets(p1_matches)),
            ("p2", p2, _matched_offsets(p2_matches))):
        kept, total = [], 0
        for f in _iter_real_functions(program):
            total += 1
            if int(f.getEntryPoint().getOffset()) not in matched:
                kept.append(f)
        sets.append(_addr_set_from_functions(kept))
        stats[tag] = (total, len(kept))
        if logger:
            logger.info(
                "bsim restrict (NAIVE unmatched-only, recall-hazardous) %s: "
                "%d/%d functions kept", tag, len(kept), total)
    st = RestrictStats(
        p1_total=stats["p1"][0], p1_kept=stats["p1"][1],
        p1_unmatched=stats["p1"][1], p1_frontier=0,
        p2_total=stats["p2"][0], p2_kept=stats["p2"][1],
        p2_unmatched=stats["p2"][1], p2_frontier=0)
    return sets[0], sets[1], st


def build_frontier_addr_sets(p1, p2, p1_matches, p2_matches, match_pairs=None,
                             monitor=None, logger=None):
    """RECOMMENDED shape: unmatched functions + the 1-hop matched FRONTIER
    (matched functions that directly call or are called by an unmatched
    function), with pairwise lifting.

    match_pairs: iterable of (p1_addr, p2_addr) Ghidra Address tuples — in
    ghidriff this is `matches.keys()`.  When given, a matched pair is kept if
    EITHER endpoint is frontier, so findAcceptedSeeds can resolve BOTH sides
    (it needs a FunctionNode in each program).  Without it, a pair whose p1
    side is frontier but whose p2 side is interior would silently fail to seed.

    Cost: one callers+callees reference query per UNMATCHED function
    (~u1 + u2 queries).  For 12k+20k unmatched this is minutes, run it once.

    Returns (p1_addr_set, p2_addr_set, RestrictStats).
    """
    if monitor is None:
        monitor = _dummy_monitor()

    m1 = _matched_offsets(p1_matches)
    m2 = _matched_offsets(p2_matches)

    per_side = []  # [(kept_funcs, total, n_unmatched, frontier_offsets), ...]
    for program, matched in ((p1, m1), (p2, m2)):
        unmatched, matched_funcs_by_off = [], {}
        total = 0
        for f in _iter_real_functions(program):
            total += 1
            off = int(f.getEntryPoint().getOffset())
            if off in matched:
                matched_funcs_by_off[off] = f
            else:
                unmatched.append(f)

        frontier_offs = set()
        for f in unmatched:
            for g in _neighbor_functions(f, monitor):
                goff = int(g.getEntryPoint().getOffset())
                if goff in matched:
                    frontier_offs.add(goff)
        per_side.append(
            (unmatched, total, matched_funcs_by_off, frontier_offs))

    # Pairwise lift: if either endpoint of a matched pair is frontier,
    # keep both endpoints.
    f1, f2 = set(per_side[0][3]), set(per_side[1][3])
    if match_pairs is not None:
        for a1, a2 in match_pairs:
            o1, o2 = int(a1.getOffset()), int(a2.getOffset())
            if o1 in f1 or o2 in f2:
                f1.add(o1)
                f2.add(o2)
    elif logger:
        logger.warning(
            "bsim restrict: no match_pairs given — frontier computed per-side "
            "only; matched pairs whose other endpoint is interior will NOT "
            "seed (findAcceptedSeeds needs nodes on BOTH sides). Pass "
            "matches.keys().")

    results = []
    stats_flat = []
    for (unmatched, total, matched_funcs_by_off, _), fset, tag in (
            (per_side[0], f1, "p1"), (per_side[1], f2, "p2")):
        kept = list(unmatched)
        kept.extend(matched_funcs_by_off[o]
                    for o in fset if o in matched_funcs_by_off)
        results.append(_addr_set_from_functions(kept))
        stats_flat.extend([total, len(kept), len(unmatched), len(fset)])
        if logger:
            logger.info(
                "bsim restrict (frontier) %s: kept %d/%d "
                "(%d unmatched + %d matched-frontier anchors)",
                tag, len(kept), total, len(unmatched), len(fset))

    st = RestrictStats(*stats_flat)
    return results[0], results[1], st


def estimate_speedup(stats):
    """Back-of-envelope: the discoverPotentialMatches / aggregation /
    chooseSeeds phases scale ~ s1*s2 in the degenerate-bin regime; the
    decompile passes scale ~ s1 and ~ s2."""
    pair_factor = (stats.p1_kept * stats.p2_kept) / float(
        max(1, stats.p1_total * stats.p2_total))
    return (
        "pair-phase work factor %.3f (~%.1fx); decompile factors %.2fx / %.2fx"
        % (pair_factor,
           (1.0 / pair_factor) if pair_factor else float("inf"),
           stats.p1_total / float(max(1, stats.p1_kept)),
           stats.p2_total / float(max(1, stats.p2_kept))))


def apply_perf_tuning(options, seed_conf=15.0, implication_conf=None):
    """Set the VERIFIED tunable thresholds on a BSim VTOptions object.

    seed_conf: SEED_CONF_THRESHOLD ("Confidence Threshold for a Seed",
        default 10.0).  Raising to 15.0 trims low-confidence seeds AFTER
        chooseSeeds — improves seed precision, does NOT reduce the pair blowup.
        Per the option's own doc string, P(seed wrong) ~ 1/2^(N/5+9):
        10.0 -> ~1/2^11, 15.0 -> ~1/2^12.
    implication_conf: IMPLICATION_THRESHOLD ("Confidence Threshold for a
        Match", default 0.0).  If set (e.g. 5.0), gates neighbor-pair creation
        and stops the greedy implication loop earlier — the real exposed knob
        for doMatching cost.  Costs recall on weak-but-true implied matches.

    NOTE: ghidriff's correlate_bsim builds its options internally and offers no
    injection point; this helper is for custom drivers or a vendored
    correlate_bsim copy (see module docstring).  SIMILARITY_THRESHOLD (0.5) is
    a compile-time-inlined constant and CANNOT be tuned here.
    """
    from ghidra.feature.vt.api import BSimProgramCorrelatorFactory as F
    options.setDouble(F.SEED_CONF_THRESHOLD, float(seed_conf))
    if implication_conf is not None:
        options.setDouble(F.IMPLICATION_THRESHOLD, float(implication_conf))
    return options
