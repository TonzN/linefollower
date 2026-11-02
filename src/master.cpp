// Hoved-Nano: samme regulator/runtime som tidligere, nå med USB direkte til PC.
#include <Arduino.h>
#include "side_config.h"
#include "side_link.h"
#include "control_runtime.h"
#include <stdlib.h>

namespace host
{
    char commandBuffer[96];
    char replyBuffer[112];
    uint8_t commandLength = 0;
    uint8_t replyReadIndex = 0;
    uint8_t replyLength = 0;
    uint8_t prefixAt = 0;
    uint16_t telemetryByteIndex = 0;
    uint8_t telemetryChecksum = 0;
    uint8_t pendingTelemetryByte = 0;
    bool writingLowNibble = false;
    bool commandOverflow = false;
    bool sendingTelemetry = false;
    uint16_t sequence = 0;
    const uint8_t* snapshot = nullptr;
    sideproto::Sample sideSnapshot;
    uint16_t sideAgeMs = 65535;
    bool sideFresh = false;

    // Tekstene ligger i flash (PSTR), ikke i den lille SRAM-en på Nano.
    // Svarbufferen brukes kun til korte kommando-/feilsvar, aldri hele målerapporten.
    void appendCharacter(char character)
    {
        if (replyLength < sizeof(replyBuffer) - 1)
        {
            replyBuffer[replyLength++] = character;
            replyBuffer[replyLength] = '\0';
        }
    }

    void appendFlashText(const char* flashText)
    {
        char character = pgm_read_byte(flashText++);

        while (character != '\0')
        {
            appendCharacter(character);
            character = pgm_read_byte(flashText++);
        }
    }

    bool beginReply()
    {
        if (replyBuffer[replyReadIndex] != '\0')
        {
            return false;
        }

        replyReadIndex = 0;
        replyLength = 0;
        replyBuffer[0] = '\0';
        return true;
    }

    void respond(const char* flashText)
    {
        if (!beginReply())
        {
            return;
        }

        appendFlashText(flashText);
        appendCharacter('\n');
    }

    void error(const char* flashMessage)
    {
        if (!beginReply())
        {
            return;
        }

        appendFlashText(PSTR("{\"kind\":\"error\",\"message\":\""));
        appendFlashText(flashMessage);
        appendFlashText(PSTR("\"}\n"));
    }

    void acknowledge(uint16_t commandSequence)
    {
        if (!beginReply())
        {
            return;
        }

        appendFlashText(PSTR("{\"kind\":\"sent\",\"sequence\":"));

        // Et 16-bit sekvensnummer har maksimalt fem desimalsifre.
        char digits[5];
        uint8_t digitCount = 0;

        do
        {
            digits[digitCount++] = '0' + commandSequence % 10;
            commandSequence /= 10;
        } while (commandSequence != 0);

        while (digitCount > 0)
        {
            appendCharacter(digits[--digitCount]);
        }

        appendFlashText(PSTR("}\n"));
    }

    bool arguments(long* values, uint8_t count)
    {
        for (uint8_t i = 0; i < count; i++)
        {
            char* text = strtok(nullptr, " ");
            if (!text)
            {
                return false;
            }
            char* end;
            values[i] = strtol(text, &end, 10);
            if (*end || values[i] < -32768 || values[i] > int32_t(cfg::MaxTestMs))
            {
                return false;
            }
        }
        return strtok(nullptr, " ") == nullptr;
    }

