#pragma once
#include <stdint.h>
#include <stddef.h>
#include <vector>
struct FakeWire {
    void (*receive)(int)=nullptr;
    void (*request)()=nullptr;
    std::vector<uint8_t> incoming,outgoing;
    size_t index=0;
    void begin(uint8_t) {}
    void onReceive(void (*fn)(int)) {receive=fn;}
    void onRequest(void (*fn)()) {request=fn;}
    int available() {return int(incoming.size()-index);}
    int read() {return incoming[index++];}
    size_t write(const uint8_t* p,size_t n) {outgoing.assign(p,p+n);return n;}
    void deliver(const uint8_t* p,size_t n) {incoming.assign(p,p+n);index=0;receive(n);}
};
extern FakeWire Wire;
