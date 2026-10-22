# Kode forklart – linjefølgeren

Denne veiledningen er for deg som skal lese og endre koden, også hvis deler av den ble laget uten at du fikk forklart hvordan de virker. Du trenger ikke forstå alle formlene med en gang. Begynn med programflyten, og bruk resten som oppslagsverk.

Forklaringen bygger på kildekoden i `C:\Users\tonzg\CLionProjects\LinjeFølger1`, lest 22. oktober 2026. Kodeutdrag merket som forenklet viser prinsippet; de erstatter ikke den faktiske koden. Fysisk oppførsel må testes på roboten. Når et mulig formål forklares, er dette en tolkning av koden, ikke en bekreftet forklaring fra den opprinnelige utvikleren.

## 1. Den viktigste sammenhengen

Roboten har tre forskjellige oppgaver:

1. **Tolke målingen:** Hvilke sensorer ser linjen, og kan vi stole på det?
2. **Bestemme handling:** Skal roboten følge, bremse, søke eller rette seg inn?
3. **Sende motorpådrag:** Hvilken retning og PWM skal hvert hjul få?

Disse oppgavene ligger hovedsakelig i henholdsvis `analyzeLine()`, `LineController::update()` og `spinMotorA/B()`.

```text
main.cpp: les sensorene
    |
    v
analyzeLine(sensorValues) -> LineReading
    |
    v
control.update(line, nå)  -> control.left og control.right
    |
    v
spinMotorB(left)          -> venstre motor
spinMotorA(right)         -> høyre motor
```

`LineReading` er en beskrivelse av én måling. `LineController` er både styringen og minnet fra tidligere runder. Uten minnet kunne roboten ikke beregne feilendring, huske søkeretning eller vente på bekreftet gjenfunn.

## 2. Litt C++ som gjør koden lettere å lese

| Uttrykk | Betydning her |
| --- | --- |
| `.h` og `.cpp` | Headeren beskriver typer og funksjoner; cpp-filen inneholder vanligvis selve funksjonskoden. Konfigurasjonsheaderne definerer også verdier. |
| `struct` | Samler flere variabler, og eventuelt funksjoner, i én type. |
| `LineController control;` | Lager en ny styring med startverdiene fra `linefollow.h`. |
| `control.left` | Feltet `left` i denne styringen. |
| `LineController::update` | Funksjonen `update` som tilhører `LineController`. |
| `const LineReading &line` | Leser den eksisterende målingen uten å lage en vanlig kopi og uten å få endre den gjennom denne referansen. |
| `float &left` | En referanse som kan endres. Funksjonen endrer variabelen hos den som kalte den. |
| `const unsigned int *values` | Tilgang til sensorlisten. `values[i]` leser element nummer `i`; funksjonen endrer ikke elementene gjennom denne pekeren. |
| `vilkår ? a : b` | Velg `a` hvis vilkåret er sant, ellers `b`. |
| `&&`, `||`, `!` | OG, ELLER, IKKE. |
| `+=` og `*=` | Legg til eller gang inn i verdien som allerede står der. |
| `return` | Avslutt denne funksjonskjøringen nå. Inne i `update()` betyr det at en annen kjøremåte ikke skal kjøre etterpå i samme runde. |
| `static` foran `rampCapture()` | Funksjonen er lokal for denne cpp-filen. |
| `const` etter `mode()` | Funksjonen leser styringens tilstand uten å endre den. |
| `static_assert(...)` | Sjekk under bygging. Byggingen feiler hvis vilkåret er usant. |
| `#pragma once` / include guards | Hindrer at samme header behandles flere ganger i samme kompileringsenhet. |

`uint8_t`, `uint16_t` og `uint32_t` er heltall uten minus, med henholdsvis 8, 16 og 32 biter. `int32_t` kan også være negativt. `float` brukes til desimaltall. Endelsen `f`, som i `0.013f`, markerer en float-konstant.

## 3. `main.cpp`: hva skjer fra roboten slås på?

### Globale variabler

- `sensorValues[SensorCount]` har plass til seks sensorverdier. Første element er nummer 0.
- `reflectanceSensor` er objektet som kommuniserer med sensorrekken gjennom Zumo-biblioteket.
- `RUNNED` er et flagg rundt kjøringen. Navnet betyr ikke at roboten har kjørt tidligere.
- `CALIBRATED` er et flagg rundt manuell kalibrering. Det er **ikke** en lagret bekreftelse på at roboten er kalibrert: det settes tilbake til false etterpå, og automatisk kalibrering oppdaterer det ikke.

