# Naming Rules

Rules that enforce consistent naming conventions across the project.

---

## Rules

| Rule | Default | Effect |
|------|---------|--------|
| `require_method_uppercase_start` | `true` | Method names must start with uppercase (PascalCase) |
| `enforce_strict_file_naming` | `true` | Filename must match the class name inside it |
| `min_method_name_length` | `4` | Minimum method name characters |
| `require_const_uppercase` | `true` | `const` variables must use UPPER_CASE |

---

## `require_method_uppercase_start = true`

All method names must start with an uppercase letter (PascalCase convention):

```npp
CalculateSum method(a as int, b as int) as int { }    // âœ…
calculateSum method(a as int, b as int) as int { }    // âŒ rejected
```

**Why:** PascalCase distinguishes methods from variables at a glance:

```npp
player is GetActivePlayer();    // GetActivePlayer = method (uppercase)
score is player.score;          // score = field (lowercase)
```

---

## `enforce_strict_file_naming = true`

Three rules enforced together:

1. **Filename starts uppercase**: `player.nr` âŒ â†’ `Player.nr` âœ…
2. **No underscores**: `my_class.nr` âŒ â†’ `MyClass.nr` âœ…
3. **Class name matches filename**: `Warrior.nr` with `Fighter class` inside âŒ

```
neuronc: error: Invalid .nr filename 'player.nr': filename must start
with an uppercase letter.

neuronc: error: Invalid .nr filename 'my_class.nr': filename cannot
include '_'.

neuronc: error: Class name must match module filename.
Expected: Warrior  Found: Fighter
```

**Why:** O(1) class lookup. You can find any class by its filename â€” no searching necessary. Critical for both humans and AI agents navigating the project.

---

## `min_method_name_length = 4`

Method names must be at least 4 characters:

```npp
Init method() { }         // âœ… (4 characters)
Run method() { }          // âŒ (3 characters â€” too short)
Go method() { }           // âŒ (2 characters â€” too short)
```

**Why:** Prevents meaningless abbreviations like `fn()`, `do()`, `op()`. Method names should describe what they do.

---

## `require_const_uppercase = true`

Constants must use UPPER_CASE naming:

```npp
const MAX_SPEED is 100;     // âœ…
const maxSpeed is 100;      // âŒ rejected
```

**Why:** Constants stand out visually from regular variables, making code easier to scan.

---

## Next Steps

- [Visibility Rules](visibility_rules.md)
