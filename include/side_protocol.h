#pragma once
#include "protocol.h"

// Eget 32-byte format for ytterkortet. 9 verdier per side, maksimalt 3 sider.
// Latch fryser ett helt sett. Alle sider må ha samme sekvensnummer.
namespace sideproto {
constexpr uint8_t Magic=0xB6, Version=1, Capacity=25, PerPage=9;
constexpr uint8_t PageCount=(Capacity+PerPage-1)/PerPage;
constexpr uint8_t Latch=0, Select=1;
struct Sample {
    uint16_t sequence=0, ageMs=65535, scanUs=0;
    uint8_t count=0;
    bool valid=false;
    uint16_t raw[Capacity]={};
};
inline void page(uint8_t* dst,const Sample& s,uint8_t selected) {
    memset(dst,0,32);dst[0]=Magic;dst[1]=Version;dst[2]=selected;
    proto::put16(dst+3,s.sequence);dst[5]=s.count;dst[6]=s.valid;
    proto::put16(dst+8,s.scanUs);proto::put16(dst+10,s.ageMs);
    for(uint8_t i=0;i<PerPage;i++) {
        uint8_t index=selected*PerPage+i;
        if(index<s.count && index<Capacity)proto::put16(dst+12+2*i,s.raw[index]);
    }
    dst[31]=proto::crc8(dst,31);
}
inline bool accept(const uint8_t* src,uint8_t size,uint8_t selected,uint16_t sequence,Sample& s) {
    if(size!=32 || src[0]!=Magic || src[1]!=Version || src[2]!=selected ||
       selected>=PageCount || proto::get16(src+3)!=sequence || src[5]<1 || src[5]>Capacity ||
       proto::crc8(src,31)!=src[31])return false;
    if(selected==0) {s=Sample();s.sequence=sequence;s.count=src[5];s.valid=src[6]==1;
        s.scanUs=proto::get16(src+8);s.ageMs=proto::get16(src+10);}
    else if(s.count!=src[5] || s.valid!=(src[6]==1) || s.scanUs!=proto::get16(src+8) || s.ageMs!=proto::get16(src+10))return false;
    for(uint8_t i=0;i<PerPage;i++) {
        uint8_t index=selected*PerPage+i;
        if(index<s.count)s.raw[index]=proto::get16(src+12+2*i);
    }
    return true;
}
// Alder regnes konservativt fra skannestart, ikke bare siste ADC-avlesning.
inline uint16_t age(const Sample& s,uint32_t transferAt,uint32_t now) {
    return proto::saturated16(uint32_t(s.ageMs)+(uint32_t(now-transferAt)+999)/1000);
}
}
