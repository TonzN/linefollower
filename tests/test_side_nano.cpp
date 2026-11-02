#include <cassert>
#include <iostream>
#include <vector>
#include "Arduino.h"
#include "Wire.h"
uint32_t fakeMicros=0;
int fakePinValues[22]={};
FakeWire Wire;
std::vector<int> reads;
int analogRead(int pin) {
    reads.push_back(pin);fakeMicros+=100;
    int selected=fakePinValues[2]+2*fakePinValues[3]+4*fakePinValues[4]+8*fakePinValues[5];
    return pin==20?111:selected*100+(pin==15?1:0);
}
#include "../src/side_nano.cpp"
int main() {
    setup();assert(ready);assert(!haveSample);
    loop();loop();assert(!haveSample);loop();assert(haveSample);
    assert(reads.size()==9 && completed.raw[0]==111 && completed.raw[1]==300 && completed.raw[2]==701);
    assert(completed.scanUs==940);
    uint8_t command[3]={sideproto::Latch,7,0};Wire.deliver(command,3);loop();
    request();sideproto::Sample frozen;
    assert(sideproto::accept(Wire.outgoing.data(),32,0,7,frozen));
    assert(frozen.valid && frozen.raw[2]==701 && frozen.ageMs==1);
    auto saved=Wire.outgoing;fakeMicros=8000;loop();request();assert(Wire.outgoing==saved);
    // Ny latch midt i neste skann må fremdeles gi forrige komplette skann.
    command[1]=8;Wire.deliver(command,3);loop();request();
    assert(sideproto::accept(Wire.outgoing.data(),32,0,8,frozen));
    assert(frozen.scanUs==940 && frozen.ageMs>=8);
    std::cout << "Actual side firmware: direct/mux ADC, channel selection, averaging and frozen complete scan OK\n";
}