### `setup()` – én gang ved oppstart

Setter knapper som innganger med intern pullup, og motorpinner som utganger. Starter serieporten på 115200 baud og initialiserer sensorbiblioteket.

Etter 500 ms slås lampen på og sensoren kalibreres 200 ganger med 20 ms pause mellom. Alle lesere må føres over både linje og bakgrunn. Motorene kjøres ikke gjennom en automatisk kalibreringsbevegelse. Deretter slås lampen av.

### `loop()` – venter på en oppgave

START kaller `follow2(1000000000)`. CALIBRATE kaller `calibrate()`. Hvis begge knappene er trykket når denne koden sjekker dem, prioriteres START på grunn av `if ... else if`.

Funksjonene blokkerer: mens `follow2()` kjører, venter `loop()` til den er ferdig. Derfor behandles stoppknappen inne i `follow2()`, mens CALIBRATE ikke behandles under kjøring.

### `calibrate()` – kalibrering på nytt

Gjør omtrent samme måleserie som i `setup()`, med en ekstra melding om at den begynner. Prosjektets funksjon nullstiller ikke eksplisitt bibliotekets gamle kalibreringsdata før måleserien. Ikke legg inn kode som er avhengig av en full nullstilling uten først å kontrollere bibliotekets kalibreringsoppførsel.

### `follow2(runTime)` – selve kjøringen

Lager først en ny `LineController`. Dermed blir minnet fra forrige kjøring borte, og motorpådragene i styringen starter på null.

Hver runde gjør følgende:

1. Leser og filtrerer START-knappen.
2. Venter til det har gått minst `CONTROL_PERIOD_MS` siden forrige kontrollrunde.
3. Leser sensorene og kjører `analyzeLine()`.
4. Oppdaterer styringen med målingen og et nytt tidspunkt.
5. Sender `left` til motor B og `right` til motor A. Desimaltallene gjøres om til heltall.
6. Skriver eventuelt en diagnoselinje.
7. Avslutter hvis styringen har satt `stopped`.

Når knappen, tidsgrensen eller styringen avslutter løkken, kalles alltid `stopMotors()` på den normale veien ut av funksjonen.

`lastUpdate = runStart - CONTROL_PERIOD_MS` gjør at første kontrollrunde kan kjøre med en gang. `continue` hopper til neste runde i while-løkken når det ennå ikke er tid for en ny sensormåling. Dette er aktiv venting, ikke en `delay()`.

### Hvorfor er stoppknappen så omstendelig?

Mekaniske knapper kan veksle raskt mellom av og på under ett trykk. Dette kalles kontaktsprett. Koden godtar en endring først etter `BUTTON_DEBOUNCE_MS`, nå 25 ms, med uendret råtilstand.

- `rawButton`: sist avleste tilstand.
- `buttonChanged`: tidspunktet råtilstanden sist endret seg.
- `stableButton`: sist godkjente tilstand.
- `stopArmed`: om starttrykket har blitt sluppet, slik at neste trykk kan stoppe.

`stableButton` begynner som true fordi kjøringen startes med en trykket knapp. Slipp aktiverer stoppmuligheten. Neste godkjente trykk avslutter kjøringen.

`waitForRelease(pin)` venter etterpå på at knappen har vært sluppet sammenhengende i 25 ms. Det hindrer at et holdt trykk starter en ny kjøring eller kalibrering med en gang.

### `printSnapshot()` – diagnose uten for mye forsinkelse

`snprintf()` lager tekst i en fast buffer og får vite bufferens størrelse. Koden sjekker at teksten fikk plass. Under vanlig kjøring skrives den bare hvis serieportens sendebuffer har nok ledig plass, slik at logging i mindre grad forsinker styringen. Ved stopp forsøkes utskrift også uten denne plassjekken.

Sensorverdiene og `LineReading` er fra samme måling. Men loggen viser målingen **før** `update()` eventuelt avviser en svak kandidat i sin lokale kopi. En `L` med tall etter `e` er derfor ikke alene bevis på at styringen brukte målingen til vanlig følging.

