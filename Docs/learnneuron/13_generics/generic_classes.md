# Generic Classes

Generic classes accept type parameters, enabling type-safe reusable containers.

---

## Syntax

```npp
Box class<T> {
    value is T;
}
```

---

## Usage

```npp
b is Box<int>();
b.value is 42;
Print(b.value);    // 42

s is Box<string>();
s.value is "hello";
```

---

## Real Example

From `Box.nr`:

```npp
Box class<T> {
    value is T;
}

Init method() {
    boxValue is Box<int>();
    boxValue.value is 123;
    Print(boxValue.value);
}
```

---

## Multiple Type Parameters

```npp
Pair class<K, V> {
    key is K;
    value is V;
}

p is Pair<string, int>();
p.key is "age";
p.value is 25;
```

---

## Next Steps

- [Generic Methods](generic_methods.md)
- [Constraints](constraints.md)
