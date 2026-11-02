#include <Arduino.h>
#include "board_config.h"
#include "board_pins.h"
#include "hardware.h"
#include "protocol.h"

namespace
{
    bool sensorOk = false;
    bool motorOk = false;
    volatile uint32_t leftTicks = 0;
    volatile uint32_t rightTicks = 0;
    portMUX_TYPE encoderMux = portMUX_INITIALIZER_UNLOCKED;
    uint32_t encoderAt = 0;
    uint32_t previousLeft = 0;
    uint32_t previousRight = 0;
    constexpr uint8_t LeftPwmChannel = 0;
    constexpr uint8_t RightPwmChannel = 1;

    void IRAM_ATTR leftPulse()
    {
        portENTER_CRITICAL_ISR(&encoderMux);
        ++leftTicks;
        portEXIT_CRITICAL_ISR(&encoderMux);
    }

    void IRAM_ATTR rightPulse()
    {
        portENTER_CRITICAL_ISR(&encoderMux);
        ++rightTicks;
        portEXIT_CRITICAL_ISR(&encoderMux);
    }

    enum class PinRole
    {
        Input,
        Output,
        Analog
    };

    bool takePin(int pin, bool* used, PinRole role)
    {
        if (!board_pins::isAvailable(pin, cfg::AllowBootPins) || used[pin])
        {
            return false;
        }

        if (role == PinRole::Output && !board_pins::isOutput(pin))
        {
            return false;
        }

        if (role == PinRole::Analog && !board_pins::isAdc(pin))
        {
            return false;
        }

        used[pin] = true;
        return true;
    }

    bool takeMotor(const cfg::MotorPins& pins, bool* used)
    {
        return takePin(pins.in1, used, PinRole::Output) &&
               takePin(pins.in2, used, PinRole::Output) && takePin(pins.pwm, used, PinRole::Output);
    }

    bool validatePins()
    {
        bool used[40] = {};

        if (cfg::MotorsEnabled &&
            (!takeMotor(cfg::LeftMotor, used) || !takeMotor(cfg::RightMotor, used)))
        {
            return false;
        }

        if (cfg::DriverEnablePin >= 0 && !takePin(cfg::DriverEnablePin, used, PinRole::Output))
        {
            return false;
        }

        if (cfg::Sensors == cfg::SensorBackend::Analog)
        {
            bool muxChannels[16] = {};
            bool muxUsed = false;

            for (uint8_t index = 0; index < cfg::SensorCount; ++index)
            {
                const auto& input = cfg::SensorInputs[index];

                if (input.physicalIndex >= cfg::PhysicalSensorCount ||
                    (index > 0 &&
                     input.physicalIndex <= cfg::SensorInputs[index - 1].physicalIndex))
                {
                    return false;
                }

                if (input.muxChannel == -1)
                {
                    if (!takePin(input.pin, used, PinRole::Analog))
                    {
                        return false;
                    }
                }
                else
                {
                    if (!cfg::MuxEnabled || input.pin != -1 || input.muxChannel < 0 ||
                        input.muxChannel > 15 || muxChannels[input.muxChannel])
                    {
                        return false;
                    }

                    muxChannels[input.muxChannel] = true;
                    muxUsed = true;
                }
            }

            if (muxUsed)
            {
                if (!takePin(cfg::MuxSignalPin, used, PinRole::Analog))
                {
                    return false;
                }

                for (int8_t pin : cfg::MuxSelectPins)
                {
                    if (!takePin(pin, used, PinRole::Output))
                    {
                        return false;
                    }
                }
            }

            if (cfg::EmitterPin >= 0 && !takePin(cfg::EmitterPin, used, PinRole::Output))
            {
                return false;
            }
        }

        const int8_t measurementPins[] = {cfg::VoltagePin, cfg::CurrentPin};
        for (int8_t pin : measurementPins)
        {
            if (pin >= 0 && !takePin(pin, used, PinRole::Analog))
            {
                return false;
            }
        }

        const int8_t encoderPins[] = {cfg::LeftEncoderPin, cfg::RightEncoderPin};
        for (int8_t pin : encoderPins)
        {
            if (pin >= 0 && (!takePin(pin, used, PinRole::Input) ||
                             (cfg::EncoderInternalPullups && !board_pins::isOutput(pin))))
            {
                return false;
            }
        }

        return true;
    }

