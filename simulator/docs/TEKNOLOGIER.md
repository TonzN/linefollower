# Teknologiene i simulatoren

Simulatoren kjører lokalt på PC og bruker Python-standardbiblioteket. Den grafiske visningen og kjøringen uten vindu deler samme simuleringskode.

| Teknologi | Bruk i prosjektet | Hvor |
| --- | --- | --- |
| Python 3.10 eller nyere | Styringsport, bane, sensorer, fysikk, logging og tester | `testbed/`, `tests/`, `tools/` |
| Tkinter, ttk og Canvas | Vindu, knapper, tegning og hendelsesløkke | `testbed/viewer.py` |
| JSON | Robotprofiler, referansekonstanter, målemal og testrapport | `configs/`, `measurements/`, `build/` |
| CSV | Tidsserier fra kjøring uten vindu | `testbed/__main__.py` |
| C++17 og g++ | Kompilert referanse for å kontrollere Python-porten | `firmware/`, `tools/verify_firmware.py` |
| unittest | Automatiske programtester | `tests/test_testbed.py` |
| Git og GitHub | Versjonering og deling på `codex/simulering` | Hele prosjektet |
| Markdown og Mermaid | Veiledninger, tabeller og flytdiagrammer | README og `docs/` |
| VS Code | Valgfrie kjøre- og debugprofiler | `.vscode/` |
| venv | Eget Python-miljø for prosjektet | Lokal `.venv/` |

## Python og standardbiblioteket

`pyproject.toml` oppgir Python >= 3.10 og en tom liste over eksterne avhengigheter. Python-moduler importeres fra pakken `testbed`. Startkommandoen `python -m testbed` kjører pakkens `__main__.py`.

De viktigste bibliotekene er:

- `argparse` leser kommandolinjevalg.
- `json` leser profiler og skriver resultat-/referanserapporter.
- `csv` skriver måleserier.
- `math` gir trigonometri, avstander og modellberegninger.
- `random.Random` lager repeterbar sensorstøy og utfall fra en seed.
- `dataclasses` samler sensor-, motor- og styringstilstand.
- `pathlib` håndterer filer og prosjektstier.
- `subprocess` kjører kompilatoren og C++-referansen.
- `hashlib` verifiserer firmware-snapshotens SHA-256-kontrollsummer.
- `unittest` og `tempfile` brukes til automatiske tester og midlertidige profiler.

## Grafisk visning med Tkinter

Tkinter følger mange Python-installasjoner og må være tilgjengelig for vinduet. Visningen bruker en hendelsesløkke med `root.mainloop()`, og `root.after()` planlegger oppdateringer.

Canvas tegner bane, robot, sensorer og kjørespor. `ttk` brukes til knapper og tekstfelt. Hele den vanlige simuleringen kjører i samme tråd. Vinduets oppdateringstid er omtrentlig veggklokketid, mens modellen regner med eksplisitt simulert tid.

Kjøring med `--headless` importerer ikke visningen og kan brukes til repeterbare forsøk og CSV-opptak.

## JSON som profilformat

En profil lagrer geometri, motorer, sensorinnstillinger, strømforsyning, bane og styringskonstanter. Gruppen kan endre modellen uten å redigere Python-koden. `config.py` validerer struktur, typer og intervaller før simuleringen starter.

`configs/default.json` er standardprofilen. `testbed/config_template.json` definerer forventet struktur. `configs/firmware_defaults.json` inneholder konstanter fra C++-snapshoten.

## C++ som testreferanse

Python-porten kjører styringen under simulering. C++-snapshoten kjøres separat av `verify_firmware.py` for å sammenligne styringsresultatene.

Verktøyet trenger `g++` på PATH og bygger med C++17. Det lager en liten lokal Arduino-stubb og en testdriver under `build/`. Dette gjør referansen kjørbar på PC. De originale snapshotfilene kontrolleres mot manifestet før sammenligningen.

Windows-kommandoene i referanseverktøyet lager `oracle.exe`. Denne publiserte versjonen er kontrollert på Windows. Vanlig Python-kjøring uten vindu trenger ingen C++-kompilator.

## Versjonering og arbeidsmiljø

GitHub-branchen `codex/simulering` samler simulatorens kode og veiledninger. Virtuelle miljøer, Python-cache, byggeprodukter og kjørelogger er ignorert i Git.

`.vscode/` har profiler for vindu, kjøring uten vindu, programtester og C++-sammenligning. Åpne undermappen `simulator/` som arbeidsmappe for at de relative stiene skal treffe.

`firmware/manifest.json` knytter referansekopien til kildeprosjektet og datoen 29. oktober 2026. Versjonskontroll gjør det mulig å sammenligne kode- og profilendringer mellom forsøk.

## Avgrensning

Fysikken er implementert direkte i Python. Prosjektet har foreløpig ingen ekstern fysikkmotor eller RL-agent. Simulerte sensorer produserer kalibrerte refleksjonsverdier. ESP32-oppkobling, RC-pulsmåling og motorens faktiske elektriske signaler må undersøkes på roboten.

Regulatoren og fysikkmodellen kan derfor brukes til å undersøke oppførsel og modellantakelser. Prediksjon av virkelig ytelse krever identifiserte parametre og validering mot målinger.

Se [kodeveiledningen](KODE_FORKLART.md) for funksjonene som kobler teknologiene sammen.
