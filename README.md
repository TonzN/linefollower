# Linjefølger 2

Seks direkte midtlesere på en hoved-Nano, og en andre Nano for valgte ytterlesere. PC kobles med USB til **hovedkortet**.

```text
PC -- USB --> hoved-Nano --> motorer
                 |  |
     6 midtlesere | I2C
                    |
                ytter-Nano
                    |
       direkte analog / multipleksere
                    |
              valgte ytterlesere
```

## Status

- Gruppen observerer omtrent **to lesere over linjen på gammel sensor**, mot **4–5 på ny**. Dette er foreløpige observasjoner, ikke mål i mm eller dokumentert presisjon.
- `0J11785` peker mot QTRX-HD-25, men oppgitt `irs47j` avviker fra produsentens `irs45j`. **Bekreft modell og A/RC-utgave før kabling.** PCB-merkingen skiller ikke alene A fra RC.
- Ytterkortet som nå er implementert, leser **analoge spenninger**, direkte eller via mux. Det støtter ikke RC-avlesning. Direkte QTR RC er fortsatt tilgjengelig på hovedkortet.
- Pinner er ukonfigurerte, motorer og ytterkommunikasjon deaktivert. Ingen opplasting er utført.
- **Ytterdata logges i dette steget. Regulatoren bruker bare seks lokale lesere.** Sidesensorenes kalibrering, valg mellom flere linjefunn og bruk av ytterdata til recovery er neste steg.

