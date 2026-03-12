# Access Modifiers

Neuron++ uses `public` and `private` to control visibility of methods and fields.

---

## `public`

Members marked `public` are accessible from outside the class:

```npp
Player class {
    GetName public method() as string {
        return "Player One";
    }
}

// External code
p is Player();
Print(p.GetName());   // ✅ accessible
```

---

## `private`

Members marked `private` are only accessible within the class:

```npp
Counter class {
    count is 0 as int;

    Increment private method() {
        count is count + 1;
    }

    GetCount public method() as int {
        Increment();      // ✅ internal access OK
        return count;
    }
}

c is Counter();
c.GetCount();         // ✅ public
c.Increment();        // ❌ compile error — private
```

---

## On Fields

Fields can also have visibility modifiers:

```npp
Player class {
    name is "" as string;          // default visibility
    health public is 100 as int;   // explicitly public
    secret private is "" as string; // explicitly private
}
```

When `require_class_explicit_visibility = true` in `.neuronsettings`, all members must have an explicit modifier.

---

## Placement

The modifier goes **before** the `method` keyword or the field name:

```npp
// Method
Run public method() { }

// Class
Player public class { }
```

---

## Next Steps

- [Lambdas](lambdas.md) — Anonymous methods
- [Callbacks](callbacks.md) — Passing methods as arguments
