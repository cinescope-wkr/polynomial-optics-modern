# `exr-sanity` (EXR output sanity checks)

`bin/exr-sanity` is a small utility included in this repo to sanity-check EXR outputs without a GUI viewer:

```bash
./bin/exr-sanity /tmp/psf_fast.exr
```

It reports:

- min/max RGB and luminance
- nonzero pixel ratio (helps detect “all black” or “too sparse” outputs)
- luminance-weighted center-of-mass (should be near the image center for an on-axis PSF)
- encircled-energy radii (50/90/99%) and a rough “sigma radius” in pixels

