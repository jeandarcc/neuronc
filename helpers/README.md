# Neuron Helpers

This directory contains utility scripts to repair common repository issues and automate development workflows.

## Development & Git Automation

### `prepare_pr.py`
Automates the workflow of creating a clean PR branch. It squashes all your current work (committed or not) from your source branch into a single clean commit on a new branch based on `public-repo/main`.
**Usage:** `python helpers/prepare_pr.py <new-branch-name> "<commit message>"`

### `push_pr.py`
Quickly pushes your current local branch to a specific branch on `public-repo`. Automatically detects your current branch so you only have to specify the target.
**Usage:** `python helpers/push_pr.py <remote-branch-name> [--force]`

### `sync_public_templates.py`
Helper script to resolve Git conflicts in `.github/ISSUE_TEMPLATE` by forcing the version from `public-repo/main`. Use this when you want to keep the public repository's official templates without manual merging.

## Repository Maintenance

### `repair_mojibake.py`
Repairs **Mojibake** (encoding corruption) where UTF-8 files were accidentally interpreted as `cp1252` and saved back to UTF-8.

### `strip_bom.py`
Automated tool to remove **Byte Order Marks (BOM)** from source files. Especially useful for fixing parser errors in manifest and configuration files.

### `normalize_diagnostics.py`
Standardizes diagnostic keys in TOML files. Converts lowercase keys (e.g., `nr9001`) to legacy-compatible uppercase format (`NR9001`).

### `migrate_nr_extensions.py`
Handles the transition from `.npp` to `.nr` by physically renaming files and updating extension references within the source code.

## Safety
- These scripts modify files in-place or perform Git operations.
- Always review changes using `git diff` before committing.
- Ensure you have a clean working state or have committed/stashed important work before running automation scripts.
