# Måleplan og modell for den virkelige roboten

Dette er neste arbeidstrinn etter emulatoren: måle sensorer og drivverk, og bruke resultatene til å tilpasse modellen. Ingen av måleverdiene er fylt inn på forhånd. Motor-, driver-, sensor- og batterimodell legges til når gruppen har dem tilgjengelig.

Start med én test av gangen. Behold dagens simulator som sammenligningsgrunnlag. Nye modellparametre bør komme fra målinger, med usikkerhet, før de brukes som robotens standardprofil.

## 1. Skill mellom det vi skal finne

| Størrelse | Hva den svarer på |
| --- | --- |
| Sensorens bakgrunn og kontrast | Hvilke tall får hver leser på ulike underlag? |
| Posisjonsfeil | Hvor langt fra faktisk linjeposisjon ligger beregnet posisjon? |
| Støy og interferens | Hvor mye varierer signalet, og hvilke forhold flytter det? |
| Måletid og forsinkelse | Hvor gammel er informasjonen når motorpådraget oppdateres? |
| Startterskel | Hvilken PWM/spenning og hvilket moment får motor/hjul/robot til å begynne å bevege seg? |
| Gangterskel | Hvor lavt pådrag kan opprettholde bevegelse etter start? |
| RPM under last | Hvilket turtall oppnås ved gitt spenning, pådrag og belastning? |
| Moment, kraft og akselerasjon | Hvor mye belastning kan drivverket overvinne, og hvor raskt øker farten? |
| Grep | Når glir hjulene mot underlaget, og hvor mye av vekten ligger på drivhjulene? |

Start er først og fremst et spørsmål om **moment**, ikke et minimum antall RPM: før akselen begynner å snurre er RPM null. Etter start kan minste stabile turtall være et nyttig måleresultat. De to tersklene for start og videre gange kan være forskjellige.

## 2. Første økt: mål opp roboten

Noter total masse med batteri, hjulradius under belastning, senteravstand mellom drivhjul, sensorhøyde, sensorrekkens bredde og avstand foran hjulakselen. Ta med linjens materiale/bredde og gulvets materiale. Bruk samme batteri og kabling som ved kjøring.

Mål normalbelastningen på drivhjulene, for eksempel med vekter under hjulene med lik støttehøyde. Ikke anta at hele robotens vekt ligger på de to drivhjulene hvis en kule eller støtte også bærer vekt. Noter tyngdepunktets omtrentlige plassering. Belastning kan flyttes under akselerasjon.

Mål spenning ved driverens forsyningsinngang både i hvile og under kjøring. Spenningsnivået skrevet på batteriet er ikke nødvendigvis spenningen motoren får under belastning.

## 3. Sensorforsøk

### 3.1 Logg råverdier, kalibrering og tid

Dagens firmware-logg viser kalibrerte verdier og styringstilstand. Den er nyttig ved kjøring, men utilstrekkelig til å skille råsensorens egenskaper fra kalibreringen.

Et eget måleprogram bør senere logge:

- sekvensnummer, start- og sluttid for målingen med `micros()`;
- råverdien for hver leser og sensorens avlesningsmodus;
- kalibreringens minimum/maksimum per leser og hvilken kalibreringsøkt som brukes;
- PWM, retning, målt forsyningsspenning og testbetingelser;
- kjent linjeforskyvning, sensorhøyde og vinkel når disse endres.

Kalibrerte verdier bør beregnes fra samme råavlesning og lagrede kalibrering hvis de skal sammenlignes parvis. Et nytt kall til `readCalibrated()` kan gjøre en ny fysisk måling; ikke merk to separate avlesninger som samtidige. Kontroller implementasjonen i den installerte bibliotekversjonen.

