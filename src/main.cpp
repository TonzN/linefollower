// V9: jevnere posisjon og samordnet hjulramping; svak linje og tvetydig mønster skilles.
#include <ZumoReflectanceSensorArray.h>
#include <stdio.h>

// Eksplisitte prototyper hindrer Arduino i å plassere disse før typene er deklarert.
struct LineReading;
struct LineController;
LineReading analyzeLine(const uint16_t *values);
void printSnapshot(const LineReading &line, const LineController &control);

ZumoReflectanceSensorArray reflectanceSensor;

const uint8_t SensorCount = 6;
uint16_t sensorValues[SensorCount];

const int AIN1 = 13; // HØYRE
const int AIN2 = 12;
const int PWMA = 6;
const int BIN1 = 8; // VENSTRE
const int BIN2 = 9;
const int PWMB = 10;
const int START = 7;
const int CALIBRATE = 3;

bool CALIBRATED = false;
bool RUNNED = false;

// Original motoroffset. Behold 0 under tuning: originalfunksjonen legger dette til også ved reversering.
int offsetA = 0;
int offsetB = 0;

// Midten av seks sensorer (0–5000). Endres bare ved annen sensorgeometri.
const int CENTER = 2500;
// +1 når linje til venstre gir negativ feil. Sett -1 hvis statisk sensortest viser motsatt fortegn.
const int SENSOR_DIRECTION = 1;
// Grunn-PWM på stabil rett linje. Høyere gir mer fart og krever raskere korrigering.
const float BASE_SPEED = 220.0f;
// Grunn-PWM i svinger/ved gjenfinning, ikke minste PWM per hjul. Lavere gir roligere kjøring, men kan gi stillstand.
const float CORNER_SPEED = 140.0f;
// Tak per motor. Høyere gir kraftigere ytre hjul i svinger; begge mål skaleres sammen ved metning.
const float MOTOR_LIMIT = 250.0f;
// P-forsterkning i PWM per posisjonsenhet. Høyere følger avvik hardere, men kan øke pendling.
const float KP = 0.045f;
// D-forsterkning i PWM per posisjonsenhet/sekund. Høyere demper bevegelse over linjen, men forsterker støy.
const float KD = 0.0055f;
// D-filterets tidskonstant i sekunder. Høyere filtrerer mer, men forsinker dempingen.
const float D_FILTER_SECONDS = 0.020f;
// Tak på svingkorreksjonen i PWM. Høyere gir krappere svinger; indre hjul kan stoppe.
const float MAX_TURN = 115.0f;
// Fartstap per posisjonsenhet. Høyere senker farten mer ved avvik fra sentrum.
const float ERROR_SPEED_DROP = 0.015f;
// Fartstap per posisjonsenhet/sekund. Høyere holder farten lavere mens roboten krysser linjen raskt.
const float RATE_SPEED_DROP = 0.001f;
// Maks økning av grunn-PWM per sekund. Lavere gir langsommere akselerasjon etter svinger.
const float BASE_ACCEL = 29.0f;
// Maks økning av hver motors PWM-magnitude per sekund. Lavere gir mykere, men tregere styrerespons.
const float MOTOR_ACCEL = 850.0f;
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
const uint16_t SENSOR_FLOOR = 700;
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
const uint32_t LOSS_CONFIRM_MS = 30;
// PWM-reduksjon per sekund i korte glipper. Høyere bremser hardere før bekreftet tap.
const float GAP_DECEL = 300.0f;
// Opptrapping av grunn-PWM per sekund under CORNER_SPEED, også ved start/gjenfunn.
// Høyere kommer raskere ut av motorenes svake lav-PWM-område; hjulrampene gjelder fortsatt.
const float RESTART_ACCEL = 300.0f;
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

static_assert(POSITION_FLOOR < SENSOR_FLOOR && SENSOR_FLOOR < TRACK_MIN_PEAK && TRACK_MIN_PEAK < LINE_MIN_PEAK,
              "Krev POSITION_FLOOR < SENSOR_FLOOR < TRACK_MIN_PEAK < LINE_MIN_PEAK");
static_assert(CORE_PERCENT > 0 && CORE_PERCENT <= 100 && GROUP_DOMINANCE > 1, "Ugyldig sensorgruppe-konfigurasjon");

void spinMotorA(int speedValue);
void spinMotorB(int speedValue);
void stopMotors();

float limitValue(float value, float low, float high) {
  return value < low ? low : (value > high ? high : value);
}

float magnitude(float value) {
  return value < 0 ? -value : value;
}

float approach(float value, float target, float step) {
  return value + limitValue(target - value, -step, step);
}

