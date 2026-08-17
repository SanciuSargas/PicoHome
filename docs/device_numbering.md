# Device Number Assignments

Concrete plan-style numbers for devices that had none in the wiring spreadsheet
(`naujas_dizainas_geroji_versija_laidu_sujungimai.xlsx`). Ranges per `hardware.md`.
IDs are unique **per building**. Rows marked **TBC** are my best guess from the wiring —
please correct while copying into the Excel; this file is then the config-file reference.

Wire/branch numbers in the "Spreadsheet ref" column refer to the cable numbering in the xlsx.

## utility (Ūkinio skydelis)

### New assignments

| ID | Type | Room | Spreadsheet ref | Notes |
|---|---|---|---|---|
| 500 | blinds | kitchen | wire 2.1.2 — Žaliuzių jungiklis, rėlė 1, rėlė 2 | |
| 501 | blinds | living_room | wire 5 — jungiklis 1, rėlė 1.1, 1.2 | |
| 502 | blinds | living_room | wire 5 — jungiklis 2, rėlė 2.1, 2.2 | |
| 503 | blinds | living_room | wire 6 — jungiklis 3, rėlė 3.1, 3.2 | |
| 504 | blinds | living_room | wire 6 — jungiklis 4, rėlė 4.1, 4.2 | |
| 505 | blinds | kids_toilet | wire 17.2 branch — jungiklis, rėlė 1, rėlė 2 | |
| 506 | blinds | ainius_room | wire 20.1 — jungiklis 1, rėlė 1.1, 1.2 | |
| 550 | curtains | ainius_room | wire 20.1 — Užuolaidų jungiklis 2, rėlė 2.1, 2.2 | |
| 600 | outlet | ainius_room | wire 21.2 — Kontroliuojama rozetė | |
| 650 | damper | ainius_room | 21.1.1 — Oro sklendė relė 1 + 2 | |
| 651 | damper | living_room | xlsx note "svetainej ventiliacijos reles" | **TBC** — todo note only, no wires yet |
| 700 | reed | kitchen | wire 2.1.1 — Durų/Langų sensorius | |
| 701 | reed | living_room | wire 7.2 — Durų/Langų (utility building door) | **TBC** room |
| 702 | reed | hallway | wire 12.1 — Durų/Langų (lauko koridorius) | |
| 703 | reed | kids_toilet | 18.2.1 — Durų/Langų (window) | |
| 704 | reed | kids_toilet | 18.3 — Durų sensorius (door) | |
| 705 | reed | ainius_room | wire 20.2 — Durų sensorius (door) | |
| 706 | reed | ainius_room | 21.2.1 — Langų sensorius (window) | |
| 710 | water_leak | kitchen | wire 2 — Vandens sensorius | |
| 711 | water_leak | corridor | wire 4/17.1 — Vandens sensorius | **TBC** room |
| 720 | pir | corridor | 18.1 (koridorius lubos) | |
| 721 | pir | kids_toilet | 18.2 (tualetas lubos) | |
| 722 | pir | ainius_room | wire 21.1 | |
| 723 | pir | sauna_canopy | wire 22 — PIR sensorius 1 | outdoor |
| 724 | pir | sauna_canopy | wire 22 — PIR sensorius 2 | outdoor |
| 725 | pir | sauna_canopy | wire 22 — PIR sensorius 3 | outdoor |
| 730 | presence | corridor | 18.1 — Būvio sensorius | LD2410C, OUT pin |
| 731 | presence | kids_toilet | 18.2 — Būvio sensorius | LD2410C, OUT pin |
| 732 | presence | ainius_room | wire 21.1 — Būvio sensorius | LD2410C, OUT pin |
| 740 | smoke | kids_toilet | 18.2 — Dūmų sensorius | digital, via room node |
| 741 | smoke | ainius_room | wire 21.1 — Dūmų sensorius | digital, via room node |
| 750 | light_level | corridor | 18.1 — Šviesumo sensorius | analog, via room node |
| 751 | light_level | kids_toilet | 18.2 — Šviesumo sensorius | analog, via room node |
| 752 | light_level | ainius_room | wire 21.1 — Šviesumo sensorius | analog, via room node |
| 760 | temperature | living_room | room node (Kompiuteriuko 1/2, wire 9) | |
| 761 | temperature | kids_toilet | room node + Temperatūros sensorius (17.2 branch) | |
| 762 | temperature | ainius_room | room node (wire 19) | |
| 770 | humidity | living_room | room node | |
| 771 | humidity | kids_toilet | room node | |
| 772 | humidity | ainius_room | room node | |
| 780 | co2 | living_room | room node | **TBC** which nodes carry CO2 |
| 781 | co2 | kids_toilet | room node | **TBC** |
| 782 | co2 | ainius_room | room node | **TBC** |
| 850 | rgb | ainius_room | 21.1.1 — RGB LED signalas | WS2815 |

