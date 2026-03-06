#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

LENS_DEFAULT="systems/Edmund-Optics-achromat-NT32-921.lens"
LENS="${1:-$LENS_DEFAULT}"

OUTDIR="${OUTDIR:-OutputPFM/sanity}"
mkdir -p "$OUTDIR"

echo "[sanity] lens: $LENS"
echo "[sanity] outdir: $OUTDIR"

make -s >/dev/null

SCENE_POINT="$OUTDIR/scene_point_disc.pfm"
SCENE_STARS="$OUTDIR/scene_stars.pfm"

python3 tools/gen_pfm.py --out "$SCENE_POINT" --w 256 --h 144 --pattern disc --cx 0.5 --cy 0.5 \
  --radius_px 1.5 --soft_edge_px 1.0 --rgb 50,50,50 --bg 0,0,0 >/dev/null

python3 tools/gen_pfm.py --out "$SCENE_STARS" --w 512 --h 288 --pattern stars --stars 400 --seed 7 \
  --rgb 10,10,10 --bg 0,0,0 --max_amp 1.0 >/dev/null

run_one() {
  local name="$1"; shift
  local out="$OUTDIR/${name}.exr"
  echo
  echo "[run] $name"
  echo "  cmd: ./bin/ex1-postprocess $* -o $out"
  ./bin/ex1-postprocess "$@" -o "$out" >/dev/null
  ./bin/exr-sanity "$out" | sed 's/^/  /'
}

# A) Baseline (no lens): should stay near input with minor resampling artifacts.
run_one "A_baseline_nolens_fast" "$SCENE_POINT" -p 1 -s 50 -x 1 -c 3 -z 5000000 -e 19.5 -d 0 -a 1

# B) Lens in-focus (default): should be centered PSF; chromatic can add mild color fringes.
run_one "B_lens_infocus_p1" "$SCENE_POINT" -i "$LENS" -p 1 -s 200 -x 1 -c 3 -z 5000 -e 19.5 -d 0 -a 1
run_one "B_lens_infocus_p12" "$SCENE_POINT" -i "$LENS" -p 12 -s 80 -x 1 -c 3 -z 5000 -e 19.5 -d 0 -a 1

# C) Defocus sweep: radii should increase as |defocus| increases.
run_one "C_defocus_minus20" "$SCENE_POINT" -i "$LENS" -p 12 -s 80 -x 1 -c 3 -z 5000 -e 19.5 -d -20 -a 1
run_one "C_defocus_plus20" "$SCENE_POINT" -i "$LENS" -p 12 -s 80 -x 1 -c 3 -z 5000 -e 19.5 -d 20 -a 1

# D) Aperture shape: with blades, expect polygonal bokeh when defocused.
run_one "D_hex_bokeh" "$SCENE_POINT" -i "$LENS" -p 12 -s 80 -x 1 -c 3 -z 5000 -e 19.5 -d 25 -b 6 -a 1

# E) “Night lights” synthetic: should show aberrations/bokeh patterns on points (slower; moderate samples).
run_one "E_stars_bokeh" "$SCENE_STARS" -i "$LENS" -p 6 -s 50 -x 1 -c 3 -z 5000 -e 19.5 -d 30 -b 0 -a 1

echo
echo "[done] EXR outputs in: $OUTDIR"
echo "[note] Open them with your local EXR viewer for qualitative inspection."

