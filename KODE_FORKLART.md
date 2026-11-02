# Kode forklart – ESP32-versjonen

Dette er én ESP32-S / NodeMCU-32S med tolv valgte lesere. Programmet har ingen I2C-forbindelse og intet slavekort. Den gamle [Nano-forklaringen](docs/historikk/KODE_FORKLART_NANO.md) er arkivert for rapporten.

## Leserekkefølge

| Fil | Ansvar |
| --- | --- |
| `include/board_config.h` | Antall, sensorutvalg, GPIO, mux, motorer og tidsgrenser |
| `include/board_pins.h` | Hva NodeMCU-32S-pinnene kan brukes til |
| `src/master.cpp`, nederst | Arduino `setup()` og `loop()` |
| `include/control_runtime.h` | Grensesnittet fra PC-kommunikasjon til kontrollprogram |
| `src/control_runtime.cpp` | Testmodi, kalibrering, kontrollrunde og feilstopper |
| `src/control/linefollow.cpp` | Linjeanalyse, PD, linjetap og recovery |
| `src/hardware.cpp` | ADC, valgfri mux, LEDC-PWM og encoder-avbrudd |
| `src/platform_runtime.cpp` | ESP32 task-watchdog |
| `telemetry.py`, `hardware_testbed.py` | Rapporttolking, kommandoer og CSV/JSON på PC |

## Hovedløkken

`setup()` starter kontrollprogram og seriellport. `loop()` kaller først `control_runtime::tick()`. Deretter behandles litt PC-trafikk dersom neste kontrollrunde ikke er for nær. Når det er mer enn 2 ms igjen, gis en kort pause til FreeRTOS-oppgavene. Nær fristen venter vi ikke.

Selv om ESP32 har flere kjerner og FreeRTOS, kjører kontroll og PC-behandling her i **samme Arduino-oppgave**. Vi trenger ikke egne tråder. Encoder-avbruddene deler derimot pulstellere med hovedløkken og bruker ESP32s critical sections.

## En kontrollrunde

`tick()` behandler heartbeat og prioritert stopp, sjekker PC-lease og kjører `cycle()` når tidspunktet er nådd. Vanlige kommandoer behandles i ledig tid.

`cycle()` utfører:

1. Registrer starttid, periode og eventuell forsinkelse.
2. `readControlSensors()` leser alle valgte kanaler og anvender lokal kalibrering.
3. `finishExpiredTest()` avslutter forsøket dersom tiden er ute.
4. `applyMotorOutput()` lar regulatoren eller motorforsøket bestemme utgangene.
5. Oppdater valgfritt måleutstyr og kontroller arbeidstiden.

Tidsplanleggeren holder en fast tidsakse og hopper over tapte perioder. Den kjører ikke flere gamle runder rett etter hverandre. Tidsdifferanser regnes usignert for å håndtere telleromløp. Perioden er foreløpig 8 ms, arbeidsgrensen 6 ms og startforsinkelsesgrensen 1 ms.

## Pinnekontroll og ADC

`hw::begin()` sjekker hele oppsettet før utganger aktiveres. En pinne kan bare reserveres én gang. UART, flash, ikke-tilgjengelige pinner og normalt strapping-pinner avvises. GPIO34–39 kan leses, men ikke drive motorutganger eller mux-valg. Encoder med intern pullup må også bruke en pinne som faktisk har dette.

`SensorInputs` består av `{physicalIndex, pin, muxChannel}`. Direkte kanal bruker en ADC-pin og mux=-1. Mux-kanal bruker pin=-1 og kanal 0..15. Etter kanalbytte kan vi vente litt og forkaste første ADC-konvertering. Gjennomsnitt tas bare over flere tidsprøver av samme kanal.

ADC-oppløsningen er eksplisitt 12 bit: råverdier 0–4095. Dette er ikke millivolt. Valgfri strøm-/spenningsmåling bruker `analogReadMilliVolts()` og egne skaleringsverdier. ADC2 brukes av flere kanaler; WiFi skal derfor ikke startes i dette oppsettet.

## Kalibrering og fysisk posisjon

