# Contributing to Neuron

Thank you for your interest in contributing to **Neuron**! To maintain a high-quality codebase and clear project history, we follow a strict **Issue-First** development pipeline.

## The Development Pipeline

Every change must go through the following lifecycle:

1. **Open an Issue:** Before writing any code, you must create a GitHub Issue describing the problem or the proposed feature.
2. **Discussion:** Wait for the issue to be discussed and approved. This ensures the approach aligns with the project's architecture.
3. **Create a Pull Request:** Once approved, implement your changes in a separate branch.
4. **Submit for Review:** Open a PR targeting the `main` or `release` branch.

---

## Commit Message Guidelines

We enforce a strict commit format to ensure every change is traceable to its original discussion.

**Format:** `[ISSUE#ID] Short descriptive message`

**Example:**

- `[ISSUE#42] Implement tensor broadcasting logic`
- `[ISSUE#105] Fix memory leak in ModuleResolver`

### Why this format?

- **Traceability:** One glance at the `git log` tells exactly *why* a line of code was changed.
- **Automation:** Our pipeline automatically links commits to issues, closing them upon merge.
- **Squash & Merge:** We prefer squashing PRs into a single, clean commit. Having the Issue ID in the title ensures the final history is readable.

---

## Using the Helpers

To make following this pipeline easier, we provide automation scripts in the `helpers/` directory:

### 1. Preparing your PR

Instead of manually managing branches and commits, use:

```powershell
python helpers/prepare_pr.py feature/my-feature "My commit message" 123
```

This will create a clean branch and format your commit as `[ISSUE#123] My commit message`.

### 2. Pushing to Public

When ready to upload:

```powershell
python helpers/push_pr.py feature/my-feature
```

---

## Code Quality Standards

- **Formatting:** We use `.clang-format`. Run it on your files before submitting.
- **Clean UTF-8:** No Byte Order Marks (BOM) are allowed. Use `helpers/strip_bom.py` if needed.
- **Encoding:** Ensure diagnostic files are properly encoded UTF-8. Use `helpers/repair_mojibake.py` if you encounter corrupted strings.

---

## Summary

We value **quality over quantity**. A clean project history is just as important as the code itself. By following this pipeline, you help us keep Neuron a state-of-the-art engine.
