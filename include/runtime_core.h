#pragma once
#include <stdint.h>
#include "protocol.h"

namespace runtime {
inline uint32_t age(uint32_t now,uint32_t before) {return now-before;}
struct Scheduler {
    uint32_t next=0, skipped=0;
    explicit Scheduler(uint32_t start=0):next(start) {}
    bool due(uint32_t now) const {return int32_t(now-next)>=0;}
    uint32_t take(uint32_t now,uint32_t period) {
        uint32_t late=now-next, lost=late/period;
        skipped+=lost;next+=(lost+1)*period;return late;
    }
};
struct Sweep {
    int16_t start=0,end=0,step=1;
    uint16_t dwell=1,index=0;
    uint8_t side=0;
    uint32_t totalMs() const {
        uint16_t distance=start>end?start-end:end-start;
        uint16_t increment=step<0?-step:step;
        if(!increment || !dwell)return 0;
        return (uint32_t((distance+increment-1)/increment)+1)*dwell;
    }
    bool valid(int16_t pwmLimit,uint32_t durationLimit) const {
        if(side<1 || side>3 || step==0 || !dwell)return false;
        if(start>pwmLimit || start<-int16_t(pwmLimit) || end>pwmLimit || end<-int16_t(pwmLimit))return false;
        if((end>start && step<0)||(end<start && step>0))return false;
        return totalMs()>0 && totalMs()<=durationLimit;
    }
    int16_t value() const {
        int32_t v=int32_t(start)+int32_t(index)*step;
        return step>0?(v>end?end:v):(v<end?end:v);
    }
};
}
