#pragma once
#include "protocol.h"

// Eget 32-byte format for ytterkortet. 9 verdier per side, maksimalt 3 sider.
// Latch fryser ett helt sett. Alle sider må ha samme sekvensnummer.
namespace sideproto
{
    constexpr uint8_t Magic = 0xB6;
    constexpr uint8_t Version = 1;
    constexpr uint8_t Capacity = 25;
    constexpr uint8_t PerPage = 9;
    constexpr uint8_t PageCount = (Capacity + PerPage - 1) / PerPage;
    constexpr uint8_t Latch = 0;
    constexpr uint8_t Select = 1;

    struct Sample
    {
        uint16_t sequence = 0;
        uint16_t ageMs = 65535;
        uint16_t scanUs = 0;
        uint8_t count = 0;
        bool valid = false;
        uint16_t raw[Capacity] = {};
    };

    inline void page(uint8_t* destination, const Sample& sample, uint8_t selected)
    {
        memset(destination, 0, 32);
        destination[0] = Magic;
        destination[1] = Version;
        destination[2] = selected;
        proto::put16(destination + 3, sample.sequence);
        destination[5] = sample.count;
        destination[6] = sample.valid;
        proto::put16(destination + 8, sample.scanUs);
        proto::put16(destination + 10, sample.ageMs);
        for (uint8_t i = 0; i < PerPage; i++)
        {
            uint8_t index = selected * PerPage + i;
            if (index < sample.count && index < Capacity)
            {
                proto::put16(destination + 12 + 2 * i, sample.raw[index]);
            }
        }
        destination[31] = proto::crc8(destination, 31);
    }

    inline bool
    accept(const uint8_t* packet, uint8_t size, uint8_t selected, uint16_t sequence, Sample& sample)
    {
        if (size != 32 || packet[0] != Magic || packet[1] != Version || packet[2] != selected ||
            selected >= PageCount || proto::get16(packet + 3) != sequence || packet[5] < 1 ||
            packet[5] > Capacity || proto::crc8(packet, 31) != packet[31])
        {
            return false;
        }
        if (selected == 0)
        {
            sample = Sample();
            sample.sequence = sequence;
            sample.count = packet[5];
            sample.valid = packet[6] == 1;
            sample.scanUs = proto::get16(packet + 8);
            sample.ageMs = proto::get16(packet + 10);
        }
        else if (sample.count != packet[5] || sample.valid != (packet[6] == 1) ||
                 sample.scanUs != proto::get16(packet + 8) ||
                 sample.ageMs != proto::get16(packet + 10))
        {
            return false;
        }
        for (uint8_t i = 0; i < PerPage; i++)
        {
            uint8_t index = selected * PerPage + i;
            if (index < sample.count)
            {
                sample.raw[index] = proto::get16(packet + 12 + 2 * i);
            }
        }
        return true;
    }

    // Alder regnes konservativt fra skannestart, ikke bare siste ADC-avlesning.
    inline uint16_t age(const Sample& sample, uint32_t transferAt, uint32_t now)
    {
        return proto::saturated16(uint32_t(sample.ageMs) +
                                  (uint32_t(now - transferAt) + 999) / 1000);
    }
} // namespace sideproto
