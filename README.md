# Linjefølger – ESP32-S / NodeMCU-32S

Én ESP32, uten I2C eller slavekort. Standardoppsettet har **12 valgte analoge lesere**, med støtte for valgfri analog multiplekser på samme kort.

- [Robot og pinnevalg](docs/LINJEFOLGEREN.md)
- [Kode forklart](KODE_FORKLART.md)
- [Kodestil](docs/KODESTIL.md)
- [Protokoll](docs/PROTOKOLL.md)
- [Minnebruk](docs/MINNEBRUK.md)
- [Nano mot ESP32: ytelse, sensor og strømforsyning](docs/NANO_VS_ESP32.md)

## Hvorfor tolv?

Brikken har 18 ADC-kanaler, men NodeMCU-32S-profilen har normalt 16 ført ut. GPIO37/38 er ikke kortpinner i denne profilen. Fire tilgjengelige ADC-pinner påvirker oppstarten og blokkeres som standard: GPIO0, 2, 12 og 15.

De tolv øvrige ADC-pinnene er GPIO4, 13, 14, 25, 26, 27, 32, 33, 34, 35, 36 og 39. GPIO1/3 er reservert til USB-UART, 6..11 til flash, og 34..39 kan ikke brukes som motorutganger.

Kortprofilen er valgt ut fra «32s». Bekreft fysisk modell og pinout før kabling. WiFi skal ikke aktiveres når ADC2 brukes; programmet starter verken WiFi eller Bluetooth. **Ikke koble 5 V-signaler til ESP32-GPIO.** Sensorens A/RC-utgave er fortsatt ikke bekreftet; denne porten leser analoge spenninger.

Kilder: [ESP32 ADC](https://docs.espressif.com/projects/esp-idf/en/v4.4.8/esp32/api-reference/peripherals/adc.html), [byggeprofil](https://docs.platformio.org/en/stable/boards/espressif32/nodemcu-32s.html), [Arduino-ESP32 ADC](https://docs.espressif.com/projects/arduino-esp32/en/latest/api/adc.html).

## Konfigurasjon

Alt står i `include/board_config.h`. Standardpinnene er −1, sensoren er `Unconfigured` og motorene er deaktivert.

- `SensorCount` må stemme med antall rader i `SensorInputs`.
- Hver rad er `{physicalIndex, pin, muxChannel}`. `{12,32,-1}` betyr fysisk leser 12 på GPIO32.
- Indeksene er nullbaserte, sortert venstre–høyre. Utvalget er foreløpig og tettere nær midten.
- Posisjonsberegningen bruker fysisk indeks, slik at ulike mellomrom ikke behandles som lik avstand.
- `PhysicalSensorCount` er foreløpig 25. Bekreft sensormodellen.
- `Sensors=Analog` aktiverer ADC. Råskala er 0–4095; kalibrert skala er fortsatt 0–1000.
- `MotorPwmHz` styrer LEDC-frekvensen. Regulatorens PWM-skala er fortsatt 0–255.
- Ugyldige og doble pinner avvises. Oppstartspinner tillates bare med eksplisitt `AllowBootPins` etter elektrisk vurdering.

For mux: sett `MuxEnabled=true`, velg `MuxSignalPin` og fire `MuxSelectPins`. En rad `{index,-1,channel}` bruker mux-kanal 0..15. Enable kobles aktiv iht. databladet. Noen direkte ADC-pinner kan måtte frigjøres til valgpinner; å slå på en bool skaper ikke fire ekstra GPIO. Flere tidsprøver av samme kanal kan gjennomsnittberegnes uten å blande ulike lesere.

## Bygg og test

Åpne mappen i CLion eller VS Code med PlatformIO. Bygg `esp32` fra PlatformIO-panelet eller kjør:

```powershell
pio run -e esp32
python tools/run_tests.py
python tools/format_code.py --check
python tools/measure_memory.py
```

Bruk prosjektets `.venv` ved Python-kommandoene. Utviklerverktøy installeres fra `requirements-dev.txt`.

Plattformen er låst til `espressif32@6.12.0`, Arduino-ESP32 2.0.17. `esp32_fixture` bygger med tolv direkte kanaler og motorer. `esp32_mux_fixture` bruker åtte direkte og fire mux-kanaler. Fixture-pinner er byggeeksempler, ikke godkjent kabling. Ingen opplasting er utført.

## Kontroll og PC-testbenk

Kontrollperioden er foreløpig 8 ms, arbeidsgrensen 6 ms og tillatt startforsinkelse 1 ms. Task-watchdog er et separat vern på ett sekund. Maskinvare og watchdog kontrolleres før armering.

```powershell
python hardware_testbed.py --list-ports
python hardware_testbed.py --port COM5 --test sensors --seconds 5
```

Tester: info, sensors, calibrate, timing, motor, sweep og follow. Motorforsøk krever `--confirm-motion`. Kalibreringen gjelder alle valgte lesere og ligger bare i RAM. PC logger nedprøvde målebilder, ikke alle kontrollrunder.

ESP32 sender S3 med CRC. Python lager feltnavn og CSV/JSON. Nye rapporter har ingen slave-/I2C-felter. Oppdater firmware og Python sammen. PC-programmet kan fortsatt lese eldre S2-/JSON-rapporter.

Testene dekker regulator, ujevne sensorposisjoner, kontrollflyt, stopp, UART, CRC, GPIO, ADC, LEDC og mux. De bekrefter ikke fysisk sensorrespons eller faktisk kjøretid på roboten.

## Historikk til rapporten

To-Nano-løsningen ligger urørt på `fase2`, i [commit 7faf126](https://github.com/TonzN/linefollower/tree/7faf12686bfa70a4d3174b0da3893507ab42d8e3). Dokumentasjonen er også arkivert under `docs/historikk/`. Denne branchen heter `fase2_esp32`, siden Git ikke tillater mellomrom i branchnavn.
