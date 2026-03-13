# Neuron Language Reference

> **Status:** Living document â€” updated alongside language evolution.

## Type System

### Primitive Types

| Type | Size | Description |
|------|------|-------------|
| `i8`, `i16`, `i32`, `i64` | 1/2/4/8 bytes | Signed integers |
| `u8`, `u16`, `u32`, `u64` | 1/2/4/8 bytes | Unsigned integers |
| `f32`, `f64` | 4/8 bytes | IEEE 754 floating point |
| `bool` | 1 byte | Boolean (`true`/`false`) |
| `char` | 4 bytes | Unicode scalar value |
| `void` | â€” | Unit/no-value type |

### Composite Types

```neuron
// Struct
struct Point { x: f32, y: f32 }

// Enum (sum type)
enum Result<T, E> { Ok(T), Err(E) }

// Tuple
let pair: (i32, f32) = (1, 2.0)

// Slice
let nums: []i32 = [1, 2, 3]

// Pointer
let p: *i32 = &x
let mp: *mut i32 = &mut x
```

### Tensor Types

```neuron
// Static-shape tensor
let weights: tensor<f32, [128, 64]>

// Dynamic-shape tensor  
let input: tensor<f32, dyn>

// GPU-resident tensor
let gpu_buf: tensor<f32, [1024]> @gpu
```

---

## Declarations

### Functions

```neuron
fn add(a: i32, b: i32) -> i32 {
    return a + b
}

// Named return
fn divide(a: f64, b: f64) -> (result: f64, ok: bool) {
    if b == 0.0 { return (0.0, false) }
    return (a / b, true)
}

// Generic
fn max<T: Ord>(a: T, b: T) -> T {
    if a > b { a } else { b }
}
```

### Modules

```neuron
module Math {
    pub fn sqrt(x: f64) -> f64 { ... }
}
```

---

## Control Flow

```neuron
// Conditional
if x > 0 { ... } else if x < 0 { ... } else { ... }

// Loop forms
loop { ... }             // infinite
while condition { ... }
for item in collection { ... }
for i in 0..10 { ... }  // range

// Pattern matching
match value {
    0     => "zero",
    1..=9 => "single digit",
    _     => "other",
}

// Early exit
return value
break
continue
```

---

## GPU Scopes

```neuron
// Declare a GPU kernel
gpu fn matmul(a: tensor<f32, dyn>, b: tensor<f32, dyn>) -> tensor<f32, dyn> {
    // Runs on GPU â€” tensor ops auto-mapped to Vulkan/CUDA
}

// Fused kernel (optimizer eligible)
@fuse
gpu fn forward(x: tensor<f32, [128]>) -> tensor<f32, [64]> {
    let h = relu(x @ weights1 + bias1)
    return h @ weights2 + bias2
}
```

---

## Ownership

Neuron uses a lightweight ownership system:

```neuron
let a = String::new("hello")
let b = a             // moves ownership; `a` is now invalid
consume(b)            // b is dropped here

let c = String::new("world")
let d = &c            // immutable borrow; c still valid
let e = &mut c        // mutable borrow; no other borrows allowed
```

Rules:
1. A value has exactly one owner at a time.
2. Borrows are checked at compile time by `FlowAnalyzer` + `ReferenceTracker`.
3. `@gpu` tensors have extended lifetime rules managed by `GraphicsAnalyzer`.

---

## Package System (ncon)

```toml
# neuron.toml
[dependencies]
math = { github = "neuron-libs/math", version = "^1.0.0" }

[modulecpp.MyNativeLib]
sources = ["native/mylib.cpp"]
include_dirs = ["native/include"]
```

Import in code:

```neuron
import math
import MyNativeLib

fn main() {
    let x = math.sin(3.14)
}
```
