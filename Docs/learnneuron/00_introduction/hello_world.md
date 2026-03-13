# Hello World

Create and run your first Neuron project in under a minute.

---

## Step 1: Create a New Project

```bash
neuron new myproject
```

This generates the following structure:

```
myproject/
â”œâ”€â”€ neuron.toml
â”œâ”€â”€ .neuronsettings
â”œâ”€â”€ .gitignore
â”œâ”€â”€ src/
â”‚   â””â”€â”€ Main.nr
â”œâ”€â”€ docs/scripts/
â”‚   â””â”€â”€ Main.md
â”œâ”€â”€ tests/
â”‚   â”œâ”€â”€ auto/
â”‚   â””â”€â”€ unit/
â”œâ”€â”€ agents/
â”‚   â”œâ”€â”€ language/
â”‚   â””â”€â”€ project/
â”œâ”€â”€ modules/
â””â”€â”€ build/
```

---

## Step 2: Explore Main.nr

Open `src/Main.nr`. You will see:

```npp
// myproject - Neuron Project

Init method()
{
    Print("Hello Neuron!");
};
```

### What's Happening?

- **`Init`** is the program's entry point â€” like `main()` in C/C++
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

Edit `src/Main.nr`:

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
// With 'is' â€” more explicit
Init is method() {
    Print("Hello!");
};

// Without 'is' â€” recommended for methods
Init method() {
    Print("Hello!");
}
```

**Convention:** Use `is` for variable assignments. Omit `is` for method and class declarations.

---

## Next Steps

- [Project Structure](project_structure.md) â€” Understand every file and directory
- [Variables](../01_variables/declaration.md) â€” Learn how variables work
