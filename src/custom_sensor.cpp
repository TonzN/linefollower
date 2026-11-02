#include "hardware.h"

// Ingen gjetning om grensesnittet til den nye sensorrekken.
// Ved Custom-backend implementeres en avgrenset råavlesning her. Returner false
// ved feil. raw[] skal ordnes venstre -> høyre; maximum() må angi råskalaen.
namespace custom_sensor
{
    bool begin()
    {
        return false;
    }

    bool read(uint16_t*, uint8_t)
    {
        return false;
    }

    uint16_t maximum()
    {
        return 1000;
    }
} // namespace custom_sensor
