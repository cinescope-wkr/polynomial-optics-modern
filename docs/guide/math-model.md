# Math model + optical elements

## Core types

### `PolyTerm<T, N>`

A single monomial:

- coefficient `T coefficient`
- exponent vector `echar exponents[N]`

### `TruncPoly<T, N>`

A truncated multivariate polynomial as a list of terms.

Main operations:

- `print()`
- `evaluate(x)`
- `differentiate(i)`
- `truncate(degree)`
- symbolic substitution/composition (via `plug_into`)

### `Transform<T, N>`

A vector of `N` polynomials: a multivariate map.

- A `Transform4f` maps 4D inputs to 4D outputs: `out = T(in)`.

Composition is function composition:

- `T3 = T2.plug_into(T1)` corresponds to `T3(x) = T2(T1(x))`.

## Optical elements

Optical elements are implemented as functions that return a `Transform4f` for a particular action, e.g.:

- `propagate_5(d)`
- `refract_spherical_5(R, n1, n2, degree)`
- `reflect_spherical_5(R, degree)`
- `refract_cylindrical_x_5(R, n1, n2)` / `refract_cylindrical_y_5(...)`

Typical pattern:

1. Create a `TwoPlane5` parametrization system
2. Append element transforms via `plug_into`
3. Evaluate the ray transform many times (per sample)

## Performance and numerical considerations

- Term growth can be large for higher degrees; use truncation (`% degree`) aggressively.
- Evaluation uses repeated integer powers; for very hot loops, consider caching or specialized evaluation strategies.
- Exponents are stored as `unsigned char` (`echar`); exponent overflow is possible if you exceed intended degrees.
- Composition (`plug_into`) is the most expensive symbolic operation; it is intended for building systems once, then evaluating many times.

