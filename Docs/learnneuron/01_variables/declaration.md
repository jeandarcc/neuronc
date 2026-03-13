# Variable Declaration

Variables in Neuron are declared using the `is` keyword, or optionally without it using bare declaration syntax.

---

## Basic Declaration with `is`

The `is` keyword binds a name to a value:

```npp
x is 10;
name is "Alice";
pi is 3.14159;
active is true;
```

---

## The `is` Keyword Is Optional

In Neuron, `is` is **optional**. You can declare variables without it:

```npp
// With 'is' — recommended for variables
x is 10;
name is "hello";

// Without 'is' — shorthand form
y 20;
count 0;
```

### Convention

| Context | Recommendation |
|---------|---------------|
| Variable declaration | **Use `is`** — e.g., `x is 10;` |
| Variable reassignment | **Use `is`** — e.g., `x is 20;` |
| Method declaration | **Omit `is`** — e.g., `Run method() { }` |
| Class declaration | **Omit `is`** — e.g., `Player class { }` |

Both forms compile identically. The convention improves readability.

---

## Bare Declaration

Declaring a variable name alone (without a value) creates an uninitialized `dynamic` variable:

```npp
a;          // dynamic, uninitialized
b 10;       // int, value is 10
c as int;   // int, uninitialized (typed but no value)
d is 0 as dynamic;  // explicit dynamic with initial value
```

---

## Type Annotation with `as`

Use `as` to explicitly specify the type:

```npp
x is 10 as int;
name is "hello" as string;
ratio is 0.5 as float;
flag is true as bool;
```

When `as` is omitted, the compiler infers the type from the assigned value.

---

## Examples from Real Code

```npp
// From Hello.nr
x is 10;
Init is method() {
    Print(x);
};

// From IsOptionalDynamic.nr — all valid declarations
a;              // bare declaration (dynamic)
b 10;           // shorthand (int, value 10)
c as dynamic;   // typed declaration (dynamic, no value)
d is 0 as dynamic;  // full form
```

---

## Next Steps

- [Assignment](assignment.md) — Reassigning variables and alias semantics
- [Copying](copying.md) — Creating independent copies with `another`
- [Move](move.md) — Transferring ownership with `move`
