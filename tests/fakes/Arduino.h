#pragma once
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
extern uint32_t fakeMicros;
inline uint32_t micros() {return fakeMicros;}
inline uint32_t millis() {return fakeMicros/1000;}
template<class T> inline T min(T a,T b) {return a<b?a:b;}
template<class T> inline T max(T a,T b) {return a>b?a:b;}
#define constrain(v,lo,hi) ((v)<(lo)?(lo):((v)>(hi)?(hi):(v)))
