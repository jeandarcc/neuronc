# Class Access Modifiers

Control visibility of class members with `public` and `private`.

---

## On the Class Itself

```npp
Player public class {
    // accessible from outside this module
}
```

---

## On Fields and Methods

```npp
BankAccount class {
    owner public is "" as string;
    balance private is 0.0 as float;

    Deposit public method(amount as float) {
        balance is balance + amount;
    }

    GetBalance public method() as float {
        return balance;
    }
}
```

---

## Enforcement

When `require_class_explicit_visibility = true` in `.neuronsettings`, every member must have an explicit `public` or `private` modifier.

---

## Next Steps

- [Inheritance](inheritance.md)
