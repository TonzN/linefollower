// Hoved-Nano: samme regulator/runtime som tidligere, nå med USB direkte til PC.
#include <Arduino.h>
#include "side_config.h"
#include "side_link.h"
#define LINEFOLLOW_MASTER
#define setup controlSetup
#define loop controlLoop
#include "nano.cpp"
#undef setup
#undef loop
#include <stdio.h>
#include <stdlib.h>

namespace host {
char input[96],reply[112],token[80];
uint8_t inputSize=0,replyAt=0,tokenAt=0,fieldIndex=0,arrayIndex=0;
bool overflow=false,streaming=false;
uint16_t sequence=0;
sideproto::Sample sideSnapshot;
uint16_t sideAge=65535;
bool sideFresh=false;

void respond(const char* text) {
    // Kommandoer behandles én om gangen; motorstopp skjer før evt. svar-køfeil.
    if(!reply[replyAt]) {snprintf_P(reply,sizeof(reply),PSTR("%S\n"),text);replyAt=0;}
}
void error(const char* message) {
    if(!reply[replyAt]) {snprintf_P(reply,sizeof(reply),PSTR("{\"kind\":\"error\",\"message\":\"%S\"}\n"),message);replyAt=0;}
}
bool arguments(long* values,uint8_t count) {
    for(uint8_t i=0;i<count;i++) {
        char* text=strtok(nullptr," ");if(!text)return false;
        char* end;values[i]=strtol(text,&end,10);
        if(*end || values[i]<-32768 || values[i]>int32_t(cfg::MaxTestMs))return false;
    }
    return strtok(nullptr," ")==nullptr;
}
void dispatch(char* text) {
    char* name=strtok(text," ");if(!name)return;
    if(!strcmp_P(name,PSTR("PING"))) {heartbeat=true;respond(PSTR("{\"kind\":\"pong\"}"));return;}
    if(!strcmp_P(name,PSTR("HELLO"))) {heartbeat=true;respond(PSTR("{\"kind\":\"hello\",\"protocol\":1}"));return;}
    if(!strcmp_P(name,PSTR("POLL"))) {
        if(streaming || token[tokenAt] || reply[replyAt]) {error(PSTR("Previous response pending"));return;}
        heartbeat=true;publish(0);sideSnapshot=side_link::latest();
        sideAge=side_link::ageMs(micros());sideFresh=side_link::fresh(micros());
        fieldIndex=arrayIndex=tokenAt=0;token[0]=0;streaming=true;return;
    }
    proto::Command op;uint8_t data[proto::PayloadSize]={},size=0;long v[5];
    if(!strcmp_P(name,PSTR("STOP")))op=proto::Command::Stop;
    else if(!strcmp_P(name,PSTR("ARM")))op=proto::Command::Arm;
    else if(!strcmp_P(name,PSTR("CLEAR")))op=proto::Command::Clear;
    else if(!strcmp_P(name,PSTR("CALIBRATE")) || !strcmp_P(name,PSTR("SENSORS")) || !strcmp_P(name,PSTR("TIMING")) || !strcmp_P(name,PSTR("FOLLOW"))) {
        op=!strcmp_P(name,PSTR("CALIBRATE"))?proto::Command::Calibrate:!strcmp_P(name,PSTR("SENSORS"))?proto::Command::Sensors:!strcmp_P(name,PSTR("TIMING"))?proto::Command::Timing:proto::Command::Follow;
        if(!arguments(v,1) || v[0]<1) {error(PSTR("Expected duration_ms"));return;}
        proto::put32(data,v[0]);size=4;
    } else if(!strcmp_P(name,PSTR("MOTOR"))) {
        op=proto::Command::Motor;
        if(!arguments(v,3) || v[0]<-255 || v[0]>255 || v[1]<-255 || v[1]>255 || v[2]<1) {error(PSTR("Expected left right duration_ms"));return;}
        proto::put16(data,uint16_t(v[0]));proto::put16(data+2,uint16_t(v[1]));proto::put32(data+4,v[2]);size=8;
    } else if(!strcmp_P(name,PSTR("SWEEP"))) {
        op=proto::Command::Sweep;
        if(!arguments(v,5) || v[0]<1 || v[0]>3 || v[1]<-255 || v[1]>255 || v[2]<-255 || v[2]>255 || v[3]<-255 || v[3]>255 || v[4]<1) {error(PSTR("Expected side start end step dwell_ms"));return;}
        proto::put16(data,uint16_t(v[1]));proto::put16(data+2,uint16_t(v[2]));proto::put16(data+4,uint16_t(v[3]));proto::put16(data+6,v[4]);data[8]=v[0];size=9;
    } else {error(PSTR("Unknown command"));return;}
    heartbeat=true;
    uint16_t seq=++sequence;
    if(op==proto::Command::Stop) {stopSequence=seq;stopRequested=true;pending=false;}
    else {
        if(pending) {error(PSTR("Command pending"));return;}
        proto::frame(mailbox,op,seq,data,size);pending=true;
    }
    char sent[48];snprintf_P(sent,sizeof(sent),PSTR("{\"kind\":\"sent\",\"sequence\":%u}"),seq);respond(sent);
}

// Én liten JSON-del om gangen. Ingen Serial.print-løkke som venter på USB/UART.
// Målebildet er frosset fra POLL til siste del er sendt.
void numeric(const char* key,long value,bool valid=true) {
    if(valid)snprintf_P(token,sizeof(token),PSTR(",\"%S\":%ld"),key,value);
    else snprintf_P(token,sizeof(token),PSTR(",\"%S\":null"),key);
}
void nextToken() {
    const uint8_t* s=masterSnapshot;uint16_t flags=proto::get16(s+42);
    switch(fieldIndex++) {
    case 0: snprintf_P(token,sizeof(token),PSTR("{\"kind\":\"sample\",\"sample\":%lu"),(unsigned long)proto::get32(s));break;
    case 1: snprintf_P(token,sizeof(token),PSTR(",\"time_us\":%lu"),(unsigned long)proto::get32(s+4));break;
    case 2: numeric(PSTR("sensor_us"),proto::get16(s+8));break;
    case 3: numeric(PSTR("cycle_us"),proto::get16(s+10));break;
    case 4: numeric(PSTR("period_us"),proto::get16(s+12));break;
    case 5: numeric(PSTR("lateness_us"),proto::get16(s+14));break;
    case 6: numeric(PSTR("deadline_misses"),proto::get32(s+16));break;
    case 7: numeric(PSTR("skipped_cycles"),proto::get32(s+20));break;
    case 8: numeric(PSTR("min_cycle_us"),proto::get16(s+24));break;
    case 9: numeric(PSTR("max_cycle_us"),proto::get16(s+26));break;
    case 10: numeric(PSTR("error"),int16_t(proto::get16(s+28)),(flags&proto::Calibrated)&&(flags&proto::SampleValid)&&(s[47]=='V'||s[47]=='L'));break;
    case 11: numeric(PSTR("left_pwm"),int16_t(proto::get16(s+30)));break;
    case 12: numeric(PSTR("right_pwm"),int16_t(proto::get16(s+32)));break;
    case 13: numeric(PSTR("voltage_mv"),proto::get16(s+34),flags&proto::VoltageValid);break;
    case 14: numeric(PSTR("current_ma"),int16_t(proto::get16(s+36)),flags&proto::CurrentValid);break;
    case 15: numeric(PSTR("left_rpm_x10"),int16_t(proto::get16(s+38)),flags&proto::LeftRpmValid);break;
    case 16: numeric(PSTR("right_rpm_x10"),int16_t(proto::get16(s+40)),flags&proto::RightRpmValid);break;
    case 17: numeric(PSTR("flags"),flags);break;
    case 18: numeric(PSTR("mode"),s[44]);break;
    case 19: numeric(PSTR("fault"),s[45]);break;
    case 20: numeric(PSTR("sensor_count"),s[46]);break;
    case 21: numeric(PSTR("ack_sequence"),proto::get16(s+48));break;
    case 22: numeric(PSTR("ack_result"),s[50]);break;
    case 23: numeric(PSTR("min_period_us"),proto::get16(s+52));break;
    case 24: numeric(PSTR("max_period_us"),proto::get16(s+54));break;
    case 25: numeric(PSTR("link_age_ms"),proto::get16(s+56));break;
    case 26: numeric(PSTR("raw_maximum"),proto::get16(s+58));break;
    case 27: numeric(PSTR("step"),proto::get16(s+60));break;
    case 28: numeric(PSTR("encoder_age_ms"),proto::get16(s+62));break;
    case 29: snprintf_P(token,sizeof(token),PSTR(",\"quality\":\"%c\",\"control_mode\":\"%c\""),s[47],s[51]);break;
    case 30: numeric(PSTR("side_sequence"),sideSnapshot.sequence);break;
    case 31: numeric(PSTR("side_age_ms"),sideAge,sideAge!=65535);break;
    case 32: numeric(PSTR("side_scan_us"),sideSnapshot.scanUs,sideSnapshot.valid);break;
    case 33: numeric(PSTR("side_fresh"),sideFresh);break;
    case 34: numeric(PSTR("side_count"),sidecfg::Count);break;
    case 35: strcpy_P(token,PSTR(",\"raw\":["));arrayIndex=0;break;
    case 36: case 38: case 40: case 42: case 44: {
        uint8_t state=fieldIndex-1;
        uint8_t count=(state==40 || state==42)?sidecfg::Count:cfg::SensorCount;
        if(arrayIndex==count) {strcpy_P(token,PSTR("]"));break;}
        bool valid=true;uint16_t value=0;
        if(state==36 || state==38) {
            valid=(flags&proto::SampleValid) && (state==36 || (flags&proto::Calibrated));
            value=proto::get16(s+64+2*(arrayIndex+(state==38?cfg::SensorCount:0)));
        } else if(state==40) {valid=sideFresh;value=sideSnapshot.raw[arrayIndex];}
        else if(state==42)value=sidecfg::Inputs[arrayIndex].physicalIndex;
        else value=sidecfg::MainIndices[arrayIndex];
        if(valid)snprintf_P(token,sizeof(token),PSTR("%s%u"),arrayIndex?",":"",value);
        else snprintf_P(token,sizeof(token),PSTR("%snull"),arrayIndex?",":"");
        ++arrayIndex;--fieldIndex;break;
    }
    case 37: strcpy_P(token,PSTR(",\"calibrated\":["));arrayIndex=0;break;
    case 39: strcpy_P(token,PSTR(",\"side_raw\":["));arrayIndex=0;break;
    case 41: strcpy_P(token,PSTR(",\"side_indices\":["));arrayIndex=0;break;
    case 43: strcpy_P(token,PSTR(",\"main_indices\":["));arrayIndex=0;break;
    default: strcpy_P(token,PSTR("}\n"));streaming=false;break;
    }
    tokenAt=0;
}
void output() {
    // Maks 16 bytes per kall; tilgjengelig TX-plass sjekkes før hver skriving.
    for(uint8_t i=0;i<16 && Serial.availableForWrite()>0;i++) {
        if(token[tokenAt]) {Serial.write(token[tokenAt++]);continue;}
        if(streaming) {nextToken();continue;}
        if(reply[replyAt]) {Serial.write(reply[replyAt++]);continue;}
        break;
    }
}
void service() {
    output();
    for(uint8_t i=0;i<16 && Serial.available();i++) {
        char c=Serial.read();if(c=='\r')continue;
        if(c=='\n') {
            if(overflow)error(PSTR("Command too long"));else {input[inputSize]=0;dispatch(input);}
            inputSize=0;overflow=false;
        } else if(inputSize<sizeof(input)-1)input[inputSize++]=c;
        else overflow=true;
    }
}
}
static_assert(sizeof(sidecfg::MainIndices)==cfg::SensorCount,"Main index list must match local sensor count");
void setup() {controlSetup();Serial.begin(cfg::SerialBaud);}
void loop() {
    controlLoop();
    if(int32_t(scheduler.next-micros())>int32_t(cfg::CommandSlackUs))host::service();
    if(int32_t(scheduler.next-micros())>int32_t(sidecfg::ServiceSlackUs))side_link::service(micros());
}
