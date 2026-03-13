?# Neuron Language Guide

## Overview

Neuron (Neuron) is a high-performance, compiled programming language designed for artificial intelligence, scientific computing, and GPU-accelerated workloads. It combines the raw performance of C++ with the readability of modern high-level languages.

### Execution Paths

Neuron supports **two execution paths**:

**1. LLVM Native Compilation** — For maximum performance production builds:
```
Neuron Source → Lexer → Parser → AST → Semantic Analysis → NIR → LLVM IR → Native Binary
```

**2. NCON Bytecode Containers** — For sandboxed, portable, and hot-reloadable execution:
```
Neuron Source → Lexer → Parser → AST → NIR → Bytecode → .ncon Container → VM Execution
```

The NCON path compiles source into portable bytecode containers (`.ncon` files) that run inside a sandboxed virtual machine with security isolation, resource control, and live hot-reload support. This is the default path used by `neuron run` and `neuron ncon watch`.

---

## Core Syntax

### Variable Binding (`is`)

All variable declarations and assignments use the `is` keyword:

```nr
x is 10;
y is 3.14 as float;
name is "Neuron" as string;
```

### Type Annotation (`as`)

Explicit types are specified with `as`. When omitted, the compiler infers the type automatically:

```nr
counter is 0 as int;       // explicit type
ratio is 0.5;              // inferred as float
```

### Copying (`another`)

Use `another` to create an independent deep copy:

```nr
original is 42;
copied is another original;  // separate memory, same value
```

### Aliasing (`is` without `another`)

Assigning without `another` creates a reference alias — both names share the same memory:

```nr
x is 10;
y is x;      // y is an alias of x (shared memory)
y is 20;     // x is now also 20
```

---

## Memory Model

Neuron provides four fundamental memory primitives:

| Keyword        | Meaning                           | C++ Equivalent  |
|----------------|-----------------------------------|-----------------|
| `is`           | Alias / reference binding         | `T& y = x;`    |
| `another`      | Independent value copy            | `T y = x;`     |
| `address of`   | Obtain pointer to variable        | `&x`           |
| `value of`     | Dereference a pointer             | `*p`           |

### Pointer Example

```nr
x is 10;
p is address of x;
Print(value of p);      // prints 10
value of p is 99;       // x is now 99
```

---

## Functions (Methods)

All callable units — named functions, lambdas, and callbacks — are declared with `method`:

```nr
Add is method(a as int, b as int) as int {
    return a + b;
};

// Lambda / anonymous method
Run(method() {
    Print("Executed");
});
```

### Callback Pattern

```nr
Execute is method(action as method) {
    action();
};
```

---

## Classes and Object-Oriented Programming

Classes follow a one-class-per-file rule. The class name **must** match the filename:

```nr
// File: Vector2.nr
Vector2 is public class {
    x is 0.0 as float;
    y is 0.0 as float;

    constructor is public method(x as float, y as float) {
        this.x is x;
        this.y is y;
    }

    Length is public method() as float {
        return Math.sqrt(x * x + y * y);
    }
}
```

### Inheritance

```nr
Dog is public class inherits Animal, ISerializable {
    Speak is public method() {
        Print("Woof");
    }
}
```

---

## Control Flow

| Construct        | Description                                |
|------------------|--------------------------------------------|
| `if / else`      | Conditional branching                      |
| `while`          | Condition-based loop                       |
| `for`            | Classic C-style counted loop               |
| `for-in`         | Range-based iteration over collections     |
| `parallel for`   | Data-parallel loop (CPU threads / GPU)     |
| `break`          | Exit loop early                            |
| `continue`       | Skip to next iteration                     |

```nr
for(i is 0; i < 100; i++) {
    if(i == 50) { break; }
    Print(i);
}

for(element in collection) {
    Process(element);
}
```

---

## Tensor and GPU Computing

Neuron has first-class support for multi-dimensional tensors and GPU execution:

```nr
A is Tensor<float>.Random(1024, 1024);
B is Tensor<float>.Ones(1024, 1024);

C is A + B;          // element-wise addition
D is A @ B;          // matrix multiplication

gpu {
    result is A @ B;  // GPU-accelerated matmul
}
```

### GPU Block Semantics

The `gpu { ... }` block opts into GPU-prefer execution with automatic CPU fallback. Backend priority: **CUDA > Vulkan > CPU**.

