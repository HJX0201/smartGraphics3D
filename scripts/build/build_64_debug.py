#!/usr/bin/env python3
"""Build, test, and deploy smartGraphics3D for Windows x64 Debug."""

from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import re
import shutil
import struct
import subprocess
import sys
import tempfile


if hasattr(sys.stdout, "reconfigure"):
    sys.stdout.reconfigure(encoding="utf-8")
    sys.stderr.reconfigure(encoding="utf-8")

BUILD_TYPE = "Debug"
QT_VERSION = "5.12.10"
OCCT_VERSION = "7.7.0"
MACHINE_AMD64 = 0x8664


def fail(message: str) -> "NoReturn":
    raise RuntimeError(message)


def unique_paths(values: list[Path | None]) -> list[Path]:
    result: list[Path] = []
    seen: set[str] = set()
    for value in values:
        if value is None:
            continue
        path = value.expanduser().resolve()
        key = str(path).lower()
        if key not in seen:
            seen.add(key)
            result.append(path)
    return result


def run(command: list[str], environment: dict[str, str], cwd: Path | None = None) -> None:
    print("+", subprocess.list2cmdline(command), flush=True)
    completed = subprocess.run(command, cwd=cwd, env=environment, check=False)
    if completed.returncode != 0:
        fail(f"命令失败，退出码 {completed.returncode}: {subprocess.list2cmdline(command)}")


def capture(command: list[str], environment: dict[str, str]) -> str:
    completed = subprocess.run(
        command,
        env=environment,
        check=False,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        encoding="utf-8",
        errors="replace",
    )
    if completed.returncode != 0:
        return ""
    return completed.stdout.strip()


def pe_machine(path: Path) -> int:
    with path.open("rb") as stream:
        if stream.read(2) != b"MZ":
            return 0
        stream.seek(0x3C)
        offset_data = stream.read(4)
        if len(offset_data) != 4:
            return 0
        stream.seek(struct.unpack("<I", offset_data)[0])
        if stream.read(4) != b"PE\0\0":
            return 0
        machine_data = stream.read(2)
        return struct.unpack("<H", machine_data)[0] if len(machine_data) == 2 else 0


def find_vs_environment() -> dict[str, str]:
    roots = [
        Path(os.environ.get("ProgramFiles", r"C:\Program Files")),
        Path(os.environ.get("ProgramFiles(x86)", r"C:\Program Files (x86)")),
    ]
    candidates: list[Path] = []
    for root in roots:
        vswhere = root / "Microsoft Visual Studio" / "Installer" / "vswhere.exe"
        if vswhere.is_file():
            output = capture(
                [
                    str(vswhere),
                    "-latest",
                    "-products",
                    "*",
                    "-requires",
                    "Microsoft.VisualStudio.Component.VC.Tools.x86.x64",
                    "-property",
                    "installationPath",
                ],
                os.environ.copy(),
            )
            if output:
                candidates.append(
                    Path(output.splitlines()[-1]) / "VC" / "Auxiliary" / "Build" / "vcvars64.bat"
                )
    for edition in ("BuildTools", "Community", "Professional", "Enterprise"):
        candidates.append(
            roots[0]
            / "Microsoft Visual Studio"
            / "2022"
            / edition
            / "VC"
            / "Auxiliary"
            / "Build"
            / "vcvars64.bat"
        )
    vcvars = next((path for path in unique_paths(candidates) if path.is_file()), None)
    if vcvars is None:
        fail(
            "未找到 Visual Studio 2022 C++ x64 工具链。"
            "请按 .vsconfig 安装 Build Tools、MSVC v143 和 Windows SDK。"
        )

    wrapper_path: Path | None = None
    try:
        with tempfile.NamedTemporaryFile(
            mode="w", suffix=".cmd", delete=False, encoding="utf-8"
        ) as wrapper:
            wrapper.write(f'@echo off\ncall "{vcvars}" >nul\nset\n')
            wrapper_path = Path(wrapper.name)
        output = subprocess.check_output(
            ["cmd.exe", "/d", "/c", str(wrapper_path)],
            text=True,
            encoding="utf-8",
            errors="replace",
        )
    finally:
        if wrapper_path is not None:
            wrapper_path.unlink(missing_ok=True)
    environment = os.environ.copy()
    captured_path = ""
    for line in output.splitlines():
        if "=" in line:
            key, value = line.split("=", 1)
            if key == "PATH" or (key.lower() == "path" and not captured_path):
                captured_path = value
            environment[key] = value
    for key in list(environment):
        if key.lower() == "path":
            del environment[key]
    environment["PATH"] = captured_path
    return environment


