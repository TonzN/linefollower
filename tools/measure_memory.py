"""Build the three ESP32 profiles and save PlatformIO's memory totals.

No real pin configuration is changed and no firmware is uploaded.
Fixture environments are build examples, not an approved wiring plan.
"""

import json
import os
from pathlib import Path
import re
import shutil
import subprocess

ROOT = Path(__file__).resolve().parent.parent


def main():
    platformio = shutil.which("platformio")
    if platformio is None:
        platformio = str(Path.home() / ".platformio/penv/Scripts/platformio.exe")

    environment = os.environ.copy()
    environment["PYTHONIOENCODING"] = "utf-8"
    report = {}
    build_directory = ROOT / "build"
    build_directory.mkdir(exist_ok=True)

    for profile in ("esp32", "esp32_fixture", "esp32_mux_fixture"):
        result = subprocess.run(
            [platformio, "run", "-e", profile],
            cwd=ROOT,
            env=environment,
            capture_output=True,
            text=True,
            encoding="utf-8",
            errors="replace",
        )
        log = result.stdout + result.stderr
        (build_directory / f"memory_{profile}.log").write_text(log, encoding="utf-8")
        if result.returncode:
            raise RuntimeError(log)

        totals = {}
        for resource, used, capacity in re.findall(
            r"(RAM|Flash):[^\n]*used (\d+) bytes from (\d+) bytes", log
        ):
            used = int(used)
            capacity = int(capacity)
            totals[resource.lower()] = {
                "used_bytes": used,
                "capacity_bytes": capacity,
                "percent": round(100 * used / capacity, 2),
            }

        if len(totals) != 2:
            raise RuntimeError(f"Missing memory totals for {profile}")

        report[profile] = totals

    output = build_directory / "memory_report_esp32.json"
    output.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(report, indent=2))
    print(f"Saved: {output}")


if __name__ == "__main__":
    main()
