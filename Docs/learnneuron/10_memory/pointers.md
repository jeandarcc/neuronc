# Pointers

Neuron uses readable keywords `address of` and `value of` instead of `&` and `*`.

---

## Getting a Pointer

```npp
x is 42;
p is address of x;    // p points to x
```

---

## Dereferencing

```npp
Print(value of p);     // prints 42
```

---

## Writing Through a Pointer

```npp
value of p is 99;      // x is now 99
Print(x);              // 99
```

---

## Real Example

From `Node.nr`:

```npp
Init method() {
    x is 42;
    pointerValue is address of x;
    Print(value of pointerValue);     // 42
}
```

---

## C++ Comparison

| Neuron | C++ | Description |
|----------|-----|-------------|
| `p is address of x;` | `auto p = &x;` | Get memory address |
| `value of p` | `*p` | Dereference pointer |
| `value of p is 99;` | `*p = 99;` | Write through pointer |
| `p is address of y; q is p;` | `int** q = &p;` | Pointer alias (reference) |
| `q is another p;` | `int* q = p;` | Pointer copy |

## Pointer Chains
Neuron supports natural pointer chains:
`pp is address of address of x;`

To access the value:
`value of value of pp` (Equivalent to `**pp` in C++)

---

## Next Steps
- [Ownership](ownership.md) â€” Lifetime model
