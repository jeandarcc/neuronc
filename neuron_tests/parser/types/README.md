# Parser Types Specification

## Scope

This folder specifies type-annotation syntax as written in `.npp` source.

Covered forms:

- primitive annotations such as `int`, `float`, `bool`, `string`
- user-defined type names
- generic instantiations such as `Box<int>` and `Tensor<float>`
- dictionary/collection-like type spellings
- constrained generic parameter syntax in declarations
- multi-step cast/type-chain syntax where parsing alone is under test

## Error Codes

| Code or range | Meaning |
| --- | --- |
| `N1400-N1499` | malformed type syntax |
| `N1300-N1399` | type errors embedded in declaration headers |
| `NPP1002` | current parser fallback |

## Test List

| Name | `.npp` input code | Expected output or error | Why important |
| --- | --- | --- | --- |
| `types__primitive_annotation` | `count as int; ratio as float;` | Parse succeeds | Baseline annotated bindings |
| `types__generic_tensor_type` | `input as Tensor<float>;` | Parse succeeds | Covers common generic type usage in repo docs |
| `types__nested_generic_type` | `palette as Dictionary<string, int>;` | Parse succeeds | Confirms comma-separated generic arguments |
| `types__generic_class_instantiation` | `box as Box<int>;` | Parse succeeds | Keeps user-defined generic type grammar stable |
| `types__constrained_generic_param` | `Add method<T:Numeric>(a as T, b as T) as T { return a + b; }` | Parse succeeds | Pins constraint syntax |
| `types__cast_chain_form` | `value as maybe dynamic then string then float;` | Parse succeeds if cast-chain syntax is legal | Preserves advanced type-form parsing |
| `types__missing_generic_close` | `box as Box<int;` | `N1400-N1499` | Classic delimiter error inside type syntax |
| `types__dangling_constraint_colon` | `Add method<T:>(a as T) {}` | `N1400-N1499` or `N1300-N1399` | Defines malformed constraint handling |

## Edge Cases

- Whitespace around `<`, `>`, `,`, and `:`
- Nested generics several levels deep
- Generic arguments that are themselves generic types
- Type annotations on fields, parameters, locals, and returns
- Empty generic argument lists
- Type keywords reused as identifiers in invalid positions

## Happy Path

Happy-path tests should cover every type form in all declaration positions:

- variable
- parameter
- return
- field

## Error Path

Error-path tests should determine whether the parser recovers at the next `>`, the next `,`, or the next declaration boundary when generic syntax is malformed.
