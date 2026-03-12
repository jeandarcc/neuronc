# Generic Methods

Methods can accept type parameters for type-safe operations on any type.

---

## Syntax

```npp
Swap method<T>(a as T, b as T) {
    temp is another a;
    a is b;
    b is temp;
}
```

---

## Calling

The compiler infers the type parameter:

```npp
x is 10;
y is 20;
Swap(x, y);
Print(x);    // 20
Print(y);    // 10
```

---

## With Return Type

```npp
Identity method<T>(value as T) as T {
    return value;
}

result is Identity(42);    // int
name is Identity("hello"); // string
```

---

## Next Steps

- [Constraints](constraints.md) — Limiting type parameters
