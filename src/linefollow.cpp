//
// Created by tonzg on 05/10/2026.
//

#include <stdint.h>
#include "linefollow.h"
#include "motor.h"
#include "conf.h"
#include "helper.h"
#include "recovery_conf.h"
#include "ambiguous_conf.h"
#include "capture_conf.h"

// Egen nedbremsing ved gjenfunn; normal motorakselerasjon beholdes.
// Retningsskifte går gjennom null før motsatt pådrag neste oppdatering.
static float rampCapture(float value, float target, float dt) {
    if (value * target < 0) target = 0;
    float rate = magnitude(target) > magnitude(value) ? MOTOR_ACCEL : RECOVERY_BRAKE_RATE;
    return approach(value, target, rate * dt);
}

// Samme fremdriftsandel på begge hjul hindrer at vekslende korreksjoner bremser ned begge.
// Recovery og stopp bruker fortsatt de separate, raske bremserampene.
void rampFollowing(float &left, float &right, float targetLeft, float targetRight, float dt) {
    float deltaLeft = targetLeft - left, deltaRight = targetRight - right;
    float fraction = 1.0f;
    float rateLeft = magnitude(targetLeft) > magnitude(left) ? MOTOR_ACCEL : MOTOR_DECEL;
    float rateRight = magnitude(targetRight) > magnitude(right) ? MOTOR_ACCEL : MOTOR_DECEL;
    if (deltaLeft != 0) fraction = limitValue(rateLeft * dt / magnitude(deltaLeft), 0, fraction);
    if (deltaRight != 0) fraction = limitValue(rateRight * dt / magnitude(deltaRight), 0, fraction);
    left += fraction * deltaLeft;
    right += fraction * deltaRight;
}

LineReading analyzeLine(const unsigned int *values) {
  LineReading result;
  uint16_t lowest = values[0], weights[SensorCount];
  for (uint8_t i = 0; i < SensorCount; i++) {
    if (values[i] > result.peak) result.peak = values[i];
    if (values[i] < lowest) lowest = values[i];
    weights[i] = values[i] > SENSOR_FLOOR ? values[i] - SENSOR_FLOOR : 0;
  }
  if (result.peak < TRACK_MIN_PEAK) return result;
  if (result.peak - lowest < LINE_MIN_CONTRAST) { result.quality = 'F'; return result; }

  uint16_t coreLevel = (uint32_t)(result.peak - SENSOR_FLOOR) * CORE_PERCENT / 100;
  uint16_t allCore = 0, bestWeight = 0;
  uint8_t bestStart = 0, bestEnd = 0;
  for (uint8_t i = 0; i < SensorCount;) {
    if (weights[i] < coreLevel || weights[i] == 0) { i++; continue; }
    uint8_t start = i;
    uint16_t groupWeight = 0;
    while (i < SensorCount && weights[i] >= coreLevel && weights[i] > 0) groupWeight += weights[i++];
    allCore += groupWeight;
    if (groupWeight > bestWeight) { bestWeight = groupWeight; bestStart = start; bestEnd = i - 1; }
  }
  if (bestWeight == 0 || (uint32_t)bestWeight < (uint32_t)GROUP_DOMINANCE * (allCore - bestWeight)) {
    result.quality = 'X'; return result;
  }
  if (bestEnd - bestStart + 1 > LINE_MAX_WIDTH) { result.quality = 'B'; return result; }

  // Svake nabosensorer bidrar mykt til posisjonen, men teller ikke som en egen linje/ekstra bredde.
  uint8_t first = bestStart, last = bestEnd;
  if (first > 0 && weights[first - 1] < coreLevel) first--;
  if (last + 1 < SensorCount && weights[last + 1] < coreLevel) last++;
  uint16_t total = 0;
  int32_t weightedError = 0;
  for (uint8_t i = first; i <= last; i++) {
    uint16_t weight = values[i] > POSITION_FLOOR ? values[i] - POSITION_FLOOR : 0;
    total += weight;
    // 1000 er posisjonsskala per sensoravstand; 32-bit hindrer overflow på AVR.
    weightedError += (int32_t)weight * ((int32_t)i * 1000 - CENTER);
  }
  result.error = SENSOR_DIRECTION * (int)(weightedError / total);
  result.strong = result.peak >= LINE_MIN_PEAK;
  result.quality = result.strong ? 'V' : 'L';
  result.valid = true;
  return result;
}

