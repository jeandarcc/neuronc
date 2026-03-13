# Parser Diagnostic Specification

## Scope

This folder covers the quality of parser-facing diagnostics: syntax errors, recovery diagnostics, and lexer-adjacent literal diagnostics as presented to the user.

## Error Codes

| Code or range | Meaning |
| --- | --- |
| `N1000-N1099` | malformed literal/token diagnostics |
| `N1100-N1499` | malformed expression/statement/declaration/type diagnostics |
| `N1500-N1599` | incomplete-source and recovery diagnostics |
| `W1000-W1099` | recovery warnings |
| `NR1001`, `NR1002` | current fallbacks |

## Test List

| Name | `.nr` input code | Expected output or error | Why important |
| --- | --- | --- | --- |
| `diag_parser__missing_semicolon_message` | `x is 1\ny is 2;` | Diagnostic explains statement terminator expectation and points near line 1 end | Common user error; wording must be actionable |
| `diag_parser__unterminated_string_span` | `name is "Neuron;` | Diagnostic points at string start or end consistently with `N1000-N1099` | Span quality matters more than prose polish |
| `diag_parser__bad_parameter_form` | `Add method(int a) as int { return a; }` | Diagnostic explicitly says parameter name must precede `as` | Teaches the language grammar |
| `diag_parser__unexpected_token_after_expr` | `value is 1 + ) ;` | Primary span highlights the unexpected token | Prevents vague “parse failed” messages |
| `diag_parser__recovery_keeps_second_error` | file with two independent parser errors | Both diagnostics remain visible in stable order or stable set | Recovery behavior is part of UX |

## Edge Cases

- Multi-line diagnostics for unclosed blocks
- Nested generic-bracket errors
- Error at first token vs last token in file
- Single-line vs multi-line source span formatting
- Presence of comments around the error site

## Happy Path

Happy-path tests here assert that valid parser scenarios produce no diagnostics at all.

## Error Path

Error-path tests should pin:

- code or fallback code
- severity
- line/column quality
- message stem
- recovery continuation behavior
