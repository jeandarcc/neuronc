# Complexity Rules

Rules that limit code complexity to keep methods readable and maintainable.

---

## Rules

| Rule | Default | Effect |
|------|---------|--------|
| `max_lines_per_method` | `50` | Maximum lines in a method body |
| `max_lines_per_block_statement` | `20` | Maximum lines in a single if/for/while block |
| `max_nesting_depth` | `3` | Maximum if/for/while nesting depth |

---

## `max_lines_per_method = 50`

No method body can exceed 50 lines.

```
neuronc: error: Method 'ProcessInput' exceeds maximum allowed length
(72 lines, limit 50).
For agents: Split large methods into smaller helpers.
```

**Why:** A 50-line limit forces decomposition. Instead of a 200-line `Init()` method, you get well-named helpers:

```npp
Init method() {
    LoadConfig();
    InitializeNetwork();
    StartTraining();
}
```

This is especially critical for AI agents, which tend to generate monolithic methods.

---

## `max_lines_per_block_statement = 20`

Individual blocks (if, for, while) inside a method can't exceed 20 lines:

```npp
// ❌ Rejected — 25-line if block
if (condition) {
    // ... 25 lines of code ...
}

// ✅ Extract to a helper
if (condition) {
    HandleCondition();    // clean and readable
}
```

**Why:** Even if a method is under 50 lines total, one massive block doing everything is still unreadable. This rule forces granularity within methods.

---

## `max_nesting_depth = 3`

Maximum nesting of control flow structures:

```npp
// ❌ Rejected — nesting depth 4
if (a) {                        // depth 1
    if (b) {                    // depth 2
        for (i in data) {       // depth 3
            if (c) {            // depth 4 — too deep!

            }
        }
    }
}

// ✅ Use early returns and helpers
if (!a) { return; }
if (!b) { return; }
ProcessData(data);
```

**Why:** Deeply nested code is extremely hard to reason about. AI agents are particularly prone to generating "arrow code" — these limits force them to use better patterns like early returns and extracted helpers.

---

## Next Steps

- [Documentation Rules](documentation_rules.md)