    void writeMotor(const cfg::MotorPins& pins, uint8_t channel, int16_t value)
    {
        if (pins.inverted)
        {
            value = -value;
        }

        digitalWrite(pins.in1, value > 0 ? HIGH : LOW);
        digitalWrite(pins.in2, value < 0 ? HIGH : LOW);
        ledcWrite(channel, value < 0 ? -value : value);
    }

    int16_t bounded(float value)
    {
        if (value > 32767)
        {
            return 32767;
        }

        if (value < -32767)
        {
            return -32767;
        }

        return int16_t(value);
    }
} // namespace

namespace hw
{
    int16_t appliedLeft = 0;
    int16_t appliedRight = 0;
    uint16_t measurementFlags = 0;
    uint16_t voltageMv = 0;
    uint16_t encoderAgeMs = 0;
    int16_t currentMa = 0;
    int16_t leftRpm10 = 0;
    int16_t rightRpm10 = 0;

    bool begin()
    {
        if (!validatePins())
        {
            return false;
        }

        analogReadResolution(cfg::AdcBits);
        analogSetAttenuation(ADC_11db);

        if (cfg::MotorsEnabled)
        {
            const cfg::MotorPins motors[] = {cfg::LeftMotor, cfg::RightMotor};

            for (uint8_t channel = 0; channel < 2; ++channel)
            {
                const auto& pins = motors[channel];
                pinMode(pins.in1, OUTPUT);
                pinMode(pins.in2, OUTPUT);
                digitalWrite(pins.in1, LOW);
                digitalWrite(pins.in2, LOW);

                if (ledcSetup(channel, cfg::MotorPwmHz, cfg::MotorPwmBits) <= 0)
                {
                    return false;
                }

                ledcAttachPin(pins.pwm, channel);
                ledcWrite(channel, 0);
            }

            if (cfg::DriverEnablePin >= 0)
            {
                pinMode(cfg::DriverEnablePin, OUTPUT);
                digitalWrite(cfg::DriverEnablePin, !cfg::DriverEnableActiveHigh);
            }

            motorOk = true;
        }

        if (cfg::Sensors == cfg::SensorBackend::Analog)
        {
            if (cfg::MuxEnabled)
            {
                for (int8_t pin : cfg::MuxSelectPins)
                {
                    pinMode(pin, OUTPUT);
                    digitalWrite(pin, LOW);
                }
            }

            if (cfg::EmitterPin >= 0)
            {
                pinMode(cfg::EmitterPin, OUTPUT);
                digitalWrite(cfg::EmitterPin, HIGH);
                delay(1);
            }

            sensorOk = true;
        }

        if (cfg::VoltagePin >= 0)
        {
            measurementFlags |= proto::VoltageValid;
        }

        if (cfg::CurrentPin >= 0 && cfg::CurrentVoltsPerAmp > 0)
        {
            measurementFlags |= proto::CurrentValid;
        }

        if (cfg::EncoderPulsesPerWheelRevolution > 0)
        {
            int inputMode = cfg::EncoderInternalPullups ? INPUT_PULLUP : INPUT;

            if (cfg::LeftEncoderPin >= 0)
            {
                pinMode(cfg::LeftEncoderPin, inputMode);
                attachInterrupt(cfg::LeftEncoderPin, leftPulse, RISING);
            }

            if (cfg::RightEncoderPin >= 0)
            {
                pinMode(cfg::RightEncoderPin, inputMode);
                attachInterrupt(cfg::RightEncoderPin, rightPulse, RISING);
            }
        }

        encoderAt = micros();
        return true;
    }

