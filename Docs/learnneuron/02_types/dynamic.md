# Dynamic Type

The `dynamic` keyword declares a variable with a runtime-determined type. Dynamic variables can hold any value and their type can change during execution.

---

## Declaring Dynamic Variables

```npp
// Explicit dynamic
x is 0 as dynamic;
y as dynamic;

// Bare declaration â€” implicitly dynamic
a;                  // dynamic, uninitialized
```

A bare declaration (just an identifier followed by a semicolon) automatically becomes a `dynamic` variable.

---

## Dynamic Behavior

Dynamic variables accept any value type:

```npp
value as dynamic;
value is 42;         // holds an int
value is "hello";    // now holds a string
value is true;       // now holds a bool
```

---

## Real Example

From `IsOptionalDynamic.nr`:

```npp
RunFmaBench method() as int {
    a;                  // dynamic, bare declaration
    b 10;               // int shorthand
    c as dynamic;       // explicit dynamic
    d is 0 as dynamic;  // dynamic with initial value

    Print("is-optional + dynamic demo");
    return 0;
}
```

---

## When to Use `dynamic`

| Use Case | Recommendation |
|----------|---------------|
| Type is known at compile time | Use a concrete type (`int`, `string`) |
| Type varies at runtime | Use `dynamic` |
| Interfacing with untyped data | Use `dynamic` |
| Prototyping | Use `dynamic`, refine later |

---

## Next Steps

- [Type Casting](type_casting.md) â€” Converting between types with `as`, `maybe`, and `then`
