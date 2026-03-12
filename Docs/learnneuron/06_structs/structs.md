# Structs

A `struct` is a value-type class — a lightweight data container.

---

## Syntax

```npp
Point struct {
    x is 0 as int;
    y is 0 as int;
}
```

---

## Structs vs Classes

| Feature | `class` | `struct` |
|---------|---------|----------|
| Semantics | Reference type (alias) | Value type (copy) |
| Default assignment | Alias (shared memory) | Copy (independent) |
| Inheritance | ✅ Supported | Limited |
| Methods | ✅ Full support | ✅ Supported |

---

## Usage

```npp
p is Point();
p.x is 10;
p.y is 20;
Print(p.x);    // 10
```

---

## Real Example

From `StructFeature.npp`:

```npp
StructFeature struct {
    x is 0 as int;
    y is 0 as int;
}

Init method() as int {
    return 0;
}
```

---

## Next Steps

- [Interfaces](../07_interfaces/interfaces.md)
- [Enums](../08_enums/enums.md)
