# Diagnostic Message Library

This directory contains the source-of-truth diagnostic dictionary for Neuron++.

It is data only.

- No runtime binding is performed in this phase.
- Messages are split by locale and by diagnostic family.
- The bucket layout mirrors the current behavior-test taxonomy under `neuron_tests/error_messages/`.

## Layout

```text
config/diagnostics/
|- README.md
\- catalog.toml
\- plugger/           # CLI tool to manage diagnostics
   |- plugger.py      # Entry point
   |- plugger_ops.py  # Core logic
   |- plugger_template.toml
   |- pending/        # Generated templates for edits
   \- backups/        # Automatic backups
\- az/
   |- shared_compat.toml
   |- N1xxx_parser.toml
   |- N2xxx_semantic.toml
   |- N3xxx_ownership.toml
   |- N4xxx_module.toml
   \- N5xxx_codegen.toml
\- de/
|  |- shared_compat.toml
|  |- N1xxx_parser.toml
|  |- N2xxx_semantic.toml
|  |- N3xxx_ownership.toml
|  |- N4xxx_module.toml
|  \- N5xxx_codegen.toml
\- en/
   |- shared_compat.toml
   |- N1xxx_parser.toml
   |- N2xxx_semantic.toml
   |- N3xxx_ownership.toml
   |- N4xxx_module.toml
   \- N5xxx_codegen.toml
\- es/
   |- shared_compat.toml
   |- N1xxx_parser.toml
   |- N2xxx_semantic.toml
   |- N3xxx_ownership.toml
   |- N4xxx_module.toml
   \- N5xxx_codegen.toml
\- fr/
   |- shared_compat.toml
   |- N1xxx_parser.toml
   |- N2xxx_semantic.toml
   |- N3xxx_ownership.toml
   |- N4xxx_module.toml
   \- N5xxx_codegen.toml
\- it/
   |- shared_compat.toml
   |- N1xxx_parser.toml
   |- N2xxx_semantic.toml
   |- N3xxx_ownership.toml
   |- N4xxx_module.toml
   \- N5xxx_codegen.toml
\- la/
   |- shared_compat.toml
   |- N1xxx_parser.toml
   |- N2xxx_semantic.toml
   |- N3xxx_ownership.toml
   |- N4xxx_module.toml
   \- N5xxx_codegen.toml
|- ja/
|  |- shared_compat.toml
|  |- N1xxx_parser.toml
|  |- N2xxx_semantic.toml
|  |- N3xxx_ownership.toml
|  |- N4xxx_module.toml
|  \- N5xxx_codegen.toml
\- ko/
   |- shared_compat.toml
   |- N1xxx_parser.toml
   |- N2xxx_semantic.toml
   |- N3xxx_ownership.toml
   |- N4xxx_module.toml
   \- N5xxx_codegen.toml
\- ru/
   |- shared_compat.toml
   |- N1xxx_parser.toml
   |- N2xxx_semantic.toml
   |- N3xxx_ownership.toml
   |- N4xxx_module.toml
   \- N5xxx_codegen.toml
\- tr/
   |- shared_compat.toml
   |- N1xxx_parser.toml
   |- N2xxx_semantic.toml
   |- N3xxx_ownership.toml
   |- N4xxx_module.toml
   \- N5xxx_codegen.toml
\- zh/
   |- shared_compat.toml
   |- N1xxx_parser.toml
   |- N2xxx_semantic.toml
   |- N3xxx_ownership.toml
   |- N4xxx_module.toml
   \- N5xxx_codegen.toml
```

## Why the Files Are Split

- The library must never collapse into one file.
- Each TOML file owns a bounded code space.
- Future additions remain local and reviewable.
- A future validator can reject codes that appear in the wrong file before they are ever bound into the compiler.

## Contract Model

`catalog.toml` is the global registry.

It defines:

- which buckets exist
- which file each bucket owns
- which exact legacy codes are allowed
- which numeric ranges are allowed for `Nxxxx` and `Wxxxx`
- which locales must exist

Each locale TOML file repeats its own bucket contract in `[contracts]`.

That duplication is intentional:

- `catalog.toml` supports repository-wide validation
- file-local `[contracts]` makes each TOML self-describing

## Adding or Updating Codes

It is **strongly recommended** to use the `plugger` tool to maintain consistency across all 12 locales.

### Using the Plugger Tool

1.  **Initialize**: Generate a template for a new code or pull existing data for an update.
    ```bash
    python plugger/plugger.py add
    # OR
    python plugger/plugger.py update N1234
    ```
2.  **Edit**: Open the generated TOML file in `plugger/pending/` and fill in/edit the fields for all locales.
3.  **Apply**: Validate and propagate the changes to all diagnostic files.
    ```bash
    python plugger/plugger.py apply plugger/pending/ADD_N1234.toml
    ```

### Manual Rules (If not using the tool)

1. Pick the correct bucket from `catalog.toml`.
2. Verify the code is inside that bucket's allowed range or exact-code list.
3. Add the same code to **ALL** 12 locales (`en`, `tr`, `ja`, `de`, `zh`, `es`, `fr`, `it`, `la`, `ru`, `az`, `ko`).
4. Keep the same `slug`, `severity`, `phase`, and `tags` across locales.
5. Translate only the human-facing text fields.

## Expected Future Validation Rules

The dictionary is structured so a future loader or CI validator can enforce:

- unknown bucket -> reject
- duplicate code across files -> reject
- code outside file contract -> reject
- locale parity mismatch -> reject
- missing required fields -> reject
- non-canonical sort order -> reject

## Entry Shape

Each message entry lives under `[messages.<CODE>]` and contains:

- `severity`
- `phase`
- `slug`
- `title`
- `summary`
- `default_message`
- `recovery`
- `tags`

These fields are intentionally stable so binding code can stay simple.
