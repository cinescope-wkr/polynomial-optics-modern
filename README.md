# Polynomial Optics (EGSR 2012) — Modernized Repository Guide

[Project Page](https://www.cs.ubc.ca/labs/imager/tr/2012/PolynomialOptics/) | [Paper](https://dl.acm.org/doi/abs/10.1111/j.1467-8659.2012.03132.x) | [Technical Documentation](documentation.md)

This repository contains the **Polynomial Optics** code package accompanying the following EGSR 2012 paper. 

> *Polynomial Optics: A Construction Kit for Efficient Ray-Tracing of Lens Systems*  
> Matthias B. Hullin, Johannes Hanika, Wolfgang Heidrich — Computer Graphics Forum (EGSR 2012)

The original upstream-style README is preserved as `README.upstream.md` (license, citation request, original usage notes).

## Notice
> [!NOTE]
> This `README` is a **modernized, practical guide** to building and using the code (including the current OpenEXR/CImg workflow). See `documentation.md` for details.
>
> **Maintainer**: [Jinwoo Lee](https://cinescope-wkr.github.io) (cinescope@kaist.ac.kr)

## Quick Navigation

- [1. Build and Dependency Requirements](#1-build-and-dependency-requirements)
- [2. Project Structure](#2-project-structure)
- [3. Runtime Entry Points (Examples)](#3-runtime-entry-points-examples)
- [4. Input / Output Specification](#4-input--output-specification)
- [5. Lens Definition Files (`systems/*.lens`)](#5-lens-definition-files-systemslens)
- [6. Notes on Validity and Limitations](#6-notes-on-validity-and-limitations)
- [7. Documentation](#7-documentation)
- [8. Citation](#8-citation)

## 1. Build and Dependency Requirements

### 1.1 Supported host

- OS: Linux/macOS/Windows (the code is standard C++ with a few GNU extensions in the postprocess example; see “Limitations”).
- Compiler: `g++`/`clang++` with C++11 support recommended.

### 1.2 Dependencies

This repository’s `Makefile` builds the postprocess example with **OpenEXR** support through CImg:

- `pkg-config`
- OpenEXR development packages discoverable via `pkg-config OpenEXR`

### 1.3 Build (GNU Make)

```bash
make
```

Outputs:

- `bin/ex0-basicarithmetic`
- `bin/ex1-postprocess`
- `bin/ex2-eclipsed-bokeh` (synthetic “eclipsed bokeh” demo)
- `bin/exr-sanity` (EXR stats for quick sanity checks)

### 1.4 Build (CMake)

A minimal CMake project exists (`CMakeLists.txt`). Note that OpenEXR flags/links are not currently mirrored in CMake the same way as in the Makefile, so prefer `make` if you need EXR I/O.

## 2. Project Structure

```text
TruncPoly/
  TruncPolySystem.hh          # core polynomial + polynomial-system implementation
OpticalElements/
  OpticalMaterial.hh          # glass database + Sellmeier IOR
  TwoPlane5.hh                # ray parametrization (world+pupil -> ray)
  Propagation5.hh             # free-space propagation
  Spherical5.hh               # spherical refraction/reflection
  Cylindrical5.hh             # cylindrical refraction
  FindFocus.hh                # paraxial focal length utilities
  PointToPupil5.hh            # point-to-pupil mapping
include/
  CImg.h                      # vendored CImg
  spectrum.h                  # wavelength <-> RGB/XYZ helpers
systems/
  *.lens                      # lens element sequences for the postprocess example
Example_BasicArithmetic.cpp
Example_PostprocessImage.cpp
Makefile
CMakeLists.txt
README.upstream.md             # upstream-style README (original)
```

## 3. Runtime Entry Points (Examples)

### 3.1 Basic arithmetic + simple lens system

```bash
./bin/ex0-basicarithmetic
```

What it demonstrates:

- Polynomial creation/printing, composition (`>>`), differentiation, truncation.
- Building a composite lens system from optical elements + glass IOR.
- Computing back focal length from 1st-order (matrix optics) terms.

### 3.2 Postprocess renderer (HDR image re-trace through lens)

This example reads an input image (often PFM/EXR), re-traces it through a lens, and writes an output image (default `out.exr` in this repo).

Low-sample sanity run:

```bash
./bin/ex1-postprocess path/to/input.pfm -p 1 -s 1 -o out.exr
```

Load a lens description:

```bash
./bin/ex1-postprocess input.pfm -i systems/Edmund-Optics-achromat-NT32-921.lens -p 1 -s 1 -o out.exr
```

Quick sanity check on the output (no GUI required):

```bash
./bin/exr-sanity out.exr
```

Important note: `ex1-postprocess` currently documents a positional “outputfile” in its help text, but output selection is actually done via `-o` (default `out.exr`). Treat `-o` as authoritative.

Common knobs:

- `-c <degree>`: truncation degree (default 3)
- `-p <count>`: number of wavelength passes (default 12)
- `-s <mult>`: sample multiplier (default 1000; reduce for speed)
- `-z <distance_mm>`: scene distance for `two_plane` (default 5,000,000 mm)

### 3.3 Eclipsed bokeh demo (Debevec 2020)

This repo also includes a small synthetic demo that reproduces *eclipsed bokeh* by clipping a defocused point light’s bokeh disk with an occluding disk at a different depth.

```bash
./bin/ex2-eclipsed-bokeh -t A
./bin/ex2-eclipsed-bokeh -t B
./bin/ex2-eclipsed-bokeh -t C
```

It writes EXR frames to `OutputEXR/eclipsed-bokeh/` by default; see `./bin/ex2-eclipsed-bokeh -h` for all parameters.
If your EXR viewer shows the frames as black, either increase viewer exposure or run with `-g` (gain) and/or `-p` (write tonemapped PNG).
For an out-of-the-box “cinematic” sequence wrapper, see `tools/cinematic_eclipsed_sequence.py`.

## 4. Input / Output Specification

### 4.1 Units and conventions (core optics)

- Length units in the provided element builders (`*5.hh`) are **millimeters** (radii, thickness, propagation distances).
- Wavelength `lambda` is treated as **nanometers** in `OpticalMaterial::get_index(lambda)` (converted internally to µm for Sellmeier).

### 4.2 `ex1-postprocess` I/O

Inputs:

- `argv[1]`: input image file path (CImg-supported formats; PFM commonly used; EXR supported when compiled with OpenEXR).
- Optional: `-i systems/*.lens` to load a lens definition.

Outputs:

- Output image via `-o <path>` (default: `out.exr`).
- Output resolution is currently fixed in the code (sensor model), independent of input resolution.

## 5. Lens Definition Files (`systems/*.lens`)

The `.lens` format is a simple line-based sequence of optical element operations.

Example:

```text
two_plane 5000000
refract_spherical 65.22 1.0 N-SSK8
propagate 9.60
refract_spherical -62.03 N-SSK8 N-SF10
propagate 4.20
refract_spherical -1240.67 N-SF10 1.0
```

Supported ops in the current parser:

- `two_plane`
- `propagate <d>`
- `refract_spherical <R> <n_or_glass1> <n_or_glass2>`
- `reflect_spherical <R>`
- `cylindrical_x <R> <n_or_glass1> <n_or_glass2>`
- `cylindrical_y <R> <n_or_glass1> <n_or_glass2>`

Where `<n_or_glass*>` can be a numeric refractive index (e.g. `1.616`) or a glass name (e.g. `N-BK7`).

## 6. Notes on Validity and Limitations

- **Degree truncation vs. accuracy**: polynomial truncation trades accuracy for speed; higher degrees can explode term counts.
- **Chromatic interpolation in `ex1-postprocess`**: the example uses system interpolation between a small set of sampled wavelengths; results can degrade outside that bracket.
- **Portability**: `Example_PostprocessImage.cpp` uses a variable-length array for aperture blades (GNU extension). If you target MSVC/strict C++, you may need to switch that to `std::vector`.
- **Numerical range**: exponents are stored as `unsigned char` (`echar`), so very high-degree constructions can overflow exponents if pushed beyond intended use.

## 7. Documentation

- Full technical documentation: `documentation.md`
- Historical upstream README: `README.upstream.md`

## 8. Citation

The upstream, original package requests citation of the EGSR 2012 paper when using the code or ideas:

```bibtex
@article{PolynomialOptics-Hullin2012,
  author = {Matthias B. Hullin and Johannes Hanika and Wolfgang Heidrich},
  title = {{Polynomial Optics}: A Construction Kit for Efficient Ray-Tracing of Lens Systems},
  journal = {Computer Graphics Forum (Proceedings of EGSR 2012)},
  volume = {31},
  number = {4},
  year = {2012},
  month = jul
}
```
