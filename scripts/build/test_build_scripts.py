"""Unit tests for Qt discovery and deployment in both standalone build scripts."""

from __future__ import annotations

import importlib.util
import os
import tempfile
import unittest
from pathlib import Path
from types import ModuleType
from unittest import mock


SCRIPT_DIRECTORY = Path(__file__).resolve().parent


def load_script(name: str) -> ModuleType:
    path = SCRIPT_DIRECTORY / name
    spec = importlib.util.spec_from_file_location(f"test_{path.stem}", path)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"无法加载 {path}")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def create_qt_candidate(root: Path, debug: bool) -> None:
    suffix = "d" if debug else ""
    files = [
        root / "bin" / "qmake.exe",
        root / "lib" / "cmake" / "Qt5" / "Qt5Config.cmake",
        root / "bin" / "Qt5Core.dll",
        root / "bin" / f"Qt5Core{suffix}.dll",
        root / "bin" / f"Qt5Gui{suffix}.dll",
        root / "bin" / f"Qt5Widgets{suffix}.dll",
        root / "bin" / f"Qt5Svg{suffix}.dll",
        root / "plugins" / "platforms" / f"qwindows{suffix}.dll",
        root / "plugins" / "iconengines" / f"qsvgicon{suffix}.dll",
        root / "plugins" / "imageformats" / f"qgif{suffix}.dll",
        root / "plugins" / "imageformats" / f"qico{suffix}.dll",
        root / "plugins" / "imageformats" / f"qjpeg{suffix}.dll",
        root / "plugins" / "imageformats" / f"qsvg{suffix}.dll",
        root / "plugins" / "styles" / f"qwindowsvistastyle{suffix}.dll",
    ]
    for path in files:
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(b"test")


class SBuildScriptTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.modules = [
            load_script("build_64_debug.py"),
            load_script("build_64_release.py"),
        ]

    def test_explicit_and_environment_candidates_have_priority(self) -> None:
        explicit = Path(r"C:\explicit-qt")
        environment_root = Path(r"C:\environment-qt")
        for module in self.modules:
            with mock.patch.dict(
                os.environ,
                {"SMARTGRAPHICS3D_QT_ROOT": str(environment_root)},
                clear=False,
            ):
                with mock.patch.object(module.shutil, "which", return_value=None):
                    candidates = module.qt_candidates(explicit)
            self.assertEqual(candidates[0], explicit)
            self.assertEqual(candidates[1], environment_root)

    def test_accepts_exact_qt_version_and_x64(self) -> None:
        for module in self.modules:
            with self.subTest(script=module.__name__):
                with tempfile.TemporaryDirectory() as directory:
                    root = Path(directory)
                    create_qt_candidate(root, module.BUILD_TYPE == "Debug")
                    with mock.patch.object(module, "qt_candidates", return_value=[root]):
                        with mock.patch.object(module, "capture", return_value="5.12.10"):
                            with mock.patch.object(
                                module, "pe_machine", return_value=module.MACHINE_AMD64
                            ):
                                self.assertEqual(module.find_qt(root, {}), root)

    def test_rejects_wrong_version_wrong_architecture_and_missing_qt(self) -> None:
        for module in self.modules:
            with self.subTest(script=module.__name__):
                with tempfile.TemporaryDirectory() as directory:
                    root = Path(directory)
                    create_qt_candidate(root, module.BUILD_TYPE == "Debug")
                    with mock.patch.object(module, "qt_candidates", return_value=[root]):
                        with mock.patch.object(module, "capture", return_value="6.10.1"):
                            with self.assertRaises(RuntimeError):
                                module.find_qt(root, {})
                        with mock.patch.object(module, "capture", return_value="5.12.10"):
                            with mock.patch.object(module, "pe_machine", return_value=0x014C):
                                with self.assertRaises(RuntimeError):
                                    module.find_qt(root, {})
                    with mock.patch.object(module, "qt_candidates", return_value=[]):
                        with self.assertRaises(RuntimeError):
                            module.find_qt(None, {})

    def test_falls_back_when_windeployqt_is_missing_or_fails(self) -> None:
        for module in self.modules:
            for failing_deploy in (False, True):
                with self.subTest(script=module.__name__, failing=failing_deploy):
                    with tempfile.TemporaryDirectory() as directory:
                        temporary = Path(directory)
                        root = temporary / "qt"
                        output = temporary / "output"
                        output.mkdir()
                        executable = output / "smartGraphics3D.exe"
                        executable.write_bytes(b"test")
                        create_qt_candidate(root, module.BUILD_TYPE == "Debug")
                        deploy = root / "bin" / "windeployqt.exe"
                        if failing_deploy:
                            deploy.write_bytes(b"test")
                        fake_result = mock.Mock(returncode=1)
                        with mock.patch.object(module.subprocess, "run", return_value=fake_result):
                            module.deploy_qt(root, executable, {})
                        suffix = "d" if module.BUILD_TYPE == "Debug" else ""
                        self.assertTrue(
                            (output / "platforms" / f"qwindows{suffix}.dll").is_file()
                        )
                        self.assertTrue((output / f"Qt5Core{suffix}.dll").is_file())


if __name__ == "__main__":
    unittest.main()
