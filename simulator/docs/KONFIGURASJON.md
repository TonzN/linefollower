# Konfigurasjon av emulatoren

Rediger `configs/default.json` eller en kopi av den. Alle seksjoner unntatt `controller` må ha fullstendig struktur. `controller` kan ha et utvalg kjente konstanter; resten hentes fra `firmware_defaults.json`. Ukjente felt, feil typer og flere ugyldige kombinasjoner avvises før kjøring.

Ingen av de fysiske eksempelverdiene er målt på roboten deres. Firmwareverdiene er derimot lest fra kildekoden. Endring av sensorantall eller robotgeometri er et nytt eksperiment, og sikrer ikke at eksisterende firmware-tuning fortsatt passer.

## Robot

| Felt | Betydning |
| --- | --- |
| `body_length_m`, `body_width_m` | Tegnet kroppsrektangel sentrert om hjulakselen. Ingen kollisjonsmodell. |
| `wheelbase_m` | Senteravstand mellom venstre og høyre drivhjul, på tvers av roboten. Større avstand gir lavere rotasjon ved samme hjulforskjell. |
| `wheel_diameter_m` | Gjør RPM om til lineær hjulhastighet. |
| `mass_kg` | Brukes sammen med motormoment og grep for å begrense akselerasjon. |
| `yaw_inertia_kg_m2` | Treghetsmoment ved rotasjon rundt vertikalaksen. Oppgis separat fra kroppsmål. |
| `grip_coefficient` | Friksjonskoeffisient mu. Grenser er omtrent `mu*g` for lineær og lateral akselerasjon. Null gir hjulspinn uten bevegelse fra hvile. |
| `rolling_resistance` | Enkel reduksjon av tilgjengelig akselerasjon ved fartsøkning, i enheter av g. Ingen detaljert rullemotstandsmodell. |

## Sensorer

Indeks 0 er til venstre sett i robotens kjøreretning. Dette gir samme forventede fortegn som firmware: linje til venstre gir negativ feil når `SENSOR_DIRECTION = 1`.

| Felt | Betydning |
| --- | --- |
| `count` | Antall lesere, 2–32. `SensorCount` avledes fra dette. |
| `span_m` | Senteravstand mellom ytterste lesere. Naboavstand er `span_m/(count-1)`. |
| `forward_m` | Sensorrekkens avstand foran drivhjulenes aksel. Kan være negativ for eksperimenter bak akselen. |
| `lateral_m` | Forskyvning fra robotens midtlinje. Positiv er venstre. |
| `mount_angle_deg` | Sensorrekkens rotasjon relativt til vanlig tverrstilt montering. Positiv er mot klokken. |
| `footprint_width_m` | Målebredden til hver leser, på tvers av normal kjøreretning. |
| `footprint_length_m` | Måleflatens lengde langs normal kjøreretning. Måleflatene roteres med monteringen. |
| `samples_per_axis` | Rutenettoppløsning over hver måleflate. 3 betyr 3×3 målepunkter per leser. |
| `edge_softness_m` | Myk overgang ved linjekanten; større verdi gir mer utjevning og svakere topper på smale linjer. |
| `background_value`, `line_value` | Idealverdier for lyst underlag og mørk linje før gain/støy. |
| `gain`, `bias` | Multipliser og legg til for å undersøke endret refleksjon/kalibrering. Verdier klippes til 0–1000. |
| `noise_std` | Standardavvik for normalfordelt støy, i sensorenheter. |
| `dropout_probability` | Sannsynlighet per leser og kontrollmåling for å erstatte verdien med bakgrunn. 0–1. |

Et målepunkt bruker en logistisk overgang fra bakgrunn til linje. Gjennomsnittet over leserens måleflate gir leserens verdi. Det er en etterligning av kalibrerte refleksjonsverdier, ikke en implementasjon av Zumo-bibliotekets rå avlesning.

## Motorer – konfigureres separat for venstre og høyre

| Felt | Betydning |
| --- | --- |
| `min_rpm` | Minste mål-RPM rett over PWM-dødsonen, ved referansespenning og speed_scale 1. Lavere faktisk RPM kan forekomme under akselerasjon og nedbremsing. |
| `max_rpm` | Mål-RPM ved PWM 255 og referansespenning. Skaleres med forsyningsspenning. |
| `deadzone_pwm` | PWM opp til og med denne gir null mål-RPM. |
| `acceleration_rpm_s` | Motorens største opptrapping per sekund ved referansespenning. Dette er fysisk respons, separat fra firmware sin PWM-rampe. |
| `deceleration_rpm_s` | Motorens nedtrapping per sekund ved referansespenning. Revers går via null. |
| `speed_scale` | Justering av turtall for å etterligne ulike motorer. 0,95 er 5 % svakere i denne hastighetsmodellen. |
| `no_load_current_a` | Anslått strøm uten stor forskjell mellom mål og oppnådd RPM. |
| `stall_current_a` | Øvre parameter for den enkle strømtilnærmingen ved stor RPM-forskjell. Ikke beregnet fra viklingsmotstand. |
| `max_torque_nm` | Momentparameter brukt for grensen på chassisets akselerasjon. Ikke en full moment-/turtallskurve. |

