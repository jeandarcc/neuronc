# How to Add a Diagnostic Error Code

Neuron uses a multi-locale diagnostic system. Error codes are defined in TOML
files under `config/diagnostics/<locale>/` and referenced by numeric ID from C++.

---

## Step 1 â€” Claim a Code Number

Open `config/diagnostics/catalog.toml`. Find the appropriate range for your error:

| Range | Category |
|-------|----------|
| N1xxx | Parser errors |
| N2xxx | Semantic errors |
| N3xxx | Ownership / lifetime errors |
| N4xxx | Type system errors |
| N5xxx | Code generation errors |
| N6xxx | Runtime errors |
| N7xxx | ncon / package manager errors |
| N9xxx | Internal compiler errors |

Pick the next available code in the range and add an entry to `catalog.toml`:

```toml
[N2042]
summary = "Type mismatch in assignment"
category = "semantic"
severity = "error"
```

---

## Step 2 â€” Add the Diagnostic to Each Locale

For every locale directory in `config/diagnostics/` (currently: `de/`, `en/`, `it/`, `ko/`, `la/`, `zh/`),
find the file matching your code range (e.g. `N2xxx_semantic.toml`) and add:

```toml
[N2042]
message = "cannot assign value of type '{got}' to variable of type '{expected}'"
note    = "consider an explicit cast or check the declared type"
```

Use the **plugger tool** to automate this across all locales:

```powershell
# Add a new code interactively (opens editor, then applies to all locales)
python config/diagnostics/plugger/plugger.py add N2042

# Update an existing code's translations
python config/diagnostics/plugger/plugger.py update N2042
```

See `config/diagnostics/plugger/README.md` for full plugger usage.

---

## Step 3 â€” Reference the Code from C++

In the appropriate semantic/parser source file, emit the diagnostic:

```cpp
// In src/sema/ExpressionAnalyzer.cpp (for example)
ctx.emitter.emit(N2042, expr.location(), {
    {"got",      got_type.name()},
    {"expected", expected_type.name()}
});
```

The `ctx.emitter` is a `DiagnosticEmitter` (`src/sema/DiagnosticEmitter.h`).
The `DiagnosticLocalizer` (`src/diagnostics/DiagnosticLocalizer.cpp`) resolves
the locale at runtime using `config/diagnostics/<locale>/` TOML files.

---

## Step 4 â€” Write a Test

Add a test that compiles a `.nr` snippet and asserts the diagnostic fires:

```cpp
// In tests/sema/<something>.cpp
TEST("N2042 fires on type mismatch") {
    auto result = compile("let x: i32 = \"hello\"");
    EXPECT_DIAGNOSTIC(result, N2042);
}
```

---

## Step 5 â€” Verify

```powershell
scripts\build.bat
powershell -File scripts/build_tests.ps1 -Filter "sema*" ...
```
