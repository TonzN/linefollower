#pragma once
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <cstdarg>
#include <cstdio>
#include <cassert>
#define PSTR(s) s
#define pgm_read_byte(address) (*(address))
#define strcpy_P strcpy
#define strcmp_P strcmp

inline int snprintf_P(char* dst, size_t size, const char* format, ...)
{
    std::string f = format;
    size_t pos = 0;
    while ((pos = f.find("%S", pos)) != std::string::npos)
    {
        f.replace(pos, 2, "%s");
    }
    va_list args;
    va_start(args, format);
    int result = vsnprintf(dst, size, f.c_str(), args);
    va_end(args);
    return result;
}

struct FakeSerial
{
    std::string incoming, outgoing;
    size_t at = 0;
    int space = 64;

    void begin(uint32_t)
    {
    }

    int available()
    {
        return int(incoming.size() - at);
    }

    char read()
    {
        return incoming[at++];
    }

    int availableForWrite()
    {
        return space;
    }

    void write(char value)
    {
        assert(space > 0);
        --space;
        outgoing += value;
    }
};

extern FakeSerial Serial;
extern uint32_t fakeMicros;
#define HIGH 1
#define LOW 0
#define OUTPUT 1
extern int fakePinValues[40];

inline void pinMode(int, int)
{
}

inline void digitalWrite(int pin, int value)
{
    fakePinValues[pin] = value;
}

inline void delayMicroseconds(uint16_t us)
{
    fakeMicros += us;
}

int analogRead(int pin);

inline uint32_t micros()
{
    return fakeMicros;
}

inline uint32_t millis()
{
    return fakeMicros / 1000;
}

template <class T> inline T min(T a, T b)
{
    return a < b ? a : b;
}

template <class T> inline T max(T a, T b)
{
    return a > b ? a : b;
}

#define constrain(v, lo, hi) ((v) < (lo) ? (lo) : ((v) > (hi) ? (hi) : (v)))

#define INPUT 0
#define INPUT_PULLUP 2
#define RISING 1
#define IRAM_ATTR
#define ADC_11db 3
using portMUX_TYPE = int;
#define portMUX_INITIALIZER_UNLOCKED 0
#define portENTER_CRITICAL(lock) ((void)(lock))
#define portEXIT_CRITICAL(lock) ((void)(lock))
#define portENTER_CRITICAL_ISR(lock) ((void)(lock))
#define portEXIT_CRITICAL_ISR(lock) ((void)(lock))
extern uint32_t fakePwmDuty[2];

inline void analogReadResolution(int)
{
}

inline void analogSetAttenuation(int)
{
}

inline double ledcSetup(int, uint32_t frequency, uint8_t)
{
    return frequency;
}

inline void ledcAttachPin(int, int)
{
}

inline void ledcWrite(int channel, uint32_t duty)
{
    fakePwmDuty[channel] = duty;
}

inline void attachInterrupt(int, void (*)(), int)
{
}

inline void delay(uint32_t ms)
{
    fakeMicros += 1000 * ms;
}

uint32_t analogReadMilliVolts(int pin);

namespace platform_runtime
{
    inline bool watchdogOk = true;

    inline bool beginWatchdog()
    {
        return watchdogOk;
    }

    inline void feedWatchdog()
    {
    }
} // namespace platform_runtime
