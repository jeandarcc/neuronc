# .neuronsettings

The `.neuronsettings` file controls **compiler-enforced** source code rules for every Neuron project. These rules are not suggestions â€” they are **hard constraints** checked at compile time.

---

## Why These Rules Exist

> **`.neuronsettings` is the project's immune system.**

These rules serve two critical purposes:

### 1. Developer Discipline

Without enforced rules, projects naturally degrade over time:
- Files grow to thousands of lines
- Methods become impossible to understand
- Naming conventions drift into chaos
- Documentation falls behind

`.neuronsettings` prevents this by making violations **compile errors**, not warnings. Bad code simply **cannot** enter the project.

### 2. AI Agent Protection

This is equally important: when AI coding assistants (Copilot, Cursor, Claude, etc.) generate or modify code in a Neuron project, they MUST follow these rules or the **compiler will reject** their output.

Without `.neuronsettings`:
- An agent might dump 500 lines into a single method
- An agent might create `utilities.nr` with 12 classes inside
- An agent might skip documentation entirely
- An agent might use `processData()` as a method name (lowercase start)
- An agent might place `.nr` files directly in the project root

With `.neuronsettings`, **none of this compiles**. The rules act as a guardrail that protects the project's quality regardless of who â€” or what â€” writes the code.

---

## Default Configuration

```ini
# â”€â”€ Structure â”€â”€
max_classes_per_file = 1
max_lines_per_file = 1000
forbid_root_scripts = true

# â”€â”€ Naming â”€â”€
require_method_uppercase_start = true
enforce_strict_file_naming = true
min_method_name_length = 4
require_const_uppercase = true

# â”€â”€ Visibility â”€â”€
require_class_explicit_visibility = true
require_property_explicit_visibility = true

# â”€â”€ Complexity â”€â”€
max_lines_per_method = 50
max_lines_per_block_statement = 20
max_nesting_depth = 3

# â”€â”€ Documentation â”€â”€
require_script_docs = true
require_script_docs_exclude = ["Test*"]
require_script_docs_min_lines = 5
require_public_method_docs = true

# â”€â”€ Testing â”€â”€
max_auto_test_duration_ms = 5000

# â”€â”€ Agent Integration â”€â”€
agent_hints = true
```

---

## Disabling Rules

Set a rule to `0` or `false` to disable it:

```ini
max_lines_per_method = 0
require_script_docs = false
```

> **âš ï¸ Warning:** Disabling rules removes protection. Use with caution.

---

## Rules by Category

- [Structure Rules](structure_rules.md) â€” File limits, root script policy
- [Naming Rules](naming_rules.md) â€” PascalCase, file naming, const casing
- [Visibility Rules](visibility_rules.md) â€” Explicit public/private enforcement
- [Complexity Rules](complexity_rules.md) â€” Method length, block length, nesting depth
- [Documentation Rules](documentation_rules.md) â€” Script docs, method docs
- [Testing Rules](testing_rules.md) â€” Automated test gate (`tests/auto`)
- [Agent Hints](agent_hints.md) â€” AI-specific compiler guidance
