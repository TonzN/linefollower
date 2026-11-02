# Linjefølger 2

Ny arkitektur med **Nano for sensorer, motorer og regulering**, og **Uno for I²C-styring og USB mot PC**. Dette er et eget prosjekt. `LinjeFølger1`, den eldre mappen `LinjeFølger2` uten mellomrom og Python-simulatoren er ikke endret.

## Start her

1. Åpne denne mappen i CLion: `C:\Users\tonzg\CLionProjects\linjefølger 2`.
2. Les og fyll ut `include/board_config.h` når pinner og komponentmodellene er kjent.
3. Bygg Nano og Uno med PlatformIO. Velg korrekt Nano-bootloader før senere opplasting.
4. Begynn med `info`, deretter sensorer og timing. Motorforsøk kommer etter verifisert oppkobling.

**Standardoppsettet driver ingen motorer.** `MotorsEnabled = false`, ukjente pinner er −1, og sensorbackend er `Unconfigured`. Programmet bygger i denne tilstanden, men avviser forsøk som trenger ukonfigurert maskinvare. Ingen firmware er automatisk lastet opp til kortene.

## Hva som er implementert

- Fast kontrollperiode på Nano, foreløpig 8000 µs.
- Maksimalt kontrollarbeid på 6000 µs og tillatt startforsinkelse på 1000 µs, begge konfigurerbare.
- Måling av sensoravlesningstid, kontrollarbeid, kontrollperiode, startforsinkelse og tidsfristbrudd.
- Stopp ved for lang kontrollrunde, for sen start under kjøring, sensorfeil eller tapt kommunikasjonslease.
- Watchdog på Nano for blokkert programflyt. Den erstatter ikke kontrollrundens kortere tidsbudsjett.
- I²C-pakker på 32 bytes med versjon, sekvensnummer og CRC8.
- Frosset snapshot i flere sider. Uno setter sammen sensorverdier og timing fra samme kontrollrunde.
- Enkel Python-testbenk for sensoropptak, kalibrering, timing, motorpådrag, PWM-sveip og linjefølging.
- Gjenbruk av PD-, recovery- og tvetydighetslogikken fra første prosjekt.

## Hva er likt som før, og hva er nytt?

Regulatoren bygger på den tidligere `fase2`-koden i `LinjeFølger1`. PD-regulering, gjenfinning av mistet linje, håndtering av tvetydige målinger og motorramper er i hovedsak beholdt. Sensorposisjonen er nå normalisert over hele sensorrekken, slik at flere lesere ikke alene øker posisjonsfeilens skala. Terskler og tuning må likevel tilpasses den nye sensoren.

Det gamle `main.cpp` er erstattet av to programmer: `src/nano.cpp` og `src/uno.cpp`. Maskinvaretilgang, kalibreringsflyt, tidsplanlegging, feilstopper og testkommandoer er nye. Tidligere oppstartsknapper og motoroffset er ikke videreført i denne versjonen.

```text
PC / hardware_testbed.py
        | USB / seriell
       Uno  (starter I²C-overføringene)
        | I²C: kommandoer ned, måledata tilbake
       Nano (leser sensorer, regulerer og styrer motorer)
```

Uno sender for eksempel en kommando om å starte en sensortest. Nano tar imot kommandoen, utfører testen og oppdaterer måledata. Når Uno ber om data, fryser Nano ett sammenhengende målebilde som Uno henter i flere pakker. Nano kjører kontrollsløyfen selv; Uno trenger ikke sende en kommando for hver kontrollrunde. Begge kort lastes med hvert sitt program fra samme prosjekt.

`fase1` og den tidligere `fase2` var heller ikke identiske: `fase2` hadde blant annet endringer i gjenfinning, tvetydige linjefunn og regulatorverdier. Den gamle kildekodeforklaringen i `KODE_FORKLART.md` gjelder oppsettet før denne todelingen; bruk denne README-en for den nye programflyten. Git-historikken beholder den tidligere koden.

## Konfigurasjon

`include/board_config.h` inneholder sensorantall, pinner, motorpolariteter, I²C-adresse, tidsgrenser, testgrenser og valgfritt måleutstyr.

Antall lesere står foreløpig til **26** som oppgitt. Det er konfigurerbart. Avlesningsgrensesnittet til den nye sensoren er ikke gjettet ut fra antall pinner:

- **QtrRC / QtrAnalog:** direkte Pololu-avlesning med eksplisitte sensorpinner, via QTRSensors 4.0.0.
- **Custom:** eget grensesnitt i `src/custom_sensor.cpp` hvis sensoroppsettet bruker andre signaler eller en mellomliggende modul.
- **Unconfigured:** avlesning er deaktivert inntil oppsettet er kjent.

