# Neuron IR (NIR) Specification

> **Status:** Living document — tracks `src/nir/` implementation.

## Overview

NIR is a **typed, SSA-based, high-level intermediate representation** that sits
between the semantic AST and the LLVM IR. Key properties:

- Typed: every SSA value carries a `NIRType`.
- High-level: preserves ownership annotations, tensor shapes, GPU scopes.
- Module-scoped: a `NIRModule` contains `NIRFunction`s, globals, and type declarations.
- Immutable reference semantics: instructions reference values by ID.

---

## Module Structure

```
NIRModule
  ├── globals: Vec<NIRGlobal>
  ├── type_decls: Vec<NIRTypeDecl>
  └── functions: Vec<NIRFunction>

NIRFunction
  ├── params: Vec<(name, NIRType)>
  ├── return_type: NIRType
  └── basic_blocks: Vec<NIRBlock>

NIRBlock
  ├── name: Label
  └── instructions: Vec<NIRInstr>
```

---

## Type System

| NIR Type | Representation |
|----------|---------------|
| `i8`–`i64`, `u8`–`u64` | Integer types |
| `f32`, `f64` | Float types |
| `bool` | `i1` |
| `ptr<T>` | Pointer to T |
| `ref<T>` | Borrow reference |
| `slice<T>` | Fat pointer (ptr + len) |
| `tensor<T, Shape>` | Tensor descriptor |
| `fn(Args) -> Ret` | Function type |
| `struct { fields }` | Named struct type |
| `enum { variants }` | Tagged union |

---

## Instruction Set

### Value Instructions

| Mnemonic | Semantics |
|----------|-----------|
| `const <type> <value>` | Constant literal |
| `alloca <type>` | Stack allocation |
| `load <ptr>` | Load from pointer |
| `store <val>, <ptr>` | Store to pointer |
| `gep <ptr>, <indices>` | Get element pointer |
| `cast <val> to <type>` | Type cast |

### Arithmetic

| Mnemonic | Description |
|----------|-------------|
| `add`, `sub`, `mul`, `div` | Integer/float arithmetic |
| `rem` | Remainder |
| `neg` | Negate |
| `and`, `or`, `xor`, `shl`, `shr` | Bitwise |

### Control Flow

| Mnemonic | Description |
|----------|-------------|
| `br <label>` | Unconditional branch |
| `br_cond <val>, <then>, <else>` | Conditional branch |
| `ret <val>` | Return value |
| `ret void` | Return from void function |
| `phi [<label>: <val>]+` | SSA phi node |

### Function Calls

| Mnemonic | Description |
|----------|-------------|
| `call <fn>(<args>)` | Direct call |
| `icall <fn_ptr>(<args>)` | Indirect call through pointer |
| `tail call <fn>(<args>)` | Tail call (eligible for TCO) |

### Ownership

| Mnemonic | Description |
|----------|-------------|
| `move <val>` | Transfer ownership |
| `borrow <val>` | Immutable borrow |
| `borrow_mut <val>` | Mutable borrow |
| `drop <val>` | Invoke destructor |

### Tensor Instructions

| Mnemonic | Description |
|----------|-------------|
| `tensor_create <type> <shape>` | Allocate tensor |
| `tensor_load <tensor> <idx>` | Element access |
| `tensor_store <val>, <tensor> <idx>` | Element write |
| `tensor_op <op> <args>` | High-level tensor op (matmul, conv, etc.) |
| `gpu_dispatch <kernel> <args>` | GPU kernel dispatch |

---

## Optimizer Pass Ordering

```
1. Cleanup (DCE, constant folding)
2. GPU Scope Lifting
3. Tensor Fusion
4. Cleanup (again — fusion may expose new DCE opportunities)
```

Each pass is idempotent and can be run multiple times safely.

---

## Text Format Example

```nir
define fn add(a: i32, b: i32) -> i32 {
entry:
  %r = add i32 %a, %b
  ret i32 %r
}

define fn relu(x: tensor<f32, [128]>) -> tensor<f32, [128]> {
entry:
  %zero = const f32 0.0
  %mask = tensor_op gt tensor<f32, [128]> %x, %zero
  %out  = tensor_op select tensor<f32, [128]> %mask, %x, %zero
  ret tensor<f32, [128]> %out
}
```