float rampMotor(float value, float target, float dt) {
  // Retningsskifte går via null i minst én oppdatering.
  if (value * target < 0) target = 0;
  float rate = magnitude(target) > magnitude(value) ? MOTOR_ACCEL : MOTOR_DECEL;
  return approach(value, target, rate * dt);
}

// Samme fremdriftsandel på begge hjul hindrer at vekslende korreksjoner bremser ned begge.
// Recovery og stopp bruker fortsatt de separate, raske bremserampene.
void rampFollowing(float &left, float &right, float targetLeft, float targetRight, float dt) {
  float deltaLeft = targetLeft - left, deltaRight = targetRight - right;
  float fraction = 1.0f;
  float rateLeft = magnitude(targetLeft) > magnitude(left) ? MOTOR_ACCEL : MOTOR_DECEL;
  float rateRight = magnitude(targetRight) > magnitude(right) ? MOTOR_ACCEL : MOTOR_DECEL;
  if (deltaLeft != 0) fraction = limitValue(rateLeft * dt / magnitude(deltaLeft), 0, fraction);
  if (deltaRight != 0) fraction = limitValue(rateRight * dt / magnitude(deltaRight), 0, fraction);
  left += fraction * deltaLeft;
  right += fraction * deltaRight;
}

// V = tydelig linje, L = svak kandidat, W = for svak, F = flat, B = bred, X = flere grupper.
struct LineReading {
  int error = 0;
  uint16_t peak = 0;
  char quality = 'W';
  bool valid = false;
  bool strong = false;
};

LineReading analyzeLine(const uint16_t *values) {
  LineReading result;
  uint16_t lowest = values[0], weights[SensorCount];
  for (uint8_t i = 0; i < SensorCount; i++) {
    if (values[i] > result.peak) result.peak = values[i];
    if (values[i] < lowest) lowest = values[i];
    weights[i] = values[i] > SENSOR_FLOOR ? values[i] - SENSOR_FLOOR : 0;
  }
  if (result.peak < TRACK_MIN_PEAK) return result;
  if (result.peak - lowest < LINE_MIN_CONTRAST) { result.quality = 'F'; return result; }

  uint16_t coreLevel = (uint32_t)(result.peak - SENSOR_FLOOR) * CORE_PERCENT / 100;
  uint16_t allCore = 0, bestWeight = 0;
  uint8_t bestStart = 0, bestEnd = 0;
  for (uint8_t i = 0; i < SensorCount;) {
    if (weights[i] < coreLevel || weights[i] == 0) { i++; continue; }
    uint8_t start = i;
    uint16_t groupWeight = 0;
    while (i < SensorCount && weights[i] >= coreLevel && weights[i] > 0) groupWeight += weights[i++];
    allCore += groupWeight;
    if (groupWeight > bestWeight) { bestWeight = groupWeight; bestStart = start; bestEnd = i - 1; }
  }
  if (bestWeight == 0 || (uint32_t)bestWeight < (uint32_t)GROUP_DOMINANCE * (allCore - bestWeight)) {
    result.quality = 'X'; return result;
  }
  if (bestEnd - bestStart + 1 > LINE_MAX_WIDTH) { result.quality = 'B'; return result; }

  // Svake nabosensorer bidrar mykt til posisjonen, men teller ikke som en egen linje/ekstra bredde.
  uint8_t first = bestStart, last = bestEnd;
  if (first > 0 && weights[first - 1] < coreLevel) first--;
  if (last + 1 < SensorCount && weights[last + 1] < coreLevel) last++;
  uint16_t total = 0;
  int32_t weightedError = 0;
  for (uint8_t i = first; i <= last; i++) {
    uint16_t weight = values[i] > POSITION_FLOOR ? values[i] - POSITION_FLOOR : 0;
    total += weight;
    // 1000 er posisjonsskala per sensoravstand; 32-bit hindrer overflow på AVR.
    weightedError += (int32_t)weight * ((int32_t)i * 1000 - CENTER);
  }
  result.error = SENSOR_DIRECTION * (int)(weightedError / total);
  result.strong = result.peak >= LINE_MIN_PEAK;
  result.quality = result.strong ? 'V' : 'L';
  result.valid = true;
  return result;
}

struct LineController {
  float left = 0, right = 0, base = 0, derivative = 0;
  int previousError = 0, lastSide = 0, searchSide = 0, candidateError = 0;
  uint32_t previousTime = 0, lossStart = 0, stableStart = 0, sideTime = 0, candidateStart = 0;
  bool initialized = false, derivativeReady = false, stableTiming = false;
  bool gapActive = false, recovering = false, braking = false;
  bool candidate = false, aligning = false, stopped = false;
  bool haveStrong = false, uncertain = false;
  int strongError = 0;
  uint32_t strongTime = 0, uncertainStart = 0;

