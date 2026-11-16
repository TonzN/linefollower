"""JSON-konfigurasjon. Fysiske enheter er SI; RPM og ms er merket i navnene."""
import json
import math
from pathlib import Path
from types import SimpleNamespace

ROOT = Path(__file__).resolve().parent.parent


def load_config(path=None):
    path = Path(path) if path else ROOT / 'configs' / 'default.json'
    data = json.loads(path.read_text(encoding='utf-8'))
    defaults = json.loads((ROOT / 'testbed' / 'config_template.json').read_text(encoding='utf-8'))
    # Fullstendige profiler, ikke stille fallback ved feilstavede feltnavn.
    def check(actual, expected, prefix=''):
        if not isinstance(actual, dict):
            raise ValueError(f'{prefix}: forventer objekt')
        if prefix == 'controller':
            return  # Denne seksjonen tillater et utvalg kjente firmwarekonstanter.
        if actual.keys() != expected.keys():
            raise ValueError(f'{prefix}: manglende/ukjente felt: {actual.keys() ^ expected.keys()}')
        for key, value in actual.items():
            name = f'{prefix}.{key}'.strip('.')
            template = expected[key]
            if isinstance(template, dict):
                check(value, template, name)
            elif isinstance(template, bool):
                if type(value) is not bool:
                    raise ValueError(f'{name}: forventer true/false')
            elif isinstance(template, (int, float)):
                if isinstance(value, bool) or not isinstance(value, (int, float)) or not math.isfinite(value):
                    raise ValueError(f'{name}: forventer endelig tall')
                if type(template) is int and type(value) is not int:
                    raise ValueError(f'{name}: forventer heltall')
    check(data, defaults)
    s, r, p, track, sim = (data[k] for k in ('sensors', 'robot', 'power', 'track', 'simulation'))
    def positive(section, keys):
        for key in keys.split():
            if section[key] <= 0:
                raise ValueError(f'{key} må være større enn null')
    positive(r, 'body_length_m body_width_m wheelbase_m wheel_diameter_m mass_kg yaw_inertia_kg_m2')
    positive(s, 'span_m footprint_width_m footprint_length_m edge_softness_m')
    positive(p, 'voltage_v nominal_voltage_v capacity_ah minimum_voltage_v')
    positive(track, 'straight_m radius_m line_width_m')
    positive(sim, 'physics_step_ms frame_ms')
    if not 2 <= s['count'] <= 32 or not 1 <= s['samples_per_axis'] <= 15:
        raise ValueError('Sensorantall må være 2–32; samples_per_axis må være 1–15')
    if not 0 <= s['background_value'] < s['line_value'] <= 1000 or s['noise_std'] < 0:
        raise ValueError('Sensorverdier må være 0–1000, bakgrunn < linje og støy >= 0')
    if s['gain'] <= 0 or not 0 <= s['dropout_probability'] <= 1:
        raise ValueError('Gain må være positiv, dropout_probability 0–1')
    if r['grip_coefficient'] < 0 or r['rolling_resistance'] < 0 or p['internal_resistance_ohm'] < 0:
        raise ValueError('Grep, rullemotstand og indre motstand må være >= 0')
    if p['minimum_voltage_v'] > p['voltage_v'] or not 0 <= p['state_of_charge'] <= 1:
        raise ValueError('Ugyldig minimumsspenning eller state_of_charge')
    if p['electronics_current_a'] < 0:
        raise ValueError('electronics_current_a < 0')
    for side in ('left_motor', 'right_motor'):
        m = data[side]
        positive(m, 'max_rpm acceleration_rpm_s deceleration_rpm_s stall_current_a max_torque_nm')
        if not 0 <= m['min_rpm'] <= m['max_rpm'] or not 0 <= m['deadzone_pwm'] < 255:
            raise ValueError(f'{side}: ugyldig RPM/deadzone')
        if not 0 <= m['no_load_current_a'] <= m['stall_current_a'] or m['speed_scale'] <= 0:
            raise ValueError(f'{side}: ugyldig strøm eller speed_scale')
    firmware = json.loads((ROOT / 'configs' / 'firmware_defaults.json').read_text(encoding='utf-8'))
    for name, value in data['controller'].items():
        if name == 'SensorCount':
            raise ValueError('Bruk sensors.count; SensorCount avledes automatisk')
        if name not in firmware:
            raise ValueError(f'Ukjent firmwarekonstant: {name}')
        if isinstance(value, bool) or not isinstance(value, (int, float)) or not math.isfinite(value):
            raise ValueError(f'{name}: forventer endelig tall')
        if type(firmware[name]) is int and type(value) is not int:
            raise ValueError(f'{name}: forventer heltall')
        firmware[name] = value
    # Geometrisk antall har én kilde. CENTER kan fortsatt overstyres eksplisitt.
    firmware['SensorCount'] = s['count']
    firmware['CENTER'] = data['controller'].get('CENTER', (s['count'] - 1) * 500)
    c = SimpleNamespace(**firmware)
    if not 0 <= c.POSITION_FLOOR < c.SENSOR_FLOOR < c.TRACK_MIN_PEAK < c.LINE_MIN_PEAK <= 1000:
        raise ValueError('Ugyldig rekkefølge på sensorterskler')
    if not 0 < c.CORE_PERCENT <= 100 or c.GROUP_DOMINANCE <= 1:
        raise ValueError('Ugyldig sensorgruppe-konfigurasjon')
    if not 1 <= c.LINE_MAX_WIDTH <= 32 or c.SENSOR_DIRECTION not in (-1, 1):
        raise ValueError('Ugyldig LINE_MAX_WIDTH eller SENSOR_DIRECTION')
    if not 0 <= c.CORNER_SPEED <= c.BASE_SPEED <= 255 or not 0 < c.MOTOR_LIMIT <= 255:
        raise ValueError('Ugyldig grunnfart eller MOTOR_LIMIT')
    if c.CONTROL_PERIOD_MS <= 0 or c.MAX_RAMP_DT <= 0 or c.D_FILTER_SECONDS < 0:
        raise ValueError('Ugyldig kontrollperiode/filter')
    if c.UNKNOWN_SEARCH_SIDE not in (-1, 1):
        raise ValueError('UNKNOWN_SEARCH_SIDE må være -1 eller 1')
    for name, value in firmware.items():
        if name.endswith('_MS') and not 0 <= value < 2**31:
            raise ValueError(f'{name}: forventer ms innenfor 0..2^31-1')
        if name not in ('SENSOR_DIRECTION', 'UNKNOWN_SEARCH_SIDE', 'offsetA', 'offsetB') and value < 0:
            raise ValueError(f'{name} kan ikke være negativ')
    for name in ('MOTOR_ACCEL', 'MOTOR_DECEL', 'RECOVERY_BRAKE_RATE', 'BASE_ACCEL', 'BASE_DECEL', 'RESTART_ACCEL'):
        if getattr(c, name) <= 0:
            raise ValueError(f'{name} må være positiv')
    if not 0 <= c.RECOVERY_MOVING_MIN_FACTOR <= 1 or not 0 <= c.RECOVERY_FAR_MOVING_FACTOR <= 1:
        raise ValueError('Recovery-faktorer må være 0–1')
    if sim['physics_step_ms'] > c.CONTROL_PERIOD_MS:
        raise ValueError('physics_step_ms må være <= kontrollperioden')
    return data, c
