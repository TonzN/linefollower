#pragma once
#include <stdint.h>
#include <stddef.h>
#include <vector>

struct FakeWire
{
    void (*receive)(int) = nullptr;
    void (*request)() = nullptr;
    std::vector<uint8_t> incoming, outgoing;
    size_t index = 0;
    std::vector<std::vector<uint8_t>> responses;
    bool timeout = false;
    bool nack = false;
    unsigned transfers = 0;

    void begin()
    {
    }

    void begin(uint8_t)
    {
    }

    void setClock(uint32_t)
    {
    }

    void setWireTimeout(uint32_t, bool)
    {
    }

    bool getWireTimeoutFlag()
    {
        return timeout;
    }

    void clearWireTimeoutFlag()
    {
        timeout = false;
    }

    void beginTransmission(uint8_t)
    {
    }

    uint8_t endTransmission()
    {
        ++transfers;
        return nack ? 1 : 0;
    }

    uint8_t requestFrom(uint8_t, uint8_t)
    {
        ++transfers;
        incoming.clear();
        index = 0;
        if (!responses.empty())
        {
            incoming = responses.front();
            responses.erase(responses.begin());
        }
        return uint8_t(incoming.size());
    }

    void onReceive(void (*fn)(int))
    {
        receive = fn;
    }

    void onRequest(void (*fn)())
    {
        request = fn;
    }

    int available()
    {
        return int(incoming.size() - index);
    }

    int read()
    {
        return incoming[index++];
    }

    size_t write(const uint8_t* p, size_t n)
    {
        outgoing.assign(p, p + n);
        return n;
    }

    void deliver(const uint8_t* p, size_t n)
    {
        incoming.assign(p, p + n);
        index = 0;
        receive(n);
    }
};

extern FakeWire Wire;
