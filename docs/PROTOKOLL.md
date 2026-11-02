# ESP32 og PC – S3

ESP32 har ingen I2C-forbindelse i denne branchen. Kommandoer og rapporter går over USB-UART. Kommandoene avsluttes med linjeskift:

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

Varighet er millisekunder. MOTOR tar venstre PWM, høyre PWM og varighet. SWEEP tar sidebitmaske (1 venstre, 2 høyre, 3 begge), start, slutt, steg og millisekunder per trinn.

HELLO rapporterer protokoll 3. Korte bekreftelser/feil er JSON. `sent` betyr lagt i kø; `ack_sequence` og `ack_result` i et målebilde viser om kontrollprogrammet godtok kommandoen. STOP overstyrer en vanlig kommando som venter.

## S3-målelinjen

`S3:` etterfølges av heksadesimale bytepar og linjeskift. Første byte er antall valgte lesere. Deretter følger:

1. En 64-byte header.
2. Råverdier: uint16 per valgt leser.
3. Kalibrerte verdier: uint16 per valgt leser.
4. Fysiske sensorindekser: uint8 per valgt leser.
5. CRC8 med polynom 0x07 over alt før CRC.

Totalt blir det `66 + 5 * sensor_count` databytes inkludert CRC. Tall er little endian. Python sjekker lengde, CRC, antall og stigende indekser før feltene tolkes. Feil eller trunkering avvises.

## Header

| Offset | Type | Innhold |
| --- | --- | --- |
| 0, 4 | uint32 | Kontrollrunde, starttid i micros |
| 8, 10, 12, 14 | uint16 | Sensor-, arbeids-, periode- og forsinkelsestid i us |
| 16, 20 | uint32 | Tidsfristbrudd, hoppede perioder |
| 24, 26 | uint16 | Min/maks arbeidstid i us |
| 28 | int16 | Normalisert posisjonsfeil |
| 30, 32 | int16 | Anvendt venstre/høyre PWM |
| 34, 36 | uint16, int16 | Spenning i mV, strøm i mA |
| 38, 40 | int16 | Venstre/høyre RPM ganger ti |
| 42 | uint16 | Gyldighetsflagg |
| 44–47 | uint8 | Testmodus, feil, antall, kvalitetskode |
| 48, 50, 51 | uint16, uint8, uint8 | ACK-sekvens, resultat, regulatorens modus |
| 52, 54, 56 | uint16 | Min/maks periode i us, PC-leasealder i ms |
| 58, 60, 62 | uint16 | Maks råverdi, testtrinn, encoderalder i ms |

Feil 1–4 er arbeidsgrense, sen start, utløpt lease og sensorfeil. Feil 5 er ugyldig maskinvareoppsett; feil 6 er watchdog-oppstart. Raw maximum er normalt 4095. Kalibrert skala er fortsatt 0–1000.

Manglende målinger blir null i JSON og tomme felt i CSV, mens virkelig null beholdes. `sensor_indices` beskriver utvalget. Det finnes ingen `side_count`, sidemålealder eller I2C-sider i S3. PC-verktøyet støtter fortsatt historiske S2-/JSON-rapporter; [det gamle formatet](historikk/PROTOKOLL_NANO.md) er arkivert.

Rapporten sendes i små deler når UART har plass. Snapshotet fryses ved POLL og endres ikke mens rapporten sendes. PC-logging er nedprøvd og representerer ikke hver eneste kontrollrunde.
