# Hardware Reference

Source of truth for wiring: `docs/naujas_dizainas_geroji_versija_laidu_sujungimai.xlsx`
(sheets: Sandėliuko/Ūkinio skydelis, Drabužinės skydelis, Pirtis).

## Buildings and controllers

| Building topic | Panel (xlsx sheet) | Controller |
|---|---|---|
| `utility` | Ūkinio skydelis | YD-RP2040 + W5100 shield (SD on shield) |
| `closet`  | Drabužinės skydelis | YD-RP2040 + W5100 shield (SD on shield) |
| `sauna`   | Pirties skydelis | YD-RP2040 + W5100 shield (SD on shield) |

W5100 first (hardware-proven with PicoHomeOld); W5500 lite + microSD breakout is the
fallback if the W5100 fights pico-sdk.

## Room name mapping (LT → topic segment)

| Lithuanian | Topic segment | Building |
|---|---|---|
| Virtuvė | `kitchen` | utility |
| Svetainė | `living_room` | utility |
| Koridorius (vidaus) | `corridor` | utility / closet |
| Lauko koridorius | `hallway` | utility |
| Vaikų tualetas | `kids_toilet` | utility |
| Ainiaus kambarys | `ainius_room` | utility |
| Pirties stoginė | `sauna_canopy` | utility |
| Tėvų tualetas | `parents_toilet` | closet |
| Drabužinė | `closet_room` | closet |
| Tėvų miegamasis | `parents_bedroom` | closet |
| Smiltės kambarys | `smilte_room` | closet |
| Darbo kambarys | `office` | closet |
| Pirtis | `sauna` | sauna |
| Dušas | `shower` | sauna |
| Kambarys (pirtyje) | `rest_room` | sauna |

Adjust freely before the first config file is written; after that, renames are breaking changes for HA.

## Device inventory (from wiring + old code)

**Plan-number suffixes** (per user, 2026-07-17): **D** = dimmable set (light + dimming-gesture
switch) · **P** = multiple switch locations control the same light/group · **J** = motion-activated
(implemented as a generic config binding: any motion sensor → any light) · **E** = external light
on an outlet, logic same as plain light · **PJ** = P + J.

