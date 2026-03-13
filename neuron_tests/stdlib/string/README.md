# Standard Library String Specification

## Scope

This folder covers source-visible behavior for string values and string-oriented standard functionality.

Covered behavior:

- string literal round-trips
- concatenation if supported
- length/property access such as `.Length`
- string values flowing through `Print`, `Input`, and generic APIs

## Error Codes

| Code or range | Meaning |
| --- | --- |
| `N2000-N2099` | invalid string operation or member access |
| `N5000-N5099` | runtime-visible string handling failure |
| `NR2001`, `NR0001` | current fallbacks |

## Test List

| Name | `.nr` input code | Expected output or error | Why important |
| --- | --- | --- | --- |
| `string__literal_print_roundtrip` | `module System; Init is method() { value is "Neuron"; Print(value); }` | Prints `Neuron` | Baseline string value propagation |
| `string__concatenation` | `module System; Init is method() { Print("hello" + " world"); }` | Prints `hello world` if concatenation is a defined language feature | Makes string `+` semantics explicit |
| `string__length_property` | `module System; Init is method() { value is "ok"; Print(value.Length); }` | Prints `2` | Mirrors current semantic tests for member access |
| `string__empty_string_length` | `module System; Init is method() { value is ""; Print(value.Length); }` | Prints `0` | Important boundary value |
| `string__input_secret_mode` | `module System; Init is method() { password is Input<string>("Password: ").Secret().Default("guest"); Print(password.Length); }` | Compiles and prints deterministic length | Covers a string-specific fluent API contract present in semantic tests |
| `string__null_string_member_access` | `module System; Init is method() { value is null; Print(value.Length); }` | `N2000-N2099` or null-safety diagnostic | Existing semantic null-unsafe behavior should remain visible |

## Edge Cases

- Escaped quotes and backslashes
- Empty vs whitespace-only strings
- Very long strings
- Concatenation mixed with numbers or dynamic values
- Null and maybe-null string values
- Unicode behavior if the runtime promises it

## Happy Path

Happy-path tests should assert exact text and exact length values after all formatting normalization rules are applied.

## Error Path

Error-path tests should specify whether failure is at compile time, runtime, or both for null and dynamic string operations.
