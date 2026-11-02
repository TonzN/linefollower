# Kodestil for gruppen

C++ har flere etablerte stiler, og ingen enkelt stil alle prosjekter må følge slik mange Python-prosjekter følger PEP 8. Her velger vi en enkel stil som gjør det lett å lese og endre robotkoden. Reglene gjelder også når kode blir generert med en assistent.

## Våre regler

- Fire mellomrom per innrykk. Ingen tabulatorer.
- Klammer på egen linje. Bruk klammer også rundt korte `if`, `else` og løkker.
- Én handling og én variabeldeklarasjon per linje.
- Blanklinje mellom logiske steg. Ikke press sammen funksjoner for å få færre linjer.
- Normalt høyst 100 tegn per linje. Del lange betingelser og kall.
- Unngå nestede `?:`-uttrykk. En enkel verdi som `valid ? value : 0` er grei når betydningen er tydelig.
- Variabler og funksjoner bruker beskrivende `camelCase`. Typer bruker `PascalCase`.
- Tids- og måleverdier får enhet i navnet når det er nyttig: `ageMs`, `sensorUs`, `voltageMv`.
- Behold eksisterende konfigurasjonsnavn som `KP` og `LINE_MAX_WIDTH`, slik at gammel tuning fortsatt er gjenkjennelig.
- Del lange funksjoner etter oppgave. Ikke lag nye klasser eller lag bare for å pakke inn én enkel operasjon.
- Kommentarer skrives på norsk og forklarer hensikt, forutsetninger og konsekvenser. Standard API-navn kan være engelske.

## Eksempel

```cpp
// Uten konfigurert sensor finnes det ingen måling vi kan styre etter.
if (!hw::sensorReady())
{
    return;
}

uint16_t rawValues[cfg::SensorCount] = {};
hw::readSensors(rawValues);
```

Kommentaren forklarer hvorfor kontrollen finnes. En kommentar som bare sier «returnerer hvis sensoren ikke er klar» ville ikke gitt ekstra hjelp.

## Hva bør kommenteres?

- Hva en funksjon gjør og hvilke enheter argumentene bruker.
- Om verdiene er rå, kalibrerte, normaliserte eller fysisk målte.
- Hvorfor vi bruker atomisk kopiering, CRC, tidsgrenser eller spesielle startfunksjoner.
- Hvilke arbeid som må skje utenfor avbrudd og kontrollrunde.
- At noe er en foreløpig antakelse eller ennå ikke støttet.

Kommentarer erstatter ikke gode navn. Unngå kommentarer på åpenbare operasjoner som `index++` med mindre det er en særskilt grunn til operasjonen.

## Automatisk formatering

`.clang-format` styrer C++-formatering. `.editorconfig` styrer blant annet innrykk og linjeskift. Python formateres med Black.

```powershell
.\.venv\Scripts\python.exe -m pip install -r requirements-dev.txt
.\.venv\Scripts\python.exe tools/format_code.py
.\.venv\Scripts\python.exe tools/format_code.py --check
```

CLion kan bruke prosjektets ClangFormat-oppsett under innstillinger for editor/kodestil. Kommandoen over fungerer uavhengig av editorvalg. Formattereren kan ordne luft og innrykk, men kan ikke finne gode variabelnavn eller forklare algoritmen for oss.

Bakgrunn: [ClangFormat](https://clang.llvm.org/docs/ClangFormatStyleOptions.html) og [C++ Core Guidelines om navn og layout](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#nl-naming-and-layout-rules). Dette er prosjektets valg, ikke en påstand om at vår klammerstil er en universell C++-standard.
