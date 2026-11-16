"""Build and verify the portable Windows game.

Tested on Windows x64 with Python 3.14 and PyInstaller 6.22.3. From the repo:
  python -m pip install PyInstaller==6.22.3
  python Tools/jam-player/package.py

Tools/fetch-models.ps1 -Only vulkan,cpu,e4b supplies the model and sidecars.
The app-local VC++ runtime is sourced from the installed Visual Studio redist.
"""

import argparse
import hashlib
from html.parser import HTMLParser
from importlib import metadata
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import zipfile


HERE = Path(__file__).resolve().parent
PROJECT = HERE.parents[1]
CASE = Path("Content/Llm/Cases/SpadeClub")
MODEL = Path("Models/gemma-4-E4B-it-Q4_K_M.gguf")
MODEL_SHA256 = "85a896a047553e842f25297ee5b031d64ff30147d9c4af17b1e4b394cd1fab87"
GAME = "Black Spade"
ARCHIVE = "Black Spade Windows.zip"


def copy_file(source, target):
    target.parent.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(source, target)


def vc_runtime():
    """Use the redistributable copies, which can be deployed alongside each sidecar."""
    if configured := os.environ.get("BLACK_SPADE_VC_CRT"):
        return Path(configured)
    locator = Path(os.environ["ProgramFiles(x86)"], "Microsoft Visual Studio/Installer/vswhere.exe")
    installs = subprocess.check_output([
        str(locator), "-products", "*", "-requires", "Microsoft.VisualStudio.Component.VC.Tools.x86.x64",
        "-property", "installationPath",
    ], encoding="mbcs")
    for install in installs.splitlines():
        vc = Path(install, "VC")
        version_file = vc / "Auxiliary/Build/Microsoft.VCRedistVersion.default.txt"
        if version_file.is_file():
            version = version_file.read_text(encoding="utf-8-sig").strip()
            dirs = sorted((vc / "Redist/MSVC" / version / "x64").glob("Microsoft.VC*.CRT"))
            if dirs:
                return dirs[-1]
    raise RuntimeError("Set BLACK_SPADE_VC_CRT to the Visual Studio x64 redistributable CRT folder.")


def freeze_player(work, dest):
    subprocess.run([
        sys.executable, "-m", "PyInstaller", "--noconfirm", "--onefile", "--windowed", "--noupx",
        "--name", GAME, "--icon", str(HERE / "black-spade.ico"),
        "--paths", str(PROJECT / "Tools/llm-harness"),
        "--paths", str(HERE), "--hidden-import", "engine", "--hidden-import", "local_model",
        "--distpath", str(dest), "--workpath", str(work / "freeze"), "--specpath", str(work),
        str(HERE / "player.py"),
    ], check=True)


def stage_game(dest):
    shutil.copytree(HERE / "static", dest / "static", ignore=shutil.ignore_patterns(
        "__pycache__", "*.pyc", "*.map", ".*"))
    static = dest / "static"
    class InterfaceAssets(HTMLParser):
        def handle_starttag(self, tag, attrs):
            for key, value in attrs:
                if key in ("src", "href") and value and not (static / value).is_file():
                    raise RuntimeError(f"Missing interface file: {value}")
    InterfaceAssets().feed((static / "index.html").read_text(encoding="utf-8"))
    manifest = PROJECT / CASE / "case.json"
    case = json.loads(manifest.read_text(encoding="utf-8"))
    for name in ["case.json", *case["suspects"]]:
        copy_file(PROJECT / CASE / name, dest / CASE / name)
    print("Copying and verifying the dialogue model...", flush=True)
    copy_file(PROJECT / MODEL, dest / MODEL)
    with (dest / MODEL).open("rb") as model_file:
        digest = hashlib.file_digest(model_file, "sha256").hexdigest()
    if digest != MODEL_SHA256:
        raise RuntimeError(f"Unexpected SHA-256 for {MODEL.name}. Run Tools/fetch-models.ps1 to restore it.")

    crt = vc_runtime()
    names = ["llama-server.exe", "llama-server-impl.dll", "llama-common.dll", "llama.dll",
             "mtmd.dll", "ggml.dll", "ggml-base.dll", "libomp.dll"]
    for backend in ("vulkan", "cpu"):
        directory = Path("ThirdParty/llama") / f"b12368-{backend}"
        source = PROJECT / directory
        target = dest / directory
        for name in names:
            copy_file(source / name, target / name)
        for cpu_backend in source.glob("ggml-cpu-*.dll"):
            copy_file(cpu_backend, target / cpu_backend.name)
        if not (target / "ggml-cpu-x64.dll").is_file():
            raise RuntimeError(f"CPU backend missing from {source}.")
        if backend == "vulkan":
            copy_file(source / "ggml-vulkan.dll", target / "ggml-vulkan.dll")
        for name in ("msvcp140.dll", "vcruntime140.dll", "vcruntime140_1.dll"):
            copy_file(crt / name, target / name)
        subprocess.run([str(target / "llama-server.exe"), "--version"],
                       cwd=target, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE, check=True)

    licenses = dest / "licenses"
    shutil.copytree(HERE / "licenses", licenses)
    copy_file(PROJECT / "ThirdParty/llama/b12368-cpu/LICENSE-LLVM-OpenMP", licenses / "LLVM-OpenMP.txt")
    copy_file(Path(sys.base_prefix, "LICENSE.txt"), licenses / "Python.txt")
    distro = metadata.distribution("PyInstaller")
    copying = next(p for p in distro.files if p.name == "COPYING.txt")
    copy_file(distro.locate_file(copying), licenses / "PyInstaller.txt")
    copy_file(HERE / "PLAY.txt", dest / "PLAY.txt")


def zip_game(dest, output):
    files = sorted(path for path in dest.rglob("*") if path.is_file())
    with zipfile.ZipFile(output, "w", allowZip64=True, compresslevel=3) as z:
        for path in files:
            # The quantized model is already compact. Storing it makes extraction quick.
            method = zipfile.ZIP_STORED if path.suffix == ".gguf" else zipfile.ZIP_DEFLATED
            z.write(path, path.relative_to(dest.parent).as_posix(), compress_type=method)
    with zipfile.ZipFile(output) as z:
        if len(z.infolist()) != len(files):
            raise RuntimeError("Archive file count changed.")
        if corrupt := z.testzip():
            raise RuntimeError(f"Archive CRC mismatch in {corrupt}.")
    print(f"Verified all {len(files)} ZIP members ({output.stat().st_size:,} bytes).", flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=PROJECT.parent / "Release")
    args = parser.parse_args()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    final = output / GAME
    if final.exists() and (not final.is_dir() or final.is_symlink() or final.is_junction()
                           or not (final / f"{GAME}.exe").is_file()):
        raise RuntimeError(f"Existing output is not a Black Spade build: {final}")
    with tempfile.TemporaryDirectory(prefix=".black-spade-build-", dir=output) as temporary:
        work = Path(temporary)
        staged = work / GAME
        staged.mkdir()
        freeze_player(work, staged)
        stage_game(staged)
        zip_game(staged, work / ARCHIVE)
        previous = work / "previous-build"
        if final.exists():
            final.rename(previous)
        try:
            staged.rename(final)
            (work / ARCHIVE).replace(output / ARCHIVE)
        except OSError:
            if final.exists():
                final.rename(staged)
            if previous.exists():
                previous.rename(final)
            raise
    print(f"Play: {final / (GAME + '.exe')}\nShare: {output / ARCHIVE}")


if __name__ == "__main__":
    main()
