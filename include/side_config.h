#pragma once
#include <stdint.h>

// Foreløpig analog arkitektur. Bekreft A/RC-utgaven før kabling/opplasting.
// Alle pinnenumre er Arduino-numre (A0=14, A6=20, A7=21).
namespace sidecfg {
#ifndef LINEFOLLOW_SIDE_ENABLED
#define LINEFOLLOW_SIDE_ENABLED 0
#endif
constexpr bool Enabled = LINEFOLLOW_SIDE_ENABLED;
constexpr uint8_t Address = 0x22;
constexpr uint32_t ClockHz = 100000;
constexpr uint32_t TimeoutUs = 4000;
constexpr uint32_t ServiceSlackUs = 5000; // Én I2C-operasjon per servicekall.
constexpr uint32_t PollPeriodUs = 16000;
constexpr uint16_t MaxAgeMs = 40;
constexpr uint32_t ScanPeriodUs = 8000;
constexpr uint16_t SettleUs = 20; // Foreløpig; verifiser med faktisk kildeimpedans.
constexpr bool DiscardFirstAdc = true;
constexpr uint8_t Samples = 1;
constexpr uint8_t PhysicalCount = 25; // Foreløpig identifikasjon, ikke bekreftet modell.
// Eksempel på seks sammenhengende midtlesere; tilpass faktisk montering.
constexpr uint8_t MainIndices[] = {10,11,12,13,14,15};
struct Input { uint8_t physicalIndex; int8_t pin, mux; uint8_t channel; };
// mux=-1: direkte analog pin. mux=0/1: signalpin fra MuxSignalPins.
// Velg fritt hvilke ytterlesere som brukes; listen sorteres fra venstre mot høyre.
constexpr Input Inputs[] = {
    {0,-1,-1,0}, {4,-1,-1,0}, {8,-1,-1,0},
    {17,-1,-1,0}, {21,-1,-1,0}, {24,-1,-1,0}
};
constexpr uint8_t Count = sizeof(Inputs)/sizeof(Inputs[0]);
constexpr int8_t MuxSignalPins[] = {-1,-1};
constexpr int8_t MuxSelectPins[] = {-1,-1,-1,-1}; // S0..S3, deles av to 16:1-muxer.
// Mux-enable kobles aktivt iht. kretsens datablad; støttes ikke som GPIO her.
// Kun hovedkortet skal eventuelt styre sensorbrettets felles emitterpinner.
static_assert(Count>0 && Count<=25,"Use 1..25 side channels");
static_assert(Samples>=1 && Samples<=16,"Invalid ADC averaging");
static_assert(Address>=8 && Address<=119,"Invalid side I2C address");
static_assert(ScanPeriodUs>0 && PollPeriodUs>0 && MaxAgeMs>0,"Invalid side timing");
}
