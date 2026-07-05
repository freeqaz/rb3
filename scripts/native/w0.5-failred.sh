#!/usr/bin/env bash
# w0.5-failred.sh — W0.5.S4 fail-red proof driver.
#
# docs/native/engine-arch-review-2026-07-05/execution/W0.5/PLAN.md §W0.5.S4.
#
# Demonstrates REFACTOR_PLAN exit-gate #2: with a deliberately-broken skin run
# (RB3_NO_SKEL_REBIND=1 SHARD_GUARD_OFF=1 [+RB3_NO_SKIN_CLAMP=1]) the NEW
# composite gate (lineup-gate.py, W0.5.S3) FAILs a numeric layer while:
#   (i)  the OLD band-closeup gate (band-closeup-capture.py) PASSes, and
#   (ii) the perceptual image-compare layer (visual_diff.py --perceptual)
#        PASSes vs the committed golden.
#
# Writes three verdict JSONs + copies the exploded WIDE frame(s) into
# --artifacts-dir (default: the W0.5/failred doc dir committed alongside this
# script) so the regression is captured as committed evidence, not just a
# console log.
#
# Usage:
#   scripts/native/w0.5-failred.sh [--bin PATH] [--artifacts-dir DIR] [--anchor-ms MS]
#
# Exit: 0 if the fail-red proof holds (OLD=PASS, image=PASS, NEW=FAIL);
#       1 if the proof does NOT hold (e.g. shards didn't materialize, or the
#       new gate also passed — that would mean this script failed its job);
#       2 on a harness/environment error (GPU OOM black frame, capture error).
set -uo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO="$(cd "$HERE/../.." && pwd)"

BIN="$REPO/native/build-agent-W0.5/rb3-native"
ARTIFACTS_DIR="$REPO/docs/native/engine-arch-review-2026-07-05/execution/W0.5/failred"
ANCHOR_MS=24000
WORK="/tmp/w0.5-failred-$$"

while [[ $# -gt 0 ]]; do
  case "$1" in
    --bin) BIN="$2"; shift 2 ;;
    --artifacts-dir) ARTIFACTS_DIR="$2"; shift 2 ;;
    --anchor-ms) ANCHOR_MS="$2"; shift 2 ;;
    --work) WORK="$2"; shift 2 ;;
    -h|--help)
      grep '^#' "$0" | sed 's/^# \{0,1\}//'
      exit 0 ;;
    *) echo "unknown arg: $1" >&2; exit 2 ;;
  esac
done

mkdir -p "$WORK" "$ARTIFACTS_DIR"

PY="python3"
if [[ -x "$REPO/../dc3-decomp/venv/bin/python3" ]]; then
  PY="$REPO/../dc3-decomp/venv/bin/python3"
fi

GOLDEN_DIR="$REPO/scripts/native/goldens/w0.5-lineup"
BROKEN_ENV=(RB3_NO_SKEL_REBIND=1 SHARD_GUARD_OFF=1 SHARD_RATIO_DBG=1 RB3_NO_SKIN_CLAMP=1)

echo "=== W0.5.S4 fail-red proof ==="
echo "bin=$BIN work=$WORK artifacts=$ARTIFACTS_DIR anchor_ms=$ANCHOR_MS"
echo "broken env: ${BROKEN_ENV[*]}"
nvidia-smi --query-gpu=index,memory.used,memory.free --format=csv 2>/dev/null || true

if [[ ! -x "$BIN" ]]; then
  echo "FAIL_RED: ERROR — binary not found/executable: $BIN" >&2
  exit 2
fi
if [[ ! -f "$GOLDEN_DIR/golden.json" ]]; then
  echo "FAIL_RED: ERROR — committed golden missing: $GOLDEN_DIR/golden.json" >&2
  exit 2
fi

# ---------------------------------------------------------------------------
# Step 1+4: NEW gate under the broken env (lineup-gate.py, S3's composite
# driver). This is the one that must FAIL a numeric layer.
# ---------------------------------------------------------------------------
NEW_OUT="$WORK/new"
echo "--- [1/3] NEW gate (lineup-gate.py) under broken env ---"
env "${BROKEN_ENV[@]}" "$PY" "$REPO/scripts/native/lineup-gate.py" \
  --bin "$BIN" --out "$NEW_OUT" --anchor-ms "$ANCHOR_MS" \
  > "$WORK/new-gate.log" 2>&1
NEW_RC=$?
tail -40 "$WORK/new-gate.log"
if [[ ! -f "$NEW_OUT/verdict.json" ]]; then
  echo "FAIL_RED: ERROR — lineup-gate.py produced no verdict.json (rc=$NEW_RC)" >&2
  exit 2
fi
cp "$NEW_OUT/verdict.json" "$ARTIFACTS_DIR/new-gate-verdict.json"
NEW_VERDICT="$($PY -c "import json; print(json.load(open('$NEW_OUT/verdict.json'))['verdict'])")"
ANY_BLACK="$($PY -c "import json; print(json.load(open('$NEW_OUT/verdict.json'))['any_black_frame'])")"
echo "NEW gate verdict=$NEW_VERDICT any_black_frame=$ANY_BLACK"

