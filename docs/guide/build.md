# Build

## Compiler and standard

- C++11 is sufficient for this codebase.
- GNU Make build uses `g++` directly.

## Dependencies

Core polynomial + optical element code is header-only.

The postprocess example (`examples/Example_PostprocessImage.cpp`) uses:

- vendored `include/CImg.h`
- OpenEXR (enabled via Makefile’s `-Dcimg_use_openexr` + `pkg-config OpenEXR`)

## Build with Make

```bash
make
```

Outputs:

- `bin/ex0-basicarithmetic`
- `bin/ex1-postprocess`
- `bin/ex2-eclipsed-bokeh`
- `bin/exr-sanity`

## Build with CMake

`CMakeLists.txt` can build the examples, but currently does not mirror the Makefile’s OpenEXR integration. Prefer `make` if you rely on EXR I/O.
