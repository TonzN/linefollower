"""Kjør tester uten tilkoblet maskinvare. Krever Python og g++ på PATH."""

from pathlib import Path
import shutil
import subprocess
import sys
import json

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
from telemetry import decode_sample

root = Path(__file__).resolve().parent.parent
compiler = shutil.which("g++")
if not compiler:
    raise SystemExit("g++ mangler på PATH; alternativt bruk CMake-testmålene i CLion.")
build = root / "build"
build.mkdir(exist_ok=True)
subprocess.run(
    [sys.executable, "-m", "unittest", "discover", "-s", "tests", "-v"],
    cwd=root,
    check=True,
)
common = [
    str(root / "src/control" / name)
    for name in ("linefollow.cpp", "helper.cpp", "ramps.cpp")
]
for source, name, fakes in [
    ("test_runtime.cpp", "runtime_tests", False),
    ("test_control_runtime.cpp", "control_runtime_tests", True),
    ("test_master.cpp", "master_tests", True),
    ("test_board_pins.cpp", "board_pins_tests", False),
    ("test_hardware.cpp", "hardware_tests", True),
    ("test_hardware.cpp", "hardware_mux_tests", True),
]:
    output = build / (name + ".exe" if sys.platform == "win32" else name)
    includes = ["-I", str(root / "include"), "-I", str(root / "include/control")]
    if fakes:
        includes = ["-I", str(root / "tests/fakes")] + includes
    sources = common
    if name in ("hardware_tests", "hardware_mux_tests"):
        includes += ["-DLINEFOLLOW_BUILD_FIXTURE"]
        if name == "hardware_mux_tests":
            includes += ["-DLINEFOLLOW_MUX_FIXTURE"]
        sources = []
    subprocess.run(
        [
            compiler,
            "-std=c++17",
            "-Wall",
            "-Wextra",
            *includes,
            str(root / "tests" / source),
            *sources,
            "-o",
            str(output),
        ],
        check=True,
    )
    result = subprocess.run([str(output)], check=True, capture_output=True, text=True)
    print(
        "\n".join(
            line
            for line in result.stdout.splitlines()
            if not line.startswith(("{", "S2:", "S3:"))
        )
    )
    if name == "master_tests":
        samples = [
            decode_sample((line + "\n").encode("ascii"))
            for line in result.stdout.splitlines()
            if line.startswith("S3:")
        ]
        assert len(samples) == 2 and samples[0]["time_us"] == 0xF1234567
        assert samples[0]["sensor_count"] == 12
        assert samples[1]["raw"][:2] == [0, 950]
        assert len(samples[1]["sensor_indices"]) == 12
        assert "side_count" not in samples[1]
