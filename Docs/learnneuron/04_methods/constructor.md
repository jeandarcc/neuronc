# Constructor

The `constructor` keyword declares a special method that initializes a new class instance.

---

## Syntax

```npp
Vector2 class {
    x is 0.0 as float;
    y is 0.0 as float;

    constructor public method(x as float, y as float) {
        this.x is x;
        this.y is y;
    }
}
```

---

## Using a Constructor

Create an instance by calling the class name as a function:

```npp
v is Vector2(3.0, 4.0);
Print(v.x);    // 3.0
Print(v.y);    // 4.0
```

---

## Default Values

If no constructor is defined, fields use their declared defaults:

```npp
Player class {
    name is "Unknown" as string;
    health is 100 as int;
}

p is Player();
Print(p.name);     // "Unknown"
Print(p.health);   // 100
```

---

## Constructor with Access Modifier

Constructors typically use `public` to allow external instantiation:

```npp
constructor public method(name as string) {
    this.name is name;
}
```

A `private` constructor prevents external Instantiation.

---

## Next Steps

- [Access Modifiers](access_modifiers.md) — public / private