Se bokstavforklaringene i [README](README.md#lese-diagnose-fra-serieporten).

## 4. `helper.cpp`: tre små verktøy

### `limitValue(value, low, high)`

Holder verdien innenfor grensene. Eksempler:

```cpp
limitValue(300, 0, 225)  // 225
limitValue(-20, 0, 225)  // 0
limitValue(100, 0, 225)  // 100
```

Funksjonen forutsetter at nedre grense ikke er større enn øvre grense.

### `magnitude(value)`

Gir absoluttverdien: `magnitude(-80)` blir 80. Brukes når størrelsen betyr noe uansett venstre/høyre eller fremover/bakover.

### `approach(value, target, step)`

Går mot målet med maksimalt `step`, uten å passere målet.

```cpp
approach(100, 200, 12)  // 112
approach(195, 200, 12)  // 200
approach(100, 0, 12)    // 88
```

Dette er grunnlaget for rampene: i stedet for å hoppe rett til et nytt motorpådrag tas et lite steg hver kontrollrunde. Steglengden må være ikke-negativ.

## 5. `motor.cpp`: pådrag og ramper

### `spinMotorA()` og `spinMotorB()`

Disse er nesten like, men bruker forskjellige pinner. Fortegnet velger retningssignaler; størrelsen sendes som PWM gjennom `analogWrite()`.

| Kommando | Retningspinne 1 | Retningspinne 2 |
| --- | --- | --- |
| Positiv | HIGH | LOW |
| Negativ | LOW | HIGH |
| Null | LOW | LOW |

Hva LOW/LOW fysisk gjør med motoren, avhenger av motordriveren. Navn som `brake()` andre steder i koden beskriver nedtrapping av kommandoen; de dokumenterer ikke at driveren bruker elektrisk kortslutningsbrems.

Motorfunksjonene begrenser ikke selv kommandoen til et gyldig PWM-område. Dagens styring begrenser pådragene før kallene. Hvis dere lager nye direkte motorkall, må også de ha gyldige grenser.

`abs(speedValue + offsetA)` legger offset til før absoluttverdien tas. Med offset 10 får +100 størrelsen 110, mens −100 får størrelsen 90. Nullkommando får PWM 10. Derfor bør ikke offset endres uten at denne logikken er vurdert. Nå er begge offset null.

### `stopMotors()`

Sender null direkte til begge motorfunksjonene. Det går ikke gjennom en rampe.

### `rampMotor(value, target, dt)`

`dt` er tiden i sekunder. Hvis opptrappingen er 1400 PWM per sekund og `dt = 0.008`, er største økning 11,2 PWM i denne runden.

Hvis `value * target < 0`, har nåværende pådrag og målet motsatt fortegn. Funksjonen setter da det midlertidige målet til null. Pådraget må ned til null før det kan bygges opp i motsatt retning i en senere oppdatering.

Funksjonen bruker `MOTOR_ACCEL` når pådragets størrelse skal øke og `MOTOR_DECEL` når den skal avta.

### `rampCapture()` i `linefollow.cpp`

Samme prinsipp, men nedtrappingen bruker `RECOVERY_BRAKE_RATE`, nå 3000 PWM/s. Den fjerner tidligere rotasjonspådrag raskere når roboten finner linjen igjen. Med 8 ms blir maksimalt steg ned 24 PWM. Opptrappingen bruker fortsatt `MOTOR_ACCEL`.

### `rampFollowing()` – hvorfor en felles brøk?

Ved vanlig følging skal begge hjul bevege seg samme andel av veien mot hvert sitt mål. Funksjonen regner først ut hvor stor andel hvert hjul kan klare innenfor sin rampegrense, og velger den minste andelen for begge.

Eksempel med 8 ms:

```text
Venstre: 100 -> 200, altså +100. Tillatt økning er 11,2.
Høyre:   100 ->  50, altså  -50. Tillatt reduksjon er 9,6.

Venstre tillater andelen 11,2 / 100 = 0,112.
Høyre tillater andelen   9,6 / 50  = 0,192.
Begge bruker 0,112:
Nytt venstre = 111,2. Nytt høyre = 94,4.
```

Dette samordner overgangen mellom hjulmålene. Kommentaren tyder på at hensikten er å motvirke at vekslende korreksjoner stadig trekker ned begge hjul. Det er ingen måling eller garanti for faktisk hjulhastighet. Denne funksjonen har heller ikke samme eksplisitte nullpassering som `rampMotor()`; dagens vanlige følging lager bare ikke-negative mål.

## 6. `analyzeLine()`: fra seks tall til en troverdig posisjon

Funksjonen begynner med en `LineReading` der `valid = false`, `strong = false` og `quality = '?'`. Hvis en sjekk feiler, returnerer den tidlig. Dermed må en måling passere alle sjekkene for å bli gyldig.

### A. Fjern svake bidrag og sjekk kontrast

`peak` blir høyeste sensorverdi og `lowest` den laveste. For hver sensor beregnes:

```text
weights[i] = maks(sensorverdi - SENSOR_FLOOR, 0)
```

Med `SENSOR_FLOOR = 500` blir en verdi på 300 til vekt 0, og 900 blir vekt 400. Dette er bakgrunnsfjerning for gruppevalget.

Hvis `peak < 700`, returneres den ugyldige standardmålingen. Hvis forskjellen mellom topp og bunn er mindre enn 300, returneres kvalitet `F`. At alle sensorer ser mye mørkt gir altså ikke nødvendigvis en gyldig linje.

### B. Finn sammenhengende sterke grupper

`coreLevel` er en relativ grense basert på toppens bakgrunnskorrigerte verdi. Ved topp 900 er toppvekten 400, og med `CORE_PERCENT = 50` blir grensen 200. Sensorer med vekt minst 200 inngår da i sterke grupper.

En gruppe består av sterke sensorer som ligger rett ved siden av hverandre. Koden summerer vektene i hver gruppe, husker den tyngste og sammenligner den med resten:

```text
bestWeight >= GROUP_DOMINANCE * (allCore - bestWeight)
```

Med dagens faktor 3 må beste gruppe veie minst tre ganger summen av de andre sterke gruppene. Hvis ikke returneres `X`. Deretter avvises en valgt gruppe med mer enn tre sterke sensorer som `B`.

### C. Regn ut posisjonen mykere

Ved posisjonsberegningen tas også én svak nabo på hver side med, hvis den finnes. Her brukes `POSITION_FLOOR = 350`, ikke 500. Det lar svakere naboutslag flytte posisjonen gradvis når linjen glir mellom sensorer.

Formelen er et vektet gjennomsnitt av avstanden fra midten:

```text
feil = sum(vekt * (sensorindeks * 1000 - 2500)) / sum(vekt)
```

Eksempel med verdiene `100,100,900,700,100,100`:

```text
Sensor 2: vekt 900 - 350 = 550, avstand -500.
Sensor 3: vekt 700 - 350 = 350, avstand +500.
De svake naboene har vekt null.

Feil = (550 * -500 + 350 * 500) / 900 ≈ -111.
```

Linjen ligger litt til venstre. `SENSOR_DIRECTION` kan snu fortegnet hvis sensororienteringen krever det.

`weightedError` og multiplikasjonen bruker 32-bit heltall for at produktene skal få plass på AVR. Den endelige heltallsdivisjonen kutter desimaldelen. Med dagens grenser er summen av vektene positiv når denne delen nås; ikke endre tersklene uten å bevare dette.

Til slutt blir `valid = true`. Toppen avgjør om kvaliteten blir `V` (minst 750) eller `L` (700–749). Gruppesjekkene må fortsatt være bestått.

## 7. `LineController`: hva husker alle variablene?

| Variabler | Hva de brukes til |
| --- | --- |
| `left`, `right` | Nåværende beregnede motorpådrag etter ramping. Dette er ikke tilbakemåling fra motorene. |
| `base` | Grunnpådraget under vanlig følging. |
| `previousError`, `derivative`, `derivativeReady` | Forrige feil, filtrert feilendring og om det finnes et grunnlag for å beregne den. |
| `previousTime`, `initialized` | Tidspunkt fra forrige oppdatering og om første oppdatering er gjort. |
| `lastSide`, `sideTime` | Siden linjen sist ble sett på under vanlig følging, og tidspunktet for hintet. |
| `stableTiming`, `stableStart` | Om roboten har en pågående stabil periode, og når den begynte. |
| `gapActive`, `lossStart` | Om det finnes et pågående tap/glipp, og når tapet begynte. |
| `recovering`, `braking`, `searchSide` | Om roboten gjenfinner linjen, om den først bremser, og blind søkeretning. |
| `candidate`, `candidateStart`, `candidateError` | Om en linjekandidat følges under recovery, når perioden begynte og forrige kandidatfeil. |
| `aligning` | Om kandidaten brukes til synlig innretting. |
| `captureSeen`, `captureTime` | Om en linje er sett under denne recoveryen, og tidspunktet for siste treff. |
| `captureRate`, `captureSamples` | Filtrert feilendring under innretting og om nok sammenhengende målinger finnes. Telleren stopper på 2. |
| `captureSide` | Siste bekreftede side for videre blind søking etter et treff. Null betyr at treffet ikke ga en bekreftet side. |
| `directionCandidateSide`, `directionCandidateStart` | En mulig ny søkeretning og når bekreftelsen av den begynte. |
| `haveStrong`, `strongTime`, `strongError` | Om en sterk linje er sett, og siste sterke målings tid og feil. |
| `uncertain`, `uncertainStart` | Om et sammenhengende tvetydig område håndteres, og når det begynte. |
| `uncertainLeft`, `uncertainRight` | Hjulmål som ble lagret ved starten av tvetydigheten. |
| `stopped` | Låser styringen i stopp frem til en ny `LineController` lages. |

`mode()` gir én diagnosebokstav ut fra disse flaggene. Flaggenes kombinasjon er den egentlige tilstanden; koden bruker ikke én samlet tilstandsvariabel. `stop()` setter begge pådrag rett til null og `stopped = true`. `brake(dt)` trapper derimot ned pådragene gradvis.

## 8. `update()`: les den i denne rekkefølgen

Funksjonen er lang fordi den inneholder flere kjøremåter. Den første delen som håndterer situasjonen ferdig, bruker `return` slik at resten ikke overstyrer kommandoene.

### A. Tid og godkjenning av svake målinger

`dt` er sekunder siden forrige oppdatering. Første gang brukes 0,008 sekunder. `rampDt` begrenses til maksimalt 0,020 sekunder. Hvis en oppdatering blir forsinket, unngås dermed ett veldig stort rampesteg. Feilendring og filtrering bruker fortsatt faktisk `dt`.

Uttrykk som `(uint32_t)(now - previousTime)` bruker differansen mellom to tidspunkter. Med usignert 32-bit aritmetikk tåler denne formen også at millisekundtelleren går rundt, for intervallene som brukes her.

En lokal kopi, `line`, lages fra `observation`. En svak måling godtas bare hvis roboten ikke er i recovery, en sterk måling finnes fra de siste 100 ms og feilen er innenfor 350 enheter fra denne. Ellers settes kopiens `valid` til false.

### B. Tvetydig signal får førsteprioritet

Hvis en ugyldig måling har kvalitet F, B eller X, går den hit. Ved starten lagres tid og et mulig hjulmål. Med et ferskt linjehint skaleres de eksisterende hjulpådragene slik at det sterkeste får størrelse 150, mens forhold og fortegn beholdes.

Eksempel: pådrag 200/100 gir lagret mål 150/75. Dette kan også øke et lite eksisterende pådrag; 100/50 gir samme mål 150/75. Rampene bestemmer hvor raskt det nås.

De første 200 ms brukes de lagrede målene. Deretter settes målene til null. Ved 500 ms sammenhengende tvetydighet kalles `stop()`. Uten ferskt hint blir målene null fra starten.

Stabilitetsmåling, D-minne og deler av kandidatminnet nullstilles så gammel informasjon ikke styrer en ny innretting. Denne grenen avsluttes med `return`. Derfor starter ikke vanlig blindt søk samtidig som det tvetydige signalet håndteres.

### C. Bekreft tap og velg søkeretning

Uten recovery og uten gyldig linje startes en glipptimer. Før `LOSS_CONFIRM_MS`, nå 4 ms, går hjulene mot null med `GAP_DECEL`. Ved en kontrollperiode på minst 8 ms kan neste måling allerede være forbi denne fristen. 4 ms betyr altså ikke at sensoren leses på nytt etter 4 ms.

Når tapet bekreftes, blir `searchSide` et ferskt `lastSide` eller standarden +1. Med sidehint begynner rotasjonsstyringen straks; uten hint aktiveres først `braking`.

### D. Recovery uten synlig linje

Søkerotasjonen bruker følgende mål:

```text
venstre = searchSide * søkepådrag
høyre   = -searchSide * søkepådrag
```

Med side +1 går venstre fremover og høyre bakover: roboten forsøker å rotere mot høyre. Pådraget begrenses av `SEARCH_SPEED`, `RECOVERY_SEARCH_LIMIT` og `MOTOR_LIMIT`.

Hvis en linje nylig ble sett og forsvant igjen, brukes `captureSeen` for å unngå et øyeblikkelig hopp tilbake til fullt søk. Ved ukjent bekreftet side blir målet null de første 120 ms. Ved kjent side beholdes et begrenset rotasjonsmål på maksimalt 120. Deretter går den tilbake mot vanlig søkepådrag.

Søket har en total tidsfrist regnet fra første tap. En gyldig linje ved fristen kan gi opptil 150 ms ekstra til innretting. Svake målinger er allerede avvist under recovery.

### E. Recovery med synlig linje: bekreft og mål bevegelse

Det er to ulike spørsmål: «Har linjen vært synlig lenge nok?» og «Har vi nok sammenhengende posisjonsmålinger til å beregne bevegelse?»

`candidateStart` starter en kandidatperiode. Forskjellen fra `candidateError` sammenlignes med `REACQUIRE_MAX_STEP = 600`. Ved for stort hopp nullstilles feilendringsestimatet, og måletelleren blir 1. Ved neste sammenhengende måling blir telleren 2.

**En detalj som er lett å misforstå:** Et stort posisjonshopp nullstiller ikke `candidateStart` så lenge `candidate` fortsatt er true. Dermed bekrefter 40-ms-timeren vedvarende gyldige treff, ikke nødvendigvis 40 ms uten store posisjonshopp. Etter et hopp må det likevel komme nok sammenhengende målinger til at `captureSamples >= 2` igjen. Kommentaren ved konfigurasjonen beskriver ikke hele denne forskjellen.

Synlig innretting reagerer med en gang. For å endre retningen som brukes hvis linjen forsvinner igjen, kreves derimot et vedvarende sidefunn på motsatt side i 32 ms. Det beskytter blind søkeretning mot korte tilfeldige treff.

## 9. Den mest kompliserte delen: beregning av innretting

Denne delen ligger mellom `float distance = magnitude(line.error);` og `bool confirmed ...` i recovery-grenen. Den kombinerer PD-styring med minimumspådrag og myke overganger. Minimumsreglene kan dominere PD-resultatet.

### `farWeight`: hvor mye ekstra kraft skal brukes langt ute?

Først lages en verdi mellom 0 og 1:

```text
t = begrens((avstand - 900) / (1600 - 900), 0, 1)
farWeight = t * t * (3 - 2*t)
```

Med dagens oppsett er vekten 0 ved avstand opptil 900 og 1 fra 1600. Ved 1250 er den 0,5. Uttrykket `t*t*(3-2*t)` kalles smoothstep og gir en overgang som begynner og slutter mykt. Tallene 3 og 2 er del av denne kurven, ikke motorinnstillinger.

`captureLimit` blandes så fra 140 nær midten til 200 langt ute. Formelen `a + vekt * (b - a)` betyr «gå fra a til b etter hvor stor vekten er».

PD-korreksjonen begrenses til dette taket. D-bidraget har også sitt eget tak, nå 30.

### `approaching`: beveger linjen seg allerede mot midten?

`side` er −1 eller +1 fra feilens fortegn. Produktet `-side * captureRate` blir positivt når feilen beveger seg mot null.

Eksempel: feil +1000 og feilendring −2000 enheter/s betyr at linjen nærmer seg midten. Med grensen 2500 blir `approaching = 0,8`. Er feilen på vei bort fra midten, blir verdien begrenset til null.

Denne vekten reduserer minimumspådraget når roboten allerede beveger linjen raskt mot sentrum. Det er en beregning fra sensorfeilen, ikke en måling av robotens vinkelhastighet.

### Tre regler bygger et minimumspådrag

1. `minimumTurn` begynner med `ALIGN_MIN_SPEED`, skalert ned nær sentrum og ved rask tilnærming.
2. `farMinimum` blandes inn langt fra sentrum, slik at større avvik kan få kraftigere rotasjon.
3. `bridgeMinimum` legger et gulv under minimumet, for å unngå svært svakt pådrag før fremdriften kan overta.

Med dagens verdier er `bridgeMinimum` opptil 140 nær ytterkanten av området med `captureLimit = 140`, selv om konstanten heter `RECOVERY_BRIDGE_MIN_TURN = 150`. Taket begrenser den. Konstantene virker altså sammen; én konstant er ikke alltid det faktiske pådraget.

### `lowerBound`: tillat bremsende motstyring nær sentrum

Korreksjonen regnes først om til retningen mot linjen:

```text
directedTurn = side * turn
```

Positiv verdi betyr styring mot linjen uansett hvilken side den ligger på. Langt fra sentrum kreves minst minimumspådraget. Nær sentrum senkes nedre grense slik at D-leddet også kan gi motstyring som demper rotasjonen.

Det er dette `lowerBound = minimumTurn - captureLimit * (1 - weight)` gjør. `weight` går mykt fra 0 til 1 mellom sentrum og `RECOVERY_FORWARD_ERROR = 700`.

Resultatet er ikke bare «P pluss D»: det er P pluss D justert av grenser som skal holde roboten i bevegelse langt ute og tillate demping nær midten. Om dette gir riktig bevegelse avhenger av motorer, friksjon, batteri og sensorplassering.

### Fremdrift og avslutning av recovery

Gjennomsnittet `(left + right) / 2` brukes som et anslag på gjeldende fremdriftskommando. Ved ren motrotasjon er gjennomsnittet null.

Innenfor feil ±700 kan roboten bygge opp et fremdriftsmål på 100, også før full bekreftelse. Når kandidaten er bekreftet, minst to sammenhengende målinger finnes og feilendringen er liten nok, blir målet `CORNER_SPEED`.

Fremdriften begrenses av `MOTOR_LIMIT - abs(turn)`. Dette holder plass til svingbidraget i `drive + turn` og `drive - turn`.

Vanlig følging overtar først når:

- kandidatperioden har vart minst 40 ms;
- minst to sammenhengende målinger finnes;
- absolutt feil er maksimalt 700 og filtrert feilendring maksimalt 2500 enheter/s;
- ingen av de to beregnede hjulpådragene er negative;
- halvparten av hjulforskjellen er maksimalt 50 PWM.

Ved overtakelse nullstilles recovery-flaggene og D-minnet. `base` overtar gjennomsnittet av de nåværende hjulpådragene, begrenset til `CORNER_SPEED`, for å unngå et unødvendig sprang.

## 10. Vanlig følging: PD, fart og motorgrenser

Denne delen er nederst i `update()`. Den nås når en gyldig linje kan følges og recovery ikke håndterer situasjonen.

### D-filteret forklart med tall

Først beregnes feilendringen per sekund:

```text
rate = (ny feil - forrige feil) / dt
alpha = dt / (D_FILTER_SECONDS + dt)
derivative = derivative + alpha * (rate - derivative)
```

Ved feil fra 200 til 240 på 8 ms blir rå endring 5000 enheter/s. Med tidskonstant 0,013 s blir `alpha ≈ 0,381`. Hvis forrige filtrerte verdi var null, blir ny verdi omtrent 1905.

Filteret tar altså bare en del av spranget mot den nye råverdien. Større tidskonstant gir mindre del per oppdatering og dermed tregere filtrering. Ved første måling eller etter nullstilling settes D-verdien til null, fordi gammel og ny feil ikke skal sammenlignes på tvers av et avbrudd.

`derivative` er navnet på den filtrerte feilendringen. Koden har ingen I-del som summerer feil over tid; dette er PD, ikke full PID.

### Sideminne og stabilitet

En sterk måling utenfor ±100 lagrer side og tidspunkt. Sidehintet eldes ut etter 250 ms. Det slettes også etter 150 ms sammenhengende stabil følging, slik at en gammel sving ikke bestemmer neste søk etter et langt rettstrekk.

Stabil betyr sterk linje, absolutt feil under 200 og absolutt filtrert feilendring under 2000. Etter 160 ms sammenhengende stabilitet kan grunnfarten økes utover `CORNER_SPEED`.

### Fartsberegningen

```text
targetBase = begrens(200 - 0,05*abs(feil) - 0,001*abs(feilendring), 180, 200)
```

Farten begrenses altså til intervallet 180–200 etter denne formelen. Hvis roboten ikke er ferdig stabilisert, brukes målet 180 direkte. Ved oppstart kan selve `base` likevel være under 180 mens den rampes opp.

`BASE_DECEL` senker grunnpådraget, `RESTART_ACCEL` løfter det opp mot svingfarten og `BASE_ACCEL` gir roligere økning over svingfarten. Deretter gjelder hjulenes egne ramper i tillegg. Grunnfart og faktisk sendt hjulpådrag er derfor forskjellige størrelser.

### Svingberegningen

P-leddet er `KP * error`. Ved absolutt feil over 1400 legges `EXTRA_TURN_GAIN * overskytende feil` til i retningen mot linjen.

D-leddet er `KD * derivative`, begrenset til ±20. Hele summen begrenses til ±250. Hvis feilen er minst 500 og D-leddet har fått den samlede korreksjonen til å peke bort fra linjen, erstattes korreksjonen med `0,35 * KP * error`. Dette er en beskyttelse ved store avvik.

Tall-eksempel uten ekstra svingbidrag:

```text
base = 180, error = 1000, derivative = 0
turn = 0,005 * 1000 = 5
hjulmål = 185 venstre / 175 høyre
```

Det er hjulmålene før ramping. Motorene mottar vanligvis ikke disse verdiene med en gang.

Negative mål klippes til null under vanlig følging. Hvis høyeste hjulmål overstiger 225, skaleres begge med samme faktor. For eksempel blir mål 300/100 til 225/75. Til slutt sender `rampFollowing()` begge pådragene samme andel mot målene.

## 11. Hva er målt, og hva er bare antatt?

Koden måler refleksjon og tid. Den måler ikke hjulenes turtall, robotens fart, avstand eller vinkel.

Navn med `SPEED` inneholder PWM-verdier. `captureRate` og `derivative` er endring i beregnet linjeposisjon per sekund. `left` og `right` er kommandoer, ikke hva motorene faktisk klarer.

Minstepådrag, ekstrastyring og ramper ser ut til å være laget for å håndtere blant annet motorenes svake pådrag ved lave PWM-verdier og oversving når linjen gjenfinnes. Det er en tolkning av reglene og kommentarene. Uten målinger fra roboten kan vi ikke bekrefte at alle disse tilleggene trengs eller at verdiene er godt tilpasset.

## 12. Detaljer som er verdt å rydde opp i senere

Dette er observasjoner fra gjennomlesingen, ikke endringer som er gjort i programmet:

| Observasjon | Hva den betyr for videre arbeid |
| --- | --- |
| `REACQUIRE_ERROR` brukes ikke. | Å endre den påvirker ikke styringen. Dagens grense for fremdrift er `RECOVERY_FORWARD_ERROR`. |
| Kommentaren nevner kvalitet `W`, men analysen returnerer `?` for for svakt signal. | Bruk faktisk returverdi ved feilsøking. |
| `C` finnes i `mode()`, men `candidate` settes sammen med `aligning = true` i dagens kjørevei. | Diagnosetilstanden ser ut til å være en rest eller reserve. |
| `candidateStart` beholdes ved et stort posisjonshopp. | Bekreftelsestid og kontinuitet er forskjellige tester, som forklart over. |
| `RUNNED` og `CALIBRATED` ser ut som historikk, men brukes som midlertidige flagg. | Bedre navn og enklere flyt kan gjøre oppstartskoden lettere å forstå. |
| Mange boolske flagg beskriver kjøremåten. | Ved større ombygging kan tydeligere tilstander gjøre ulovlige kombinasjoner vanskeligere å lage. |
| Hovedkommentaren sier V9, og andre kommentarer nevner eldre versjoner. | Versjonsnavnene forklarer ikke sikkert dagens oppførsel. Les implementasjonen. |
| `SEARCH_TIMEOUT_MS` er 560000. | Søking kan vare 9 minutter og 20 sekunder. Sjekk at det er tilsiktet. |
| `AIN1` og `LED_BUILTIN` deler D13 på vanlig Uno/Nano. | Kalibreringslampen og høyre motors retningssignal må vurderes sammen med driveren og kablingen. |
| PlatformIO-oppsettet sier Uno; prosjektbeskrivelsen sier Nano. | Kortvariant og bootloader må avklares før korrekt oppsett kan velges. |

## 13. Slik jobber dere videre uten å miste oversikten

Begynn med en konkret observasjon, for eksempel «roboten ser linjen igjen, men fortsetter å rotere». Finn så delen av koden som behandler situasjonen. I dette eksempelet er det recovery med synlig linje, med særlig vekt på `captureRate`, minimumspådrag og kravene for å avslutte recovery.

Noter gjerne dette ved hver endring:

```text
Problem: Hva gjorde roboten, og på hvilket sted på banen?
Mistanke: Hvilken kode eller innstilling kan forklare det?
Endring: Gammel verdi -> ny verdi, eller kort kodebeskrivelse.
Resultat: Hva ble bedre/verre ved samme test?
```

En god kodekommentar forklarer hvorfor en regel finnes. For eksempel: «Nullstill D-minnet etter linjetap fordi målingene før og etter tapet ikke beskriver en sammenhengende bevegelse.» Da vet neste person både hva regelen gjør og hva som må bevares ved en omskriving.