def load_toolchain(root: Path | None) -> tuple[Path | None, dict]:
    candidates = unique_paths(
        [
            root,
            Path(os.environ["SMARTGRAPHICS3D_TOOLCHAIN_ROOT"])
            if os.environ.get("SMARTGRAPHICS3D_TOOLCHAIN_ROOT")
            else None,
            Path(__file__).resolve().parents[3] / "smartGraphics3D-toolchain-v0.1.0-windows",
            Path(__file__).resolve().parents[2] / ".toolchain",
        ]
    )
    for candidate in candidates:
        manifest_path = candidate / "toolchain-manifest.json"
        if manifest_path.is_file():
            return candidate, json.loads(manifest_path.read_text(encoding="utf-8"))
    return None, {}


def manifest_path(root: Path | None, manifest: dict, key: str) -> Path | None:
    value = manifest.get("paths", {}).get(key)
    return (root / value).resolve() if root is not None and value else None


def find_program(name: str, candidates: list[Path | None], environment: dict[str, str]) -> Path:
    path_value = shutil.which(name, path=environment.get("PATH"))
    paths = unique_paths(candidates + ([Path(path_value)] if path_value else []))
    for path in paths:
        if path.is_file():
            return path
    fail(f"未找到 {name}。请安装工具链包，或通过环境变量指定其位置。")


def qt_candidates(explicit: Path | None) -> list[Path]:
    values: list[Path | None] = [
        explicit,
        Path(os.environ["SMARTGRAPHICS3D_QT_ROOT"])
        if os.environ.get("SMARTGRAPHICS3D_QT_ROOT")
        else None,
        Path(os.environ["QTDIR"]) if os.environ.get("QTDIR") else None,
    ]
    for item in os.environ.get("CMAKE_PREFIX_PATH", "").split(os.pathsep):
        if item:
            values.append(Path(item))
    qmake = shutil.which("qmake")
    if qmake:
        values.append(Path(qmake).resolve().parent.parent)
    values.extend(
        [
            Path(r"C:\Qt\Qt5.12.10\5.12.10\msvc2017_64"),
            Path(r"C:\Qt\5.12.10\msvc2017_64"),
            Path.home() / "Qt" / "5.12.10" / "msvc2017_64",
        ]
    )
    return unique_paths(values)


def find_qt(explicit: Path | None, environment: dict[str, str]) -> Path:
    rejected: list[str] = []
    for root in qt_candidates(explicit):
        qmake = root / "bin" / "qmake.exe"
        config = root / "lib" / "cmake" / "Qt5" / "Qt5Config.cmake"
        core = root / "bin" / "Qt5Core.dll"
        if not qmake.is_file() or not config.is_file() or not core.is_file():
            rejected.append(f"{root}: 缺少qmake、Qt5Config或Qt5Core")
            continue
        version = capture([str(qmake), "-query", "QT_VERSION"], environment)
        if version != QT_VERSION:
            rejected.append(f"{root}: Qt版本为{version or '未知'}")
            continue
        if pe_machine(core) != MACHINE_AMD64:
            rejected.append(f"{root}: 不是x64 Qt")
            continue
        print(f"Qt: {root}")
        return root
    details = "\n".join(f"  - {item}" for item in rejected) or "  - 未发现候选目录"
    fail(
        f"未找到Qt {QT_VERSION} MSVC x64。\n{details}\n"
        "可使用 --qt-root PATH 或 SMARTGRAPHICS3D_QT_ROOT 指定。"
    )