Produsent: [identifikasjon](https://www.pololu.com/product/4325/specs), [RC-avlesning](https://www.pololu.com/docs/0J13/2), [analog avlesning](https://www.pololu.com/docs/0J13/3).

## Valgt arkitektur og alternativer

| Oppsett | Fordel | Ulempe |
| --- | --- | --- |
| Bare seks midtlesere | Enkel lokal regulering | Smalt avlest område på ny sensor |
| Seks midtlesere + seks direkte ytterlesere | Opptil tolv kanaler uten mux, kan dekke hele bredden | Glisnere målepunkter på sidene |
| Seks midtlesere + multipleksede ytterlesere | Lokal midtgruppe og mulighet for alle ytterpunktene | Mer elektronikk, sekvensiell avlesning og stabiliseringstid |
| Ett kort med nok innganger | Enklere tidskoordinering | Krever annet kort eller inngangselektronikk |

Hele bredden krever ikke alle leserne: ytterste aktive lesere bestemmer dekningen. Avstanden mellom aktive lesere bestemmer detaljnivået. En smal linje kan havne mellom glisne punkter.

Flere lesere kan forbedre posisjonsestimatet, men stopper ikke pendling automatisk. Støy, høyde, kalibrering, fart og PD-tuning må undersøkes. En tverrgående sensorrekke gir ikke alene et kart over neste sving. Flere svarte grupper må senere behandles som separate funn; gjennomsnitt mellom to streker kan havne på hvitt.

## Konfigurasjon

**Hovedkort: `include/board_config.h`**

- `SensorCount=6`; `SensorPins[0..5]` følger venstre–høyre rekkefølge. Sett `QtrAnalog` først etter bekreftet analogutgave.
- A4/A5 brukes til I2C. A0–A3 og A6/A7 er de seks analoge inngangene som gjenstår på klassisk Nano. Dette gir ingen ledige ADC-innganger til strøm-/spenningsmåling uten endret oppsett.
- `AnalogSamplesPerSensor` gir flere tidsprøver av samme kanal. Første ADC-konvertering etter kanalbytte forkastes.
- Sett motorpinner, polaritet og eventuell enable før `MotorsEnabled` aktiveres. Pinnekontrollen avviser kollisjoner.

**Ytterkort: `include/side_config.h`**, felles fil for begge kort:

- Sett `LINEFOLLOW_SIDE_ENABLED` til `1` når ytterkortet er konfigurert og tilkoblet.
- `MainIndices` angir midtgruppens fysiske utganger. Eksempelindeksene er foreløpige.
- Hver rad i `Inputs` er `{physicalIndex, pin, mux, channel}`. Nullbaserte indekser, sortert venstre–høyre. Velg utganger fritt uten å lese alle mellomliggende kanaler.
- Direkte: `{0,14,-1,0}` betyr fysisk leser 0 på A0. Mux: `{0,-1,0,3}` betyr leser 0 via kanal 3 på mux 0.
- `MuxSignalPins` angir ADC-inngangene fra to 16:1-muxer. De deler fire digitale `MuxSelectPins` (S0–S3). Enable må kobles aktiv iht. databladet.
- `SettleUs`, `DiscardFirstAdc`, `Samples` og `ScanPeriodUs` styrer avlesning. Verdiene er antakelser som må måles.
- `PollPeriodUs`, `MaxAgeMs`, `ClockHz`, `TimeoutUs` og `ServiceSlackUs` styrer overføring og aldersgrense.
- Ikke koble samme sensorutgang til begge kort. Bruk felles GND og passende I2C-pullups. Kun hovedkortet skal eventuelt styre sensorbrettets emitterpinner. Begge emittergrupper må være på for dette opptaksoppsettet.

Indekslistene er metadata, ikke mm. Regulatoren normaliserer fortsatt seks jevnt plasserte lokale lesere til 0–5000. Ujevn plassering av midtlesere krever endret posisjonsberegning.

## Timing og overføring

Hovedkortet kjører lokal regulering hver 8000 us, med foreløpig arbeidsgrense 6000 us og tillatt startforsinkelse 1000 us. For sen start under motorforsøk, for lang kontrollrunde, lokal sensorfeil eller utløpt PC-lease gir stopp. Watchdog dekker fastlåst programflyt. Dette er konfigurerte grenser, ikke målte garantier.

Ytterkortet leser én kanal per løkkegjennomgang og publiserer bare hele skann. Hovedkortet ber om et frosset sett og henter 32-byte pakker med CRC og felles sekvensnummer. Forrige komplette sett beholdes ved overføringsfeil. Én bussoperasjon utføres per servicekall når tidsplanleggeren viser nok ledig tid. Mål virkelig bussvarighet og startforsinkelse; timeout beviser ikke maksimal tid for en hel transaksjon.

Begge kort arbeider parallelt uten tråder. Verken venstre/høyre eller midt-/yttergruppene er nødvendigvis lest samtidig. USB-rapporten sendes i små deler når TX-bufferen har plass.

JSON/CSV inneholder `side_sequence`, `side_scan_us`, `side_age_ms`, `side_fresh`, `side_raw`, `side_indices` og `main_indices`. Alder regnes konservativt fra skannestart, inkludert overføringstid, ved PC-snapshot. Ukjente/foreldede ytterverdier blir null/tomme felt. Reell null beholdes. Ytterverdiene er rå ADC, uten egen kalibrering.

PC-opptak er nedprøvde og inneholder ikke alle kontrollrunder eller sideskann. Normal loggrate er ikke egnet til å karakterisere høyfrekvent sensorstøy.

## Bygg og tester

Åpne denne mappen i CLion. CMake bygger fem C++-testmål. Python-testene kjøres av testskriptet:

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run -e master_nano -e side_nano
.\.venv\Scripts\python.exe tools/run_tests.py
```

Miljøer med suffikset `_new_bootloader` finnes for begge Nanoer. Velg korrekt variant før opplasting.

Bygg også med den faktiske sensor-/motorprofilen aktivert. Ukonfigurerte verdier kan få kompilatoren til å fjerne kode. En byggekontroll med seks analoge midtlesere, motorutganger, ytterkommunikasjon og 19 multipleksede ytterlesere ga hovedkortet 1450 byte statisk RAM / 29292 byte flash, og ytterkortet 495 / 3924 byte. Stack og virkelig kjøretid må fortsatt måles. Prøvetildelingene er ikke lagret som robotens pinner.

## Hardware-testbenk

```powershell
python -m venv .venv
.\.venv\Scripts\python.exe -m pip install -r requirements.txt
.\.venv\Scripts\python.exe hardware_testbed.py --list-ports
.\.venv\Scripts\python.exe hardware_testbed.py --port COM5 --test sensors --seconds 5
```

Koble PC til hoved-Nano. Tester: `info`, `sensors`, `calibrate`, `timing`, `motor`, `sweep`, `follow`. Motorforsøk krever `--confirm-motion` og armering via programmet. Kalibrering gjelder midtgruppen og lagres bare i RAM. CSV/JSON lagres under `runs/`; råopptak overskrives ikke.

Start med sensoropptak og timing. Noter høyde, underlag, lys og forsyning manuelt. Mål kontrast, sideskannetid, alder på ytterdata og maksimal kontrolltid før tuning. PWM er pådrag, ikke kraft eller moment. RPM, strøm og spenning krever egne målekretser.

## Filkart og gjenbruk

- `src/master.cpp`: USB-kommandoer, oppdelt JSON-sending og koordinering.
- `src/nano.cpp`: felles kontroll-runtime, også brukt av eldre oppsett.
- `src/side_nano.cpp`: analog avlesning og komplette, frosne sideskann.
- `src/side_link.cpp`: henting, validering og målealder på hovedkortet.
- `src/hardware.cpp`: lokale sensorer, motorer og valgfritt måleutstyr.
- `include/side_protocol.h`: byteformat for sidemålinger.
- `src/control/`: gjenbrukt PD, recovery og ramper.
- `hardware_testbed.py`: PC-programmet og CSV-/JSON-opptak.

`LINE_MAX_WIDTH` er foreløpig økt fra tre til fem sterke kjernelesere ut fra observasjonen om linjebredde. Andre regulatorverdier er ikke retunet.

De eldre miljøene `nano`, `nano_new_bootloader` og `uno` er beholdt for Uno-bridgen. De bruker ikke ytterkoblingen. `KODE_FORKLART.md` er historisk forklaring fra før todelingen. Git-historikk og tidligere 3D-modeller er bevart.
