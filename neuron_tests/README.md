# Neuron Source Behavior Test Architecture

## Scope

`neuron_tests/` is the canonical specification tree for black-box `.nr` behavior tests.

This tree answers one question only:

> When this `.nr` program is compiled and executed, what observable behavior is correct?

Included:

- Parse acceptance and parse rejection for user-written `.nr` source
- Semantic acceptance and semantic rejection visible to end users
- Ownership and aliasing behavior visible at compile time or runtime
- Runtime/codegen-visible program behavior such as arithmetic results, branching, data layout, and standard library side effects
- Diagnostic quality as seen by the user: code, severity, location, message intent, and recovery behavior
- Regression scenarios reproduced from fixed bugs

Explicitly excluded:

- LSP behavior
- Internal MIR/NIR/LLVM/unit-level compiler infrastructure tests
- IDE-only experiences
- Performance microbenchmarks unless the optimization changes observable program behavior
- Package manager, project generator, installer, or other CLI workflows unless they materially change `.nr` source execution

## Category Index

- [parser/README.md](parser/README.md): Syntax acceptance, syntax rejection, and parser recovery
- [semantic/README.md](semantic/README.md): Type rules, name resolution, generics, and overload resolution
- [ownership/README.md](ownership/README.md): Alias, copy, move, borrow, and lifetime behavior
- [codegen/README.md](codegen/README.md): Executable program behavior after successful compilation
- [stdlib/README.md](stdlib/README.md): Observable behavior of standard modules used from `.nr`
- [error_messages/README.md](error_messages/README.md): Diagnostic contracts, code coverage, wording, and source-location quality
- [regression/README.md](regression/README.md): Bug-reproduction tests that must never regress

## Error-Code Master List

The repository currently exposes coarse fallback codes such as `NR1002`, `NR2001`, and `NR9000` in the frontend diagnostics layer. This test architecture defines the finer-grained taxonomy that behavior tests should target. Until the compiler emits fully specific codes, tests may assert:

- Exact fine-grained code when available
- Otherwise the phase-level fallback code plus the stable message stem and source span

### Fatal/Error ranges

| Range | Area | Description |
| --- | --- | --- |
| `N1000-N1099` | Token and literal form | malformed literals, invalid escape sequences, illegal characters |
| `N1100-N1199` | Expression grammar | precedence, operand placement, malformed calls, malformed indexing |
| `N1200-N1299` | Statement grammar | missing separators, invalid control-flow form, malformed blocks |
| `N1300-N1399` | Function and declaration grammar | malformed parameter lists, duplicate modifiers, invalid generic arity syntax |
| `N1400-N1499` | Type grammar | malformed annotations, bad generic argument lists, invalid nullable/pointer/tensor syntax |
| `N1500-N1599` | Parser recovery and incomplete source | unmatched delimiters, missing terminators, recovery failure |
| `N2000-N2099` | Type checking | invalid assignments, invalid returns, invalid operator operand pairs |
| `N2100-N2199` | Type inference | ambiguous inference, null inference without context, conflicting branch inference |
| `N2200-N2299` | Scope and symbol resolution | unknown name, duplicate symbol, inaccessible symbol, shadowing violations |
| `N2300-N2399` | Generics | arity mismatch, unsatisfied constraints, invalid substitution |
| `N2400-N2499` | Overloading | no viable overload, ambiguous overload, inaccessible candidate |
| `N3000-N3099` | Move semantics | use-after-move, double move, move from invalid source |
| `N3100-N3199` | Borrowing and alias rules | write during active borrow, invalid aliasing, escaping borrow |
| `N3200-N3299` | Lifetime rules | returned dangling reference, captured expired value, scope-escape violations |
| `N4000-N4099` | Module and package use from source | missing modules, invalid imports, duplicate module declarations |
| `N5000-N5099` | Runtime/code generation behavior | unsupported lowering visible to the user, runtime trap, bad ABI-visible layout |

### Warning ranges

| Range | Area | Description |
| --- | --- | --- |
| `W1000-W1099` | Parser tolerances | recoverable syntax oddities, ignored trailing tokens, deprecated spellings |
| `W2000-W2099` | Semantic quality | unused bindings, dead branches, redundant casts, shadowing that is legal but risky |
| `W3000-W3099` | Ownership quality | unnecessary copy, needless move, suspicious alias chain |
| `W4000-W4099` | Module quality | redundant imports, duplicate module declarations tolerated by compatibility mode |
| `W5000-W5099` | Codegen/runtime quality | lossy lowering, backend fallback, optimization disabled by language construct |

### Fallback mapping to current frontend codes

| Current fallback | Meaning today | Expected replacement range |
| --- | --- | --- |
| `NR1001` | lexer/tokenization error | `N1000-N1099` |
| `NR1002` | parser error | `N1100-N1599` |
| `NR2001` | semantic error | `N2000-N3299` |
| `NR3001` | config-related error visible to source execution | case-specific; usually out of scope here |
| `NR4001` | module error | `N4000-N4099` |
| `NR9000` | generic warning | one of the `Wxxxx` ranges |
| `NR9001` | config warning | usually out of scope here |

