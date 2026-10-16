"""VS Code-friendly clang-format and clang-tidy checks for Windows/MSVC."""

import os
from pathlib import Path
import subprocess
import sys
import tempfile


ROOT = Path(__file__).resolve().parent.parent
SOURCE_FILES = sorted([*ROOT.glob("src/*.cpp"), *ROOT.glob("src/*.h"),
                       *ROOT.glob("tests/*.cpp"), *ROOT.glob("packaging/*.cpp")])


def vs_root() -> Path:
    installer = Path(os.environ.get("ProgramFiles(x86)", r"C:\Program Files (x86)"))
    vswhere = installer / "Microsoft Visual Studio" / "Installer" / "vswhere.exe"
    location = subprocess.check_output([str(vswhere), "-latest", "-property",
                                        "installationPath"], text=True).strip()
    if location:
        return Path(location)
    candidates = installer / "Microsoft Visual Studio" / "2022"
    for edition in ("BuildTools", "Community", "Professional", "Enterprise"):
        root = candidates / edition
        if (root / "Common7" / "Tools" / "VsDevCmd.bat").is_file():
            return root
    raise RuntimeError("Visual Studio C++ Build Tools are missing")


def llvm_tool(name: str) -> str:
    location = vs_root() / "VC" / "Tools" / "Llvm" / "x64" / "bin" / f"{name}.exe"
    if not location.is_file():
        raise RuntimeError(f"Install the Visual Studio LLVM tools: {name}")
    return str(location)


def developer_environment(root: Path) -> dict[str, str]:
    script = root / "Common7" / "Tools" / "VsDevCmd.bat"
    with tempfile.TemporaryDirectory(prefix="welleyz-vs-") as folder:
        wrapper = Path(folder) / "env.bat"
        wrapper.write_text(f'@echo off\ncall "{script}" -arch=x64 >nul\nif errorlevel 1 exit /b 1\nset\n',
                           encoding="ascii")
        output = subprocess.check_output(["cmd.exe", "/d", "/c", str(wrapper)], text=True)
    env = dict(os.environ)
    for line in output.splitlines():
        if "=" in line and not line.startswith("="):
            key, value = line.split("=", 1)
            env[key] = value
    return env


def main() -> None:
    mode = sys.argv[1] if len(sys.argv) > 1 else "check"
    if mode in ("format", "check"):
        args = [llvm_tool("clang-format")]
        args += ["-i"] if mode == "format" else ["--dry-run", "--Werror"]
        subprocess.run([*args, *(str(path) for path in SOURCE_FILES)], check=True, cwd=ROOT)
        return
    if mode == "test":
        for name in ("ProcessorBehaviourTest", "EditorSmokeTest", "FxEngineTest",
                     "PresetCompatibility", "StandaloneTransportTest"):
            subprocess.run([str(ROOT / "build" / f"{name}_artefacts" / "Release" /
                                f"{name}.exe")], check=True, cwd=ROOT)
        return
    if mode == "tidy":
        root = vs_root()
        ninja = root / "Common7" / "IDE" / "CommonExtensions" / "Microsoft" / "CMake" / "Ninja" / "ninja.exe"
        env = developer_environment(root)
        configure = ["cmake", "--preset", "windows-tidy", f"-DCMAKE_MAKE_PROGRAM={ninja}"]
        cached_juce = ROOT / "build" / "_deps" / "juce-src"
        if cached_juce.exists():
            configure.append(f"-DFETCHCONTENT_SOURCE_DIR_JUCE={cached_juce}")
        subprocess.run(configure, check=True, env=env, cwd=ROOT)
        subprocess.run(["cmake", "--build", str(ROOT / "build-tidy"), "--target",
                        "Welleyz_VST3", "EditorSmokeTest", "ProcessorBehaviourTest", "FxEngineTest",
                        "PresetCompatibility", "StandaloneTransportTest"], check=True, env=env, cwd=ROOT)
        subprocess.run([llvm_tool("clang-tidy"), "-p", str(ROOT / "build-tidy"),
                        *[str(path) for path in SOURCE_FILES if path.suffix == ".cpp"]],
                       check=True, env=env, cwd=ROOT)
        return
    raise SystemExit("Usage: python scripts/tools.py [format|check|tidy|test]")


if __name__ == "__main__":
    try:
        main()
    except (OSError, RuntimeError, subprocess.CalledProcessError) as error:
        raise SystemExit(f"C++ check failed: {error}") from error
