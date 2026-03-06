# Documentation — Polynomial Optics (EGSR 2012) Codebase

This document describes the **Polynomial Optics** codebase in this repository: structure, workflows, I/O contracts, and the implementation model (polynomial terms → polynomials → polynomial systems → optical elements → rendering example).

For upstream historical notes and licensing, see `README.upstream.md`.

## Original Work and Repository Modernization

### Original work (EGSR 2012)

This codebase accompanies:

> *Polynomial Optics: A Construction Kit for Efficient Ray-Tracing of Lens Systems*  
> Matthias B. Hullin, Johannes Hanika, Wolfgang Heidrich — Computer Graphics Forum (EGSR 2012)

The core idea: represent ray transforms through lens systems as **truncated multivariate polynomials**, enabling efficient approximate ray-tracing via polynomial composition.

### What’s “modernized” / repurposed in this repository

This repository version contains practical updates that improve correctness and “works-out-of-the-box” behavior on modern toolchains:

- Fixed/implemented declared-but-stubbed template APIs in `TruncPoly` (now safe to instantiate):
  - `TruncPoly::lerp_with()` correctness for arbitrary positions
  - `TruncPoly::querp_with()`, `TruncPoly::bake_input_variable()`
- Fixed broken wrapper call in `OpticalElements/PointToPupil5.hh`
- Fixed sampling PDF mismatch in `include/spectrum.h`
- Updated the build notes (this repo’s `Makefile` uses OpenEXR via `pkg-config` for EXR I/O through CImg)

## Quick Navigation

