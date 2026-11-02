#include <cassert>
#include <iostream>
#include "Arduino.h"
#include "Wire.h"
#include "hardware.h"
#include "protocol.h"
uint32_t fakeMicros=0;
FakeWire Wire;
namespace fake {bool sensor=true,motor=true,readOk=true;uint32_t readUs=200;uint16_t value=100;}
namespace hw {
int16_t appliedLeft=0,appliedRight=0,currentMa=0,leftRpm10=0,rightRpm10=0;
uint16_t measurementFlags=0,voltageMv=0,encoderAgeMs=0;
bool begin() {return true;}
bool sensorReady() {return fake::sensor;}
bool motorReady() {return fake::motor;}
bool readSensors(uint16_t* raw) {fakeMicros+=fake::readUs;for(uint8_t i=0;i<cfg::SensorCount;i++)raw[i]=fake::value;return fake::readOk;}
uint16_t rawMaximum() {return 2000;}
void motors(int16_t l,int16_t r) {appliedLeft=l;appliedRight=r;}
void stop() {motors(0,0);}
void updateMeasurements(uint32_t) {}
}
// Inkluder den faktiske Nano-runtime, ikke en ny implementasjon av den.
#include "../src/nano.cpp"

void reset() {
    fakeMicros=0;fake::sensor=fake::motor=fake::readOk=true;fake::value=100;fake::readUs=200;
    mode=Mode::Idle;fault=Fault::None;armed=calibratedOk=complete=sampleValid=false;
    pending=heartbeat=stopRequested=dropped=false;sampleNumber=sampleAt=lastCycleAt=0;
    missedDeadlines=0;lastHeartbeat=0;minimumCycle=minimumPeriod=65535;maximumCycle=maximumPeriod=0;
    controller=LineController();scheduler=runtime::Scheduler();hw::stop();
    memset(raw,0,sizeof(raw));memset(calibrated,0,sizeof(calibrated));
    memset(calibrationMin,0,sizeof(calibrationMin));memset(calibrationMax,0,sizeof(calibrationMax));
    setup();
}
Result invoke(Command op,const uint8_t* p=nullptr,uint8_t size=0) {
    uint8_t packet[32];proto::frame(packet,op,42,p,size);return command(packet);
}
Result motor(int16_t left,int16_t right,uint32_t ms=1000) {
    uint8_t p[8];proto::put16(p,left);proto::put16(p+2,right);proto::put32(p+4,ms);return invoke(Command::Motor,p,8);
}
int main() {
    reset();assert(motor(20,20)==Result::NotArmed);assert(invoke(Command::Arm)==Result::Ok);
    assert(motor(-100,-100)==Result::Ok);cycle(0);assert(hw::appliedLeft==-100);
    reset();invoke(Command::Arm);assert(motor(-32768,0)==Result::BadArgument);assert(motor(121,0)==Result::BadArgument);
    reset();fake::motor=false;assert(invoke(Command::Arm)==Result::NotConfigured);
    reset();invoke(Command::Arm);motor(20,20);fakeMicros=2000;cycle(fakeMicros);
    assert(fault==Fault::LateStart && hw::appliedLeft==0 && !armed);
    reset();mode=Mode::Sensors;testDuration=10000;fake::readUs=cfg::MaxCycleUs+1;cycle(0);
    assert(fault==Fault::CycleBudget && hw::appliedLeft==0);
    reset();mode=Mode::Sensors;fake::readOk=false;cycle(0);assert(fault==Fault::SensorRead);
    reset();invoke(Command::Arm);motor(20,20);fakeMicros=(cfg::NanoLeaseMs+1)*1000;loop();
    assert(fault==Fault::LinkLost && !armed && hw::appliedLeft==0);
    reset();invoke(Command::Arm);motor(20,20);cycle(0);
    uint8_t packet[32];proto::frame(packet,Command::Stop,99);Wire.deliver(packet,32);loop();
    assert(mode==Mode::Idle && !armed && hw::appliedLeft==0 && lastSequence==99);
    reset();mode=Mode::Sensors;testDuration=1000;cycle(0);publish(70);
    uint16_t saved=proto::get16(pages[2]+7+16); // cache er stabil også etter ny kontrollrunde.
    fake::value=900;fakeMicros=8000;cycle(8000);
    assert(proto::get16(pages[0]+3)==70);assert(proto::get16(pages[2]+7+16)==saved);
    publish(71);assert(proto::get16(pages[0]+3)==71);
    reset();uint8_t duration[4];proto::put32(duration,16);assert(invoke(Command::Calibrate,duration,4)==Result::Ok);
    cycle(0);fake::value=900;fakeMicros=8000;cycle(8000);fakeMicros=16000;cycle(16000);
    assert(mode==Mode::Idle && complete && calibratedOk && calibrated[0]==1000);
    reset();proto::put32(duration,8);invoke(Command::Calibrate,duration,4);cycle(0);fakeMicros=8000;cycle(8000);
    assert(complete && !calibratedOk);
    reset();uint8_t invalid[32]={};Wire.deliver(invalid,32);assert(!pending);
    std::cout << "Actual Nano runtime: motor gate, reverse bounds, deadlines, link lease, priority STOP, snapshot cache, calibration OK\n";
}
