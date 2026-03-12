# Parameters & Return Types

Methods accept parameters using `name as type` syntax and return values using `as type`.

---

## Parameter Syntax

Every parameter has the form `name as type`:

```npp
Greet method(name as string) {
    Print("Hello, " + name + "!");
}

Add method(a as int, b as int) as int {
    return a + b;
}
```

---

## Multiple Parameters

Separate parameters with commas:

```npp
CreateVector method(x as float, y as float, z as float) as Vector3 {
    // ...
}
```

---

## Method Type Parameters

You can pass a `method` as a parameter type:

```npp
Execute method(action as method) {
    action();
}
```

See [Callbacks](callbacks.md) for more details.

---

## Return Type

Use `as` after the parameter list:

```npp
GetName method() as string {
    return "Neuron++";
}

Square method(x as int) as int {
    return x * x;
}
```

---

## No Return Type (Void)

Omit `as` if the method returns nothing:

```npp
PrintHello method() {
    Print("Hello!");
}
```

---

## Returning Values

Use `return` to send a value back to the caller:

```npp
Max method(a as int, b as int) as int {
    if (a > b) {
        return a;
    }
    return b;
}
```

---

## Next Steps

- [Constructor](constructor.md) — Initialization method
- [Lambdas](lambdas.md) — Anonymous methods
