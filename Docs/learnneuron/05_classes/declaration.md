# Class Declaration

Classes in Neuron define objects with fields and methods.

---

## Basic Syntax

```npp
Player class {
    name is "Unknown" as string;
    health is 100 as int;
}
```

**Convention:** Omit `is` for class declarations (it's optional).

---

## With `is` (Also Valid)

```npp
Player is class {
    name is "Unknown" as string;
}
```

Both forms compile identically. Omitting `is` is the recommended style.

---

## One Class Per File

Neuron enforces a strict **one-class-per-file** rule. The class name must match the filename:

| File | Class Name |
|------|-----------|
| `Player.nr` | `Player class { }` |
| `Vector2.nr` | `Vector2 class { }` |
| `Main.nr` | Contains `Init` method (no class required) |

This is enforced by the compiler when `max_classes_per_file = 1` in `.neuronsettings`.

---

## Creating Instances

Call the class name as a function:

```npp
p is Player();
Print(p.name);     // "Unknown"
Print(p.health);   // 100
```

---

## Access Modifier on Class

```npp
Player public class {
    // visible outside this module
}
```

---

## Generic Classes

Classes can accept type parameters:

```npp
Box class<T> {
    value is T;
}

b is Box<int>();
b.value is 42;
```

See [Generic Classes](../13_generics/generic_classes.md).

---

## Next Steps

- [Fields](fields.md) â€” Class fields and `this`
- [Constructors](constructors.md) â€” Initialization methods
- [Inheritance](inheritance.md) â€” Extending classes
