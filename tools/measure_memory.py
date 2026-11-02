"""Compare firmware size using the same enabled hardware profile.

Temporary copies keep the robot's real pin configuration untouched.
The example pins are build fixtures, not a wiring recommendation.
"""

import argparse
import json
from pathlib import Path
import re
import shutil
import subprocess
import tempfile


ROOT = Path(__file__).resolve().parent.parent
BUILD = ROOT / "build"
PLATFORMIO = Path.home() / ".platformio/penv/Scripts/platformio.exe"
AVR_TOOLS = Path.home() / ".platformio/packages/toolchain-atmelavr/bin"


def command(arguments, directory):
    result = subprocess.run(
        [str(argument) for argument in arguments],
        cwd=directory,
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="replace",
    )
    if result.returncode:
        raise RuntimeError(result.stdout + result.stderr)
    return result.stdout


def copy_sources(destination, revision):
    if revision is None:
        for directory in ("src", "include"):
            shutil.copytree(ROOT / directory, destination / directory)
        shutil.copy2(ROOT / "platformio.ini", destination / "platformio.ini")
        return

    paths = command(
        [
            "git",
            "ls-tree",
            "-r",
            "--name-only",
            revision,
            "--",
            "src",
            "include",
            "platformio.ini",
        ],
        ROOT,
    )
    for name in paths.splitlines():
        target = destination / name
        target.parent.mkdir(parents=True, exist_ok=True)
        content = subprocess.check_output(
            ["git", "show", f"{revision}:{name}"], cwd=ROOT
        )
        target.write_bytes(content)


def enable_build_fixture(project):
    board_path = project / "include/board_config.h"
    board = board_path.read_text(encoding="utf-8")
    board = re.sub(r"SensorBackend::Unconfigured", "SensorBackend::QtrAnalog", board)
    board = re.sub(r"MotorsEnabled\s*=\s*false", "MotorsEnabled = true", board)
    board = re.sub(r"LeftMotor\s*\{[^}]+\}", "LeftMotor{2,4,3,false}", board)
    board = re.sub(r"RightMotor\s*\{[^}]+\}", "RightMotor{7,8,5,false}", board)
    board = re.sub(
        r"constexpr int8_t SensorPins\[\]\s*=\s*\{.*?\};",
        "constexpr int8_t SensorPins[] = {14,15,16,17,20,21,"
        + ",".join(["-1"] * 25)
        + "};",
        board,
        flags=re.DOTALL,
    )
    board_path.write_text(board, encoding="utf-8")

    side_path = project / "include/side_config.h"
    side = side_path.read_text(encoding="utf-8")
    side = side.replace(
        "#define LINEFOLLOW_SIDE_ENABLED 0", "#define LINEFOLLOW_SIDE_ENABLED 1"
    )
    indices = [index for index in range(25) if index not in range(10, 16)]
    inputs = ",\n".join(
        f"    {{{index},-1,{number // 16},{number % 16}}}"
        for number, index in enumerate(indices)
    )
    side = re.sub(
        r"constexpr Input Inputs\[\]\s*=\s*\{.*?\};",
        "constexpr Input Inputs[] = {\n" + inputs + "\n};",
        side,
        flags=re.DOTALL,
    )
    side = re.sub(
        r"MuxSignalPins\[\]\s*=\s*\{[^}]+\}", "MuxSignalPins[] = {14,15}", side
    )
    side = re.sub(
        r"MuxSelectPins\[\]\s*=\s*\{[^}]+\}", "MuxSelectPins[] = {2,3,4,5}", side
    )
    side_path.write_text(side, encoding="utf-8")


def measure(label, revision):
    with tempfile.TemporaryDirectory(prefix="memory_", dir=BUILD) as temporary:
        project = Path(temporary)
        if not project.resolve().is_relative_to(BUILD.resolve()):
            raise RuntimeError("Temporary build directory escaped build/")

        copy_sources(project, revision)
        enable_build_fixture(project)
        build_log = command(
            [PLATFORMIO, "run", "-e", "master_nano", "-e", "side_nano"], project
        )
        (BUILD / f"memory_{label}.log").write_text(build_log, encoding="utf-8")
        results = {}

        for environment in ("master_nano", "side_nano"):
            firmware = project / ".pio/build" / environment / "firmware.elf"
            sections = command([AVR_TOOLS / "avr-size.exe", "-A", firmware], project)
            sizes = {}
            for line in sections.splitlines():
                fields = line.split()
                if len(fields) >= 2 and fields[0] in (".text", ".data", ".bss"):
                    sizes[fields[0]] = int(fields[1])

            symbols = command([AVR_TOOLS / "avr-nm.exe", "-C", firmware], project)
            results[environment] = {
                "flash_bytes": sizes.get(".text", 0) + sizes.get(".data", 0),
                "static_ram_bytes": sizes.get(".data", 0) + sizes.get(".bss", 0),
                "printf_linked": "vfprintf" in symbols,
                "allocator_linked": " T malloc" in symbols,
            }

        return results


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--baseline", help="Git revision to compare against")
    args = parser.parse_args()
    BUILD.mkdir(exist_ok=True)
    report = {}

    if args.baseline:
        report["baseline"] = measure("baseline", args.baseline)
    report["current"] = measure("current", None)

    output = BUILD / "memory_report.json"
    output.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(report, indent=2))
    print(f"Saved: {output}")


if __name__ == "__main__":
    main()
