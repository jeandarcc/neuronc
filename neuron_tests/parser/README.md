# Parser Behavior Specification

## Scope

This folder covers syntactic behavior of `.nr` programs before semantic analysis. It answers whether source text is accepted, rejected, or partially recovered into a usable parse result.

Included:

- Lexically valid token sequences that should parse
- Source forms that should fail with parser diagnostics
- Recovery behavior after malformed tokens or delimiters
- AST-shape-sensitive syntax decisions that later affect execution

Excluded:

- Type legality after successful parse
- Ownership semantics
- Runtime evaluation correctness

Subfolders:

- [literals/README.md](literals/README.md)
- [expressions/README.md](expressions/README.md)
- [statements/README.md](statements/README.md)
- [functions/README.md](functions/README.md)
- [types/README.md](types/README.md)
- [error_recovery/README.md](error_recovery/README.md)

## Error Codes

| Code or range | Meaning |
| --- | --- |
| `N1000-N1099` | literal/token form errors that surface during parse preparation |
| `N1100-N1199` | malformed expressions, operand placement, precedence-sensitive grammar errors |
| `N1200-N1299` | malformed statements and block syntax |
| `N1300-N1399` | malformed declarations and function signatures |
| `N1400-N1499` | malformed type syntax |
| `N1500-N1599` | recovery after missing delimiters, missing terminators, or incomplete files |
| `W1000-W1099` | parser recovery warnings and deprecated-but-accepted syntax |
| `NR1002` | current coarse parser fallback when fine-grained codes are unavailable |

## Test List

| Name | `.nr` input code | Expected output or error | Why important |
| --- | --- | --- | --- |
| `parser_baseline__empty_file` | `` | Parse succeeds or produces a well-defined empty program result | Defines the lower bound for source acceptance |
| `parser_baseline__module_and_method` | `module System; Init is method() { Print("ok"); }` | Parse succeeds with no syntax diagnostics | Establishes the canonical well-formed file |
| `parser_baseline__missing_semicolon` | `x is 1\ny is 2;` | Parser error in `N1200-N1299` and recovery into later declarations | Validates statement-boundary recovery |
| `parser_baseline__bad_parameter_form` | `sum is method(int a) as int { return a; }` | Parser error in `N1300-N1399` | Pins the languageâ€™s parameter grammar |
| `parser_baseline__unclosed_block` | `Init is method() { Print("x");` | Parser error in `N1500-N1599` | Ensures incomplete files report useful diagnostics |
| `parser_baseline__nested_expression_precedence` | `value is 1 + 2 * 3;` | Parse succeeds with multiplication binding tighter than addition | Keeps precedence rules stable |
| `parser_baseline__generic_type_spelling` | `box as Array<int>;` | Parse succeeds | Confirms type argument syntax is accepted |
| `parser_baseline__invalid_generic_type_spelling` | `box as Array<int;` | Parser error in `N1400-N1499` | Checks delimiter tracking inside type syntax |

## Edge Cases

- Legal syntax surrounded by comments, blank lines, or trailing whitespace
- Missing semicolons inside nested blocks rather than only at top level
- Extra closing delimiters after otherwise valid constructs
- Multiple parser errors in one file with continued recovery
- Valid constructs at file end without a trailing newline
- Deeply nested parentheses, brackets, and braces
- Mixed declaration and statement forms in the same method body

## Happy Path

Parser happy-path tests should prove that valid source forms remain accepted across formatting variants, nesting depth, and benign trivia changes.

## Error Path

Parser error-path tests should verify not only rejection, but also:

- primary error code/range
- primary location
- whether the parser recovers
- whether later declarations or statements remain discoverable