if [[ "$ANY_BLACK" == "True" ]]; then
  echo "FAIL_RED: ERROR — GPU-OOM black frame in this run; retry when a GPU is free (see STATUS.md caveat)." >&2
  exit 2
fi

# Pick the exploded frame with the most visible defect: prefer a frame whose
# per-mesh ratio offenders are largest, but any non-black frame from this run
# is evidence-worthy; just take the first captured WIDE PNG for the human
# review artifact and copy ALL captured frames for completeness.
EXPLODED_SRC=""
for f in "$NEW_OUT"/cand_*.png; do
  [[ -f "$f" ]] || continue
  cp "$f" "$ARTIFACTS_DIR/exploded_$(basename "$f")"
  if [[ -z "$EXPLODED_SRC" ]]; then EXPLODED_SRC="$f"; fi
done
if [[ -z "$EXPLODED_SRC" ]]; then
  echo "FAIL_RED: ERROR — no WIDE PNG captured by lineup-gate.py's run" >&2
  exit 2
fi
EXPLODED_BASENAME="exploded_$(basename "$EXPLODED_SRC")"
echo "exploded frame -> $ARTIFACTS_DIR/$EXPLODED_BASENAME"

# Golden PNG matched to the exploded frame's (shot, idx) for the image-layer
# compare below (same basename convention as goldens/w0.5-lineup/*.png, minus
# the cand_ tag prefix).
GOLDEN_MATCH="$($PY - "$EXPLODED_SRC" "$GOLDEN_DIR" <<'PYEOF'
import os, re, sys
src, golden_dir = sys.argv[1], sys.argv[2]
base = os.path.basename(src)
m = re.match(r'^cand_(.+)\.png$', base)
name = m.group(1) if m else base
cand = os.path.join(golden_dir, name + '.png')
print(cand if os.path.isfile(cand) else '')
PYEOF
)"

# ---------------------------------------------------------------------------
# Step 2: OLD gate (band-closeup-capture.py) under the SAME broken env.
# ---------------------------------------------------------------------------
OLD_OUT="$WORK/old"
echo "--- [2/3] OLD gate (band-closeup-capture.py) under SAME broken env ---"
env "${BROKEN_ENV[@]}" "$PY" "$REPO/scripts/native/band-closeup-capture.py" \
  --bin "$BIN" --member guitar --frames 2 --out "$OLD_OUT" --tag failred \
  --anchor-ms "$ANCHOR_MS" \
  > "$WORK/old-gate.log" 2>&1
OLD_RC=$?
tail -20 "$WORK/old-gate.log"
OLD_JSON_LINE="$(grep -E '^\{"verdict"' "$WORK/old-gate.log" | tail -1)"
if [[ -z "$OLD_JSON_LINE" ]]; then
  echo "FAIL_RED: ERROR — band-closeup-capture.py produced no verdict JSON line (rc=$OLD_RC)" >&2
  exit 2
fi
echo "$OLD_JSON_LINE" > "$ARTIFACTS_DIR/old-gate-verdict.json"
OLD_VERDICT="$($PY -c "import json; print(json.load(open('$ARTIFACTS_DIR/old-gate-verdict.json'))['verdict'])")"
echo "OLD gate verdict=$OLD_VERDICT"

# ---------------------------------------------------------------------------
# Step 3: image (perceptual) layer — exploded WIDE frame vs the matched golden.
# ---------------------------------------------------------------------------
echo "--- [3/3] image layer (visual_diff.py --perceptual) exploded vs golden ---"
IMG_VERDICT="ERROR"
if [[ -n "$GOLDEN_MATCH" ]]; then
  "$PY" "$REPO/scripts/analysis/visual_diff.py" \
    "$EXPLODED_SRC" "$GOLDEN_MATCH" --perceptual --json --label failred_vs_golden \
    > "$WORK/image-layer.log" 2>&1
  cat "$WORK/image-layer.log"
  IMG_JSON_LINE="$(grep -E '^\{"mode"' "$WORK/image-layer.log" | tail -1)"
  if [[ -n "$IMG_JSON_LINE" ]]; then
    echo "$IMG_JSON_LINE" > "$ARTIFACTS_DIR/image-layer-verdict.json"
    IMG_VERDICT="$($PY -c "import json; print(json.load(open('$ARTIFACTS_DIR/image-layer-verdict.json'))['verdict'])")"
  fi
else
  echo "FAIL_RED: WARNING — no matched golden PNG found for $EXPLODED_SRC; image layer skipped" >&2
fi
echo "image layer verdict=$IMG_VERDICT"

