//
// Created by tonzg on 05/10/2026.
//

#include <stdint.h>
#include "linefollow.h"
#include "motor.h"
#include "conf.h"
#include "helper.h"

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

LineReading analyzeLine(const uint16_t *values) {
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
        }

        if ((uint32_t)(now - uncertainStart) >= AMBIGUOUS_TIMEOUT_MS) {
            stop();
            return;
        }
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

            if (ambiguous) {
                left = approach(left, 0, GAP_DECEL * rampDt);
                right = approach(right, 0, GAP_DECEL * rampDt);
                return;
            }

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
        if (ambiguous) {
            candidate = false;
            aligning = false;
            braking = true;

            brake(rampDt);

            if ((uint32_t)(now - lossStart) >= SEARCH_TIMEOUT_MS) {
                stop();
            }

            return;
        }

        if (line.valid) {
            if (!candidate ||
                magnitude(line.error - candidateError) > REACQUIRE_MAX_STEP) {
                candidateStart = now;
                candidate = true;
                aligning = false;
            }

            candidateError = line.error;
            braking = true;
        } else {
            candidate = false;
            aligning = false;
        }

        uint32_t elapsed = (uint32_t)(now - lossStart);

        if (elapsed >= SEARCH_TIMEOUT_MS &&
            (!candidate ||
             elapsed >= SEARCH_TIMEOUT_MS + REACQUIRE_GRACE_MS)) {
            stop();
            return;
        }

        bool confirmed =
            candidate &&
            (uint32_t)(now - candidateStart) >= REACQUIRE_TIME_MS;

        if (candidate) {
            if (!confirmed || magnitude(line.error) <= REACQUIRE_ERROR) {
                // Bekreftet sentralt treff kan overtas uten full stopp
                // når begge motorutgangene fortsatt driver fremover.
                bool rollingResume =
                    confirmed &&
                    magnitude(line.error) <= STABLE_ERROR &&
                    left > 0 &&
                    right > 0;

                aligning = false;

                if (!rollingResume) {
                    brake(rampDt);

                    if (!confirmed || left != 0 || right != 0) {
                        return;
                    }
                }

                recovering = false;
                gapActive = false;
                braking = false;
                candidate = false;
                derivativeReady = false;
                derivative = 0;
                stableTiming = false;
                lastSide = 0;

                // Gjennomsnittlig gjenværende PWM; null etter full nedbremsing.
                base = limitValue(
                    (left + right) * 0.5f,
                    0.0f,
                    CORNER_SPEED
                );
            } else {
                int observedSide = line.error > 0 ? 1 : -1;

                if (aligning && observedSide != searchSide) {
                    aligning = false;
                    candidateStart = now;
                    brake(rampDt);
                    return;
                }

                if (!aligning) {
                    brake(rampDt);

                    if (left == 0 && right == 0) {
                        // Bare et bekreftet sidefunn får endre søkeretningen.
                        searchSide = observedSide;
                        aligning = true;
                    }

                    return;
                }

                float turn = limitValue(
                    KP * magnitude(line.error),
                    ALIGN_MIN_SPEED,
                    SEARCH_SPEED
                );

                left = rampMotor(left, searchSide * turn, rampDt);
                right = rampMotor(right, -searchSide * turn, rampDt);
                return;
            }
        } else {
            if (braking) {
                brake(rampDt);

                if (left == 0 && right == 0) {
                    braking = false;
                }
            } else {
                left = rampMotor(
                    left,
                    searchSide * SEARCH_SPEED,
                    rampDt
                );

                right = rampMotor(
                    right,
                    -searchSide * SEARCH_SPEED,
                    rampDt
                );
            }

            return;
        }
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