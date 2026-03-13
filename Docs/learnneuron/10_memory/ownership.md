# Ownership

Neuron provides a clear ownership model through its memory primitives.

---

## Ownership Rules

1. Every value has exactly one **owner** at a time
2. `is` creates an **alias** (shared ownership)
3. `another` creates a **copy** (new owner)
4. `move` **transfers** ownership (old owner invalidated)

---

## Ownership Transfer with `move`

```npp
buffer is CreateLargeBuffer();
newOwner is move buffer;    // buffer is invalidated
// buffer should not be used after this point
```

---

## Shared Access with Alias

```npp
data is LoadData();
view is data;     // view and data share the same memory
// Both are valid, both see the same data
```

---

## Independent Copy

```npp
original is LoadData();
backup is another original;   // completely independent
// Modifying backup does not affect original
```

---

## Next Steps

- [Modules](../11_modules/importing.md) â€” Importing code
