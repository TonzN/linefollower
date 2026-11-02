#pragma once
#include <stdint.h>
// Testfixture: én direkte kanal og én kanal fra hver av to muxer.
namespace sidecfg {
constexpr bool Enabled=true;
constexpr uint8_t Address=0x22,Count=3,PhysicalCount=25,Samples=2;
constexpr uint32_t ScanPeriodUs=8000;
constexpr uint16_t SettleUs=20;
constexpr bool DiscardFirstAdc=true;
constexpr uint8_t MainIndices[]={10,11,12,13,14,15};
struct Input {uint8_t physicalIndex;int8_t pin,mux;uint8_t channel;};
constexpr Input Inputs[]={{0,20,-1,0},{4,-1,0,3},{24,-1,1,7}};
constexpr int8_t MuxSignalPins[]={14,15},MuxSelectPins[]={2,3,4,5};
}
