#include <cassert>
#include <iostream>
#include "board_pins.h"

int main()
{
    int safeAdcCount = 0;
    int exposedAdcCount = 0;

    for (int pin = 0; pin < 40; ++pin)
    {
        if (board_pins::isAdc(pin) && board_pins::isAvailable(pin, false))
        {
            ++safeAdcCount;
        }
        if (board_pins::isAdc(pin) && board_pins::isAvailable(pin, true))
        {
            ++exposedAdcCount;
        }
    }

    assert(safeAdcCount == 12 && exposedAdcCount == 16);
    assert(!board_pins::isAvailable(37, true));
    assert(!board_pins::isAvailable(38, true));
    assert(!board_pins::isAvailable(6, true));
    assert(!board_pins::isAvailable(1, true));
    assert(board_pins::isAvailable(21, false)); // Ikke reservert for I2C.
    std::cout << "NodeMCU-32S: 12 non-strapping ADC pins, 16 exposed ADC pins OK\n";
}
