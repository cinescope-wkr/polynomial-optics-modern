# `ex1-postprocess`

```bash
./bin/ex1-postprocess input.pfm -p 1 -s 1 -o out.exr
```

What it does:

- builds a lens system (hardcoded or from `.lens`)
- computes focus (paraxial estimate) and appends propagation to film plane
- runs a spectral loop (wavelength samples)
- samples aperture per input pixel (Monte Carlo)
- writes an output image

## Input / output contracts

### Units

- Lens geometry is in millimeters in the provided systems and element builders.
- `OpticalMaterial::get_index(lambda)` expects `lambda` in **nanometers**.

### Input image

- Any format CImg can read (PFM is common for HDR float data).
- If built with OpenEXR support (Makefile), EXR input/output should work.

### Output image

- Controlled by `-o <path>` (default: `out.exr`).
- Output resolution is determined by a fixed “sensor” model in the code.

## Rendering pipeline notes

### Spectral sampling

The example approximates chromatic effects by interpolating between polynomial systems sampled at a small number of wavelengths, then “baking” wavelength into the system.

### Aperture sampling

- If `blade_count == 0`, samples a circular aperture (rejection sampling).
- Otherwise, samples a polygonal aperture defined by blade vertices.

### Output writing

Output is written via CImg; in this repo’s Makefile build the intended output format is EXR.

## CLI reference (current behavior)

`examples/Example_PostprocessImage.cpp` supports the following options:

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

Lens files:

- [Lens file format (`systems/*.lens`)](../reference/lens-format.md)