## `.nr` Test Writing Conventions

- Treat every test as a user-observable contract, not an implementation probe.
- Prefer minimal `.nr` inputs that isolate one rule.
- Keep syntax tests source-only unless runtime execution is necessary to disambiguate behavior.
- For executable tests, specify `stdout`, `stderr`, process exit status, and whether output ordering matters.
- Always pin the expected phase: parse failure, semantic failure, compile success with runtime output, compile success with runtime trap, or warning-only success.
- Record the expected diagnostic code, severity, primary location, and the key message fragment that must remain stable.
- Avoid asserting internal IR text, optimizer pass names, or exact backend instruction sequences.
- Make tests deterministic: no wall-clock dependence, no uncontrolled randomness, no filesystem or network state unless the test explicitly targets stdlib I/O behavior.
- Normalize line endings and whitespace in expected output.
- Use one behavior per test file whenever possible; when bundling multiple expectations, explain why they cannot be separated.
- Distinguish aliasing (`is`), copying (`another`), and ownership transfer (`move`) explicitly in both test names and expectations.
- Prefer cross-platform semantics; if behavior is platform-sensitive, mark the invariant and the allowed divergence.
- When the compiler only emits a coarse fallback code, assert the fallback code plus the finer-grained category that the test belongs to.

## Coverage Targets

The tree should be rich enough that an agent can derive at least 5,000 meaningful tests by combining canonical scenarios with data-shape, nesting, whitespace, typing, and control-flow permutations.

Recommended minimum targets:

| Area | Baseline scenarios | Expected parameterized total |
| --- | --- | --- |
| Parser | 250 | 1,200+ |
| Semantic | 300 | 1,400+ |
| Ownership | 140 | 600+ |
| Codegen/runtime behavior | 220 | 900+ |
| Standard library | 120 | 500+ |
| Error messages | 80 | 250+ |
| Regression | 80 | 250+ |
| Total | 1,190 | 5,100+ |

Parameterization axes that should be reused everywhere:

- Different primitive types
- Nested blocks and scopes
- Single-line vs multi-line formatting
- Leading/trailing trivia and comments
- Aliased vs copied vs moved values
- Global-scope declarations vs method-local declarations
- Generic instantiation arity and substitution shape
- Backend-relevant integer/float width boundaries
- Empty, singleton, and large collection/tensor sizes where runtime behavior matters

## Test List

These root-level tests validate the architecture itself and should exist as smoke tests before the tree is filled out.

| Name | `.nr` input code | Expected output or error | Why important |
| --- | --- | --- | --- |
| `root_smoke__parse_and_run_hello` | `module System; Init is method() { Print("hello"); }` | Compiles and prints `hello` | Establishes the end-to-end happy path baseline |
| `root_smoke__parser_error_is_routed` | `Init is method( { }` | Parser failure with `N1xxx` or `NR1002` | Proves syntax failures are classified into the parser branch |
| `root_smoke__semantic_error_is_routed` | `Init is method() { x is "a" + 1; }` | Semantic failure with `N2xxx` or `NR2001` | Proves semantic failures are classified distinctly from parser failures |
| `root_smoke__ownership_error_is_routed` | `Init is method() { a is 1; b is move a; Print(a); }` | Ownership failure with `N3xxx` or semantic fallback | Prevents ownership diagnostics from being lost inside generic semantic buckets |
| `root_smoke__runtime_behavior_is_asserted` | `module System; Init is method() { Print(1 + 2); }` | Compiles and prints `3` | Confirms runtime-visible behavior is the test oracle, not internal IR |
| `root_smoke__diagnostic_location_is_asserted` | `x is` | Diagnostic points at the incomplete binding line and column | Keeps source mapping quality part of the contract |

## Edge Cases

- Empty files, whitespace-only files, and comment-only files
- Mixed success and failure in the same source file after recovery
- Programs with multiple modules or illegal repeated module declarations
- Platform-sensitive outputs such as newline conventions or floating formatting
- Programs that compile but intentionally trap or exit non-zero
- User code that shadows standard names like `Print`, `Input`, or `Math`
- Deep nesting that stresses parser recovery and scope tracking
- Single-feature scenarios and realistic composite programs both need coverage

## Happy Path

Happy-path tests in this tree compile cleanly and, when executed, produce a fully specified observable result. They must define:

- exact source program
- exact expected stdout/stderr
- exact exit behavior
- whether warnings are absent or explicitly expected

## Error Path

Error-path tests in this tree fail in a controlled, specified way. They must define:

- the earliest phase that should fail
- the expected diagnostic code or fallback code
- the required message intent
- the required location quality
- whether the compiler should recover and continue reporting follow-on issues
