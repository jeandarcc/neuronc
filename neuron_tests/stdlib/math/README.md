# Standard Library Math Specification

## Scope

This folder covers user-visible behavior of `Math` and math-adjacent standard facilities used from `.nr` source.

Covered behavior:

- scalar math functions such as `sqrt`, `sin`, `cos`, `pow`
- constants such as `Math.PI`
- deterministic numeric formatting expectations

## Error Codes

| Code or range | Meaning |
| --- | --- |
| `N4000-N4099` | missing math module usage |
| `N2000-N2499` | wrong argument types or counts for math APIs |
| `N5000-N5099` | runtime-visible math execution failure |
| `NR2001`, `NR4001`, `NR0001` | current fallbacks |

## Test List

| Name | `.nr` input code | Expected output or error | Why important |
| --- | --- | --- | --- |
| `math__sqrt_baseline` | `module System; module Math; Init is method() { Print(Math.sqrt(9)); }` | Prints normalized representation of `3` | Canonical math-module smoke test |
| `math__pi_constant` | `module System; module Math; Init is method() { Print(Math.PI); }` | Prints stable normalized constant value or prefix | Keeps constant exposure stable |
| `math__pow_or_equivalent` | `module System; module Math; Init is method() { Print(Math.pow(2, 3)); }` | Prints normalized representation of `8` if API exists | Covers multi-argument math calls |
| `math__vector_length_example` | `module System; module Math; Vector2 class { x is 3 as float; y is 4 as float; Length method() as float { return Math.sqrt(this.x * this.x + this.y * this.y); } } Init is method() { v is Vector2(); Print(v.Length()); }` | Prints normalized representation of `5` | Connects math APIs to real user code |
| `math__wrong_argument_type` | `module Math; Init is method() { value is Math.sqrt("oops"); }` | `N2000-N2499` | Ensures bad math calls fail early |
| `math__domain_edge_case` | call a domain-sensitive function on a boundary value | Either defined numeric result or defined runtime/diagnostic failure | Forces domain behavior to be documented |

## Edge Cases

- Integer vs float arguments
- Large and tiny floating values
- Formatting stability for irrational results
- Domain errors such as negative square root if complex numbers are unsupported
- Chained math calls inside larger expressions

## Happy Path

Happy-path tests should normalize float formatting so platform differences do not hide semantic regressions.

## Error Path

Error-path tests must distinguish argument-type misuse from mathematically invalid runtime input.