For biblioteket som er i prosjektet, er rå RC-avlesning tidsbasert. Refleksjon og sensorhøyde påvirker måletiden; rådata skal derfor ikke merkes som en vanlig ADC-verdi. Pololus applikasjonsnotat beskriver også påvirkning fra omgivelseslys og motorstøy. [Pololu: sensorenes avlesning og signaler](https://www.pololu.com/docs/0j13/all).

Ikke skriv lange tekstlinjer synkront for hver måling når dere måler kjøretid. Buffre korte opptak eller reduser loggraten, og noter tapte prøver. Mål også avlesning og kontrollsløyfe uten logging for å finne loggens påvirkning. Bruk differanser av usignerte tidsstempler for å håndtere telleroverløp.

### 3.2 Bakgrunn, svart og alt mellom

Begynn med sensoren festet i faktisk høyde. Ta som et første opplegg 5 separate opptak på omtrent 5 sekunder per betingelse. Dette er et startopplegg, ikke en garanti for statistisk uavhengige prøver. Gjenta etter omstart/ny kalibrering for å måle variasjonen mellom økter.

| Forsøk | Hva som endres | Hva vi finner |
| --- | --- | --- |
| Vanlig bakgrunn, uten svart | Alle lesere over samme gulv, motor av | Bakgrunnsnivå og forskjell mellom lesere. |
| Svart flate | Alle lesere over linjens materiale | Svartnivå, metning og kontrast. |
| Andre ikke-svarte flater | Andre gulvfarger, grått, skjøter, blankt og matt materiale | Fare for å forveksle bakgrunn og linje. |
| Linjekant og smal linje | Flytt linjen gjennom hele rekken i kjente trinn, eksempelvis 1 mm | Responskurve, overlapning mellom lesere og posisjonsfeil. |
| Høyde og helning | Faktisk høyde og små avvik rundt denne | Følsomhet for vibrasjon og kroppsbevegelse. |
| Lys | Skygge, normal belysning og relevante lysforhold på banen | Endring i bakgrunn, kontrast og støy. |
| Motorpåvirkning | Samme optiske oppstilling med motor av/på, ulik PWM og retning | Elektrisk påvirkning og eventuell vibrasjon. |
| Spenningsvariasjon | Relevante spenninger innenfor maskinvarens grenser | Om lysstyrke, signal eller måletid endres. |

Ved motorforsøket skal linjen og sensoren holdes i samme relative posisjon. Hvis oppstillingen samtidig vibrerer, er det en samlet effekt av elektrisk og mekanisk påvirkning. Skill dem ved et separat forsøk med mekanisk isolert motor dersom det er nødvendig.

Mål emitter av/på dersom sensor og kabling støtter det. Biblioteket har ulike avlesningsmoduser, men det må bekreftes at emitterstyringen virker på deres maskinvare. [Pololu: kalibrering og avlesningsmoduser](https://www.pololu.com/docs/0j19/all).

**Interferens må ikke bare modelleres som seks uavhengige tilfeldige tall.** Se om flere lesere endrer seg samtidig. Lys og forsyningsstøy kan gi felles variasjon. Naboleseres samtidige respons er ikke i seg selv bevis på optisk krysstale: de kan bare se samme linjekant. Individuell emitterstyring brukes bare dersom maskinvaren faktisk støtter det.

### 3.3 Hva vi beregner fra opptakene

For hver leser og hver testbetingelse beregnes middelverdi, standardavvik, median, 1-/99-persentil, observerte ekstremverdier og metningsandel. Estimer også samvariasjon mellom lesere og tidskorrelasjon i støyen.

Sammenlign fordelingene på bakgrunn og linje. Tell hvor ofte bakgrunn gir falsk linje og hvor ofte ekte linje avvises, både per leser og etter den faktiske `analyzeLine()`-logikken. Bruk ulike opptak til tilpasning og kontroll av terskler. Nøyaktighet må måles mot kjent posisjon eller tilstand; lite støy alene betyr ikke god nøyaktighet.

Ved kontrollert sideforskyvning kjøres den virkelige sensoranalysen på opptakene. Konverter skalaen til meter med kjent sensoravstand når geometrien er jevnt fordelt. Sammenlign beregnet linjeposisjon med den målte forskyvningen: bias, RMS-feil, største observerte feil og andel ugyldige målinger.

### 3.4 Modell for sensorene

Et nyttig målebasert uttrykk for råverdien til leser i er:

```text
rå_i(t) = klipp(
    bakgrunn_i(h, V, lys)
    + kontrast_i(h, V, lys) * respons_i(linjeforskyvning, linjebredde, vinkel)
    + felles_støy(t)
    + lokal_støy_i(t)
    + motorpåvirkning_i(PWM, retning, strøm, t)
)
```

Kurvene kan først representeres av tabeller med interpolasjon. Vi trenger ikke tvinge alle sensorene til å ha identisk logistisk kurve slik standardmodellen gjør nå. Tidskorrelert og korrelert støy kan innføres etter at vi har sett på dataene.

Måletid og forsinkelse modelleres separat. Sensorresultatet skal leveres etter målingen, og styringen får holde forrige motorutgang mens den venter. For en bevegelig linje må vi undersøke hva avlesningen representerer over måleintervallet; en tidsbasert RC-måling er ikke automatisk et enkelt bilde fra intervallets midtpunkt.

## 4. Motorforsøk: én motor og retning om gangen

### 4.1 Ubelastet start og gange

Med hjulene løftet kjøres venstre og høyre motor hver for seg i begge retninger. Bruk flere relevante forsyningsnivåer innenfor motorens og driverens spesifikasjoner. Mål faktisk spenning ved hvert forsøk.

Øk PWM gradvis fra null. Noter når akselen begynner å bevege seg og når den går stabilt. Senk deretter PWM til den stopper. Gjenta flere ganger fra ulike akselposisjoner; børster og gir kan gjøre terskelen posisjonsavhengig.

For stabile punkter logges PWM, retning, spenning, strøm, RPM og temperatur/oppvarmingstilstand. Mål RPM med enkoder, optisk turteller eller video med tydelig hjulmarkering og kjent bildefrekvens. Video må ha tilstrekkelig tidsoppløsning for fenomenet som måles.

En første målebasert mapping kan være:

```text
stoppet motor starter hvis PWM >= start_PWM(V, last, retning)
motor i gang fortsetter hvis PWM >= gang_PWM(V, last, retning)
stabil RPM ≈ tabell(V, PWM, last, retning)
```

Dette er hysterese: motorens forrige tilstand påvirker resultatet. Én felles `deadzone_pwm` beskriver ikke nødvendigvis begge tersklene.

### 4.2 Belastning med hele roboten

Gjenta start-/gangtest på gulvet med full robotvekt, og eventuelt én eller to kjente ekstra belastninger. Gjør prøvene separat for rett frem, revers og rotasjon på stedet. Svinging kan ha betydelig annen motstand enn rett kjøring.

Mål hjulenes RPM og robotens faktiske bevegelse separat. Hvis hjulene snurrer uten tilsvarende forflytning, er det glidning, ikke høyere robotfart. Kontroller hvor mye vekt støttehjul/kule bærer i hver oppstilling.

### 4.3 Akselerasjon og nedbremsing

Gi repeterbare PWM-steg, for eksempel fra hvile til et valgt pådrag og mellom to pådrag i bevegelse. Logg posisjon/fart, hjul-RPM, spenning og strøm over tid. Test også nullpådrag og retningsskifte med den faktiske driverens bremse-/friløpsmodus.

Beregn fart og akselerasjon med lokal kurvetilpasning eller et dokumentert filter. Dobbel differensiering av støyete posisjonstall gir lett misvisende akselerasjonstopper. Behold råopptakene og oppgi tidsoppløsningen.

En respons av typen `n(t) ≈ n_slutt + (n_start - n_slutt)*exp(-t/T)` kan brukes som et første empirisk anslag rundt et valgt driftspunkt. Ikke anta én tidskonstant for alle spenninger, belastninger og retningsskifter.

### 4.4 Momentmåling

Med målt trekkraft ved et kjent momentarm-radius kan utgangsmoment estimeres:

```text
moment [Nm] = tangentiell kraft [N] * momentarm [m]
```

En kjent last kan også gi et målepunkt, men akselerasjon og friksjon i opphenget må tas med. Kraftmåling ved hjulomkretsen må skilles fra trekkraften som faktisk overføres til gulvet. Trekkraft er begrenset av grep.

Vi trenger ikke en langvarig blokkeringstest for å begynne modelleringen. Bruk først datablad eller en strømbegrenset belastningsmåling innenfor komponentenes grenser. Langvarig blokkering kan varme opp motor og driver raskt.

## 5. Modelluttrykk: spenning → strøm → moment → bevegelse

Disse er kandidater til den neste modellen, ikke ligninger som allerede er koblet inn i simulatoren. En enkel modell kan først bruke måletabeller. Mer fysisk parameterisering er nyttig når det finnes nok data til å identifisere parameterne.

### 5.1 Motorens elektriske og mekaniske del

For en børstet permanentmagnet-DC-motor er et vanlig utgangspunkt:

```text
L * di/dt = u_motor - R*i - k_e*omega_motor
moment_motor = k_t*i
J_rotor * d(omega_motor)/dt = moment_motor - friksjonsmoment_motor - lastmoment_motor
```

Her er u faktisk eller modellert gjennomsnittlig motorspenning, R viklingsmotstand, L induktans, i strøm, omega vinkelhastighet, k_e motspenningskonstant og k_t momentkonstant. Disse sammenhengene følger standard DC-motormodellering. [University of Michigan: motorens elektriske og mekaniske ligninger](https://seiler.engin.umich.edu/wp-content/uploads/sites/99/2020/01/ClassicalControl_Master.pdf).

PWM-kommandert spenning kan i en første middelverdimodell tilnærmes med fortegn ganger duty ganger forsyningsspenning, med korreksjon for drivertap. Dette må kontrolleres mot driverens PWM-/friløpsmodus. Ikke les gjennomsnittlig batteristrøm som om den alltid er den samme som viklingsstrømmen under PWM.

Ved neglisjerbar elektrisk transient blir strømmen omtrent:

```text
i ≈ (u_motor - k_e*omega_motor) / R
```

Da faller tilgjengelig motormoment med økende turtall ved samme motorspenning. Spenning og RPM alene bestemmer likevel ikke virkelig akselerasjon uten informasjon om last, strømbegrensning, gir og friksjon.

### 5.2 Friksjon og start

En første modell skiller statisk friksjon fra tap under bevegelse:

```text
Ved stillstand:
  friksjonen kan balansere drivmomentet inntil statisk terskel nås.

Ved rotasjon:
  friksjonsmoment ≈ coulomb_moment * sign(omega) + viskøs_faktor * omega
```

Tomgangsstrøm kan gi informasjon om tap ved tomgangsturtallet, men identifiserer ikke automatisk startfriksjon eller girfriksjon. Maxon beskriver både konstante og hastighetsavhengige tapsbidrag. [Maxon: friksjon og motordata](https://support.maxongroup.com/hc/en-us/articles/360013761160-Motor-data-and-simulation).

I modellen må stillstand håndteres eksplisitt. Å sette `sign(0) = 0` i en vanlig friksjonsformel gir ellers ingen motstand ved start. Startterskelen skal komme fra måling eller en identifisert statisk momentgrense.

### 5.3 Gir og hjul

Med girforhold G = motoromdreininger per hjulomdreining:

```text
omega_hjul = omega_motor / G
RPM_hjul = omega_hjul * 60 / (2*pi)
moment_hjul ≈ eta * G * overført_motormoment - eksternt_modellert_girtap
```

Eta er en virkningsgradstilnærming. Ikke trekk fra samme girtap både gjennom eta og en separat friksjonsparameter. Ved transienter går noe motormoment til å akselerere rotoren før moment overføres videre. Tilsvarende går noe hjulmoment til å akselerere selve hjulet.

Vi kan enten modellere disse treghetene eksplisitt eller bruke én konsistent effektiv modell som er tilpasset data. Ikke legg inn en fritt valgt RPM-rampe og samtidig en ekstra momentmodell som teller samme treghet to ganger.

### 5.4 Fra hjulmoment til faktisk akselerasjon

For ett hjul, med radius r og uten glidning:

```text
kontaktkraft ≈ (moment_hjul - J_hjul * d(omega_hjul)/dt) / r
```

For hele roboten langs kjøreretningen:

```text
m * dv/dt = F_venstre + F_høyre - F_rulling - m*g*sin(helning)
```

Ved oppstart på flatt underlag må drivverket overvinne statisk intern motstand og startmotstanden ved hjul/støtte. Når roboten beveger seg, øker ekstra masse tregheten og påvirker normalbelastning og rullemotstand. Masse er ikke i seg selv et konstant bremsemoment på flat vei.

For svinging trengs også rotasjonsligningen med hjulforskjell, treghetsmoment og målt svingmotstand. Rettkjøringsdata er ikke tilstrekkelig til å identifisere motstand mot rotasjon på stedet.

### 5.5 Grep og friksjon mot bakken

I en enkel statisk grepsmodell gjelder per hjul omtrent:

```text
sqrt(F_langs**2 + F_side**2) <= mu_statisk * normalbelastning_hjul
```

Samme grep kan ikke brukes fullt til både akselerasjon og svinging samtidig. Når grensen overskrides, trengs en glidemodell og eventuelt egen glidningskoeffisient. Dette er en mer egnet retning for neste modell enn dagens uavhengige grepstak.

Mål rullemotstand ved å trekke roboten rolig med kraftmåler i relevant friløpsoppsett. Dette må holdes atskilt fra grepsgrensen. For å anslå grep kan hjulene/roboten festes i en egnet prøveoppstilling med kjent normalbelastning og målt tangentiell kraft ved begynnende glidning. En hel robot som begynner å rulle på en skrå plate måler ikke uten videre dekkfriksjon.

## 6. Hva må endres i dagens simulator?

- Motorens mål-RPM er nå en enkel PWM-/spenningsmapping uten faktisk lastkobling. Den bør erstattes eller tilpasses med målte belastningskurver.
- Momentgrensen bruker nå summen av to `max_torque_nm` skalert med spenning, uten faktisk PWM-/strømkobling. Det kan overvurdere tilgjengelig akselerasjon ved lite pådrag.
- Start og videre gange bruker samme dødsone. Vi trenger målte start-/gangterskler og forskjeller mellom motorene.
- Friksjon, hjulspinn og glidning er forenklet. Dagens `slip_ratio` er et avvik i ønsket/oppnådd rotasjon, ikke målt fysisk hjulslipp.
- Sensorene bruker en felles idealisert respons og leverer verdier uten måleforsinkelse. Vi trenger leseravhengig respons, usikkerhet og tid.

Disse punktene identifiserer modellens mangler. De er ikke grunn til å skru tilfeldige standardverdier opp eller ned til bevegelsen «ser ekte ut».

## 7. Tilpasning og kontroll

Bruk én del av målingene til tilpasning, og andre opptak til kontroll. Kontroller at modellen forutsier start/stopp, RPM, spenningsfall og posisjon over tid på forsøk som ikke ble brukt i tilpasningen.

Ikke tilpass mange ukjente fysiske konstanter fra én RPM-kurve. Uten strøm- eller lastmåling kan flere kombinasjoner av motorkonstant, friksjon og girvirkningsgrad gi omtrent samme kurve. Da er en empirisk tabell med dokumentert gyldighetsområde bedre enn falskt presise parametre.

Lagre intervaller og variasjon mellom forsøk, ikke bare ett gjennomsnitt. Disse intervallene kan senere brukes til variasjon i treningsmiljøet. Start med området som faktisk er målt; marker ekstrapolering utenfor dette.

## 8. Første konkrete arbeidsøkt

1. Fyll inn geometri, masse, komponentnavn og forsyning i en kopi av `measurements/session_template.json`.
2. Ta sensoropptak på bakgrunn og svart flate med motorene av. Behold én fast kalibrering gjennom sammenlignbare opptak.
3. Gjenta samme optiske oppstilling med motorer i gang og noter PWM/spenning.
4. Mål start- og gangterskel for én motor med løftet hjul, ved én relevant spenning, i begge retninger.
5. Se på dataene før vi planlegger hele testmatrisen eller endrer simulatorens fysikk.

Dette er et avgrenset neste steg. Automatisk tilpasning, ny fysikkmotor og RL-trening kommer etter at de første opptakene er kontrollert.
