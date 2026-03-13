# Semantic Scope Specification

## Scope

This folder covers name lookup, declaration visibility, duplicate detection, legal shadowing, and source-level module/file-name relationships.

## Error Codes

| Code or range | Meaning |
| --- | --- |
| `N2200-N2299` | unknown names, duplicate declarations, visibility failures, shadowing violations |
| `N4000-N4099` | source-visible module/file resolution failures |
| `W2000-W2099` | legal-but-risky shadowing or dead-scope warnings |
| `NR2001`, `NR4001` | current coarse fallbacks |

## Test List

| Name | `.nr` input code | Expected output or error | Why important |
| --- | --- | --- | --- |
| `scope__unknown_identifier` | `Init is method() { Print(missing); }` | `N2200-N2299` | Baseline unresolved-name failure |
| `scope__builtin_name_as_value` | `Print;` | `N2200-N2299` with message explaining `Print(...)` call form | Existing repo behavior that should stay stable |
| `scope__duplicate_base_class` | `Dog is class inherits Animal, Animal { }` | `N2200-N2299` | Guards duplicate symbol use inside inheritance lists |
| `scope__self_module_import_rule` | `module Box; Box is class { value is 0 as int; }` in `Box.nr` under strict naming rules | `N4000-N4099` or semantic fallback | Matches a current strict-naming semantic contract |
| `scope__class_filename_mismatch` | `Vec2 is class { x is 0 as int; }` in `Vector2.nr` under strict naming rules | `N4000-N4099` or semantic fallback | Ensures filename-based scope rules stay explicit |
| `scope__shadow_warning_or_acceptance` | outer `value` shadowed by inner `value` | Either warning-only success or hard error per language policy | Forces the shadowing policy to be documented, not implicit |
| `scope__duplicate_inline_handler` | duplicate `OnFrame` handlers in one canvas block | `N2200-N2299` | Example of domain-specific duplicate detection already present in semantic tests |

## Edge Cases

- Shadowing between globals, locals, fields, and parameters
- Scope exit after `if`, `while`, `for`, `try`, and `catch`
- Named arguments referencing parameter names that collide with locals
- Imported module names colliding with class or method names
- Case sensitivity in identifiers and file stems
- Built-in methods like `Input` and `Print` treated as reserved callables

## Happy Path

Happy-path tests should include legal shadowing if supported, forward visibility if supported, and correct lookup preference across nested scopes.

## Error Path

Error-path tests should specify the declaration site and the conflicting use or duplicate site, not only the final message text.
