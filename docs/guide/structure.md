# Structure

## Core library

- `TruncPoly/TruncPolySystem.hh`
  - polynomial algebra and polynomial-system composition engine

## Optical elements

- `OpticalElements/TwoPlane5.hh`: input (world+pupil) → ray parameterization
- `OpticalElements/Propagation5.hh`: free-space propagation
- `OpticalElements/Spherical5.hh`: spherical refraction/reflection expansions
- `OpticalElements/Cylindrical5.hh`: cylindrical refraction expansions
- `OpticalElements/OpticalMaterial.hh`: glass database + Sellmeier IOR
- `OpticalElements/FindFocus.hh`: back focal length / magnification utilities
- `OpticalElements/PointToPupil5.hh`: point-to-pupil mapping

## Examples

- `Example_BasicArithmetic.cpp`: tutorial-style walkthrough of core API
- `Example_PostprocessImage.cpp`: image-space postprocess renderer through a lens system
- `Example_EclipsedBokeh.cpp`: synthetic “eclipsed bokeh” demo (EXR sequence output)

Tools:

- `tools/`: helper scripts and small utilities (sanity sweeps, EXR stats, reproducibility helpers)

## Assets / data

- `systems/*.lens`: lens sequences consumed by the postprocess example
- `include/spectrum.h`: wavelength↔RGB conversions used by the postprocess example

## File responsibilities (quick index)

- `TruncPoly/TruncPolySystem.hh`: polynomial algebra, system composition, derivatives, interpolation helpers
- `OpticalElements/*.hh`: element builder functions returning polynomial systems
- `Example_BasicArithmetic.cpp`: API and lens-system walkthrough
- `Example_PostprocessImage.cpp`: postprocess renderer with `.lens` parsing and EXR output
- `systems/*.lens`: lens sequences
- `include/CImg.h`: image I/O and buffer ops (vendored)
- `include/spectrum.h`: wavelength↔RGB helpers used by postprocess example

