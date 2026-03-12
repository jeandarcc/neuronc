# Break, Continue, Return

Control the flow of execution within loops and methods.

---

## `break`

Exit a loop immediately:

```npp
for (i is 0; i < 100; i++) {
    if (i == 50) {
        break;      // exits the loop
    }
    Print(i);
}
// prints 0 through 49
```

---

## `continue`

Skip to the next iteration:

```npp
for (i is 0; i < 10; i++) {
    if (i == 5) {
        continue;   // skips printing 5
    }
    Print(i);
}
```

---

## `return`

Exit a method and optionally return a value:

```npp
FindFirst method(target as int) as int {
    for (i is 0; i < 100; i++) {
        if (data[i] == target) {
            return i;    // exits method, returns index
        }
    }
    return -1;           // not found
}
```

---

## In Nested Loops

`break` and `continue` only affect the **innermost** loop.

---

## Next Steps

- [Memory](../10_memory/alias_semantics.md) — Alias, copy, and move
