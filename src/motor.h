//
// Created by tonzg on 05/10/2026.
//

#ifndef LINEFOLLOWER_MOTOR_H
#define LINEFOLLOWER_MOTOR_H

void stopMotors();
void spinMotorA(int speedValue);
void spinMotorB(int speedValue);
float rampMotor(float value, float target, float dt);

#endif