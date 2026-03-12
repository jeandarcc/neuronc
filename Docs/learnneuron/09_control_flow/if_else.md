# If / Else

Conditional branching with `if` and `else`.

---

## Syntax

```npp
if (condition) {
    // runs when condition is true
}
```

---

## If / Else

```npp
if (score >= 90) {
    Print("Grade: A");
} else {
    Print("Grade: B or lower");
}
```

---

## Else If Chain

```npp
if (score >= 90) {
    Print("A");
} else if (score >= 80) {
    Print("B");
} else if (score >= 70) {
    Print("C");
} else {
    Print("F");
}
```

---

## With Boolean Variables

```npp
active is true;

if (active) {
    Print("System active");
}
```

---

## Comparison Operators

```npp
if (x == 10) { }
if (x != 0) { }
if (x > y && x < z) { }
if (a || b) { }
```

---

## Next Steps

- [Switch / Case](switch_case.md)
