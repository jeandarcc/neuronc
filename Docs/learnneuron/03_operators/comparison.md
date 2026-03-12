# Comparison Operators

Compare two values and produce a `bool` result (`true` or `false`).

---

## Operators

| Operator | Description | Example |
|----------|-------------|---------|
| `==` | Equal to | `a == b` |
| `!=` | Not equal to | `a != b` |
| `>` | Greater than | `a > b` |
| `<` | Less than | `a < b` |
| `>=` | Greater than or equal | `a >= b` |
| `<=` | Less than or equal | `a <= b` |

---

## Examples

```npp
x is 10;
y is 20;

Print(x == y);    // false
Print(x != y);    // true
Print(x < y);     // true
Print(x >= 10);   // true
```

---

## Usage in Conditions

```npp
if (score >= 90) {
    Print("Grade: A");
} else if (score >= 80) {
    Print("Grade: B");
} else {
    Print("Grade: C");
}
```

---

## Next Steps

- [Logical Operators](logical.md)
