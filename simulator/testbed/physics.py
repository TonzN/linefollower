"""Forenklet differensialdrift. Parametrene må måles på roboten før trening.

RPM-respons -> hjulhastighet -> begrenset chassisrespons. Grep begrenser både
akselerasjon og sideakselerasjon. Ingen kontaktløser eller elektrisk motormodell.
"""
from dataclasses import dataclass
import math
from .controller import approach, limit


def motor_output(command, offset):
    """Emuler int-cast, retningspinner og abs(command+offset) i motor.cpp.

    Returnerer retning og PWM separat: ved kommando null er retningen 0 selv
    om et ikke-null offset gir PWM. Ugyldig analogWrite-område avvises eksplisitt.
    """
    value = int(command)
    pwm = abs(value + offset)
    if pwm > 255:
        raise ValueError('Motoroffset/kommando gir PWM > 255; AVR-overflyt modelleres ikke')
    return (1 if value > 0 else -1 if value < 0 else 0), pwm


@dataclass
class Motor:
    rpm: float = 0
    target_rpm: float = 0
    current_a: float = 0

    def update(self, output, voltage, nominal, config, dt):
        direction, pwm = output
        duty = pwm / 255
        fraction = max(0, (pwm - config['deadzone_pwm']) / (255 - config['deadzone_pwm']))
        scale = voltage / nominal * config['speed_scale']
        magnitude = (config['min_rpm'] + fraction * (config['max_rpm'] - config['min_rpm'])) if fraction > 0 and direction else 0
        self.target_rpm = direction * magnitude * scale
        # Fysisk revers går via null selv om kommandolaget skulle bytte fortegn straks.
        target = 0 if self.rpm * self.target_rpm < 0 else self.target_rpm
        rate = config['acceleration_rpm_s'] if abs(target) > abs(self.rpm) else config['deceleration_rpm_s']
        self.rpm = approach(self.rpm, target, rate * max(0, scale) * dt)
        mismatch = min(1, abs(self.target_rpm - self.rpm) / max(config['max_rpm'] * scale, 1e-9))
        self.current_a = duty * (config['no_load_current_a'] + mismatch * (config['stall_current_a'] - config['no_load_current_a'])) if direction else 0


class RobotPhysics:
    def __init__(self, config):
        self.config = config
        initial = config['initial_pose']
        self.x, self.y, self.heading = initial['x_m'], initial['y_m'], math.radians(initial['heading_deg'])
        self.left, self.right = Motor(), Motor()
        self.velocity = self.angular_velocity = 0.0
        self.charge = config['power']['state_of_charge']
        self.voltage = config['power']['voltage_v']
        self.current = 0.0
        self.slip_ratio = 0.0

    @property
    def pose(self):
        return self.x, self.y, self.heading

    def step(self, left_output, right_output, dt):
        r, p = self.config['robot'], self.config['power']
        # Én integrasjonstidsstegs forsinkelse i spenningsfallet, ikke algebraisk løser.
        unloaded = p['voltage_v']
        if p['battery_discharge_enabled']:
            unloaded *= 0.8 + 0.2 * self.charge
        self.voltage = max(p['minimum_voltage_v'], unloaded - self.current * p['internal_resistance_ohm'])
        for motor, output, name in ((self.left, left_output, 'left_motor'), (self.right, right_output, 'right_motor')):
            motor.update(output, self.voltage, p['nominal_voltage_v'], self.config[name], dt)
        self.current = self.left.current_a + self.right.current_a + p['electronics_current_a']
        if p['battery_discharge_enabled']:
            self.charge = max(0, self.charge - self.current * dt / (3600 * p['capacity_ah']))
        circumference = math.pi * r['wheel_diameter_m']
        vl, vr = self.left.rpm * circumference / 60, self.right.rpm * circumference / 60
        desired_v = (vl + vr) / 2
        desired_w = (vr - vl) / r['wheelbase_m']
        grip_accel = r['grip_coefficient'] * 9.81
        torque = sum(self.config[name]['max_torque_nm'] for name in ('left_motor', 'right_motor')) * self.voltage / p['nominal_voltage_v']
        max_force = torque / (r['wheel_diameter_m'] / 2)
        accel = min(grip_accel, max_force / r['mass_kg'])
        # Rullemotstand reduserer tilgjengelig akselerasjon og lar chassis rulle ut.
        if abs(desired_v) > abs(self.velocity):
            accel = max(0, accel - r['rolling_resistance'] * 9.81)
        self.velocity = approach(self.velocity, desired_v, accel * dt)
        yaw_accel = min(max_force, r['mass_kg'] * grip_accel) * r['wheelbase_m'] / (2 * r['yaw_inertia_kg_m2'])
        raw_w = approach(self.angular_velocity, desired_w, yaw_accel * dt)
        # |v*omega| <= mu*g: enkel tilnærming til at lavt grep gir for vide svinger.
        w_limit = grip_accel / max(abs(self.velocity), 1e-9)
        self.angular_velocity = limit(raw_w, -w_limit, w_limit)
        self.slip_ratio = min(1, abs(desired_w - self.angular_velocity) / max(abs(desired_w), 1e-9))
        mid_heading = self.heading + self.angular_velocity * dt / 2
        self.x += self.velocity * math.cos(mid_heading) * dt
        self.y += self.velocity * math.sin(mid_heading) * dt
        self.heading = (self.heading + self.angular_velocity * dt + math.pi) % (2 * math.pi) - math.pi
