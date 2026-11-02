#pragma once

// ESP32s task-watchdog overvåker Arduino-løkken. Kontrollbudsjettet på 6 ms
// måles separat; watchdog er ikke en avbryter for den kortere tidsfristen.
namespace platform_runtime
{
    bool beginWatchdog();
    void feedWatchdog();
} // namespace platform_runtime
