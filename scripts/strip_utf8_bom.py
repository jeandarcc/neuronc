#!/usr/bin/env python3
"""
Safely removes UTF-8 BOM bytes from source files.

Default scope: src/nir
"""

from __future__ import annotations

import argparse
import os
import tempfile
from pathlib import Path

UTF8_BOM = b"\xef\xbb\xbf"
DEFAULT_EXTENSIONS = {".c", ".cc", ".cpp", ".cxx", ".h", ".hh", ".hpp", ".hxx"}


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Remove UTF-8 BOM from text source files using safe atomic writes."
    )
    parser.add_argument(
        "--root",
        default="src/nir",
        help="Root directory to scan recursively (default: src/nir).",
    )
    parser.add_argument(
        "--check",
        action="store_true",
        help="Only report files that contain BOM; do not modify files.",
    )
    parser.add_argument(
        "--verbose",
        action="store_true",
        help="Print each scanned file.",
    )
    return parser.parse_args()


def iter_source_files(root: Path):
    for path in root.rglob("*"):
        if path.is_file() and path.suffix.lower() in DEFAULT_EXTENSIONS:
            yield path


def file_has_bom(path: Path) -> bool:
    with path.open("rb") as handle:
        prefix = handle.read(3)
    return prefix == UTF8_BOM


def rewrite_without_bom(path: Path) -> bool:
    original = path.read_bytes()
    if not original.startswith(UTF8_BOM):
        return False

    stripped = original
    while stripped.startswith(UTF8_BOM):
        stripped = stripped[len(UTF8_BOM) :]
    fd, tmp_name = tempfile.mkstemp(
        prefix=f".{path.name}.",
        suffix=".tmp",
        dir=str(path.parent),
    )
    tmp_path = Path(tmp_name)

    try:
        with os.fdopen(fd, "wb") as tmp_file:
            tmp_file.write(stripped)
            tmp_file.flush()
            os.fsync(tmp_file.fileno())
        os.replace(tmp_path, path)
    finally:
        if tmp_path.exists():
            tmp_path.unlink()

    return True


def main() -> int:
    args = parse_args()
    root = Path(args.root).resolve()

    if not root.exists():
        print(f"[error] root path does not exist: {root}")
        return 2
    if not root.is_dir():
        print(f"[error] root path is not a directory: {root}")
        return 2

    scanned = 0
    bom_files = []
    changed = 0

    for path in iter_source_files(root):
        scanned += 1
        if args.verbose:
            print(f"[scan] {path}")
        if not file_has_bom(path):
            continue

        bom_files.append(path)
        if args.check:
            continue

        if rewrite_without_bom(path):
            changed += 1

    if bom_files:
        print("[bom] files:")
        for path in bom_files:
            print(f"  - {path}")
    else:
        print("[bom] no UTF-8 BOM detected")

    if args.check:
        print(f"[summary] scanned={scanned} bom_found={len(bom_files)}")
    else:
        print(f"[summary] scanned={scanned} bom_found={len(bom_files)} changed={changed}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
