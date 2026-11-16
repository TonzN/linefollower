"""Port av firmware/linefollow.cpp. Navn beholdes for enkel sammenligning.

Python bruker double, firmware bruker float. Dette er logikkemulering, ikke
syklusnøyaktig AVR-emulering. Alle tidsdifferanser følger uint32_t-semantikk.
"""
from dataclasses import dataclass, replace


def limit(value, low, high):
    return max(low, min(value, high))


def approach(value, target, step):
    return value + limit(target - value, -step, step)


def elapsed(now, before):
    return (now - before) & 0xFFFFFFFF


def ramp_motor(value, target, dt, c, capture=False):
    if value * target < 0:
        target = 0
    decel = c.RECOVERY_BRAKE_RATE if capture else c.MOTOR_DECEL
    rate = c.MOTOR_ACCEL if abs(target) > abs(value) else decel
    return approach(value, target, rate * dt)


def ramp_following(left, right, target_left, target_right, dt, c):
    fraction = 1.0
    for current, target in ((left, target_left), (right, target_right)):
        rate = c.MOTOR_ACCEL if abs(target) > abs(current) else c.MOTOR_DECEL
        if target != current:
            fraction = limit(rate * dt / abs(target - current), 0, fraction)
    return (left + fraction * (target_left - left),
            right + fraction * (target_right - right))


@dataclass
class LineReading:
    error: int = 0
    peak: int = 0
    valid: bool = False
    strong: bool = False
    quality: str = '?'


def analyze_line(values, c):
    if len(values) != c.SensorCount:
        raise ValueError('Antall sensorverdier stemmer ikke med SensorCount')
    if any(type(v) is not int or not 0 <= v <= 1000 for v in values):
        raise ValueError('Forventer kalibrerte heltallsverdier 0–1000')
    result = LineReading(peak=max(values))
    weights = [max(0, value - c.SENSOR_FLOOR) for value in values]
    if result.peak < c.TRACK_MIN_PEAK:
        return result
    if result.peak - min(values) < c.LINE_MIN_CONTRAST:
        result.quality = 'F'
        return result
    core = (result.peak - c.SENSOR_FLOOR) * c.CORE_PERCENT // 100
    all_core = best_weight = best_start = best_end = 0
    i = 0
    while i < c.SensorCount:
        if weights[i] < core or weights[i] == 0:
            i += 1
            continue
        start, weight = i, 0
        while i < c.SensorCount and weights[i] >= core and weights[i] > 0:
            weight += weights[i]
            i += 1
        all_core += weight
        if weight > best_weight:
            best_weight, best_start, best_end = weight, start, i - 1
    if best_weight == 0 or best_weight < c.GROUP_DOMINANCE * (all_core - best_weight):
        result.quality = 'X'
        return result
    if best_end - best_start + 1 > c.LINE_MAX_WIDTH:
        result.quality = 'B'
        return result
    first, last = best_start, best_end
    if first > 0 and weights[first - 1] < core:
        first -= 1
    if last + 1 < c.SensorCount and weights[last + 1] < core:
        last += 1
    total = weighted = 0
    for i in range(first, last + 1):
        weight = max(0, values[i] - c.POSITION_FLOOR)
        total += weight
        weighted += weight * (i * 1000 - c.CENTER)
    # C++ heltallsdivisjon kutter mot null, også for negative tall.
    result.error = c.SENSOR_DIRECTION * int(weighted / total)
    result.strong = result.peak >= c.LINE_MIN_PEAK
    result.quality = 'V' if result.strong else 'L'
    result.valid = True
    return result


