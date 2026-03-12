# Logical Operators

Combine boolean expressions using logical AND, OR, and NOT.

---

## Operators

| Operator | Description | Example |
|----------|-------------|---------|
| `&&` | Logical AND | `a && b` |
| `\|\|` | Logical OR | `a \|\| b` |
| `!` | Logical NOT | `!a` |

---

## Examples

```npp
a is true;
b is false;

Print(a && b);    // false
Print(a || b);    // true
Print(!a);        // false
Print(!b);        // true
```

---

## Combining Conditions

```npp
age is 25;
hasLicense is true;

if (age >= 18 && hasLicense) {
    Print("Can drive");
}

if (age < 13 || age > 65) {
    Print("Special pricing");
}
```

---

## Short-Circuit Evaluation

Logical operators use short-circuit evaluation:
- `&&` — if left side is `false`, right side is **not evaluated**
- `||` — if left side is `true`, right side is **not evaluated**

---

## Next Steps

- [Range Operator](range.md)
