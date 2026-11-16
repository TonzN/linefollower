# LinjeFølger testbed

- [Kode forklart](docs/KODE_FORKLART.md)
- [Teknologier](docs/TEKNOLOGIER.md)
- [Samlet simuleringsveiledning](../docs/SIMULASJON.md)

**Trinn 1: en kjørbar Python-emulator av linjefølgeren, med en enkel fysisk modell og pillebane.** Prosjektet er klart til å åpnes som mappe i VS Code.

Arduino-styringen er portert fra `C:\Users\tonzg\CLionProjects\LinjeFølger1` 29. oktober 2026. Originalprosjektet er ikke endret. En kopi av firmware og SHA-256-kontrollsummer ligger i `firmware/`.

Vi bygger dette videre i steg. Dette trinnet har ingen reinforcement learning-agent, belønningsfunksjon eller treningsløype. Målet er først å ha en forståelig og testbar robotmodell som dagens styring kan kjøre i.

## Start i VS Code

Etter kloning av branchen, åpne undermappen `simulator/` i VS Code. Kjør terminalkommandoene fra denne undermappen.

Opprett et virtuelt miljø med `python -m venv .venv`. Velg `.venv\Scripts\python.exe` som interpreter i VS Code. Prosjektet krever Python 3.10 eller nyere med Tkinter, og ingen ekstra Python-pakker. Python-utvidelsen og debuggeren i VS Code trengs for F5; terminalkommandoene fungerer uten dem.

Kjør i PowerShell-terminalen i prosjektmappen:

```powershell
.\.venv\Scripts\python.exe -m testbed
```

Vinduet starter på pause. Velg **Start / pause** eller mellomrom for å kjøre. **Stopp** låser styringen til stopp, mens motorene har en fysisk nedbremsing. **Nullstill** gir en ny kjøring med samme startposisjon og seed. **Last konfig på nytt** leser JSON-filen og nullstiller simuleringen.

F5 har ferdige profiler for vindu og kjøring uten vindu. VS Code har også oppgaver for emulator, tester og C++-sammenligning.

For å lage miljøet på nytt ved behov:

```powershell
python -m venv .venv
```

## Endre roboten

Rediger `configs/default.json`, lagre og velg **Last konfig på nytt**. Alle fysiske standardverdier er **eksempler**, ikke målte egenskaper til roboten deres.

| Seksjon | Hva dere kan endre |
| --- | --- |
| `robot` | Kroppens lengde/bredde, hjulenes senteravstand, hjuldiameter, masse, treghetsmoment, grep og rullemotstand. |
| `sensors` | Antall, total bredde mellom ytterste sensorers sentre, avstand foran hjulakselen, sideforskyvning, monteringsvinkel, hver lesers måleflate, kontrast, støy og utfall. |
| `left_motor`, `right_motor` | Min/maks RPM, PWM-dødsone, akselerasjon, nedbremsing, forskjell i motorstyrke, moment og anslått strøm. |
| `power` | Spenning, referansespenning, indre motstand, batterikapasitet, ladenivå og valgfri utladning. |
| `track` | Lengde på rettstrekk, svingradius og linjebredde. |
| `initial_pose` | Startposisjon og retning. |
| `simulation` | Seed, fysikksteg og visningsintervall. |
| `controller` | Firmwarekonstanter med samme navn som i C++. |

Meter, kilogram og sekunder er standardenhetene. Navn som inneholder `rpm`, `ms`, `deg`, `v`, `a` og `ohm` angir andre enheter. Ikke bland millimeter og meter: 85 mm skrives `0.085`.

Sensoravstanden mellom naboer beregnes som `span_m / (count - 1)`. `footprint_width_m` er målebredden til **hver** leser. Dette er to forskjellige bredder.

`sensors.count` styrer `SensorCount`, og sentrum beregnes automatisk som `(count - 1) * 500`. Dagens seks sensorer gir 2500. `CENTER` kan overstyres i `controller` ved et bevisst eksperiment. Andre feilgrenser og regulatorverdier skaleres ikke automatisk ved endret sensorantall: det vil kreve ny tuning.

For en egen profil, kopier hele `default.json` og bruk:

```powershell
.\.venv\Scripts\python.exe -m testbed --config configs/min_robot.json
```

Se [detaljert konfigurasjon](docs/KONFIGURASJON.md) for betydning og begrensninger. `configs/firmware_defaults.json` er originalverdiene fra C++, og `testbed/config_template.json` beskriver forventet struktur. Endre den vanlige profilen, ikke disse referansefilene.

## Kjør uten vindu og lagre målinger

```powershell
.\.venv\Scripts\python.exe -m testbed --headless --seconds 20 --csv runs/test.csv
```

Dette simulerer 20 sekunder, skriver telemetri til CSV og gir siste tilstand som JSON i terminalen. Kjøringen er raskere enn sanntid. Ingen Python-pakker eller grafikkort kreves.

CSV inneholder blant annet posisjon, sensorverdier, målekvalitet, modus, motorpådrag, RPM, spenning og avstand til linjen. `time_ms` er fysikktidspunktet; `sensor_time_ms` er tidspunktet for siste kontrollmåling. Sensorene beholdes mellom kontrollrundene.

Mørk linje på lyst underlag er standard. Roboten starter på øvre rettstrekk, vendt mot høyre. Koordinatsystemet har x mot høyre, y opp og positiv vinkel mot klokken. Robotens posisjon er midt mellom drivhjulene.

