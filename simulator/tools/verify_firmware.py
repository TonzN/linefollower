"""Sammenlign Python med kompilert snapshot av den virkelige C++-styringen.

Krever g++ på PATH. Ingen Arduino eller nettverk behøves. Arduino.h-stubben
dekker kun motorpinnene; linefollow.cpp/helper.cpp/motor.cpp er uendret snapshot.
"""
import dataclasses
import hashlib
import json
import math
from pathlib import Path
import random
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT))
from testbed.controller import LineController, analyze_line


def sequences():
    cases = []
    center = [50, 50, 950, 950, 50, 50]
    right = [50, 50, 50, 50, 950, 50]
    left = list(reversed(right))
    lost = [50] * 6
    # Oppstart, stabilitet, sidehint, glipper, blindt søk og synlig innretting.
    cases.append([(8, center)] * 80 + [(8, right)] * 12 + [(8, lost)] * 35
                 + [(8, right)] * 8 + [(8, center)] * 100)
    cases.append([(8, center)] * 50 + [(8, lost)] * 80 + [(8, left)] * 10
                 + [(8, lost)] * 5 + [(8, center)] * 100)
    for pattern in ([900] * 6, [50, 950, 950, 950, 950, 50], [950, 50, 50, 50, 50, 950]):
        cases.append([(8, center)] * 30 + [(8, pattern)] * 80)
    weak = [50, 50, 720, 720, 50, 50]
    cases.append([(8, center)] * 30 + [(8, weak)] * 25 + [(8, center)] * 40)
    cases.append([(8, lost)] * 5 + [(560000, lost)])
    # Grace-tid med sterk linje ved fristen.
    cases.append([(8, lost), (8, lost), (560000, center), (160, center)])
    # Gå rundt uint32-klokka. Rekkefølgen er nå tidsdifferanser, ikke absolutte tall.
    cases.append([(0xFFFFFF00, center)] + [(8, right)] * 100 + [(8, lost)] * 20 + [(8, center)] * 80)
    rng = random.Random(1234)
    patterns = [center, right, left, lost, weak, [900] * 6, [950, 50, 50, 50, 50, 950]]
    for _ in range(120):
        seq = []
        for _ in range(12):
            pattern = rng.choice(patterns)
            seq += [(rng.choice([4, 8, 12, 20, 35]), pattern)] * rng.randint(1, 20)
        cases.append(seq)
    # Mange posisjoner/styrkegrenser for sensoranalysen.
    sweep = []
    for _ in range(5000):
        position, amplitude = rng.uniform(-1, 6), rng.uniform(600, 1000)
        values = [int(min(1000, 50 + amplitude * math.exp(-((i-position)/0.65)**2))) for i in range(6)]
        sweep.append((8, values))
    cases.append(sweep)
    return cases


