# Lexer (`src/lexer/`)

The Lexer is the first stage of the Neuron compiler pipeline. It converts raw
UTF-8 source bytes into a linear stream of `Token`s.

## Files

| File | Purpose |
|------|---------|
| `Lexer.cpp` | Main tokenizer state machine (`scanToken()`). Scans identifiers, strings, numbers, and operators. Single pass, no lookahead memory allocation context. |
| `Token.cpp` | Defines `Token` and `TokenType`. Maps enums to debug strings (e.g. `TokenType::FloatLiteral`). |

## Architecture Notes

### Supported Keywords
The lexer natively tracks `g_keywords` statically. Keywords include control flow (`if`, `match`, `for`), OOP constructs (`class`, `interface`, `inherits`, `constructor`), async and thread concurrency (`async`, `await`, `thread`, `atomic`), and GPU/graphics domain keywords (`gpu`, `canvas`, `shader`, `pass`).

### Multi-Word Keywords
A unique feature of the Lexer implemented via `checkMultiWordKeyword()` is looking ahead to support space-separated operator keywords. The lexer specifically identifies:
- `address of` -> `TokenType::AddressOf`
- `value of` -> `TokenType::ValueOf`

If the next word doesn't match, it elegantly restores Lexer state and falls back to a standard `Identifier`.

### Error Handling
The lexer **does not stop at the first error**. If it encounters an unterminated block comment (`/*` without `*/`) or invalid string escapes, it creates an `Error` token via `errorToken()` and appends it to `m_errors`, but keeps scanning the rest of the file so the diagnostics engine can report multiple lexing errors.

### Source Range Tracking
Every token captures `SourceLocation`, containing `line`, `column`, and the parsed `filename`. This allows the exact text range to be propagated throughout the pipeline to the semantic analysis phase.
