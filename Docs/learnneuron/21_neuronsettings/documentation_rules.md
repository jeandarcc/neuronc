# Documentation Rules

Rules that enforce documentation as a first-class requirement.

---

## Rules

| Rule | Default | Effect |
|------|---------|--------|
| `require_script_docs` | `true` | Each `.nr` file needs a `docs/scripts/<Name>.md` |
| `require_script_docs_exclude` | `["Test*"]` | Files matching these patterns are exempt |
| `require_script_docs_min_lines` | `5` | Doc file must have minimum 5 non-empty lines |
| `require_public_method_docs` | `true` | Public methods need `/// <summary>` XML docs |

---

## `require_script_docs = true`

For every `src/Player.nr`, a matching `docs/scripts/Player.md` must exist:

```
neuronc: error: Missing script documentation:
expected 'docs/scripts/Player.md'.
For agents: Create docs/scripts/<ScriptName>.md before compiling.
```

**Why:** This prevents the universal problem of "we'll document it later" â€” which means never. Documentation is enforced **at compile time**.

For AI agents: if an agent creates a new `.nr` file, it **must** also create the matching documentation file or the project won't compile.

---

## `require_script_docs_exclude = ["Test*"]`

Files matching these glob patterns are exempt from the documentation requirement:

```ini
require_script_docs_exclude = ["Test*"]
```

This means `TestPlayer.nr`, `TestVector.nr`, etc. don't need doc files â€” test files are self-documenting.

---

## `require_script_docs_min_lines = 5`

The documentation file must contain at least 5 non-empty lines:

```
neuronc: error: Script documentation 'docs/scripts/Player.md' is too
short (2 non-empty lines, minimum 5).
For agents: Add meaningful usage details to satisfy
require_script_docs_min_lines.
```

**Why:** Prevents empty or stub documentation from passing the check. 5 lines forces at least a title and a meaningful description.

---

## `require_public_method_docs = true`

Public methods must have XML documentation comments:

```npp
/// <summary>
/// Calculates the distance between two vectors.
/// </summary>
Distance public method(other as Vector2) as float {
    return Math.sqrt((this.x - other.x) * (this.x - other.x));
}
```

```
neuronc: error: Public method 'Distance' must have XML documentation
comments with <summary>.
For agents: Add C#-style docs directly above the method,
e.g. `/// <summary>...</summary>`.
```

**Why:** Public methods are API contracts. Anyone consuming a class needs to understand what each public method does without reading the implementation.

---

## Next Steps

- [Testing Rules](testing_rules.md)
