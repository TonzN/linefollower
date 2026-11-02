# Kode forklart – dagens linjefølger

Denne veiledningen er for deg som skal lese, feilsøke eller videreutvikle programmet. Les [robotoversikten](docs/LINJEFOLGEREN.md) først for maskinvare og arbeidsfordeling. Stilen vi bruker videre står i [KODESTIL](docs/KODESTIL.md).

## Les filene i denne rekkefølgen

| Fil | Hva du finner |
| --- | --- |
| `include/board_config.h`, `include/side_config.h` | Konfigurasjon og foreløpige antakelser |
| `src/master.cpp`, nederst | Hovedkortets `setup()` og `loop()` |
| `include/control_runtime.h` | Grensesnitt fra kommunikasjon til kontrollprogram |
| `src/control_runtime.cpp`: `tick()`, `cycle()` | Prioritering og én kontrollrunde |
| `src/control/linefollow.cpp`: `analyzeLine()` | Sensorverdier til posisjonsfeil |
| Samme fil: `LineController::update()` | PD, linjetap, søk og gjenfinning |
| `src/hardware.cpp` | Faktisk lesing og skriving av pinner |
| `src/side_nano.cpp`, `src/side_link.cpp` | Ytteravlesning og I2C |

## C++-uttrykk du møter

- `.h` beskriver typer og funksjoner andre filer kan bruke. `.cpp` implementerer dem.
- `namespace` grupperer navn. `control_runtime::tick()` er funksjonen `tick` i gruppen `control_runtime`.
- `struct` samler data. `LineReading` beskriver resultatet av én linjeanalyse.
- `uint16_t` er et usignert 16-bit heltall, 0–65535. `int16_t` kan også være negativt.
- `constexpr` betyr her at verdien er kjent ved kompilering. Endret konfigurasjon krever nytt bygg og opplasting.
- `const uint8_t*` er en peker til bytes som funksjonen kan lese uten å endre dem.
- `float&` er en referanse. En funksjon kan da endre variabelen til den som kalte den; rampene bruker dette for venstre/høyre pådrag.
- `volatile` brukes blant annet når data kan endres fra avbrudd. Det gjør ikke automatisk flerdelte operasjoner atomiske.
- `ATOMIC_BLOCK` beskytter en kort kopiering/endring mot avbrudd. Lange beregninger og sensoravlesning skal ligge utenfor.
- `PSTR` og `pgm_read_byte` lagrer og leser faste tekster i AVR-flash i stedet for SRAM.
- `#if defined(LINEFOLLOW_MASTER)` velger kode ved bygging, ikke mens roboten kjører.

## Hovedkortets hovedløkke

`setup()` starter kontroll-runtime og seriellporten. `loop()` gjør følgende:

1. Kall `control_runtime::tick()` først.
2. Behandle litt USB-trafikk hvis det er nok tid før neste kontrollrunde.
3. Utfør ett steg av ytterkommunikasjonen hvis det er nok tid til det.

Kontrollprogrammet har en vanlig header og separat implementasjon. `master.cpp` inkluderer ikke lenger `nano.cpp` og omdøper ikke `setup`/`loop` med makroer.

### `tick()` – prioriteringen

Først mates watchdog. Deretter behandles heartbeat, prioritert stopp og eventuell utløpt PC-lease. Hvis kontrollrunden er klar, kjøres den før annen kommandohåndtering. Vanlige kommandoer behandles i ledig tid.

Heartbeat betyr at PC-programmet fortsatt kommuniserer. Det er ikke en fysisk nødstopp. En fastlåst funksjon må returnere eller bli avbrutt av watchdog før programmet kommer videre.

### `cycle()` – én kontrollrunde

1. Registrer start, periode og forsinkelse.
2. `readControlSensors()` leser og kalibrerer de seks lokale verdiene.
3. `finishExpiredTest()` avslutter et tidsbegrenset forsøk når tiden er ute.
4. `applyMotorOutput()` lar riktig modus bestemme motorpådraget.
5. Oppdater valgfritt måleutstyr, mål arbeidstiden og sjekk grensen.

Dette er en tidsplanlagt løkke, ikke et RTOS. `Scheduler` holder en fast tidsakse. Tapte perioder hoppes over i stedet for at mange gamle kontrollrunder kjøres rett etter hverandre. Usignerte tidsdifferanser håndterer telleromløp for de korte intervallene vi bruker.

## Fra råverdi til posisjonsfeil

Analog råverdi er normalt 0–1023. Under kalibrering registreres minimum og maksimum per lokal kanal. Før hver leser over både linje og underlag. For liten variasjon gjør at kalibreringen avvises.

```text
kalibrert = 1000 * (råverdi - kanalens minimum) / kanalens variasjon
```

Resultatet begrenses til 0–1000. Dette gjør kanalene mer sammenlignbare, men fjerner ikke all støy eller påvirkning fra lys og høyde. Kalibreringen lagres bare i RAM.

`analyzeLine()` finner sterkeste/svakeste verdi, begrenser svake bidrag og avviser for svakt signal eller for liten kontrast. Deretter finner den grupper av sterke nabolesere. Én gruppe må dominere, og den må ikke være for bred. Posisjonen beregnes fra et vektet gjennomsnitt innenfor den valgte gruppen, med bidrag fra svake naboer.

