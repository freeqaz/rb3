#!/usr/bin/env python3
"""Wave-20 Lane N: parse LOADBIND_SLOT probe rows into the A10 native binding table.

Input: the LOADBIND-filtered engine log of a boot with RB3_LOADBIND_PROBE=1.
Emits the A10 schema (one row per platform/state/member/mesh/boneSlot) as JSON + md,
plus the three-branch hit-count table. Read-only; regenerable.

A10 fields: platform, state, member, memberGender, mesh, boneSlotIndex, boneName,
status{RESOLVED|UNRESOLVED|MESH_ABSENT}, owningDirName, owningDirClass, owningDirInstanceId,
boneCount.
"""
import re, json, sys, os

SLOT = re.compile(
    r"\[LOADBIND_SLOT\] hook='(?P<hook>[^']*)' member='(?P<member>[^']*)' "
    r"mesh='(?P<mesh>[^']*)' slot=(?P<slot>\d+) bone='(?P<bone>[^']*)' "
    r"boundPtr=(?P<bound>0x[0-9a-f]+) owningDir=(?P<odir>0x[0-9a-f]+|\(nil\)) "
    r"owningDirName='(?P<oname>[^']*)' owningDirClass=(?P<oclass>[A-Z_]+) "
    r"ownFindPtr=(?P<own>0x[0-9a-f]+|\(nil\)) distinct=(?P<distinct>[01])")
MESH = re.compile(
    r"\[LOADBIND_MESH\] hook='[^']*' member='(?P<member>[^']*)' gender=(?P<gender>\w+) "
    r"mesh='(?P<mesh>[^']*)' meshPtr=0x[0-9a-f]+ class=(?P<cls>\w+) numBones=(?P<nb>\d+)")
COUNT = re.compile(
    r"\[LOADBIND_COUNTERS\] (?:afterInstall member='(?P<member>[^']*)'|marker='[^']*') "
    r"calls=(?P<calls>\d+) br0_charShared=(?P<br0>\d+) br1_instrument=(?P<br1>\d+) "
    r"br2_boneMergeEntered=(?P<br2>\d+) br3_boneMergeReplaced=(?P<br3>\d+)")

def build(path, platform, state):
    meshbones, meshgender = {}, {}
    rows, opaque = [], {}
    with open(path) as f:
        for line in f:
            m = MESH.search(line)
            if m:
                meshbones[(m['member'], m['mesh'])] = int(m['nb'])
                meshgender[m['member']] = m['gender']
            m = SLOT.search(line)
            if m:
                odir = m['odir']
                iid = opaque.setdefault(odir, f"inst{len(opaque)}")
                rows.append({
                    "platform": platform, "state": state, "member": m['member'],
                    "memberGender": meshgender.get(m['member'], "other"),
                    "mesh": m['mesh'], "boneSlotIndex": int(m['slot']),
                    "boneName": m['bone'],
                    "status": "RESOLVED",  # a SLOT row means the bone trans resolved
                    "owningDirName": m['oname'] or "(anon)",
                    "owningDirClass": m['oclass'],
                    "owningDirInstanceId": iid,
                    "boneCount": meshbones.get((m['member'], m['mesh']), 0),
                    "boundPtr": m['bound'], "ownFindPtr": m['own'],
                    "distinctFromOwnFind": m['distinct'] == '1',
                })
    # final per-member branch counters (last afterInstall line per member)
    counters = {}
    with open(path) as f:
        for line in f:
            c = COUNT.search(line)
            if c and c['member']:
                counters[c['member']] = {
                    "calls": int(c['calls']), "br0_charShared": int(c['br0']),
                    "br1_instrument": int(c['br1']),
                    "br2_boneMergeEntered": int(c['br2']),
                    "br3_boneMergeReplaced": int(c['br3'])}
    return rows, counters

def main():
    log = sys.argv[1] if len(sys.argv) > 1 else "loadbind_control_shimON.log"
    platform = "native"
    state = sys.argv[2] if len(sys.argv) > 2 else "main_hub+gameplay"
    rows, counters = build(log, platform, state)
    out = {"schema": "A10", "platform": platform, "state": state,
           "source_log": os.path.basename(log), "rows": rows,
           "filter_branch_counters_final": counters,
           "row_count": len(rows)}
    js = os.path.splitext(log)[0] + "_a10.json"
    with open(js, "w") as f:
        json.dump(out, f, indent=1)
    # summary
    from collections import Counter
    byclass = Counter(r['owningDirClass'] for r in rows)
    bydistinct = Counter(r['distinctFromOwnFind'] for r in rows)
    diri = Counter((r['member'], r['owningDirInstanceId']) for r in rows)
    print(f"rows={len(rows)} owningDirClass={dict(byclass)} "
          f"distinctFromOwnFind={dict(bydistinct)}")
    print("per-member owningDir instances:",
          sorted(set((m, i) for (m, i) in diri)))
    print("branch counters (final per member):")
    for mem, c in counters.items():
        print(f"  {mem}: {c}")

if __name__ == "__main__":
    main()
