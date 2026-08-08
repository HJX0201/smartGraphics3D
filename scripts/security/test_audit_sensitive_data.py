from __future__ import annotations

import importlib.util
import tempfile
import unittest
import zipfile
from pathlib import Path


MODULE_PATH = Path(__file__).with_name("audit_sensitive_data.py")
SPEC = importlib.util.spec_from_file_location("audit_sensitive_data", MODULE_PATH)
assert SPEC and SPEC.loader
AUDIT = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(AUDIT)


class SensitiveDataAuditTest(unittest.TestCase):
    def test_allows_safe_data_and_path(self) -> None:
        self.assertEqual(AUDIT.scan_data(b"ordinary configuration", []), set())
        self.assertEqual(AUDIT.path_policy("docs/building.md"), set())

    def test_detects_token_without_returning_value(self) -> None:
        token = b"gh" + b"p_" + b"A" * 32
        self.assertEqual(AUDIT.scan_data(token, []), {"github-token"})

    def test_detects_local_marker(self) -> None:
        marker = b"C:\\Users\\developer"
        self.assertEqual(AUDIT.scan_data(marker + b"\\project", [marker]), {"local-path"})

    def test_rejects_tracked_artifacts(self) -> None:
        findings = AUDIT.path_policy("artifacts/toolchain.zip")
        self.assertIn("tracked-artifact", findings)
        self.assertIn("forbidden-tracked-file", findings)

    def test_rejects_archive_traversal(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            archive = Path(directory) / "release.zip"
            with zipfile.ZipFile(archive, "w") as package:
                package.writestr("../outside.txt", "safe")
            findings = AUDIT.scan_release_zip(archive, [], AUDIT.MAX_SCAN_BYTES)
            self.assertIn(("unsafe-archive-path", "release.zip:../outside.txt"), findings)

    def test_rejects_forbidden_release_file(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            dump = Path(directory) / "crash.dmp"
            dump.write_bytes(b"safe")
            findings = AUDIT.scan_release_directory(Path(directory), [], AUDIT.MAX_SCAN_BYTES)
            self.assertIn(("forbidden-release-file", "crash.dmp"), findings)

    def test_rejects_oversized_release_entry(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            archive = Path(directory) / "release.zip"
            with zipfile.ZipFile(archive, "w") as package:
                package.writestr("payload.bin", b"1234")
            findings = AUDIT.scan_release_zip(archive, [], 3)
            self.assertIn(("oversized-release-file", "release.zip:payload.bin"), findings)


if __name__ == "__main__":
    unittest.main()
