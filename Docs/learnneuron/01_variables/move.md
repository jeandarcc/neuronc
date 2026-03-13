# Move Semantics

The `move` keyword transfers ownership of a value from one variable to another. After the move, the original variable is invalidated.

---

## Syntax

```npp
x is 100;
y is move x;    // ownership transferred to y

Print(y);       // 100
Print(x);       // undefined — x has been moved
```

---

## How Move Differs from Copy and Alias

| Statement | Behavior | Original after? |
|-----------|----------|----------------|
| `y is x;` | Alias — shared memory | Still valid (shared) |
| `y is another x;` | Deep copy | Still valid (independent) |
| `y is move x;` | Transfer ownership | **Invalidated** |

---

## When to Use `move`

- When transferring ownership of a large data structure without copying
- When the original variable is no longer needed
- For resource management patterns (file handles, GPU buffers)

---

## Example

```npp
Init method() {
    x is 100;
    y is move x;
    z is x;         // z binds to x's (now-moved) memory

    Print(y);       // 100
    Print(z);       // may be undefined
}
```

---

## Shorthand (Without `is`)

```npp
a 100;
b move a;       // move without 'is'
```

---

## C++ Equivalent

| Neuron | C++ |
|----------|-----|
| `y is move x;` | `T y = std::move(x);` |

---

## Next Steps

- [Primitive Types](../02_types/primitive_types.md) — Available data types
- [Memory Model](../10_memory/alias_semantics.md) — Deep dive into alias, copy, and move
