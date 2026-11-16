# Linjefølger – banesimulator

Denne branchen inneholder Python-simulatoren for en linjefølger på en virtuell pillebane. Den er laget for å forstå og sammenligne styring, sensorplassering og forenklet robotfysikk.

## Start her

- [Kjør simulatoren](simulator/README.md)
- [Kode forklart – moduler, dataflyt og én kontrollrunde](simulator/docs/KODE_FORKLART.md)
- [Teknologiene vi bruker](simulator/docs/TEKNOLOGIER.md)
- [Simuleringsmodell, tester og begrensninger](docs/SIMULASJON.md)
- [Konfigurasjonsfeltene](simulator/docs/KONFIGURASJON.md)
- [Målinger som kan forbedre modellen](simulator/docs/MALEPLAN_OG_MODELL.md)

Fra rotmappen etter kloning av branchen `codex/simulering`:

```powershell
cd simulator
python -m venv .venv
.\.venv\Scripts\python.exe -m testbed
```

Åpne undermappen `simulator/` som prosjekt i VS Code for å bruke de medfølgende kjøreprofilene. Vinduet starter på pause.

## Innhold

| Mappe | Innhold |
| --- | --- |
| `simulator/testbed/` | Kjørbar Python-kode |
| `simulator/configs/` | Robotprofil og referansekonstanter |
| `simulator/docs/` | Kodeveiledning, teknologier, konfigurasjon og måleplan |
| `simulator/tests/` | Automatiske tester |
| `simulator/tools/` | C++-sammenligning og snapshot-verktøy |
| `simulator/firmware/` | Kontrollsummert C++-referanse brukt av sammenligningstesten |
| `simulator/measurements/` | Maler og måleark for modellparametre |

Simulatoren bygger på Nano-styringen fra 29. oktober 2026 og bruker en forenklet fysikkmodell. Parameterne må tilpasses målinger på roboten. C++-referansen i `simulator/firmware/` er nødvendig for å teste Python-porten.

Robotprogram og oppkobling er dokumentert på [fase2_esp32](https://github.com/TonzN/linefollower/tree/fase2_esp32). [fase2](https://github.com/TonzN/linefollower/tree/fase2) inneholder den tidligere to-Nano-løsningen.
