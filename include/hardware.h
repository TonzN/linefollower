#pragma once
#include <stdint.h>
#include "board_config.h"
namespace hw {
bool begin();
bool sensorReady();
bool motorReady();
bool readSensors(uint16_t* raw);
uint16_t rawMaximum();
void motors(int16_t left,int16_t right);
void stop();
void updateMeasurements(uint32_t now);
extern int16_t appliedLeft,appliedRight;
extern uint16_t measurementFlags,voltageMv;
extern uint16_t encoderAgeMs;
extern int16_t currentMa,leftRpm10,rightRpm10;
}
// Tilpass disse tre funksjonene når sensormodell/grensesnitt er kjent.
namespace custom_sensor {
bool begin();
bool read(uint16_t* raw,uint8_t count);
uint16_t maximum();
}
