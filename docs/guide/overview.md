# Overview

This documentation describes the **Polynomial Optics** codebase in this repository: structure, workflows, I/O contracts, and the implementation model (polynomial terms → polynomials → polynomial systems → optical elements → rendering examples).

For upstream historical notes and licensing, see [Upstream README](../upstream.md).

## Original work (EGSR 2012)

This codebase accompanies:

> *Polynomial Optics: A Construction Kit for Efficient Ray-Tracing of Lens Systems*  
> Matthias B. Hullin, Johannes Hanika, Wolfgang Heidrich — Computer Graphics Forum (EGSR 2012)

Core idea: represent ray transforms through lens systems as **truncated multivariate polynomials**, enabling efficient approximate ray-tracing via polynomial composition.

## What’s modernized in this repository

This repo contains practical updates to improve correctness and “works-out-of-the-box” behavior on modern toolchains:

- Fixed/implemented declared-but-stubbed template APIs in `TruncPoly` (now safe to instantiate):
  - `TruncPoly::lerp_with()` correctness for arbitrary positions
  - `TruncPoly::querp_with()`, `TruncPoly::bake_input_variable()`
- Fixed broken wrapper call in `OpticalElements/PointToPupil5.hh`
- Fixed sampling PDF mismatch in `include/spectrum.h`
- Updated build notes (Makefile uses OpenEXR via `pkg-config` for EXR I/O through CImg)

## Where to go next

- Build: [Build](build.md)
- High-level layout: [Structure](structure.md)
- How the math maps to optics: [Math model + optical elements](math-model.md)

