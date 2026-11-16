# Fysiske målinger

Her samles egne målinger fra roboten. `session_template.json` er en tom registreringsmal, ikke et datasett. `null` betyr ukjent eller ikke målt; det betyr aldri null volt, null kraft eller null støy.

Lag en undermappe per økt, for eksempel `2026-11-01_sensor_baseline`, med en kopi av malen, råopptak, eventuell video og notater. Behold rådata uendret og legg analyser ved siden av. Ikke bruk eksempelverdiene i simulatoren som måleresultater.

Les [måleplan og modell](../docs/MALEPLAN_OG_MODELL.md) først. Alle numeriske felt har enhet i navnet. Behold måletidspunkter og oppgi om RPM gjelder motorakselen eller girutgangen/hjulet.

Objektene i `record_templates` viser feltene for én prøve. Kopier relevante objekter til egne opptaksfiler. De er ikke konfigurering av simulatoren. Start med sensorbakgrunn og én motors start-/gangterskel; resten kan fylles inn senere.
