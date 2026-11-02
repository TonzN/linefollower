#include <Arduino.h>
#include <QTRSensors.h>
#include <util/atomic.h>
#include <math.h>
#include "hardware.h"
#include "protocol.h"

namespace {
QTRSensors qtr;
bool sensorOk=false,motorOk=false;
volatile uint32_t leftTicks=0,rightTicks=0;
uint32_t encoderAt=0,previousLeft=0,previousRight=0;
void leftPulse() {++leftTicks;}
void rightPulse() {++rightTicks;}
bool takePin(int8_t pin,bool* used,bool analogOnly=false) {
    if(pin<0 || pin>21 || (!analogOnly && pin>19) || (analogOnly && pin<14) || used[pin])return false;
    used[pin]=true;return true;
}
bool takeMotor(const cfg::MotorPins& p,bool* used) {
    return takePin(p.in1,used) && takePin(p.in2,used) && takePin(p.pwm,used) && digitalPinHasPWM(p.pwm);
}
void writeMotor(const cfg::MotorPins& p,int16_t value) {
    if(p.inverted)value=-value;
    digitalWrite(p.in1,value>0?HIGH:LOW);digitalWrite(p.in2,value<0?HIGH:LOW);
    analogWrite(p.pwm,value<0?-value:value);
}
int16_t bounded(float value) {return int16_t(value>32767?32767:value<-32767?-32767:value);}
}

