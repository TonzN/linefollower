//
// Created by tonzg on 05/10/2026.
//
#pragma once
#include <stdint.h>

#ifndef LINEFOLLOWER_FOLLOW_H
#define LINEFOLLOWER_FOLLOW_H

void rampFollowing(float& left, float& right, float targetLeft, float targetRight, float dt);

struct LineReading
{
    // Posisjonsfeil i normalisert skala: negativ venstre, positiv høyre.
    int error = 0;
    // Sterkeste kalibrerte sensorverdi, 0..1000. Dette er ikke posisjonen.
    uint16_t peak = 0;
    // valid sier at vi kan bruke posisjonen; strong gir en strengere bekreftelse.
    bool valid = false;
    bool strong = false;
    char quality = '?';
};

struct LineController
{
    // Faktisk rampet PWM-utgang. base er felles fremdrift, derivative er filtrert D-rate.
    float left = 0;
    float right = 0;
    float base = 0;
    float derivative = 0;
    // Historikk for styring og siste troverdige søkeretning.
    int previousError = 0;
    int lastSide = 0;
    int searchSide = 0;
    int candidateError = 0;
    // Alle tider her er millis(), ikke micros(). Differanser tåler telleromløp.
    uint32_t previousTime = 0;
    uint32_t lossStart = 0;
    uint32_t stableStart = 0;
    uint32_t sideTime = 0;
    uint32_t candidateStart = 0;
    bool initialized = false;
    bool derivativeReady = false;
    bool stableTiming = false;
    // Et kort linjetap kan være et gap. Ved lengre tap går vi til recovery/søk.
    bool gapActive = false;
    bool recovering = false;
    bool braking = false;
    bool candidate = false;
    bool aligning = false;
    bool stopped = false;
    bool haveStrong = false;
    bool uncertain = false;
    int strongError = 0;
    uint32_t strongTime = 0;
    uint32_t uncertainStart = 0;
    float uncertainLeft = 0;
    float uncertainRight = 0;

    // Gjenfunn må bekreftes over flere observasjoner før vanlig følging gjenopptas.
    float captureRate = 0;
    uint32_t captureTime = 0;
    bool captureSeen = false;
    int captureSide = 0;
    uint8_t captureSamples = 0;
    int directionCandidateSide = 0;
    uint32_t directionCandidateStart = 0;

    // Kalles én gang per kontrollrunde. now er millisekunder; left/right oppdateres.
    void update(const LineReading& line, uint32_t now);
    void stop();
    char mode() const;
    void brake(float dt);
};

// values må ha SensorCount kalibrerte verdier (0..1000), sortert venstre–høyre.
// Avviser lav kontrast, for brede funn og flere grupper uten tydelig dominans.
LineReading analyzeLine(const uint16_t* values);

#endif
