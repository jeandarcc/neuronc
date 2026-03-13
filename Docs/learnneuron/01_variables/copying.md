# Copying with `another`

The `another` keyword creates an **independent deep copy** of a value. The copy has its own memory â€” changes to one do not affect the other.

---

## Syntax

```npp
original is 42;
copied is another original;    // separate memory, same value

copied is 100;
Print(original);  // still 42
Print(copied);    // 100
```

---

## Why `another`?

In Neuron, `is` creates an alias by default. To get a true copy, you must request one explicitly:

| Statement | Behavior |
|-----------|----------|
| `y is x;` | Alias â€” `y` and `x` share memory |
| `y is another x;` | Copy â€” `y` gets its own memory |

---

## Copying Objects

`another` works with class instances too:

```npp
v1 is Vector2(3.0, 4.0);
v2 is another v1;         // deep copy of the object

v2.x is 10.0;
Print(v1.x);  // still 3.0
Print(v2.x);  // 10.0
```

---

## Shorthand (Without `is`)

The shorthand declaration form also supports `another`:

```npp
a 10;
b another a;    // copy without 'is'
```

---

## When to Use `another`

- When you need to modify a value without affecting the original
- When passing data to a function that may mutate it
- When storing a snapshot of a value at a point in time

---

## C++ Equivalent

| Neuron | C++ |
|----------|-----|
| `y is x;` | `T& y = x;` (reference) |
| `y is another x;` | `T y = x;` (value copy) |

---

## Next Steps

- [Move](move.md) â€” Transfer ownership instead of copying
