// V9: jevnere posisjon og samordnet hjulramping; svak linje og tvetydig mønster skilles.
#include <ZumoReflectanceSensorArray.h>
#include <stdio.h>
#include "motor.h"
#include "helper.h"
#include "conf.h"
#include "linefollow.h"

unsigned int sensorValues[SensorCount];

bool CALIBRATED = false;
bool RUNNED = false;

// Eksplisitte prototyper hindrer Arduino i å plassere disse før typene er deklarert.
struct LineReading;
struct LineController;
void printSnapshot(const LineReading &line, const LineController &control);

ZumoReflectanceSensorArray reflectanceSensor;

static_assert(POSITION_FLOOR < SENSOR_FLOOR && SENSOR_FLOOR < TRACK_MIN_PEAK && TRACK_MIN_PEAK < LINE_MIN_PEAK,
              "Krev POSITION_FLOOR < SENSOR_FLOOR < TRACK_MIN_PEAK < LINE_MIN_PEAK");
static_assert(CORE_PERCENT > 0 && CORE_PERCENT <= 100 && GROUP_DOMINANCE > 1, "Ugyldig sensorgruppe-konfigurasjon");

// V = tydelig linje, L = svak kandidat, W = for svak, F = flat, B = bred, X = flere grupper.

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

