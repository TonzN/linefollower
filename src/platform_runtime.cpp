#include <esp_task_wdt.h>
#include "board_config.h"
#include "platform_runtime.h"

namespace
{
    bool watchingTask = false;
}

namespace platform_runtime
{
    bool beginWatchdog()
    {
        // PlatformIO-profilen låser Arduino-ESP32 2.0.17 / ESP-IDF 4.4.
        uint32_t timeoutSeconds = (cfg::WatchdogTimeoutMs + 999) / 1000;
        if (esp_task_wdt_init(timeoutSeconds, true) != ESP_OK)
        {
            return false;
        }

        watchingTask =
            esp_task_wdt_status(nullptr) == ESP_OK || esp_task_wdt_add(nullptr) == ESP_OK;
        return watchingTask;
    }

    void feedWatchdog()
    {
        if (watchingTask)
        {
            esp_task_wdt_reset();
        }
    }
} // namespace platform_runtime
