#pragma once
#include <stdint.h>
#include "protocol.h"

namespace runtime
{
    inline uint32_t age(uint32_t now, uint32_t before)
    {
        return now - before;
    }

    // Neste tidspunkt følger en fast tidsakse. Vi hopper over tapte perioder
    // i stedet for å kjøre mange gamle kontrollrunder rett etter hverandre.
    struct Scheduler
    {
        uint32_t next = 0;
        uint32_t skipped = 0;

        explicit Scheduler(uint32_t start = 0) : next(start)
        {
        }

        bool due(uint32_t now) const
        {
            // Fortegnet differanse håndterer micros()-omløp for korte tidsavstander.
            return int32_t(now - next) >= 0;
        }

        uint32_t take(uint32_t now, uint32_t period)
        {
            uint32_t late = now - next;
            uint32_t lost = late / period;
            skipped += lost;
            next += (lost + 1) * period;
            return late;
        }
    };

    // Et tidsbegrenset PWM-forsøk, ikke en motor-/kraftmodell.
    struct Sweep
    {
        int16_t start = 0;
        int16_t end = 0;
        int16_t step = 1;
        uint16_t dwell = 1; // Millisekunder per testtrinn.
        uint16_t index = 0;
        uint8_t side = 0;

        uint32_t totalMs() const
        {
            uint16_t distance = start > end ? start - end : end - start;
            uint16_t increment = step < 0 ? -step : step;
            if (!increment || !dwell)
            {
                return 0;
            }
            return (uint32_t((distance + increment - 1) / increment) + 1) * dwell;
        }

        bool valid(int16_t pwmLimit, uint32_t durationLimit) const
        {
            if (side < 1 || side > 3 || step == 0 || !dwell)
            {
                return false;
            }
            if (start > pwmLimit || start < -int16_t(pwmLimit) || end > pwmLimit ||
                end < -int16_t(pwmLimit))
            {
                return false;
            }
            if ((end > start && step < 0) || (end < start && step > 0))
            {
                return false;
            }
            return totalMs() > 0 && totalMs() <= durationLimit;
        }

        int16_t value() const
        {
            int32_t requestedValue = int32_t(start) + int32_t(index) * step;

            if (step > 0 && requestedValue > end)
            {
                return end;
            }

            if (step <= 0 && requestedValue < end)
            {
                return end;
            }

            return int16_t(requestedValue);
        }
    };
} // namespace runtime
