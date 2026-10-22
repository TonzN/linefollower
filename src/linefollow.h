//
// Created by tonzg on 05/10/2026.
//
#pragma once
#include <stdint.h>

#ifndef LINEFOLLOWER_FOLLOW_H
#define LINEFOLLOWER_FOLLOW_H

void rampFollowing(float &left, float &right, float targetLeft, float targetRight, float dt);

struct LineReading
{
    int error = 0;
    uint16_t peak = 0;
    bool valid = false;
    bool strong = false;
    char quality = '?';
};

struct LineController
{
    float left = 0, right = 0, base = 0, derivative = 0;
    int previousError = 0, lastSide = 0, searchSide = 0, candidateError = 0;
    uint32_t previousTime = 0, lossStart = 0, stableStart = 0, sideTime = 0, candidateStart = 0;
    bool initialized = false, derivativeReady = false, stableTiming = false;
    bool gapActive = false, recovering = false, braking = false;
    bool candidate = false, aligning = false, stopped = false;
    bool haveStrong = false, uncertain = false;
    int strongError = 0;
    uint32_t strongTime = 0, uncertainStart = 0;
    float uncertainLeft = 0, uncertainRight = 0;


    float captureRate = 0;
    uint32_t captureTime = 0;
    bool captureSeen = false;
    int captureSide = 0;
    uint8_t captureSamples = 0;
    int directionCandidateSide = 0;
    uint32_t directionCandidateStart = 0;

    void update(const LineReading &line, uint32_t now);
    void stop();
    char mode() const;
    void brake(float dt);
};

LineReading analyzeLine(const unsigned int *values);

#endif
