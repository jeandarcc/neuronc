# Regression Specification

## Scope

This folder contains permanently retained reproductions of bugs that have already occurred in the `.nr` source experience. Every regression test must explain the original failure mode and the user-visible contract that now must remain stable.

Included:

- Parser regressions
- Semantic regressions
- Ownership regressions
- Runtime/codegen regressions
- Diagnostic regressions

Excluded:

- Internal-only bugs that cannot be observed from `.nr` source behavior
- LSP-only regressions

## Error Codes

Regression tests inherit the codes from the behavior area they exercise. Every regression README entry must name:

- the expected current code or range
- any accepted fallback code
- whether the historical bug was a false positive, false negative, crash, wrong output, or bad diagnostic

## Test List

| Name | `.nr` input code | Expected output or error | Why important |
| --- | --- | --- | --- |
| `regression_template__parse_crash_now_reports_error` | smallest source that once crashed the parser | Stable parser error instead of crash | Prevents catastrophic failures from returning |
| `regression_template__semantic_false_negative_now_rejected` | source that was accepted but should not be | Stable semantic error | Captures safety fixes |
| `regression_template__semantic_false_positive_now_accepts` | source that was rejected but should compile | Compiles and produces expected output | Protects valid user code |
| `regression_template__ownership_diagnostic_keeps_origin_note` | use-after-move example | Ownership error with stable move-origin context | Ensures debuggability stays intact |
| `regression_template__wrong_runtime_output_fixed` | executable example from a past miscompile | Stable corrected output | Guards against silent wrong-code regressions |

## Edge Cases

- Regressions caused by feature interaction rather than a single feature
- Minimal repro vs realistic repro variants
- Historical bugs that depended on formatting or line breaks
- Regressions that only appeared after parser recovery or warning emission
- Regressions fixed in one backend but still at risk in another

## Happy Path

Happy-path regression tests prove that previously rejected or miscompiled programs now compile and execute correctly.

## Error Path

Error-path regression tests prove that previously accepted invalid programs, crashes, or unusable diagnostics now fail in a controlled and specified way.
