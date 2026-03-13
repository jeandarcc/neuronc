# Standard Library Behavior Specification

## Scope

This folder covers behavior exposed through standard modules when called from `.nr` source. The oracle is still user-observable source behavior, not implementation details of the runtime libraries.

Subfolders:

- [io/README.md](io/README.md)
- [string/README.md](string/README.md)
- [collections/README.md](collections/README.md)
- [math/README.md](math/README.md)

Included:

- `System`, `IO`, `Math`, and collection/string-like facilities as observable from `.nr`
- Success and failure behavior for module APIs
- Diagnostics when stdlib APIs are misused from source

Excluded:

- Runtime internals tested directly from C/C++
- Package-manager or module-install workflows

## Error Codes

| Code or range | Meaning |
| --- | --- |
| `N2000-N2499` | semantic misuse of stdlib APIs |
| `N4000-N4099` | module/import resolution failures |
| `N5000-N5099` | runtime-visible stdlib execution failures |
| `W2000-W2099` | warning-only stdlib misuse |
| `NR2001`, `NR4001`, `NR0001` | current coarse fallbacks |

## Test List

| Name | `.nr` input code | Expected output or error | Why important |
| --- | --- | --- | --- |
| `stdlib_baseline__system_print` | `module System; Init is method() { Print("ok"); }` | Prints `ok` | Smallest stdlib smoke test |
| `stdlib_baseline__math_function` | call a stable math function and print result | Prints normalized numeric result | Confirms module method invocation works |
| `stdlib_baseline__string_roundtrip` | create, combine, and print text | Prints expected text | Covers common user workflows |
| `stdlib_baseline__collection_basic_access` | create collection, read/write element, print result | Prints expected element/value | Validates container semantics visible from source |
| `stdlib_baseline__missing_module` | `module MissingPkg; Init is method() {}` | `N4000-N4099` or `NR4001` | Keeps source-level module errors in scope |
| `stdlib_baseline__bad_argument_type` | wrong type passed to stdlib function | `N2000-N2499` | Ensures stdlib calls obey the same semantic contracts as user code |

## Edge Cases

- Empty strings, empty collections, and singleton collections
- File or console I/O with newline normalization
- Numeric domain edges for math functions
- Unicode or escaped text if string semantics support it
- Missing imports vs implicitly available built-ins
- Runtime error reporting from stdlib calls that compile successfully

## Happy Path

Happy-path stdlib tests should show the exact visible effect of the API from `.nr` source, not only that the call type-checks.

## Error Path

Error-path stdlib tests must distinguish:

- missing or wrong module usage
- type misuse at compile time
- runtime failure after successful compilation
