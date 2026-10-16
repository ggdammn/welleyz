"""Build, test and package the Windows release without PowerShell."""

import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
from datetime import datetime, timezone
from zipfile import ZipFile, ZIP_DEFLATED


ROOT = Path(__file__).resolve().parent.parent
BUILD = ROOT / "build"
match = re.search(r"project\(Welleyz VERSION ([^)]+)\)",
                  (ROOT / "CMakeLists.txt").read_text(encoding="utf-8"))
if not match:
    raise SystemExit("Project version is missing from CMakeLists.txt")
VERSION = match.group(1)
NAME = f"welleyz-{VERSION}-windows-x64"
STAGE = ROOT / "local" / "distributions" / "release" / NAME
ARCHIVE = ROOT / "local" / "distributions" / "release" / f"{NAME}.zip"
UPDATE_URL = os.environ.get("WELLEYZ_UPDATE_URL", "")


def run(*args: str, env: dict[str, str] | None = None) -> None:
    subprocess.run(args, cwd=ROOT, env=env, check=True)


def copy(src: Path, dest: Path) -> None:
    dest.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(src, dest)


def find_iscc() -> Path:
    command = shutil.which("ISCC.exe")
    if command:
        return Path(command)
    for env in ("LOCALAPPDATA", "ProgramFiles(x86)", "ProgramFiles"):
        root = os.environ.get(env)
        if root:
            candidate = Path(root) / "Inno Setup 6" / "ISCC.exe"
            if env == "LOCALAPPDATA":
                candidate = Path(root) / "Programs" / "Inno Setup 6" / "ISCC.exe"
            if candidate.is_file():
                return candidate
    raise RuntimeError("Inno Setup 6 (ISCC.exe) is required to build an installer")


def main() -> None:
    run("cmake", "-S", str(ROOT), "-B", str(BUILD),
        f"-DWELLEYZ_UPDATE_URL={UPDATE_URL}")
    targets = ("Welleyz_VST3", "Welleyz_Standalone", "welleyz_uninstall",
               "welleyz_update", "ProcessorBehaviourTest", "EditorSmokeTest",
               "FxEngineTest", "PresetCompatibility", "StandaloneTransportTest")
    run("cmake", "--build", str(BUILD), "--config", "Release", "--target", *targets)
    for test in targets[-5:]:
        run(str(BUILD / f"{test}_artefacts" / "Release" / f"{test}.exe"))

    if STAGE.exists():
        shutil.rmtree(STAGE)
    STAGE.mkdir(parents=True)
    shutil.copytree(BUILD / "Welleyz_artefacts" / "Release" / "VST3" / "welleyz.vst3",
                    STAGE / "VST3" / "welleyz.vst3")
    copy(BUILD / "Welleyz_artefacts" / "Release" / "Standalone" / "welleyz.exe",
         STAGE / "Standalone" / "welleyz.exe")
    for name in ("welleyz-uninstall.exe", "welleyz-update.exe"):
        copy(BUILD / "Release" / name, STAGE / name)
    copy(ROOT / "src" / "assets" / "standalone-icon.ico", STAGE / "welleyz.ico")
    for name in ("Setup.bat", "README.md", "CHANGELOG.md", "THIRD-PARTY-NOTICES.md"):
        copy(ROOT / "packaging" / name, STAGE / name)
    copy(ROOT / "src" / "resources" / "Manual.md", STAGE / "Manual.md")
    for font in ("Syne", "Outfit", "Sarina"):
        copy(ROOT / "src" / "assets" / "fonts" / f"{font}-OFL.txt", STAGE / f"{font}-OFL.txt")

    binaries = ("VST3/welleyz.vst3/Contents/x86_64-win/welleyz.vst3",
                "Standalone/welleyz.exe", "welleyz-uninstall.exe", "welleyz-update.exe")
    manifest = {
        "product": "welleyz", "publisher": "shadowave", "version": VERSION,
        "architecture": "windows-x64", "packageDateUtc": datetime.now(timezone.utc).isoformat(),
        "updateEndpoint": UPDATE_URL, "updateNotificationsEnabled": bool(UPDATE_URL),
        "sha256": {name: hashlib.sha256((STAGE / name).read_bytes()).hexdigest()
                   for name in binaries},
    }
    (STAGE / "release.json").write_text(json.dumps(manifest, indent=2), encoding="utf-8")
    env = dict(os.environ, WELLEYZ_STAGE=str(STAGE), WELLEYZ_VERSION=VERSION)
    run(str(find_iscc()), str(ROOT / "packaging" / "Welleyz.iss"), env=env)
    setup = STAGE / f"welleyz-{VERSION}-setup.exe"
    if not setup.is_file():
        raise RuntimeError("The installer executable is missing")
    with ZipFile(ARCHIVE, "w", ZIP_DEFLATED) as archive:
        for path in STAGE.rglob("*"):
            if path.is_file():
                archive.write(path, path.relative_to(STAGE))
    with ZipFile(ARCHIVE) as archive:
        required = {*binaries, setup.name, "release.json", "README.md",
                    "Manual.md", "Setup.bat"}
        if not required.issubset(archive.namelist()) or archive.testzip() is not None:
            raise RuntimeError("The release archive is incomplete or corrupt")
    print(f"Release ready: {ARCHIVE}")


if __name__ == "__main__":
    try:
        main()
    except (OSError, RuntimeError, subprocess.CalledProcessError) as error:
        print(f"Release failed: {error}", file=sys.stderr)
        sys.exit(1)