char LineController::mode() const {
    if (stopped) return 'S';
    if (uncertain) return 'U';
    if (!recovering) return gapActive ? 'G' : 'T';
    if (candidate) return aligning ? 'A' : 'C';
    return braking ? 'B' : 'R';
  }

void LineController::stop() { left = right = 0; stopped = true; }

void LineController::brake(float dt) {
    left = rampMotor(left, 0, dt);
    right = rampMotor(right, 0, dt);
  }


void LineController::update(const LineReading &observation, uint32_t now) {
    if (stopped) return;

    float dt = initialized
        ? (uint32_t)(now - previousTime) / 1000.0f
        : CONTROL_PERIOD_MS / 1000.0f;

    if (dt <= 0) return;

    initialized = true;
    previousTime = now;

    float rampDt = dt < MAX_RAMP_DT ? dt : MAX_RAMP_DT;
    LineReading line = observation;

    if (line.valid && !line.strong) {
        // Svakt signal får videreføre kjent linje, men ikke avslutte recovery.
        line.valid =
            !recovering &&
            haveStrong &&
            (uint32_t)(now - strongTime) <= WEAK_HOLD_MS &&
            magnitude(line.error - strongError) <= WEAK_MAX_SHIFT;
    }

    bool ambiguous =
        !line.valid &&
        (line.quality == 'F' ||
         line.quality == 'B' ||
         line.quality == 'X');

    if (ambiguous) {
        if (!uncertain) {
            uncertain = true;
            uncertainStart = now;
            uncertainLeft = uncertainRight = 0;
            bool recentLine = (haveStrong &&
                (uint32_t)(now - strongTime) <= AMBIGUOUS_HINT_MS) ||
                (recovering && captureSeen &&
                (uint32_t)(now - captureTime) <= AMBIGUOUS_HINT_MS);
            float peak = magnitude(left) > magnitude(right)
                ? magnitude(left) : magnitude(right);
            if (recentLine && peak > 0) {
                // Behold forhold og fortegn fra faktisk utgang; ingen ny blind retning.
                float speed = limitValue(AMBIGUOUS_ESCAPE_PWM, 0, MOTOR_LIMIT);
                uncertainLeft = left / peak * speed;
                uncertainRight = right / peak * speed;
            }
        }

        if ((uint32_t)(now - uncertainStart) >= AMBIGUOUS_TIMEOUT_MS) {
            stop();
            return;
        }
        if (recovering && (uint32_t)(now - lossStart) >= SEARCH_TIMEOUT_MS) {
            stop();
            return;
        }
        if (!recovering && !gapActive) { gapActive = true; lossStart = now; }
        stableTiming = false;
        derivativeReady = false;
        derivative = 0;
        candidate = false;
        directionCandidateSide = 0;
        captureSamples = 0;
        captureRate = 0;
        aligning = false;
        bool moving = (uint32_t)(now - uncertainStart) < AMBIGUOUS_ESCAPE_MS;
        left = rampCapture(left, moving ? uncertainLeft : 0, rampDt);
        right = rampCapture(right, moving ? uncertainRight : 0, rampDt);
        return;
    } else {
        uncertain = false;
    }

    if (!recovering) {
        if (!line.valid) {
            if (!gapActive) {
                gapActive = true;
                lossStart = now;
            }

            stableTiming = false;
            derivativeReady = false;
            derivative = 0;

            if ((uint32_t)(now - lossStart) < LOSS_CONFIRM_MS) {
                left = approach(left, 0, GAP_DECEL * rampDt);
                right = approach(right, 0, GAP_DECEL * rampDt);
                return;
            }

            if (lastSide &&
                (uint32_t)(now - sideTime) > SIDE_MAX_AGE_MS) {
                lastSide = 0;
            }

            searchSide = lastSide ? lastSide : UNKNOWN_SEARCH_SIDE;
            recovering = true;
            captureSeen = false;
            directionCandidateSide = 0;
            captureSide = 0;
            captureSamples = 0;
            // Med et ferskt sidehint begynner styringen straks.
            // Uten sidehint beholdes nedbremsing før søk i standardretningen.
            braking = (lastSide == 0);
            candidate = false;
            aligning = false;
        } else {
            gapActive = false;
        }
    }

    if (recovering) {
        uint32_t elapsed = (uint32_t)(now - lossStart);
        if (elapsed >= SEARCH_TIMEOUT_MS &&
            (!line.valid || elapsed >= SEARCH_TIMEOUT_MS + REACQUIRE_GRACE_MS)) {
            stop();
            return;
        }

        if (!line.valid) {
            directionCandidateSide = 0;
            candidate = false;
            captureSamples = 0;
            aligning = false;
            captureRate = 0;
            if (captureSeen) {
                // Et kort treff skal ikke glemmes og føre til full søkefart
                // i en retning som siste sterke posisjon motsier.
                uint32_t age = (uint32_t)(now - captureTime);
                float searchSpeed = limitValue(SEARCH_SPEED, 0, RECOVERY_SEARCH_LIMIT);
                searchSpeed = limitValue(searchSpeed, 0, MOTOR_LIMIT);
                if (captureSide == 0) {
                    float speed = age < RECOVERY_HOLD_MS ? 0 :
                        searchSpeed;
                    left = rampCapture(left, searchSide * speed, rampDt);
                    right = rampCapture(right, -searchSide * speed, rampDt);
                    return;
                }
                searchSide = captureSide;
                // Kort demping etter treff; deretter ramper søket opp igjen.
                // Siste troverdige side beholdes, uten tidsstyrt retningsbytte.
                float speed = age < RECOVERY_HOLD_MS
                    ? limitValue(searchSpeed, 0, RECOVERY_HOLD_TURN) : searchSpeed;
                left = rampCapture(left, searchSide * speed, rampDt);
                right = rampCapture(right, -searchSide * speed, rampDt);
            } else if (braking) {
                brake(rampDt);
                if (left == 0 && right == 0) braking = false;
            } else {
                float speed = limitValue(SEARCH_SPEED, 0, RECOVERY_SEARCH_LIMIT);
                speed = limitValue(speed, 0, MOTOR_LIMIT);
                left = rampMotor(left, searchSide * speed, rampDt);
                right = rampMotor(right, -searchSide * speed, rampDt);
            }
            return;
        }

        // Bekreftelse av linjetilstedeværelse og måling av bevegelse er separate.
        bool continuous = candidate &&
            magnitude(line.error - candidateError) <= REACQUIRE_MAX_STEP;
        if (!candidate) candidateStart = now;
        if (!continuous) {
            captureRate = 0;
            captureSamples = 1;
        } else {
            float sampleDt = (uint32_t)(now - captureTime) / 1000.0f;
            if (sampleDt > 0) {
                float rate = (line.error - candidateError) / sampleDt;
                float alpha = sampleDt / (D_FILTER_SECONDS + sampleDt);
                captureRate += alpha * (rate - captureRate);
                if (captureSamples < 2) captureSamples++;
            }
        }
        candidate = true;
        captureSeen = true;
        candidateError = line.error;
        captureTime = now;
        braking = false;
        aligning = true;

        // Synlig styring reagerer straks; blind søkeretning krever vedvarende sidefunn.
        int observedSide = magnitude(line.error) >= RECOVERY_DIRECTION_ERROR
            ? (line.error > 0 ? 1 : -1) : 0;
        if (observedSide == 0) {
            directionCandidateSide = 0;
            captureSide = 0;
        } else if (observedSide == searchSide) {
            directionCandidateSide = 0;
            captureSide = searchSide;
        } else {
            if (directionCandidateSide != observedSide) {
                directionCandidateSide = observedSide;
                directionCandidateStart = now;
            }
            captureSide = 0;
            if ((uint32_t)(now - directionCandidateStart) >= RECOVERY_DIRECTION_CONFIRM_MS) {
                searchSide = observedSide;
                captureSide = observedSide;
                directionCandidateSide = 0;
            }
        }

        float distance = magnitude(line.error);
        float boostStart = RECOVERY_FAR_START_ERROR > RECOVERY_FORWARD_ERROR
            ? RECOVERY_FAR_START_ERROR : RECOVERY_FORWARD_ERROR;
        float boostEnd = RECOVERY_FAR_FULL_ERROR > boostStart
            ? RECOVERY_FAR_FULL_ERROR : boostStart + 1;
        float farWeight = limitValue((distance - boostStart) / (boostEnd - boostStart), 0, 1);
        farWeight = farWeight * farWeight * (3 - 2 * farWeight);
        float nearLimit = limitValue(RECOVERY_CAPTURE_LIMIT, 0, MOTOR_LIMIT);
        float farTurn = limitValue(RECOVERY_FAR_TURN, nearLimit, MOTOR_LIMIT);
        float captureLimit = nearLimit + farWeight * (farTurn - nearLimit);
        float dLimit = limitValue(RECOVERY_D_LIMIT, 0, captureLimit);
        float dTurn = limitValue(KD * captureRate, -dLimit, dLimit);
        float turn = limitValue(KP * line.error + dTurn, -captureLimit, captureLimit);
        if (line.error != 0) {
            float side = line.error > 0 ? 1.0f : -1.0f;
            float distance = magnitude(line.error);
            float centerRange = RECOVERY_FORWARD_ERROR > 0 ? RECOVERY_FORWARD_ERROR : 1;
            float fullRange = RECOVERY_FULL_ALIGN_ERROR > centerRange
                ? RECOVERY_FULL_ALIGN_ERROR : centerRange;
            float rateRange = RECOVERY_FORWARD_RATE > 0 ? RECOVERY_FORWARD_RATE : 1;
            float approaching = limitValue(-side * captureRate / rateRange, 0, 1);
            float minimumTurn = limitValue(ALIGN_MIN_SPEED, 0, captureLimit);
            minimumTurn *= limitValue(distance / fullRange, 0, 1);
            minimumTurn *= 1 - approaching *
                (1 - limitValue(RECOVERY_MOVING_MIN_FACTOR, 0, 1));

            // Eget pådrag langt ute; den opprinnelige sentreringen beholdes nær midten.
            float farMinimum = farTurn * (1 - approaching *
                (1 - limitValue(RECOVERY_FAR_MOVING_FACTOR, 0, 1)));
            minimumTurn += farWeight * (farMinimum - minimumTurn);

            // Unngå svakt pådrag like utenfor området der fremdriften overtar.
            float bridgeMinimum = limitValue(RECOVERY_BRIDGE_MIN_TURN, 0, captureLimit)
                * limitValue(distance / centerRange, 0, 1);
            if (minimumTurn < bridgeMinimum) minimumTurn = bridgeMinimum;

            // Jevn overgang: behold innstyring langt ute, tillat motdemping nær sentrum.
            float weight = limitValue(distance / centerRange, 0, 1);
            weight = weight * weight * (3 - 2 * weight); // Smoothstep, faste polynomkoeffisienter.
            float lowerBound = minimumTurn - captureLimit * (1 - weight);
            float directedTurn = side * turn;
            if (directedTurn < lowerBound) directedTurn = lowerBound;
            turn = side * limitValue(directedTurn, -captureLimit, captureLimit);
        }

        bool confirmed = (uint32_t)(now - candidateStart) >= REACQUIRE_TIME_MS;
        bool forwardReady = confirmed && captureSamples >= 2 &&
            magnitude(line.error) <= RECOVERY_FORWARD_ERROR &&
            magnitude(captureRate) <= RECOVERY_FORWARD_RATE;
        float drive = limitValue((left + right) * 0.5f, 0, MOTOR_LIMIT);
        // Bygg litt fremdrift under sentrering; full overtakelse krever fortsatt bekreftelse.
        float approachDrive = magnitude(line.error) <= RECOVERY_FORWARD_ERROR
            ? limitValue(RECOVERY_APPROACH_DRIVE, 0, CORNER_SPEED) : 0;
        float targetDrive = forwardReady ? CORNER_SPEED : approachDrive;
        float driveRate = targetDrive > drive ? RESTART_ACCEL : BASE_DECEL;
        drive = approach(drive, targetDrive, driveRate * rampDt);
        drive = limitValue(drive, 0, MOTOR_LIMIT - magnitude(turn));
        left = rampCapture(left, drive + turn, rampDt);
        right = rampCapture(right, drive - turn, rampDt);

        if (!forwardReady || left < 0 || right < 0 ||
            magnitude((left - right) * 0.5f) > RECOVERY_EXIT_TURN) return;

        recovering = false;
        gapActive = false;
        braking = false;
        candidate = false;
        aligning = false;
        captureSeen = false;
        captureSide = 0;
        captureSamples = 0;
        derivativeReady = false;
        derivative = 0;
        previousError = line.error;
        stableTiming = false;
        base = limitValue((left + right) * 0.5f, 0, CORNER_SPEED);
        haveStrong = true;
        strongTime = now;
        strongError = line.error;
        lastSide = magnitude(line.error) > SIDE_MEMORY_ERROR ? (line.error > 0 ? 1 : -1) : 0;
        sideTime = now;
        return;
    }

    int error = line.error;

    if (line.strong) {
        haveStrong = true;
        strongTime = now;
        strongError = error;
    }

    if (derivativeReady) {
        float rate = (error - previousError) / dt;
        float alpha = dt / (D_FILTER_SECONDS + dt);
        derivative += alpha * (rate - derivative);
    } else {
        derivative = 0;
    }

    previousError = error;
    derivativeReady = true;

    if (line.strong &&
        (error > SIDE_MEMORY_ERROR || error < -SIDE_MEMORY_ERROR)) {
        lastSide = error > 0 ? 1 : -1;
        sideTime = now;
    } else if ((uint32_t)(now - sideTime) > SIDE_MAX_AGE_MS) {
        lastSide = 0;
    }

    bool stable =
        line.strong &&
        magnitude(error) < STABLE_ERROR &&
        magnitude(derivative) < STABLE_RATE;

    if (!stable) {
        stableTiming = false;
    } else if (!stableTiming) {
        stableTiming = true;
        stableStart = now;
    }

    if (stableTiming &&
        (uint32_t)(now - stableStart) >= SIDE_CLEAR_MS) {
        lastSide = 0;
    }

    bool settled =
        stableTiming &&
        (uint32_t)(now - stableStart) >= STABLE_TIME_MS;

    float targetBase = limitValue(
        BASE_SPEED
            - ERROR_SPEED_DROP * magnitude(error)
            - RATE_SPEED_DROP * magnitude(derivative),
        CORNER_SPEED,
        BASE_SPEED
    );

    if (!settled) {
        targetBase = CORNER_SPEED;
    }

    if (targetBase < base) {
        base = approach(base, targetBase, BASE_DECEL * rampDt);
    } else if (base < CORNER_SPEED) {
        base = approach(base, CORNER_SPEED, RESTART_ACCEL * rampDt);
    } else {
        base = approach(base, targetBase, BASE_ACCEL * rampDt);
    }

    float dTurn = limitValue(
        KD * derivative,
        -MAX_D_TURN,
        MAX_D_TURN
    );

    float pTurn = KP * error;
    float extraError = magnitude(error) - EXTRA_TURN_START;

    if (extraError > 0.0f) {
        float extraTurn = EXTRA_TURN_GAIN * extraError;
        pTurn += error > 0 ? extraTurn : -extraTurn;
    }

    float turn = limitValue(
        pTurn + dTurn,
        -MAX_TURN,
        MAX_TURN
    );

    if (magnitude(error) >= EDGE_GUARD_ERROR && turn * error < 0) {
        turn = EDGE_P_FACTOR * KP * error;
    }

    float targetLeft = limitValue(
        base + turn,
        0,
        MOTOR_LIMIT + MAX_TURN
    );

    float targetRight = limitValue(
        base - turn,
        0,
        MOTOR_LIMIT + MAX_TURN
    );

    float peakMotor = targetLeft > targetRight
        ? targetLeft
        : targetRight;

    if (peakMotor > MOTOR_LIMIT) {
        targetLeft *= MOTOR_LIMIT / peakMotor;
        targetRight *= MOTOR_LIMIT / peakMotor;
    }

    rampFollowing(left, right, targetLeft, targetRight, rampDt);
}
