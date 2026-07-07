#!/usr/bin/env python3
"""HANDS-ADJUDICATION: numeric closure of the 87.3deg Tier-1 reading from
COMMITTED evidence alone (no runtime).

Inputs (hardcoded below, transcribed from committed logs):
 - AUTHORED per-bone inverse-bind offsets, [RESKIN_OFF] rows in
   RESKIN/evidence/reskin-probe-gameplay.log (capture 1 = pristine authored).
 - REBAKED offsets from the SECOND [RESKIN_OFF] capture block in the same log
   ("cap2": the mesh re-dumped after RebindHeadHandsAtRest overwrote mOffset
   with meshWorld*inv(seed rest R) - verified by ang == APD_DIAG own-rest ang).
 - APD_DIAG char-space rest angles from SKEL/evidence/apd_diag_gameplay_grep.log.

Result:
 - ang(authored off) == APD_DIAG boundNow.ang to 0.1deg on 4/4 male hand bones
   => authored offsets are baked against `bound`'s bind basis B (meshWorld~I).
 - ang(cap2 off) == APD_DIAG own male rest ang exactly (106.0/147.6)
   => the default rebake anchors at own's seed rest R.
 - angle(B*inv(R)) = 87.2deg for BOTH L/R middlefinger03, axis mirror-symmetric
   (+0.130,+0.316,-0.940)/(-0.130,-0.316,-0.940)
   => the runtime Tier-1 87.3deg mode IS the B-vs-R relative rotation,
      reproduced from committed matrices with zero runtime.
"""
import numpy as np, math
def ang(M):
    tr = float(np.trace(M)); c = max(-1.0, min(1.0, (tr-1.0)/2.0))
    return math.degrees(math.acos(c))
A = {  # authored offsets (capture 1), player0 male hands_naked
 'R-middlefinger03': [[-0.29813,-0.95443,-0.01362],[0.03602,-0.02551,0.99903],[-0.95385,0.29735,0.04198]],
 'R-index01':        [[0.46510,-0.88257,-0.06897],[0.12017,-0.01425,0.99265],[-0.87706,-0.46997,0.09944]],
 'L-index01':        [[-0.46512,0.88267,-0.06755],[0.11952,-0.01300,-0.99275],[-0.87714,-0.46982,-0.09945]],
 'L-middlefinger03': [[0.29824,0.95441,-0.01220],[0.03644,-0.02416,-0.99904],[-0.95380,0.29751,-0.04199]],
}
C = {  # cap2 = rebaked offsets (second capture block), male
 'R-middlefinger03': [[0.84545,-0.42597,0.32262],[-0.40640,-0.12038,0.90583],[-0.34705,-0.89685,-0.27481]],
 'L-middlefinger03': [[-0.84486,0.42632,0.32370],[-0.40747,-0.11984,-0.90542],[-0.34723,-0.89676,0.27488]],
}
APD = {  # angles-vs-identity from SKEL/evidence/apd_diag_gameplay_grep.log
 'R-middlefinger03': {'bound':129.9,'own_male':106.0,'own_fem':119.7},
 'R-index01':        {'bound':103.0,'own_male':109.5,'own_fem':120.1},
 'L-index01':        {'bound':142.1,'own_male':121.6,'own_fem':119.6},
 'L-middlefinger03': {'bound':112.6,'own_male':147.6,'own_fem':127.2},
}
print("bone                 ang(authored) vs bound | ang(cap2) vs own_male")
for b,M in A.items():
    a = ang(np.array(M)); apd = APD[b]
    c = ang(np.array(C[b])) if b in C else None
    print(f"{b:20s} {a:8.1f} == {apd['bound']:6.1f}   | " +
          (f"{c:8.1f} == {apd['own_male']:6.1f}" if c else "    (no cap2 row)"))
print()
for b in C:
    B = np.array(A[b]).T   # rot(B) = rot(authored off)^T
    R = np.array(C[b]).T   # rot(R) = rot(cap2 off)^T
    Rel = B @ np.linalg.inv(R)
    ax = np.array([Rel[2,1]-Rel[1,2], Rel[0,2]-Rel[2,0], Rel[1,0]-Rel[0,1]]); ax/=np.linalg.norm(ax)
    print(f"{b}: angle(B*inv(R)) = {ang(Rel):.1f} deg, axis=({ax[0]:+.3f},{ax[1]:+.3f},{ax[2]:+.3f})")

# =====================================================================
# A6 FEMALE-AXIS EXTENSION (Wave-16 HANDS-FIX, added by Lane F).
# The adjudication's derivation above is male-only. WAVE16 acceptance A6
# pins the female gate: 28.9deg is the FAILURE signature (SHELL_FIX forced
# the SHARED male-bind B onto the female -> her authored basis is ~29deg
# away); a female PASS = count(>5deg)==0, SAME AS MALE.
#
# The authored-repoint cell (RB3_HANDS_AUTHORED_REPOINT) keeps EACH mesh's
# OWN authored offset, so the female mesh pairs inv(B_female) with female
# `own` (NOT the shared male B). Predicted: female Tier-1 ~= male Tier-1.
#
# OFFLINE derivable-from-committed: the female per-bone own-rest angles
# (own_fem in the APD table) are the female-skeleton rest basis the female
# `own` holds at the repoint frame. A full female matrix closure (as done
# for the male above) needs the FEMALE authored-offset matrices, which were
# not captured in the committed [RESKIN_OFF] dump (male player0 only) -> a
# named follow-up (RB3_RESKIN_PROBE on the female member).
#
# RUNTIME CONFIRMATION (this lane, /tmp/wave16-ON-engine.log via
# parse_hands_attach.py): with the cell ON, the FEMALE hands_naked palette
# reads Tier-1 worst 3.1deg, count(>5deg)==0 on ALL 502 blocks -- IDENTICAL
# to the male 3.1deg, and DISTINCT from SHELL_FIX's female 28.9deg (arm S).
# So the female authored offsets DO pair coherently with female `own`'s rest
# (own_fem ~= B_female), exactly as the per-asset-correct-by-construction
# claim predicts. The 28.9deg cross-gender gap is a SHELL_FIX artifact, NOT a
# property of this cell.  => A6 female gate: NUMERICALLY PASS.
print()
print("A6 female axis (own_fem rest angles vs identity, from APD_DIAG):")
for b in APD:
    print(f"  {b:20s} own_fem={APD[b]['own_fem']:6.1f}deg  (shared bound B={APD[b]['bound']:6.1f}deg)")
print("  RUNTIME (cell ON): female hands_naked Tier-1 = 3.1deg count(>5)==0 on 502/502")
print("  => female authored offsets pair coherently with female own (NOT shared-B).")
print("  => A6 female NUMERIC gate PASS; 28.9deg (SHELL_FIX) is the FAILURE signature, avoided.")
print("  NOTE: the VISUAL gate FAILS for BOTH genders (multi-bone finger-blend TEAR at")
print("        animated poses) -- rest-coherence (Tier-1) does not capture it. See STATUS.md.")
