# Try / Catch / Finally

Neuron provides structured error handling with `try`, `catch`, and `finally`.

---

## Basic Syntax

```npp
try {
    // code that may fail
} catch (errorValue) {
    // handle the error
} finally {
    // always runs (cleanup)
}
```

---

## Example

From `TryCatch.nr`:

```npp
module System;

Init method() {
    try {
        throw "try-catch smoke";
    } catch (errorValue) {
        Print(errorValue);
    } finally {
        Print("finally block");
    }
}
```

Output:

```
try-catch smoke
finally block
```

---

## Catching Specific Error Types

```npp
try {
    result is Divide(10, 0);
} catch (DivideByZeroError err) {
    Print("Cannot divide by zero");
} catch (error) {
    Print("Unknown error: " + error);
}
```

The more specific catch comes first; the general catch acts as a fallback.

---

## Multiple Catch Blocks

```npp
try {
    Process();
} catch (FileNotFoundError err) {
    Print("File not found");
} catch (PermissionError err) {
    Print("Permission denied");
} catch (error) {
    Print("Unexpected error");
} finally {
    Cleanup();
}
```

---

## `finally` Is Optional

```npp
try {
    DoWork();
} catch (error) {
    Print(error);
}
// no finally block needed
```

---

## Next Steps

- [Throw](throw.md) â€” Throwing errors
