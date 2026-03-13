# Neuron Test Suite (`tests/`)

Neuron ships a unified, single-binary test suite. Instead of compiling hundreds of
tiny executables, all tests are linked into the `neuron_tests.exe` binary.

This repository structure exactly mirrors `src/`. For example, tests for
`src/sema/` are located in `tests/sema/`.

## The `test_main.cpp` Entry Point
The root of this directory contains `test_main.cpp`, which initializes the Catch2
test runner. It sets up the global `AppGlobals` and prepares the memory arenas
before any test cases are executed.

## Running Tests

Never run CMake/CTest directly. The canonical runners are:

### Legacy Runner
```powershell
powershell -File scripts/build_tests.ps1 -BuildDir "C:\Users\...\build-mingw" -Filter "sema*"
```

### V2 Runner (`build_tests_v2.ps1`)
The modern runner supports parallel execution and better output formatting.
```powershell
powershell -File scripts/build_tests_v2.ps1 -BuildDir "C:\Users\...\build-mingw" -Filter "*Ownership*"
```

## Creating a Test

1. Navigate to the appropriate subsystem (e.g., `tests/parser/`).
2. Open or create a `.cpp` file (e.g., `test_parser_structs.cpp`).
3. Use the global `compile()` test fixture macro.

```cpp
#include "TestingFramework.h"

TEST_CASE("Struct Parsing", "[parser]") {
    auto result = compile("struct Point { x: i32 }");
    REQUIRE(result.success());
}
```

## Adding Source files to Tests

When you create a new C++ test file, you must add it to the `neuron_tests` executable in the root `CMakeLists.txt`.

## `fixtures/`
The `tests/fixtures/` directory is for `.nr` source files, C/C++ FFI files (`test_c.c`, `tmp_compile_test.cpp`), and shaders (`triangle.frag/.vert`) that
are larger than inline string literals. Use `compileFile("tests/fixtures/my_test.nr")`
to test them.
