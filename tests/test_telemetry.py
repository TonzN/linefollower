import struct
import unittest

import hardware_testbed
from telemetry import crc8, decode_sample


def packet(local_count=6, side_count=6, side_flags=3):
    snapshot = bytearray(64 + 4 * local_count)
    struct.pack_into("<I", snapshot, 4, 0xF1234567)
    struct.pack_into("<H", snapshot, 42, 1026)
    snapshot[46] = local_count
    snapshot[47] = ord("V")
    snapshot[51] = ord("T")
    struct.pack_into("<h", snapshot, 30, -120)

    body = bytearray([local_count, side_count]) + snapshot
    body += struct.pack("<HHHB", 65535, 7, 1200, side_flags)
    body += struct.pack(f"<{side_count}H", *range(side_count))
    body += bytes(range(side_count))
    body += bytes(range(local_count))
    return body + bytes([crc8(body)])


def line_from_packet(data):
    return b"S2:" + data.hex().encode("ascii") + b"\n"


class TelemetryTests(unittest.TestCase):
    def test_s3_single_array_12_bit_and_nonuniform_positions(self):
        count = 12
        snapshot = bytearray(64 + 4 * count)
        struct.pack_into("<H", snapshot, 42, 1024)
        struct.pack_into("<H", snapshot, 58, 4095)
        struct.pack_into("<H", snapshot, 64, 4095)
        snapshot[46] = count
        snapshot[47] = ord("?")
        snapshot[51] = ord("T")
        indices = bytes([0, 3, 6, 9, 10, 11, 12, 13, 14, 17, 21, 24])
        body = bytes([count]) + snapshot + indices
        packet_bytes = body + bytes([crc8(body)])
        line = b"S3:" + packet_bytes.hex().encode() + b"\n"
        sample = decode_sample(line)
        self.assertEqual(sample["raw"][0], 4095)
        self.assertEqual(sample["raw_maximum"], 4095)
        self.assertEqual(sample["sensor_indices"], list(indices))
        self.assertNotIn("side_count", sample)
        for index in range(len(packet_bytes)):
            damaged = bytearray(packet_bytes)
            damaged[index] ^= 1
            with self.assertRaises(ValueError):
                decode_sample(b"S3:" + damaged.hex().encode() + b"\n")

    def test_crc_reference_vector(self):
        self.assertEqual(crc8(b"123456789"), 0xF4)

    def test_unsigned_time_signed_pwm_and_real_zero(self):
        sample = decode_sample(line_from_packet(packet()))
        self.assertEqual(sample["time_us"], 0xF1234567)
        self.assertEqual(sample["left_pwm"], -120)
        self.assertEqual(sample["side_raw"][0], 0)
        self.assertIsNone(sample["voltage_mv"])
        self.assertEqual(sample["side_sequence"], 65535)

    def test_stale_values_are_missing_but_scan_metadata_is_kept(self):
        sample = decode_sample(line_from_packet(packet(side_flags=2)))
        self.assertEqual(sample["side_raw"], [None] * 6)
        self.assertEqual(sample["side_scan_us"], 1200)

    def test_maximum_supported_channel_counts(self):
        sample = decode_sample(line_from_packet(packet(local_count=31, side_count=25)))
        self.assertEqual(len(sample["raw"]), 31)
        self.assertEqual(len(sample["side_raw"]), 25)

    def test_corruption_of_any_byte_is_detected(self):
        original = packet()
        for index in range(len(original)):
            damaged = bytearray(original)
            damaged[index] ^= 1
            with self.assertRaises(ValueError):
                decode_sample(line_from_packet(damaged))

    def test_truncation_bad_hex_and_inconsistent_counts(self):
        original = packet()
        invalid_lines = [b"S2:xyz\n", b"S2:00", line_from_packet(original[:-1])]
        inconsistent = bytearray(original)
        inconsistent[2 + 46] = 5
        inconsistent[-1] = crc8(inconsistent[:-1])
        invalid_lines.append(line_from_packet(inconsistent))

        for line in invalid_lines:
            with self.assertRaises(ValueError):
                decode_sample(line)

    def test_serial_bridge_accepts_s2_and_preserves_the_public_sample_schema(self):
        class FakeSerial:
            def write(self, data):
                pass

            def flush(self):
                pass

            def readline(self):
                return line_from_packet(packet())

        sample = hardware_testbed.SerialBridge(FakeSerial()).poll()
        row = hardware_testbed.flatten(sample, "test", 0, 0, 0)
        self.assertEqual(row["side_raw_0"], 0)
        self.assertEqual(row["side_indices_5"], 5)


if __name__ == "__main__":
    unittest.main()
