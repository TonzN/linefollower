#pragma once
#include <stdint.h>

// Pinneegenskaper for NodeMCU-32S. GPIO37/38 finnes på brikken, men ikke headerne.
namespace board_pins
{
    inline bool isAdc(int pin)
    {
        return pin == 0 || pin == 2 || pin == 4 || (pin >= 12 && pin <= 15) ||
               (pin >= 25 && pin <= 27) || (pin >= 32 && pin <= 36) || pin == 39;
    }

    inline bool isBootPin(int pin)
    {
        return pin == 0 || pin == 2 || pin == 5 || pin == 12 || pin == 15;
    }

    inline bool isAvailable(int pin, bool allowBootPins)
    {
        // GPIO1/3 er USB-UART. 6..11 er flash. Andre hull er ikke kortpinner.
        bool exposed = pin == 0 || (pin >= 2 && pin <= 5) || (pin >= 12 && pin <= 19) ||
                       (pin >= 21 && pin <= 23) || (pin >= 25 && pin <= 27) ||
                       (pin >= 32 && pin <= 36) || pin == 39;
        return exposed && pin != 3 && (allowBootPins || !isBootPin(pin));
    }

    inline bool isOutput(int pin)
    {
        return pin >= 0 && pin < 34;
    }
} // namespace board_pins
