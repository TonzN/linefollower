# Linjefølger – banesimulator

Denne branchen, `codex/simulering`, inneholder banesimulatoren i [simulator/](simulator/).

- [Kom i gang med simulering](docs/SIMULASJON.md)
- [Simulatorens README og kommandoer](simulator/README.md)
- [Konfigurasjon](simulator/docs/KONFIGURASJON.md)
- [Måleplan og modelluttrykk](simulator/docs/MALEPLAN_OG_MODELL.md)

```powershell
cd simulator
python -m venv .venv
.\.venv\Scripts\python.exe -m testbed
```

Simulatoren bruker en Python-port av Nano-styringen fra 29. oktober 2026. Fysikken er forenklet og parameterne må måles på roboten. Alle 19 programtester og 21 361 kontrollrunder mot C++-referansen passerer ved publisering 16. november 2026.

Robotprogrammet og pinneveiledningen videreføres på [fase2_esp32](https://github.com/TonzN/linefollower/tree/fase2_esp32). De arvede robotfilene i roten til denne branchen kommer fra samme utgangspunkt.
