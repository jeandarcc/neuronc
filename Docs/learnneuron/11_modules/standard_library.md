# Standard Library

Neuron includes built-in modules for common operations.

---

## Available Modules

| Module | Purpose | Key Functions |
|--------|---------|---------------|
| `System` | I/O, process control | `Print()`, `Exit()` |
| `Math` | Mathematical functions | `sqrt()`, `sin()`, `cos()`, `pow()` |
| `IO` | File operations | `ReadFile()`, `WriteFile()` |
| `Time` | Timestamps, timing | `Now()`, `Elapsed()` |
| `Random` | Random numbers | `Random()`, `Range()` |
| `Tensor` | Multi-dimensional arrays | `Random()`, `Ones()`, `Zeros()` |
| `NN` | Neural networks | `SelfTest()`, layers, optimizers |

---

## Usage

```npp
module System;
module Math;

Init method() {
    Print("Pi is: ");
    Print(Math.PI);
}
```

---

## Tensor Module

```npp
module Tensor;

Init method() {
    t is Tensor<float>.Random(3, 3);
    Print(t);
}
```

---

## NN Module

```npp
module NN;

Init method() {
    result is NN.SelfTest();
    Print(result);
}
```

---

## Next Steps

- [Error Handling](../12_error_handling/try_catch.md)