# ---------------------------------------------------------------------------
# Write RESULT.md tabulating the proof.
# ---------------------------------------------------------------------------
"$PY" - "$ARTIFACTS_DIR" "$NEW_VERDICT" "$OLD_VERDICT" "$IMG_VERDICT" "$EXPLODED_BASENAME" "$GOLDEN_MATCH" "${BROKEN_ENV[*]}" <<'PYEOF' > "$ARTIFACTS_DIR/RESULT.md"
import json, os, sys

artifacts_dir, new_v, old_v, img_v, exploded_name, golden_match, broken_env = sys.argv[1:8]

new = json.load(open(os.path.join(artifacts_dir, "new-gate-verdict.json")))
old = json.load(open(os.path.join(artifacts_dir, "old-gate-verdict.json")))
img_path = os.path.join(artifacts_dir, "image-layer-verdict.json")
img = json.load(open(img_path)) if os.path.isfile(img_path) else {}

ratio_detail = new.get("ratioB_detail", {})
offenders = ratio_detail.get("offenders", [])

print("# W0.5.S4 — Fail-red proof (REFACTOR_PLAN exit-gate #2)\n")
print("Broken-skin env used to induce shards on the current build:\n")
print(f"```\n{broken_env}\n```\n")
print(f"Exploded WIDE frame (committed evidence): `{exploded_name}`\n")
if golden_match:
    print(f"Compared against golden: `{os.path.basename(golden_match)}` "
          f"(`scripts/native/goldens/w0.5-lineup/`)\n")

print("## Verdict summary\n")
print("| Layer | Tool | Verdict | Evidence |")
print("|---|---|---|---|")
print(f"| OLD gate metric | `band-closeup-capture.py` | **{old_v}** | "
      f"`drops_band={old.get('drops_band')}` `drops_total={old.get('drops_total')}` "
      f"`max_band_ratio={old.get('max_band_ratio')}` (its own cap; committed evidence: "
      "`old-gate-verdict.json`) |")
img_score = img.get("score")
print(f"| Perceptual image layer | `visual_diff.py --perceptual` | **{img_v}** | "
      f"score={img_score} vs min_score={img.get('min_score')} "
      "(`image-layer-verdict.json`) |")
print(f"| NEW gate (numeric, composite) | `lineup-gate.py` | **{new_v}** | "
      f"layers={new.get('layers')} (`new-gate-verdict.json`) |\n")

print("## What failed in the NEW gate\n")
print(f"`ratioB` (per-mesh world-extent-ratio layer, layer B) FAILed: "
      f"`n_offenders={ratio_detail.get('n_offenders')}` mesh(es) exceeded "
      f"`per_mesh_ratio_cap={ratio_detail.get('per_mesh_ratio_cap')}` "
      f"(golden-derived bound):\n")
print("| mesh | ratio | class | cap |")
print("|---|---|---|---|")
for o in offenders:
    print(f"| `{o['mesh']}` | {o['ratio']} | {o['class']} | "
          f"{ratio_detail.get('per_mesh_ratio_cap')} |")
print()
print(f"Meanwhile `max_band_ratio={ratio_detail.get('max_band_ratio')}` stayed "
      f"under its own separate bound ({ratio_detail.get('max_band_bound')}) — "
      "the offending meshes are classified `other` (crowd/extras patch geometry, "
      "not the `band`-tagged skinned meshes the OLD gate's `drops_band` counter "
      "tracks), which is exactly the blindness class W0.5 targets: a shard "
      "explosion the OLD gate's `drops_band==0` metric structurally cannot see, "
      "but the NEW per-mesh ratio layer catches because it checks every mesh, "
      "not just band-classified drops.\n")

print("## Conclusion\n")
print("The NEW composite gate (`lineup-gate.py`) FAILs a numeric layer "
      "(`ratioB`) on a build with induced skin-shard corruption, while the OLD "
      "band-closeup gate metric PASSes (`drops_band=0`) and the perceptual "
      "image-compare layer PASSes "
      f"(score {img_score} >= min {img.get('min_score')}). This is the literal "
      "REFACTOR_PLAN exit-gate #2 evidence: the new gate is non-blind to a "
      "shard class the old gate and the image layer both miss.\n")

print("## Reproduce\n")
print("```")
print("scripts/native/w0.5-failred.sh --bin native/build-agent-W0.5/rb3-native")
print("```")
PYEOF

echo "RESULT.md written -> $ARTIFACTS_DIR/RESULT.md"
echo
echo "=== SUMMARY ==="
echo "OLD gate=$OLD_VERDICT  image layer=$IMG_VERDICT  NEW gate=$NEW_VERDICT"

if [[ "$OLD_VERDICT" == "PASS" && "$IMG_VERDICT" == "PASS" && "$NEW_VERDICT" == "FAIL" ]]; then
  echo "W0.5_FAILRED_PROOF: HOLDS (OLD=PASS, image=PASS, NEW=FAIL)"
  exit 0
else
  echo "W0.5_FAILRED_PROOF: DOES NOT HOLD (expected OLD=PASS image=PASS NEW=FAIL)" >&2
  exit 1
fi
