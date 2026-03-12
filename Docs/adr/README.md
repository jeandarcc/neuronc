# ADR Index

Architecture Decision Records capture important design decisions and their rationale.
ADRs are immutable once accepted — new decisions create new ADRs.

## Format

Each ADR contains:
- **Status:** Proposed / Accepted / Deprecated / Superseded by ADR-XXXX
- **Context:** Why the decision was needed
- **Decision:** What was chosen
- **Consequences:** Trade-offs and downstream effects

## Records

| ADR | Title | Status |
|-----|-------|--------|
| [0001](0001-llvm-backend.md) | Use LLVM as Code Generation Backend | Accepted |
| [0002](0002-nir-mir-dual-ir.md) | Two-Layer IR Design (NIR + MIR) | Accepted |
| [0003](0003-ncon-manifest.md) | ncon Package Manifest Format (neuron.toml) | Accepted |

## Creating a New ADR

1. Copy the naming pattern: `NNNN-short-hyphenated-title.md`
2. Use this template at the top of the file:
   ```markdown
   # ADR NNNN: <Title>
   **Status:** Proposed
   **Date:** YYYY-MM-DD
   **Deciders:** <team>
   **Categories:** <Compiler / Runtime / Tooling / …>
   ```
3. Fill in Context, Decision, Consequences sections.
4. Change Status to `Accepted` when merged.
5. Add a row to the table above.
