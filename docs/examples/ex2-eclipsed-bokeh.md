# `ex2-eclipsed-bokeh` (Debevec 2020 “eclipsed bokeh” demo)

This demo renders a defocused point light whose bokeh becomes **sharply clipped** by an occluding disk at a different depth (the “eclipsed bokeh” phenomenon). It writes a sequence of EXR frames.

Build:

```bash
make bin/ex2-eclipsed-bokeh
```

Run one of the three depth-ordering cases (writes `OutputEXR/eclipsed-bokeh/eclipsed_####.exr`):

```bash
./bin/ex2-eclipsed-bokeh -t A
./bin/ex2-eclipsed-bokeh -t B
./bin/ex2-eclipsed-bokeh -t C
```

You can set the output prefix using `-o <prefix>` or by passing it as the last positional argument.

Common knobs:

- `-F/-L/-K`: focus/light/occluder distances (mm)
- `-d`: film-plane defocus offset (mm) to enlarge/shrink bokeh without changing case ordering
- `-E/-M/-N/-D`: end values for `-F/-L/-K/-d` to linearly ramp distances across frames
- `-T`: target radius in the light plane (use this to make the “target” larger even when in focus)
- `-A/-B -n`: occluder travel (Y-from/Y-to) and frame count
- `-R`: occluder radius (mm) for blocking
- `-S`: aperture samples per frame (quality vs. speed)
- `-g`: brightness gain (use this if EXR looks “black” in your viewer)
- `-u`: also write baseline `*_open_####.*` (no occluder) and `*_diff_####.*` (blocked energy)
- `-W -a -b`: multi-wavelength rendering (sample count and range in nm); use `-C` to pick the focus reference wavelength
- `-p -q`: also write tonemapped PNG (and control its exposure)
- `-i systems/*.lens`: swap the lens definition (default is the achromat used by `ex1-postprocess`)
  - `-h`: full CLI help

Occluder reference:

- `-m 0`: no occluder visualization (recommended if you find overlays misleading)
- `-m 1`: overlay a faint neutral “occluder bokeh” *visualization* on top of the main image
- `-m 2`: write separate `*_occ_####.exr` (and `*_occ_####.png` if `-p` is set) for that visualization
- `-m 3`: overlay a non-physical marker (crosshair at occluder chief-ray location) for unambiguous motion reference

If you want the most “physically honest” presentation of the occluder’s effect (without pretending the occluder emits light), prefer `-u` and look at `*_diff_####.*`.

Helper tool (cinematic sequence wrapper):

- `tools/cinematic_eclipsed_sequence.py` generates a “cinematic” multi-light shot with simultaneous occluder motion + defocus sweep (and optional MP4 if `ffmpeg` exists).

