"""Engangsoppsett av en kildekodesnapshot; overskriver aldri eksisterende filer."""
import hashlib
import json
from pathlib import Path
import re
import shutil
import sys

ROOT = Path(__file__).resolve().parent.parent


def main():
    source = Path(sys.argv[1])
    target = ROOT / 'firmware'
    target.mkdir(exist_ok=True)
    files = list((source / 'src').glob('*.h')) + list((source / 'src').glob('*.cpp'))
    files += [source / 'platformio.ini']
    constants, manifest = {}, {'source_directory': str(source), 'snapshot_date': '2026-10-29', 'sha256': {}}
    for file in files:
        destination = target / file.name
        if destination.exists():
            raise FileExistsError(destination)
        shutil.copy2(file, destination)
        manifest['sha256'][file.name] = hashlib.sha256(file.read_bytes()).hexdigest()
        if file.name.endswith('conf.h'):
            for name, value in re.findall(r'const\s+\w+\s+(\w+)\s*=\s*([-+\d.]+)f?\s*;', file.read_text(encoding='utf-8-sig')):
                constants[name] = float(value) if '.' in value else int(value)
    (target / 'manifest.json').write_text(json.dumps(manifest, indent=2, ensure_ascii=False) + '\n', encoding='utf-8')
    configs = ROOT / 'configs'
    configs.mkdir(exist_ok=True)
    (configs / 'firmware_defaults.json').write_text(json.dumps(constants, indent=2) + '\n', encoding='utf-8')
    motor = dict(min_rpm=35.0, max_rpm=180.0, deadzone_pwm=40, acceleration_rpm_s=500.0,
                 deceleration_rpm_s=700.0, speed_scale=1.0, no_load_current_a=0.08,
                 stall_current_a=1.2, max_torque_nm=0.025)
    config = {
        'robot': dict(body_length_m=0.15, body_width_m=0.11, wheelbase_m=0.095,
                      wheel_diameter_m=0.032, mass_kg=0.35, yaw_inertia_kg_m2=0.001,
                      grip_coefficient=0.8, rolling_resistance=0.015),
        'sensors': dict(count=6, span_m=0.05, forward_m=0.085, lateral_m=0.0, mount_angle_deg=0.0,
                        footprint_width_m=0.004, footprint_length_m=0.004, samples_per_axis=3,
                        edge_softness_m=0.001, background_value=50, line_value=1000,
                        gain=1.0, bias=0.0, noise_std=0.0, dropout_probability=0.0),
        'left_motor': motor.copy(), 'right_motor': motor.copy(),
        'power': dict(voltage_v=6.0, nominal_voltage_v=6.0, minimum_voltage_v=3.0,
                      internal_resistance_ohm=0.25, capacity_ah=2.0, state_of_charge=1.0,
                      electronics_current_a=0.05, battery_discharge_enabled=False),
        'track': dict(straight_m=1.0, radius_m=0.3, line_width_m=0.015),
        'initial_pose': dict(x_m=-0.4, y_m=0.3, heading_deg=0.0),
        'simulation': dict(seed=42, physics_step_ms=2, frame_ms=20),
        'controller': {name: value for name, value in constants.items() if name not in ('SensorCount', 'CENTER')},
    }
    (configs / 'default.json').write_text(json.dumps(config, indent=2) + '\n', encoding='utf-8')
    config['controller'] = {}
    (ROOT / 'testbed' / 'config_template.json').write_text(json.dumps(config, indent=2) + '\n', encoding='utf-8')
    print(f'Snapshot: {len(files)} filer, {len(constants)} konstanter')


if __name__ == '__main__':
    main()