Custom-adapteren returnerer foreløpig «ikke konfigurert». Den må implementeres når sensormodell og signalformat er kjent. Å fylle inn motorpinner alene gjør derfor ikke ukjent sensormaskinvare lesbar.

Pinnekonfigurasjonen kontrollerer duplikater, I²C-pinner og gyldige PWM-/enkoderpinner for klassisk Nano. Den kan ikke verifisere ledninger eller en fremtidig Custom-adapters interne bruk av pinner. Eventuell driver-enable-/standby-pin bør kobles og konfigureres der driveren støtter det.

Regulatorverdier står i `include/control/conf.h`, `recovery_conf.h`, `capture_conf.h` og `ambiguous_conf.h`. Gamle motor-/knappepinner er fjernet fra regulatorfilen.

**Posisjonsskala:** sensorrekken normaliseres til `SensorPositionSpan`, som nå er 0–5000 med sentrum 2500. Det beholder omtrent samme feilenhet som den gamle seksleser-varianten. Antall sterke lesere, kontrastgrenser og regulatorverdier må fortsatt tunes til den nye sensoren og geometrien. `LINE_MAX_WIDTH` er fortsatt en grense i antall lesere.

## Bygg i CLion / PlatformIO

CMake-prosjektet gir CLion to kjørbare testmål: `runtime_tests` og `nano_runtime_tests`. Hvis PlatformIO finnes, finnes også mål for `firmware_nano` og `firmware_uno`.

