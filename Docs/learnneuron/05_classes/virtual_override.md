# Virtual, Override, and Overload

These three modifiers enable polymorphism in Neuron.

---

## `virtual`

A `virtual` method can be overridden by subclasses:

```npp
BaseClass class {
    virtual Negate method(a as int) as int {
        return 0 - a;
    }
}
```

---

## `override`

Marks a method as replacing a `virtual` or `abstract` method from a parent class:

```npp
DerivedClass class inherits BaseClass {
    override Negate method(a as int) as int {
        return -1 * a;
    }
}
```

---

## `overload`

Creates multiple methods with the same name but different parameter signatures:

```npp
Calculator class {
    overload Add method(a as int, b as int) as int {
        return a + b;
    }

    overload Add method(a as float, b as float) as float {
        return a + b;
    }
}
```

---

## Real Example

From `VirtualOverrideOverload.nr`:

```npp
VirtualOverrideOverload class {
    overload Add method(a as int, b as int) as int {
        return a + b;
    }

    virtual Negate method(a as int) as int {
        return 0 - a;
    }

    override Normalize method(a as int) as int {
        return a;
    }
}
```

---

## Summary

| Modifier | Purpose |
|----------|---------|
| `virtual` | Method can be overridden by subclasses |
| `override` | Replaces a virtual/abstract method |
| `overload` | Same name, different parameter types |

---

## Next Steps

- [Structs](../06_structs/structs.md) â€” Value-type classes
- [Interfaces](../07_interfaces/interfaces.md) â€” Contracts
