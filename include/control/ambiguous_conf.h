#pragma once
#include <stdint.h>

// PWM-mål for kraftigste hjul ved kort passering av tvetydig sensorområde.
// Høyere gir mer bevegelse ut av området, men større blind forflytning.
// Hjulenes eksisterende forhold/fortegn beholdes. 0 gir nedbremsing som før.
const float AMBIGUOUS_ESCAPE_PWM = 150.0f;

// Maks tid i ms med denne bevegelsen før utgangene rampes mot null.
// Høyere kan passere større felt, men forlenger blind kjøring. 0 deaktiverer.
// AMBIGUOUS_TIMEOUT_MS kan fortsatt stoppe roboten tidligere.
const uint32_t AMBIGUOUS_ESCAPE_MS = 200;

// Maks alder i ms på siste sterke linjetreff ved starten av tvetydigheten.
// Høyere godtar eldre retning; lavere krever ferskere grunnlag for bevegelsen.
const uint32_t AMBIGUOUS_HINT_MS = 200;
