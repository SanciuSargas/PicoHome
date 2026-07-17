# PicoHome

Home automation firmware for RP2040 (YD-RP2040) building controllers — three buildings,
each driven by one controller configured from a JSON file on SD card, exposed over MQTT
(Home Assistant on top). Room co-processor nodes report over RS485/Modbus RTU.

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

### Later / background
- [ ] Watering system scope (future 4th controller — cellar + 2 underground points)
