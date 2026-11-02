#include <Arduino.h>
#include <Wire.h>
#include <util/atomic.h>
#include <avr/wdt.h>
#include "side_config.h"
#include "side_protocol.h"

#if defined(__AVR__)
void sideWatchdogOff() __attribute__((naked, section(".init3")));

void sideWatchdogOff()
{
    MCUSR = 0;
    wdt_disable();
}
#endif

namespace
{
    bool ready = false;
    uint16_t working[sidecfg::Count] = {};
    sideproto::Sample completed;
    uint32_t begun = 0;
    uint32_t nextScan = 0;
    uint32_t completedStart = 0;
    uint8_t channel = 0;
    bool scanning = false;
    bool haveSample = false;
    volatile bool latchRequested = false;
    volatile uint16_t latchSequence = 0;
    volatile uint8_t selected = 0;
    uint8_t pages[sideproto::PageCount][32];

    bool analogPin(int8_t pin)
    {
        return pin >= 14 && pin <= 21 && pin != 18 && pin != 19;
    }

    // Sjekk hele oppsettet før GPIO-utganger tas i bruk.
    bool configure()
    {
        if (!sidecfg::Enabled)
        {
            return false;
        }
        bool used[22] = {};
        used[19] = true;
        used[18] = true;
        used[1] = true;
        used[0] = true;
        bool muxUsed[2] = {};
        for (uint8_t i = 0; i < sidecfg::Count; i++)
        {
            const auto& in = sidecfg::Inputs[i];
            if (in.physicalIndex >= sidecfg::PhysicalCount ||
                (i && in.physicalIndex <= sidecfg::Inputs[i - 1].physicalIndex))
            {
                return false;
            }
            for (uint8_t index : sidecfg::MainIndices)
            {
                if (index == in.physicalIndex)
                {
                    return false;
                }
            }
            if (in.mux == -1)
            {
                if (!analogPin(in.pin) || used[in.pin])
                {
                    return false;
                }
                used[in.pin] = true;
            }
            else
            {
                if (in.mux < 0 || in.mux > 1 || in.channel > 15)
                {
                    return false;
                }
                muxUsed[in.mux] = true;
                for (uint8_t j = 0; j < i; j++)
                {
                    if (sidecfg::Inputs[j].mux == in.mux &&
                        sidecfg::Inputs[j].channel == in.channel)
                    {
                        return false;
                    }
                }
            }
        }
        for (uint8_t bank = 0; bank < 2; bank++)
        {
            if (muxUsed[bank])
            {
                int8_t pin = sidecfg::MuxSignalPins[bank];
                if (!analogPin(pin) || used[pin])
                {
                    return false;
                }
                used[pin] = true;
            }
        }
        if (muxUsed[0] || muxUsed[1])
        {
            for (int8_t pin : sidecfg::MuxSelectPins)
            {
                if (pin < 2 || pin > 17 || used[pin])
                {
                    return false;
                }
                used[pin] = true;
            }
            for (int8_t pin : sidecfg::MuxSelectPins)
            {
                pinMode(pin, OUTPUT);
                digitalWrite(pin, LOW);
            }
        }
        return true;
    }

    // I2C-avbruddet setter bare korte forespørsler; ADC-skann skjer i loop().
    void receive(int size)
    {
        uint8_t input[3];
        uint8_t n = 0;
        while (Wire.available())
        {
            uint8_t value = Wire.read();
            if (n < 3)
            {
                input[n++] = value;
            }
        }
        if (size == 3 && n == 3 && input[0] == sideproto::Latch)
        {
            latchSequence = proto::get16(input + 1);
            latchRequested = true;
            selected = 0;
        }
        else if (size == 2 && n == 2 && input[0] == sideproto::Select &&
                 input[1] < sideproto::PageCount)
        {
            selected = input[1];
        }
    }

    void request()
    {
        Wire.write(pages[selected], 32);
    }

    // Bygg nye sider før atomisk bytte, slik at en forespørsel ser et helt sett.
    void freeze(uint16_t sequence)
    {
        sideproto::Sample frozen = completed;
        frozen.sequence = sequence;
        frozen.count = sidecfg::Count;
        frozen.valid = ready && haveSample;
        frozen.ageMs = haveSample
                           ? proto::saturated16((uint32_t(micros() - completedStart) + 999) / 1000)
                           : 65535;
        uint8_t staged[sideproto::PageCount][32];
        for (uint8_t page = 0; page < sideproto::PageCount; page++)
        {
            sideproto::page(staged[page], frozen, page);
        }
        ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
        {
            memcpy(pages, staged, sizeof(pages));
        }
    }
} // namespace

void setup()
{
    ready = configure();
    freeze(0);
    Wire.begin(sidecfg::Address);
    Wire.onReceive(receive);
    Wire.onRequest(request);
    nextScan = micros();
    wdt_enable(WDTO_120MS);
}

void loop()
{
    wdt_reset();
    if (latchRequested)
    {
        uint16_t seq;
        ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
        {
            seq = latchSequence;
            latchRequested = false;
        }
        freeze(seq);
    }
    uint32_t now = micros();
    if (!ready)
    {
        return;
    }
    if (!scanning)
    {
        if (int32_t(now - nextScan) < 0)
        {
            return;
        }
        scanning = true;
        channel = 0;
        begun = now;
    }
    // Én kanal per loop: I2C-håndtering kan skje mellom ADC-avlesningene.
    const auto& input = sidecfg::Inputs[channel];
    int8_t pin = input.pin;
    if (input.mux >= 0)
    {
        for (uint8_t bit = 0; bit < 4; bit++)
        {
            digitalWrite(sidecfg::MuxSelectPins[bit], (input.channel >> bit) & 1);
        }
        pin = sidecfg::MuxSignalPins[input.mux];
        delayMicroseconds(sidecfg::SettleUs);
    }
    if (sidecfg::DiscardFirstAdc)
    {
        analogRead(pin);
    }
    uint32_t sum = 0;
    for (uint8_t i = 0; i < sidecfg::Samples; i++)
    {
        sum += analogRead(pin);
    }
    working[channel] = sum / sidecfg::Samples;
    if (++channel == sidecfg::Count)
    {
        memcpy(completed.raw, working, sizeof(working));
        completedStart = begun;
        completed.scanUs = proto::saturated16(micros() - begun);
        haveSample = true;
        scanning = false;
        nextScan = begun + sidecfg::ScanPeriodUs;
        if (int32_t(micros() - nextScan) >= 0)
        {
            nextScan = micros() + sidecfg::ScanPeriodUs;
        }
    }
}