def occt_candidates(explicit: Path | None, toolchain_occt: Path | None) -> list[Path]:
    values: list[Path | None] = [
        explicit,
        Path(os.environ["SMARTGRAPHICS3D_OCCT_ROOT"])
        if os.environ.get("SMARTGRAPHICS3D_OCCT_ROOT")
        else None,
        toolchain_occt,
        Path(os.environ["CASROOT"]) if os.environ.get("CASROOT") else None,
        Path(r"C:\OpenCASCADE\7.7.0"),
        Path(r"C:\OpenCASCADE-7.7.0-vc14-64"),
    ]
    return unique_paths(values)


def find_occt(explicit: Path | None, toolchain_occt: Path | None) -> Path:
    rejected: list[str] = []
    for root in occt_candidates(explicit, toolchain_occt):
        config = root / "cmake" / "OpenCASCADEConfig.cmake"
        release_dll = root / "win64" / "vc14" / "bin" / "TKernel.dll"
        debug_dll = root / "win64" / "vc14" / "bind" / "TKernel.dll"
        if config.is_file() and release_dll.is_file() and debug_dll.is_file():
            if pe_machine(release_dll) == MACHINE_AMD64:
                print(f"OCCT: {root}")
                return root
            rejected.append(f"{root}: TKernel不是x64")
        else:
            rejected.append(f"{root}: 缺少OCCT CMake配置或Debug/Release运行库")
    details = "\n".join(f"  - {item}" for item in rejected) or "  - 未发现候选目录"
    fail(
        f"未找到OCCT {OCCT_VERSION} x64。\n{details}\n"
        "可使用 --occt-root PATH 或 SMARTGRAPHICS3D_OCCT_ROOT 指定。"
    )


def copy_file(source: Path, destination: Path) -> None:
    destination.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(source, destination)


def fallback_deploy_qt(qt_root: Path, output: Path) -> None:
    suffix = "d"
    required_dlls = ["Core", "Gui", "Widgets", "Svg"]
    for module in required_dlls:
        source = qt_root / "bin" / f"Qt5{module}{suffix}.dll"
        if not source.is_file():
            fail(f"Qt回退部署缺少 {source}")
        copy_file(source, output / source.name)
    for name in ("libEGLd.dll", "libGLESV2d.dll", "d3dcompiler_47.dll", "opengl32sw.dll"):
        source = qt_root / "bin" / name
        if source.is_file():
            copy_file(source, output / name)
    plugins = {
        "platforms": ["qwindowsd.dll"],
        "iconengines": ["qsvgicond.dll"],
        "imageformats": ["qgifd.dll", "qicod.dll", "qjpegd.dll", "qsvgd.dll"],
        "styles": ["qwindowsvistastyled.dll"],
    }
    for folder, names in plugins.items():
        for name in names:
            source = qt_root / "plugins" / folder / name
            if source.is_file():
                copy_file(source, output / folder / name)


def deploy_qt(qt_root: Path, executable: Path, environment: dict[str, str]) -> None:
    deploy = qt_root / "bin" / "windeployqt.exe"
    succeeded = False
    if deploy.is_file():
        command = [str(deploy), "--debug", "--no-translations", str(executable)]
        print("+", subprocess.list2cmdline(command), flush=True)
        succeeded = subprocess.run(command, env=environment, check=False).returncode == 0
    if not succeeded:
        print("windeployqt不可用，切换到Qt最小依赖部署。")
        fallback_deploy_qt(qt_root, executable.parent)
    platform = executable.parent / "platforms" / "qwindowsd.dll"
    if not platform.is_file():
        fail(f"Qt平台插件部署失败: {platform}")


def dependent_dlls(path: Path, dumpbin: Path, environment: dict[str, str]) -> list[str]:
    output = capture([str(dumpbin), "/DEPENDENTS", str(path)], environment)
    return re.findall(r"(?im)^\s+([A-Za-z0-9_.+-]+\.dll)\s*$", output)


