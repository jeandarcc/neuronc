# Hello World

Create and run your first Neuron++ project in under a minute.

---

## Step 1: Create a New Project

```bash
neuron new myproject
```

This generates the following structure:

```
myproject/
├── neuron.toml
├── .neuronsettings
├── .gitignore
├── src/
│   └── Main.npp
├── docs/scripts/
│   └── Main.md
├── tests/
│   ├── auto/
│   └── unit/
├── agents/
│   ├── language/
│   └── project/
├── modules/
└── build/
```

---

## Step 2: Explore Main.npp

Open `src/Main.npp`. You will see:

```npp
// myproject - Neuron++ Project

Init method()
{
    Print("Hello Neuron!");
};
```

### What's Happening?

- **`Init`** is the program's entry point — like `main()` in C/C++
- **`method()`** declares it as a function with no parameters and no return type
- **`Print(...)`** is a built-in function that outputs to the console
- The `is` keyword is omitted here for the method declaration (it's optional)

---

## Step 3: Run It

```bash
cd myproject
neuron run
```

Output:

```
Hello Neuron!
```

---

## Step 4: Modify the Program

Edit `src/Main.npp`:

```npp
Init method() {
    name is "World" as string;
    count is 3 as int;

    for(i is 0; i < count; i++) {
        Print("Hello, " + name + "!");
    }
}
```

Run again with `neuron run`. Output:

```
Hello, World!
Hello, World!
Hello, World!
```

---

## With `is` vs Without `is`

Both of these are valid and equivalent:

```npp
// With 'is' — more explicit
Init is method() {
    Print("Hello!");
};

// Without 'is' — recommended for methods
Init method() {
    Print("Hello!");
}
```

**Convention:** Use `is` for variable assignments. Omit `is` for method and class declarations.

---

## Next Steps

- [Project Structure](project_structure.md) — Understand every file and directory
- [Variables](../01_variables/declaration.md) — Learn how variables work
