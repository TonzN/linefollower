# Linjefølger – veiledning for gruppen

Denne roboten skal følge en linje ved å styre to motorer hver for seg. Den har Arduino Nano, en egen 3D-printet kropp og en sensorrekke med seks lesere foran motorene.

Dokumentasjonen beskriver koden som ble lest 22. oktober 2026 i `C:\Users\tonzg\CLionProjects\LinjeFølger1`. Den er laget for at nye gruppemedlemmer skal kunne forstå koden og fortsette arbeidet. Oppkoblingen og kjøringen er ikke kontrollert på selve roboten.

## Start her

1. Les `src/main.cpp` for å forstå oppstart, knapper og kjøringen.
2. Les `analyzeLine()` i `src/linefollow.cpp` for å se hvordan sensorverdier blir til en linjeposisjon.
3. Les `LineController::update()` i samme fil for å forstå styringen.
4. Bruk konfigurasjonsfilene når dere skal justere oppførselen.

## Slik bruker dere roboten

1. Slå på roboten. Den kalibrerer automatisk ved oppstart.
2. Før alle seks leserne over både linjen og underlaget under kalibreringen. Koden beveger ikke motorene under kalibrering, så dette må gjøres manuelt. Selve måleserien tar omtrent fire sekunder, i tillegg til oppstartstid og sensoravlesninger. Lampen lyser under serien.
3. Plasser roboten med sensoren over linjen.
4. Trykk START og slipp knappen. Roboten begynner å følge linjen.
5. Trykk START på nytt for å stoppe. Knappen må først ha vært sluppet for at et nytt trykk skal stoppe kjøringen.

CALIBRATE starter en ny kalibrering når roboten ikke kjører. Denne knappen blir ikke behandlet under kjøring. Kalibreringen gjentas etter omstart; koden lagrer den ikke i permanent minne.

## Hva filene gjør

| Fil | Innhold |
| --- | --- |
| `src/main.cpp` | Setter opp maskinvaren, kalibrerer, leser knapper og kobler sensorer, styring og motorer sammen. Skriver også diagnose til serieporten. |
| `src/linefollow.h` | Beskriver `LineReading` og `LineController`, altså målingen og minnet styringen trenger. |
| `src/linefollow.cpp` | Tolker sensorene, følger linjen, søker etter mistet linje og retter inn roboten ved gjenfunn. |
| `src/motor.cpp` og `.h` | Sender retning og PWM til motorene. Inneholder også en funksjon for gradvise motorendringer. |
| `src/helper.cpp` og `.h` | Små hjelpefunksjoner: begrense en verdi, finne absoluttverdi og bevege en verdi gradvis mot et mål. |
| `src/conf.h` | Pinner og hovedinnstillinger for fart, styring, sensorgrenser og tidsfrister. |
| `src/recovery_conf.h` | Innstillinger for søking og overgangen tilbake til vanlig følging. |
| `src/capture_conf.h` | Finjustering av innretting når linjen blir synlig igjen. |
| `src/ambiguous_conf.h` | Kort viderebevegelse gjennom et uklart sensorområde. |
| `platformio.ini` | Valg av kort, Arduino-rammeverk og sensorbibliotek. |

## Fra sensor til motor

Under kjøring går informasjonen denne veien:

```text
Seks sensorverdier
    -> analyzeLine(): hvor er linjen, og er målingen troverdig?
    -> control.update(): beregn venstre og høyre motorpådrag
    -> spinMotorB(left) og spinMotorA(right)
```

`follow2()` i `main.cpp` kjører denne runden med en ønsket periode på 8 ms. Sensorlesing og annen behandling kan gjøre at en runde tar lengre tid. Styringen bruker faktisk medgått tid i beregningene sine.

### 1. Finne linjen

`readLine(sensorValues)` fyller listen med kalibrerte verdier på skalaen 0–1000. Koden bruker disse verdiene, men ignorerer posisjonen som bibliotekets `readLine()` returnerer. Oppsettet forventer en mørk linje på et lysere underlag.

`analyzeLine()` gjør i hovedsak dette:

1. Finner høyeste og laveste sensorverdi og sjekker signalstyrke og kontrast.
2. Samler sterke utslag i sammenhengende grupper.
3. Velger den tydeligste gruppen. Flere konkurrerende grupper eller en for bred gruppe blir avvist.
4. Beregner et vektet gjennomsnitt fra den valgte gruppen og nærmeste svake naboer. Større sensorutslag påvirker posisjonen mer.

Sensorene får posisjonene 0, 1000, 2000, 3000, 4000 og 5000. Midten er 2500, mellom de to midterste leserne.

```text
feil = posisjon - 2500       (når SENSOR_DIRECTION = 1)
```

