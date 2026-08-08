#!/usr/bin/env python3
"""Scan reachable Git content and release packages for sensitive data."""

from __future__ import annotations

import argparse
import os
import re
import subprocess
import sys
import zipfile
from pathlib import Path, PurePosixPath


MAX_SCAN_BYTES = 128 * 1024 * 1024
FORBIDDEN_TRACKED_SUFFIXES = {
    ".7z",
    ".dll",
    ".dmp",
    ".exe",
    ".ilk",
    ".pdb",
    ".rar",
    ".sg3d",
    ".sgdiag",
    ".zip",
}
FORBIDDEN_RELEASE_SUFFIXES = {".dmp", ".ilk", ".log", ".pdb", ".sg3d", ".sgdiag", ".user"}
SECRET_PATTERNS = {
    "private-key": re.compile(
        b"-----BEGIN " + b"(?:RSA |EC |OPENSSH |DSA )?PRIVATE KEY-----"
    ),
    "github-token": re.compile(rb"gh[pousr]_[A-Za-z0-9_]{20,}|github_pat_[A-Za-z0-9_]{20,}"),
    "cloud-key": re.compile(rb"AKIA[0-9A-Z]{16}|ASIA[0-9A-Z]{16}|AIza[0-9A-Za-z_-]{30,}"),
    "credential-url": re.compile(rb"[A-Za-z][A-Za-z0-9+.-]*://[^/@:\s]+:[^/@\s]+@"),
    "assigned-secret": re.compile(
        rb"(?i)(?:password|passwd|secret|api[_-]?key|access[_-]?token)"
        rb"[ \t]*[:=][ \t]*[\"']?[^\s\"']{8,}"
    ),
}


def run_git(root: Path, arguments: list[str], input_data: bytes | None = None) -> bytes:
    completed = subprocess.run(
        ["git", "-C", str(root), *arguments],
        input=input_data,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=False,
    )
    if completed.returncode != 0:
        message = completed.stderr.decode("utf-8", errors="replace").strip()
        raise RuntimeError(message or f"git {' '.join(arguments)} failed")
    return completed.stdout


def marker_bytes(root: Path) -> list[bytes]:
    values: set[bytes] = set()
    for path in (Path.home().resolve(), root.resolve()):
        text = str(path)
        variants = {text, text.replace("\\", "/"), text.replace("/", "\\")}
        for variant in variants:
            values.add(variant.encode("utf-8"))
            values.add(variant.encode("utf-16-le"))
    values.add((b"smart3D" + b"Cam").lower())
    values.add(("smart3D" + "Cam").encode("utf-16-le").lower())
    return sorted((value for value in values if value), key=len, reverse=True)


def scan_data(data: bytes, markers: list[bytes]) -> set[str]:
    findings = {name for name, pattern in SECRET_PATTERNS.items() if pattern.search(data)}
    lowered = data.lower()
    if any(marker.lower() in lowered for marker in markers):
        findings.add("local-path")
    return findings


def path_policy(path: str) -> set[str]:
    normalized = path.replace("\\", "/").lstrip("./")
    findings: set[str] = set()
    if normalized == "artifacts" or normalized.startswith("artifacts/"):
        findings.add("tracked-artifact")
    if Path(normalized).suffix.lower() in FORBIDDEN_TRACKED_SUFFIXES:
        findings.add("forbidden-tracked-file")
    return findings


def append_data_findings(
    findings: set[tuple[str, str]], location: str, data: bytes, markers: list[bytes]
) -> None:
    for category in scan_data(data, markers):
        findings.add((category, location))


def scan_worktree(root: Path, markers: list[bytes], maximum: int) -> set[tuple[str, str]]:
    findings: set[tuple[str, str]] = set()
    output = run_git(root, ["ls-files", "--cached", "--others", "--exclude-standard", "-z"])
    for raw_path in output.split(b"\0"):
        if not raw_path:
            continue
        relative = raw_path.decode("utf-8", errors="surrogateescape")
        for category in path_policy(relative):
            findings.add((category, relative))
        file_path = root / relative
        if not file_path.is_file():
            continue
        size = file_path.stat().st_size
        if size > maximum:
            findings.add(("oversized-tracked-file", relative))
            continue
        append_data_findings(findings, relative, file_path.read_bytes(), markers)
    return findings


