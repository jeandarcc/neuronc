# Method Declaration

All functions in Neuron are declared with the `method` keyword.

---

## Basic Syntax

```npp
// Without 'is' — recommended for methods
MethodName method() {
    // body
}

// With 'is' — also valid
MethodName is method() {
    // body
};
```

**Convention:** Omit `is` for method declarations. Use `is` for variable assignments.

---

## With Parameters and Return Type

```npp
Add method(a as int, b as int) as int {
    return a + b;
}
```

- Parameters: `name as type`, separated by commas
- Return type: `as type` after the parameter list
- No return type means the method returns nothing (void)

---

## Calling a Method

```npp
result is Add(3, 5);
Print(result);      // 8
```

---

## Entry Point

The `Init` method in `src/Main.nr` is the program entry point:

```npp
Init method() {
    Print("Hello Neuron!");
}
```

No return type, no parameters. Automatically invoked by the runtime.

---

## Method Naming Rules

- Method names must start with an **uppercase letter** (PascalCase)
- Minimum name length: 4 characters (configurable)

```npp
CalculateSum method(a as int, b as int) as int {   // ✅ correct
    return a + b;
}

add method(a as int, b as int) as int {   // ❌ must start uppercase
    return a + b;
}
```

---

## Methods in Classes

Inside a class, methods are declared the same way:

```npp
Player class {
    GetName public method() as string {
        return "Player One";
    }
}
```

---

## Next Steps

- [Parameters & Return Types](parameters_return.md) — Detailed parameter syntax
- [Constructor](constructor.md) — Special initialization method
- [Access Modifiers](access_modifiers.md) — public / private
