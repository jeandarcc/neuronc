# Diagnostic Quality Specification

## Scope

This folder exists for tests whose main oracle is the diagnostic itself: code, severity, message intent, source span, recovery quality, and whether the user receives actionable information.

Subfolders:

- [N1xxx_parser/README.md](N1xxx_parser/README.md)
- [N2xxx_semantic/README.md](N2xxx_semantic/README.md)
- [N3xxx_ownership/README.md](N3xxx_ownership/README.md)
- [N4xxx_module/README.md](N4xxx_module/README.md)
- [N5xxx_codegen/README.md](N5xxx_codegen/README.md)

These tests are not duplicates of the behavior folders. They assert the quality and stability of diagnostics emitted for those behaviors.

## Error Codes

| Code or range | Meaning |
| --- | --- |
| `N1xxx` | parser-facing diagnostics |
| `N2xxx` | semantic-facing diagnostics |
| `N3xxx` | ownership-facing diagnostics |
| `N4xxx` | module/import diagnostics |
| `N5xxx` | runtime/codegen diagnostics |
| `Wxxxx` | warning families paired with those ranges |

## Test List

| Name | `.npp` input code | Expected output or error | Why important |
| --- | --- | --- | --- |
| `diag_baseline__primary_span_is_precise` | malformed single-line binding | Diagnostic points at the offending token, not line start only | Source mapping quality matters as much as the message text |
| `diag_baseline__message_explains_expected_form` | malformed parameter syntax | Message explains what syntax was expected | Keeps diagnostics teachable |
| `diag_baseline__warning_does_not_block_execution` | warning-only program | Warning emitted, binary still runs | Confirms severity handling |
| `diag_baseline__multiple_errors_are_stable` | file with two independent failures | Two diagnostics in stable order or stable set | Prevents regressions in recovery behavior |
| `diag_baseline__fallback_code_is_allowed_temporarily` | known coarse-code scenario | Accepts `NPPxxxx` fallback while demanding message intent | Bridges current implementation to future taxonomy |

## Edge Cases

- Multi-line spans
- Diagnostics inside generic argument lists or nested expressions
- Message text that should mention both source and destination types
- Diagnostics that include secondary notes such as move origin or candidate overloads
- Recovery where the second error must not replace the first primary message

## Happy Path

Happy-path tests here are warning-only outcomes or intentionally absent diagnostics. They prove the compiler stays quiet when it should.

## Error Path

Error-path tests here must pin the diagnostic contract more tightly than behavior tests elsewhere: code, severity, span, stable phrasing, and follow-on recovery behavior.
