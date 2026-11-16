# Arbeidsregler for simulatoren

- Prioriter kode og dokumentasjon gruppen kan lese og videreutvikle.
- Simulatorprosjektets rot er `simulator/`. Kjør Python-kommandoer derfra.
- Hold styring, sensor-/banemodell, fysikk og visning adskilt.
- Kommenter hensikt, enheter, tidsmodell og modellantakelser.
- Oppdater kodeveiledning og modellbeskrivelse ved endringer i programflyten.
- Behold `simulator/firmware/` som kontrollsummert referanse. Endring av snapshot er et eget, eksplisitt arbeid.
- Kjør `python -m unittest discover -s tests -v` etter relevante kodeendringer.
- Ved endringer i styringsporten skal også `python tools/verify_firmware.py` kjøres.
- Programtester og C++-sammenligning bekrefter modellens og styringsportens oppførsel. Fysisk realisme må valideres mot robotmålinger.
- Loggfiler, build og virtuelle miljøer skal holdes utenfor Git.
