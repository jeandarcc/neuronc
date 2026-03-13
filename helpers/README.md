# Neuron Fixers

This directory contains utility scripts to repair common repository issues like encoding corruption or historical branding mismatches.

## Scripts

### `repair_mojibake.py`
This script repairs **Mojibake** (encoding corruption) where UTF-8 files were accidentally interpreted as `cp1252` and saved back to UTF-8.

### `strip_bom.py`
Automated tool to remove **Byte Order Marks (BOM)** from source files. Especially useful for fixing parser errors in manifest and configuration files.

### `normalize_diagnostics.py`
Standardizes diagnostic keys in TOML files. Converts lowercase keys (e.g., `nr9001`) to legacy-compatible uppercase format (`NR9001`).

### `sync_public_templates.py`
Helper script to resolve Git conflicts in `.github/ISSUE_TEMPLATE` by forcing the version from `public-repo/main`. Use this when you want to keep the public repository's official templates without manual merging.

### `prepare_pr.py`
Automates the workflow of creating a new branch, staging all changes, and committing everything in one command. Perfect for when you have uncommitted work and want to move it to a clean PR branch.
**Usage:** `python fixers/prepare_pr.py <branch-name> "<commit message>"`

## Safety
- These scripts modify files in-place.
- Always review changes using `git diff` before committing.
- By default, the script skips directories named `backups` or `.git`.
