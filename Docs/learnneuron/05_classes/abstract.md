# Abstract Classes

An `abstract` class cannot be instantiated directly — it serves as a base for other classes.

---

## Syntax

```npp
Shape abstract class {
    abstract Area method() as float;
}
```

---

## Abstract Methods

Methods marked `abstract` have no body — subclasses must implement them:

```npp
AbstractFeature abstract class {
    abstract Run method() as int;
}
```

---

## Implementing Abstract Methods

```npp
ConcreteFeature class inherits AbstractFeature {
    override Run method() as int {
        return 42;
    }
}
```

---

## Cannot Instantiate

```npp
s is Shape();    // ❌ compile error — abstract
c is ConcreteFeature();  // ✅ OK
```

---

## Next Steps

- [Virtual / Override / Overload](virtual_override.md)
