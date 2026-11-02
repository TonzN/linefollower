#pragma once
#include "side_protocol.h"
namespace side_link {
void service(uint32_t now);
const sideproto::Sample& latest();
uint16_t ageMs(uint32_t now);
bool fresh(uint32_t now);
}
