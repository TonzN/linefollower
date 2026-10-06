//
// Created by tonzg on 05/10/2026.
//

#include <Arduino.h>
#include "motor.h"
#include "conf.h"
#include "helper.h"

void stopMotors() {
    spinMotorA(0);
    spinMotorB(0);
}

void spinMotorA(int speedValue) {
    if (speedValue > 0) {
        digitalWrite(AIN1, HIGH);
        digitalWrite(AIN2, LOW);
    } else if (speedValue < 0) {
        digitalWrite(AIN1, LOW);
        digitalWrite(AIN2, HIGH);
    } else {
        digitalWrite(AIN1, LOW);
        digitalWrite(AIN2, LOW);
    }

    analogWrite(PWMA, abs(speedValue+offsetA));
}

void spinMotorB(int speedValue) {
    if (speedValue > 0) {
        digitalWrite(BIN1, HIGH);
        digitalWrite(BIN2, LOW);
    } else if (speedValue < 0) {
        digitalWrite(BIN1, LOW);
        digitalWrite(BIN2, HIGH);
    } else {
        digitalWrite(BIN1, LOW);
        digitalWrite(BIN2, LOW);
    }

    analogWrite(PWMB, abs(speedValue+offsetB));
}

float rampMotor(float value, float target, float dt) {
    // Retningsskifte går via null i minst én oppdatering.
    if (value * target < 0) target = 0;
    float rate = magnitude(target) > magnitude(value) ? MOTOR_ACCEL : MOTOR_DECEL;
    return approach(value, target, rate * dt);
}

