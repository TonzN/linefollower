#include <cassert>
#include <cstring>
#include <iostream>
#include "protocol.h"
#include "runtime_core.h"
#include "linefollow.h"
#include "conf.h"

int main()
{
    uint8_t frame[32];
    uint8_t payload[] = {1, 2, 3};
    proto::frame(frame, proto::Command::Motor, 123, payload, 3);
    assert(proto::valid(frame, 32));
    assert(proto::get16(frame + 3) == 123);
    for (uint8_t i = 0; i < 32; i++)
    {
        frame[i] ^= 1;
        assert(!proto::valid(frame, 32));
        frame[i] ^= 1;
    }
    assert(!proto::valid(frame, 31));
    uint8_t snapshot[proto::SnapshotSize];
    uint8_t pages[proto::PageCount][32];
    uint8_t joined[proto::SnapshotSize];
    for (uint16_t i = 0; i < sizeof(snapshot); i++)
    {
        snapshot[i] = i;
    }
    proto::makePages(snapshot, 65535, pages);
    for (uint8_t page = 0; page < proto::PageCount; page++)
    {
        assert(proto::valid(pages[page], 32));
        assert(proto::get16(pages[page] + 3) == 65535);
        for (uint8_t i = 0; i < proto::PageData; i++)
        {
            uint16_t offset = uint16_t(page) * proto::PageData + i;
            if (offset < sizeof(snapshot))
            {
                joined[offset] = pages[page][7 + i];
            }
        }
    }
    assert(!memcmp(snapshot, joined, sizeof(snapshot)));
    runtime::Scheduler schedule(100);
    assert(!schedule.due(99));
    assert(schedule.due(100));
    assert(schedule.take(100, 8) == 0);
    assert(schedule.take(129, 8) == 21);
    assert(schedule.skipped == 2);
    assert(schedule.next == 132);
    schedule = runtime::Scheduler(0xFFFFFFF8);
    assert(schedule.due(8));
    assert(schedule.take(8, 8) == 16);
    assert(schedule.next == 16);
    runtime::Sweep sweep;
    sweep.start = 0;
    sweep.end = 12;
    sweep.step = 5;
    sweep.dwell = 100;
    sweep.side = 1;
    assert(sweep.valid(120, 30000));
    assert(sweep.totalMs() == 400);
    sweep.index = 3;
    assert(sweep.value() == 12);
    sweep.start = 0;
    sweep.end = -100;
    sweep.step = -5;
    assert(sweep.valid(120, 30000));
    sweep.index = 1;
    assert(sweep.value() == -5);
    sweep.step = 5;
    assert(!sweep.valid(120, 30000));
    sweep.step = 0;
    assert(!sweep.valid(120, 30000));
    sweep.start = -32768;
    assert(!sweep.valid(120, 30000));
    uint16_t values[SensorCount] = {};
    assert(!analyzeLine(values).valid);
    values[0] = 950;
    auto left = analyzeLine(values);
    assert(left.valid && left.error == -CENTER);
    values[0] = 0;
    values[SensorCount - 1] = 950;
    auto right = analyzeLine(values);
    assert(right.valid && right.error == cfg::SensorPositionSpan - CENTER);
    // Den nye linjen dekker etter oppgitt observasjon 4–5 lesere, ikke bare to.
    for (uint8_t i = 0; i < SensorCount; i++)
    {
        values[i] = i < 5 ? 950 : 0;
    }
    assert(analyzeLine(values).valid);
    LineController controller;
    for (uint32_t now = 0; now < 500; now += 8)
    {
        LineReading r;
        r.valid = r.strong = true;
        r.quality = 'V';
        controller.update(r, now);
    }
    assert(controller.mode() == 'T' && controller.left > 0 && controller.right > 0);
    for (uint32_t now = 504; now < 900; now += 8)
    {
        controller.update(LineReading(), now);
    }
    assert(controller.recovering);
    controller.stop();
    controller.update(right, 908);
    assert(controller.left == 0 && controller.stopped);
    std::cout << "Protocol, CRC, snapshot pages, scheduler rollover, sweep and controller OK\n";
}
