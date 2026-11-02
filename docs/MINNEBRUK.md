# Minnebruk på ESP32-S

PlatformIO-profilen bruker NodeMCU-32S med 4 MB fysisk flash. Standard app-partisjon er 1310720 byte; prosentene nedenfor gjelder denne partisjonen, ikke hele flashbrikken. RAM-tallene er statiske og bruker PlatformIOs grense 327680 byte.

| Profil | Flash | Statisk RAM |
| --- | ---: | ---: |
| `esp32`, ukonfigurert | 288841 B / 22,04 % | 22344 B / 6,82 % |
| `esp32_fixture`, 12 direkte + motorer | 295745 B / 22,56 % | 22472 B / 6,86 % |
| `esp32_mux_fixture`, 8 direkte + 4 mux + motorer | 295881 B / 22,57 % | 22472 B / 6,86 % |

Rapporten kan gjentas med `python tools/measure_memory.py`. Byggelogger og JSON lagres under `build/`. Ingen reelle konfigurasjonsfiler endres og ingen firmware lastes opp.

ESP32 har en større Arduino-/FreeRTOS-/drivergrunnpakke enn Nano. Det er derfor normalt at antall programbytes er høyere selv om prosentandelen er mye lavere. Et bibliotek som kompileres av rammeverket betyr ikke automatisk at alle dets funksjoner er med i det ferdige programmet.

Heap, stack, SDK-oppgaver og avbrudd bruker også RAM under kjøring. Statisk RAM er ikke et mål på alt tilgjengelig arbeidsminne. Dette må undersøkes på kortet når elektronikken er klar.

Programmet starter ikke WiFi eller Bluetooth. Nano/Uno-kode, slaveprotokoll og QTR-bibliotek er ikke del av dette firmwaremålet. ADC leses direkte, mens PC fortsatt lager JSON/CSV fra en fast rapport. Den tidligere [Nano-optimaliseringen](historikk/MINNEBRUK_NANO.md) er bevart som materiale til rapporten.
