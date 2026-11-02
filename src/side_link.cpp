#include <Arduino.h>
#include <Wire.h>
#include "side_config.h"
#include "side_link.h"

namespace {
sideproto::Sample cached,assembling;
uint32_t nextPoll=0,transferAt=0,cachedTransferAt=0;
uint16_t sequence=0;
uint8_t state=0,page=0;
bool haveSample=false;
bool send(const uint8_t* data,uint8_t size) {
    Wire.beginTransmission(sidecfg::Address);Wire.write(data,size);
    bool ok=Wire.endTransmission()==0;
    if(Wire.getWireTimeoutFlag()) {Wire.clearWireTimeoutFlag();ok=false;}
    return ok;
}
}
namespace side_link {
const sideproto::Sample& latest() {return cached;}
uint16_t ageMs(uint32_t now) {return haveSample?sideproto::age(cached,cachedTransferAt,now):65535;}
bool fresh(uint32_t now) {return haveSample && cached.valid && ageMs(now)<=sidecfg::MaxAgeMs;}
void service(uint32_t now) {
    if(!sidecfg::Enabled)return;
    if(state && uint32_t(now-transferAt)>50000)state=0;
    if(state==0) {
        if(int32_t(now-nextPoll)<0)return;
        uint8_t command[3]={sideproto::Latch};proto::put16(command+1,++sequence);
        transferAt=now;nextPoll=now+sidecfg::PollPeriodUs;page=0;
        if(send(command,3))state=1;
    } else if(state==1) {
        uint8_t select[2]={sideproto::Select,page};
        state=send(select,2)?2:0;
    } else {
        uint8_t packet[32],n=Wire.requestFrom(sidecfg::Address,uint8_t(32)),i=0;
        while(Wire.available() && i<32)packet[i++]=Wire.read();
        bool timeout=Wire.getWireTimeoutFlag();Wire.clearWireTimeoutFlag();
        if(timeout || n!=32 || !sideproto::accept(packet,i,page,sequence,assembling)) {state=0;return;}
        if(assembling.count!=sidecfg::Count) {state=0;return;}
        if(++page>=(assembling.count+sideproto::PerPage-1)/sideproto::PerPage) {
            cached=assembling;cachedTransferAt=transferAt;haveSample=true;state=0;
        } else state=1;
    }
}
}
