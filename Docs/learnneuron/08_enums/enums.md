# Enums

An `enum` defines a set of named constant values.

---

## Syntax

```npp
Color enum {
    Red,
    Yellow,
    Green
}
```

---

## Using Enums

```npp
color is Color.Red as Color;

switch (color) {
    case Color.Red:
        Print("Red");
        break;
    case Color.Yellow:
        Print("Yellow");
        break;
    case Color.Green:
        Print("Green");
        break;
    default:
        Print("Unknown");
        break;
}
```

---

## Enums with Names

Define enums before or after the `Init` method:

```npp
Status enum { Pending, Running, Done };

Init method() as int {
    state is Status.Pending;
    return 0;
}
```

---

## Real Example

From `CSharpFeatures.nr`:

```npp
Color enum {
    Red,
    Yellow,
    Green
}

Status enum { Pending, Running, Done };
```

---

## Next Steps

- [Control Flow](../09_control_flow/if_else.md) — Branching and looping
