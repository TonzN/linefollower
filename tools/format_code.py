"""Apply or check the project's C++ and Python formatting rules."""

import argparse
from pathlib import Path
import shutil
import subprocess
import sys


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--check", action="store_true", help="Check without changing files"
    )
    args = parser.parse_args()

    root = Path(__file__).resolve().parent.parent
    formatter = shutil.which("clang-format")

    if formatter is None:
        suffix = ".exe" if sys.platform == "win32" else ""
        candidate = Path(sys.executable).parent / ("clang-format" + suffix)
        if candidate.exists():
            formatter = str(candidate)

    if formatter is None:
        parser.error(
            "Installer utviklerverktøyene: python -m pip install -r requirements-dev.txt"
        )

    cpp_files = []
    for directory in ("src", "include", "tests"):
        cpp_files.extend(
            str(path)
            for path in (root / directory).rglob("*")
            if path.suffix in (".cpp", ".h")
        )

    cpp_options = ["--dry-run", "--Werror"] if args.check else ["-i"]
    subprocess.run(
        [formatter, "--style=file", *cpp_options, *cpp_files], cwd=root, check=True
    )

    python_options = ["--check"] if args.check else []
    subprocess.run(
        [
            sys.executable,
            "-m",
            "black",
            *python_options,
            "hardware_testbed.py",
            "telemetry.py",
            "tools",
            "tests",
        ],
        cwd=root,
        check=True,
    )


if __name__ == "__main__":
    main()
