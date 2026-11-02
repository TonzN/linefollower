"""Fysisk testbenk: USB til ESP32, med støtte for eldre Nano-protokoll.

Eksempel: python hardware_testbed.py --port COM5 --test sensors --seconds 5
Dette er målinger på maskinvare, ikke Python-simulatoren fra forrige prosjekt.
"""

from __future__ import annotations
import argparse
import csv
from datetime import datetime, timezone
import json
from pathlib import Path
import statistics
import time

from telemetry import decode_sample

ROOT = Path(__file__).resolve().parent
RESULTS = {
    1: "Ukjent kommando",
    2: "Ugyldig argument eller testgrense",
    3: "Maskinvare/pinner er ikke konfigurert",
    4: "Motorene er ikke armert",
    5: "Sensorene må kalibreres",
    6: "Kortet er opptatt",
    7: "Feil er låst; kjør CLEAR",
}
MODE_NAMES = {
    0: "idle",
    1: "calibrating",
    2: "sensors",
    3: "timing",
    4: "motor",
    5: "sweep",
    6: "follow",
}
FAULT_NAMES = {
    0: "none",
    1: "cycle_budget",
    2: "late_start",
    3: "link_lost",
    4: "sensor_read",
    5: "configuration",
    6: "watchdog",
}
MOTION_TESTS = {"motor", "sweep", "follow"}


class BoardError(RuntimeError):
    pass


class SerialBridge:
    def __init__(self, serial_port, timeout=2.0):
        self.serial = serial_port
        self.timeout = timeout

    def request(self, command, kind):
        self.serial.write((command + "\n").encode("ascii"))
        self.serial.flush()
        deadline = time.monotonic() + self.timeout
        while time.monotonic() < deadline:
            line = self.serial.readline()
            if not line:
                continue
            try:
                if line.startswith((b"S2:", b"S3:")):
                    message = decode_sample(line)
                else:
                    message = json.loads(line.decode("utf-8"))
            except (UnicodeDecodeError, ValueError) as exc:
                raise BoardError("Ugyldig eller trunkert svar fra kortet") from exc
            if not isinstance(message, dict):
                raise BoardError("Forventet en strukturert rapport fra kortet")
            if message.get("kind") == "error":
                raise BoardError(message.get("message", "Feil fra kortet"))
            if message.get("kind") == kind:
                return message
        raise BoardError(f"Tidsavbrudd ved {command}")

    def poll(self):
        sample = self.request("POLL", "sample")
        count = sample.get("sensor_count", 0)
        if not isinstance(count, int) or not 2 <= count <= 31:
            raise BoardError("Ugyldig sensorantall i telemetri")
        if (
            len(sample.get("raw", [])) != count
            or len(sample.get("calibrated", [])) != count
        ):
            raise BoardError("Sensorlisten er ufullstendig")
        if "sensor_indices" in sample and len(sample["sensor_indices"]) != count:
            raise BoardError("Ufullstendig liste med sensorindekser")
        if "side_count" in sample:
            side_count = sample["side_count"]
            if not isinstance(side_count, int) or not 1 <= side_count <= 25:
                raise BoardError("Ugyldig antall ytterlesere")
            if (
                len(sample.get("side_raw", [])) != side_count
                or len(sample.get("side_indices", [])) != side_count
            ):
                raise BoardError("Listen med ytterlesere er ufullstendig")
            if len(sample.get("main_indices", [])) != count:
                raise BoardError("Posisjonslisten for midtleserne er ufullstendig")
        return sample

    def command(self, command):
        sent = self.request(command, "sent")
        sequence = sent["sequence"]
        deadline = time.monotonic() + self.timeout
        while time.monotonic() < deadline:
            sample = self.poll()
            if sample.get("ack_sequence") == sequence:
                result = sample["ack_result"]
                if result:
                    raise BoardError(RESULTS.get(result, f"Kortfeil {result}"))
                return sample
        raise BoardError(f"Kortet bekreftet ikke kommando {sequence}")


def milliseconds(seconds):
    import math

    if not math.isfinite(seconds) or seconds <= 0 or seconds * 1000 > 0x7FFFFFFF:
        raise ValueError("Varighet må være et positivt, endelig antall sekunder")
    return max(1, round(seconds * 1000))