namespace hw {
int16_t appliedLeft=0,appliedRight=0;
uint16_t measurementFlags=0,voltageMv=0;
uint16_t encoderAgeMs=0;
int16_t currentMa=0,leftRpm10=0,rightRpm10=0;
bool begin() {
    bool used[22]={};used[18]=used[19]=true; // A4/A5 er I2C på klassisk Nano.
    bool valid=true;
    if(cfg::MotorsEnabled) {
        valid=takeMotor(cfg::LeftMotor,used)&&takeMotor(cfg::RightMotor,used);
        if(cfg::DriverEnablePin>=0)valid=takePin(cfg::DriverEnablePin,used)&&valid;
    }
    if(cfg::Sensors==cfg::SensorBackend::QtrRC || cfg::Sensors==cfg::SensorBackend::QtrAnalog) {
        for(uint8_t i=0;i<cfg::SensorCount;i++)
            valid=takePin(cfg::SensorPins[i],used,cfg::Sensors==cfg::SensorBackend::QtrAnalog)&&valid;
        if(cfg::EmitterPin>=0)valid=takePin(cfg::EmitterPin,used)&&valid;
    }
    if(cfg::VoltagePin>=0)valid=takePin(cfg::VoltagePin,used,true)&&valid;
    if(cfg::CurrentPin>=0)valid=takePin(cfg::CurrentPin,used,true)&&valid;
    const int8_t encoderPins[]={cfg::LeftEncoderPin,cfg::RightEncoderPin};
    for(int8_t pin:encoderPins) {
        if(pin>=0)valid=takePin(pin,used)&&digitalPinToInterrupt(pin)!=NOT_AN_INTERRUPT&&valid;
    }
    if(!valid)return false;
    if(cfg::MotorsEnabled) {
        const cfg::MotorPins motorPins[]={cfg::LeftMotor,cfg::RightMotor};
        for(const auto& p:motorPins) {
            pinMode(p.in1,OUTPUT);pinMode(p.in2,OUTPUT);pinMode(p.pwm,OUTPUT);writeMotor(p,0);
        }
        if(cfg::DriverEnablePin>=0) {
            pinMode(cfg::DriverEnablePin,OUTPUT);
            digitalWrite(cfg::DriverEnablePin,!cfg::DriverEnableActiveHigh);
        }
        motorOk=true;
    }
    if(cfg::Sensors==cfg::SensorBackend::Custom)sensorOk=custom_sensor::begin() && custom_sensor::maximum()>0;
    else if(cfg::Sensors==cfg::SensorBackend::QtrAnalog) {
        // Direkte ADC-avlesning; kalibrering skjer allerede i kontroll-runtime.
        // Unngår å dra med QTRs RC-/emitterstyring på den lille hoved-Nanoen.
        if(cfg::EmitterPin>=0) {
            pinMode(cfg::EmitterPin,OUTPUT);digitalWrite(cfg::EmitterPin,HIGH);delay(1);
        }
        sensorOk=true;
    }
    else if(cfg::Sensors==cfg::SensorBackend::QtrRC) {
        uint8_t pins[cfg::SensorCount];
        for(uint8_t i=0;i<cfg::SensorCount;i++)pins[i]=cfg::SensorPins[i];
        qtr.setTypeRC();qtr.setTimeout(cfg::QtrTimeoutUs);
        qtr.setSensorPins(pins,cfg::SensorCount);
        if(cfg::EmitterPin>=0)qtr.setEmitterPin(cfg::EmitterPin);
        sensorOk=true;
    }
    if(cfg::VoltagePin>=0)measurementFlags|=proto::VoltageValid;
    if(cfg::CurrentPin>=0 && cfg::CurrentVoltsPerAmp>0)measurementFlags|=proto::CurrentValid;
    if(cfg::EncoderPulsesPerWheelRevolution>0) {
        if(cfg::LeftEncoderPin>=0) {pinMode(cfg::LeftEncoderPin,INPUT_PULLUP);attachInterrupt(digitalPinToInterrupt(cfg::LeftEncoderPin),leftPulse,RISING);}
        if(cfg::RightEncoderPin>=0) {pinMode(cfg::RightEncoderPin,INPUT_PULLUP);attachInterrupt(digitalPinToInterrupt(cfg::RightEncoderPin),rightPulse,RISING);}
    }
    encoderAt=micros();return true;
}
bool sensorReady() {return sensorOk;}
bool motorReady() {return motorOk;}
uint16_t rawMaximum() {return cfg::Sensors==cfg::SensorBackend::Custom?custom_sensor::maximum():cfg::Sensors==cfg::SensorBackend::QtrAnalog?1023:cfg::QtrTimeoutUs;}
bool readSensors(uint16_t* raw) {
    if(!sensorOk)return false;
    if(cfg::Sensors==cfg::SensorBackend::Custom)return custom_sensor::read(raw,cfg::SensorCount);
    if(cfg::Sensors==cfg::SensorBackend::QtrAnalog) {
        for(uint8_t i=0;i<cfg::SensorCount;i++) {
            analogRead(cfg::SensorPins[i]); // Forkast første ADC-konvertering etter kanalbytte.
            uint32_t sum=0;
            for(uint8_t n=0;n<cfg::AnalogSamplesPerSensor;n++)sum+=analogRead(cfg::SensorPins[i]);
            raw[i]=sum/cfg::AnalogSamplesPerSensor;
        }
        return true;
    }
    qtr.read(raw,QTRReadMode::On);return true;
}
void motors(int16_t left,int16_t right) {
    if(!motorOk) {appliedLeft=appliedRight=0;return;}
    appliedLeft=constrain(left,-255,255);appliedRight=constrain(right,-255,255);
    if(cfg::DriverEnablePin>=0)digitalWrite(cfg::DriverEnablePin,(left||right)?cfg::DriverEnableActiveHigh:!cfg::DriverEnableActiveHigh);
    writeMotor(cfg::LeftMotor,appliedLeft);writeMotor(cfg::RightMotor,appliedRight);
}
void stop() {motors(0,0);}
void updateMeasurements(uint32_t now) {
    if(measurementFlags&proto::VoltageValid)voltageMv=uint16_t(constrain(analogRead(cfg::VoltagePin)*cfg::AdcReferenceV/1023*cfg::VoltageDividerRatio*1000,0.0f,65535.0f));
    if(measurementFlags&proto::CurrentValid)currentMa=bounded((analogRead(cfg::CurrentPin)*cfg::AdcReferenceV/1023-cfg::CurrentZeroV)/cfg::CurrentVoltsPerAmp*1000);
    uint32_t dt=now-encoderAt;
    encoderAgeMs=proto::saturated16(dt/1000);
    if(dt<cfg::EncoderWindowUs || !cfg::EncoderPulsesPerWheelRevolution)return;
    uint32_t left,right;
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {left=leftTicks;right=rightTicks;}
    const float factor=600000000.0f/(dt*float(cfg::EncoderPulsesPerWheelRevolution));
    leftRpm10=bounded((left-previousLeft)*factor);rightRpm10=bounded((right-previousRight)*factor);
    if(cfg::LeftEncoderPin>=0)measurementFlags|=proto::LeftRpmValid;
    if(cfg::RightEncoderPin>=0)measurementFlags|=proto::RightRpmValid;
    encoderAgeMs=0;
    previousLeft=left;previousRight=right;encoderAt=now;
}
}