Kalibreringen lagrer minimum/maksimum per valgt kanal mens roboten føres over linje og underlag. Den avvises hvis én kanal har for liten variasjon. Verdiene ligger bare i RAM.

```text
kalibrert = 1000 * (råverdi - minimum) / (maksimum - minimum)
posisjon = physicalIndex * 5000 / (PhysicalSensorCount - 1)
feil = vektet posisjon - 2500
```

Kalibrert verdi begrenses til 0–1000. Posisjonen er en normalisert skala, ikke millimeter. Den bruker fysisk indeks fordi de tolv valgte leserne ikke nødvendigvis ligger med lik avstand. GPIO-nummeret sier ingenting om plasseringen på sensorrekken.

`analyzeLine()` sjekker signalstyrke og kontrast, finner sterke nabogrupper og krever at én gruppe dominerer. For bred gruppe avvises. Bredden beregnes som avstand mellom de fysiske indeksene pluss én, slik at glisne målepunkter ikke tolkes som en smal gruppe. Usamplede områder er fortsatt ukjent informasjon.

| `quality` | Betydning |
| --- | --- |
| `V` | Sterkt og gyldig funn |
| `L` | Svakere funn; krever passende fersk historikk |
| `F` | For liten kontrast |
| `B` | For bred gruppe |
| `X` | Ingen dominerende gruppe |
| `?` | Ingen godkjent analyse |

## PD, ramper og recovery

P-leddet bruker posisjonsfeilen. D-leddet bruker filtrert endring i feilen per sekund. Filteret demper støy, men gir også forsinkelse. En svingverdi legges til venstre motor og trekkes fra høyre. Metning og ramper begrenser utgangene.

Et kort linjetap behandles som et mulig gap. Ved lengre tap bremses/søkes det med siste troverdige retning. Et gjenfunn må bekreftes og roboten innrettes før normal følging gjenopptas. Tvetydige funn får egen håndtering før vanlig PD. Disse algoritmene er videreført; verdiene er ikke fysisk retunet for ESP32 og det nye utvalget.

| `control_mode` | Betydning |
| --- | --- |
| `T`, `G`, `U` | Følging, kort gap, tvetydighet |
| `B`, `R` | Bremsing før søk, recovery/søk |
| `C`, `A`, `S` | Kandidat til gjenfunn, innretting, stopp |

`mode` er et annet felt: den ytre testmodusen idle, kalibrering, sensorer, timing, motor, sweep eller follow.

## Motorer, watchdog og stopp

Hver motor har to retningspinner og en LEDC-PWM-utgang. Frekvensen er konfigurerbar; pådragsskalaen er fortsatt −255..255. PWM er ikke målt fart eller kraft.

Task-watchdog overvåker Arduino-oppgaven. Det er et separat vern på ett sekund, ikke en garanti om at en 6-ms-runde avbrytes straks. Oppstartsfeil i maskinvare eller watchdog blir synlig som feil. Armering avvises uten fungerende watchdog selv etter CLEAR.

STOP har en prioritert kanal og forkaster en vanlig kommando som venter. PC-lease, sensorfeil og tidsgrensebrudd kan også stoppe roboten. Encoderverdier kopieres i en kort critical section; ADC og regulering ligger utenfor.

## Kommunikasjon og testing

`host::dispatch()` tolker kommandolinjer. `host::output()` sender små deler når TX-bufferen har plass. Det frosne målebildet er stabilt mens S3-rapporten sendes. Python kontrollerer CRC, lengde og indekser, og lager lesbare felter og CSV. Ingen sidedata eller I2C-sider sendes.

Kjør `tools/run_tests.py`, `tools/format_code.py --check` og bygg `esp32`. `esp32_fixture` og `esp32_mux_fixture` sjekker aktive kodeveier; de er ikke koblingsskjemaer. Bruk `tools/measure_memory.py` for minnetall.

Nyttige C++-uttrykk: `namespace` grupperer funksjoner, `struct` samler data, `constexpr` er kompileringstidsverdier, `uint16_t` er 16-bit uten fortegn, og `float&` lar en funksjon oppdatere variabelen til den som kalte den. En header beskriver grensesnittet; `.cpp` implementerer det. Kodestilen er forklart i [KODESTIL](docs/KODESTIL.md).
