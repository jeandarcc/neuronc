# Machine IR (`src/mir/`)

> **Status:** Bridging High-Level NIR to Low-Level LLVM IR.

MIR (Machine Intermediate Representation) is the lower-level intermediate language
that immediately precedes LLVM code generation. It is built from NIR after
high-level optimizations (tensor fusion, loop-lifting) have completed.

## Architecture & Subsystems

| File | Purpose |
|------|---------|
| `MIRBuilder.cpp` | Main driver for converting a `NIRModule` into an untyped, closer-to-hardware MIR representation. Connects the frontend to the backend. |
| `MIRBuilderBindings.cpp` | Lowers high-level variable scopes and lifetimes into flat stack slots or registers. Removes nesting in favor of labels. |
| `MIRBuilderLowering.cpp` | Lowers calls, loops, and complex NIR instructions into more atomic machine operations. |
| `MIROwnershipPass.cpp` | The crucial memory management pass. Iterates over the MIR graph to track liveness and insert `drop` (destructor) calls at the exact points where ownership is relinquished or scopes end. |
| `MIRPrinter.cpp` | Handles dumping the MIR to a human-readable text format for debugging compiler logic without needing to read LLVM IR assemblies. |

## The Ownership Enforcement (`MIROwnershipPass`)

Neuron does not use a tracing garbage collector. Instead, ownership semantics
dictate object lifespans. 

While ownership violations (use-after-move, borrow-checker errors) are caught in
`src/sema/` (`ReferenceTracker.cpp`), the actual *freeing* of memory is handled
here. The `MIROwnershipPass` guarantees that memory allocations mapped to local
variables are injected with a `drop` instruction exactly when the variable falls
out of scope.

This is fundamentally easier to do on MIR than LLVM IR, because MIR retains a simpler,
higher-level control-flow graph (CFG) making liveness analysis straightforward.
