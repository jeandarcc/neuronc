# Standard Library I/O Specification

## Scope

This folder covers source-visible behavior of console and file-style I/O exposed through standard modules.

Covered behavior:

- `Print(...)`
- `Input(...)` / `Input<T>(...)`
- file helpers such as `ReadFile()` / `WriteFile()` if available
- newline normalization and ordered output

## Error Codes

| Code or range | Meaning |
| --- | --- |
| `N4000-N4099` | missing I/O module or invalid module usage |
| `N2000-N2499` | compile-time misuse of I/O APIs |
| `N5000-N5099` | runtime-visible I/O failure |
| `NPP2001`, `NPP4001`, `NPP0001` | current fallbacks |

## Test List

| Name | `.npp` input code | Expected output or error | Why important |
| --- | --- | --- | --- |
| `io__print_string` | `module System; Init is method() { Print("hello"); }` | Prints `hello` | Smallest observable runtime test in the whole tree |
| `io__print_multiple_lines` | `module System; Init is method() { Print("a"); Print("b"); }` | Prints `a` then `b` on separate normalized lines | Locks output ordering and newline rules |
| `io__input_string_default` | `module System; Init is method() { name is Input("Name: ").Default("guest"); Print(name); }` | With deterministic harness input or default behavior, prints expected string | Common interactive source scenario |
| `io__input_typed_numeric` | `module System; Init is method() { age is Input<int>("Age: ").Min(18).Default(21); Print(age); }` | Compiles and prints the expected typed result under controlled input/defaults | Covers generic I/O API use |
| `io__read_write_roundtrip` | file write then file read from a temporary path | Output equals original text | Defines stdlib file contract if the APIs exist |
| `io__invalid_default_type` | `module System; Init is method() { age is Input<int>("Age: ").Default("oops"); }` | `N2000-N2499` | Existing built-in misuse contract already represented in semantic tests |

## Edge Cases

- Empty input string
- Prompt text with quotes or escapes
- EOF/no-input behavior
- Temporary-file paths with spaces
- Line-ending normalization on Windows vs non-Windows
- Failure to open a missing file

## Happy Path

Happy-path tests should define the exact harness behavior for stdin and filesystem setup so results stay deterministic.

## Error Path

Error-path tests must distinguish compile-time misuse of I/O APIs from runtime environment failures such as missing files.