## Hva som er emulert

- Sensorgruppering, kontrastkrav og vektet posisjonsberegning fra `analyzeLine()`.
- PD-regulator, filter, ekstrastyring, grunnfart og samordnede hjulramper.
- Svake signaler, linjetap, sidehukommelse, blindt søk og innretting ved gjenfunn.
- Tvetydige mønstre, tidsfrister og låst stopp.
- Millisekundtid med uint32-omslag, heltallsdivisjon mot null og heltallskonvertering før motorutgang.
- Retningsvalg og motoroffset som i `motor.cpp`, også den skjeve effekten av offset ved revers.

Python bruker double-presisjon. Dette er ikke en AVR-emulator med identiske flyttallsbiter eller kjøretider. `tools/verify_firmware.py` sammenligner målinger, moduser og alle 36 tilstandsfelt mot uendret C++ fra snapshoten.

Det første verifiserte datasettet har **21 361 kontrollrunder** og dekker T, G, U, B, R, A og S. Målinger, moduser og heltallstilstander samsvarte. Flyttall sammenlignes med toleranse fordi C++ bruker float. Rapporten skrives til `build/parity_report.json` når testen kjøres.

## Modellens grenser på dette trinnet

Fysikken er en enkel differensialmodell. RPM, moment, masse, grep og spenning påvirker bevegelsen, men modellen er ikke validert mot roboten. Grepsgrensen er en forenklet begrensning av akselerasjon og svingevne, ikke en detaljert modell av dekk og underlag. Kroppens lengde/bredde brukes til visning; treghetsmomentet oppgis separat. Kollisjoner modelleres ikke.

Sensormodellen genererer **kalibrerte 0–1000-verdier** fra linjens dekning under hver måleflate. Den etterligner ikke RC-utladning, emittertiming eller råkalibrering i Zumo-biblioteket. Sensorhøyde og materialenes optiske egenskaper er foreløpig representert gjennom måleflate, kantmykhet, gain og kontrast. Elektrisk pinnekonflikt, knappesprett, `setup()`-kalibreringssekvens og seriell bufferbelastning simuleres ikke. Vinduets kontroller erstatter knappene.

Strøm og spenningsfall er grove anslag. Dette er ingen kretsmodell eller nøyaktig batterikjemi. Minimumsspenning er et gulv, ikke simulering av brownout eller helt tomt batteri. Motorenes standardparametre må måles før vi kan stole på overføring fra simulering til robot.

Firmwarekonstanter som bare gjelder Arduino-pinner, debug eller knapper er bevart som referanse, men påvirker ikke fysikken. `REACQUIRE_ERROR` er fortsatt ubrukt, akkurat som i C++.

## Tester

```powershell
.\.venv\Scripts\python.exe -m unittest discover -s tests -v
.\.venv\Scripts\python.exe tools/verify_firmware.py
```

Første kommando tester emulatoren uten kompilator. Andre krever `g++` på PATH og bygger C++-referansen lokalt. Den laster ikke ned noe og berører ikke Arduino-prosjektet. Referansetesten gjelder seks sensorer og originalkonstantene, også hvis dere har endret standardprofilen.

## Hvor koden ligger

| Fil | Ansvar |
| --- | --- |
| `testbed/controller.py` | Python-port av firmware. Samme tilstandsnavn som C++ for enkel sammenligning. |
| `testbed/world.py` | Pillebane og sensorverdier. |
| `testbed/physics.py` | Motorrespons, strømforsyning og robotbevegelse. |
| `testbed/simulation.py` | Klokke, reset og koblingen mellom delene. |
| `testbed/viewer.py` | Enkel Tkinter-visning. |
| `testbed/config.py` | Innlesing og validering av konfigurasjon. |
| `testbed/__main__.py` | Kommandolinje og CSV-logging. |
| `firmware/` | Referansekopi av C++ og konfigurasjonen som porten er laget fra. |
| `tests/` | Tester av oppførsel, konfigurasjon og fysikkegenskaper. |
| `tools/verify_firmware.py` | Bygger og kjører C++-referansen mot samme sensorsekvenser. |

## Neste steg – etter at vi har sett på emulatoren

Se [måleplan og modelluttrykk](docs/MALEPLAN_OG_MODELL.md) for sensorbakgrunn, interferens, start-/gangterskler, moment, RPM, strøm og grep. En tom registreringsmal ligger i [measurements](measurements/README.md). Ingen fysiske målinger er lagt inn ennå.

1. Mål faktisk geometri, RPM, batterispenning og sensorverdier. Juster en robotprofil mot målingene.
2. Test dagens styring på flere startposisjoner, grep og baner. Bestem hva som skal telles som en god runde eller linjetap.
3. Definer hva en treningsagent skal bestemme: direkte motorpådrag eller regulatorinnstillinger. Det valget avgjør action- og observation-formatet.
4. Lag treningsgrensesnitt, belønning og episoderegler. Først da legger vi til treningsbibliotek og en RL-agent.

## Kontrollert publisering

Publisert på branchen `codex/simulering` 16. november 2026. Alle 19 programtester og referansesammenligningen med 21 361 kontrollrunder passerer. Batteritesten sammenligner nå spenningsfallet mot forsyningen i profilen, fremfor en fast 6 V-grense.
