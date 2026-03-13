# Main Script

This document describes `src/Main.nr`, the program entry point for this Neuron project.

## Purpose

`Main.nr` serves as the primary entry point for the project. When the project is executed via `neuron run` or `neuron ncon watch`, the runtime locates this file, finds the `Init` method, and invokes it as the first user-level code to run.

## Entry Point

The `Init` method is the program's entry point — analogous to `main()` in C/C++ or `Main()` in C#. It is automatically invoked by the Neuron runtime during program startup; no manual call is required.

```nr
Init is method() {
    Print("Hello Neuron!");
}
```

## Module Imports

To use functionality from other modules, add `module <Name>;` statements at the top of the file, before the `Init` method:

```nr
module Vector2;
module Math;

Init is method() {
    v is Vector2(3.0, 4.0);
    Print(v.Length());
}
```

## Execution Order

1. The runtime initializes core subsystems (memory, thread pool, tensor engine).
2. All imported modules are loaded and their `Init` methods are called in dependency order.
3. The `Init` method in `Main.nr` is executed.
4. When `Init` returns, the program exits.

## Notes

- `Main.nr` must be located at `src/Main.nr` (configurable via `build.main` in `neuron.toml`).
- The `Init` method takes no parameters and returns no value.
- This file is subject to all standard code rules defined in `.neuronsettings`.