    bool sensorReady()
    {
        return sensorOk;
    }

    bool motorReady()
    {
        return motorOk;
    }

    uint16_t rawMaximum()
    {
        return cfg::AdcMaximum;
    }

    bool readSensors(uint16_t* raw)
    {
        if (!sensorOk)
        {
            return false;
        }

        for (uint8_t index = 0; index < cfg::SensorCount; ++index)
        {
            const auto& input = cfg::SensorInputs[index];
            int pin = input.pin;

            if (input.muxChannel >= 0)
            {
                for (uint8_t bit = 0; bit < 4; ++bit)
                {
                    digitalWrite(cfg::MuxSelectPins[bit], (input.muxChannel >> bit) & 1);
                }

                pin = cfg::MuxSignalPin;
                delayMicroseconds(cfg::MuxSettleUs);
            }

            if (cfg::DiscardFirstAdc)
            {
                analogRead(pin);
            }

            uint32_t sum = 0;
            for (uint8_t sample = 0; sample < cfg::AnalogSamplesPerSensor; ++sample)
            {
                sum += analogRead(pin);
            }

            raw[index] = sum / cfg::AnalogSamplesPerSensor;
        }

        return true;
    }

    void motors(int16_t left, int16_t right)
    {
        if (!motorOk)
        {
            appliedLeft = 0;
            appliedRight = 0;
            return;
        }

        appliedLeft = constrain(left, -255, 255);
        appliedRight = constrain(right, -255, 255);

        if (cfg::DriverEnablePin >= 0)
        {
            digitalWrite(cfg::DriverEnablePin,
                         (left || right) ? cfg::DriverEnableActiveHigh
                                         : !cfg::DriverEnableActiveHigh);
        }

        writeMotor(cfg::LeftMotor, LeftPwmChannel, appliedLeft);
        writeMotor(cfg::RightMotor, RightPwmChannel, appliedRight);
    }

    void stop()
    {
        motors(0, 0);
    }

    void updateMeasurements(uint32_t now)
    {
        // ESP32s kalibrerte mV-API brukes til valgfrie spennings-/strømmålinger.
        if (measurementFlags & proto::VoltageValid)
        {
            float measuredMv = analogReadMilliVolts(cfg::VoltagePin) * cfg::VoltageDividerRatio;
            voltageMv = uint16_t(constrain(measuredMv, 0.0f, 65535.0f));
        }

        if (measurementFlags & proto::CurrentValid)
        {
            float measuredV = analogReadMilliVolts(cfg::CurrentPin) / 1000.0f;
            currentMa = bounded((measuredV - cfg::CurrentZeroV) / cfg::CurrentVoltsPerAmp * 1000);
        }

        uint32_t elapsedUs = now - encoderAt;
        encoderAgeMs = proto::saturated16(elapsedUs / 1000);

        if (elapsedUs < cfg::EncoderWindowUs || cfg::EncoderPulsesPerWheelRevolution == 0)
        {
            return;
        }

        portENTER_CRITICAL(&encoderMux);
        uint32_t left = leftTicks;
        uint32_t right = rightTicks;
        portEXIT_CRITICAL(&encoderMux);

        float factor = 600000000.0f / (elapsedUs * float(cfg::EncoderPulsesPerWheelRevolution));
        leftRpm10 = bounded((left - previousLeft) * factor);
        rightRpm10 = bounded((right - previousRight) * factor);
        previousLeft = left;
        previousRight = right;
        encoderAt = now;
        encoderAgeMs = 0;

        if (cfg::LeftEncoderPin >= 0)
        {
            measurementFlags |= proto::LeftRpmValid;
        }

        if (cfg::RightEncoderPin >= 0)
        {
            measurementFlags |= proto::RightRpmValid;
        }
    }
} // namespace hw
