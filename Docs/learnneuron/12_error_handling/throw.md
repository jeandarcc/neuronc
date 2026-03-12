# Throw

The `throw` keyword raises an error that can be caught by `try/catch`.

---

## Syntax

```npp
throw "Something went wrong";
throw errorValue;
```

---

## Throwing a String Error

```npp
Validate method(age as int) {
    if (age < 0) {
        throw "Age cannot be negative";
    }
}
```

---

## Catching a Thrown Error

```npp
try {
    Validate(-1);
} catch (error) {
    Print(error);    // "Age cannot be negative"
}
```

---

## Custom Error Types

Define error classes and throw instances:

```npp
ValidationError class {
    message is "" as string;

    constructor public method(msg as string) {
        this.message is msg;
    }
}

// later...
throw ValidationError("Invalid input");
```

---

## Next Steps

- [Generics](../13_generics/generic_classes.md)
