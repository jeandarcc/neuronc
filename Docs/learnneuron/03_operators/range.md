# Range Operator

The `..` operator creates a range between two values.

---

## Syntax

```npp
start..end
```

---

## Usage in For-In Loops

```npp
for(i in 0..10) {
    Print(i);
}
```

This iterates from `0` to `10`.

---

## Usage with Tensor Slicing

Ranges are used for tensor slicing operations:

```npp
slice is tensor[0..5];
```

---

## Next Steps

- [Methods](../04_methods/declaration.md) — Declaring functions
