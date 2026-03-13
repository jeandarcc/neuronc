# Assignment & Alias Semantics

In Neuron, the default assignment behavior creates an **alias** — both names share the same memory.

---

## Alias Assignment (Default)

When you assign one variable to another using `is`, they become aliases:

```npp
x is 10;
y is x;       // y is an ALIAS of x — shared memory

y is 20;      // x is now ALSO 20
Print(x);     // prints 20
```

This is fundamentally different from most languages. In Neuron:
- `y is x` does **not** copy the value
- Both `x` and `y` point to the **same** memory

### Why Aliases?

Alias-by-default avoids unnecessary copies, making the language fast by default. When you need a separate copy, you use `another` explicitly.

---

## Reassignment

Reassigning with `is` updates the binding:

```npp
x is 10;
x is 20;      // x is now 20
x is "hello"; // ERROR: type mismatch (x is int)
```

---

## Member Assignment

Class field assignment uses `is` with dot notation:

```npp
node is Node();
node.value is 100;    // assign to field
Print(node.value);    // prints 100
```

---

## Indexed Assignment

Array/collection element assignment:

```npp
data[i] is data[i] * 2;
```

---

## `this` Keyword in Classes

Inside class methods, use `this` to assign to own fields:

```npp
constructor public method(x as float, y as float) {
    this.x is x;
    this.y is y;
}
```

---

## Comparison with Other Languages

| Language | `y = x` means... |
|----------|-------------------|
| **C++** | Copy the value |
| **Python** | Share the reference (for objects) |
| **Neuron** | **Alias** — shared memory for all types |

To get C++-style copy behavior in Neuron, use [`another`](copying.md).

---

## Next Steps

- [Copying](copying.md) — Create independent copies with `another`
- [Move](move.md) — Transfer ownership with `move`
