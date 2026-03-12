# Module Diagnostic Specification

## Scope

This folder covers diagnostics for module declarations, imports, file/module naming contracts, and modulecpp integration as seen from `.npp` source.

## Error Codes

| Code or range | Meaning |
| --- | --- |
| `N4000-N4099` | module/import/source-resolution diagnostics |
| `W4000-W4099` | warning-only module diagnostics |
| `NPP4001` | current fallback |

## Test List

| Name | `.npp` input code | Expected output or error | Why important |
| --- | --- | --- | --- |
| `diag_module__missing_module_name` | `module MissingPkg; Init is method() {}` | Message names the missing module and failure reason | Smallest module-resolution contract |
| `diag_module__self_import_rule` | `module Box; Box is class {}` in `Box.npp` with strict naming | Message states module/file cannot import itself | Existing repo-specific rule should stay understandable |
| `diag_module__class_filename_mismatch` | `Vec2 is class {}` in `Vector2.npp` with strict naming | Message mentions both class name and filename mismatch | Critical for project-structure usability |
| `diag_module__modulecpp_missing_config` | `modulecpp Tensorflow; Init is method() { return; }` without configured exports | Message says configuration/export metadata is missing | Avoids opaque native-module failures |
| `diag_module__redundant_import_warning` | repeated legal import if compatibility mode allows it | Warning points to duplicate declaration without blocking build | Warning severity must remain stable if implemented |

## Edge Cases

- Case-insensitive vs case-sensitive module names
- Duplicate module declarations in one file
- Module diagnostics combined with valid later declarations
- File path, file stem, and declared type-name disagreement
- Native module exports with wrong signature metadata

## Happy Path

Happy-path tests ensure valid module and modulecpp usage emit no module diagnostics.

## Error Path

Error-path tests should require diagnostics to mention the user-facing module/type names, not only internal config keys.
