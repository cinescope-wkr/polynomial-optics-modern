# Lens file format (`systems/*.lens`)

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

## Current parser quirks (important for reproducibility)

The parser in `Example_PostprocessImage.cpp` is intentionally lightweight; be aware of:

- `two_plane` lines: the numeric argument (if present in the `.lens` file) is currently ignored; distance is taken from `-z` (or its default).
- Blank lines / comments: not explicitly supported (blank lines can print “invalid op”).
- Numeric index parsing: the code only checks whether the first character is `'0'..'9'`, so values like `.99` or `-1.0` will be treated as glass names.
- Cylindrical elements: truncation degree is not passed into `refract_cylindrical_*_5(...)` in the current implementation (it uses the element default).

