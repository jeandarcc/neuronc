# Parser Statements Specification

## Scope

This folder specifies statement-level syntax inside methods and blocks.

Covered statement families:

- bindings and assignments
- `const` bindings
- `return`, `break`, and `continue`
- `if`, `else if`, `else`
- `while`, `for`, and `for in`
- `switch`/pattern-like statement forms
- `try`/`catch`/`finally`

## Error Codes

| Code or range | Meaning |
| --- | --- |
| `N1200-N1299` | malformed statement grammar |
| `N1500-N1599` | recovery after incomplete or unmatched blocks |
| `NR1002` | current parser fallback |

## Test List

| Name | `.nr` input code | Expected output or error | Why important |
| --- | --- | --- | --- |
| `stmt__binding_and_assignment` | `x is 1; x is 2;` | Parse succeeds | Baseline statement sequence |
| `stmt__const_binding` | `const answer is 42 as int;` | Parse succeeds | Confirms modifier placement |
| `stmt__if_else_chain` | `if(score > 0) { Print("a"); } else { Print("b"); }` | Parse succeeds | Core control-flow syntax |
| `stmt__while_loop` | `while(active) { active is false; }` | Parse succeeds | Pins loop block grammar |
| `stmt__for_loop` | `for(i is 0; i < 3; i++) { Print(i); }` | Parse succeeds | Common compound statement form |
| `stmt__try_catch_finally` | `try { Throw("x"); } catch(error) { Print(error); } finally { Print("done"); }` | Parse succeeds | Covers nested statement headers |
| `stmt__missing_block_after_if` | `if(flag) Print("x");` | Either parse succeeds if single-line form is legal, or `N1200-N1299` if blocks are required | Forces the grammar contract to be explicit |
| `stmt__dangling_else_or_unclosed_block` | `if(flag) { Print("x"); else { Print("y"); }` | `N1200-N1299` or `N1500-N1599` | Checks block-structure recovery |

## Edge Cases

- Single-line control flow without braces if supported
- Nested loops with inner `break` and `continue`
- Empty blocks `{}`
- Multiple semicolons or stray separators between statements
- `return` at file end and within nested blocks
- Interleaving declarations and executable statements

## Happy Path

Happy-path tests should exercise every statement form in:

- an isolated method
- nested block context
- mixed statement sequences

## Error Path

Error-path tests should specify whether the parser should recover to the next statement boundary, the next block close, or the next top-level declaration.