### Existing plan numbers (reference, unchanged)

- Plain lights: 20, 29, 41, 42, 50, 92, 93, 100, 102, 104, 105, 201
- Motion (J): 21, 31, 101 (101 has 2 relay groups)
- Switch groups (P): 24, 25, 26, 202, 203; (PJ): 30
- External (E): 94, 95, 106
- 103 — 2 relay groups, one switch
- Dimmable AC (D): 22, 28, 40, 52 · Dimmable 1-10V (D): 51
- Heating mat: 401 (kids_toilet)

## closet (Drabužinės skydelis)

### New assignments

| ID | Type | Room | Spreadsheet ref | Notes |
|---|---|---|---|---|
| 500 | blinds | parents_toilet | wire 1 — jungiklis 1, rėlė 1, rėlė 2 | |
| 501 | blinds | parents_bedroom | wire 10 — jungiklis 3, rėlė 3.1, 3.2 | |
| 502 | blinds | smilte_room | 15.1 — jungiklis 1, rėlė 1.1, 1.2 | |
| 503 | blinds | office | wire 23 — jungiklis 1, rėlė 1.1, 1.2 | |
| 550 | curtains | parents_bedroom | wire 9 — jungiklis 1, rėlė 1.1, 1.2 | |
| 551 | curtains | parents_bedroom | wire 9 — jungiklis 2, rėlė 2.1, 2.2 | |
| 552 | curtains | smilte_room | 15.1 — jungiklis 2, rėlė 2.1, 2.2 | |
| 600 | outlet | closet_room | wire 4 — Kontroliuojama rozetė | |
| 601 | outlet | parents_bedroom | wire 8 (2_17) — Kontroliuojama rozetė | |
| 602 | outlet | parents_bedroom | wire 8.1 (3_2) — Kontroliuojama rozetė | **TBC** — one or two outlets? |
| 603 | outlet | smilte_room | 16.3 — Kontroliuojama rozetė | |
| 604 | outlet | office | wire 22 — Kontroliuojama rozetė (desk) | |
| 650 | damper | parents_bedroom | 12.1 — Oro sklendė relė 1 + 2 | |
| 651 | damper | smilte_room | 17.1 — Oro sklendė relė 1 + 2 | |
| 652 | damper | office | wire 22 — Oro sklendė relė 1 + 2 | |
| 700 | reed | parents_toilet | 1.1 — Durų/Langų sensorius | |
| 701 | reed | closet_room | 6.1 — Durų/Langų (toilet door side) | |
| 702 | reed | parents_bedroom | wire 13 — Durų/Langų (window) | |
| 703 | reed | smilte_room | 16.1 — Durų sensorius (door) | |
| 704 | reed | smilte_room | 17.1 — Durų/Langų (window) | |
| 705 | reed | corridor | 19.1 — Durų sensorius | |
| 706 | reed | office | 21.1.1 — Durų/Langų sensorius | |
| 710 | water_leak | parents_toilet | 1.2 — Vandens sensorius | |
| 720 | pir | parents_toilet | 2.1 | |
| 721 | pir | closet_room | wire 5 (lubos) | |
| 722 | pir | smilte_room | 16.2 | |
| 723 | pir | corridor | wire 18 (lubos) | |
| 724 | pir | office | 21.1 | |
| 725 | pir | parents_bedroom | 12.1.1 — Pir lauko | outdoor |
| 730 | presence | parents_toilet | 2.1 — Būvio sensorius | LD2410C, OUT pin |
| 731 | presence | closet_room | wire 5 — Būvio sensorius | LD2410C, OUT pin |
| 732 | presence | parents_bedroom | 12.1 — Būvio sensorius | LD2410C, OUT pin |
| 733 | presence | smilte_room | 16.2 — Būvio sensorius | LD2410C, OUT pin |
| 734 | presence | corridor | wire 18 — Būvio sensorius | LD2410C, OUT pin |
| 735 | presence | office | 21.1 — Būvio sensorius | LD2410C, OUT pin |
| 740 | smoke | parents_toilet | 2.1 — Dūmų sensorius | digital, via room node |
| 741 | smoke | closet_room | wire 5 — Dūmų sensorius | digital, via room node |
| 742 | smoke | parents_bedroom | 12.1 — Dūmų sensorius | digital, via room node |
| 743 | smoke | smilte_room | 16.2 — Dūmų sensorius | digital, via room node |
| 744 | smoke | corridor | wire 18 — Dūmų sensorius | digital, via room node |
| 745 | smoke | office | 21.1 — Dūmų sensorius | digital, via room node |
| 750 | light_level | parents_toilet | 2.1 — Šviesumo sensorius | via room node |
| 751 | light_level | closet_room | wire 5 — Šviesumo sensorius | via room node |
| 752 | light_level | parents_bedroom | 12.1 — Šviesumo sensorius | via room node |
| 753 | light_level | smilte_room | 16.2 — Šviesumo sensorius | via room node |
| 754 | light_level | corridor | wire 18 — Šviesumo sensorius | via room node |
| 755 | light_level | office | 21.1 — Šviesumo sensorius | via room node |
| 760 | temperature | parents_toilet | room node (Kompiuteriuko 1/2, wire 3) | |
| 761 | temperature | parents_bedroom | room node + Temperatūros sensorius (10.1, 3_12) | **TBC** — bed-side temp sensor wiring |
| 762 | temperature | smilte_room | room node (wire 14) | |
| 763 | temperature | office | room node (wire 20) | |
| 770–773 | humidity | same rooms as 760–763 | room nodes | |
| 780–783 | co2 | same rooms as 760–763 | room nodes | **TBC** which nodes carry CO2 |
| 850 | rgb | parents_bedroom | 12.1 — RGB LED signalas | WS2815 |
| 851 | rgb | smilte_room | 16.2.1 — RGB LED signalas | WS2815 |

