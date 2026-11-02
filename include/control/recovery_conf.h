#pragma once
#include <stdint.h>

// Tak på blind søkerotasjon, i tillegg til SEARCH_SPEED og MOTOR_LIMIT.
// Høyere søker raskere, men kan passere linjen før hjulene rekker å bremse.
const float RECOVERY_SEARCH_LIMIT = 200.0f;
// Tak på rotasjons-PWM straks en sterk linje er synlig.
// Høyere innretter raskere, men øker oversving. Uavhengig av blind søkefart.
const float RECOVERY_CAPTURE_LIMIT = 140.0f;
// Maks posisjonsavvik før fremdriften kan gjenopptas under innretting.
// Høyere starter tidligere, men roboten kan fortsatt stå langt fra sentrum.
const int RECOVERY_FORWARD_ERROR = 700;
// Mål for gjennomsnittlig PWM nær sentrum mens recovery fortsatt bekrefter treffet.
// Høyere bygger fremdrift tidligere, men kan føre roboten ut før den er innrettet.
// Lavere gir roligere overtakelse; 0 gjenoppretter V12. Hjulrampene gjelder fortsatt.
const float RECOVERY_APPROACH_DRIVE = 100.0f;
// Maks filtrert feilendring/sekund før fremdrift og vanlig følging kan overta.
// Høyere godtar raskere sentrumspassering, men øker risikoen for nytt oversving.
const float RECOVERY_FORWARD_RATE = 2500.0f;
// Maks halv hjulforskjell i PWM før overtakelse. Høyere slipper vanlig
// følging til med mer gjenværende rotasjonskommando; lavere innretter lenger.
const float RECOVERY_EXIT_TURN = 50.0f;

// Maks reduksjon av PWM-magnitude per sekund ved treff/innretting.
// Høyere fjerner gammel rotasjonskommando raskere; for høyt gir rykk.
const float RECOVERY_BRAKE_RATE = 3000.0f;
// Ms med forsiktig innretting etter et nylig treff som forsvinner.
// Høyere bruker siste retning lenger; lavere går tidligere til begrenset søk.
const uint32_t RECOVERY_HOLD_MS = 120;
// Maks PWM under kort glipp etter treff. Høyere finner kanten raskere,
// men øker risikoen for å passere linjen på nytt.
const float RECOVERY_HOLD_TURN = 120.0f;
// Absolutt posisjonsavvik som gir et nytt sidehint fra sterk linje.
// Høyere ignorerer mer sentrumsstøy, men beholder gammel retning lenger.
const int RECOVERY_DIRECTION_ERROR = 300;
