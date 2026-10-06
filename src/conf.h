//
// Created by tonzg on 05/10/2026.
//
#pragma once

#include <stdint.h>
#ifndef LINEFOLLOWER_CONF_H
#define LINEFOLLOWER_CONF_H

const uint8_t SensorCount = 6;

const int AIN1 = 13; // HØYRE
const int AIN2 = 12;
const int PWMA = 6;
const int BIN1 = 8; // VENSTRE
const int BIN2 = 9;
const int PWMB = 10;
const int START = 7;
const int CALIBRATE = 3;


// Original motoroffset. Behold 0 under tuning: originalfunksjonen legger dette til også ved reversering.
const int offsetA = 0;
const int offsetB = 0;

// Midten av seks sensorer (0–5000). Endres bare ved annen sensorgeometri.
const int CENTER = 2500;
// +1 når linje til venstre gir negativ feil. Sett -1 hvis statisk sensortest viser motsatt fortegn.
const int SENSOR_DIRECTION = 1;
// Grunn-PWM på stabil rett linje. Høyere gir mer fart og krever raskere korrigering.
const float BASE_SPEED = 250.0f;
// Grunn-PWM i svinger/ved gjenfinning, ikke minste PWM per hjul. Lavere gir roligere kjøring, men kan gi stillstand.
const float CORNER_SPEED = 200.0f;
// Tak per motor. Høyere gir kraftigere ytre hjul i svinger; begge mål skaleres sammen ved metning.
const float MOTOR_LIMIT = 250.0f;
// P-forsterkning i PWM per posisjonsenhet. Høyere følger avvik hardere, men kan øke pendling.
const float KP = 0.04f;
// D-forsterkning i PWM per posisjonsenhet/sekund. Høyere demper bevegelse over linjen, men forsterker støy.
const float KD = 0.006f;
// D-filterets tidskonstant i sekunder. Høyere filtrerer mer, men forsinker dempingen.
const float D_FILTER_SECONDS = 0.015f;
// Tak på svingkorreksjonen i PWM. Høyere gir krappere svinger; indre hjul kan stoppe.
const float MAX_TURN = 250.0f;
// Fartstap per posisjonsenhet. Høyere senker farten mer ved avvik fra sentrum.
const float ERROR_SPEED_DROP = 0.05f;
// Fartstap per posisjonsenhet/sekund. Høyere holder farten lavere mens roboten krysser linjen raskt.
const float RATE_SPEED_DROP = 0.001f;
// Maks økning av grunn-PWM per sekund. Lavere gir langsommere akselerasjon etter svinger.
const float BASE_ACCEL = 80.0f;
// Maks økning av hver motors PWM-magnitude per sekund. Lavere gir mykere, men tregere styrerespons.
const float MOTOR_ACCEL = 1200.0f;
// Maks reduksjon av hver motors PWM-magnitude per sekund. Høyere bremser/reverserer raskere.
const float MOTOR_DECEL = 1200.0f;
// Maks tidssteg for utgangsrampen i sekunder. Lavere begrenser PWM-hopp etter en forsinket oppdatering.
const float MAX_RAMP_DT = 0.020f;
// Kontrollperiode i ms. Lavere leser oftere; D-leddet bruker faktisk tid og trenger ikke ny enhet ved endring.
const uint32_t CONTROL_PERIOD_MS = 10;
// Feil som regnes som nær sentrum. Høyere tillater fartsøkning lenger fra midten.
const int STABLE_ERROR = 200;
// Maks feilendring/sekund for stabil følging. Høyere godtar mer bevegelse før farten øker.
const float STABLE_RATE = 2500.0f;
// Sammenhengende stabil tid i ms før farten kan øke. Høyere hindrer korte sentrumspasseringer fra å gi fartshopp.
const uint32_t STABLE_TIME_MS = 160;
// Minste toppverdi for en troverdig linje (0–1000). Høyere avviser mer bakgrunn, men også svak linje.
const uint16_t LINE_MIN_PEAK = 850;
// Verdier under dette bidrar ikke til posisjonen. Høyere fjerner mer bakgrunn, men gjør posisjonen grovere.
const uint16_t SENSOR_FLOOR = 650;
// Minste forskjell mellom høyeste/laveste sensor. Høyere krever tydeligere kontrast mot underlaget.
const uint16_t LINE_MIN_CONTRAST = 300;
// Maks antall sterke sensorer i valgt gruppe. Høyere godtar bredere linjer/felt.
const uint8_t LINE_MAX_WIDTH = 3;
// Prosent av største bakgrunnskorrigerte utslag som regnes som sterkt. Høyere ignorerer flere svake sideutslag.
const uint8_t CORE_PERCENT = 50;
// Valgt gruppe må ha minst så mange ganger vekten til øvrige sterke grupper samlet.
// Høyere avviser flere tvetydige mønstre; lavere kan velge feil linje ved flere streker.
const uint8_t GROUP_DOMINANCE = 3;
// Ugyldig signal må vare så mange ms før fullt søk. Høyere tåler flere glipper, men reagerer senere på tap.
const uint32_t LOSS_CONFIRM_MS = 4;
// PWM-reduksjon per sekund i korte glipper. Høyere bremser hardere før bekreftet tap.
const float GAP_DECEL = 600.0f;
// Opptrapping av grunn-PWM per sekund under CORNER_SPEED, også ved start/gjenfunn.
// Høyere kommer raskere ut av motorenes svake lav-PWM-område; hjulrampene gjelder fortsatt.
const float RESTART_ACCEL = 900.0f;
// Minste rotasjons-PWM ved bekreftet linje ute på kanten. Høyere retter raskere, men øker oversving.
const float ALIGN_MIN_SPEED = 60.0f;
// PWM under søk. Høyere roterer raskere, men kan passere linjen før gjenfunn bekreftes.
const float SEARCH_SPEED = 85.0f;
// Maks tid fra første ugyldige måling i ms. Høyere gir lengre søk og større fare for å finne banen baklengs.
const uint32_t SEARCH_TIMEOUT_MS = 1400;
// Ekstra ms kun for et pågående gjenfunn ved fristen. Høyere utsetter absolutt stoppgrense.
const uint32_t REACQUIRE_GRACE_MS = 150;
// Linjen må være innenfor dette avviket før recovery kan avsluttes. Lavere krever bedre sentrering.
const int REACQUIRE_ERROR = 1000;
// Bekreftelsestid i ms for et troverdig treff. Robotens kommandoer bremses mens treffet bekreftes.
// Høyere avviser flere korte treff, men gir lengre pause ved gjenfunn.
const uint32_t REACQUIRE_TIME_MS = 40;
// Største posisjonshopp mellom bekreftelsesmålinger. Høyere godtar raskere bevegelse eller mer støy.
const int REACQUIRE_MAX_STEP = 600;
// Minste avvik for å huske side. Høyere ignorerer mer støy, men også små retningshint.
const int SIDE_MEMORY_ERROR = 100;
// Maks alder på sidehint i ms ved søkestart. Høyere stoler lenger på gammel retning.
const uint32_t SIDE_MAX_AGE_MS = 250;
// Sentrert tid i ms før sideminnet slettes. Høyere beholder gammel side lenger på rette strekk.
const uint32_t SIDE_CLEAR_MS = 150;
// Ukjent søkeretning: +1 høyre, -1 venstre. Endres bare etter et bekreftet, tydelig sidefunn.
const int UNKNOWN_SEARCH_SIDE = 1;
// Maks bidrag fra D-leddet i PWM. Høyere gir mer demping, men også større utslag ved posisjonshopp.
const float MAX_D_TURN = 20.0f;
// Avvik der D-leddet ikke får snu korreksjonen bort fra linjen. Høyere tillater dette lenger ut.
const int EDGE_GUARD_ERROR = 500;
// Andel av P-korreksjonen ved feilvendt D-ledd. Høyere gir sterkere korreksjon mot linjen.
const float EDGE_P_FACTOR = 0.35f;
// Seriell diagnoseintervall; høyere gir færre utskrifter og mindre belastning.
const uint32_t DEBUG_PERIOD_MS = 200;
// Stabil knappetilstand i ms. Høyere tåler mer kontaktsprett, men øker tiden før stopp.
const uint32_t BUTTON_DEBOUNCE_MS = 25;

