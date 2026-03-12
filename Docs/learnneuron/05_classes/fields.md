# Class Fields

Fields are variables declared inside a class body.

---

## Declaring Fields

```npp
Node class {
    value is 0 as int;
    label is "" as string;
}
```

Fields use the same `is` syntax as regular variables.

---

## Accessing Fields

Use dot notation:

```npp
n is Node();
n.value is 100;
n.label is "root";
Print(n.value);    // 100
```

---

## The `this` Keyword

Inside a method, `this` refers to the current instance:

```npp
Player class {
    name is "" as string;

    constructor public method(name as string) {
        this.name is name;
    }

    GetName public method() as string {
        return this.name;
    }
}
```

Use `this` when a parameter name shadows a field name.

---

## Field Defaults

Fields can have default values:

```npp
Settings class {
    volume is 80 as int;
    brightness is 100 as int;
    fullscreen is false as bool;
}

s is Settings();
Print(s.volume);      // 80
```

---

## Field Visibility

Fields can be `public` or `private`:

```npp
Account class {
    name public is "" as string;
    balance private is 0.0 as float;
}
```

---

## Next Steps

- [Constructors](constructors.md)
- [Access Modifiers](access_modifiers.md)
