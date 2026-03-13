# Neuron Fixers

This directory contains utility scripts to repair common repository issues like encoding corruption or historical branding mismatches.

## Scripts

### `repair_mojibake.py`

This script repairs **Mojibake** (encoding corruption) where UTF-8 files were accidentally interpreted as `Windows-1252` (CP1252) and saved back to UTF-8. 

**Symptoms Fixed:**
- Chinese/Japanese characters appearing as `å…±äº«å…¼å®¹`
- Turkish characters appearing as `Ä°Ã§erik`
- Unexpected `...` sembols instead of specific UTF-8 bytes.

**Usage:**
```powershell
python fixers/repair_mojibake.py
```

**How it works:**
The script performs "aggressive recovery" by mapping corrupted CP1252 character sequences back to their original raw bytes and re-decoding them as proper UTF-8. It also automatically removes Byte Order Marks (BOM) to ensure cross-platform compatibility.

## Safety
- These scripts modify files in-place.
- Always review changes using `git diff` before committing.
- By default, the script skips directories named `backups` or `.git`.
