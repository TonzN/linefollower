"""Decode ESP32 telemetry on the PC, where memory is plentiful.

The ESP32 sends S3 lines: hexadecimal bytes with CRC8 and physical sensor
indices. Legacy S2 recordings from the Nano version remain readable.
"""

from __future__ import annotations

import struct


# The first 64 bytes have the same layout as the original I2C snapshot.
# Format letters: H=uint16, h=int16, I=uint32; '<' means little endian.
HEADER_FIELDS = (
    ("sample", 0, "I"),
    ("time_us", 4, "I"),
    ("sensor_us", 8, "H"),
    ("cycle_us", 10, "H"),
    ("period_us", 12, "H"),
    ("lateness_us", 14, "H"),
    ("deadline_misses", 16, "I"),
    ("skipped_cycles", 20, "I"),
    ("min_cycle_us", 24, "H"),
    ("max_cycle_us", 26, "H"),
    ("error", 28, "h"),
    ("left_pwm", 30, "h"),
    ("right_pwm", 32, "h"),
    ("voltage_mv", 34, "H"),
    ("current_ma", 36, "h"),
    ("left_rpm_x10", 38, "h"),
    ("right_rpm_x10", 40, "h"),
    ("flags", 42, "H"),
    ("mode", 44, "B"),
    ("fault", 45, "B"),
    ("sensor_count", 46, "B"),
    ("ack_sequence", 48, "H"),
    ("ack_result", 50, "B"),
    ("min_period_us", 52, "H"),
    ("max_period_us", 54, "H"),
    ("link_age_ms", 56, "H"),
    ("raw_maximum", 58, "H"),
    ("step", 60, "H"),
    ("encoder_age_ms", 62, "H"),
)


def crc8(data: bytes) -> int:
    """CRC8 with polynomial 0x07, identical to the firmware implementation."""
    checksum = 0

    for value in data:
        checksum ^= value

        for _ in range(8):
            if checksum & 0x80:
                checksum = ((checksum << 1) ^ 0x07) & 0xFF
            else:
                checksum = (checksum << 1) & 0xFF

    return checksum


def decode_sample(line: bytes) -> dict:
    """Validate a complete S2 line and return the usual named sample fields.

    Raises ValueError on corruption, truncation or inconsistent channel counts.
    Missing measurements become None; a measured zero remains zero.
    """
    if line.startswith(b"S3:"):
        return decode_esp32_sample(line)

    if not line.startswith(b"S2:") or not line.endswith(b"\n"):
        raise ValueError("Ufullstendig S2-rapport")

    packet = bytes.fromhex(line[3:].decode("ascii"))

    if len(packet) < 2:
        raise ValueError("S2-rapporten mangler sensorantall")

    local_count, side_count = packet[:2]

    if not 2 <= local_count <= 31 or not 1 <= side_count <= 25:
        raise ValueError("Ugyldig sensorantall i S2-rapport")

    expected_size = 74 + 5 * local_count + 3 * side_count

    if len(packet) != expected_size or crc8(packet[:-1]) != packet[-1]:
        raise ValueError("Feil lengde eller CRC i S2-rapport")

    snapshot_size = 64 + 4 * local_count
    snapshot = packet[2 : 2 + snapshot_size]
    sample = {"kind": "sample"}

    for name, offset, format_code in HEADER_FIELDS:
        sample[name] = struct.unpack_from("<" + format_code, snapshot, offset)[0]

    if sample["sensor_count"] != local_count:
        raise ValueError("Ulike sensorantall i samme S2-rapport")

    sample["quality"] = chr(snapshot[47])
    sample["control_mode"] = chr(snapshot[51])

    flags = sample["flags"]
    raw_valid = bool(flags & 1024)
    calibrated_valid = raw_valid and bool(flags & 2)

    if not calibrated_valid or sample["quality"] not in ("V", "L"):
        sample["error"] = None

    optional_measurements = (
        ("voltage_mv", 16),
        ("current_ma", 32),
        ("left_rpm_x10", 64),
        ("right_rpm_x10", 128),
    )

    for name, validity_bit in optional_measurements:
        if not flags & validity_bit:
            sample[name] = None

    raw = list(struct.unpack_from(f"<{local_count}H", snapshot, 64))
    calibrated = list(
        struct.unpack_from(f"<{local_count}H", snapshot, 64 + 2 * local_count)
    )
    sample["raw"] = raw if raw_valid else [None] * local_count
    sample["calibrated"] = calibrated if calibrated_valid else [None] * local_count

    side_offset = 2 + snapshot_size
    sequence, age_ms, scan_us, side_flags = struct.unpack_from(
        "<HHHB", packet, side_offset
    )
    side_fresh = bool(side_flags & 1)
    side_valid = bool(side_flags & 2)
    values_offset = side_offset + 7
    indices_offset = values_offset + 2 * side_count

    sample["side_sequence"] = sequence
    sample["side_age_ms"] = age_ms if age_ms != 65535 else None
    sample["side_scan_us"] = scan_us if side_valid else None
    sample["side_fresh"] = int(side_fresh)
    sample["side_count"] = side_count
    sample["side_raw"] = (
        list(struct.unpack_from(f"<{side_count}H", packet, values_offset))
        if side_fresh
        else [None] * side_count
    )
    sample["side_indices"] = list(packet[indices_offset : indices_offset + side_count])
    sample["main_indices"] = list(packet[indices_offset + side_count : -1])

    return sample


def decode_esp32_sample(line: bytes) -> dict:
    """S3: one local sensor array; no side controller or I2C fields."""
    if not line.endswith(b"\n"):
        raise ValueError("Ufullstendig S3-rapport")

    packet = bytes.fromhex(line[3:].decode("ascii"))
    if not packet or not 2 <= packet[0] <= 31:
        raise ValueError("Ugyldig sensorantall i S3-rapport")

    count = packet[0]
    if len(packet) != 66 + 5 * count or crc8(packet[:-1]) != packet[-1]:
        raise ValueError("Feil lengde eller CRC i S3-rapport")

    snapshot = packet[1 : 1 + 64 + 4 * count]
    sample = {"kind": "sample"}

    for name, offset, format_code in HEADER_FIELDS:
        sample[name] = struct.unpack_from("<" + format_code, snapshot, offset)[0]

    if sample["sensor_count"] != count:
        raise ValueError("Ulike sensorantall i S3-rapport")

    sample["quality"] = chr(snapshot[47])
    sample["control_mode"] = chr(snapshot[51])
    flags = sample["flags"]
    raw_valid = bool(flags & 1024)
    calibrated_valid = raw_valid and bool(flags & 2)

    if not calibrated_valid or sample["quality"] not in ("V", "L"):
        sample["error"] = None

    for name, bit in (
        ("voltage_mv", 16),
        ("current_ma", 32),
        ("left_rpm_x10", 64),
        ("right_rpm_x10", 128),
    ):
        if not flags & bit:
            sample[name] = None

    raw = list(struct.unpack_from(f"<{count}H", snapshot, 64))
    calibrated = list(struct.unpack_from(f"<{count}H", snapshot, 64 + 2 * count))
    sample["raw"] = raw if raw_valid else [None] * count
    sample["calibrated"] = calibrated if calibrated_valid else [None] * count
    sample["sensor_indices"] = list(packet[1 + len(snapshot) : -1])

    if any(
        left >= right
        for left, right in zip(sample["sensor_indices"], sample["sensor_indices"][1:])
    ):
        raise ValueError("Sensorindeksene er ikke sortert")

    return sample
