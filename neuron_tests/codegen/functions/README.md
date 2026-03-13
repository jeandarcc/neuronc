# Runtime Function Call Specification

## Scope

This folder covers executable behavior of function and method calls, parameter passing, returns, recursion, and named arguments after semantic success.

## Error Codes

| Code or range | Meaning |
| --- | --- |
| `N5000-N5099` | call-lowering/runtime failure after semantic success |
| `N2400-N2499` | overload-related semantic rejection before codegen |
| `NR0001`, `NR2001` | current fallbacks |

## Test List

| Name | `.nr` input code | Expected output or error | Why important |
| --- | --- | --- | --- |
| `func_runtime__simple_call` | `module System; Add method(a as int, b as int) as int { return a + b; } Init is method() { Print(Add(2, 3)); }` | Prints `5` | Baseline free-function call |
| `func_runtime__nested_calls` | `module System; Inc method(v as int) as int { return v + 1; } Init is method() { Print(Inc(Inc(1))); }` | Prints `3` | Covers nested call sequencing |
| `func_runtime__named_arguments` | `module System; Sum method(first as int, second as int) as int { return first + second; } Init is method() { Print(Sum(second: 2, first: 1)); }` | Prints `3` | Keeps named-argument lowering aligned with semantic binding |
| `func_runtime__method_call_on_instance` | class instance method returning field-derived value | Prints expected value | Covers receiver passing |
| `func_runtime__generic_identity_output` | `module System; Identity method<T>(value as T) as T { return value; } Init is method() { Print(Identity("ok")); }` | Prints `ok` | Confirms generic call behavior survives to runtime |
| `func_runtime__recursion_if_supported` | factorial-like or countdown recursion | Prints exact result if recursion is legal; otherwise this README should anchor the rejection elsewhere | Makes recursion support an explicit contract |

## Edge Cases

- Many arguments
- Zero arguments
- Void-return vs value-return calls
- Calls inside expressions
- Calls that transfer ownership with `move`
- Generic calls with explicit and inferred arguments

## Happy Path

Happy-path tests should prove both value correctness and argument-evaluation order when side effects such as `Print` are involved.

## Error Path

Error-path tests should be limited to post-semantic call failures, such as backend/runtime issues, not overload/type-checking failures that belong elsewhere.