    void dispatch(char* text)
    {
        char* name = strtok(text, " ");
        if (!name)
        {
            return;
        }
        if (!strcmp_P(name, PSTR("PING")))
        {
            control_runtime::keepAlive();
            respond(PSTR("{\"kind\":\"pong\"}"));
            return;
        }
        if (!strcmp_P(name, PSTR("HELLO")))
        {
            control_runtime::keepAlive();
            respond(PSTR("{\"kind\":\"hello\",\"protocol\":2}"));
            return;
        }
        if (!strcmp_P(name, PSTR("POLL")))
        {
            if (sendingTelemetry || replyBuffer[replyReadIndex])
            {
                error(PSTR("Previous response pending"));
                return;
            }
            control_runtime::keepAlive();
            snapshot = control_runtime::captureSnapshot();
            sideSnapshot = side_link::latest();
            sideAgeMs = side_link::ageMs(micros());
            sideFresh = side_link::fresh(micros());
            prefixAt = 0;
            telemetryByteIndex = 0;
            telemetryChecksum = 0;
            writingLowNibble = false;
            sendingTelemetry = true;
            return;
        }
        proto::Command operation;
        uint8_t payload[proto::PayloadSize] = {};
        uint8_t payloadSize = 0;
        long argumentValues[5];
        if (!strcmp_P(name, PSTR("STOP")))
        {
            operation = proto::Command::Stop;
        }
        else if (!strcmp_P(name, PSTR("ARM")))
        {
            operation = proto::Command::Arm;
        }
        else if (!strcmp_P(name, PSTR("CLEAR")))
        {
            operation = proto::Command::Clear;
        }
        else if (!strcmp_P(name, PSTR("CALIBRATE")) || !strcmp_P(name, PSTR("SENSORS")) ||
                 !strcmp_P(name, PSTR("TIMING")) || !strcmp_P(name, PSTR("FOLLOW")))
        {
            if (!strcmp_P(name, PSTR("CALIBRATE")))
            {
                operation = proto::Command::Calibrate;
            }
            else if (!strcmp_P(name, PSTR("SENSORS")))
            {
                operation = proto::Command::Sensors;
            }
            else if (!strcmp_P(name, PSTR("TIMING")))
            {
                operation = proto::Command::Timing;
            }
            else
            {
                operation = proto::Command::Follow;
            }
            if (!arguments(argumentValues, 1) || argumentValues[0] < 1)
            {
                error(PSTR("Expected duration_ms"));
                return;
            }
            proto::put32(payload, argumentValues[0]);
            payloadSize = 4;
        }
        else if (!strcmp_P(name, PSTR("MOTOR")))
        {
            operation = proto::Command::Motor;
            if (!arguments(argumentValues, 3) || argumentValues[0] < -255 ||
                argumentValues[0] > 255 || argumentValues[1] < -255 || argumentValues[1] > 255 ||
                argumentValues[2] < 1)
            {
                error(PSTR("Expected left right duration_ms"));
                return;
            }
            proto::put16(payload, uint16_t(argumentValues[0]));
            proto::put16(payload + 2, uint16_t(argumentValues[1]));
            proto::put32(payload + 4, argumentValues[2]);
            payloadSize = 8;
        }
        else if (!strcmp_P(name, PSTR("SWEEP")))
        {
            operation = proto::Command::Sweep;
            if (!arguments(argumentValues, 5) || argumentValues[0] < 1 || argumentValues[0] > 3 ||
                argumentValues[1] < -255 || argumentValues[1] > 255 || argumentValues[2] < -255 ||
                argumentValues[2] > 255 || argumentValues[3] < -255 || argumentValues[3] > 255 ||
                argumentValues[4] < 1)
            {
                error(PSTR("Expected side start end step dwell_ms"));
                return;
            }
            proto::put16(payload, uint16_t(argumentValues[1]));
            proto::put16(payload + 2, uint16_t(argumentValues[2]));
            proto::put16(payload + 4, uint16_t(argumentValues[3]));
            proto::put16(payload + 6, argumentValues[4]);
            payload[8] = argumentValues[0];
            payloadSize = 9;
        }
        else
        {
            error(PSTR("Unknown command"));
            return;
        }
        control_runtime::keepAlive();
        uint16_t commandSequence = ++sequence;
        if (operation == proto::Command::Stop)
        {
            control_runtime::requestStop(commandSequence);
        }
        else if (!control_runtime::enqueueCommand(operation, commandSequence, payload, payloadSize))
        {
            error(PSTR("Command pending"));
            return;
        }
        acknowledge(commandSequence);
    }

    // Rapportformat S2: to antall, lokalt målebilde, sidemetadata, ytterverdier,
    // fysiske indekser og CRC8. Hver byte sendes som to heksadesimale tegn.
    // Python gir feltene navn og lager lesbar JSON/CSV; Nano trenger ingen printf.
    constexpr uint16_t TelemetryPayloadSize =
        2 + control_runtime::SnapshotSize + 7 + 3 * sidecfg::Count + cfg::SensorCount;

    uint8_t byteOf(uint16_t value, uint8_t byteIndex)
    {
        return byteIndex == 0 ? uint8_t(value) : uint8_t(value >> 8);
    }

