# Parser Functions Specification

## Scope

This folder specifies parsing of method and callable declarations, including parameters, return types, modifiers, generics, and body forms.

## Error Codes

| Code or range | Meaning |
| --- | --- |
| `N1300-N1399` | malformed method/declaration syntax |
| `N1400-N1499` | malformed type syntax within signatures |
| `N1500-N1599` | incomplete declarations requiring recovery |
| `NR1002` | current parser fallback |

## Test List

| Name | `.nr` input code | Expected output or error | Why important |
| --- | --- | --- | --- |
| `func__simple_method` | `Echo method(value as string) { Print(value); }` | Parse succeeds | Smallest named method baseline |
| `func__method_with_return_type` | `Add method(a as int, b as int) as int { return a + b; }` | Parse succeeds | Pins return-type annotation placement |
| `func__generic_method` | `Identity method<T>(value as T) as T { return value; }` | Parse succeeds | Confirms method-level generic syntax |
| `func__modifier_and_method_name` | `overload Add method(a as int, b as int) as int { return a + b; }` | Parse succeeds | Covers modifier ordering |
| `func__async_or_virtual_style_header` | `virtual Negate method(a as int) as int { return 0 - a; }` | Parse succeeds | Guards modifier-plus-signature composition |
| `func__type_first_parameter_rejected` | `Add method(int a, int b) as int { return a + b; }` | `N1300-N1399` | Keeps parameter grammar unambiguous |
| `func__missing_comma_between_params` | `Add method(a as int b as int) as int { return a + b; }` | `N1300-N1399` | Common typo with actionable diagnostics |
| `func__missing_body_or_terminator` | `Echo method(value as string)` | `N1500-N1599` | Defines incomplete declaration handling |

## Edge Cases

- Zero-parameter and many-parameter methods
- Generic methods with constrained parameters such as `<T:Numeric>`
- Methods declared inside classes vs top-level
- Optional semicolon after block-bodied methods if supported
- Mixed modifiers such as `override`, `virtual`, `overload`, `async`
- Return-less methods using explicit `return;`

## Happy Path

Happy-path tests should cover:

- declaration-only syntax acceptance
- full block body parsing
- generic signatures
- modifier combinations known to be legal

## Error Path

Error-path tests should pinpoint whether the failure is in:

- generic parameter list
- ordinary parameter list
- return type
- body opening/closing delimiter
