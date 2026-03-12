# Type Inference

Neuron++ can automatically deduce the type of a variable from its assigned value, so you don't always need to write `as <type>`.

---

## How It Works

The compiler examines the right-hand side of the assignment and infers the type:

```npp
x is 42;           // int
y is 3.14;         // float
name is "Alice";   // string
flag is true;      // bool
```

No `as` annotation is required when the type is unambiguous.

---

## Inference Rules

| Assigned Value | Inferred Type |
|----------------|---------------|
| Integer literal (`42`) | `int` |
| Float literal (`3.14`) | `float` |
| String literal (`"hi"`) | `string` |
| `true` / `false` | `bool` |
| Function call | Return type of the function |
| Another variable | Type of that variable |
| `another x` | Type of `x` |
| `move x` | Type of `x` |

---

## When Inference Needs Help

Sometimes the compiler cannot determine the type. Use `as` in these cases:

```npp
// Ambiguous: is this int or float?
value is 0 as float;    // explicitly float

// Uninitialized declaration
count as int;            // type required, no value
```

---

## Inference with Expressions

The compiler infers from expression results:

```npp
a is 5 + 10;     // int (int + int = int)
b is a * 2;      // int
c is a * 2.0;    // float (int * float = float)
```

---

## Next Steps

- [Dynamic](dynamic.md) — Runtime-typed variables
- [Type Casting](type_casting.md) — Converting between types