// Bakgrunn som trekkes fra ved posisjon innenfor valgt gruppe og dens nærmeste naboer.
// Lavere gir mykere posisjon, men mer påvirkning fra bakgrunn; fjernsensorer tas fortsatt ikke med.
const uint16_t POSITION_FLOOR = 350;
// Minste topp for kortvarig viderefølging av en allerede kjent linje. Høyere tåler mindre signalfall.
const uint16_t TRACK_MIN_PEAK = 750;
// Maks ms siden siste sterke linjemåling for å bruke svakere signal. Høyere kan følge svak linje lenger.
const uint32_t WEAK_HOLD_MS = 100;
// Maks avvik fra siste sterke posisjon ved svakere signal. Høyere godtar større bevegelse og mer bakgrunnsfeil.
const int WEAK_MAX_SHIFT = 350;
// Hvor lenge et sterkt, men tvetydig mønster kan vare før stopp. Høyere gir lengre tid til å finne en tydelig linje.
const uint32_t AMBIGUOUS_TIMEOUT_MS = 250;
// Maks reduksjon av grunn-PWM per sekund under vanlig følging. Høyere bremser tidligere inn i svinger.
const float BASE_DECEL = 800.0f;
// Avvik før ekstra svingstyrke begynner.
// Lavere gir tidligere ekstra styring, men påvirker også slake svinger mer.
const int EXTRA_TURN_START = 1200;

// Ekstra P-forsterkning over grensen, i PWM per posisjonsenhet.
// Høyere gir kraftigere styring ved store avvik, men øker risikoen for oversving.
const float EXTRA_TURN_GAIN = 0.03f;

#endif