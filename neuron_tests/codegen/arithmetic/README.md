# Runtime Arithmetic Specification

## Scope

This folder covers successful execution of arithmetic and numeric expression behavior after compilation.

Covered behavior:

- integer arithmetic
- floating-point arithmetic
- mixed precedence
- unary arithmetic
- tensor/matrix arithmetic when the observable result can be asserted

## Error Codes

| Code or range | Meaning |
| --- | --- |
| `N5000-N5099` | runtime-visible arithmetic lowering failure or trap |
| `N2000-N2099` | semantic rejection before codegen for invalid operand pairs |
| `NR0001`, `NR2001` | current fallbacks |

## Test List

| Name | `.nr` input code | Expected output or error | Why important |
| --- | --- | --- | --- |
| `arith__int_precedence` | `module System; Init is method() { Print(1 + 2 * 3); }` | Prints `7` | Baseline arithmetic precedence at runtime |
| `arith__grouped_expression` | `module System; Init is method() { Print((1 + 2) * 3); }` | Prints `9` | Confirms grouping survives lowering |
| `arith__sub_and_div` | `module System; Init is method() { Print(10 - 3); Print(8 / 2); }` | Prints `7` then `4` | Covers multiple operators and sequencing |
| `arith__unary_minus` | `module System; Init is method() { Print(0 - 5); }` | Prints `-5` or language-normalized equivalent | Prevents sign-handling regressions |
| `arith__float_expression` | `module System; Init is method() { Print(1.5 + 2.5); }` | Prints normalized float result | Baseline floating runtime behavior |
| `arith__tensor_matmul_visible_result` | tensor program with small deterministic operands | Compiles and prints deterministic tensor result | Keeps user-visible tensor arithmetic in behavior scope |

## Edge Cases

- Large integer boundaries
- Division by zero if the language defines trap vs diagnostic behavior
- Mixed `int` and `float`
- Nested arithmetic in function-call arguments
- Arithmetic inside loops and conditionals
- Tensor dimensions that are 1x1, 1xN, and NxN

## Happy Path

Happy-path tests should pin exact outputs, including newline order and float-format normalization rules.

## Error Path

Error-path coverage should distinguish semantic rejection from runtime trap or backend lowering failure.