---

## Module System

Every `.nr` file is a module. Import modules with the `module` keyword:

```nr
module Vector2;
module Math;

Init is method() {
    v is Vector2(3.0, 4.0);
    Print(v.Length());
};
```

### Program Entry Point

Execution begins at the `Init` method inside `src/Main.nr`. When a project is run, the runtime locates `Main.nr`, finds the `Init` method, and executes it:

```nr
Init is method() {
    Print("Hello, Neuron!");
};
```

---

## Error Handling

Neuron supports structured exception handling:

```nr
try {
    result is Divide(10, 0);
}
catch(DivideByZeroError err) {
    Print("Cannot divide by zero");
}
catch(error) {
    Print(error);
}
finally {
    Cleanup();
}
```

---

## Generics

Generic types and methods use angle-bracket syntax with optional constraints:

```nr
Swap is method<T>(a as T, b as T) {
    temp is another a;
    a is b;
    b is temp;
};

Add<T:Numeric> is method(a as T, b as T) as T {
    return a + b;
};
```

---

## Concurrency

```nr
// Parallel CPU loop
parallel for(i is 0; i < data.Length; i++) {
    data[i] is data[i] * 2;
}

// Async methods
LoadData is async method() as Tensor<float> {
    return ReadDataset();
};

result is await LoadData();
```

---

## Standard Library Modules

| Module   | Purpose                                    |
|----------|--------------------------------------------|
| `System` | I/O, process control (`Print`, `Exit`)     |
| `Math`   | Mathematical functions (`sqrt`, `sin`)     |
| `IO`     | File read/write operations                 |
| `Time`   | Timestamps and duration measurement        |
| `Random` | Random number generation                   |
| `Tensor` | Multi-dimensional numeric operations       |

---

## Toolchain & CLI

The `neuron` command is the unified entry point for all toolchain operations:

### Project Management

| Command                  | Description                                    |
|--------------------------|------------------------------------------------|
| `neuron new <name>`      | Create a new Neuron project                  |
| `neuron build`           | Build the current project (LLVM native)        |
| `neuron run`             | Build and run via NCON container               |
| `neuron add <package>`   | Add a package dependency                       |
| `neuron remove <package>`| Remove a package dependency                    |
| `neuron update`          | Update all dependencies                        |
| `neuron publish`         | Create a `.nrkg` package artifact             |
| `neuron packages`        | List registry packages                         |
| `neuron release`         | Build, test, and package a release bundle      |

### NCON Container System

NCON (Neuron Container) is the portable bytecode execution system:

| Command                          | Description                                     |
|----------------------------------|-------------------------------------------------|
| `neuron ncon build [dir] [-o f]` | Compile project into a `.ncon` container        |
| `neuron ncon run <file.ncon>`    | Execute a `.ncon` container in the sandboxed VM |
| `neuron ncon watch [dir]`        | Watch for changes and auto-rebuild/hot-reload   |
| `neuron ncon inspect <file.ncon>`| Inspect container contents (human or `--json`)  |
| `neuron ncon config set <k> <v>` | Set NCON configuration (e.g., `hot_reload`)     |

The `watch` command monitors source files and automatically rebuilds the container on change. With hot-reload enabled (`--hot-reload` or `ncon.hot_reload = true` in `neuron.toml`), compatible changes are patched live into the running program without restart.

The NCON sandbox provides security isolation via restricted process tokens, job objects, and configurable file system permissions.

### Debug Commands

| Command                  | Description                                   |
|--------------------------|-----------------------------------------------|
| `neuron lex <file>`      | Tokenize a `.nr` file                        |
| `neuron parse <file>`    | Parse and print AST                           |
| `neuron nir <file>`      | Generate and print NIR                        |
| `neuron compile <file>`  | Compile a `.nr` file to native executable    |

All debug commands accept `--trace-errors` for source-context traces.

---

## Further Reading

Refer to the files in `agents/language/Details/` for in-depth coverage of specific topics:

- **RULES.md** — Core language rules and compiler constraints
- **NAMING.md** — Naming conventions for methods, variables, classes, and constants
- **STRUCTURE.md** — Recommended project layout and file organization
- **ERROR_GUIDE.md** — Common compiler diagnostics and how to resolve them
- **GPU_SEMANTICS.md** — GPU block execution model, backend selection, and kernel dispatch
