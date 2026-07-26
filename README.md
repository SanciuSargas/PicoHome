# PicoHome

Home automation firmware for RP2040 (YD-RP2040) building controllers — three buildings,
each driven by one controller configured from a JSON file on SD card, exposed over MQTT
(Home Assistant on top). Room co-processor nodes report over RS485/Modbus RTU.

- Design rationale (the concepts behind every decision): [`docs/design_rationale.md`](docs/design_rationale.md)
- Hardware reference, BOM, dimmer options: [`docs/hardware.md`](docs/hardware.md)
- Device number assignments: [`docs/device_numbering.md`](docs/device_numbering.md)
- Wiring source of truth: `docs/naujas_dizainas_geroji_versija_laidu_sujungimai.xlsx`

## Development setup

```bash
# One-time (Fedora): native + cross compilers
sudo dnf install gcc-c++ arm-none-eabi-gcc-cs arm-none-eabi-gcc-cs-c++ arm-none-eabi-newlib

# After cloning:
git submodule update --init            # vendored libraries in external/
# pico-sdk 2.3.0 is expected at ~/pico-sdk (see CMakePresets.json)

# Host unit tests
cmake --preset host && cmake --build --preset host && ctest --preset host

# Firmware (controller.uf2 + roomnode.uf2)
cmake --preset pico && cmake --build --preset pico
```

In CLion: open the project, pick the `host` or `pico` CMake preset in
Settings → Build, Execution, Deployment → CMake.

## Build and flash the controller

```bash
# Configure once per clone (or after changing CMakeLists/presets)
cmake --preset pico

# Build just the building controller -> build/pico/controller/controller.uf2
cmake --build --preset pico --target controller
```

Flash it — hold **BOOTSEL** while plugging the board in, then either:

```bash
# a) drag-and-drop equivalent: copy the UF2 to the mounted RPI-RP2 drive
cp build/pico/controller/controller.uf2 /run/media/$USER/RPI-RP2/

# b) or use picotool (built by the SDK; also reboots the board afterwards)
build/pico/_deps/picotool/picotool load -x build/pico/controller/controller.uf2
```

Watch the boot log over USB serial (attach *before* pressing RST — the banner is
printed once and USB CDC output is dropped while nothing is listening):

```bash
cat /dev/ttyACM0
# PicoHome controller booting, reading /config.json
# PicoHome 'testbench' up: 2 devices, 1 bindings, 1 subscriptions
```

The SD card must hold `config.json` in its root (start from
[`config/testbench.json`](config/testbench.json)); a bad or missing config gives a fast
blink plus the reason on serial every 2 s.

## Checking MQTT from the command line

Set `network.ip` / `network.broker` in `config.json` to suit your LAN first. Then, with
`mosquitto-clients` installed:

```bash
# Watch everything this controller says (states are retained, so they show up at once)
mosquitto_sub -h 192.168.1.200 -v -t 'testbench/#'

# Switch the bench relay from "high level"
mosquitto_pub -h 192.168.1.200 -t testbench/kitchen/light/29/set_binary -m 1
mosquitto_pub -h 192.168.1.200 -t testbench/kitchen/light/29/set_binary -m 0

# Day mode: motion may no longer switch this light on (retain it, so the
# controller picks the value up again after a reboot)
mosquitto_pub -h 192.168.1.200 -r -t testbench/kitchen/light/29/set_night_mode -m 0
```

Expected traffic: `testbench/controller/status/0/alive` = `online` (and `offline` a few
seconds after you unplug the board — the broker publishes that on the controller's
behalf), `.../light/29/state` after every switch press, `.../button/29/event` = `press_1`
per press.

**The point of the milestone:** pull the Ethernet cable and the wall switch must keep
working with no change in feel; plug it back in and the current states reappear on the
broker by themselves.

## User TODO list

### Decisions
- [ ] Choose AC phase-cut dimmer hardware (22D, 28D, 40D, 52D, 80D, 90/91D) — options
      compared in [`docs/hardware.md`](docs/hardware.md#dimmer-options)
- [ ] Review room name mapping (LT → EN topic segments) in `docs/hardware.md` — renames
      after the first config file are breaking changes

### Device numbering (`docs/device_numbering.md`)
- [ ] Review all rows marked **TBC**, in particular:
  - [ ] utility: living_room damper 651 (xlsx only has a todo note, no wires)
  - [ ] utility: rooms for reed 701 and water_leak 711
  - [ ] closet: parents_bedroom bedside outlets — one or two (601/602)?
  - [ ] closet: bed-side temperature sensor wiring (761) — panel or room node?
  - [ ] which room nodes actually get CO2 sensors (780+ rows)
  - [ ] sauna: sheet looks incomplete — confirm device list, relay locations for
        lights 110–112, whether a blind (500) exists
- [ ] Confirm proposed new sauna light numbers 110 (shower), 111 (sauna), 112 (rest_room)
- [ ] Copy all confirmed numbers into the wiring Excel
- [ ] Record for **every switch** whether it is momentary (spring-return pushbutton) or
      toggle (latching rocker) — the config requires `"kind"` per button and there is no
      safe default. Devices needing hold gestures (dimmable lights, blinds, curtains, air
      dampers) want momentary ones

### Development machine
- [x] Install compilers (done 2026-07-17; host tests and firmware both build)

### Hardware
- [ ] Order parts from the BOM in `docs/hardware.md`: Debug Probe, RP2040-Zero boards,
      3.3 V RS485 transceivers, 74HCT125 buffers, 12 V PSUs for WS2815 strips,
      resistors for dividers, 5 V→3.3 V regulators
- [ ] Verify the relay boards trigger reliably from 3.3 V logic (prefer low-trigger opto
      boards; add transistor stage if not)
- [ ] Provide the exact model/datasheet of the RS485 Modbus relay module (serial params +
      register map needed for the Modbus master config)
- [ ] Confirm MQTT broker details (old code used 192.168.1.200:1883) and that
      Home Assistant can reach it
- [ ] When HA is set up (M13): the entity driving each light's `set_night_mode` must
      publish with `retain: true`, so a rebooted controller picks up the current
      day/night value from the broker instead of defaulting to night
- [ ] Get 3 SD cards: **SDHC, 4–32 GB, FAT32** (SDSC ≤2 GB is rejected by the driver,
      SDXC ≥64 GB ships exFAT — see `docs/hardware.md`)

### Later / background
- [ ] Watering system scope (future 4th controller — cellar + 2 underground points)