Med forventet sensorrekkefølge betyr negativ feil linje til venstre, positiv feil linje til høyre og feil nær null linje i midten. Dette må bekreftes med en statisk sensortest på roboten.

Resultatet lagres i `LineReading`:

| Felt | Betydning |
| --- | --- |
| `error` | Avstand fra midten i sensorens posisjonsskala, ikke millimeter. |
| `peak` | Høyeste sensorverdi. |
| `valid` | Sensoranalysen fant en brukbar linjekandidat. |
| `strong` | Kandidaten har tilstrekkelig sterk toppverdi. |
| `quality` | En bokstav som forklarer typen måling. |

En svak kandidat kan bare brukes kortvarig under vanlig følging, når den ligger nær en nylig sterk måling. Den kan ikke bekrefte at en mistet linje er funnet igjen. Derfor kan en kandidat ha `valid = true` i sensoranalysen og likevel bli avvist inne i styringen.

### 2. Følge linjen

`LineController` husker blant annet forrige feil, motorpådrag og hvilken side linjen sist ble sett på. `update()` velger handling ut fra den nye målingen og dette minnet.

Vanlig følging bruker PD-regulering:

- **P-leddet** styrer ut fra hvor langt linjen er fra midten: `KP * error`.
- **D-leddet** reagerer på hvor raskt feilen endrer seg. Det filtreres og begrenses for å redusere støy og brå utslag.

Forenklet beregnes motorene slik:

```text
venstre = grunnfart + svingkorreksjon
høyre   = grunnfart - svingkorreksjon
```

En linje til høyre gir dermed mer pådrag på venstre hjul og mindre på høyre. Roboten svinger mot linjen. Ved store avvik legges ekstra svingstyrke til.

Grunnfarten går mot `CORNER_SPEED` når følgingen er ustabil, og får øke mot `BASE_SPEED` etter en sammenhengende stabil periode. Pådragene endres gradvis gjennom ramper. Hvis et hjulmål overstiger `MOTOR_LIMIT`, skaleres begge mål ned med samme forhold.

Under vanlig følging får hjulene bare mål fremover eller null. Under søking og innretting kan hjulene gå i motsatt retning slik at roboten roterer på stedet.

### 3. Når linjen forsvinner

Ved en kort glipp senkes pådraget. Hvis tapet varer minst `LOSS_CONFIRM_MS`, går styringen over i søk, også kalt recovery.

Roboten søker mot siden der linjen sist ble sett, hvis dette minnet er ferskt. Uten et ferskt hint bremser den først og søker deretter i standardretningen, som nå er høyre. Søket bytter ikke side bare fordi det har gått en bestemt tid.

Når en sterk linje dukker opp, begynner roboten å rette seg inn med en gang. Den går tilbake til vanlig følging først når treffet er bekreftet, linjen er nær nok midten, posisjonen endrer seg sakte nok og motorpådragene passer for overgangen.

### 4. Når sensorene gir et uklart mønster

Et bredt felt, lite kontrast eller flere konkurrerende linjegrupper behandles som tvetydig informasjon. Dette er en annen situasjon enn at linjen bare er borte.

Med et ferskt retningsgrunnlag kan roboten kort fortsette med samme forhold mellom hjulene for å komme ut av området. Deretter bremses hjulene mot null. Ved vedvarende tvetydighet stopper den etter `AMBIGUOUS_TIMEOUT_MS`, nå 500 ms. Koden har ingen egen ruteplan for veikryss.

## Oppkobling som koden forventer

| Funksjon | Arduino-pinne |
| --- | --- |
| Høyre motor: AIN1 / AIN2 / PWMA | D13 / D12 / D6 |
| Venstre motor: BIN1 / BIN2 / PWMB | D8 / D9 / D10 |
| START | D7 |
| CALIBRATE | D3 |

Knappene bruker `INPUT_PULLUP` og regnes som trykket når inngangen er LOW. Dette tilsvarer knapper mellom inngang og GND.

Sensorpinnene står ikke i `conf.h`: `reflectanceSensor.init()` bruker standardene i det installerte Zumo-biblioteket. For ATmega328P/168 er sensorrekkefølgen D4, A3, D11, A0, A2, D5, med emitterstyring på D2. Kontroller dette mot faktisk sensormodell og kabling før dere bygger videre.

Motor A er høyre motor, og motor B er venstre. Positive verdier er ment som fremover, negative som bakover og null som stoppkommando. PWM er pådrag, ikke målt hjulhastighet. Koden har ingen tilbakemelding fra hjulenkodere.

## Bygging og opplasting

Prosjektet bruker PlatformIO med Arduino-rammeverket og Pololus Zumo-sensorbibliotek. Åpne prosjektmappen i CLion med eksisterende PlatformIO-oppsett, eller bruk PlatformIO fra terminalen.

