# Primitive Types

Neuron++ provides four fundamental primitive types.

---

## Type Overview

| Type | Description | Example |
|------|-------------|---------|
| `int` | Integer numbers | `42`, `-7`, `0` |
| `float` | Floating-point numbers | `3.14`, `-0.5`, `1.0` |
| `string` | Text strings | `"hello"`, `"Neuron++"` |
| `bool` | Boolean values | `true`, `false` |

---

## Integer (`int`)

Whole numbers without a decimal point:

```npp
age is 25 as int;
count is 0;           // inferred as int
negative is -100;
```

---

## Float (`float`)

Numbers with a decimal point:

```npp
pi is 3.14159 as float;
ratio is 0.5;          // inferred as float
temperature is -40.0;
```

---

## String (`string`)

Text enclosed in double quotes:

```npp
name is "Alice" as string;
greeting is "Hello, Neuron++!";
empty is "";
```

String concatenation uses `+`:

```npp
full is "Hello, " + name + "!";
Print(full);    // Hello, Alice!
```

---

## Boolean (`bool`)

The values `true` and `false`:

```npp
active is true as bool;
visible is false;

if (active) {
    Print("Active!");
}
```

---

## Default Type Inference

When no `as` annotation is provided, the compiler infers the type:

| Value | Inferred Type |
|-------|---------------|
| `42` | `int` |
| `3.14` | `float` |
| `"text"` | `string` |
| `true` / `false` | `bool` |

---

## Next Steps

- [Type Annotation](type_annotation.md) — Explicitly specifying types with `as`
- [Dynamic](dynamic.md) — Untyped / late-bound variables
