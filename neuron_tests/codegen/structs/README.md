# Runtime Structs Specification

## Scope

This folder covers user-visible runtime behavior of structs and aggregate-like data: construction, field access, field mutation, copying/aliasing effects, and passing structs through functions.

## Error Codes

| Code or range | Meaning |
| --- | --- |
| `N5000-N5099` | aggregate layout/lowering failure visible at runtime |
| `N2000-N2099` | semantic rejection of invalid field usage |
| `NPP0001`, `NPP2001` | current fallbacks |

## Test List

| Name | `.npp` input code | Expected output or error | Why important |
| --- | --- | --- | --- |
| `struct__field_read_write` | `module System; Point struct { x is 0 as int; y is 0 as int; } Init is method() { p is Point(); p.x is 3; p.y is 4; Print(p.x); Print(p.y); }` | Prints `3` then `4` | Baseline aggregate field behavior |
| `struct__copy_vs_alias` | struct assigned with `is` vs `another` then mutated | Output proves whether state is shared or copied | Makes aggregate ownership behavior visible |
| `struct__pass_through_function` | helper accepts struct and returns field-derived value | Prints expected field-based result | Covers ABI-visible aggregate passing |
| `struct__nested_struct_access` | struct containing another struct and reading nested field | Prints expected nested value | Guards offset/layout bugs |
| `struct__pointer_to_struct_field` | take `address of` a field or struct, mutate through `value of`, then print | Prints updated field value | Exercises aggregate-plus-pointer interaction |
| `struct__default_initialized_fields` | instantiate struct and print default field values | Prints documented defaults | Makes zero/default initialization a contract |

## Edge Cases

- Empty struct if supported
- Single-field and many-field structs
- Nested structs and arrays of structs
- Struct fields with generic or tensor types
- Mutation after copy, alias, and move
- Field access through method return values

## Happy Path

Happy-path tests should validate not only direct field reads, but also state persistence across copies, calls, and nested aggregates.

## Error Path

Error-path tests should focus on runtime-visible wrong-code or unsupported aggregate lowering, not ordinary field-name resolution failures.
