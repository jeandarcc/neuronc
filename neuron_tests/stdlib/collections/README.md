# Standard Library Collections Specification

## Scope

This folder covers source-visible behavior of collection-like standard types such as arrays and dictionaries.

Covered behavior:

- declaration and initialization of collection types
- indexing and iteration
- empty vs non-empty behavior
- key/value lookup for dictionary-like types

## Error Codes

| Code or range | Meaning |
| --- | --- |
| `N2000-N2499` | invalid collection typing, indexing, or API misuse |
| `N5000-N5099` | runtime-visible collection failure |
| `NR2001`, `NR0001` | current fallbacks |

## Test List

| Name | `.nr` input code | Expected output or error | Why important |
| --- | --- | --- | --- |
| `collections__array_iteration` | `Init method(items as Array<int>) { for(item in items) Print(item); }` with harness-provided array input or equivalent fixture | Prints elements in documented order | Existing syntax already appears in semantic tests |
| `collections__array_index_read` | collection setup followed by `Print(items[0]);` | Prints first element | Baseline indexed read behavior |
| `collections__dictionary_type_acceptance` | `Init method() { palette as Dictionary<string, int>; }` | Semantic success | Current repo tests already recognize the type spelling |
| `collections__dictionary_lookup` | dictionary setup then `Print(palette["red"]);` | Prints mapped value | Makes key lookup a runtime contract if supported |
| `collections__out_of_bounds_access` | read invalid index from a small array | Defined runtime error or diagnostic | Forces the bounds policy to be explicit |
| `collections__wrong_key_type` | use non-string key against `Dictionary<string, int>` | `N2000-N2499` | Compile-time misuse should not slip to runtime |

## Edge Cases

- Empty collection
- Single-element collection
- Duplicate keys and overwrite behavior
- Iteration order guarantees
- Nested collections
- Aliasing/copying/moving collections under ownership rules

## Happy Path

Happy-path tests should define fixture creation clearly, especially if collections are produced by helper methods rather than literals.

## Error Path

Error-path tests should pin whether invalid access is a compile-time error, runtime trap, or recoverable runtime exception.
