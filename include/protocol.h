#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>

// Eksplisitt byteformat: ingen delte C++-structer eller plattformavhengig padding.
namespace proto
{
    constexpr uint8_t Version = 1;
    constexpr uint8_t Magic = 0xA5;
    constexpr uint8_t FrameSize = 32;
    constexpr uint8_t PayloadSize = 25;
    constexpr uint8_t SensorCapacity = 31;
    constexpr uint8_t HeaderSize = 64;
    enum class Command : uint8_t
    {
        SelectPage = 0,
        Stop = 1,
        Arm = 2,
        Clear = 3,
        Calibrate = 4,
        Sensors = 5,
        Timing = 6,
        Motor = 7,
        Sweep = 8,
        Follow = 9,
        Latch = 10,
        Heartbeat = 11
    };
    enum class Mode : uint8_t
    {
        Idle = 0,
        Calibrating = 1,
        Sensors = 2,
        Timing = 3,
        Motor = 4,
        Sweep = 5,
        Follow = 6
    };
    enum class Fault : uint8_t
    {
        None = 0,
        CycleBudget = 1,
        LateStart = 2,
        LinkLost = 3,
        SensorRead = 4,
        Configuration = 5,
        Watchdog = 6
    };
    enum class Result : uint8_t
    {
        Ok = 0,
        BadCommand = 1,
        BadArgument = 2,
        NotConfigured = 3,
        NotArmed = 4,
        NotCalibrated = 5,
        Busy = 6,
        FaultLatched = 7
    };

    enum Flags : uint16_t
    {
        SensorReady = 1,
        Calibrated = 2,
        MotorReady = 4,
        Armed = 8,
        VoltageValid = 16,
        CurrentValid = 32,
        LeftRpmValid = 64,
        RightRpmValid = 128,
        TestComplete = 256,
        DroppedCommand = 512,
        SampleValid = 1024
    };

    // Tall lagres little endian: minst signifikante byte først.
    inline void put16(uint8_t* bytes, uint16_t value)
    {
        bytes[0] = value;
        bytes[1] = value >> 8;
    }

    inline uint16_t get16(const uint8_t* bytes)
    {
        return uint16_t(bytes[0]) | (uint16_t(bytes[1]) << 8);
    }

    inline void put32(uint8_t* bytes, uint32_t value)
    {
        put16(bytes, value);
        put16(bytes + 2, value >> 16);
    }

    inline uint32_t get32(const uint8_t* bytes)
    {
        return get16(bytes) | (uint32_t(get16(bytes + 2)) << 16);
    }

    inline uint8_t crc8(const uint8_t* data, size_t size)
    {
        uint8_t crc = 0;
        while (size--)
        {
            crc ^= *data++;
            for (uint8_t i = 0; i < 8; i++)
            {
                crc = (crc & 0x80) ? uint8_t((crc << 1) ^ 0x07) : uint8_t(crc << 1);
            }
        }
        return crc;
    }

    inline void frame(uint8_t* destination,
                      Command command,
                      uint16_t sequence,
                      const uint8_t* payload = nullptr,
                      uint8_t size = 0)
    {
        memset(destination, 0, FrameSize);
        destination[0] = Magic;
        destination[1] = Version;
        destination[2] = uint8_t(command);
        put16(destination + 3, sequence);
        destination[5] = size;
        if (payload && size <= PayloadSize)
        {
            memcpy(destination + 6, payload, size);
        }
        destination[31] = crc8(destination, 31);
    }

    inline bool valid(const uint8_t* data, uint8_t size)
    {
        return size == FrameSize && data[0] == Magic && data[1] == Version &&
               data[5] <= PayloadSize && crc8(data, 31) == data[31];
    }

    inline uint16_t saturated16(uint32_t x)
    {
        return x > 65535 ? 65535 : uint16_t(x);
    }

} // namespace proto
