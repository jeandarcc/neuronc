# Arithmetic Operators

Neuron supports standard arithmetic operations plus a special matrix multiplication operator.

---

## Operators

| Operator | Description | Example |
|----------|-------------|---------|
| `+` | Addition | `a + b` |
| `-` | Subtraction | `a - b` |
| `*` | Multiplication | `a * b` |
| `/` | Division | `a / b` |
| `@` | Matrix multiplication | `A @ B` |
| `++` | Increment | `i++` |
| `--` | Decrement | `i--` |

---

## Basic Arithmetic

```npp
x is 10;
y is 3;

sum is x + y;       // 13
diff is x - y;      // 7
product is x * y;   // 30
quotient is x / y;  // 3
```

---

## Increment and Decrement

```npp
counter is 0;
counter++;          // counter is now 1
counter--;          // counter is now 0
```

These are commonly used in `for` loops:

```npp
for(i is 0; i < 10; i++) {
    Print(i);
}
```

---

## Matrix Multiplication (`@`)

The `@` operator performs matrix multiplication on tensors:

```npp
A is Tensor<float>.Random(2, 2);
B is Tensor<float>.Random(2, 2);
C is A @ B;     // matrix multiplication
```

See [Tensor Operations](../14_tensors/operations.md) for details.

---

## Operator Precedence

From highest to lowest:

1. `!` (not), unary `-`
2. `*`, `/`, `@`
3. `+`, `-`
4. `==`, `!=`, `>`, `<`, `>=`, `<=`
5. `&&`
6. `||`

Use parentheses to override precedence:

```npp
result is (a + b) * c;
```

---

## Next Steps

- [Comparison Operators](comparison.md)
- [Logical Operators](logical.md)
