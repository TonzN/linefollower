"""Enkel pillebane og modell av kalibrerte refleksjonsverdier."""
import math
from .controller import limit


class PillTrack:
    def __init__(self, config):
        self.half_straight = config['straight_m'] / 2
        self.radius = config['radius_m']
        self.width = config['line_width_m']

    def distance(self, x, y):
        """Avstand til senterlinjen: avstand til kapselens indre segment minus R."""
        closest_x = limit(x, -self.half_straight, self.half_straight)
        return abs(math.hypot(x - closest_x, y) - self.radius)

    def points(self, n=120):
        # Høyre halvsirkel, nedre rettstrekning, venstre halvsirkel, øvre rettstrekning.
        points = []
        for center, start in ((self.half_straight, math.pi / 2), (-self.half_straight, -math.pi / 2)):
            for i in range(n + 1):
                a = start - math.pi * i / n
                points.append((center + self.radius * math.cos(a), self.radius * math.sin(a)))
        return points + points[:1]


class SensorArray:
    def __init__(self, config, rng):
        self.config, self.rng = config, rng

    def local_positions(self):
        c = self.config
        # Indeks 0 er til venstre sett i robotens kjøreretning.
        for i in range(c['count']):
            lateral = c['span_m'] * (0.5 - i / (c['count'] - 1))
            angle = math.radians(c['mount_angle_deg'])
            yield (c['forward_m'] - lateral * math.sin(angle),
                   c['lateral_m'] + lateral * math.cos(angle))

    @staticmethod
    def transform(x, y, pose):
        px, py, heading = pose
        return (px + x * math.cos(heading) - y * math.sin(heading),
                py + x * math.sin(heading) + y * math.cos(heading))

    def positions(self, pose):
        return [self.transform(x, y, pose) for x, y in self.local_positions()]

    def read(self, pose, track):
        c = self.config
        n = c['samples_per_axis']
        angle = math.radians(c['mount_angle_deg'])
        readings = []
        for x, y in self.local_positions():
            total = 0
            for i in range(n):
                for j in range(n):
                    dx = ((i + 0.5) / n - 0.5) * c['footprint_length_m']
                    dy = ((j + 0.5) / n - 0.5) * c['footprint_width_m']
                    wx, wy = self.transform(x + dx * math.cos(angle) - dy * math.sin(angle),
                                             y + dx * math.sin(angle) + dy * math.cos(angle), pose)
                    signed = (track.distance(wx, wy) - track.width / 2) / c['edge_softness_m']
                    coverage = 1 / (1 + math.exp(limit(signed, -60, 60)))
                    total += coverage
            value = c['background_value'] + total / (n * n) * (c['line_value'] - c['background_value'])
            value = c['gain'] * value + c['bias'] + self.rng.gauss(0, c['noise_std'])
            if self.rng.random() < c['dropout_probability']:
                value = c['background_value']
            readings.append(int(round(limit(value, 0, 1000))))
        return readings