def main():
    compiler = shutil.which('g++')
    if not compiler:
        raise SystemExit('g++ mangler på PATH. Vanlige Python-tester krever ikke kompilator.')
    manifest = json.loads((ROOT / 'firmware' / 'manifest.json').read_text(encoding='utf-8'))
    for name, expected in manifest['sha256'].items():
        if hashlib.sha256((ROOT / 'firmware' / name).read_bytes()).hexdigest() != expected:
            raise SystemExit(f'Snapshot er endret: {name}. Ikke en gyldig referansetest.')
    build = ROOT / 'build'
    build.mkdir(exist_ok=True)
    (build / 'Arduino.h').write_text(
        '#pragma once\n#include <cstdlib>\nusing std::abs;\n'
        '#define HIGH 1\n#define LOW 0\n'
        'inline void digitalWrite(int,int) {}\ninline void analogWrite(int,int) {}\n', encoding='utf-8')
    fields = [field.name for field in dataclasses.fields(LineController)]
    float_fields = {field.name for field in dataclasses.fields(LineController) if field.type is float}
    fields_stream = ''.join(f' << " " << +control.{name}' for name in fields)
    harness = '''#include <iostream>
#include <iomanip>
#include <cstdint>
#include "linefollow.h"
int main() {
  LineController control;
  unsigned int values[6];
  uint64_t timestamp;
  int reset;
  std::cout << std::setprecision(12);
  while (std::cin >> reset >> timestamp) {
    for (int i=0; i<6; ++i) std::cin >> values[i];
    if (reset) control = LineController{};
    auto reading = analyzeLine(values);
    control.update(reading, (uint32_t)timestamp);
    std::cout << reading.error << " " << reading.peak << " " << reading.valid
              << " " << reading.strong << " " << reading.quality << " " << control.mode()
              FIELD_STREAM << "\\n";
  }
}
'''.replace('FIELD_STREAM', fields_stream)
    (build / 'oracle.cpp').write_text(harness, encoding='utf-8')
    executable = build / 'oracle.exe'
    command = [compiler, '-std=c++17', '-O0', '-static-libgcc', '-static-libstdc++',
               '-I', str(build), '-I', str(ROOT / 'firmware'), str(build / 'oracle.cpp'),
               *[str(ROOT / 'firmware' / name) for name in ('linefollow.cpp', 'helper.cpp', 'motor.cpp')],
               '-o', str(executable)]
    subprocess.run(command, check=True, capture_output=True, text=True)
    # Denne referansen gjelder opprinnelige standarder, uavhengig av redigert default.json.
    from types import SimpleNamespace
    c = SimpleNamespace(**json.loads((ROOT / 'configs' / 'firmware_defaults.json').read_text(encoding='utf-8')))
    inputs, expected = [], []
    for case in sequences():
        control, now = LineController(), 0
        for index, (delta, values) in enumerate(case):
            now = (now + delta) & 0xFFFFFFFF
            inputs.append(f'{int(index == 0)} {now} ' + ' '.join(map(str, values)))
            reading = analyze_line(values, c)
            control.update(reading, now, c)
            expected.append((dataclasses.asdict(reading), control.mode(), dataclasses.asdict(control)))
    result = subprocess.run([str(executable)], input='\n'.join(inputs)+'\n', capture_output=True, text=True, check=True)
    lines = result.stdout.splitlines()
    if len(lines) != len(expected):
        raise AssertionError(f'C++ returnerte {len(lines)} linjer, forventet {len(expected)}')
    max_errors, modes = {}, set()
    for index, (line, (reading, mode, state)) in enumerate(zip(lines, expected)):
        tokens = line.split()
        actual_reading = dict(error=int(tokens[0]), peak=int(tokens[1]), valid=bool(int(tokens[2])), strong=bool(int(tokens[3])), quality=tokens[4])
        if reading != actual_reading or mode != tokens[5]:
            raise AssertionError(f'Rad {index}: måling/modus avviker: {reading}/{mode} mot {actual_reading}/{tokens[5]}')
        modes.add(mode)
        for name, text_value in zip(fields, tokens[6:]):
            actual, wanted = float(text_value), state[name]
            if name not in float_fields:
                if actual != wanted:
                    raise AssertionError(f'Rad {index}, {name}: C++={actual} Python={wanted}')
            else:
                error = abs(actual - wanted)
                max_errors[name] = max(max_errors.get(name, 0), error)
                # Double vs float32: tak for PWM 0,02 og rate 0,5 enheter/s.
                tolerance = 0.5 if name in ('derivative', 'captureRate') else 0.02
                if error > tolerance:
                    raise AssertionError(f'Rad {index}, {name}: C++={actual} Python={wanted}, differanse={error}')
    required = set('TGUBRAS')
    if not required <= modes:
        raise AssertionError(f'Mangler dekning av moduser: {required - modes}')
    report = dict(rows=len(expected), modes=sorted(modes), state_fields=len(fields),
                  max_absolute_error=max_errors, snapshot_verified=True,
                  scope='Host C++ mot Python; standardkonfig, seks sensorer. Ikke AVR bitnøyaktighet eller fysikkvalidering.')
    (build / 'parity_report.json').write_text(json.dumps(report, indent=2, ensure_ascii=False)+'\n', encoding='utf-8')
    print(json.dumps(report, indent=2, ensure_ascii=False))


if __name__ == '__main__':
    main()