def test_command(args):
    duration = milliseconds(args.seconds)
    if args.test in ("sensors", "calibrate", "timing", "follow"):
        return f"{args.test.upper()} {duration}", duration / 1000
    if args.test == "motor":
        if not -255 <= args.left <= 255 or not -255 <= args.right <= 255:
            raise ValueError(
                "PWM må være -255..255; kortet har i tillegg egen testgrense"
            )
        return f"MOTOR {args.left} {args.right} {duration}", duration / 1000
    if args.test == "sweep":
        if (
            not all(-255 <= x <= 255 for x in (args.start, args.end, args.step))
            or not args.step
        ):
            raise ValueError(
                "Start/slutt/steg må være -255..255, og steg kan ikke være null"
            )
        if (args.end - args.start) * args.step < 0:
            raise ValueError("Steget peker bort fra sluttverdien")
        if not 1 <= args.dwell_ms <= 30000:
            raise ValueError("dwell-ms må være 1..30000")
        distance = abs(args.end - args.start)
        points = (distance + abs(args.step) - 1) // abs(args.step) + 1
        side = {"left": 1, "right": 2, "both": 3}[args.side]
        return (
            f"SWEEP {side} {args.start} {args.end} {args.step} {args.dwell_ms}",
            points * args.dwell_ms / 1000,
        )
    raise ValueError("Ingen testkommando for info")


def flatten(sample, label, elapsed_s, received_s, gap):
    arrays = (
        "raw",
        "calibrated",
        "side_raw",
        "side_indices",
        "main_indices",
        "sensor_indices",
    )
    row = {key: value for key, value in sample.items() if key not in ("kind", *arrays)}
    row.update(
        label=label,
        nano_elapsed_s=elapsed_s,
        host_monotonic_s=received_s,
        skipped_between_polls=gap,
        mode_name=MODE_NAMES.get(sample["mode"], "unknown"),
        fault_name=FAULT_NAMES.get(sample["fault"], "unknown"),
    )
    for key, scale in [
        ("voltage_mv", 1000),
        ("current_ma", 1000),
        ("left_rpm_x10", 10),
        ("right_rpm_x10", 10),
    ]:
        output = {
            "voltage_mv": "voltage_v",
            "current_ma": "current_a",
            "left_rpm_x10": "left_rpm",
            "right_rpm_x10": "right_rpm",
        }[key]
        value = sample.get(key)
        row[output] = value / scale if value is not None else None
    for name in arrays:
        row.update(
            {f"{name}_{i}": value for i, value in enumerate(sample.get(name, []))}
        )
    return row