  char mode() const {
    if (stopped) return 'S';
    if (uncertain) return 'U';
    if (!recovering) return gapActive ? 'G' : 'T';
    if (candidate) return aligning ? 'A' : 'C';
    return braking ? 'B' : 'R';
  }

  void stop() { left = right = 0; stopped = true; }

  void brake(float dt) {
    left = rampMotor(left, 0, dt);
    right = rampMotor(right, 0, dt);
  }

  void update(const LineReading &observation, uint32_t now) {
    if (stopped) return;
    float dt = initialized ? (uint32_t)(now - previousTime) / 1000.0f : CONTROL_PERIOD_MS / 1000.0f;
    if (dt <= 0) return;
    initialized = true;
    previousTime = now;
    float rampDt = dt < MAX_RAMP_DT ? dt : MAX_RAMP_DT;
    LineReading line = observation;
    if (line.valid && !line.strong) {
      // Svak kandidat får aldri starte/avslutte recovery eller endre sideminnet.
      line.valid = !recovering && haveStrong && (uint32_t)(now - strongTime) <= WEAK_HOLD_MS &&
                   magnitude(line.error - strongError) <= WEAK_MAX_SHIFT;
    }
    bool ambiguous = !line.valid && (line.quality == 'F' || line.quality == 'B' || line.quality == 'X');
    if (ambiguous) {
      if (!uncertain) { uncertain = true; uncertainStart = now; }
      if ((uint32_t)(now - uncertainStart) >= AMBIGUOUS_TIMEOUT_MS) { stop(); return; }
    } else uncertain = false;


    if (!recovering) {
      if (!line.valid) {
        if (!gapActive) { gapActive = true; lossStart = now; }
        stableTiming = false;
        derivativeReady = false;
        derivative = 0;
        if (ambiguous) {
          // Tvetydig sterkt signal er ikke bevis på at linjen ligger på en bestemt side.
          left = approach(left, 0, GAP_DECEL * rampDt);
          right = approach(right, 0, GAP_DECEL * rampDt);
          return;
        }
        if ((uint32_t)(now - lossStart) < LOSS_CONFIRM_MS) {
          left = approach(left, 0, GAP_DECEL * rampDt);
          right = approach(right, 0, GAP_DECEL * rampDt);
          return;
        }
        if (lastSide && (uint32_t)(now - sideTime) > SIDE_MAX_AGE_MS) lastSide = 0;
        searchSide = lastSide ? lastSide : UNKNOWN_SEARCH_SIDE;
        recovering = true;
        braking = true;
        candidate = false;
        aligning = false;
      } else gapActive = false;
    }

    if (recovering) {
      if (ambiguous) {
        candidate = false;
        aligning = false;
        braking = true;
        brake(rampDt);
        if ((uint32_t)(now - lossStart) >= SEARCH_TIMEOUT_MS) stop();
        return;
      }
      if (line.valid) {
        if (!candidate || magnitude(line.error - candidateError) > REACQUIRE_MAX_STEP) {
          candidateStart = now;
          candidate = true;
          aligning = false;
        }
        candidateError = line.error;
        // Et treff skal bremse rotasjonen straks, også ute på ytterste sensor.
        braking = true;
      } else { candidate = false; aligning = false; }

      uint32_t elapsed = (uint32_t)(now - lossStart);
      if (elapsed >= SEARCH_TIMEOUT_MS && (!candidate || elapsed >= SEARCH_TIMEOUT_MS + REACQUIRE_GRACE_MS)) {
        stop(); return;
      }
      bool confirmed = candidate && (uint32_t)(now - candidateStart) >= REACQUIRE_TIME_MS;
      if (candidate) {
        if (!confirmed || magnitude(line.error) <= REACQUIRE_ERROR) {
          aligning = false;
          brake(rampDt);
          if (!confirmed || left != 0 || right != 0) return;
          recovering = false;
          gapActive = false;
          braking = false;
          candidate = false;
          derivativeReady = false;
          derivative = 0;
          stableTiming = false;
          lastSide = 0;
          base = 0; // RESTART_ACCEL tar grunnfarten raskt tilbake, mens hjulrampene begrenser hvert steg.
        } else {
          int observedSide = line.error > 0 ? 1 : -1;
          if (aligning && observedSide != searchSide) {
            aligning = false;
            candidateStart = now;
            brake(rampDt);
            return;
          }
          if (!aligning) {
            brake(rampDt);
            if (left == 0 && right == 0) {
              // Bare et bekreftet sidefunn får endre retningen; ingen tidsstyrt vending.
              searchSide = observedSide;
              aligning = true;
            }
            return;
          }
          float turn = limitValue(KP * magnitude(line.error), ALIGN_MIN_SPEED, SEARCH_SPEED);
          left = rampMotor(left, searchSide * turn, rampDt);
          right = rampMotor(right, -searchSide * turn, rampDt);
          return;
        }
      } else {
        if (braking) {
          brake(rampDt);
          if (left == 0 && right == 0) braking = false;
        } else {
          left = rampMotor(left, searchSide * SEARCH_SPEED, rampDt);
          right = rampMotor(right, -searchSide * SEARCH_SPEED, rampDt);
        }
        return;
      }
    }

    int error = line.error;
    if (line.strong) { haveStrong = true; strongTime = now; strongError = error; }
    if (derivativeReady) {
      float rate = (error - previousError) / dt;
      float alpha = dt / (D_FILTER_SECONDS + dt);
      derivative += alpha * (rate - derivative);
    } else derivative = 0;
    previousError = error;
    derivativeReady = true;

    if (line.strong && (error > SIDE_MEMORY_ERROR || error < -SIDE_MEMORY_ERROR)) {
      lastSide = error > 0 ? 1 : -1;
      sideTime = now;
    } else if ((uint32_t)(now - sideTime) > SIDE_MAX_AGE_MS) lastSide = 0;
    bool stable = line.strong && magnitude(error) < STABLE_ERROR && magnitude(derivative) < STABLE_RATE;
    if (!stable) stableTiming = false;
    else if (!stableTiming) { stableTiming = true; stableStart = now; }
    if (stableTiming && (uint32_t)(now - stableStart) >= SIDE_CLEAR_MS) lastSide = 0;
    bool settled = stableTiming && (uint32_t)(now - stableStart) >= STABLE_TIME_MS;

    float targetBase = limitValue(BASE_SPEED - ERROR_SPEED_DROP * magnitude(error)
                                 - RATE_SPEED_DROP * magnitude(derivative), CORNER_SPEED, BASE_SPEED);
    if (!settled) targetBase = CORNER_SPEED;
    if (targetBase < base) base = approach(base, targetBase, BASE_DECEL * rampDt);
    else if (base < CORNER_SPEED) base = approach(base, CORNER_SPEED, RESTART_ACCEL * rampDt);
    else base = approach(base, targetBase, BASE_ACCEL * rampDt);
    float dTurn = limitValue(KD * derivative, -MAX_D_TURN, MAX_D_TURN);
    float turn = limitValue(KP * error + dTurn, -MAX_TURN, MAX_TURN);
    if (magnitude(error) >= EDGE_GUARD_ERROR && turn * error < 0) turn = EDGE_P_FACTOR * KP * error;
    float targetLeft = limitValue(base + turn, 0, MOTOR_LIMIT + MAX_TURN);
    float targetRight = limitValue(base - turn, 0, MOTOR_LIMIT + MAX_TURN);
    float peakMotor = targetLeft > targetRight ? targetLeft : targetRight;
    if (peakMotor > MOTOR_LIMIT) {
      targetLeft *= MOTOR_LIMIT / peakMotor;
      targetRight *= MOTOR_LIMIT / peakMotor;
    }
    rampFollowing(left, right, targetLeft, targetRight, rampDt);
  }
};