Fra terminalen i prosjektmappen:

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run -e nano -e uno
```

`nano` bruker `nanoatmega328`. Alternativet `nano_new_bootloader` bruker `nanoatmega328new`. Byggearkitektur er den samme; korrekt bootloader/opplastingsvalg må bekreftes for kortet deres.

## Hardware-testbenken

Opprett Python-miljø og installer seriell støtte:

```powershell
python -m venv .venv
.\.venv\Scripts\python.exe -m pip install -r requirements.txt
.\.venv\Scripts\python.exe hardware_testbed.py --list-ports
```

Et miljø med pyserial er allerede opprettet på denne maskinen. USB-porten nedenfor er et eksempel og må byttes til Uno sin port:

```powershell
.\.venv\Scripts\python.exe hardware_testbed.py --port COM5 --test info
.\.venv\Scripts\python.exe hardware_testbed.py --port COM5 --test timing --seconds 5
.\.venv\Scripts\python.exe hardware_testbed.py --port COM5 --test sensors --seconds 5 --label "lyst gulv, motor av"
.\.venv\Scripts\python.exe hardware_testbed.py --port COM5 --test calibrate --seconds 5
```

Under kalibrering føres alle lesere manuelt over både underlag og linje. Motorene står stille. Rå min/maks oppdateres fra én avlesning per kontrollrunde, og kalibrerte verdier beregnes fra samme råprøve. Dette erstatter det gamle bibliotekets blokkerende flermålingskalibrering. Kalibreringen ligger bare i RAM og må gjøres etter Nano-omstart.

Motorforsøk krever konfigurert maskinvare, `MotorsEnabled = true`, og et eksplisitt flagg i PC-programmet:

```powershell
.\.venv\Scripts\python.exe hardware_testbed.py --port COM5 --test motor --left 60 --right 0 --seconds 2 --confirm-motion
.\.venv\Scripts\python.exe hardware_testbed.py --port COM5 --test sweep --side left --start 0 --end 100 --step 5 --dwell-ms 500 --confirm-motion
.\.venv\Scripts\python.exe hardware_testbed.py --port COM5 --test sweep --side left --start 100 --end 0 --step -5 --dwell-ms 500 --confirm-motion
.\.venv\Scripts\python.exe hardware_testbed.py --port COM5 --test follow --seconds 10 --confirm-motion
```

Standardgrensen for måleforsøk er PWM ±120 og maksimalt 30 sekunder. Grensene gjelder testkommandoene. Vanlig linjefølging bruker egne hastighetsgrenser i regulatoren, men har samme tidsgrense, lease og feilovervåking. Direkte motorforsøk bruker pådraget uten regulatorens myke PWM-rampe, slik at en virkelig stegrespons kan måles.

CSV og en JSON-fil med testbetingelser/resultatstatus lagres i `runs/`. `--csv` og `--label` kan brukes til egen fil og beskrivelse. Ctrl+C eller en feil forsøker å sende STOP i `finally`. Ved brutt PC-forbindelse utløper Uno sin hostlease og deretter Nano sin motorlease. Dette er ikke en elektrisk nødstopp.

## Hva kan måles automatisk nå?

| Måling | Krever |
| --- | --- |
| Rå og kalibrerte sensorverdier | Fungerende og konfigurert sensorbackend. |
| Kontroll- og avlesningstider | Nano-firmware. Timing uten konfigurert sensor måler ikke avlesning av den virkelige sensoren. |
| Faktisk pådrag og testtrinn | Motorpinner/driver konfigurert. Pådrag er ikke målt kraft. |
| Spenning og strøm | Egne målekretser på konfigurerte ADC-innganger, med korrekt skala/nullpunkt. |
| Hjul-RPM | Enkoder og pulser per hjulomdreining. Første versjon bruker enkeltkanals pulser og rapporterer bare RPM-magnitude. |
| Kraft, moment, chassisfart og faktisk akselerasjon | Separat måleutstyr/video. Ingen av disse beregnes som om de var målt fra PWM alene. |

Manglende spenning/strøm/enkoder gir `null` i telemetri og tomme felt i CSV. Reelle nullverdier beholdes som null. En PWM-sveip kjører automatisk, men start-/gangterskler må etterpå vurderes mot RPM eller video. Programmet påstår ikke at det har funnet en terskel uten bevegelsesmåling.

PC-opptakene er **nedprøvd telemetri**, ikke hver eneste 8-ms-runde. Hver rad har Nano-tidsstempel og sekvensnummer; hull mellom PC-prøver rapporteres. Min/maks kontrolltid og tidsfristtellere oppdateres på Nano for alle runder. Det finnes ennå ingen fullhastighets rådatabuffer. Høyfrekvent sensorstøy og raske transienter krever en senere opptaksmodus eller eksternt måleutstyr.

`encoder_age_ms` viser alder siden siste RPM-oppdatering. RPM-feltene er tomme frem til første enkodervindu er ferdig. `quality` og `control_mode` viser henholdsvis sensoranalysens kvalitet og regulatorens kjøremåte. Eksisterende CSV-/JSON-opptak overskrives ikke.

## Tidsmodell og kommunikasjon

Nano prioriterer kontrollsløyfen. Neste planlagte tidspunkt følger en fast tidsakse; ved forsinkelse telles hoppede perioder i stedet for at flere gamle runder kjøres rett etter hverandre. Kommando- og snapshotarbeid kjøres i ledig tid. I²C-avbruddene kopierer bare kommandoer/ferdige sider og validerer faste pakker; de kjører ikke sensor eller regulator.

`cycle_us` omfatter sensor, regulering, motorutgang og valgfri ADC-/enkoderoppdatering. Snapshotarbeid utenfor kontrollrunden fanges gjennom startforsinkelse og hoppede perioder. `MaxCycleUs` er en kontrollert grense etter at arbeidet returnerer; den kan ikke avbryte en fastlåst driver midt i et kall. Watchdog er et separat vern mot dette.

`time_us` er kontrollrundens start på Nano, ikke PC-ankomsttid. Telleren går rundt etter ca. 71,6 minutter. Differanser håndteres usignert, og PC-programmet pakker ut tiden over omløpet. Regulatoren bruker `millis()` separat, slik at micros-omløp ikke blir et kunstig stort D-tidssteg.

Uno er I²C-controller, og Nano svarer på adressen `NanoAddress`. Nano bruker de faste I²C-pinnene på klassisk Uno/Nano, A4/A5, med felles GND og korrekt pullup-oppsett. Flytting av disse maskinvarepinnene krever annet grensesnitt/kort, ikke bare et nytt tall i konfigurasjonen. Eksakt øvrig kabling kommer senere.

## Tester

```powershell
.\.venv\Scripts\python.exe tools/run_tests.py
```

Testene dekker CRC/snapshotpakker, tidsplanlegger og uint32-omløp, PWM-sveip, regulator, faktisk Nano-programflyt med falsk maskinvare, feilstopper, kalibrering og Python-logging. Testene trenger ikke Arduino tilkoblet. De bekrefter programlogikk og bygging, ikke virkelig sensorrespons, elektrisk kobling eller garantert kjøretid på roboten.

## Filene

- `src/nano.cpp`: kontrollsløyfe, testmodi, tidsmålinger og Nano sin I²C-side.
- `src/uno.cpp`: PC-kommandoer, heartbeat og sammensetting av I²C-snapshot.
- `src/hardware.cpp`: QTR-avlesning, pinnekontroll, motorutgang og valgfritt måleutstyr.
- `src/custom_sensor.cpp`: avgrenset sted for den nye sensorens grensesnitt.
- `src/control/`: gjenbrukt regulatorlogikk og ramper.
- `include/board_config.h`: maskinvare og tidsgrenser.
- `include/protocol.h`: felles byteformat og CRC.
- `hardware_testbed.py`: PC-programmet for fysiske måleforsøk.
