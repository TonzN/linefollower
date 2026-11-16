"""Kobler kontrollsløyfen til sensorer og fysikk, med faste simulerte klokker."""
import random
from .controller import LineController, analyze_line
from .physics import RobotPhysics, motor_output
from .world import PillTrack, SensorArray


class Simulation:
    def __init__(self, config, controller_config):
        self.config, self.c = config, controller_config
        self.reset()

    def reset(self):
        self.robot = RobotPhysics(self.config)
        self.track = PillTrack(self.config['track'])
        self.sensors = SensorArray(self.config['sensors'], random.Random(self.config['simulation']['seed']))
        self.control = LineController()
        self.time_ms = self.next_control_ms = 0
        self.sensor_time_ms = 0
        self.values = self.sensors.read(self.robot.pose, self.track)
        self.line = analyze_line(self.values, self.c)
        self.outputs = ((0, 0), (0, 0))
        self.distance_m = 0.0
        self.max_line_distance_m = 0.0
        self.stopped_by_user = False

    def stop(self):
        self.control.stop()
        self.outputs = ((0, 0), (0, 0))
        self.stopped_by_user = True

    def advance(self, duration_ms):
        if type(duration_ms) is not int or duration_ms < 0:
            raise ValueError('duration_ms må være et ikke-negativt heltall')
        end = self.time_ms + duration_ms
        while self.time_ms < end:
            if self.time_ms >= self.next_control_ms:
                self.sensor_time_ms = self.time_ms
                self.values = self.sensors.read(self.robot.pose, self.track)
                self.line = analyze_line(self.values, self.c)
                self.control.update(self.line, self.time_ms, self.c)
                self.outputs = (motor_output(self.control.left, self.c.offsetB),
                                motor_output(self.control.right, self.c.offsetA))
                self.next_control_ms += self.c.CONTROL_PERIOD_MS
            dt_ms = min(self.config['simulation']['physics_step_ms'],
                        end - self.time_ms, self.next_control_ms - self.time_ms)
            self.robot.step(*self.outputs, dt_ms / 1000)
            self.time_ms += dt_ms
            self.distance_m += abs(self.robot.velocity) * dt_ms / 1000
            self.max_line_distance_m = max(self.max_line_distance_m, self.track.distance(self.robot.x, self.robot.y))
        return self.observation()

    def observation(self):
        """Data for visning/logg. Ingen reward/action-kontrakt på dette trinnet."""
        r = self.robot
        return dict(time_ms=self.time_ms, sensor_time_ms=self.sensor_time_ms, x_m=r.x, y_m=r.y, heading_rad=r.heading,
                    velocity_m_s=r.velocity, angular_velocity_rad_s=r.angular_velocity,
                    sensors=list(self.values), error=self.line.error if self.line.valid else None,
                    quality=self.line.quality, mode=self.control.mode(), search_side=self.control.searchSide,
                    left_command=self.control.left, right_command=self.control.right,
                    left_pwm=self.outputs[0][1], right_pwm=self.outputs[1][1],
                    left_rpm=r.left.rpm, right_rpm=r.right.rpm,
                    voltage_v=r.voltage, current_a=r.current, state_of_charge=r.charge,
                    slip_ratio=r.slip_ratio, distance_m=self.distance_m,
                    line_distance_m=self.track.distance(r.x, r.y),
                    max_line_distance_m=self.max_line_distance_m)
