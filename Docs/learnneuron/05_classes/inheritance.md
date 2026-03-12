# Inheritance

Classes can extend other classes using the `inherits` keyword.

---

## Syntax

```npp
Animal class {
    name is "" as string;

    Speak public method() {
        Print("...");
    }
}

Dog public class inherits Animal {
    Speak public method() {
        Print("Woof!");
    }
}
```

---

## Multiple Inheritance

A class can inherit from multiple base classes:

```npp
Dog public class inherits Animal, ISerializable {
    Speak public method() {
        Print("Woof");
    }
}
```

Separate base classes with commas.

---

## Calling Base Constructors

```npp
Vehicle class {
    speed is 0 as int;

    constructor public method(speed as int) {
        this.speed is speed;
    }
}

Car class inherits Vehicle {
    doors is 4 as int;
}
```

---

## Next Steps

- [Abstract Classes](abstract.md) — Classes that can't be instantiated
- [Virtual / Override / Overload](virtual_override.md) — Polymorphism
