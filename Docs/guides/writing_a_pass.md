# How to Write a Compiler Pass

Passes in Neuron++ operate at two levels: **NIR passes** (optimizer) and
**Sema passes** (semantic analysis). This guide covers both.

---

## NIR Optimizer Pass

NIR passes are run by the pass manager in `src/nir/Optimizer.cpp`.

### 1. Create the pass file

Create `src/nir/OptimizerMyPass.cpp`:

```cpp
#include "OptimizerInternal.h"

// Entry point — called by the pass manager on every NIR function
void runMyPass(NIRFunction& fn, OptimizerContext& ctx) {
    for (auto& bb : fn.basicBlocks()) {
        for (auto& instr : bb.instructions()) {
            // Inspect and transform instructions here
        }
    }
}
```

### 2. Register the pass

In `src/nir/Optimizer.cpp`, add to the pass pipeline:

```cpp
// In runOptimizerPipeline():
runMyPass(fn, ctx);
```

### 3. Add to CMakeLists

In `src/nir/CMakeLists.txt`:

```cmake
target_sources(neuronc_nir PRIVATE
    ...
    OptimizerMyPass.cpp
)
```

### 4. Verify

The pass can be tested by dumping NIR before/after:

```powershell
neuron nir path/to/test.npp   # dumps NIR to stdout
```

Use `src/nir/OptimizerUtils.cpp` utilities for common patterns (instruction
removal, value substitution).

---

## Semantic Analysis Pass

Semantic passes hang off of `SemanticDriver` (`src/sema/SemanticDriver.cpp`).

### 1. Create the analyzer file

Create `src/sema/MyAnalyzer.cpp` + `MyAnalyzer.h`:

```cpp
// MyAnalyzer.h
#pragma once
#include "AnalysisContext.h"

class MyAnalyzer {
public:
    explicit MyAnalyzer(AnalysisContext& ctx) : ctx_(ctx) {}
    void analyze(const ASTNode& node);
private:
    AnalysisContext& ctx_;
};
```

```cpp
// MyAnalyzer.cpp
#include "MyAnalyzer.h"

void MyAnalyzer::analyze(const ASTNode& node) {
    // Walk the AST, use ctx_.emitter to emit diagnostics
    // Use ctx_.symbolTable() for name lookups
    // Use ctx_.typeArena() to construct types
}
```

### 2. Invoke from SemanticDriver

In `src/sema/SemanticDriver.cpp`:

```cpp
#include "MyAnalyzer.h"

// In SemanticDriver::run():
MyAnalyzer myAnalyzer(ctx_);
myAnalyzer.analyze(ast.root());
```

### 3. CMakeLists

In `src/sema/CMakeLists.txt`:

```cmake
target_sources(neuronc_sema PRIVATE
    ...
    MyAnalyzer.cpp
)
```

---

## Testing Your Pass

Add a test in `tests/sema/` or `tests/nir/`:

```cpp
TEST("MyPass: <description>") {
    auto result = compile("... .npp snippet ...");
    // Assert no diagnostics, or specific diagnostics
    EXPECT_CLEAN(result);
}
```

Run targeted:
```powershell
powershell -File scripts/build_tests.ps1 -Filter "nir*my_pass*" ...
```

---

## Design Principles

- **Passes must be incremental:** avoid re-analyzing nodes that haven't changed.
- **Never mutate the AST from an NIR pass** — work on NIR instructions only.
- **Emit diagnostics via `ctx_.emitter`** — never use `printf`/`std::cerr` directly.
- **Consult `AnalysisContext`** for all shared state; never hold raw pointers across pass boundaries.
