# Type Annotation with `as`

The `as` keyword explicitly specifies the type of a variable, parameter, or return value.

---

## Variable Type Annotation

```npp
counter is 0 as int;
ratio is 0.5 as float;
name is "Alice" as string;
flag is true as bool;
```

---

## Without `as` (Type Inference)

When `as` is omitted, the compiler infers the type automatically:

```npp
counter is 0;        // inferred as int
ratio is 0.5;        // inferred as float
name is "Alice";     // inferred as string
```

See [Type Inference](type_inference.md) for details.

---

## Method Parameter Types

Parameters use `name as type` syntax:

```npp
Add method(a as int, b as int) as int {
    return a + b;
}
```

---

## Method Return Type

Use `as` after the parameter list to specify the return type:

```npp
GetName method() as string {
    return "Neuron";
}

// No return type (void)
PrintHello method() {
    Print("Hello!");
}
```

---

## Class Field Types

```npp
Player class {
    name is "" as string;
    health is 100 as int;
    speed is 1.0 as float;
}
```

---

## When to Use Explicit Types

| Situation | Recommendation |
|-----------|---------------|
| Type is obvious from value | Omit `as` — `x is 10;` |
| Type needs clarification | Use `as` — `x is 0 as float;` |
| Method parameters | Always specify types |
| Method return values | Always specify if non-void |

---

## Next Steps

- [Type Inference](type_inference.md) — How the compiler deduces types
- [Type Casting](type_casting.md) — Converting between types with `as`, `maybe`, and `then`