@dataclass
class LineController:
    left: float = 0
    right: float = 0
    base: float = 0
    derivative: float = 0
    previousError: int = 0
    lastSide: int = 0
    searchSide: int = 0
    candidateError: int = 0
    previousTime: int = 0
    lossStart: int = 0
    stableStart: int = 0
    sideTime: int = 0
    candidateStart: int = 0
    initialized: bool = False
    derivativeReady: bool = False
    stableTiming: bool = False
    gapActive: bool = False
    recovering: bool = False
    braking: bool = False
    candidate: bool = False
    aligning: bool = False
    stopped: bool = False
    haveStrong: bool = False
    uncertain: bool = False
    strongError: int = 0
    strongTime: int = 0
    uncertainStart: int = 0
    uncertainLeft: float = 0
    uncertainRight: float = 0
    captureRate: float = 0
    captureTime: int = 0
    captureSeen: bool = False
    captureSide: int = 0
    captureSamples: int = 0
    directionCandidateSide: int = 0
    directionCandidateStart: int = 0

    def mode(self):
        if self.stopped:
            return 'S'
        if self.uncertain:
            return 'U'
        if not self.recovering:
            return 'G' if self.gapActive else 'T'
        if self.candidate:
            return 'A' if self.aligning else 'C'
        return 'B' if self.braking else 'R'

    def stop(self):
        self.left = self.right = 0
        self.stopped = True

    def update(self, observation, now, c):
        if self.stopped:
            return
        now &= 0xFFFFFFFF
        dt = elapsed(now, self.previousTime) / 1000 if self.initialized else c.CONTROL_PERIOD_MS / 1000
        if dt <= 0:
            return
        self.initialized = True
        self.previousTime = now
        ramp_dt = min(dt, c.MAX_RAMP_DT)
        line = replace(observation)
        if line.valid and not line.strong:
            line.valid = (not self.recovering and self.haveStrong
                          and elapsed(now, self.strongTime) <= c.WEAK_HOLD_MS
                          and abs(line.error - self.strongError) <= c.WEAK_MAX_SHIFT)

        # Sterkt, men uklart signal håndteres før vanlig tap/søk.
        ambiguous = not line.valid and line.quality in 'FBX'
        if ambiguous:
            if not self.uncertain:
                self.uncertain = True
                self.uncertainStart = now
                self.uncertainLeft = self.uncertainRight = 0
                recent = ((self.haveStrong and elapsed(now, self.strongTime) <= c.AMBIGUOUS_HINT_MS)
                          or (self.recovering and self.captureSeen
                              and elapsed(now, self.captureTime) <= c.AMBIGUOUS_HINT_MS))
                peak = max(abs(self.left), abs(self.right))
                if recent and peak > 0:
                    speed = limit(c.AMBIGUOUS_ESCAPE_PWM, 0, c.MOTOR_LIMIT)
                    self.uncertainLeft = self.left / peak * speed
                    self.uncertainRight = self.right / peak * speed
            if elapsed(now, self.uncertainStart) >= c.AMBIGUOUS_TIMEOUT_MS:
                self.stop()
                return
            if self.recovering and elapsed(now, self.lossStart) >= c.SEARCH_TIMEOUT_MS:
                self.stop()
                return
            if not self.recovering and not self.gapActive:
                self.gapActive, self.lossStart = True, now
            self.stableTiming = self.derivativeReady = False
            self.derivative = 0
            self.candidate = False
            self.directionCandidateSide = self.captureSamples = 0
            self.captureRate = 0
            self.aligning = False
            moving = elapsed(now, self.uncertainStart) < c.AMBIGUOUS_ESCAPE_MS
            self.left = ramp_motor(self.left, self.uncertainLeft if moving else 0, ramp_dt, c, True)
            self.right = ramp_motor(self.right, self.uncertainRight if moving else 0, ramp_dt, c, True)
            return
        self.uncertain = False

        if not self.recovering:
            if not line.valid:
                if not self.gapActive:
                    self.gapActive, self.lossStart = True, now
                self.stableTiming = self.derivativeReady = False
                self.derivative = 0
                if elapsed(now, self.lossStart) < c.LOSS_CONFIRM_MS:
                    self.left = approach(self.left, 0, c.GAP_DECEL * ramp_dt)
                    self.right = approach(self.right, 0, c.GAP_DECEL * ramp_dt)
                    return
                if self.lastSide and elapsed(now, self.sideTime) > c.SIDE_MAX_AGE_MS:
                    self.lastSide = 0
                self.searchSide = self.lastSide or c.UNKNOWN_SEARCH_SIDE
                self.recovering = True
                self.captureSeen = False
                self.directionCandidateSide = self.captureSide = self.captureSamples = 0
                self.braking = self.lastSide == 0
                self.candidate = self.aligning = False
            else:
                self.gapActive = False

        if self.recovering:
            age = elapsed(now, self.lossStart)
            if age >= c.SEARCH_TIMEOUT_MS and (not line.valid or age >= c.SEARCH_TIMEOUT_MS + c.REACQUIRE_GRACE_MS):
                self.stop()
                return
            if not line.valid:
                self.directionCandidateSide = self.captureSamples = 0
                self.candidate = self.aligning = False
                self.captureRate = 0
                speed = limit(limit(c.SEARCH_SPEED, 0, c.RECOVERY_SEARCH_LIMIT), 0, c.MOTOR_LIMIT)
                if self.captureSeen:
                    age = elapsed(now, self.captureTime)
                    if self.captureSide == 0:
                        speed = 0 if age < c.RECOVERY_HOLD_MS else speed
                    else:
                        self.searchSide = self.captureSide
                        if age < c.RECOVERY_HOLD_MS:
                            speed = limit(speed, 0, c.RECOVERY_HOLD_TURN)
                    self.left = ramp_motor(self.left, self.searchSide * speed, ramp_dt, c, True)
                    self.right = ramp_motor(self.right, -self.searchSide * speed, ramp_dt, c, True)
                elif self.braking:
                    self.left = ramp_motor(self.left, 0, ramp_dt, c)
                    self.right = ramp_motor(self.right, 0, ramp_dt, c)
                    if self.left == 0 and self.right == 0:
                        self.braking = False
                else:
                    self.left = ramp_motor(self.left, self.searchSide * speed, ramp_dt, c)
                    self.right = ramp_motor(self.right, -self.searchSide * speed, ramp_dt, c)
                return

            continuous = self.candidate and abs(line.error - self.candidateError) <= c.REACQUIRE_MAX_STEP
            if not self.candidate:
                self.candidateStart = now
            # Bevar firmware: store hopp nullstiller rate, ikke candidateStart.
            if not continuous:
                self.captureRate, self.captureSamples = 0, 1
            else:
                sample_dt = elapsed(now, self.captureTime) / 1000
                if sample_dt > 0:
                    rate = (line.error - self.candidateError) / sample_dt
                    alpha = sample_dt / (c.D_FILTER_SECONDS + sample_dt)
                    self.captureRate += alpha * (rate - self.captureRate)
                    self.captureSamples = min(2, self.captureSamples + 1)
            self.candidate = self.captureSeen = True
            self.candidateError, self.captureTime = line.error, now
            self.braking, self.aligning = False, True
            observed_side = (1 if line.error > 0 else -1) if abs(line.error) >= c.RECOVERY_DIRECTION_ERROR else 0
            if observed_side == 0:
                self.directionCandidateSide = self.captureSide = 0
            elif observed_side == self.searchSide:
                self.directionCandidateSide = 0
                self.captureSide = self.searchSide
            else:
                if self.directionCandidateSide != observed_side:
                    self.directionCandidateSide = observed_side
                    self.directionCandidateStart = now
                self.captureSide = 0
                if elapsed(now, self.directionCandidateStart) >= c.RECOVERY_DIRECTION_CONFIRM_MS:
                    self.searchSide = self.captureSide = observed_side
                    self.directionCandidateSide = 0

            distance = abs(line.error)
            boost_start = max(c.RECOVERY_FAR_START_ERROR, c.RECOVERY_FORWARD_ERROR)
            boost_end = max(c.RECOVERY_FAR_FULL_ERROR, boost_start + 1)
            far_weight = limit((distance - boost_start) / (boost_end - boost_start), 0, 1)
            far_weight = far_weight * far_weight * (3 - 2 * far_weight)
            near_limit = limit(c.RECOVERY_CAPTURE_LIMIT, 0, c.MOTOR_LIMIT)
            far_turn = limit(c.RECOVERY_FAR_TURN, near_limit, c.MOTOR_LIMIT)
            capture_limit = near_limit + far_weight * (far_turn - near_limit)
            d_limit = limit(c.RECOVERY_D_LIMIT, 0, capture_limit)
            d_turn = limit(c.KD * self.captureRate, -d_limit, d_limit)
            turn = limit(c.KP * line.error + d_turn, -capture_limit, capture_limit)
            if line.error != 0:
                side = 1 if line.error > 0 else -1
                center_range = max(c.RECOVERY_FORWARD_ERROR, 1)
                full_range = max(c.RECOVERY_FULL_ALIGN_ERROR, center_range)
                rate_range = max(c.RECOVERY_FORWARD_RATE, 1)
                approaching = limit(-side * self.captureRate / rate_range, 0, 1)
                minimum = limit(c.ALIGN_MIN_SPEED, 0, capture_limit)
                minimum *= limit(distance / full_range, 0, 1)
                minimum *= 1 - approaching * (1 - limit(c.RECOVERY_MOVING_MIN_FACTOR, 0, 1))
                far_minimum = far_turn * (1 - approaching * (1 - limit(c.RECOVERY_FAR_MOVING_FACTOR, 0, 1)))
                minimum += far_weight * (far_minimum - minimum)
                bridge = limit(c.RECOVERY_BRIDGE_MIN_TURN, 0, capture_limit) * limit(distance / center_range, 0, 1)
                minimum = max(minimum, bridge)
                weight = limit(distance / center_range, 0, 1)
                weight = weight * weight * (3 - 2 * weight)
                lower_bound = minimum - capture_limit * (1 - weight)
                directed = max(side * turn, lower_bound)
                turn = side * limit(directed, -capture_limit, capture_limit)

            confirmed = elapsed(now, self.candidateStart) >= c.REACQUIRE_TIME_MS
            ready = (confirmed and self.captureSamples >= 2 and abs(line.error) <= c.RECOVERY_FORWARD_ERROR
                     and abs(self.captureRate) <= c.RECOVERY_FORWARD_RATE)
            drive = limit((self.left + self.right) * 0.5, 0, c.MOTOR_LIMIT)
            approach_drive = limit(c.RECOVERY_APPROACH_DRIVE, 0, c.CORNER_SPEED) if abs(line.error) <= c.RECOVERY_FORWARD_ERROR else 0
            target = c.CORNER_SPEED if ready else approach_drive
            drive = approach(drive, target, (c.RESTART_ACCEL if target > drive else c.BASE_DECEL) * ramp_dt)
            drive = limit(drive, 0, c.MOTOR_LIMIT - abs(turn))
            self.left = ramp_motor(self.left, drive + turn, ramp_dt, c, True)
            self.right = ramp_motor(self.right, drive - turn, ramp_dt, c, True)
            if not ready or self.left < 0 or self.right < 0 or abs((self.left - self.right) * 0.5) > c.RECOVERY_EXIT_TURN:
                return
            self.recovering = self.gapActive = self.braking = self.candidate = self.aligning = self.captureSeen = False
            self.captureSide = self.captureSamples = 0
            self.derivativeReady = False
            self.derivative = 0
            self.previousError = line.error
            self.stableTiming = False
            self.base = limit((self.left + self.right) * 0.5, 0, c.CORNER_SPEED)
            self.haveStrong = True
            self.strongTime, self.strongError = now, line.error
            self.lastSide = (1 if line.error > 0 else -1) if abs(line.error) > c.SIDE_MEMORY_ERROR else 0
            self.sideTime = now
            return

        error = line.error
        if line.strong:
            self.haveStrong = True
            self.strongTime, self.strongError = now, error
        if self.derivativeReady:
            rate = (error - self.previousError) / dt
            alpha = dt / (c.D_FILTER_SECONDS + dt)
            self.derivative += alpha * (rate - self.derivative)
        else:
            self.derivative = 0
        self.previousError, self.derivativeReady = error, True
        if line.strong and abs(error) > c.SIDE_MEMORY_ERROR:
            self.lastSide = 1 if error > 0 else -1
            self.sideTime = now
        elif elapsed(now, self.sideTime) > c.SIDE_MAX_AGE_MS:
            self.lastSide = 0
        stable = line.strong and abs(error) < c.STABLE_ERROR and abs(self.derivative) < c.STABLE_RATE
        if not stable:
            self.stableTiming = False
        elif not self.stableTiming:
            self.stableTiming, self.stableStart = True, now
        if self.stableTiming and elapsed(now, self.stableStart) >= c.SIDE_CLEAR_MS:
            self.lastSide = 0
        settled = self.stableTiming and elapsed(now, self.stableStart) >= c.STABLE_TIME_MS
        target = limit(c.BASE_SPEED - c.ERROR_SPEED_DROP * abs(error) - c.RATE_SPEED_DROP * abs(self.derivative), c.CORNER_SPEED, c.BASE_SPEED)
        if not settled:
            target = c.CORNER_SPEED
        if target < self.base:
            self.base = approach(self.base, target, c.BASE_DECEL * ramp_dt)
        elif self.base < c.CORNER_SPEED:
            self.base = approach(self.base, c.CORNER_SPEED, c.RESTART_ACCEL * ramp_dt)
        else:
            self.base = approach(self.base, target, c.BASE_ACCEL * ramp_dt)
        d_turn = limit(c.KD * self.derivative, -c.MAX_D_TURN, c.MAX_D_TURN)
        p_turn = c.KP * error
        extra_error = abs(error) - c.EXTRA_TURN_START
        if extra_error > 0:
            extra = c.EXTRA_TURN_GAIN * extra_error
            p_turn += extra if error > 0 else -extra
        turn = limit(p_turn + d_turn, -c.MAX_TURN, c.MAX_TURN)
        if abs(error) >= c.EDGE_GUARD_ERROR and turn * error < 0:
            turn = c.EDGE_P_FACTOR * c.KP * error
        target_left = limit(self.base + turn, 0, c.MOTOR_LIMIT + c.MAX_TURN)
        target_right = limit(self.base - turn, 0, c.MOTOR_LIMIT + c.MAX_TURN)
        peak = max(target_left, target_right)
        if peak > c.MOTOR_LIMIT:
            target_left *= c.MOTOR_LIMIT / peak
            target_right *= c.MOTOR_LIMIT / peak
        self.left, self.right = ramp_following(self.left, self.right, target_left, target_right, ramp_dt, c)
