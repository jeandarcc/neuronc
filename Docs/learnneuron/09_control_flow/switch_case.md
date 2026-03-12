# Switch / Case

Multi-branch conditional using `switch`, `case`, `default`, and `break`.

---

## Syntax

```npp
switch (expression) {
    case value1:
        // ...
        break;
    case value2:
        // ...
        break;
    default:
        // ...
        break;
}
```

---

## Example with Enum

```npp
Color enum { Red, Yellow, Green }

color is Color.Red as Color;

switch (color) {
    case Color.Red:
        Print("Stop");
        break;
    case Color.Yellow:
        Print("Caution");
        break;
    case Color.Green:
        Print("Go");
        break;
    default:
        Print("Unknown");
        break;
}
```

---

## Fall-Through

Without `break`, execution falls through to the next case. Always use `break` unless fall-through is intended.

---

## Next Steps

- [While Loop](while_loop.md)
