# Class Constructors

Constructors are special methods that initialize class instances.

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

## Usage

```npp
v is Vector2(3.0, 4.0);
Print(v.x);    // 3.0
```

---

## Without a Constructor

If no constructor is defined, instances use field defaults:

```npp
Config class {
    debug is false;
    level is 1;
}

c is Config();   // debug = false, level = 1
```

---

## Next Steps

- [Access Modifiers](access_modifiers.md)
- [Inheritance](inheritance.md)
