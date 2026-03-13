# Semantic Overloading Specification

## Scope

This folder covers callable resolution when multiple declarations share a name.

Covered behaviors:

- overload selection by arity
- overload selection by parameter type
- duplicate overload rejection
- named-argument interaction with overload resolution
- ambiguous or no-viable-overload diagnostics

## Error Codes

| Code or range | Meaning |
| --- | --- |
| `N2400-N2499` | no viable overload, ambiguous overload, duplicate overload set member |
| `N2000-N2099` | downstream type mismatch after candidate selection |
| `NR2001` | current semantic fallback |

## Test List

| Name | `.nr` input code | Expected output or error | Why important |
| --- | --- | --- | --- |
| `overload__select_by_type` | `MathOps class { overload Add method(a as int, b as int) as int { return a + b; } overload Add method(a as float, b as float) as float { return a + b; } } Init is method() { ops is MathOps(); Print(ops.Add(1, 2)); }` | Compiles and prints `3` | Core overload dispatch case |
| `overload__select_by_arity` | `MathOps class { overload Sum method(a as int) as int { return a; } overload Sum method(a as int, b as int) as int { return a + b; } } Init is method() { ops is MathOps(); Print(ops.Sum(5)); }` | Compiles and prints `5` | Ensures arity participates in resolution |
| `overload__named_arguments_still_bind_correctly` | overload set plus `result is Sum(second: 2, first: 1);` | Semantic success with the two-parameter overload chosen | Prevents named arguments from bypassing overload logic |
| `overload__no_viable_candidate` | call `ops.Add("a", "b")` against only numeric overloads | `N2400-N2499` | User-facing failure when no candidate matches |
| `overload__duplicate_signature_rejected` | two identical `overload Add method(a as int, b as int)` declarations | `N2400-N2499` | Keeps overload sets deterministic |
| `overload__ambiguity_contract` | intentionally ambiguous call shape once implicit conversions are defined | `N2400-N2499` or documented as not applicable until conversion rules exist | Prevents future ambiguity rules from appearing accidentally |

## Edge Cases

- Overloads across class inheritance if the language supports it
- Overloads with generic parameters
- Overloads differing only by named-argument labels if labels are semantic
- Method vs free-function overload precedence if both exist
- Interaction with `dynamic` arguments

## Happy Path

Happy-path tests should prove the chosen overload is the only reasonable candidate by observable output, not by inspecting internals.

## Error Path

Error-path tests should record whether diagnostics enumerate candidate signatures or only state the failure class. Both behaviors are valid only if specified and stable.