**Actuators**
- Light relays (plan numbers 20–106, 201–203 + suffixed variants per above)
- Dimmable lights, AC phase-cut: PSM + Z-C wire pairs — 22D, 28D, 40D, 52D, 80D, 90/91D → see [Dimmer options](#dimmer-options)
- Dimmable lights, 1–10 V: 32D, 51D, 61D → see [Dimmer options](#dimmer-options)
- Blinds (žaliuzės): 2-relay pairs (x.1/x.2), height + tilt timing — utility & closet, several per building
- Curtains (užuolaidos): 2-relay pairs, travel only
- Controlled outlets (kontroliuojama rozetė): several rooms
- Air dampers (oro sklendė): 2 relays each — ventilation
- Heating mat relays (šildymo kilimėlis, 401)
- RGB LED strips — WS2815 (12 V, backup data line), Ainiaus/Smiltės/parents' bedroom
- Commercial RS485 Modbus RTU relay module(s)
- (future) Watering solenoids ×3 — separate controller, out of scope

**Inputs / sensors on panels**
- Wall switches: momentary + toggle (gesture grammar: 1-press, 2-press, long, long-long)
- Reed door/window sensors (durų/langų sensorius)
- Water leak sensors (vandens sensorius) — digital high/low
- PIR sensors (HC-SR501-class, 3.3 V OUT — OK direct)
- Presence sensors HLK-LD2410C (5 V supply, OUT pin is 3.3 V logic — OK direct)
- Outdoor PIR (closet building)
- ~~Distance sensor (atstumo sensorius, kids' toilet)~~ — dropped, no longer needed (2026-07-17)

**On room nodes (kompiuteriukas, RS485)**
- I2C: temperature, humidity, CO2
- Analog: smoke detector (5 V out → divider), photoresistor (run divider at 3.3 V)
- Wiring confirms analog smoke/light signals terminate at room nodes, not panels

## Proposed plan-number ranges for unnumbered devices (TBC by user)

Existing: 20–106 + 201–203 lights, 401 heating mat. Proposal — user adds chosen numbers to the xlsx:

| Range | Category |
|---|---|
| 500–549 | blinds |
| 550–599 | curtains |
| 600–649 | controlled outlets |
| 650–699 | air dampers |
| 700–709 | reed door/window |
| 710–719 | water leak |
| 720–729 | PIR |
| 730–739 | presence (LD2410C) |
| 740–749 | smoke |
| 750–759 | light level |
| 760–769 | temperature |
| 770–779 | humidity |
| 780–789 | CO2 |
| 790–799 | reserved |
| 850–859 | RGB LED (WS2815) |

Ranges confirmed by user 2026-07-17. Concrete per-device assignments: `device_numbering.md`.

Modbus slave addresses are independent of these: 1–9 relay modules, 10–99 room nodes.

## Dimmer options

**AC phase-cut dimmers: OPEN — user deciding.**
Requirements: small, wired preferred (wireless acceptable), Home Assistant integration.

| Option | Comm | Size/mount | HA integration | Notes |
|---|---|---|---|---|
| **RS485 Modbus AC dimmer module** (e.g. Krida 2/4/8-ch) | Wired RS485 — plugs into the per-building Modbus bus that already exists | Boxed module in/near panel | Via our controller (`dimmable_light` Modbus backend) | Best architectural fit: reuses PSM/Z-C mains wiring point at panel, one bus, no cloud, no radio. Firmware drives it like the relay module. |
| **Shelly Pro Dimmer 1PM/2PM** | Ethernet (LAN) + WiFi | DIN rail — panels are DIN territory | Native (Shelly integration or MQTT) | Wired, polished, per-channel power metering; ~2 channels/unit so cost adds up across ~6 dimmed circuits/building. Bypasses our firmware entirely — wall switch must wire to Shelly SW input or be forwarded via HA. |
| **Shelly Dimmer 2 / Gen3 (mini, in-wall)** | WiFi only | Behind the wall switch | Native | Smallest and cheapest per point; wireless-only, distributed (not in panel), needs decent WiFi at switch boxes. |
| **Zigbee in-wall dimmer** (Moes/Sunricher) | Zigbee | Behind switch | Via Zigbee2MQTT | Cheap, but adds a whole radio network + coordinator to maintain. |
| **DIY: RP2040-Zero + RobotDyn dimmer as Modbus slave** | Wired RS485 | In panel | Via our controller | Cheapest wired option; reuses the room-node firmware pattern (just another register map). More build/maintenance effort, mains proximity. |

**1–10 V circuits (32D, 51D, 61D): DECIDED** — RP2040 hardware PWM → user's existing
PWM-to-0-10V converter modules; the plan's relay handles on/off. No DAC needed.

## Electrical / level-shifting rules

- RP2040 GPIO is **not 5 V tolerant**.
- MCP23017s run at **3.3 V** (spec 1.8–5.5 V): switch inputs pull up to 3.3 V, switches short
  to GND — no shifting needed even on long runs. Relay boards driven at 3.3 V logic: verify
  each board triggers reliably; prefer low-trigger opto boards, add transistor stage if not.
- The 5 V rail in the wiring powers sensors (PIR, LD2410C, smoke). Check each *output*:
  HC-SR501 OUT = 3.3 V ✓, LD2410C OUT = 3.3 V ✓, analog smoke out swings to 5 V → resistor
  divider before any ADC.
- I2C stays panel-local (MCP23017, PCA9548A, DAC); anything leaving the panel goes RS485.
- RS485 transceivers must be 3.3 V-logic (MAX3485-class); classic 5 V MAX485 modules only
  with proven 3.3 V thresholds or a shifter.
- RGB strips are **WS2815**: 12 V supply rail required; data logic still wants 5 V →
  one 74HCT125 buffer per strip; unused BI (backup data) on the first pixel tied per datasheet.

## BOM additions (beyond current parts list)

| Part | Qty | Why |
|---|---|---|
| Raspberry Pi Debug Probe (or Pico as picoprobe) | 1 | SWD debugging from CLion |
| RP2040-Zero (or similar) | per room node | Room co-processors |
| 3.3 V RS485 transceiver modules | 1/building + 1/node | Modbus bus |
| 74HCT125 | per WS2815 strip | 3.3→5 V data buffer |
| 12 V PSU per strip location | per WS2815 strip | WS2815 supply |
| Resistor dividers / assorted resistors | — | 5 V analog → 3.3 V ADC |
| 5 V→3.3 V buck/LDO per panel & node | — | Clean 3.3 V rails |
| Chosen AC dimmer hardware | TBD | See dimmer options |
| (fallback only) W5500 lite + SPI microSD breakout | 3 | If W5100 doesn't work out on pico-sdk |

Already owned, no purchase needed: W5100 shields, PWM→0-10V converter modules,
MCP23017, PCA9548A, relays, Modbus relay module, sensors.
| Chosen dimmer hardware | TBD | See dimmer options |
