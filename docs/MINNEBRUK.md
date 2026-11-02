# Minnebruk og hva som er ryddet bort

## Sammenlignbare bygg

Begge versjoner ble bygget med samme aktive prøveoppsett: seks analoge midtlesere, motorutganger, I2C og 19 multipleksede ytterlesere. Det er viktigere enn å sammenligne standardbygg der ukonfigurert maskinvare kan få kompilatoren til å fjerne mye kode.

Før-versjonen er commit `1c56e8f`. Tallene er hentet fra AVR-ELF med `avr-size`. Flash inkluderer `.text` og `.data`; statisk RAM inkluderer `.data` og `.bss`.

| Kort og ressurs | Før | Etter | Endring |
| --- | ---: | ---: | ---: |
| Hoved-Nano, flash | 29292 B (95,4 %) | 25364 B (82,6 %) | −3928 B |
| Hoved-Nano, statisk RAM | 1450 B (70,8 %) | 1342 B (65,5 %) | −108 B |
| Ytter-Nano, flash | 3924 B | 3924 B | 0 B |
| Ytter-Nano, statisk RAM | 495 B | 495 B | 0 B |

Grensene som brukes er 30720 B programflash og 2048 B SRAM. Hovedkortet har dermed 5356 B programplass igjen i denne profilen. 706 B SRAM er ikke reservert statisk, men dette er **ikke** det samme som 706 B garantert ledig under kjøring: stack, kall og avbrudd trenger også plass.

## Funn og endringer

Luft, innrykk, gode navn og kommentarer gjør kildekoden lengre å se på, men er ikke i seg selv ekstra kjørekode i flash. Derfor kan vi gjøre koden romslig uten å komprimere den for å spare programminne.

**JSON-formatering på hovedkortet var unødvendig tungt.** Den krevde mange feltnavn, en stor formatteringsrutine og generell `printf`-støtte. Nå sendes et fast måleformat, S2, i heksadesimale bytepar. Python tolker bytes, gir feltene navn og lager lesbar JSON/CSV. Brukeren får fortsatt de samme målefeltene i testbenken.

**QTR-objektet ble opprettet selv om analogvarianten ikke brukte det.** Analogavlesningen gjøres direkte med ADC. Objektet opprettes nå bare i RC-grenen. I den aktive analogprofilen blir QTR-objektet og dets avhengigheter fjernet av kompilatoren.

**Generell formatering er fjernet fra korte master-svar også.** Bekreftelser bygges direkte i en liten svarbuffer. Vi bruker ikke `String` eller dynamisk oppbygging av målerapporter.

Symbolkontrollen i minneskriptet viser at `vfprintf` og `malloc` var lenket inn før, men ikke er med etterpå i den aktive analogprofilen. Dette gjelder ikke nødvendigvis en senere RC-/Custom-konfigurasjon som bruker andre bibliotekkall.

Det samlede resultatet er målt. Besparelsen er ikke fordelt i oppdiktede nøyaktige prosentandeler per funksjon, siden linkerens LTO blant annet flytter og integrerer funksjoner i hverandre.

## Hva er ikke bloat?

Følgende er beholdt: regulator, recovery, kalibrering, motorramper, tidsgrenser, watchdog, prioritert stopp, PC-lease, CRC, alderskontroll og eksisterende testmodi. Vi har ikke oppnådd lavere tall ved å fjerne feilstopper eller gjøre standardpinnene til ferdig kabling.

Python, tester, dokumentasjon, CLion-filer og 3D-modeller lastes ikke opp til kortet. Den gamle Uno-bridgen kompileres bare i sitt eget PlatformIO-miljø. At filene finnes i prosjektmappen betyr derfor ikke at de bruker hovedkortets flash.

## Hva kan senere flyttes til ytterkortet?

| Funksjon | Vurdering |
| --- | --- |
| Ytteravlesning og mux | Ligger allerede på ytterkortet |
| Kalibrering og filtrering av ytterkanaler | Naturlig å gjøre lokalt på ytterkortet når det implementeres |
| Eventuell strøm-/spenningsmåling | Mulig hvis riktig målekrets og ledige ADC-innganger finnes |
| CSV, statistikk og rapportformatering | Hører på PC og gjøres allerede der |
| Lokal PD, motorutgang og tidskritisk stopp | Beholdes på hovedkortet for å unngå I2C i den direkte styringsveien |

Å flytte hele PC-bridgen til ytterkortet er mulig, men krever ny kommando-/køflyt mellom kortene. Det er ikke nødvendig for besparelsen over og er ikke gjort i denne oppryddingen. Seks direkte ADC-kanaler pluss I2C bruker alle analoge innganger på en klassisk Nano; en mux kan endre dette pinnebudsjettet.

## Gjenta kontrollen

```powershell
.\.venv\Scripts\python.exe tools/measure_memory.py --baseline 1c56e8f
```

Skriptet bygger midlertidige prosjektkopier og lar de virkelige konfigurasjonsfilene være urørt. Prøvepinnene er kun byggefixture. Rapport og byggelogger lagres under `build/`. Skriptet er foreløpig tilpasset Windows-installasjonen med PlatformIO og AVR-verktøy under brukerens `.platformio`-mappe.

Gjenta med den faktiske konfigurasjonen når maskinvaren er bestemt. Mål også stackmargin, kontrolltid, I2C-varighet og sensorrespons på roboten. Vellykket kompilering og gode flash-tall bekrefter ikke fysisk timing.
