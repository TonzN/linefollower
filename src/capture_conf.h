#pragma once
#include <stdint.h>

// Avvik der hele ALIGN_MIN_SPEED brukes ved langsom innretting.
// Høyere trapper ned minimumsstyringen tidligere mot sentrum og gir roligere
// innretting; for høyt kan gi svakt pådrag. Brukes minst som RECOVERY_FORWARD_ERROR.
const int RECOVERY_FULL_ALIGN_ERROR = 1200;

// Andel av minimumsstyringen som beholdes ved rask bevegelse mot sentrum.
// Høyere holder rotasjonen bedre i gang, men kan øke oversving.
// Lavere bremser sterkere; 0 kan igjen gi null pådrag langt fra sentrum.
const float RECOVERY_MOVING_MIN_FACTOR = 0.5f;

// Maks bidrag fra D-leddet i PWM under synlig innretting.
// Høyere gir sterkere demping/reaksjon på feilendring, men større rykk ved
// sensorhopp. Lavere gir roligere kommandoer, men kan gi mer oversving.
const float RECOVERY_D_LIMIT = 30.0f;

// Avvik der ekstra recovery-pådrag starter. Høyere begrenser det til større
// avvik; lavere gir tidligere kraft, men større fare for oversving.
const int RECOVERY_FAR_START_ERROR = 900;

// Avvik der ekstra pådrag er fullt. Høyere gir mer gradvis opptrapping;
// lavere gir kraftigere styring tidligere. Må være større enn startgrensen.
const int RECOVERY_FAR_FULL_ERROR = 1600;

// Rotasjons-PWM langt fra sentrum, begrenset av MOTOR_LIMIT.
// Høyere gir raskere innretting, men større oversving. Erstatter gradvis
// RECOVERY_CAPTURE_LIMIT langt ute; nær sentrum gjelder det gamle taket.
const float RECOVERY_FAR_TURN = 200.0f;

// Andel av det fjerne minimumspådraget ved rask bevegelse mot sentrum.
// Høyere beholder mer kraft; lavere demper mer før sentrum nås.
const float RECOVERY_FAR_MOVING_FACTOR = 0.8f;

// Minste rotasjonsmål utenfor fremdriftsområdet, også under bevegelse mot sentrum.
// Høyere motvirker treg innretting, men kan øke oversving. Trappes ned innenfor
// RECOVERY_FORWARD_ERROR; hjulrampene og motdempingen nær sentrum gjelder fortsatt.
const float RECOVERY_BRIDGE_MIN_TURN = 150.0f;

// Sammenhengende ms med sterk linje på motsatt side før blind søking får snu.
// Høyere avviser flere korte treff, men forsinker en nødvendig søkeretningsendring.
// Synlig motstyring venter ikke på denne bekreftelsen.
const uint32_t RECOVERY_DIRECTION_CONFIRM_MS = 32;
