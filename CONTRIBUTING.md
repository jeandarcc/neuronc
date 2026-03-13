# Contributing Guide

## Language Feature Example Rule
- Any new language feature (keyword, syntax form, type system feature, runtime-visible behavior) MUST ship with at least one runnable `.nr` example in `examples/` in the same change.
- The example file should focus on feature behavior (minimal noise) and compile with `neuron compile <file.nr>`.
- If a feature has multiple syntax variants, include each variant in the example or split into multiple example files.

## Naming
- Use clear file names, for example:
  - `CSharpFeatures.nr`
  - `IsOptionalDynamic.nr`
  - `VirtualOverrideOverload.nr`
