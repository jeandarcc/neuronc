# While Loop

Repeat a block while a condition is true.

---

## Syntax

```npp
while (condition) {
    // body
}
```

---

## Example

```npp
count is 0;
while (count < 5) {
    Print(count);
    count++;
}
// prints: 0 1 2 3 4
```

---

## Infinite Loop

```npp
while (true) {
    // runs forever unless break is used
    if (shouldStop) {
        break;
    }
}
```

---

## Next Steps

- [For Loop](for_loop.md)