void printSnapshot(const LineReading &line, const LineController &control) {
  // Sensorer og styring fra samme avlesning. '?' betyr at posisjonen er ukjent.
  char errorText[8];
  if (line.valid) snprintf(errorText, sizeof(errorText), "%d", line.error);
  else snprintf(errorText, sizeof(errorText), "?");
  char message[80];
  int length = snprintf(message, sizeof(message), "%u,%u,%u,%u,%u,%u e%s %c%c q%d %d/%d\n",
                        sensorValues[0], sensorValues[1], sensorValues[2], sensorValues[3],
                        sensorValues[4], sensorValues[5], errorText, line.quality, control.mode(),
                        control.searchSide, (int)control.left, (int)control.right);
  if (length > 0 && length < (int)sizeof(message) &&
      (control.stopped || Serial.availableForWrite() >= length))
    Serial.write((const uint8_t*)message, length);
}

void calibrate() {
  delay(500);
  Serial.println("CALIBRATION STARTING");
  digitalWrite(LED_BUILTIN, HIGH);

  for (uint16_t i = 0; i < 200; i++) {
    reflectanceSensor.calibrate();
    delay(20);
  }

  digitalWrite(LED_BUILTIN, LOW);
  Serial.println("CALIBRATION DONE");
  Serial.println();
}