**Avklar kortvalget først:** `platformio.ini` inneholder nå `[env:uno]` og `board = uno`, selv om roboten deres bruker Nano. Miljønavnet er bare et navn; kortvalget må passe den faktiske Nano-varianten og bootloaderen for bygging og opplasting.

Når kortoppsettet er avklart:

```text
pio run
pio run --target upload
pio device monitor --baud 115200
```

## Lese diagnose fra serieporten

Serieporten bruker 115200 baud. Under kjøring forsøker koden å skrive en linje omtrent hvert 60. ms. Vanlige utskrifter kan hoppes over hvis sendebufferen er full.

Eksempel på formatet, med oppdiktede verdier:

```text
100,180,850,900,200,100 e45 VT q0 185/180
```

- De seks første tallene er sensorverdiene i rekkefølge.
- `e45` betyr feil +45. `e?` betyr at sensoranalysen ikke har en gyldig posisjon.
- `VT` består av to bokstaver: først målekvalitet, så styringsmodus.
- `q0` er lagret søkeretning: −1 venstre, +1 høyre, 0 ikke valgt ennå. Den er ikke nødvendigvis aktiv utenfor søk.
- `185/180` er venstre/høyre motorpådrag.

| Målekvalitet | Betydning |
| --- | --- |
| `V` | Sterk, brukbar linje. |
| `L` | Svak linjekandidat. |
| `?` | Ingen brukbar kandidat, for eksempel for svakt signal. |
| `F` | For liten kontrast mellom sensorene. |
| `B` | Den valgte gruppen er for bred. |
| `X` | Ingen tilstrekkelig dominerende gruppe. |

Kommentaren i `main.cpp` nevner `W` for svakt signal, men implementasjonen returnerer nå `?` i dette tilfellet.

| Styringsmodus | Betydning |
| --- | --- |
| `T` | Vanlig linjefølging. |
| `G` | Kort glipp før bekreftet linjetap. |
| `U` | Tvetydig sensorinformasjon. |
| `B` | Nedbremsing før blindt søk. |
| `R` | Søker etter linjen. |
| `A` | Linjen er sett igjen, og roboten retter seg inn. |
| `C` | Kandidat uten aktiv innretting; definert i `mode()`, men ser ikke ut til å brukes i dagens kjørevei. |
| `S` | Styringen har stoppet, for eksempel ved tidsfrist. |

Stopp med knappen avslutter kjøreløkken direkte og gir ikke nødvendigvis en utskrift med `S`.

## Justere og videreutvikle

Endre én ting om gangen, og prøv på samme bane mellom endringene. Skriv ned verdien før og etter, og hva roboten gjorde.

| Det dere vil endre | Begynn med |
| --- | --- |
| Fart på rettstrekk og i sving | `BASE_SPEED`, `CORNER_SPEED` i `conf.h`. Nå 200 og 180. |
| Hvor hardt den styrer mot linjen | `KP`, og ved store avvik `EXTRA_TURN_GAIN`. |
| Pendling rundt linjen | Undersøk sensorverdiene, deretter `KP`, `KD` og `D_FILTER_SECONDS`. |
| Brå motorendringer | `MOTOR_ACCEL`, `MOTOR_DECEL` og grunnfartens ramper. |
| Feilaktig linjetap | Kalibrering først, deretter signal- og kontrastgrenser i `conf.h`. |
| Blind søkerotasjon | `SEARCH_SPEED` og `RECOVERY_SEARCH_LIMIT`. |
| Innretting etter gjenfunn | `recovery_conf.h` og `capture_conf.h`. |
| Oppførsel over brede eller uklare felt | `ambiguous_conf.h` og `AMBIGUOUS_TIMEOUT_MS`. |

Kommentarene ved konstantene forklarer mer om virkningen av høyere og lavere verdier.

Før en endring prøves i fart:

1. Kontroller med hjulene løftet at begge motorer går fremover ved positiv kommando.
2. Kontroller at en linje til venstre gir negativ feil, og en linje til høyre positiv feil. Rett opp sensorrekkefølge eller `SENSOR_DIRECTION` hvis testen viser motsatt fortegn.
3. Test start, slipp av START og nytt trykk for stopp.
4. Test rett linje, svinger, mistet linje, gjenfunn og et bredt felt ved lavere fart.
5. Bygg prosjektet og noter testresultatet sammen med endringen.

Når dere legger til funksjoner, behold samme arbeidsdeling: maskinvare og knapper i `main.cpp`, sensoranalyse og kjøreatferd i `linefollow.cpp`, motorutganger i `motor.cpp` og justerbare verdier i konfigurasjonsfilene.