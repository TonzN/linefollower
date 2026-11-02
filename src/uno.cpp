#include <Arduino.h>
#include <Wire.h>
#include <stdlib.h>
#include <string.h>
#include "board_config.h"
#include "protocol.h"

namespace {
char input[96];uint8_t inputSize=0;bool inputOverflow=false;
uint16_t sequence=0;
uint32_t lastHost=0,lastHeartbeat=0;

bool send(proto::Command command,uint16_t seq,const uint8_t* payload=nullptr,uint8_t size=0) {
    uint8_t packet[proto::FrameSize];proto::frame(packet,command,seq,payload,size);
    Wire.beginTransmission(cfg::NanoAddress);Wire.write(packet,sizeof(packet));
    uint8_t result=Wire.endTransmission();
    if(Wire.getWireTimeoutFlag()) {Wire.clearWireTimeoutFlag();return false;}
    return result==0;
}
void heartbeat() {
    uint32_t now=millis();
    if(now-lastHost<=cfg::HostLeaseMs && now-lastHeartbeat>=cfg::HeartbeatMs) {
        send(proto::Command::Heartbeat,0);lastHeartbeat=now;
    }
}
bool readPage(uint8_t page,uint8_t* packet) {
    if(!send(proto::Command::SelectPage,0,&page,1))return false;
    uint8_t n=Wire.requestFrom(cfg::NanoAddress,proto::FrameSize);
    uint8_t i=0;while(Wire.available() && i<proto::FrameSize)packet[i++]=Wire.read();
    return n==proto::FrameSize && proto::valid(packet,i) && packet[2]==uint8_t(proto::Command::Latch) && packet[5]==proto::PayloadSize && packet[6]==page;
}
bool capture(uint8_t* snapshot) {
    uint16_t seq=++sequence;if(!send(proto::Command::Latch,seq))return false;
    uint32_t begun=millis();uint8_t packet[proto::FrameSize];
    while(true) {
        heartbeat();
        if(readPage(0,packet) && proto::get16(packet+3)==seq)break;
        if(millis()-begun>120)return false;
        delay(1);
    }
    for(uint8_t page=0;page<proto::PageCount;page++) {
        if(page && (!readPage(page,packet)||proto::get16(packet+3)!=seq))return false;
        for(uint8_t i=0;i<proto::PageData;i++) {
            uint16_t offset=uint16_t(page)*proto::PageData+i;
            if(offset<proto::SnapshotSize)snapshot[offset]=packet[7+i];
        }
    }
    return snapshot[46]>=2 && snapshot[46]<=proto::SensorCapacity;
}
void error(const __FlashStringHelper* reason) {Serial.print(F("{\"kind\":\"error\",\"message\":\""));Serial.print(reason);Serial.println(F("\"}"));}
void field(const __FlashStringHelper* name,uint32_t value) {Serial.print(F(",\""));Serial.print(name);Serial.print(F("\":"));Serial.print(value);}
void signedField(const __FlashStringHelper* name,int16_t value,bool valid=true) {Serial.print(F(",\""));Serial.print(name);Serial.print(F("\":"));if(valid)Serial.print(value);else Serial.print(F("null"));}
void telemetry(const uint8_t* s) {
    uint16_t flags=proto::get16(s+42);
    Serial.print(F("{\"kind\":\"sample\",\"sample\":"));Serial.print(proto::get32(s));
    field(F("time_us"),proto::get32(s+4));field(F("sensor_us"),proto::get16(s+8));field(F("cycle_us"),proto::get16(s+10));
    field(F("period_us"),proto::get16(s+12));field(F("lateness_us"),proto::get16(s+14));
    field(F("deadline_misses"),proto::get32(s+16));field(F("skipped_cycles"),proto::get32(s+20));
    field(F("min_cycle_us"),proto::get16(s+24));field(F("max_cycle_us"),proto::get16(s+26));
    signedField(F("error"),int16_t(proto::get16(s+28)),(flags&proto::Calibrated) && (flags&proto::SampleValid) && (s[47]=='V'||s[47]=='L'));
    signedField(F("left_pwm"),int16_t(proto::get16(s+30)));signedField(F("right_pwm"),int16_t(proto::get16(s+32)));
    Serial.print(F(",\"voltage_mv\":"));
    if(flags&proto::VoltageValid)Serial.print(proto::get16(s+34));else Serial.print(F("null"));
    signedField(F("current_ma"),int16_t(proto::get16(s+36)),flags&proto::CurrentValid);
    signedField(F("left_rpm_x10"),int16_t(proto::get16(s+38)),flags&proto::LeftRpmValid);
    signedField(F("right_rpm_x10"),int16_t(proto::get16(s+40)),flags&proto::RightRpmValid);
    field(F("flags"),flags);field(F("mode"),s[44]);field(F("fault"),s[45]);field(F("sensor_count"),s[46]);
    field(F("ack_sequence"),proto::get16(s+48));field(F("ack_result"),s[50]);
    field(F("min_period_us"),proto::get16(s+52));field(F("max_period_us"),proto::get16(s+54));field(F("link_age_ms"),proto::get16(s+56));
    field(F("raw_maximum"),proto::get16(s+58));field(F("step"),proto::get16(s+60));
    field(F("encoder_age_ms"),proto::get16(s+62));
    Serial.print(F(",\"quality\":\""));Serial.write(s[47]);
    Serial.print(F("\",\"control_mode\":\""));Serial.write(s[51]);Serial.print('"');
    for(uint8_t which=0;which<2;which++) {
        Serial.print(which?F(",\"calibrated\":["):F(",\"raw\":["));
        for(uint8_t i=0;i<s[46];i++) {
            if(i)Serial.print(',');
            if(!(flags&proto::SampleValid) || (which && !(flags&proto::Calibrated)))Serial.print(F("null"));
            else Serial.print(proto::get16(s+64+2*(i+which*s[46])));
        }
        Serial.print(']');
    }
    Serial.println('}');
}
bool arguments(long* values,uint8_t count) {
    for(uint8_t i=0;i<count;i++) {
        char* token=strtok(nullptr," ");if(!token)return false;
        char* end;values[i]=strtol(token,&end,10);if(*end)return false;
        if(values[i]<-32768 || values[i]>int32_t(cfg::MaxTestMs))return false;
    }
    return strtok(nullptr," ")==nullptr;
}
void command(char* text) {
    char* name=strtok(text," ");if(!name)return;lastHost=millis();
    if(!strcmp(name,"PING")) {Serial.println(F("{\"kind\":\"pong\"}"));return;}
    if(!strcmp(name,"HELLO")) {Serial.println(F("{\"kind\":\"hello\",\"protocol\":1}"));return;}
    if(!strcmp(name,"POLL")) {
        uint8_t snapshot[proto::SnapshotSize];
        if(capture(snapshot))telemetry(snapshot);else error(F("I2C snapshot failed"));return;
    }
    proto::Command op;uint8_t data[proto::PayloadSize]={},size=0;long v[5];
    if(!strcmp(name,"STOP"))op=proto::Command::Stop;
    else if(!strcmp(name,"ARM"))op=proto::Command::Arm;
    else if(!strcmp(name,"CLEAR"))op=proto::Command::Clear;
    else if(!strcmp(name,"CALIBRATE") || !strcmp(name,"SENSORS") || !strcmp(name,"TIMING") || !strcmp(name,"FOLLOW")) {
        op=!strcmp(name,"CALIBRATE")?proto::Command::Calibrate:!strcmp(name,"SENSORS")?proto::Command::Sensors:!strcmp(name,"TIMING")?proto::Command::Timing:proto::Command::Follow;
        if(!arguments(v,1) || v[0]<1) {error(F("Expected duration_ms"));return;}
        proto::put32(data,v[0]);size=4;
    } else if(!strcmp(name,"MOTOR")) {
        op=proto::Command::Motor;
        if(!arguments(v,3) || v[0]<-255 || v[0]>255 || v[1]<-255 || v[1]>255 || v[2]<1) {error(F("Expected left_pwm right_pwm duration_ms"));return;}
        proto::put16(data,uint16_t(v[0]));proto::put16(data+2,uint16_t(v[1]));proto::put32(data+4,v[2]);size=8;
    } else if(!strcmp(name,"SWEEP")) {
        op=proto::Command::Sweep;
        if(!arguments(v,5) || v[0]<1 || v[0]>3 || v[1]<-255 || v[1]>255 || v[2]<-255 || v[2]>255 || v[3]<-255 || v[3]>255 || v[4]<1) {error(F("Expected side start end step dwell_ms"));return;}
        proto::put16(data,uint16_t(v[1]));proto::put16(data+2,uint16_t(v[2]));proto::put16(data+4,uint16_t(v[3]));proto::put16(data+6,v[4]);data[8]=v[0];size=9;
    } else {error(F("Unknown command"));return;}
    uint16_t seq=++sequence;
    if(!send(op,seq,data,size)) {error(F("I2C command failed"));return;}
    Serial.print(F("{\"kind\":\"sent\",\"sequence\":"));Serial.print(seq);Serial.println('}');
}
}
void setup() {
    Serial.begin(cfg::SerialBaud);Wire.begin();Wire.setClock(cfg::I2cClockHz);Wire.setWireTimeout(cfg::I2cTimeoutUs,true);
}
void loop() {
    heartbeat();
    while(Serial.available()) {
        char c=Serial.read();if(c=='\r')continue;
        if(c=='\n') {
            if(inputOverflow)error(F("Command too long"));else {input[inputSize]=0;command(input);}
            inputSize=0;inputOverflow=false;
        } else if(inputSize<sizeof(input)-1)input[inputSize++]=c;
        else inputOverflow=true;
    }
}
