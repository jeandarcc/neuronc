# Interfaces

An `interface` defines a contract — a set of methods that implementing classes must provide.

---

## Syntax

```npp
Drawable interface {
    Draw method() as void;
}
```

---

## Implementing an Interface

Use `inherits` to implement an interface:

```npp
Circle class inherits Drawable {
    Draw method() as void {
        Print("Drawing circle");
    }
}
```

---

## Multiple Interfaces

```npp
Player class inherits Drawable, Serializable {
    Draw method() as void { }
    Serialize method() as string { }
}
```

---

## Real Example

From `CSharpFeatures.npp`:

```npp
CSharpFeatures interface {
    Draw method() as void;
}
```

---

## Interfaces vs Abstract Classes

| Feature | Interface | Abstract Class |
|---------|-----------|---------------|
| Fields | ❌ No | ✅ Yes |
| Method bodies | ❌ No (declaration only) | ✅ Yes (mixed) |
| Multiple inheritance | ✅ Yes | ✅ Yes |

---

## Next Steps

- [Enums](../08_enums/enums.md)
