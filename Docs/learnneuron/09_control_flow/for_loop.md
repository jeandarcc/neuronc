# For Loop

C-style counted loop with init, condition, and increment.

---

## Syntax

```npp
for (init; condition; increment) {
    // body
}
```

---

## Example

```npp
for (i is 0; i < 10; i++) {
    Print(i);
}
```

---

## Counting Down

```npp
for (i is 10; i > 0; i--) {
    Print(i);
}
```

---

## Nested Loops

```npp
for (i is 0; i < 3; i++) {
    for (j is 0; j < 3; j++) {
        Print(i * 3 + j);
    }
}
```

---

## Next Steps

- [For-In Loop](for_in.md) — Range-based iteration
- [Parallel For](parallel_for.md) — Data-parallel loops