### Existing plan numbers (reference, unchanged)

- Plain lights: 33, 60, 62, 70, 73, 79, 81, 82
- Switch groups (P): 71, 72
- External (E): 34, 74
- Dimmable AC (D): 80 · Dimmable 1-10V (D): 32, 61
- Heating mat: 401 (parents_toilet)
- Corridor switches for **30PJ and 31PJ** live on this panel while the relays are on the
  utility panel → cross-panel switch group via button event topics.

## sauna (Pirties skydelis)

Sheet looks incomplete (3 wires; old sauna sketch also had one blind) — **confirm device list**.

| ID | Type | Room | Spreadsheet ref | Notes |
|---|---|---|---|---|
| 110 | light | shower | wire 2 — Dušo jungiklis | **new plan number proposed**; relay location TBC (Modbus module?) |
| 111 | light | sauna | wire 2 — Pirties jungiklis | **new plan number proposed** |
| 112 | light | rest_room | wire 2.1 — Kambario jungiklis | **new plan number proposed** |
| 500 | blinds | rest_room | not in xlsx; old sketch had 1 shade | **TBC** |
| 700 | reed | sauna | 2.2 — Pirties durų kontaktas | |
| 730 | presence | sauna | wire 1 — Būvio sensorius (lubos) | LD2410C, OUT pin |
| 731 | presence | shower | 1.1 — Būvio sensorius | **TBC** location |
| 740 | smoke | sauna | wire 1 — Dūmų sensorius | digital, via room node |
| 760 | temperature | sauna | room node — "Pirties temperatūra" | high-temp-rated sensor needed |
| 770 | humidity | sauna | room node — "Pirties drėgmė" | |
| 771 | humidity | shower | room node — "Dušo drėgmė" | |