- [1. Build and Dependency Requirements](#1-build-and-dependency-requirements)
- [2. Codebase Structure](#2-codebase-structure)
- [3. Core Types and Math Model](#3-core-types-and-math-model)
- [4. Optical Elements](#4-optical-elements)
- [5. Runtime Entry Points](#5-runtime-entry-points)
- [6. Input / Output Contracts](#6-input--output-contracts)
- [7. Lens Definition Format (`systems/*.lens`)](#7-lens-definition-format-systemslens)
- [8. Rendering Pipeline Notes (`ex1-postprocess`)](#8-rendering-pipeline-notes-ex1-postprocess)
- [9. Performance and Numerical Considerations](#9-performance-and-numerical-considerations)
- [10. Error Diagnostics](#10-error-diagnostics)
- [11. File Responsibilities](#11-file-responsibilities)
- [12. Citation and Licensing](#12-citation-and-licensing)

<details open>
<summary><strong>1. Build and Dependency Requirements</strong></summary>

## 1. Build and Dependency Requirements

### 1.1 Compiler + standard

- C++11 is sufficient for this codebase.
- GNU Make build uses `g++` directly.

### 1.2 Dependencies

Core polynomial + optical element code is header-only.

The postprocess example (`Example_PostprocessImage.cpp`) uses:

- vendored `include/CImg.h`
- OpenEXR (enabled in this repo via the Makefile’s `-Dcimg_use_openexr` + `pkg-config OpenEXR`)

### 1.3 Build with Make

```bash
make
```

Outputs:

- `bin/ex0-basicarithmetic`
- `bin/ex1-postprocess`

### 1.4 Build with CMake

`CMakeLists.txt` can build the examples, but currently does not mirror the Makefile’s OpenEXR integration. Prefer `make` if you rely on EXR I/O.

</details>

<details open>
<summary><strong>2. Codebase Structure</strong></summary>

## 2. Codebase Structure

### 2.1 Core library

- `TruncPoly/TruncPolySystem.hh`
  - Implements the polynomial algebra and polynomial-system composition engine.

### 2.2 Optical elements

- `OpticalElements/TwoPlane5.hh`: input (world+pupil) → ray parameterization
- `OpticalElements/Propagation5.hh`: free-space propagation
- `OpticalElements/Spherical5.hh`: spherical refraction/reflection expansions
- `OpticalElements/Cylindrical5.hh`: cylindrical refraction expansions
- `OpticalElements/OpticalMaterial.hh`: glass database + Sellmeier IOR
- `OpticalElements/FindFocus.hh`: back focal length / magnification utilities
- `OpticalElements/PointToPupil5.hh`: point-to-pupil mapping

### 2.3 Examples

- `Example_BasicArithmetic.cpp`: tutorial-style walkthrough of core API
- `Example_PostprocessImage.cpp`: image-space postprocess renderer through a lens system

### 2.4 Assets / data

- `systems/*.lens`: lens sequences consumed by the postprocess example
- `include/spectrum.h`: wavelength↔RGB conversions used by the postprocess example

</details>

<details open>
<summary><strong>3. Core Types and Math Model</strong></summary>

## 3. Core Types and Math Model

### 3.1 `PolyTerm<T, N>`

A single monomial:

- coefficient `T coefficient`
- exponent vector `echar exponents[N]`

### 3.2 `TruncPoly<T, N>`

A sparse polynomial in `N` variables:

- `vector<PolyTerm<T, N>> terms`
- `trunc_degree` controls truncation by total degree
- `consolidate_terms()` sorts terms and merges duplicates (same exponent vector)

Supported operations (selection):

- `+`, `-`, `*` (symbolic polynomial algebra)
- `^` (power; exponential blow-up is possible)
- `%` / `%=` truncation to total degree
- `get_derivative(wrt)` analytic derivative polynomial
- `evaluate(x)` evaluation at a numeric point

### 3.3 `TruncPolySystem<T, Nin, Nout>`

A rectangular system of polynomials:

- `equations[Nout]`, each a `TruncPoly<T, Nin>`
- Composition `P1 >> P2` (“plug into”) substitutes outputs of `P1` into inputs of `P2`

Interpretation:

You can model ray transforms as:

```text
(x0, x1, ..., xNin-1)  ->  (y0, y1, ..., yNout-1)
```

Then lens systems become repeated compositions of element transforms.

</details>

<details open>
<summary><strong>4. Optical Elements</strong></summary>

## 4. Optical Elements

The `OpticalElements/*5.hh` headers provide pre-expanded polynomial systems (up to degree 5) corresponding to common optical operations.

Key idea:

- Each builder returns a `Transform4f` (4-in/4-out polynomial system) for ray state:
  - typical ray variables are `x, y, dx, dy` under a two-plane parametrization

Notable elements:

- `two_plane_5(dz, degree)`:
  - maps `(x_world, y_world, x_pupil, y_pupil)` → `(x_pupil, y_pupil, dx, dy)`
- `propagate_5(dist, degree)`:
  - propagates ray in free space
- `refract_spherical_5(R, n1, n2, degree)`:
  - spherical refraction surface (radius `R`, indices `n1→n2`)

</details>

<details open>
<summary><strong>5. Runtime Entry Points</strong></summary>

## 5. Runtime Entry Points

### 5.1 `ex0-basicarithmetic`

```bash
./bin/ex0-basicarithmetic
```

Use it as the canonical “API example” for:

- creating polynomials
- composing them
- truncating
- building lens systems from elements
- finding focal length

### 5.2 `ex1-postprocess`

```bash
./bin/ex1-postprocess input.pfm -p 1 -s 1 -o out.exr
```

It:

- builds a lens system (hardcoded or from `.lens`)
- computes focus (paraxial estimate) and appends propagation to film plane
- runs a spectral loop (wavelength samples)
- samples aperture per input pixel (Monte Carlo)
- writes an output image

### 5.3 `ex2-eclipsed-bokeh` (Debevec 2020 “eclipsed bokeh” demo)

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

### 5.4 `exr-sanity` (EXR output sanity checks)

`bin/exr-sanity` is a small utility included in this repo to sanity-check EXR outputs without a GUI viewer:

```bash
./bin/exr-sanity /tmp/psf_fast.exr
```

It reports:

- min/max RGB and luminance
- nonzero pixel ratio (helps detect “all black” or “too sparse” outputs)
- luminance-weighted center-of-mass (should be near the image center for an on-axis PSF)
- encircled-energy radii (50/90/99%) and a rough “sigma radius” in pixels

### 5.5 `ex1-postprocess` CLI reference (current behavior)

`Example_PostprocessImage.cpp` supports the following options:

| Flag | Meaning | Default |
|---|---|---|
| `-a` | “anamorphic” factor (divides `x_ap`) | `1` |
| `-b` | iris blade count (0 = circular aperture) | `0` |
| `-c` | polynomial truncation degree | `3` |
| `-d` | defocus (added to computed focus propagation) | `0.0` |
| `-e` | entrance pupil radius | `19.5` |
| `-f` | filter size (currently parsed but not applied) | `1` |
| `-o` | output filename | `out.exr` |
| `-p` | number of wavelength passes | `12` |
| `-s` | sample multiplier (importance sampling scale) | `1000` |
| `-x` | exposure multiplier applied to input pixel RGB | `1.0` |
| `-i` | lens definition file (`systems/*.lens`) | unset |
| `-z` | scene distance for the two-plane parametrization | `5000000` |

Notes / gotchas:

- The help text suggests a positional “outputfile”, but the program actually uses `-o` for output selection.
- `-z` is supported in the parser but is not listed in the help text.
- Options are only parsed when `argc >= 4` (so include at least one option if you rely on non-default values).

</details>

<details open>
<summary><strong>6. Input / Output Contracts</strong></summary>

## 6. Input / Output Contracts

### 6.1 Units

- Lens geometry is in millimeters in the provided systems and element builders.
- `OpticalMaterial::get_index(lambda)` expects `lambda` in **nanometers**.

### 6.2 Postprocess input image

- Any format CImg can read (PFM is common for HDR float data).
- If built with OpenEXR support (Makefile), EXR input/output should work.

### 6.3 Postprocess output image

- Controlled by `-o <path>` (default: `out.exr`).
- Output resolution is determined by a fixed “sensor” model in the code.

</details>

<details open>
<summary><strong>7. Lens Definition Format (<code>systems/*.lens</code>)</strong></summary>

## 7. Lens Definition Format (`systems/*.lens`)

Lens files are line-based operation sequences. Each line begins with an opcode followed by arguments.

Supported opcodes in the current parser:

- `two_plane`
- `propagate <d>`
- `refract_spherical <R> <n_or_glass1> <n_or_glass2>`
- `reflect_spherical <R>`
- `cylindrical_x <R> <n_or_glass1> <n_or_glass2>`
- `cylindrical_y <R> <n_or_glass1> <n_or_glass2>`

Where `n_or_glass*` is either:

- numeric index (e.g. `1.616`)
- glass name (e.g. `N-BK7`) resolved via `OpticalMaterial`

### 7.1 Current parser quirks (important for reproducibility)

The parser in `Example_PostprocessImage.cpp` is intentionally lightweight; be aware of:

- `two_plane` lines: the numeric argument (if present in the `.lens` file) is currently ignored; distance is taken from `-z` (or its default).
- Blank lines / comments: not explicitly supported (blank lines can print “invalid op”).
- Numeric index parsing: the code only checks whether the first character is `'0'..'9'`, so values like `.99` or `-1.0` will be treated as glass names.
- Cylindrical elements: truncation degree is not passed into `refract_cylindrical_*_5(...)` in the current implementation (it uses the element default).

</details>

<details>
<summary><strong>8. Rendering Pipeline Notes (<code>ex1-postprocess</code>)</strong></summary>

## 8. Rendering Pipeline Notes (`ex1-postprocess`)

### 8.1 Spectral sampling

The example approximates chromatic effects by interpolating between polynomial systems sampled at a small number of wavelengths, then “baking” wavelength into the system.

### 8.2 Aperture sampling

- If `blade_count == 0`, samples a circular aperture (rejection sampling).
- Otherwise, samples a polygonal aperture defined by blade vertices.

### 8.3 Output writing

Output is written via CImg; in this repo’s Makefile build the intended output format is EXR.

</details>

<details>
<summary><strong>9. Performance and Numerical Considerations</strong></summary>

## 9. Performance and Numerical Considerations

- Term growth can be large for higher degrees; use truncation (`% degree`) aggressively.
- Evaluation uses repeated integer powers; for very hot loops, consider caching or specialized evaluation strategies.
- Exponents are stored as `unsigned char` (`echar`); exponent overflow is possible if you exceed intended degrees.
- Composition (`plug_into`) is the most expensive symbolic operation; it is intended for building systems once, then evaluating many times.

</details>

<details open>
<summary><strong>10. Error Diagnostics</strong></summary>

## 10. Error Diagnostics

- `pkg-config: OpenEXR not found` during `make`:
  - install OpenEXR development packages + `pkg-config`
- `ex1-postprocess` reads image but output fails:
  - verify you built with Makefile OpenEXR flags if writing `.exr`
- Unexpected output / heavy runtime:
  - reduce `-s` (samples multiplier) and `-p` (lambda passes)
  - use lower polynomial degree `-c`

## 12. Practical Sanity Experiments (Scenes + Sweeps)

The fastest way to validate “is this behaving like a lens PSF / bokeh” is to run repeatable synthetic inputs and check:

- centroid stays near center for an on-axis point
- defocus changes the encircled-energy radius monotonically
- blade count produces polygonal bokeh when defocused
- chromatic passes (`-p`) introduce mild color structure (but also increase runtime)

### 12.1 Generate a minimal PSF scene (disc/point)

```bash
python3 tools/gen_pfm.py --out /tmp/scene_point.pfm --w 256 --h 144 --pattern disc \
  --cx 0.5 --cy 0.5 --radius_px 1.5 --soft_edge_px 1.0 --rgb 50,50,50 --bg 0,0,0
```

### 12.2 One-off checks (manual)

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

### 12.3 Full sweep (script)

Run a bundled script that generates two synthetic scenes (disc PSF + “stars”) and produces a small battery of EXR outputs with stats:

```bash
bash tools/run_sanity_psf.sh
```

Change lens file:

```bash
bash tools/run_sanity_psf.sh systems/Edmund-Optics-achromat-NT49-291.lens
```

### 12.4 Defocus animation sweep (EXR sequence)

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

### 12.5 Why some runs get slow

In `Example_PostprocessImage.cpp`, per-pixel sample count is set as:

- `num_samples = max(1, int(L_in * sample_mul))`

So a single very bright pixel (large `L_in`) can explode the number of samples. To keep runtime stable:

- start with smaller `-s` and `-p` and confirm qualitative shape first
- avoid huge `-x` on point/disc scenes; scale your input RGB instead

</details>

<details open>
<summary><strong>11. File Responsibilities</strong></summary>

## 11. File Responsibilities

- `TruncPoly/TruncPolySystem.hh`
  - polynomial algebra + system composition + derivatives + interpolation helpers
- `OpticalElements/*.hh`
  - element builder functions returning polynomial systems
- `Example_BasicArithmetic.cpp`
  - API and lens-system walkthrough
- `Example_PostprocessImage.cpp`
  - postprocess renderer with `.lens` parsing and EXR output
- `systems/*.lens`
  - lens sequences
- `include/CImg.h`
  - image I/O and buffer ops (vendored)
- `include/spectrum.h`
  - wavelength↔RGB helpers used by postprocess example

</details>

<details open>
<summary><strong>12. Citation and Licensing</strong></summary>

## 12. Citation and Licensing

See `README.upstream.md` for:

- upstream license terms
- citation request
- third-party licensing notes (e.g. CImg)

</details>
