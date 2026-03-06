# Practical sanity experiments (scenes + sweeps)

The fastest way to validate “is this behaving like a lens PSF / bokeh” is to run repeatable synthetic inputs and check:

- centroid stays near center for an on-axis point
- defocus changes the encircled-energy radius monotonically
- blade count produces polygonal bokeh when defocused
- chromatic passes (`-p`) introduce mild color structure (but also increase runtime)

## Generate a minimal PSF scene (disc/point)

```bash
python3 tools/gen_pfm.py --out /tmp/scene_point.pfm --w 256 --h 144 --pattern disc \
  --cx 0.5 --cy 0.5 --radius_px 1.5 --soft_edge_px 1.0 --rgb 50,50,50 --bg 0,0,0
```

## One-off checks (manual)

Lens in-focus (fast-ish):

```bash
./bin/ex1-postprocess /tmp/scene_point.pfm -i systems/Edmund-Optics-achromat-NT32-921.lens \
  -z 5000 -p 12 -s 80 -x 1 -e 19.5 -d 0 -o /tmp/psf_infocus.exr
./bin/exr-sanity /tmp/psf_infocus.exr
```

Defocus sweep:

```bash
./bin/ex1-postprocess /tmp/scene_point.pfm -i systems/Edmund-Optics-achromat-NT32-921.lens \
  -z 5000 -p 12 -s 80 -x 1 -e 19.5 -d 20 -o /tmp/psf_defocus20.exr
./bin/exr-sanity /tmp/psf_defocus20.exr
```

Hexagonal bokeh (requires defocus to be obvious):

```bash
./bin/ex1-postprocess /tmp/scene_point.pfm -i systems/Edmund-Optics-achromat-NT32-921.lens \
  -z 5000 -p 12 -s 80 -x 1 -e 19.5 -d 25 -b 6 -o /tmp/psf_hex.exr
./bin/exr-sanity /tmp/psf_hex.exr
```

## Full sweep (script)

Run a bundled script that generates two synthetic scenes (disc PSF + “stars”) and produces a small battery of EXR outputs with stats:

```bash
bash tools/run_sanity_psf.sh
```

Change lens file:

```bash
bash tools/run_sanity_psf.sh systems/Edmund-Optics-achromat-NT49-291.lens
```

## Defocus animation sweep (EXR sequence)

To visualize how the PSF/bokeh evolves as you move the sensor plane around nominal focus, render a defocus sweep into an EXR sequence:

```bash
python3 tools/defocus_sweep.py --outdir OutputPFM/defocus_anim --d_from -30 --d_to 30 --frames 61
```

Use the synthetic “stars” scene (many point lights) instead of a single PSF disc:

```bash
python3 tools/defocus_sweep.py --pattern stars --w 512 --h 288 --rgb 10,10,10 --stars 400 \
  --outdir OutputPFM/defocus_stars --d_from -30 --d_to 30 --frames 61
```

This writes:

- `OutputPFM/defocus_anim/frame_####_d+xx.xx.exr` (EXR sequence)
- `OutputPFM/defocus_anim/stats.csv` (per-frame `comX/comY/r50/r90/r99/sigmaR/maxY`)

Open the EXR sequence in your local viewer (DJV/tev/Nuke) to scrub/loop it like an animation.

## Why some runs get slow

In `examples/Example_PostprocessImage.cpp`, per-pixel sample count is set as:

- `num_samples = max(1, int(L_in * sample_mul))`

So a single very bright pixel (large `L_in`) can explode the number of samples. To keep runtime stable:

- start with smaller `-s` and `-p` and confirm qualitative shape first
- avoid huge `-x` on point/disc scenes; scale your input RGB instead