Feilen er posisjon minus sentrum. Seks jevnt plasserte lokale lesere bruker skala 0–5000 og sentrum 2500. Dette er ikke millimeter. Fysiske ytterindekser brukes foreløpig bare til logging.

| `quality` | Betydning |
| --- | --- |
| `V` | Gyldig og sterkt linjefunn |
| `L` | Svakere funn; regulatoren krever også fersk, passende historikk |
| `F` | For liten kontrast, flatt målebilde |
| `B` | For bred sterk gruppe |
| `X` | Ingen tydelig dominerende gruppe |
| `?` | Ingen godkjent analyse eller utilstrekkelig signal |

`LINE_MAX_WIDTH` er foreløpig fem sterke kjernelesere. Observasjonen om 4–5 lesere over linjen betyr ikke at alle fem alltid klassifiseres som sterke ved enhver vinkel.

## PD og motorramper

`LineController::update()` får en `LineReading` og tiden i **millisekunder**. Tidssteget omregnes til sekunder inne i funksjonen.

P-leddet er `KP * error`: større avvik gir mer styring. D-leddet bruker hvor raskt feilen endres. `D_FILTER_SECONDS` filtrerer feilendringen. Filteret demper støy, men tilfører forsinkelse. Koden har i tillegg kantstyring og begrensninger; den er mer enn bare to multiplikasjoner.

Fremdriften reduseres ved ustabil kjøring. En svingverdi legges til venstre mål og trekkes fra høyre. For høye mål skaleres ned, og rampene begrenser endringstakten. Dette er PWM-ramper, ikke en målt modell av hjulakselerasjonen.

## Linjetap og gjenfinning

Tvetydige funn behandles før vanlig følging. Ved tvetydighet kan regulatoren kort videreføre et begrenset pådrag fra fersk historikk, deretter bremse og til slutt stoppe etter tidsgrensen.

Ved et kort vanlig linjetap bremses roboten som ved et gap. Ved lengre tap brukes en fersk sidehistorikk eller konfigurert standardretning til søk. Når en sterk linje blir synlig, bremses gammel rotasjon og roboten innrettes. Gjenfunn må bekreftes før vanlig følging overtar.

| `control_mode` | Betydning |
| --- | --- |
| `T` | Vanlig følging |
| `G` | Kort linjetap/gap |
| `U` | Tvetydig målebilde |
| `B` | Bremsing før søk |
| `R` | Recovery/søk |
| `C` | Kandidat til gjenfunn |
| `A` | Innretting ved gjenfunn |
| `S` | Regulatoren er stoppet |

Dette er regulatorens kjøremodus. Telemetrifeltet `mode` er derimot den ytre testmodusen: idle, kalibrering, sensorer, timing, motor, sweep eller follow.

## Ytterkort og målealder

`side_nano.cpp` validerer pinner og leser én kanal per løkkegjennomgang. En mux-kanal velges med fire digitale pinner, får stabilisere seg og leses på ADC-inngangen. Flere tidsprøver av samme kanal kan gjennomsnittberegnes.

Arbeidssettet endres mens skannet pågår. Først et helt skann publiseres som fullført. `Latch` fryser det fullførte settet i ferdige I2C-sider. En henting midt i neste skann får derfor ikke en blanding av to skann.

`side_link.cpp` bruker navngitte steg: `Idle`, `SelectPage` og `ReadPage`. Hvert servicekall utfører én bussoperasjon. Alle sider må ha riktig CRC, sekvens og metadata før data publiseres. Alderen på forrige sett øker videre hvis en ny overføring feiler.

Ytterdata er foreløpig rådata til logging. De påvirker ikke motorene og har ikke egen kalibrering ennå.

## PC-kommandoer og rapporter

`host::dispatch()` tolker en hel kommandolinje og legger kommandoen i kontrollkøen. `STOP` har en prioritert vei. `host::output()` sender en liten del av svaret når UART-bufferen har plass.

Målerapporten sendes i format S2. `telemetry.py` sjekker rapporten og gir feltene navn. CSV og lesbar JSON lages på PC, slik at Nano slipper mye tekstformatering. Kommando- og feilsvar er fortsatt korte JSON-linjer. Se [PROTOKOLL](docs/PROTOKOLL.md) for format og offsettabell.

## Hvor endrer jeg ting?

| Endring | Start her |
| --- | --- |
| Pinner, polaritet, kontrollperiode | `include/board_config.h` |
| Ytterutvalg, mux og målealder | `include/side_config.h` |
| P/D, fart og normale terskler | `include/control/conf.h` |
| Gjenfunn og tvetydighet | `recovery_conf.h`, `capture_conf.h`, `ambiguous_conf.h` |
| Lokal sensor eller motorutgang | `src/hardware.cpp` |
| Ny testmodus | `control_runtime.cpp`, kommandoformat og Python-testbenk |
| CSV, statistikk og PC-visning | `hardware_testbed.py`, `telemetry.py` |

Kjør `tools/run_tests.py` ved funksjonelle endringer og `tools/format_code.py --check` før commit. Bygg begge kort og kontroller minne ved firmwareendringer. Se [MINNEBRUK](docs/MINNEBRUK.md).

Den tidligere forklaringen er bevart i [historikk](docs/historikk/KODE_FORKLART_v1.md). Den er ikke fasit for dagens oppstart og kommunikasjon.
