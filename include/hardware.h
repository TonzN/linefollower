#pragma once
#include <stdint.h>
#include "board_config.h"

namespace hw
{
    // Valider pinner og start konfigurert maskinvare uten å sette motorene i bevegelse.
    bool begin();
    bool sensorReady();
    bool motorReady();
    // Fyll SensorCount råverdier. Returner false hvis en gyldig avlesning mangler.
    bool readSensors(uint16_t* raw);
    uint16_t rawMaximum();
    // Fortegn er retning; magnitude er PWM (0..255), ikke fysisk hastighet.
    void motors(int16_t left, int16_t right);
    void stop();
    // Valgfrie ADC-/enkodermålinger. now bruker micros().
    void updateMeasurements(uint32_t now);
    extern int16_t appliedLeft;
    extern int16_t appliedRight;
    extern uint16_t measurementFlags;
    extern uint16_t voltageMv;
    extern uint16_t encoderAgeMs;
    extern int16_t currentMa;
    extern int16_t leftRpm10;
    extern int16_t rightRpm10;
} // namespace hw

// Tilpass disse tre funksjonene når sensormodell/grensesnitt er kjent.
namespace custom_sensor
{
    bool begin();
    bool read(uint16_t* raw, uint8_t count);
    uint16_t maximum();
} // namespace custom_sensor
