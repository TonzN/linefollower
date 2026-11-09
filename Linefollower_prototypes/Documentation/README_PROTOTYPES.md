# Utvikling av 3D-printede prototyper til linjefølgerroboten

**Prosjekt:** Linjefølgerrobot – fase 1 og fase 2  
**Omfang:** Syv hovedversjoner av 3D-printet chassis (V1–V7)  
**Fokus:** Designvalg, mekanisk konstruksjon, testing, utfordringer og læring  
**3D-modeller:** [Line_follower_prototypes på GitHub](https://github.com/TonzN/linefollower/tree/fase2_esp32/Line_follower_prototypes)

> **Om dokumentasjonen:** Denne teksten beskriver den faktiske utviklingsprosessen ut fra konstruktørens erfaringer, bilder av de utskrevne modellene og modellfilene i prosjektets GitHub-repositorium. Der det er lagt til tekniske forklaringer, er disse ment som faglige vurderinger av *hvorfor* et designvalg kan ha betydning. De er ikke presentert som måleresultater dersom det ikke finnes dokumentasjon på målingene.
>
> **Datoer:** Datoene i oversikten er oppgitt av konstruktøren som utskriftsdatoer. De er hentet fra et skjermbilde av lokale `.3mf`-filer, hvor Windows viser feltet «Date modified» (sist endret). Klokkeslettene er derfor registrerte filtidspunkter, ikke uavhengig bekreftede tidspunkt for når 3D-printeren fullførte hver utskrift.

## Innhold

1. [Bakgrunn og mål](#1-bakgrunn-og-mål)
2. [Komponenter og grunnleggende konstruksjon](#2-komponenter-og-grunnleggende-konstruksjon)
3. [Oversikt over alle syv versjoner](#3-oversikt-over-alle-syv-versjoner)
4. [Versjon 1 – første testchassis](#4-versjon-1--første-testchassis)
5. [Versjon 2 – kompaktering og nye festeløsninger](#5-versjon-2--kompaktering-og-nye-festeløsninger)
6. [Versjon 3 – batterier under Arduino og bedre casterfeste](#6-versjon-3--batterier-under-arduino-og-bedre-casterfeste)
7. [Versjon 4 – modulær sensor og skrufester til motorene](#7-versjon-4--modulær-sensor-og-skrufester-til-motorene)
8. [Versjon 5 – ferdig chassis for fase 1](#8-versjon-5--ferdig-chassis-for-fase-1)
9. [Versjon 6 – første chassis i fase 2](#9-versjon-6--første-chassis-i-fase-2)
10. [Versjon 7 – avstiving og etasjeløsning](#10-versjon-7--avstiving-og-etasjeløsning)
11. [Erfaringer på tvers av versjonene](#11-erfaringer-på-tvers-av-versjonene)
12. [Evaluering av utviklingsarbeidet](#12-evaluering-av-utviklingsarbeidet)
13. [Videre arbeid og anbefalt testplan](#13-videre-arbeid-og-anbefalt-testplan)
14. [Modellfiler og kilder](#14-modellfiler-og-kilder)

---

## 1. Bakgrunn og mål

Målet med utviklingsarbeidet var å lage et 3D-printet chassis som kunne samle de viktigste delene til en linjefølgerrobot på en oversiktlig, kompakt og stabil måte. Chassiset skulle gi faste plasser til elektronikk, batterier, motorer, hjul og linjesensor, samtidig som det måtte være mulig å montere delene uten unødvendig kompliserte løsninger.

Arbeidet ble gjennomført som en **iterativ prototypeprosess**. Det betyr at én fysisk modell ble utviklet og vurdert før erfaringene ble brukt til å lage en ny versjon. Prosessen startet med et enkelt testchassis og utviklet seg gradvis til en ferdig modell for fase 1. Da fase 2 begynte, førte nye krav til elektronikk og plassering til at konstruksjonen måtte videreutvikles igjen.

Designarbeidet hadde særlig disse målene:

- Tilpasse chassiset til de faktiske komponentene, uten unødvendig tomrom.
- Plassere batterier og andre komponenter slik at roboten sto stabilt.
- Sikre at linjesensoren satt godt og hadde en hensiktsmessig plassering foran hjulene.
- Lage pålitelige fester for motorer, hjul, Arduino og øvrige deler.
- Gjøre senere endringer enklere ved å kunne skrive ut utvalgte deler separat.
- Etter hvert forbedre utseendet, uten å miste de mekaniske fordelene.

**Viktig skille:** En prototype kan ha hatt problemer med *montering, passform eller statisk belastning* uten at det oppsto mekaniske feil *under kjøring*. Ifølge erfaringene fra prosjektet var det **V1s lange sensorarm som ga den tydelige mekanisk betingede utfordringen under kjøring**. De andre oppgitte problemene gjelder først og fremst utforming, montering, stabilitet eller bæreevne før en normal kjøretest. Dette skillet er gjennomgående i dokumentasjonen.

## 2. Komponenter og grunnleggende konstruksjon

### 2.1 Komponentliste for fase 1

| Komponent | Antall | Rolle i roboten |
| --- | ---: | --- |
| Arduino Uno | 1 | Mikrokontroller som mottar sensorinformasjon og styrer roboten gjennom programvaren. |
| Breadboard | 1 | Koblingsbrett for å organisere elektroniske tilkoblinger og teste kretsen. |
| 9 V-batterier | 2 | Strømforsyningskomponenter i den opprinnelige roboten. |
| Motordriver | 1 | Elektronikk som gjør det mulig å styre motorene fra mikrokontrolleren. |
| IR-linjesensorer fra Pololu | 6 | Registrerer den svarte linjen på underlaget; den nøyaktige sensormodellen er ikke angitt her. |
| DC-motorer | 2 | Driver hvert sitt hjul og gjør det mulig å svinge ved ulik motorhastighet. |
| Drivhjul | 2 | Overfører motorkraft til underlaget. |
| Casterhjul / støttehjul | 1 | Støtter fronten slik at roboten får et tredje kontaktpunkt mot underlaget. |
| 3D-printet chassis | 1 hovedkonstruksjon | Holder og plasserer komponentene i forhold til hverandre. |

Denne listen gjelder **fase 1**. I fase 2 ble elektronikkoppsettet endret, blant annet ved overgangen fra Arduino Uno til Arduino Nano under utviklingen av V6 og V7. GitHub-branchen `fase2_esp32` beskriver en **senere ESP32-retning** i programvarearbeidet. Den skal ikke forveksles med komponentene som ble brukt eller planlagt da disse syv chassisversjonene ble utviklet.

### 2.2 Konstruksjonsprinsipp

Robotens to DC-motorer driver hvert sitt hjul, mens casterhjulet gir støtte foran. Sensorene sitter lenger fremme enn drivhjulene og registrerer linjen før roboten fysisk kommer frem til det samme punktet. Dette gjør avstanden mellom sensorene og hjulakslingen viktig: den påvirker *når* roboten begynner å reagere på svinger.

Komponentenes vekt og høyde over underlaget spiller også en rolle. Batterier som plasseres langt bak, kan gjøre roboten baktung. Lange, tynne armer kan bøyes under belastning, og et upresist motorfeste kan føre til at drivhjulene sitter skjevt.

### 2.3 Skruehull: et element som fungerte fra starten

**Skruehullene var riktig dimensjonert i alle syv modellene og skapte aldri problemer.** Dette gjelder skruehullene som faktisk inngikk i hver enkelt konstruksjon. Enkelte versjoner endret antallet festepunkter eller selve festeløsningen, men ikke fordi hullene hadde feil størrelse.

Dette er en viktig positiv erfaring: festegeometrien kunne i stor grad videreføres mellom versjonene, slik at utviklingsarbeidet i stedet kunne konsentreres om plassering av komponenter, styrke og montering. Det er samtidig viktig å skille mellom **riktig skruehull** og **riktig plassering av hele motoren eller komponenten**; V4 forbedret sistnevnte ved å gå fra dobbeltsidig teip til skrufester.

## 3. Oversikt over alle syv versjoner

| Versjon | Registrert dato og klokkeslett | Fase | Hovedendring | Resultat |
| --- | --- | --- | --- | --- |
| **V1** | 24.09.2026 kl. 15:25 | 1 | Første chassis med Arduino-, batteri-, hjul- og sensorplass | Brukbar testplattform, men for stort og sensor for langt foran |
| **V2** | 25.09.2026 kl. 14:21 | 1 | Mer kompakt form, kortere sensorarm, større batteribokser og forsøk på casterfeste | Forkastet etter problemer med balanse og passform |
| **V3** | 28.09.2026 kl. 13:03 | 1 | Batterier under Arduino og ny løsning for casterhjulet | Betydelig mer kompakt; sensorrammen ble for romslig |
| **V4** | 30.09.2026 kl. 13:30 | 1 | Separat sensorfeste, lavere sensor og motorfester med skruer | Mer modulært og presis montering |
| **V5** | 02.10.2026 kl. 09:53 | 1 | Bedre batterifeste, avrundet form og spoiler | **Endelig modell for fase 1** |
| **V6** | 22.10.2026 kl. 10:34 | 2 | Tilpasset Arduino Nano, endret breadboard og utvendige motorarmer | Godt batterifeste, men armene bøyde seg under belastning |
| **V7** | 29.10.2026 kl. 12:36 | 2 | Støtte til motorarmene og løsning med to nivåer | Utviklet, men ikke tatt i bruk |

*Datoene stammer fra den lokale filoversikten. Merk at modellfilene kan ha andre navn på datamaskinen enn i GitHub-arkivet; koblingen mellom versjon og GitHub-fil er oppgitt i kapittel 14.*

### Bilder av prototypeutviklingen

**V1–V4 – de fire første utskrevne chassisene, fra venstre mot høyre:**

![Fire tidlige 3D-printede chassis fra versjon 1 til 4](bilete/prototypar_v1_v4.png)

**V5 – sluttmodellen i fase 1:**

![Versjon 5 med avrundet form, integrert batteriplass og spoiler](bilete/prototype_v5.png)

**V6 – første chassis for fase 2:**

![Versjon 6 med lange utvendige armer for motorene](bilete/prototype_v6.png)

**V7 – videreutvikling med avstiving og separat plate:**

![Versjon 7 med diagonal støtte og en løs plate til etasjeløsningen](bilete/prototype_v7.png)

*Bildene viser de utskrevne delene, ikke nødvendigvis en ferdig montert robot. Vurderingene av funksjon er hovedsakelig basert på erfaringene som er oppgitt av konstruktøren.*

---

## 4. Versjon 1 – første testchassis

**Registrert dato:** 24. september 2026 kl. 15:25  
**3D-modell:** [`LF.v1.3mf`](https://github.com/TonzN/linefollower/blob/fase2_esp32/Line_follower_prototypes/LF.v1.3mf)  
**Status:** Testprototype, videreutviklet

### Hensikt og designvalg

V1 ble laget for å få på plass en enkel, fysisk plattform som kunne brukes til å teste robotens komponenter. På dette tidspunktet var det viktigere å få et fungerende utgangspunkt enn å lage et så lite og optimalisert chassis som mulig.

Modellen fikk fire skruehull til Arduino Uno, et eget område for breadboard, to batteriplasser ved siden av elektronikken, to utstikk for motorer og drivhjul samt en lang frontarm med et sensorfeste ytterst. Den tydelige oppdelingen gjorde det enkelt å se hvor hver komponent skulle monteres.

### Hva fungerte?

- **Sensorfestet traff svært godt på passformen.** Sensoren kunne presses rett inn og satt slik den var tenkt.
- Skruehullene hadde riktig størrelse allerede i første versjon.
- Modellen gjorde det mulig å sammenligne CAD-tegningen med fysisk plassbehov for komponentene.
- Den enkle utformingen ga et godt utgangspunkt for å planlegge neste iterasjon.

### Ulemper i konstruksjonen

Chassiset var altfor stort i forhold til komponentene som skulle monteres. Det var mye ubrukt plass, og batterirommene var samtidig for små til at 9 V-batteriene passet ordentlig. Dette viste at et stort chassis ikke nødvendigvis gir god plassutnyttelse: størrelsen må tilpasses akkurat der komponentene faktisk trenger rom.

Den mest alvorlige svakheten var likevel plasseringen av linjesensoren. Sensorarmen var for lang og flyttet målepunktet for langt foran drivhjulene.

### Hva skjedde under kjøring?

**V1 hadde det tydeligste mekanisk betingede kjøreproblemet i utviklingsløpet.** Når roboten nærmet seg en sving, registrerte sensoren svingen mens hjulene fortsatt var et stykke unna. Roboten begynte dermed å svinge før drivhjulene hadde kommet frem til stedet der banen endret retning.

Dette var spesielt problematisk i **U-svinger**. Roboten kunne ende opp stående på tvers av svingen. I slike situasjoner kunne hele sensorrekken til slutt se den svarte linjen samtidig, og roboten stoppet opp i stedet for å komme seg gjennom svingen.

Den observerte sammenhengen var altså:

1. Sensoren oppdaget svingen tidlig på grunn av den lange sensorarmen.
2. Styringen reagerte før hjulene var kommet frem til svingen.
3. I en skarp U-sving kunne roboten bli stående på tvers, med svart linje under store deler av eller hele sensorrekken.

Det er ikke dokumentert sensorlogger fra disse testene. Derfor kan man ikke fastslå nøyaktig hvilken del av styringsalgoritmen som utløste selve stoppen. Det som er dokumentert gjennom erfaringen, er at **sensorens fysiske plassering skapte et tydelig samspillproblem mellom konstruksjon og styring**.

### Faglig vurdering og lærdom

En fremskutt sensor gir forhåndsinformasjon om banen. Det kan i noen løsninger være en fordel, men avstanden må stå i forhold til hjulplassering, robotens hastighet og reguleringsmetoden. I V1 var avstanden for stor for det aktuelle oppsettet. Erfaringen viste hvorfor mekanikken ikke kan vurderes uavhengig av programvaren.

**Konklusjon V1:** God første testplattform og svært vellykket sensorpassform, men for stort chassis, upraktiske batteriplasser og en sensorplassering som faktisk forstyrret kjøringen.

## 5. Versjon 2 – kompaktering og nye festeløsninger

**Registrert dato:** 25. september 2026 kl. 14:21  
**3D-modell:** [`LF v2.3mf`](https://github.com/TonzN/linefollower/blob/fase2_esp32/Line_follower_prototypes/LF%20v2.3mf)  
**Status:** Forkastet som helhet, men ga viktig erfaring

### Hensikt og designvalg

Etter V1 var målet å redusere størrelsen og løse de tydeligste svakhetene. Hjulene ble flyttet lenger frem, sensorarmen ble kortere, og batteriboksene ble laget større. Hjørnene ble avrundet for å gi chassiset et mer gjennomført utseende. Arduino Uno ble også flyttet **5 mm nærmere breadboardet** for å utnytte plassen bedre.

Det ble dessuten prøvd en ny løsning for casterhjulet. Et hjul fra en modifisert caster skulle kunne settes inn i en egen åpning foran, med en sylinder/aksel som skulle presses inn gjennom to hull for å holde hjulet på plass.

### Forventede fordeler

- Mindre og ryddigere chassis enn i V1.
- Kortere avstand mellom linjesensor og drivhjul.
- Større batterirom, med bedre plass til selve batteriene.
- Mer integrert løsning for casterhjulet.
- Mindre unødvendig avstand mellom Arduino Uno og breadboardet.

### Hva gikk galt?

**1. Roboten ble baktung.** Batteriene fikk mer plass, men plasseringen gjorde at roboten tippet bakover. En geometrisk løsning på batteriproblemet skapte dermed et nytt problem med tyngdepunkt og balanse.

**2. Casterhjulet passet ikke i festet.** Den planlagte aksel- og hullkonstruksjonen lot seg ikke montere slik den var tegnet. Dette gjaldt det spesielle casterfestet, ikke de vanlige skruehullene i chassiset.

**3. Sensorfestet mistet presspassformen.** Sensorarmene var gjort tynnere. De fikk en liten bøy, og sensorrammen beholdt ikke den formen som var nødvendig for å presse sensoren sikkert på plass. V1s vellykkede sensorfeste var dermed ikke automatisk videreført til det slankere designet.

### Faglig vurdering og lærdom

V2 viste at **kompakt design ikke bare handler om å fjerne materiale**. Batterienes masse må plasseres slik at robotens støttepunkter fortsatt kan bære vekten stabilt. Det er også nødvendig å kontrollere at tynnere strukturer har nok stivhet til å holde geometrien under montering og belastning.

En presspasning er særlig følsom for små avvik. Selv om åpningen i CAD-modellen har riktige mål, kan svikt i den omkringliggende konstruksjonen føre til at den faktiske passformen blir dårlig.

**Konklusjon V2:** En ambisiøs forbedring av kompakt utforming, men løsningen fungerte ikke som planlagt ved montering og balansekontroll. Modellen ble forkastet, mens erfaringene styrte utviklingen av V3. Dette skal ikke omtales som et dokumentert kjøreproblem på linjebanen.

## 6. Versjon 3 – batterier under Arduino og bedre casterfeste

**Registrert dato:** 28. september 2026 kl. 13:03  
**3D-modell:** [`LF v3.3mf`](https://github.com/TonzN/linefollower/blob/fase2_esp32/Line_follower_prototypes/LF%20v3.3mf)  
**Status:** Vellykket videreutvikling, men sensorfeste måtte justeres

### Hensikt og designvalg

Hovedideen var å bruke plassen **under Arduino Uno** til batteriene, i stedet for å sette av store separate batterirom. Arduino-festene ble hevet for å skape plass i høyden, og to av de fire opprinnelige festepunktene ble fjernet for å gi rom til batteriene. De gjenværende skruehullene hadde fortsatt riktig passform.

Dette var et viktig designskifte: i stedet for å utvide robotens lengde eller bredde, ble to komponentgrupper plassert på forskjellige nivåer i samme område.

Casterhjulet fikk også en ny løsning med et lite sylindrisk feste, en separat sylinder/aksel ved hjulet og to deler som holdt sylinderen fast i konstruksjonen. Denne løsningen fungerte langt bedre enn forsøket i V2.

### Fordeler

- Batteriplassen ble integrert under Arduino, noe som gjorde roboten vesentlig mer kompakt.
- De store batteriboksene fra V2 var ikke lenger nødvendige.
- Det nye casterfestet ga en fungerende monteringsmetode.
- Løsningen demonstrerte at høyden på chassiset kunne brukes aktivt for å spare grunnflate.

### Ulempe og forbedringsbehov

Sensorfestet var blitt for trangt og fleksibelt i V2. For å unngå dette ble sensorrammen i V3 laget større, men endringen ble for stor. Sensoren fikk for mye klaring, og rammen holdt den derfor ikke så nøyaktig som ønsket.

Dette var et **passformproblem under montering**, ikke et rapportert problem under kjøring.

### Faglig vurdering og lærdom

V3 var et godt eksempel på hvordan man kan spare plass ved å bygge i høyden. Samtidig må det være nok klaring mellom batterier, ledninger og elektronikk, og batteriene bør fortsatt være tilgjengelige for bytte eller service.

Sensorfestet viste hvor følsomme presspasninger kan være: V2 hadde for liten praktisk klaring fordi rammen deformerte seg, mens V3 hadde for stor klaring. Målet for videre utvikling ble dermed å beholde riktig passform og gjøre sensoren enklere å justere separat.

**Konklusjon V3:** Et stort steg mot et kompakt og praktisk chassis. Batteriplasseringen og casterfestet ble forbedret; hovedpunktet som gjensto var sensorrammens størrelse.

## 7. Versjon 4 – modulær sensor og skrufester til motorene

**Registrert dato:** 30. september 2026 kl. 13:30  
**3D-modell:** [`LF v4.3mf`](https://github.com/TonzN/linefollower/blob/fase2_esp32/Line_follower_prototypes/LF%20v4.3mf)  
**Status:** Videreutviklet til V5

### Hensikt og designvalg

I V4 ble **sensorfestet og hovedchassiset delt i to separate deler**. Tidligere hadde endringer i sensorrammen kunnet kreve ny utskrift av en større del av roboten. Nå kunne rammen endres og printes på nytt uten å måtte erstatte hele chassiset.

Sensoren ble dessuten plassert nærmere bakken. Dette skulle gi en mer hensiktsmessig monteringshøyde for linjeavlesningen.

Et annet viktig grep var å erstatte **dobbeltsidig teip med skrufester for motorene**. I de tre første modellene hadde teipmontering gjort det vanskeligere å få motorer og hjul helt symmetriske. I V4 fikk de en definert og repeterbar posisjon.

### Fordeler

- Sensorfestet kunne byttes ut eller justeres uten ny utskrift av hovedchassiset.
- Lavere sensorplassering ga bedre kontroll over avstanden til underlaget.
- Motorer og drivhjul satt mer konsekvent og rett i forhold til chassiset.
- Skrufestene gjorde montering og demontering enklere og mer pålitelig enn teip.
- Konstruksjonen ble mer modulær og bedre egnet for videre utvikling.

### Ulemper og avveininger

En separat sensorramme introduserer et ekstra sammenføyningspunkt. Dette krever at overgangen mellom ramme og chassis er stiv nok og holder samme posisjon hver gang den monteres. En lavere plassert sensor må også ha tilstrekkelig klaring til underlaget.

Dette er generelle konstruksjonshensyn, **ikke rapporterte feil ved V4**. Det ble heller ikke rapportert problemer med skruehullenes dimensjoner.

### Faglig vurdering og lærdom

Det er viktig å skille mellom problemer med en skrue og problemer med plasseringen av delen som skruen holder. Selve skruehullene hadde vært riktige fra starten, men den tidligere teipmonteringen kunne likevel gi skjev hjulplassering. V4 forbedret **motorenes posisjonering og innfesting**, ikke en tidligere feil med hulldiameteren.

Sensorens avstand til banen er også viktig fordi IR-avlesning kan påvirkes av høyde, vinkel og underlag. Det er ikke oppgitt målte sensorverdier for V4, så forbedringen beskrives her som en konstruksjonsendring og ikke som et dokumentert utslag i målenøyaktighet.

**Konklusjon V4:** Et viktig steg fra samlet testkonstruksjon til et mer modulært chassis med faste, presise motorfester. Ujevn montering fra teip ble eliminert.

## 8. Versjon 5 – ferdig chassis for fase 1

**Registrert dato:** 2. oktober 2026 kl. 09:53  
**3D-modell:** [`LF v5.3mf`](https://github.com/TonzN/linefollower/blob/fase2_esp32/Line_follower_prototypes/LF%20v5.3mf)  
**Status:** **Endelig modell i fase 1**

### Hensikt og designvalg

V5 skulle ferdigstille chassisdesignet for fase 1. De viktigste mekaniske forbedringene var allerede gjennomført i tidligere versjoner. Den siste funksjonelle endringen var å lage et bedre **batterifeste**, siden batteriene tidligere kunne ligge for løst under Arduino Uno.

I tillegg ble det lagt mer arbeid i utseendet. Chassiset fikk mykere, avrundede former, og en spoiler ble lagt til for å gi roboten et mer helhetlig og særpreget uttrykk.

### Fordeler

- Batteriene fikk en tydeligere og sikrere plassering.
- Det kompakte oppsettet fra V3 og den modulære tankegangen fra V4 ble videreført.
- Motorene kunne fortsatt festes med skruer og monteres symmetrisk.
- Robotens form ble mer gjennomført, uten at det gikk på bekostning av de kjente mekaniske fordelene.
- Modellen fungerte svært godt til formålet og ble brukt som sluttmodell for fase 1.

### Ulemper og begrensninger

Det er **ikke rapportert vesentlige mekaniske feil under kjøring med V5**. Det er likevel riktig å dokumentere en mindre, gjennomgående geometrisk utfordring: drivhjulene var noe store sammenlignet med det mindre casterhjulet. Dette gjorde at **robotens front lå litt lavere mot bakken enn bakdelen**.

Forskjellen var liten og ble ikke vurdert som et stort problem i fase 1. Den er heller ikke dokumentert som årsak til stopp eller svikt under kjøring. Den bør derimot tas med i vurderingen av senere chassis, særlig når sensorens høyde og vinkel skal fastsettes mer presist.

Spoileren var først og fremst et designvalg. Den bør derfor omtales som estetisk forbedring, ikke som dokumentert forbedring av fart, stabilitet eller aerodynamikk.

### Faglig vurdering og lærdom

V5 viser overgangen fra funksjonell prototype til en mer gjennomarbeidet sluttmodell. Et slikt punkt er viktig i produktutvikling: når en løsning oppfyller kravene godt nok, kan det være mer nyttig å dokumentere og ta den i bruk enn å fortsette med små endringer uten et konkret problem å løse.

**Konklusjon V5:** Den endelige og vellykkede modellen for fase 1. Batterifeste, montering og formgiving ble forbedret, samtidig som en liten høydeforskjell mellom front og bakende ble stående som et mulig punkt for fremtidig justering.

## 9. Versjon 6 – første chassis i fase 2

**Registrert dato:** 22. oktober 2026 kl. 10:34  
**3D-modell:** [`Robot v6.3mf`](https://github.com/TonzN/linefollower/blob/fase2_esp32/Line_follower_prototypes/Robot%20v6.3mf)  
**Status:** Videreutviklet til V7

### Hensikt og designvalg

Da fase 2 begynte, ble det aktuelt å endre elektronikkoppsettet. I denne delen av utviklingen ble Arduino Uno erstattet med **Arduino Nano**, som også gjorde det nødvendig å snu breadboardet og endre komponentplasseringen.

Samtidig var et viktig mål å beholde den eksisterende **plasseringen av drivhjulene og linjesensoren**. Programvaren var tilpasset geometrien til den tidligere modellen, og en endret avstand mellom sensor og hjul kunne kreve nye justeringer i styringen.

Løsningen ble derfor å lage et nytt, mer sentralt chassis med **to armer som strakte seg ut til motorene og hjulene**. Dette gjorde det mulig å endre hovedkroppen og elektronikkplassen, samtidig som sentrale ytre posisjoner ble videreført. Det ble også designet et nytt separat batterifeste, og dette fungerte godt.

### Fordeler

- Chassiset ble tilpasset det nye elektronikkoppsettet.
- Hjul- og sensorplassering kunne beholdes som utgangspunkt for eksisterende kode og regulering.
- Et separat batterifeste ga bedre fleksibilitet ved montering.
- Den åpne utformingen viste muligheter for videre ombygging og plassutnyttelse.

### Problem ved mekanisk belastning

Motorarmene ble for svake til å holde den ønskede posisjonen under vekten av roboten. De bøyde seg noe ned, og roboten sank litt sammen. Det ble derfor klart at armene trengte ekstra støtte eller avstiving.

Dette er et **konstruksjons- og belastningsproblem**, og det skal ikke uten videre omtales som en feil som ble observert under normal kjøring på linjebanen.

### Faglig vurdering og lærdom

En lang, utstikkende arm får et bøyemoment når hjulkreftene virker i enden. Hvor mye armen bøyes, avhenger blant annet av lengde, tykkelse, tverrsnitt, plastmateriale, printretning og belastning. Å beholde hjulenes plassering er bare nyttig dersom armene også er stive nok til å holde plasseringen i virkeligheten.

V6 illustrerer derfor en viktig sammenheng mellom **systemkrav og mekanisk styrke**: det er fornuftig å bevare kjent sensor- og hjulgeometri, men en ny bærende konstruksjon må tåle belastningen fra den faktiske roboten.

**Konklusjon V6:** Et godt første design for fase 2 med vellykket batterifeste og videreført hjul-/sensorplassering. Neste versjon måtte først og fremst gjøre motorarmene stivere.

## 10. Versjon 7 – avstiving og etasjeløsning

**Registrert dato:** 29. oktober 2026 kl. 12:36  
**3D-modell:** [`Robot v7.3mf`](https://github.com/TonzN/linefollower/blob/fase2_esp32/Line_follower_prototypes/Robot%20v7.3mf)  
**Status:** Utviklet, men **ikke tatt i bruk**

### Hensikt og designvalg

V7 ble laget for å løse svakheten ved motorarmene i V6. Det ble lagt til **diagonale støttearmer/avstivinger** mellom de utvendige motorfestene og hovedchassiset. Hensikten var å redusere nedbøying og gjøre konstruksjonen mer stabil under belastning.

Samtidig viste planleggingen av elektronikken at **ett breadboard ikke ville være tilstrekkelig**. Derfor ble det utviklet en løsning med **to etasjer**, der en separat plate kunne gi et ekstra monteringsnivå over hovedchassiset. På bildet av V7 vises både det avstivede chassiset og den løse platen.

### Forventede fordeler

- Diagonale støtter kunne gjøre motorarmene mer motstandsdyktige mot bøying.
- To nivåer kunne gi mer plass til elektronikk uten å gjøre robotens grunnflate tilsvarende større.
- En separat plate kunne gjøre det lettere å fordele komponenter og endre oppsettet senere.
- Designet bygget direkte på erfaringene med V6.

### Hvorfor modellen ikke ble brukt

Kort tid etter denne iterasjonen ble det klart at gruppen ønsket flere endringer i løsningen. V7 ble derfor **ikke montert og tatt i bruk som robot**. Det finnes dermed ikke grunnlag for å hevde at avstivingen var tilstrekkelig testet, at toetasjeløsningen fungerte i drift, eller at modellen hadde bestemte kjøreproblemer.

### Mulige ulemper som bør undersøkes senere

- Ekstra nivåer kan gjøre kabling og tilgang til komponenter mer komplisert.
- Tunge komponenter plassert høyere kan påvirke tyngdepunktet.
- Støttearmene bruker noe mer materiale og kan komme i veien for montering.
- En separat plate må sitte stivt og samtidig kunne demonteres ved behov.

Disse punktene er **forventede designhensyn**, ikke feil som ble påvist med V7.

### Faglig vurdering og lærdom

V7 viser en naturlig respons på et mekanisk problem: når lange armer bøyer seg, kan avstiving være en mulig løsning. Samtidig ble konstruksjonen påvirket av et nytt krav til elektronikkplass. At modellen ikke ble tatt i bruk, gjør den ikke verdiløs; den dokumenterer hvordan designet utviklet seg og hvorfor neste iterasjon måtte vurderes på nytt.

**Konklusjon V7:** En mellomprototype med planlagt avstiving og ekstra etasje. Den ble ikke praktisk verifisert fordi nye designkrav kom før den ble tatt i bruk.

---

## 11. Erfaringer på tvers av versjonene

### 11.1 Sensorplassering er en del av styringssystemet

Den viktigste erfaringen fra kjøringen kom allerede i V1: en sensor som sitter for langt foran drivhjulene, kan føre til at roboten begynner å svinge før hjulene når selve svingen. Dette ble spesielt tydelig i U-svinger, der roboten kunne bli stående på tvers og sensorrekken registrere svart linje over hele bredden.

Senere modeller fikk kortere sensorarm, og V4 gjorde det mulig å justere sensorfestet separat. Det er likevel ikke oppgitt eksakte avstandsmål eller sammenlignbare kjørelogger. Dokumentasjonen skal derfor ikke påstå at én bestemt sensoravstand er optimal for alle hastigheter og svinger.

**Praktisk lærdom:** Endringer i sensorens plassering må vurderes sammen med hjulavstand, kjørehastighet og programvarens regulering.

### 11.2 Tyngdepunkt og batteriplassering

V2 viste at en robot kan bli ustabil når de tyngre komponentene er plassert for langt bak. V3 løste plassutfordringen på en annen måte ved å flytte batteriene under Arduino. V5 forbedret batterifestet ytterligere slik at batteriene ikke lå like løst.

**Praktisk lærdom:** Et batteri trenger ikke bare nok rom; det må også ligge på et hensiktsmessig sted, være sikret og kunne byttes ved behov.

### 11.3 Passform, toleranser og skruehull

Sensorfestet utviklet seg fra perfekt presspasning i V1, via en bøyd og for trang løsning i V2, til en for romslig ramme i V3. Denne erfaringen viser at en god presspasning ikke bare avhenger av hullets CAD-mål, men også av stivheten i konstruksjonen og toleransene etter utskrift.

**Skruehullene var derimot gjennomgående riktige i alle versjoner.** Det er viktig å bevare dette som en dokumentert styrke i utviklingsarbeidet, fremfor å fremstille skruehullene som et problem som måtte fikses.

### 11.4 Motorfester og symmetrisk hjulplassering

I V1–V3 ble motorene festet med dobbeltsidig teip. Det kunne føre til at de ikke satt helt likt. V4 innførte skrufester som ga en mer bestemt plassering, og skjevheten fra den tidligere monteringsmetoden ble eliminert.

**Praktisk lærdom:** Når mekanisk montering blir mer repeterbar, blir det enklere å vite om en eventuell avvikende bevegelse skyldes konstruksjonen eller reguleringen.

### 11.5 Høydeforskjell mellom drivhjul og casterhjul

En mindre utfordring som har vært til stede, er at **drivhjulene er forholdsvis store mens casterhjulet er lite**. Det gjør at roboten heller litt fremover, slik at fronten kommer nærmere bakken enn bakenden. Dette ble ikke vurdert som et stort problem under kjøring, men det er en geometrisk svakhet som bør vurderes ved senere endringer.

Gruppen planlegger å **3D-printe egne hjul** for å jevne ut høydeforskjellen. En slik løsning bør ikke bare vurderes ut fra diameteren: man må også kontrollere hjulfestenes høyde, kontakt med underlaget, friksjon og hvordan endringen påvirker sensorhøyden. En ny hjuldiameter endrer dessuten hvor langt roboten kjører per hjulomdreining og kan derfor kreve justering av hastighet eller styring.

**Praktisk lærdom:** Små høydeforskjeller kan være akseptable i en fungerende prototype, men bør måles før man fastsetter ny hjulgeometri.

### 11.6 Stivhet i 3D-printede armer

V2 viste at en tynn sensorarm kan bøye seg nok til at en presspasning ikke fungerer. V6 viste et beslektet problem med motorarmer som bøyde seg under robotens vekt. V7 forsøkte å løse det siste ved hjelp av diagonale støtter.

**Praktisk lærdom:** Vektbesparelse og slank utforming må balanseres mot hvor stiv delen trenger å være. Bøying kan påvirke både montering og de fysiske posisjonene som programvaren er tilpasset.

### 11.7 Modularitet sparer omarbeiding

V4s separate sensorramme gjorde det mulig å endre bare én del. V6s nye batterifeste og V7s planlagte ekstra plate videreførte ideen om at komponenter kan ha egne, utskiftbare monteringsløsninger.

**Praktisk lærdom:** Når en del sannsynligvis skal endres flere ganger, kan separat utskrift spare både tid og materiale.

### 11.8 Samspill mellom mekanikk og elektronikk

Fase 1 endte med en fungerende modell tilpasset Arduino Uno, mens fase 2 begynte med endringer i elektronikken. I V6 var målet å beholde hjul- og sensorposisjoner for å redusere behovet for å endre programvaren samtidig. I V7 oppsto behovet for mer breadboardplass, noe som igjen påvirket den mekaniske konstruksjonen.

**Praktisk lærdom:** Mekanisk design bør ikke låses før gruppen har tilstrekkelig oversikt over elektronikken og hvor mye fysisk plass den krever.

## 12. Evaluering av utviklingsarbeidet

### Hva fungerte godt i prosessen?

**1. Én prototype løste konkrete problemer fra den forrige.** Utviklingen hadde en tydelig retning: fra størrelse og sensorplassering i V1–V2, via kompakt batteriplassering i V3, til bedre montering og modularitet i V4–V5. Fase 2 introduserte deretter nye krav som førte til V6–V7.

**2. Fysiske tester avslørte feil som var vanskelige å se på skjermen.** Batterienes påvirkning på balansen, casterfestets passform og bøying av tynne armer ble tydelige først når modellene skulle monteres eller belastes.

**3. Flere gode løsninger ble bevart mellom versjonene.** Sensorfestet fra V1 viste at riktig presspasning var mulig. Skruehullene fungerte i alle versjonene. Den kompakte oppbygningen fra V3 og motorfestene fra V4 ble videreført i V5.

**4. Fase 1 fikk et tydelig sluttresultat.** V5 fungerte svært godt for formålet og ble valgt som sluttmodell. Det er en styrke å kunne avslutte en iterasjonsrunde når kravene er oppfylt.

### Hva kunne vært gjort mer systematisk?

- Måle og notere avstanden mellom linjesensor og drivhjul for hver versjon.
- Registrere høyde på sensor og chassis, samt forholdet mellom drivhjul og casterhjul.
- Dokumentere masse og tyngdepunkt med alle komponenter montert.
- Skrive ut små testdeler av kritiske presspasninger før hele chassiset printes.
- Ta bilder av samme vinkel for hver prototype, både tom og ferdig montert.
- Skille tydelig i en testlogg mellom **montering**, **statisk belastning** og **kjøring på banen**.

Dette er forbedringsforslag til arbeidsmetoden, ikke påstander om at målingene eller testene allerede er gjennomført.

### Samlet resultat

Det utviklingsløpet viser tydeligst, er at et godt chassis ikke bare må gi plass til alle komponentene. Det må også holde dem riktig i forhold til hverandre, tåle belastning, være enkelt å montere og gi forutsigbare mekaniske betingelser for robotens styring.

**Fase 1 ble avsluttet med V5 som en fungerende sluttmodell. V6 og V7 representerer videre utforsking i fase 2, ikke nye endelige roboter.**

## 13. Videre arbeid og anbefalt testplan

### 13.1 Prioriteringer for neste chassis

1. **Avklar komponentoppsettet først.** Bestem mikrokontroller, antall breadboards eller kretskort, sensorer, motorer, strømforsyning og nødvendig kablingsplass.
2. **Behold dokumentert gode festeløsninger.** Skruehullene har fungert gjennom hele prosessen. Endre dem bare dersom nye komponenter krever det.
3. **Kontroller sensor–hjul-avstanden.** Ta hensyn til erfaringene fra U-svingene i V1, og test på skarpe svinger før en ny geometri regnes som ferdig.
4. **Utvikle og test nye hjul.** Mål både drivhjulene og casterhjulet, og undersøk om ny hjuldiameter eller endret feste kan gi mer vannrett chassis. Husk at hjulstørrelse påvirker utvekslingen mellom rotasjon og kjørelengde.
5. **Stiv av motorarmene ved behov.** Belast konstruksjonen med reelle komponenter og se om hjulene holder riktig høyde og vinkel.
6. **Behold modulære deler.** La sensorfeste, batterifeste og eventuell elektronikkplate kunne endres uten å printe hele chassiset på nytt.
7. **Kontroller tilgang til komponentene.** Batteribytte, kabling og skruefester må fortsatt være tilgjengelige etter montering.

### 13.2 Enkel testlogg for fremtidige prototyper

| Testkategori | Hva bør registreres? | Hvorfor? |
| --- | --- | --- |
| Versjon og dato | Modellnavn, dato, hovedendring | Gjør utviklingshistorikken etterprøvbar |
| Utskrift | Materiale, laghøyde, printretning, eventuell støtte | Kan forklare forskjeller i passform og stivhet |
| Passform | Arduino/kontroller, sensor, batterier, motorer, caster | Viser om delene kan monteres som planlagt |
| Skruehull | Passer skruer og festepunkter? | Bekrefter at en velfungerende løsning videreføres |
| Geometri | Sensoravstand, akselplassering, hjulhøyde, chassishelning | Gir grunnlag for sammenlignbare kjøretester |
| Belastning | Synlig bøying, ustabilitet, løse deler | Skiller strukturelle svakheter fra styringsfeil |
| Kjøretest | Rettstrekning, vanlige svinger, skarpe svinger, U-svinger | Viser samspillet mellom mekanikk og regulering |
| Resultat | Godkjent, må endres eller ikke testet | Hindrer at antakelser blir presentert som bekreftede funn |

### 13.3 Avgrensninger i denne dokumentasjonen

Dokumentasjonen inneholder ikke oppdiktede mål for vekt, chassisdimensjoner, materialtype, printinnstillinger, sensorhøyde eller toppfart. Slike opplysninger kan legges inn senere dersom de blir målt eller funnet i prosjektets faktiske utskrifts- og testlogger. Den er ment å kunne bygges videre på når flere detaljer blir tilgjengelige.

## 14. Modellfiler og kilder

De syv hovedversjonene er tilgjengelige som `.3mf`-filer i prosjektets GitHub-repositorium:

| Versjon | Lokalt filnavn i datooversikten | Modell på GitHub |
| --- | --- | --- |
| V1 | `LF v1.3mf` | [`LF.v1.3mf`](https://github.com/TonzN/linefollower/blob/fase2_esp32/Line_follower_prototypes/LF.v1.3mf) |
| V2 | `LF V2.3mf` | [`LF v2.3mf`](https://github.com/TonzN/linefollower/blob/fase2_esp32/Line_follower_prototypes/LF%20v2.3mf) |
| V3 | `LF v3.3mf` | [`LF v3.3mf`](https://github.com/TonzN/linefollower/blob/fase2_esp32/Line_follower_prototypes/LF%20v3.3mf) |
| V4 | `LF v4.3mf` | [`LF v4.3mf`](https://github.com/TonzN/linefollower/blob/fase2_esp32/Line_follower_prototypes/LF%20v4.3mf) |
| V5 | `LF V5.3mf` | [`LF v5.3mf`](https://github.com/TonzN/linefollower/blob/fase2_esp32/Line_follower_prototypes/LF%20v5.3mf) |
| V6 | `LF v6.3mf` | [`Robot v6.3mf`](https://github.com/TonzN/linefollower/blob/fase2_esp32/Line_follower_prototypes/Robot%20v6.3mf) |
| V7 | `LF V7.3mf` | [`Robot v7.3mf`](https://github.com/TonzN/linefollower/blob/fase2_esp32/Line_follower_prototypes/Robot%20v7.3mf) |

**Andre modellfiler:** Skjermbildet viser også `front wing.3mf` (02.10.2026 kl. 13:29) og `front wing 2.3mf` (30.10.2026 kl. 13:29). Disse regnes som separate delfiler/sideprosjekter og ikke som egne hovedversjoner av chassiset. GitHub-arkivet inneholder også ekstra `.3mf`-filer, men identisk innhold mellom lokale og publiserte delfiler er ikke kontrollert her.

**Kildegrunnlag:**

- [Prosjektets GitHub-repositorium – `fase2_esp32`](https://github.com/TonzN/linefollower/tree/fase2_esp32)
- [Mappen med 3D-prototyper](https://github.com/TonzN/linefollower/tree/fase2_esp32/Line_follower_prototypes)
- [Teknisk dokumentasjon for den senere ESP32-løsningen](https://github.com/TonzN/linefollower/blob/fase2_esp32/docs/LINJEFOLGEREN.md) – oppført for å skille nåværende prosjektretning fra de historiske chassisversjonene
- Konstruktørens egne beskrivelser av V1–V7, komponentlisten for fase 1, erfaringer fra montering og kjøring, og oversikt over lokale fildatoer
- Fotografier av de utskrevne prototypene V1–V7, inkludert i dokumentasjonspakken

---

## Konklusjon

De syv prototypene viser en tydelig utvikling fra en stor, enkel testmodell til et kompakt og gjennomarbeidet chassis. De første versjonene ga erfaring med sensorplassering, batteribalanse og passform. V4 og V5 forbedret montering, modularitet og helhetlig utforming, og **V5 ble den endelige modellen i fase 1**. I fase 2 gjorde nye elektronikkrav det nødvendig å videreutvikle konstruksjonen. V6 demonstrerte behovet for stivere motorarmer, mens V7 prøvde å løse dette og samtidig skape mer plass til elektronikk.

Den viktigste konkrete erfaringen fra kjøringen var **at sensorarmen i V1 var for lang**, noe som særlig skapte problemer i U-svinger. De øvrige oppgitte problemene var i hovedsak knyttet til design, montering og belastning, ikke dokumenterte kjøreproblemer. Arbeidet viser dermed verdien av å teste fysiske prototyper, forbedre én svakhet av gangen og ta vare på løsninger som allerede fungerer – som de konsekvent velfungerende skruehullene.