def run_test(bridge, args, output, *, clock=time.monotonic, sleep=time.sleep):
    if args.test in MOTION_TESTS and not args.confirm_motion:
        raise ValueError("Motorforsøk krever --confirm-motion")
    command, expected_seconds = test_command(args)
    interval = args.interval_ms / 1000
    if not 0.02 <= interval <= 0.5:
        raise ValueError(
            "interval-ms må være 20..500. Faktisk loggrate begrenses av USB/I2C."
        )
    output = Path(output)
    output.parent.mkdir(parents=True, exist_ok=True)
    if output.exists() or output.with_suffix(".json").exists():
        raise ValueError(
            "Målefilen finnes allerede. Velg et nytt navn; råopptak overskrives ikke."
        )
    samples = 0
    last_tick = last_us = None
    elapsed_s = 0.0
    observed = {}
    metadata = {
        "created_utc": datetime.now(timezone.utc).isoformat(),
        "test": args.test,
        "command": command,
        "label": args.label,
        "csv": str(output),
        "requested_interval_ms": args.interval_ms,
        "hardware_measured": True,
        "notes": [
            "PC-loggen er nedprøvd; den inneholder ikke alle kontrollrunder.",
            "Tomme måleverdier betyr manglende/ukonfigurert måleutstyr.",
            "RPM er magnitude fra enkeltkanals enkoder, ikke målt retning.",
        ],
    }
    try:
        bridge.command("STOP")
        bridge.command("CLEAR")
        if args.test in MOTION_TESTS:
            bridge.command("ARM")
        first = bridge.command(command)
        metadata["sensor_count"] = first["sensor_count"]
        deadline = clock() + expected_seconds + 3
        with output.open("x", newline="", encoding="utf-8") as file:
            writer = None
            sample = first
            while True:
                now = clock()
                if sample["fault"]:
                    raise BoardError(
                        "Kortet stoppet: "
                        + FAULT_NAMES.get(sample["fault"], str(sample["fault"]))
                    )
                if last_tick != sample["sample"]:
                    gap = (
                        0
                        if last_tick is None
                        else max(0, ((sample["sample"] - last_tick) & 0xFFFFFFFF) - 1)
                    )
                    if last_us is not None:
                        elapsed_s += ((sample["time_us"] - last_us) & 0xFFFFFFFF) / 1e6
                    row = flatten(sample, args.label, elapsed_s, now, gap)
                    if writer is None:
                        writer = csv.DictWriter(file, fieldnames=list(row))
                        writer.writeheader()
                    writer.writerow(row)
                    samples += 1
                    for key, value in row.items():
                        if (
                            key.startswith("raw_")
                            and key[4:].isdigit()
                            and isinstance(value, (int, float))
                        ):
                            observed.setdefault(key, []).append(value)
                    last_tick, last_us = sample["sample"], sample["time_us"]
                if sample["flags"] & 256 and sample["mode"] == 0:
                    if args.test == "calibrate" and not sample["flags"] & 2:
                        raise BoardError(
                            "Kalibreringen fikk for liten variasjon på minst én leser"
                        )
                    metadata["complete"] = True
                    break
                if clock() > deadline:
                    raise BoardError("Testen avsluttet ikke innen forventet tid")
                sleep(interval)
                sample = bridge.poll()
    except BaseException as exc:
        metadata["complete"] = False
        metadata["error"] = type(exc).__name__ + ": " + str(exc)
        raise
    finally:
        try:
            bridge.command("STOP")
            metadata["stop_acknowledged"] = True
        except Exception as exc:
            metadata["stop_acknowledged"] = False
            metadata["stop_error"] = str(exc)
        metadata["logged_samples"] = samples
        metadata["sensor_summary_of_logged_samples"] = {
            key: {
                "n": len(values),
                "mean": statistics.mean(values),
                "min": min(values),
                "max": max(values),
                "std": statistics.stdev(values) if len(values) > 1 else None,
            }
            for key, values in observed.items()
        }
        output.with_suffix(".json").write_text(
            json.dumps(metadata, indent=2, ensure_ascii=False) + "\n", encoding="utf-8"
        )
    return metadata


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", help="USB-porten til ESP32, for eksempel COM5")
    parser.add_argument("--list-ports", action="store_true")
    parser.add_argument(
        "--test",
        choices=["info", "sensors", "calibrate", "timing", "motor", "sweep", "follow"],
        default="info",
    )
    parser.add_argument("--seconds", type=float, default=5)
    parser.add_argument("--interval-ms", type=int, default=100)
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--left", type=int, default=0)
    parser.add_argument("--right", type=int, default=0)
    parser.add_argument("--side", choices=["left", "right", "both"], default="left")
    parser.add_argument("--start", type=int, default=0)
    parser.add_argument("--end", type=int, default=100)
    parser.add_argument("--step", type=int, default=5)
    parser.add_argument("--dwell-ms", type=int, default=500)
    parser.add_argument(
        "--label", default="", help="Underlag, lys, last og annen kontekst"
    )
    parser.add_argument("--csv", type=Path)
    parser.add_argument(
        "--confirm-motion",
        action="store_true",
        help="Tillat motorpådrag i dette forsøket",
    )
    args = parser.parse_args()
    if args.test in MOTION_TESTS and not args.confirm_motion:
        parser.error("Bruk --confirm-motion for motorforsøk")
    try:
        import serial
        from serial.tools import list_ports
    except ImportError:
        parser.exit(2, "Installer først: python -m pip install -r requirements.txt\n")
    if args.list_ports:
        for port in list_ports.comports():
            print(port.device, port.description)
        return
    if not args.port:
        parser.error("--port mangler; bruk --list-ports for å finne kortet")
    output = (
        args.csv or ROOT / "runs" / f"{datetime.now():%Y%m%d_%H%M%S_%f}_{args.test}.csv"
    )
    try:
        with serial.Serial(args.port, args.baud, timeout=0.2, write_timeout=2) as port:
            time.sleep(2)  # Arduino-kortet kan starte på nytt når USB-porten åpnes.
            port.reset_input_buffer()
            bridge = SerialBridge(port)
            if bridge.request("HELLO", "hello").get("protocol") not in (1, 2, 3):
                raise BoardError("Feil protokollversjon")
            if args.test == "info":
                print(json.dumps(bridge.poll(), indent=2))
            else:
                if args.test == "calibrate":
                    print(
                        "Før alle lesere over både linje og underlag under kalibreringen."
                    )
                result = run_test(bridge, args, output)
                print(f"Lagret {result['logged_samples']} prøver: {output}")
    except KeyboardInterrupt:
        parser.exit(130, "Avbrutt. Stopp er forsøkt sendt til kortet.\n")
    except (BoardError, ValueError, OSError, serial.SerialException) as exc:
        parser.exit(1, str(exc) + "\n")


if __name__ == "__main__":
    main()
