#define LINEFOLLOW_SIDE_ENABLED 1
#include <cassert>
#include <iostream>
#include "Arduino.h"
#include "Wire.h"
uint32_t fakeMicros = 0;
FakeWire Wire;
#include "../src/side_link.cpp"

void reply(const sideproto::Sample& sample, uint16_t seq)
{
    auto s = sample;
    s.sequence = seq;
    uint8_t p[32];
    sideproto::page(p, s, 0);
    Wire.responses.push_back(std::vector<uint8_t>(p, p + 32));
}

int main()
{
    sideproto::Sample s;
    s.count = sidecfg::Count;
    s.valid = true;
    s.ageMs = 2;
    s.scanUs = 500;
    s.raw[1] = 987;
    assert(!side_link::fresh(0));
    side_link::service(0);
    side_link::service(100);
    reply(s, 1);
    side_link::service(200);
    assert(side_link::fresh(200) && side_link::latest().raw[1] == 987);
    assert(!side_link::fresh(50000));
    // Feil CRC/sekvens må ikke overskrive forrige komplette målesett.
    side_link::service(50000);
    side_link::service(50100);
    reply(s, 999);
    side_link::service(50200);
    assert(side_link::latest().sequence == 1 && !side_link::fresh(50200));
    side_link::service(70000);
    side_link::service(70100);
    s.valid = false;
    reply(s, 3);
    side_link::service(70200);
    assert(!side_link::fresh(70200));
    // Pakker over flere sider, også med sekvensnummer- og micros-omløp.
    s.count = 25;
    s.sequence = 65535;
    s.valid = true;
    s.ageMs = 1;
    for (uint8_t i = 0; i < 25; i++)
    {
        s.raw[i] = i * 40;
    }
    sideproto::Sample joined;
    uint8_t p[32];
    for (uint8_t i = 0; i < 3; i++)
    {
        sideproto::page(p, s, i);
        assert(sideproto::accept(p, 32, i, 65535, joined));
    }
    assert(joined.raw[24] == 960);
    assert(sideproto::age(joined, 0xFFFFFFF0, 0x3D8) == 2);
    sideproto::page(p, s, 0);
    p[12] ^= 1;
    assert(!sideproto::accept(p, 32, 0, 65535, joined));
    std::cout << "Side link: fresh/stale, invalid sequence/CRC, paged snapshots and rollover OK\n";
}
