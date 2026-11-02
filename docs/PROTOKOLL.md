# Kommunikasjonen mellom PC og kortene

Det finnes tre formater. De har ulike versjonsnumre fordi de løser ulike oppgaver.

| Forbindelse | Format | Implementasjon |
| --- | --- | --- |
| PC til hoved-Nano | ASCII-kommandoer, korte JSON-svar, S2-målerapporter | `src/master.cpp`, `telemetry.py` |
| Hoved-Nano til ytter-Nano | Frosne målesett i 32-byte I2C-pakker | `include/side_protocol.h`, `src/side_link.cpp` |
| Eldre Uno-bridge til Nano | Kommando-/snapshotpakker med versjon 1 | `include/protocol.h`, `src/uno.cpp` |

## PC-kommandoer

Hver kommando avsluttes med linjeskift. Eksempler:

```text
HELLO
POLL
STOP
CLEAR
ARM
SENSORS 5000
CALIBRATE 5000
TIMING 5000
MOTOR 40 40 1000
SWEEP 3 0 100 5 500
FOLLOW 10000
```

Varigheter er millisekunder. `MOTOR` tar venstre PWM, høyre PWM og varighet. `SWEEP` tar sidebitmaske (1 venstre, 2 høyre, 3 begge), start, slutt, steg og millisekunder per trinn. Egen PWM-/varighetsgrense kan avvise kommandoen.

`sent` betyr at kommandoen er lagt i kø. Se `ack_sequence` og `ack_result` i et senere målebilde for å vite om kontrollprogrammet godtok den. `STOP` bruker en prioritert kanal og kan overstyre en vanlig kommando som venter.

`HELLO` gir PC-protokoll 2 for hoved-Nanoen. Eldre Uno svarer protokoll 1. Python-testbenken støtter begge.

## S2-målerapporten

Rapporten er én linje: `S2:` etterfulgt av heksadesimale bytepar og linjeskift. Det er ikke kryptering; hex gjør at byteverdier ikke kan forveksles med linjeskift. Siste byte er CRC8 med polynom 0x07 over alle foregående databytes.

Rekkefølgen før CRC:

1. Antall lokale lesere og antall ytterlesere, én byte hver.
2. Lokalt snapshot: 64-byte header, lokale råverdier og kalibrerte verdier.
3. Yttersekvens, målealder og skannetid som uint16; deretter statusbyte.
4. Råverdier for ytterlesere som uint16.
5. Fysiske ytterindekser og midtindekser som uint8.

Statusbit 0 betyr fersk; bit 1 betyr gyldig målesett. Tall med flere bytes er little endian. Ukjent alder bruker 65535 som sentinel. Python sjekker lengde, CRC og sensorantall før den lager et vanlig sample-objekt. Manglende målinger blir `None`/JSON `null`/tom CSV-celle.

Nano trenger dermed ikke lagre alle JSON-feltnavn eller bruke `printf` på hver måling. Lesbar JSON og CSV lages på PC. Kommando- og feilsvar er fortsatt korte JSON-linjer. UART-sending skjer i små deler uten å vente på ledig buffer.

## Den lokale 64-byte headeren

Offset er byteposisjon fra starten av det lokale snapshotet, etter de to antallsbytene i S2.

| Offset | Type | Innhold |
| --- | --- | --- |
| 0, 4 | uint32 | Kontrollrundenummer, starttid i micros |
| 8, 10, 12, 14 | uint16 | Sensor-, arbeids-, periode- og startforsinkelsestid i us |
| 16, 20 | uint32 | Tidsfristbrudd, hoppede perioder |
| 24, 26 | uint16 | Min/maks arbeidstid i us |
| 28 | int16 | Posisjonsfeil |
| 30, 32 | int16 | Anvendt venstre/høyre PWM |
| 34 | uint16 | Spenning i mV, bare gyldig med flagg |
| 36, 38, 40 | int16 | Strøm i mA, venstre/høyre RPM ganger ti |
| 42 | uint16 | Gyldighets-/statusflagg fra `proto::Flags` |
| 44, 45, 46, 47 | uint8 | Testmodus, feil, lokalt sensorantall, kvalitetskode |
| 48, 50, 51 | uint16, uint8, uint8 | Kommandosekvens, resultat, regulatorens kjøremodus |
| 52, 54, 56 | uint16 | Min/maks periode i us, PC-leasealder i ms |
| 58, 60, 62 | uint16 | Maks råverdi, PWM-testtrinn, enkodermålingens alder i ms |

Rå- og kalibrerte arrays følger fra offset 64, uint16 per kanal. Det er ikke sendt en rå C++-struct; eksplisitte byteposisjoner unngår problemer med padding og ulike heltallsstørrelser.

## I2C til ytterkortet

Hovedkortet starter med `Latch` og ønsket sekvensnummer. Ytterkortet fryser forrige komplette skann. Deretter velger hovedkortet side og henter 32 bytes. Hvert servicekall gjør én bussoperasjon, slik at kontrollsløyfen kan prioriteres mellom kallene.

En pakke har magic 0xB6, versjon 1, sidenummer, sekvensnummer, sensorantall, gyldighetsstatus, skannetid og målealder. Fra offset 12 ligger opptil ni uint16-verdier. Siste byte er CRC8. Opptil tre sider dekker 25 kanaler.

Alle sidene må ha samme sekvens og metadata. Først når hele settet er validert, erstattes forrige komplette sett. Feil pakke gjør ikke et gammelt sett ferskt. Målealderen økes videre på hovedkortet, og foreldede verdier rapporteres som manglende.

Sensor- og regulatorarbeid utføres aldri fra I2C-avbruddene. Avbruddene håndterer korte kommandoer og kopierer ferdige svar.