def historical_blobs(root: Path) -> list[tuple[str, int, str]]:
    objects = run_git(root, ["rev-list", "--objects", "--all"])
    if not objects.strip():
        return []
    checked = run_git(
        root,
        ["cat-file", "--batch-check=%(objectname) %(objecttype) %(objectsize) %(rest)"],
        objects,
    )
    blobs: list[tuple[str, int, str]] = []
    seen: set[str] = set()
    for line in checked.decode("utf-8", errors="replace").splitlines():
        parts = line.split(" ", 3)
        if len(parts) < 3 or parts[1] != "blob" or parts[0] in seen:
            continue
        seen.add(parts[0])
        path = parts[3] if len(parts) == 4 else "<unknown>"
        blobs.append((parts[0], int(parts[2]), path))
    return blobs


def scan_history(root: Path, markers: list[bytes], maximum: int) -> set[tuple[str, str]]:
    findings: set[tuple[str, str]] = set()
    for object_id, size, path in historical_blobs(root):
        location = f"history:{path}"
        for category in path_policy(path):
            findings.add((category, location))
        if size > maximum:
            findings.add(("oversized-history-blob", location))
            continue
        data = run_git(root, ["cat-file", "blob", object_id])
        append_data_findings(findings, location, data, markers)
    return findings


def scan_commit_emails(root: Path) -> set[tuple[str, str]]:
    findings: set[tuple[str, str]] = set()
    output = run_git(root, ["log", "--all", "--format=%ae%n%ce"])
    emails = {line.strip().lower() for line in output.decode().splitlines() if line.strip()}
    if any(not email.endswith("@users.noreply.github.com") for email in emails):
        findings.add(("public-commit-email", "Git history"))
    return findings


def unsafe_zip_name(name: str) -> bool:
    path = PurePosixPath(name.replace("\\", "/"))
    return path.is_absolute() or ".." in path.parts


def scan_release_zip(
    archive: Path, markers: list[bytes], maximum: int
) -> set[tuple[str, str]]:
    findings: set[tuple[str, str]] = set()
    with zipfile.ZipFile(archive) as package:
        for entry in package.infolist():
            location = f"{archive.name}:{entry.filename}"
            if unsafe_zip_name(entry.filename):
                findings.add(("unsafe-archive-path", location))
            if Path(entry.filename).suffix.lower() in FORBIDDEN_RELEASE_SUFFIXES:
                findings.add(("forbidden-release-file", location))
            if entry.is_dir():
                continue
            if entry.file_size > maximum:
                findings.add(("oversized-release-file", location))
                continue
            append_data_findings(findings, location, package.read(entry), markers)
    return findings


def scan_release_directory(
    directory: Path, markers: list[bytes], maximum: int
) -> set[tuple[str, str]]:
    findings: set[tuple[str, str]] = set()
    for file_path in directory.rglob("*"):
        if not file_path.is_file():
            continue
        relative = file_path.relative_to(directory).as_posix()
        if file_path.suffix.lower() in FORBIDDEN_RELEASE_SUFFIXES:
            findings.add(("forbidden-release-file", relative))
        if file_path.stat().st_size > maximum:
            findings.add(("oversized-release-file", relative))
            continue
        append_data_findings(findings, relative, file_path.read_bytes(), markers)
    return findings


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[2])
    parser.add_argument("--release", action="append", type=Path, default=[])
    parser.add_argument("--skip-history", action="store_true")
    parser.add_argument("--require-noreply", action="store_true")
    parser.add_argument("--max-bytes", type=int, default=MAX_SCAN_BYTES)
    return parser.parse_args()


def main() -> int:
    arguments = parse_arguments()
    root = arguments.root.resolve()
    markers = marker_bytes(root)
    findings = scan_worktree(root, markers, arguments.max_bytes)
    if not arguments.skip_history:
        findings.update(scan_history(root, markers, arguments.max_bytes))
    if arguments.require_noreply:
        findings.update(scan_commit_emails(root))
    for release in arguments.release:
        release = release.resolve()
        if release.is_dir():
            findings.update(scan_release_directory(release, markers, arguments.max_bytes))
        elif zipfile.is_zipfile(release):
            findings.update(scan_release_zip(release, markers, arguments.max_bytes))
        else:
            findings.add(("unsupported-release-input", str(release)))
    if findings:
        for category, location in sorted(findings):
            print(f"ERROR {category}: {location}")
        print(f"Sensitive-data audit failed with {len(findings)} finding(s).")
        return 1
    print("Sensitive-data audit passed.")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, RuntimeError, subprocess.SubprocessError, zipfile.BadZipFile) as error:
        print(f"ERROR audit-internal: {error}", file=sys.stderr)
        raise SystemExit(2)
