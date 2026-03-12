# Language Server Protocol (`src/lsp/`)

The `neuron-lsp` binary (built from this directory) is a fully compliant Language Server Protocol 3.17 server providing IDE intelligence for Neuron++.

## Architecture 

The LSP runs the compiler frontend and `sema` passes *in-memory*. It maintains a live
`DocumentManager` that tracks typed changes without hitting the disk, feeding them
into the `AnalysisContext`.

### Core Lifecycle & Networking
- `NeuronLspServer.cpp/.h`: The main thread loop, listening on stdin/stdout.
- `LspTransport.cpp`, `LspProtocol.cpp`: JSON-RPC 2.0 message parsing, routing, and serialization.
- `LspDispatcher.cpp`: Routes parsed JSON-RPC method calls to specific Handlers.
- `LspTypes.h`, `LspFeaturePayloads.cpp`: Massive boilerplate defining the LSP 3.17 JSON structs (Positions, Ranges, TextEdit, etc).

### Document & AST Management
- `LspWorkspace.cpp`, `DocumentManager.cpp`: Tracks open files and their dirty states.
- `LspPath.cpp`, `LspText.cpp`: High-performance translation between file `file:///` URIs, absolute paths, and native OS representations.
- `LspAst.cpp`, `LspDocumentQueries.cpp`: The bridge to `src/sema/`. Maps a text cursor position (`line:character`) to a semantic AST node.

## Feature Handlers

Each IDE capability is implemented in a dedicated handler class invoked by the Dispatcher:

| File | Feature Provided |
|------|------------------|
| `CodeActionHandler.cpp` | Quick fixes (e.g. auto-imports, suggested casts). |
| `CompletionHandler.cpp` | Context-aware intellisense and snippets (assisted by `LspCompletionSupport.cpp`). |
| `DefinitionHandler.cpp` | Go to Definition / Type Definition. |
| `DiagnosticsHandler.cpp`| Live error squiggles (bridges `DiagnosticEmitter` to LSP Diagnostics). |
| `HoverHandler.cpp` | Tooltips showing types, signatures, and doc-comments (assisted by `LspHoverSupport.cpp`). |
| `RenameHandler.cpp` | Safely renaming variables/functions across the workspace. |
| `SymbolsHandler.cpp` | Document Outline (breadcrumbs). |
| `WorkspaceSymbolsHandler.cpp`| Project-wide symbol search (`Ctrl+T` / `Cmd+T`). |
| `CallHierarchyHandler.cpp` | Find References / "Who calls this?". |
| `TypeHierarchyHandler.cpp` | Subclass/trait implementations tree exploration. |
| `SymbolsInlayHints.cpp` | Inline parameter names and inferred types shown in grey text. |

## Internal Tooling
- `DebugViewHandler.cpp` & `LspDebugViews.cpp`: Custom JSON-RPC endpoints not in the LSP spec. Used by `vscode-npp` to render interactive "Compiler AST Explorer" and "NIR Explorer" views.