def deploy_occt(
    occt_root: Path, executable: Path, environment: dict[str, str], dumpbin: Path
) -> None:
    runtime = (
        occt_root
        / "win64"
        / "vc14"
        / ("bind" if BUILD_TYPE == "Debug" else "bin")
    )
    queue = [executable]
    scanned: set[str] = set()
    copied: set[str] = set()
    while queue:
        current = queue.pop(0)
        current_key = str(current).lower()
        if current_key in scanned:
            continue
        scanned.add(current_key)
        for name in dependent_dlls(current, dumpbin, environment):
            source = runtime / name
            if not source.is_file():
                continue
            destination = executable.parent / name
            key = name.lower()
            if key not in copied:
                copy_file(source, destination)
                copied.add(key)
                queue.append(source)
    if "tkernel.dll" not in copied:
        fail("未能从EXE依赖闭包中解析出OCCT运行库")
    print(f"OCCT部署: {len(copied)} 个DLL")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--jobs", type=int, default=os.cpu_count() or 1)
    parser.add_argument("--qt-root", type=Path)
    parser.add_argument("--occt-root", type=Path)
    parser.add_argument("--toolchain-root", type=Path)
    parser.add_argument("--clean", action="store_true")
    arguments = parser.parse_args()
    if arguments.jobs < 1:
        fail("--jobs必须大于0")

    project_root = Path(__file__).resolve().parents[2]
    build_root = project_root / "build" / "64" / BUILD_TYPE.lower()
    if arguments.clean and build_root.exists():
        shutil.rmtree(build_root)

    environment = find_vs_environment()
    toolchain_root, manifest = load_toolchain(arguments.toolchain_root)
    cmake = find_program(
        "cmake.exe",
        [
            manifest_path(toolchain_root, manifest, "cmake"),
            Path(r"C:\Qt\Tools\CMake_64\bin\cmake.exe"),
        ],
        environment,
    )
    ninja = find_program(
        "ninja.exe",
        [
            manifest_path(toolchain_root, manifest, "ninja"),
            Path(r"C:\Qt\Tools\Ninja\ninja.exe"),
        ],
        environment,
    )
    dumpbin = find_program("dumpbin.exe", [], environment)
    qt_root = find_qt(arguments.qt_root, environment)
    occt_root = find_occt(
        arguments.occt_root, manifest_path(toolchain_root, manifest, "occt")
    )

    environment["PATH"] = os.pathsep.join(
        [
            str(qt_root / "bin"),
            str(
                occt_root
                / "win64"
                / "vc14"
                / ("bind" if BUILD_TYPE == "Debug" else "bin")
            ),
            str(ninja.parent),
            environment.get("PATH", ""),
        ]
    )
    configure = [
        str(cmake),
        "-S",
        str(project_root),
        "-B",
        str(build_root),
        "-G",
        "Ninja",
        f"-DCMAKE_MAKE_PROGRAM={ninja}",
        f"-DCMAKE_BUILD_TYPE={BUILD_TYPE}",
        f"-DCMAKE_PREFIX_PATH={qt_root}",
        f"-DSMARTGRAPHICS3D_OCCT_ROOT={occt_root}",
        "-DSMARTGRAPHICS3D_BUILD_TESTS=ON",
        "-DSMARTGRAPHICS3D_BUILD_BENCHMARKS=ON",
    ]
    run(configure, environment)
    run([str(cmake), "--build", str(build_root), "-j", str(arguments.jobs)], environment)
    run(
        [
            "ctest.exe",
            "--test-dir",
            str(build_root),
            "--output-on-failure",
        ],
        environment,
    )

    executable = build_root / "bin" / "smartGraphics3D.exe"
    if not executable.is_file() or pe_machine(executable) != MACHINE_AMD64:
        fail(f"未生成x64主程序: {executable}")
    deploy_qt(qt_root, executable, environment)
    deploy_occt(occt_root, executable, environment, dumpbin)
    print(f"完成: {executable}")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (RuntimeError, OSError, subprocess.SubprocessError, json.JSONDecodeError) as error:
        print(f"错误: {error}", file=sys.stderr)
        raise SystemExit(1)
