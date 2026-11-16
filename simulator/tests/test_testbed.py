import copy
import json
import math
from pathlib import Path
import random
import tempfile
import unittest

from testbed.config import ROOT, load_config
from testbed.controller import LineController, LineReading, analyze_line, elapsed, ramp_motor
from testbed.physics import Motor, RobotPhysics, motor_output
from testbed.simulation import Simulation
from testbed.world import PillTrack, SensorArray


class TestbedTests(unittest.TestCase):
    def setUp(self):
        self.config, self.c = load_config(ROOT / 'testbed/config_template.json')

    def load_changed(self, config):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'profile.json'
            path.write_text(json.dumps(config), encoding='utf-8')
            return load_config(path)

    def test_default_matches_all_source_constants(self):
        original = json.loads((ROOT / 'configs/firmware_defaults.json').read_text(encoding='utf-8'))
        self.assertEqual(vars(self.c), original)

    def test_sensor_classification_and_integer_division(self):
        reading = analyze_line([100, 100, 900, 700, 100, 100], self.c)
        self.assertEqual(reading.error, -111)
        self.assertTrue(reading.valid)
        cases = [('?', [50] * 6), ('F', [900] * 6), ('B', [50, 950, 950, 950, 950, 50]),
                 ('X', [950, 50, 50, 50, 50, 950]), ('L', [50, 50, 720, 720, 50, 50])]
        for quality, values in cases:
            with self.subTest(quality=quality):
                self.assertEqual(analyze_line(values, self.c).quality, quality)

    def test_configurable_sensor_geometry(self):
        for count in (2, 8, 32):
            data = copy.deepcopy(self.config)
            data['sensors']['count'] = count
            data['sensors']['span_m'] = 0.12
            _, c = self.load_changed(data)
            self.assertEqual(c.CENTER, (count - 1) * 500)
            positions = list(SensorArray(data['sensors'], random.Random(1)).local_positions())
            self.assertAlmostEqual(positions[0][1] - positions[-1][1], 0.12)
            self.assertEqual(len(analyze_values := [50] * count), c.SensorCount)
            self.assertFalse(analyze_line(analyze_values, c).valid)

    def test_config_rejects_typos_nan_and_impossible_values(self):
        profiles = []
        for section, name, value in [('sensors', 'count', 0), ('robot', 'mass_kg', -1),
                                     ('power', 'voltage_v', float('nan')),
                                     ('left_motor', 'acceleration_rpm_s', 0),
                                     ('controller', 'MOTOR_ACCEL', -1),
                                     ('controller', 'CONTROL_PERIOD_MS', 0),
                                     ('controller', 'TYPO', 1),
                                     ('controller', 'SensorCount', 8)]:
            data = copy.deepcopy(self.config)
            data[section][name] = value
            profiles.append(data)
        data = copy.deepcopy(self.config)
        data['robot']['mas_kg'] = data['robot'].pop('mass_kg')
        profiles.append(data)
        for data in profiles:
            with self.subTest(data=data):
                with self.assertRaises(ValueError):
                    self.load_changed(data)

    def test_weak_signal_cannot_start_or_reacquire(self):
        control = LineController()
        weak = analyze_line([50, 50, 720, 720, 50, 50], self.c)
        for now in range(0, 100, 8):
            control.update(weak, now, self.c)
        self.assertTrue(control.recovering)
        self.assertFalse(control.candidate)

    def test_loss_recovery_and_return_to_following(self):
        control = LineController()
        center = LineReading(error=0, peak=950, valid=True, strong=True, quality='V')
        for now in range(0, 400, 8):
            control.update(center, now, self.c)
        for now in range(400, 640, 8):
            control.update(LineReading(), now, self.c)
        self.assertTrue(control.recovering)
        modes = set()
        for now in range(640, 1600, 8):
            control.update(center, now, self.c)
            modes.add(control.mode())
        self.assertIn('A', modes)
        self.assertEqual(control.mode(), 'T')

    def test_uint32_wrap(self):
        self.assertEqual(elapsed(8, 0xFFFFFFF8), 16)
        control = LineController()
        line = LineReading(valid=True, strong=True, quality='V')
        control.update(line, 0xFFFFFFF8, self.c)
        control.update(line, 8, self.c)
        self.assertGreater(control.left, 0)
        self.assertLess(control.left, 50)

    def test_reversal_goes_via_zero(self):
        value = 100
        seen_zero = False
        for _ in range(60):
            value = ramp_motor(value, -100, 0.008, self.c)
            if value == 0:
                seen_zero = True
            if value < 0:
                self.assertTrue(seen_zero)
                break
        self.assertLess(value, 0)

    def test_motor_pin_semantics_and_offset(self):
        self.assertEqual(motor_output(100.9, 10), (1, 110))
        self.assertEqual(motor_output(-100.9, 10), (-1, 90))
        self.assertEqual(motor_output(0, 10), (0, 10))
        with self.assertRaises(ValueError):
            motor_output(250, 10)

    def test_track_straights_arcs_and_join(self):
        track = PillTrack(self.config['track'])
        for point in ((0, 0.3), (0.5, 0.3), (0.8, 0), (-0.8, 0)):
            self.assertAlmostEqual(track.distance(*point), 0)
        self.assertAlmostEqual(track.distance(0, 0), 0.3)

    def test_sensor_order_and_forward_offset(self):
        sensor = SensorArray(self.config['sensors'], random.Random(1))
        positions = sensor.positions((0, 0, 0))
        self.assertGreater(positions[0][1], positions[-1][1])
        self.assertAlmostEqual(positions[0][0], 0.085)
        # Linje under robotens venstre side skal gi negativ feil.
        values = sensor.read((0, 0.29, 0), PillTrack(self.config['track']))
        self.assertLess(analyze_line(values, self.c).error, 0)

    def test_lower_voltage_reduces_motor_rpm(self):
        motors = []
        for voltage in (3, 6):
            motor = Motor()
            for _ in range(1000):
                motor.update((1, 255), voltage, 6, self.config['left_motor'], 0.002)
            motors.append(motor.rpm)
        self.assertAlmostEqual(motors[0] * 2, motors[1])

    def test_grip_limits_lateral_acceleration(self):
        data = copy.deepcopy(self.config)
        data['robot']['grip_coefficient'] = 0.02
        robot = RobotPhysics(data)
        for _ in range(1000):
            robot.step((1, 255), (1, 80), 0.002)
        self.assertLessEqual(abs(robot.velocity * robot.angular_velocity), 0.02 * 9.81 + 1e-9)

    def test_no_grip_cannot_accelerate_from_rest(self):
        data = copy.deepcopy(self.config)
        data['robot']['grip_coefficient'] = 0
        robot = RobotPhysics(data)
        for _ in range(100):
            robot.step((1, 255), (1, 255), 0.002)
        self.assertEqual(robot.velocity, 0)
        self.assertGreater(robot.left.rpm, 0)

    def test_power_sag_and_discharge(self):
        data = copy.deepcopy(self.config)
        data['power']['battery_discharge_enabled'] = True
        data['power']['capacity_ah'] = 0.01
        robot = RobotPhysics(data)
        for _ in range(500):
            robot.step((1, 255), (1, 255), 0.002)
        self.assertLess(robot.voltage, data['power']['voltage_v'])
        self.assertLess(robot.charge, 1)

    def test_reset_repeats_seeded_noise_and_trajectory(self):
        data = copy.deepcopy(self.config)
        data['sensors']['noise_std'] = 10.0
        sim = Simulation(data, self.c)
        first = sim.advance(1000)
        sim.reset()
        self.assertEqual(sim.advance(1000), first)

    def test_control_timing_is_independent_of_frame_chunks(self):
        first, second = Simulation(self.config, self.c), Simulation(self.config, self.c)
        first.advance(1000)
        for _ in range(50):
            second.advance(20)
        self.assertEqual(first.observation(), second.observation())

    def test_stop_latches_and_motors_coast_down(self):
        sim = Simulation(self.config, self.c)
        sim.advance(1000)
        sim.stop()
        sim.advance(1000)
        self.assertEqual(sim.control.mode(), 'S')
        self.assertEqual(sim.outputs, ((0, 0), (0, 0)))
        self.assertEqual(sim.robot.left.rpm, 0)
        self.assertEqual(sim.robot.right.rpm, 0)

    def test_default_pill_run_stays_finite_and_exercises_turns(self):
        sim = Simulation(self.config, self.c)
        o = sim.advance(20000)
        self.assertTrue(all(math.isfinite(o[name]) for name in ('x_m', 'y_m', 'velocity_m_s', 'voltage_v')))
        self.assertGreater(o['distance_m'], 3)
        self.assertLess(o['max_line_distance_m'], 0.1)


if __name__ == '__main__':
    unittest.main()
