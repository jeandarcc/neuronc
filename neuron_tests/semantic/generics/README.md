# Semantic Generics Specification

## Scope

This folder covers generic classes, generic methods, generic constraints, generic arity, and built-in generic APIs used from `.npp` source.

## Error Codes

| Code or range | Meaning |
| --- | --- |
| `N2300-N2399` | generic arity, constraint, or substitution failure |
| `N2100-N2199` | generic inference ambiguity |
| `NPP2001` | current semantic fallback |

## Test List

| Name | `.npp` input code | Expected output or error | Why important |
| --- | --- | --- | --- |
| `generics__generic_class_roundtrip` | `Box class<T> { value is T; } Init is method() { boxValue is Box<int>(); boxValue.value is 123; Print(boxValue.value); }` | Compiles and prints `123` | Canonical generic class scenario |
| `generics__generic_method_identity` | `Identity method<T>(value as T) as T { return value; } Init is method() { Print(Identity(42)); }` | Compiles and prints `42` | Canonical generic method scenario |
| `generics__constraint_numeric_accepts_int` | `Add method<T:Numeric>(a as T, b as T) as T { return a + b; } Init is method() { Print(Add(1, 2)); }` | Compiles and prints `3` | Constraint success case |
| `generics__constraint_numeric_rejects_string` | `Add method<T:Numeric>(a as T, b as T) as T { return a + b; } Init is method() { Print(Add("a", "b")); }` | `N2300-N2399` | Prevents constraint bypass |
| `generics__input_valid_fluent_chain` | `Init is method() { age is Input<int>("Age: ").Min(18).Max(99).Default(21).TimeoutMs(5000); }` | Semantic success | Mirrors an existing built-in generic contract |
| `generics__input_invalid_arity` | `Init is method() { age is Input<int, float>("Age: ").Default("oops"); }` | `N2300-N2399` | Existing repo behavior worth preserving |
| `generics__input_invalid_default_type` | `Init is method() { age is Input<int>("Age: ").Default("oops"); }` | `N2300-N2399` or `N2000-N2099` | Validates substitution into fluent APIs |

## Edge Cases

- Multiple generic parameters such as `Pair<K, V>`
- Nested generics such as `Array<Box<int>>`
- Constraint combinations if the language later permits them
- Generic methods inside generic classes
- Generic argument inference vs explicit generic argument spelling
- Built-in generics with fluent follow-up methods and chained diagnostics

## Happy Path

Happy-path tests should show both explicit and inferred generic use, including successful chained operations on built-in generic APIs.

## Error Path

Error-path tests should distinguish:

- wrong generic count
- wrong constrained type
- wrong follow-up method for the instantiated type
- wrong argument type after substitution
