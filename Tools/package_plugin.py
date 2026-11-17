# -*- coding: utf-8 -*-
"""Build a source-only, ready-to-drop-in plugin zip for distribution.

The zip root contains one folder per plugin (named exactly as UE expects under
<Project>/Plugins/). No build outputs, VCS metadata, caches, machine-generated
IDE declarations or sample projects are included. Third-party vendored sources
that are compile-time requirements (UnLuaNova's Lua / Luau submodules) are
copied as real files, with their .ubtignore markers preserved.

Usage:
    python Tools/package_plugin.py                  # writes dist/<archive>.zip
    python Tools/package_plugin.py --out-dir DIR
    python Tools/package_plugin.py --verify-only ZIP

Requires Python 3.8+. No third-party dependencies.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
import sys
import zipfile
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
DIST = REPO / "dist"

# Directory names excluded wherever they appear.
SKIP_DIR_NAMES = {
    ".git", ".github", ".vs", ".vscode", ".idea", ".clang-format-cache",
    "Binaries", "Intermediate", "Saved", "DerivedDataCache", "Build",
    "__pycache__", ".pytest_cache", ".mypy_cache", ".ruff_cache",
    ".workbuddy", ".ual", "_site", "site", "dist", ".venv", "venv",
    "TestResults", "Tests", "TestsData", "evidence", "obj",
}
# Exact file names excluded. VCS metadata only - .ubtignore is intentionally
# kept because UBT needs it to avoid compiling lua.c / luau CLI directly.
SKIP_FILE_NAMES = {".git", ".gitmodules", ".gitattributes", ".gitignore",
                   ".DS_Store", "Thumbs.db"}
SKIP_SUFFIXES = (".pyc", ".pyo", ".duckdb", ".duckdb.wal", ".parquet",
                 ".pdb", ".exp", ".lib", ".obj", ".ilk", ".sln", ".slnx",
                 ".suo", ".user", ".code-workspace", ".tmp", ".log")
# Path prefixes (relative to each plugin source dir) excluded from the archive.
# NOTE: Content/IntelliSense holds hand-written annotations and IS kept; only
# the machine-generated top-level IntelliSense/ is dropped.
SKIP_REL_PREFIXES = []

# Extra root-level files copied next to the plugin folders (licences, docs).
ROOT_FILES = [('LICENSE', 'LICENSE'), ('LICENSE-COMMERCIAL.md', 'LICENSE-COMMERCIAL.md'), ('NOTICE', 'NOTICE'), ('README.md', 'README.md')]
# Each: (source dir relative to repo root, plugin folder name inside the zip).
PLUGIN_DIRS = [('Plugins/UEAbilityKit', 'UEAbilityKit')]
DESCRIPTOR = 'Plugins/UEAbilityKit/UEAbilityKit.uplugin'
RELEASE_NAME = 'UEAbilityKit'
REQUIRED_MARKERS = []


def _skip_rel(rel: Path) -> bool:
    posix = rel.as_posix()
    return any(posix == p.rstrip("/") or posix.startswith(p)
               for p in SKIP_REL_PREFIXES)


def iter_files(src: Path, dest: Path):
    for root, dirnames, filenames in os.walk(src):
        dirnames[:] = sorted(d for d in dirnames if d not in SKIP_DIR_NAMES)
        rel_root = Path(root).relative_to(src)
        for filename in sorted(filenames):
            if filename in SKIP_FILE_NAMES or filename.endswith(SKIP_SUFFIXES):
                continue
            rel = rel_root / filename
            if _skip_rel(rel_root) or _skip_rel(rel):
                continue
            f = Path(root) / filename
            if f.is_symlink():
                continue
            yield f, dest / rel


def collect() -> list:
    entries = []
    for src_rel, dest_name in PLUGIN_DIRS:
        src = REPO / src_rel
        if not src.is_dir():
            raise SystemExit(f"ERROR: plugin source dir not found: {src}")
        entries.extend(iter_files(src, Path(dest_name)))
    for src_rel, dest_name in ROOT_FILES:
        src = REPO / src_rel
        if not src.is_file():
            raise SystemExit(f"ERROR: required release file not found: {src}")
        entries.append((src, Path(dest_name)))
    return entries


def version_name() -> str:
    with (REPO / DESCRIPTOR).open("r", encoding="utf-8-sig") as fh:
        return json.load(fh)["VersionName"]


def validate(names) -> None:
    name_set = set(names)
    for _, dest_name in PLUGIN_DIRS:
        leaf = Path(dest_name).name
        descriptor = f"{dest_name}/{leaf}.uplugin"
        if descriptor not in name_set:
            raise SystemExit(f"ERROR: descriptor missing from archive: {descriptor}")
    for marker in REQUIRED_MARKERS:
        if marker not in name_set:
            raise SystemExit(f"ERROR: required vendored source missing: {marker}")
    bad = [n for n in names
           if n.endswith("/.git") or "/.git/" in f"/{n}"
           or "/Binaries/" in f"/{n}" or "/Intermediate/" in f"/{n}"
           or "/Saved/" in f"/{n}" or "/__pycache__/" in f"/{n}"
           or "/IntelliSense/" in f"/{n}" and "/Content/IntelliSense/" not in f"/{n}"]
    if bad:
        raise SystemExit("ERROR: forbidden paths in archive:/n  " + "\n  ".join(bad[:10]))


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--out-dir", default=str(DIST))
    ap.add_argument("--verify-only", metavar="ZIP")
    args = ap.parse_args()

    if args.verify_only:
        z = Path(args.verify_only)
        with zipfile.ZipFile(z) as zf:
            names = zf.namelist()
        validate(names)
        print(f"OK {z.name}: {len(names)} entries, "
              f"sha256={hashlib.sha256(z.read_bytes()).hexdigest()}")
        return 0

    entries = collect()
    validate([p.as_posix() for _, p in entries])
    out_dir = Path(args.out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)
    version = version_name()
    archive = out_dir / f"{RELEASE_NAME}-{version}-src.zip"
    if archive.exists():
        archive.unlink()
    with zipfile.ZipFile(archive, "w", zipfile.ZIP_DEFLATED, compresslevel=6) as zf:
        for src, dest in entries:
            zf.write(src, dest.as_posix())

    digest = hashlib.sha256(archive.read_bytes()).hexdigest()
    (out_dir / f"{archive.name}.sha256").write_text(
        f"{digest}  {archive.name}\n", encoding="utf-8")
    print(f"OK  {archive}")
    print(f"    files={len(entries)}  size={archive.stat().st_size/1024/1024:.2f} MiB")
    print(f"    sha256={digest}")
    print("    plugins=" + ", ".join(sorted({d for _, d in PLUGIN_DIRS})))
    return 0


if __name__ == "__main__":
    sys.exit(main())