void follow2(unsigned long runTime) {
  LineController control;
  uint32_t runStart = millis();
  uint32_t lastUpdate = runStart - CONTROL_PERIOD_MS;
  uint32_t buttonChanged = runStart;
  bool rawButton = digitalRead(START) == LOW;
  bool stableButton = true;
  bool stopArmed = false;
  uint32_t lastDebug = runStart;

  while ((uint32_t)(millis() - runStart) < runTime) {
    uint32_t now = millis();
    bool pressed = digitalRead(START) == LOW;
    if (pressed != rawButton) {
      rawButton = pressed;
      buttonChanged = now;
    }
    if ((uint32_t)(now - buttonChanged) >= BUTTON_DEBOUNCE_MS && rawButton != stableButton) {
      stableButton = rawButton;
      if (!stableButton) stopArmed = true;
      else if (stopArmed) break;
    }
    if ((uint32_t)(now - lastUpdate) < CONTROL_PERIOD_MS) continue;
    lastUpdate = now;

    reflectanceSensor.readLine(sensorValues); // Én kalibrert avlesning; bibliotekets posisjon brukes ikke.
    LineReading line = analyzeLine(sensorValues);
    control.update(line, (uint32_t)millis());
    spinMotorB((int)control.left);
    spinMotorA((int)control.right);
    if (control.stopped || (uint32_t)(now - lastDebug) >= DEBUG_PERIOD_MS) {
      lastDebug = now;
      printSnapshot(line, control);
    }
    if (control.stopped) break;
  }
  stopMotors();
}

void waitForRelease(int pin) {
  uint32_t releasedSince = millis();
  while ((uint32_t)(millis() - releasedSince) < BUTTON_DEBOUNCE_MS) {
    if (digitalRead(pin) == LOW) releasedSince = millis();
  }
}

void setup() {
  pinMode(START, INPUT_PULLUP);
  pinMode(CALIBRATE, INPUT_PULLUP);
  pinMode(AIN1, OUTPUT);
  pinMode(AIN2, OUTPUT);
  pinMode(PWMA, OUTPUT);
  pinMode(BIN1, OUTPUT);
  pinMode(BIN2, OUTPUT);
  pinMode(PWMB, OUTPUT);
  pinMode(LED_BUILTIN, OUTPUT);
  Serial.begin(9600);
  Serial.println("Starting");

  reflectanceSensor.init();

  delay(500);
  digitalWrite(LED_BUILTIN, HIGH);

  // 200 målinger: høyere gir lengre tid til å føre sensorene over linje og bakgrunn.
  for (uint16_t i = 0; i < 200; i++) {
    reflectanceSensor.calibrate();
    delay(20); // Millisekunder mellom målinger: høyere gir lengre kalibreringstid.
  }

  digitalWrite(LED_BUILTIN, LOW);
  Serial.println("CALIBRATION DONE");
  Serial.println();
}

void loop() {
  if (digitalRead(START) == LOW && RUNNED == false) {
    RUNNED = true;
    follow2(40000); // Maks kjøretid i ms. Høyere kjører lenger uten stoppetrykk.
    RUNNED = false;
    waitForRelease(START);
  } else if (digitalRead(CALIBRATE) == LOW && CALIBRATED == false) {
    CALIBRATED = true;
    calibrate();
    CALIBRATED = false;
    waitForRelease(CALIBRATE);
  }
}

void stopMotors() {
  spinMotorA(0);
  spinMotorB(0);
}

void spinMotorA(int speedValue) {
  if (speedValue > 0) {
    digitalWrite(AIN1, HIGH);
    digitalWrite(AIN2, LOW);
  } else if (speedValue < 0) {
    digitalWrite(AIN1, LOW);
    digitalWrite(AIN2, HIGH);
  } else {
    digitalWrite(AIN1, LOW);
    digitalWrite(AIN2, LOW);
  }

  analogWrite(PWMA, abs(speedValue+offsetA));
}

void spinMotorB(int speedValue) {
  if (speedValue > 0) {
    digitalWrite(BIN1, HIGH);
    digitalWrite(BIN2, LOW);
  } else if (speedValue < 0) {
    digitalWrite(BIN1, LOW);
    digitalWrite(BIN2, HIGH);
  } else {
    digitalWrite(BIN1, LOW);
    digitalWrite(BIN2, LOW);
  }

  analogWrite(PWMB, abs(speedValue+offsetB));
}
