import argparse
import csv
import json
from pathlib import Path
from .config import load_config
from .simulation import Simulation


def main():
    parser = argparse.ArgumentParser(description='LinjeFølger – emulator og enkel pillebane')
    parser.add_argument('--config', type=Path, help='Fullstendig JSON-profil (standard: configs/default.json)')
    parser.add_argument('--headless', action='store_true', help='Kjør uten vindu')
    parser.add_argument('--seconds', type=float, default=20, help='Varighet uten vindu')
    parser.add_argument('--csv', type=Path, help='Lagre telemetri som CSV')
    args = parser.parse_args()
    try:
        config, c = load_config(args.config)
        if not args.headless:
            from .viewer import launch
            launch(args.config)
            return
        import math
        if not math.isfinite(args.seconds) or args.seconds <= 0:
            parser.error('--seconds må være et positivt, endelig tall')
        sim = Simulation(config, c)
        file = None
        try:
            writer = None
            if args.csv:
                args.csv.parent.mkdir(parents=True, exist_ok=True)
                file = args.csv.open('w', newline='', encoding='utf-8')
                writer = csv.DictWriter(file, fieldnames=sim.observation().keys())
                writer.writeheader()
            end = round(args.seconds * 1000)
            while sim.time_ms < end:
                observation = sim.advance(min(c.CONTROL_PERIOD_MS, end - sim.time_ms))
                if writer:
                    writer.writerow(observation)
            print(json.dumps(sim.observation(), indent=2, ensure_ascii=False))
        finally:
            if file:
                file.close()
    except (ValueError, OSError) as exc:
        parser.exit(2, f'Feil: {exc}\n')


if __name__ == '__main__':
    main()
