# Troubleshooting

- `pkg-config: OpenEXR not found` during `make`:
  - install OpenEXR development packages + `pkg-config`
- `ex1-postprocess` reads image but output fails:
  - verify you built with Makefile OpenEXR flags if writing `.exr`
- Unexpected output / heavy runtime:
  - reduce `-s` (samples multiplier) and `-p` (lambda passes)
  - use lower polynomial degree `-c`

