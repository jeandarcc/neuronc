# Runtime and Codegen Diagnostic Specification

## Scope

This folder covers diagnostics emitted after semantic success when execution or lowering fails in a user-visible way.

## Error Codes

| Code or range | Meaning |
| --- | --- |
| `N5000-N5099` | runtime/codegen-visible failure |
| `W5000-W5099` | runtime/codegen warning or fallback |
| `NR0001` | current generic fallback |

## Test List

| Name | `.nr` input code | Expected output or error | Why important |
| --- | --- | --- | --- |
| `diag_codegen__unsupported_lowering` | source construct that semantically passes but backend cannot lower | Diagnostic explains unsupported source feature | Users need source-level explanations for backend gaps |
| `diag_codegen__runtime_trap_message` | deterministic trap such as invalid runtime operation if defined | Runtime error message is stable and actionable | Prevents backend crashes with no context |
| `diag_codegen__backend_fallback_warning` | source that triggers slower but correct fallback path | Warning states fallback occurred without changing output semantics | Makes degraded execution observable |
| `diag_codegen__wrong_backend_module_usage` | backend-specific source feature used on unsupported target | Diagnostic names feature and target mismatch | Important for cross-platform source behavior |
| `diag_codegen__post_semantic_location_quality` | backend failure mapped back to original source line | Diagnostic still points to user source, not generated glue | Source mapping quality matters beyond parser/sema |

## Edge Cases

- Diagnostics after successful optimization
- Runtime failures inside called helpers rather than `Init`
- Source-mapped failures inside generated tensor or gpu calls
- Warnings paired with correct program output

## Happy Path

Happy-path tests here ensure successful codegen/runtime scenarios emit no backend-facing diagnostics.

## Error Path

Error-path tests should specify the observable failure channel: compile-time backend error, runtime stderr, exit code, or structured warning plus continued execution.
