# Parser (`src/parser/`)

The Parser compiles the `Token` stream provided by the Lexer into an Abstract
Syntax Tree (AST). It is a handwritten, recursive-descent parser heavily modularized
across several subdirectories to maintain readability.

## Files & Directories

| Directory / File | Subfiles | Purpose |
|------------------|----------|---------|
| `AST.cpp` | - | Defines base AST nodes (`ASTNode`, `Expr`, `Stmt`, `Decl`) and the internal AST memory arena. |
| `Parser.cpp` | - | The central parser object orchestrating the sub-parsers, providing token matching (`match()`, `consume()`, `peek()`) and synchronization for error recovery. |
| `core/` | `ParserCore.cpp` | Fundamental primitives: basic type parsing, path parsing, generic arguments parsing. |
| `declarations/` | `ParseTopLevel.cpp`<br>`ParseDeclarations.cpp`<br>`ParseTypeDeclarations.cpp`<br>`ParseGraphicsDeclarations.cpp` | Function signatures, struct/enum/interface/class definitions, global let bindings, `abstract` / `virtual` methods, and graphics/shader declarations. |
| `expressions/` | `ParseExpressions.cpp`<br>`ParsePrimaryExpressions.cpp`<br>`ParseExpressionSupport.cpp` | Handles Pratt parsing for operator precedence. Parses primary expressions (literals, identifiers), method calls, indexing, dot access, closures, and `match` expressions. |
| `statements/` | `ParseStatements.cpp`<br>`ParseControlFlow.cpp`<br>`ParseGraphicsStatements.cpp` | Parses `let`/`var` declarations, block scopes, `if`, `while`, `for`, `return`, `try/catch`, and the GPU block statements (`parallel`, `pass`, `canvas`). |

## Architecture Notes

### Recursive Descent & Pratt Parsing
The parser leverages standard recursive descent for statements and top-level declarations.
However, for binary and unary operators in `ParseExpressions.cpp`, it employs a Pratt parser
to accurately handle operator precedence without runaway recursion.

### Zero-Allocation AST
Nodes are allocated completely in an `ASTContext` provided by the Frontend. They are not
managed by `std::shared_ptr` or `std::unique_ptr`. The Parser just allocates raw pointers
into a memory arena, minimizing overhead, and guaranteeing all nodes are freed instantly
upon compiler shutdown.

### Error Recovery (`synchronize`)
When the parser encounters a syntax error, it emits an `N1xxx` diagnostic. But it does
not halt parsing. It enters panic mode and invokes a `synchronize()` function (often skipping
to the next semicolon or block brace). This allows the compiler to gather multiple
syntax errors globally without crashing.