    uint8_t telemetryByte(uint16_t index)
    {
        if (index == 0)
        {
            return cfg::SensorCount;
        }

        if (index == 1)
        {
            return sidecfg::Count;
        }

        index -= 2;

        if (index < control_runtime::SnapshotSize)
        {
            return snapshot[index];
        }

        index -= control_runtime::SnapshotSize;

        if (index < 2)
        {
            return byteOf(sideSnapshot.sequence, index);
        }

        if (index < 4)
        {
            return byteOf(sideAgeMs, index - 2);
        }

        if (index < 6)
        {
            return byteOf(sideSnapshot.scanUs, index - 4);
        }

        if (index == 6)
        {
            return (sideFresh ? 1 : 0) | (sideSnapshot.valid ? 2 : 0);
        }

        index -= 7;

        if (index < 2 * sidecfg::Count)
        {
            return byteOf(sideSnapshot.raw[index / 2], index % 2);
        }

        index -= 2 * sidecfg::Count;

        if (index < sidecfg::Count)
        {
            return sidecfg::Inputs[index].physicalIndex;
        }

        return sidecfg::MainIndices[index - sidecfg::Count];
    }

    uint8_t updateChecksum(uint8_t checksum, uint8_t value)
    {
        checksum ^= value;

        for (uint8_t bit = 0; bit < 8; ++bit)
        {
            checksum = (checksum & 0x80) ? uint8_t((checksum << 1) ^ 0x07) : uint8_t(checksum << 1);
        }

        return checksum;
    }

    char hexDigit(uint8_t nibble)
    {
        return nibble < 10 ? '0' + nibble : 'A' + nibble - 10;
    }

    // Maksimalt 16 tegn per kall. Ved full UART-buffer returnerer vi umiddelbart,
    // slik at logging aldri venter på at hele målebildet skal sendes.
    void output()
    {
        for (uint8_t sent = 0; sent < 16 && Serial.availableForWrite() > 0; ++sent)
        {
            if (sendingTelemetry)
            {
                if (prefixAt < 3)
                {
                    Serial.write("S2:"[prefixAt++]);
                }
                else if (telemetryByteIndex <= TelemetryPayloadSize)
                {
                    if (!writingLowNibble)
                    {
                        if (telemetryByteIndex == TelemetryPayloadSize)
                        {
                            pendingTelemetryByte = telemetryChecksum;
                        }
                        else
                        {
                            pendingTelemetryByte = telemetryByte(telemetryByteIndex);
                            telemetryChecksum =
                                updateChecksum(telemetryChecksum, pendingTelemetryByte);
                        }

                        Serial.write(hexDigit(pendingTelemetryByte >> 4));
                        writingLowNibble = true;
                    }
                    else
                    {
                        Serial.write(hexDigit(pendingTelemetryByte & 0x0F));
                        writingLowNibble = false;
                        ++telemetryByteIndex;
                    }
                }
                else
                {
                    Serial.write('\n');
                    sendingTelemetry = false;
                }
            }
            else if (replyBuffer[replyReadIndex] != '\0')
            {
                Serial.write(replyBuffer[replyReadIndex++]);
            }
            else
            {
                break;
            }
        }
    }

    void service()
    {
        output();
        for (uint8_t i = 0; i < 16 && Serial.available(); i++)
        {
            char c = Serial.read();
            if (c == '\r')
            {
                continue;
            }
            if (c == '\n')
            {
                if (commandOverflow)
                {
                    error(PSTR("Command too long"));
                }
                else
                {
                    commandBuffer[commandLength] = 0;
                    dispatch(commandBuffer);
                }
                commandLength = 0;
                commandOverflow = false;
            }
            else if (commandLength < sizeof(commandBuffer) - 1)
            {
                commandBuffer[commandLength++] = c;
            }
            else
            {
                commandOverflow = true;
            }
        }
    }
} // namespace host

static_assert(sizeof(sidecfg::MainIndices) == cfg::SensorCount,
              "Main index list must match local sensor count");

void setup()
{
    control_runtime::begin();
    Serial.begin(cfg::SerialBaud);
}

void loop()
{
    control_runtime::tick();
    if (control_runtime::hasTimeFor(cfg::CommandSlackUs))
    {
        host::service();
    }
    if (control_runtime::hasTimeFor(sidecfg::ServiceSlackUs))
    {
        side_link::service(micros());
    }
}