Over dødsonen brukes en lineær mapping fra `min_rpm` til `max_rpm`. Begge skaleres med `faktisk spenning / nominal_voltage_v` og `speed_scale`. Grepsmodellen kan gjøre at chassisets bevegelse avviker fra hjulenes omdreininger.

`offsetA` og `offsetB` under `controller` beholder C++-oppførselen: retning velges fra kommandoens fortegn, mens PWM er `abs(int(kommando) + offset)`. A er høyre motor og B venstre. Testbenken avviser PWM over 255 i stedet for å etterligne plattformavhengig registeroverflyt.

## Strømforsyning

| Felt | Betydning |
| --- | --- |
| `voltage_v` | Ubelastet utgangsspenning ved fullt ladenivå. |
| `nominal_voltage_v` | Referansen som motorens RPM-, respons- og momentparametre gjelder ved. |
| `minimum_voltage_v` | Gulv for modellen. Ingen brownout-simulering. |
| `internal_resistance_ohm` | Anslått samlet indre motstand. Spenningsfall er forrige fysikkstegs strøm ganger motstanden. |
| `electronics_current_a` | Fast strømtillegg for elektronikk. |
| `capacity_ah`, `state_of_charge` | Kapasitet og startladenivå 0–1. |
| `battery_discharge_enabled` | Om strømmen skal redusere ladenivået. Ved false brukes en fast forsyning med eventuelt spenningsfall. |

Når utladning er på, brukes en enkel lineær faktor `0,8 + 0,2 * ladenivå` på ubelastet spenning. Den er kun et utgangspunkt. Modellen fortsetter på minimumsverdien og simulerer ikke at et fysisk batteri kobler ut. Riktig kurve og stoppkriterium kommer etter målinger.

## Bane, tid og start

Pillebanen består av to rettstrekk og to halvsirkler. `straight_m` er lengden på hvert rettstrekk, `radius_m` er senterlinjens svingradius og `line_width_m` er den mørke linjens bredde. Bredden påvirker sensordekningen, ikke senterlinjens radius.

`initial_pose.x_m`, `y_m` og `heading_deg` angir start ved hjulakselens midtpunkt. Ved endring av banen må startposisjonen eventuelt endres også; den flyttes ikke automatisk til linjen.

`physics_step_ms` er største fysikksteg. Det deles ved kontrolltidspunkter og ved slutten av en `advance()`-periode. `CONTROL_PERIOD_MS` bestemmer når sensorene leses og styringen oppdateres. `frame_ms` er varighet per bilde i vinduet. Sanntidsfarten i vinduet er omtrentlig fordi tegning og beregning tar tid i tillegg.

`seed` gjør støy og dropout repeterbare etter reset. Det finnes ingen tilfeldig endring av motorer, bane eller strømforsyning mellom kjøringer ennå.

## Firmwarekonstanter

Originalverdiene finnes samlet i `configs/firmware_defaults.json` og i kopiene av `conf.h`, `recovery_conf.h`, `capture_conf.h` og `ambiguous_conf.h`. De samme navnene brukes i Python-porten.

Verdiene under `controller` overstyrer referanseverdiene. Bare `SensorCount` må settes gjennom `sensors.count`. Hvis `CENTER` ikke er eksplisitt overstyrt, avledes det fra antallet.

Pinneverdiene `AIN1`, `AIN2`, `PWMA`, `BIN1`, `BIN2`, `PWMB`, `START`, `CALIBRATE` er kun bevart for sporbarhet. `DEBUG_PERIOD_MS` og `BUTTON_DEBOUNCE_MS` styrer ikke vinduet eller CSV. `REACQUIRE_ERROR` er ubrukt i den opprinnelige kjørekoden. Øvrige regulator- og recoverykonstanter brukes av porten.

For større sensorantall kan større feilgrenser og ny `LINE_MAX_WIDTH` være nødvendig. Ingen automatisk retuning gjøres. Dette gjør det mulig å se hva endret geometri gjør med den eksisterende styringen, før nye verdier velges.
