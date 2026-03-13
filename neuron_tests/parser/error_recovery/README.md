# Parser Error Recovery Specification

## Scope

This folder specifies how the parser behaves after syntax errors. The oracle is not only the first failure, but also what useful structure or subsequent diagnostics survive afterward.

## Error Codes

| Code or range | Meaning |
| --- | --- |
| `N1500-N1599` | recovery and incomplete-source failures |
| `N1100-N1499` | primary syntax error categories that recovery starts from |
| `W1000-W1099` | recovery warnings when the parser continues |
| `NR1002` | current parser fallback |

## Test List

| Name | `.nr` input code | Expected output or error | Why important |
| --- | --- | --- | --- |
| `recover__missing_semicolon_then_valid_binding` | `x is 1\ny is 2;` | Error on first binding, second binding still parseable | Baseline recovery contract |
| `recover__unclosed_block_then_next_decl` | `Init is method() { Print("x");\nNext method() { return; }` | Error on missing `}`, later declaration still surfaced if recoverable | Proves top-level synchronization |
| `recover__bad_param_list_then_body` | `Add method(a as int b as int) as int { return a; }\nNext method() {}` | Parameter-list error with recovery into next declaration | Common header recovery scenario |
| `recover__extra_closing_brace` | `Init method() { Print("x"); }}\nNext method() {}` | Error on stray `}`, parser resumes cleanly | Avoids cascades from one extra delimiter |
| `recover__switch_case_partial_damage` | partial `switch`/`case` body followed by valid statement | Targeted error with later body statements still visible when possible | Stress-tests nested recovery |
| `recover__eof_in_string_or_block` | file ends during string or block | Single high-quality `N1500-N1599` diagnostic | Incomplete-file editing is a common IDE/user state |

## Edge Cases

- Two unrelated syntax errors in one file
- Error inside nested generic brackets followed by ordinary code
- Error inside `try/catch/finally` structure
- Recovery across blank lines and comments
- File that is almost valid except final token
- Cascading delimiter imbalance where only one primary error should be reported first

## Happy Path

Happy-path coverage here means successful recovery: the parser still produces enough structure for downstream phases or for additional parser diagnostics to remain useful.

## Error Path

Error-path tests must specify:

- the primary error
- whether follow-on errors are expected
- the minimum surviving declarations/statements that should still be available after recovery
