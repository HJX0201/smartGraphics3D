#!/usr/bin/env python3
"""Create the optional smartGraphics3D Windows x64 toolchain archive.

Qt is intentionally excluded. Users install Qt 5.12.10 MSVC x64 separately.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import shutil
import subprocess
import sys
import zipfile
from datetime import datetime, timezone
from pathlib import Path


if hasattr(sys.stdout, "reconfigure"):
    sys.stdout.reconfigure(encoding="utf-8")
    sys.stderr.reconfigure(encoding="utf-8")

PACKAGE_NAME = "smartGraphics3D-toolchain-v0.1.0-windows"
EXCLUDED_SUFFIXES = {".pdb", ".obj", ".ilk", ".exp", ".log", ".tmp", ".bak"}
EXCLUDED_NAMES = {
    "__pycache__",
    ".git",
    "doc",
    "docs",
    "documentation",
    "examples",
    "samples",
    "test",
    "tests",
}


def fail(message: str) -> None:
    raise SystemExit(f"错误：{message}")


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--occt-root", required=True, type=Path)
    parser.add_argument("--cmake-root", required=True, type=Path)
    parser.add_argument("--ninja", required=True, type=Path)
    parser.add_argument("--python-root", required=True, type=Path)
    parser.add_argument("--occt-source", type=Path)
    parser.add_argument(
        "--output-dir",
        type=Path,
        default=Path(__file__).resolve().parents[2] / "artifacts",
    )
    parser.add_argument(
        "--format",
        choices=("auto", "7z", "zip", "directory"),
        default="auto",
        help="auto 优先 7z，找不到 7z.exe 时回退为 zip",
    )
    return parser.parse_args()


def require_file(path: Path, description: str) -> Path:
    resolved = path.expanduser().resolve()
    if not resolved.is_file():
        fail(f"{description}不存在：{resolved}")
    return resolved


def require_directory(path: Path, description: str) -> Path:
    resolved = path.expanduser().resolve()
    if not resolved.is_dir():
        fail(f"{description}不存在：{resolved}")
    return resolved


def should_copy(path: Path) -> bool:
    if path.name.lower() in EXCLUDED_NAMES:
        return False
    return path.suffix.lower() not in EXCLUDED_SUFFIXES


def copy_tree(source: Path, destination: Path) -> None:
    for item in source.rglob("*"):
        relative = item.relative_to(source)
        if any(part.lower() in EXCLUDED_NAMES for part in relative.parts):
            continue
        if item.is_dir() or not should_copy(item):
            continue
        target = destination / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(item, target)


def copy_occt(source: Path, destination: Path) -> None:
    required = ("inc", "cmake")
    for name in required:
        folder = source / name
        if not folder.is_dir():
            fail(f"OCCT SDK 缺少 {name}：{source}")
        copy_tree(folder, destination / name)

    runtime_root = source / "win64"
    if not runtime_root.is_dir():
        fail(f"OCCT SDK 不是预期的 x64 布局，缺少 win64：{source}")
    runtime_destination = destination / "win64"
    if (runtime_root / "vc14").is_dir():
        runtime_root = runtime_root / "vc14"
        runtime_destination = runtime_destination / "vc14"
    for name in ("bin", "bind", "lib", "libd"):
        folder = runtime_root / name
        if folder.is_dir():
            copy_tree(folder, runtime_destination / name)
    if not (runtime_destination / "bin" / "TKernel.dll").is_file():
        fail("整理后的 OCCT Release 运行库缺少 TKernel.dll")
    if not (runtime_destination / "bind" / "TKernel.dll").is_file():
        fail("整理后的 OCCT Debug 运行库缺少 TKernel.dll")


def copy_cmake(source: Path, destination: Path) -> None:
    executable = source / "bin" / "cmake.exe"
    if not executable.is_file():
        fail(f"CMake 根目录缺少 bin/cmake.exe：{source}")
    for name in ("bin", "share"):
        folder = source / name
        if folder.is_dir():
            copy_tree(folder, destination / name)


def copy_python(source: Path, destination: Path) -> None:
    executable = source / "python.exe"
    if not executable.is_file():
        fail(f"便携 Python 根目录缺少 python.exe：{source}")
    for item in source.iterdir():
        if item.name.lower() in EXCLUDED_NAMES or not should_copy(item):
            continue
        if item.is_dir():
            copy_tree(item, destination / item.name)
        elif item.is_file():
            destination.mkdir(parents=True, exist_ok=True)
            shutil.copy2(item, destination / item.name)


def copy_licenses(
    source: Path | None, destination: Path, prefix: str = "OCCT-"
) -> list[str]:
    copied: list[str] = []
    if source is None or not source.is_dir():
        return copied
    patterns = ("LICENSE*", "COPYING*", "README*", "*EXCEPTION*")
    for pattern in patterns:
        for item in source.glob(pattern):
            if not item.is_file():
                continue
            target = destination / f"{prefix}{item.name}"
            destination.mkdir(parents=True, exist_ok=True)
            shutil.copy2(item, target)
            copied.append(target.name)
    return sorted(set(copied))

def copy_license_file(source: Path, destination: Path, name: str) -> list[str]:
    if not source.is_file():
        return []
    destination.mkdir(parents=True, exist_ok=True)
    shutil.copy2(source, destination / name)
    return [name]


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def write_hashes(stage: Path) -> None:
    files = sorted(
        path for path in stage.rglob("*") if path.is_file() and path.name != "SHA256SUMS"
    )
    lines = [f"{sha256(path)}  {path.relative_to(stage).as_posix()}" for path in files]
    (stage / "SHA256SUMS").write_text("\n".join(lines) + "\n", encoding="utf-8")


def find_7z() -> Path | None:
    candidates = [
        shutil.which("7z"),
        shutil.which("7z.exe"),
        r"C:\Program Files\7-Zip\7z.exe",
        r"C:\Program Files\Bandizip\7z.exe",
    ]
    for candidate in candidates:
        if candidate and Path(candidate).is_file():
            return Path(candidate)
    return None


def create_zip(stage: Path, archive: Path) -> None:
    with zipfile.ZipFile(archive, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=9) as out:
        for path in sorted(stage.rglob("*")):
            if path.is_file():
                out.write(path, f"{stage.name}/{path.relative_to(stage).as_posix()}")


def create_archive(stage: Path, output_dir: Path, archive_format: str) -> Path:
    seven_zip = find_7z()
    selected = archive_format
    if selected == "auto":
        selected = "7z" if seven_zip else "zip"
    if selected == "directory":
        return stage
    if selected == "7z":
        if seven_zip is None:
            fail("未找到 7z.exe；请安装 7-Zip，或使用 --format zip")
        archive = output_dir / f"{PACKAGE_NAME}.7z"
        if archive.exists():
            archive.unlink()
        subprocess.run(
            [str(seven_zip), "a", "-t7z", "-mx=9", str(archive), str(stage)],
            check=True,
        )
        return archive
    archive = output_dir / f"{PACKAGE_NAME}.zip"
    if archive.exists():
        archive.unlink()
    create_zip(stage, archive)
    return archive


def main() -> int:
    arguments = parse_arguments()
    occt = require_directory(arguments.occt_root, "OCCT SDK")
    cmake = require_directory(arguments.cmake_root, "CMake")
    ninja = require_file(arguments.ninja, "Ninja")
    python = require_directory(arguments.python_root, "便携 Python")
    occt_source = (
        require_directory(arguments.occt_source, "OCCT 源码")
        if arguments.occt_source
        else None
    )
    output_dir = arguments.output_dir.expanduser().resolve()
    stage = output_dir / PACKAGE_NAME
    output_dir.mkdir(parents=True, exist_ok=True)
    if stage.exists():
        if stage.parent != output_dir or stage.name != PACKAGE_NAME:
            fail(f"拒绝清理非预期暂存目录：{stage}")
        shutil.rmtree(stage)
    stage.mkdir()

    copy_occt(occt, stage / "occt")
    copy_cmake(cmake, stage / "cmake")
    (stage / "ninja").mkdir()
    shutil.copy2(ninja, stage / "ninja" / "ninja.exe")
    copy_python(python, stage / "python")
    copied_licenses = copy_licenses(occt, stage / "licenses")
    copied_licenses.extend(copy_licenses(occt_source, stage / "licenses"))
    copied_licenses.extend(
        copy_license_file(
            cmake / "doc" / "cmake" / "Copyright.txt",
            stage / "licenses",
            "CMake-Copyright.txt",
        )
    )
    copied_licenses.extend(
        copy_license_file(
            python / "LICENSE.txt",
            stage / "licenses",
            "Python-LICENSE.txt",
        )
    )
    bundled_licenses = Path(__file__).resolve().parents[2] / "toolchain" / "licenses"
    copied_licenses.extend(copy_licenses(bundled_licenses, stage / "licenses", ""))
    copied_licenses = sorted(set(copied_licenses))

    manifest = {
        "package": PACKAGE_NAME,
        "version": "0.1.0",
        "platform": "windows-x64",
        "createdUtc": datetime.now(timezone.utc).isoformat(),
        "qtIncluded": False,
        "requirements": {
            "qt": "5.12.10 MSVC x64 (user installed)",
            "visualStudio": "2022 C++ Desktop",
            "windowsSdk": "10 or 11",
        },
        "components": {
            "occt": "7.7.0",
            "licenses": copied_licenses,
        },
        "paths": {
            "cmake": "cmake/bin/cmake.exe",
            "ninja": "ninja/ninja.exe",
            "occt": "occt",
            "python": "python/python.exe",
        },
    }
    (stage / "toolchain-manifest.json").write_text(
        json.dumps(manifest, ensure_ascii=False, indent=2) + "\n",
        encoding="utf-8",
    )
    write_hashes(stage)
    archive = create_archive(stage, output_dir, arguments.format)
    size = archive.stat().st_size if archive.is_file() else 0
    if archive.is_file() and size >= 2 * 1024 * 1024 * 1024:
        fail(f"压缩包达到 {size} 字节，超过 GitHub Release 单附件 2 GiB 限制")
    print(f"工具链输出：{archive}")
    if archive.is_file():
        print(f"大小：{size / (1024 * 1024):.2f} MiB")
        print(f"SHA-256：{sha256(archive)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
