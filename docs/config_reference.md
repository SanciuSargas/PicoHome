# Config Reference

Every option the SD-card config accepts, as the parser actually reads it
(`core/src/config_parser.cpp`). One file per building, on the card root as
`/config.json`; the firmware binary is identical in all three buildings.

**Comments (`//`) are allowed** — the parser enables them deliberately. Editors will
flag them as invalid JSON; ignore that.

**Anything unrecognised is refused at boot**, with the reason on USB serial and, once
MQTT is up, on `<building>/controller/status/0/error`. A controller running a
half-understood config could energize the wrong circuits, so there is no "ignore and
carry on" path.

Worked example: [`config/testbench.json`](../config/testbench.json).

---

## Top level

| Key | Required | Default | Notes |
|---|---|---|---|
| `schema_version` | yes | — | Must be `2`. Anything else is refused. |
| `building` | yes | — | First MQTT topic segment: `utility`, `closet`, `sauna`. |
| `network` | no | — | Omit only if the controller will never reach a broker. |
| `devices` | yes | — | The inventory: what physically exists. |
| `bindings` | yes | — | The behaviour: what controls what. May be `[]`. |

The two `devices`/`bindings` halves are independent by design: **no device ever refers to
another device.** A button does not know its light; the binding knows both.

## `network`

| Key | Default | Notes |
|---|---|---|
| `mac` | — | Array of exactly 6 numbers. Must be unique per controller. |
| `ip` | `""` | Static IPv4, dotted quad. No DHCP. |
| `netmask` | `""` | |
| `gateway` | `""` | |
| `broker` | `""` | Broker IPv4 — **an address, not a hostname** (no DNS). |
| `broker_port` | `1883` | |

---

## The pin object

Used by every pin field in the file, so learn it once. Where an actuator declares
`backend`, these keys sit **inside `backend`**; sensors and buttons put them in `pin`.

| Key | Default | Meaning |
|---|---|---|
| `pin` | `0` | GPIO number, or `0..15` on an expander (`0-7` = PA0–PA7, `8-15` = PB0–PB7). |
| `expander` | `-1` | `-1` = controller GPIO. `0..7` = the `index` of a declared expander. |
| `active_high` | `true` | **Outputs:** the level that energizes. **Inputs:** the level meaning pressed/triggered. |
| `pull_up` | `true` | Inputs only. See the asymmetry below. |

**`active_high`** is the only place in the firmware that knows about electrical polarity;
everything above it deals in logical values. A low-trigger relay board (most cheap
opto-isolated ones) wants `false`. A switch wired to GND wants `false` on the input side.

**`pull_up` differs between backends** — the one genuine trap here:

- **Controller GPIO:** `false` enables a pull-*down*, so the pin is still defined.
- **MCP23017:** the chip has pull-ups only, so `false` leaves the pin **floating** and it
  will read noise. A switch to GND on an expander always wants `pull_up: true`.

---

## `devices.expanders`

MCP23017 port expanders on the panel I2C bus (bus pins are compile-time, in
`controller/src/board_config.h`). Omit the list entirely if there are none.

| Key | Required | Notes |
|---|---|---|
| `index` | yes | `0..7`. What a pin's `expander` field refers to. Written out explicitly so reordering the list cannot silently remap every pin in a building. |
| `address` | yes | `0x20`–`0x27`, as a string (`"0x20"`) or a number (`32`). Set by the chip's A0/A1/A2 pins. |
| `int_pin` | yes | Controller GPIO the chip's INT line goes to. Active low; the controller enables its own pull-up. `IOCON.MIRROR` is set, so one wire covers all 16 pins. |

Duplicate `index` or duplicate `address` are refused.

## `devices.actuators`

| Key | Required | Default | Notes |
|---|---|---|---|
| `type` | yes | `"light"` | See the table below. |
| `id` | yes | — | Electrical-plan number. Non-zero, unique per type per building. |
| `room` | yes | — | Second MQTT topic segment. |
| `backend` | yes | — | How it is driven; see below. |
| `options` | cover only | — | Cover timing. |

### Actuator types

| `type` | Works today | Local bindings may drive it |
|---|---|---|
| `light` | yes | `toggle`, `motion` |
| `outlet` | yes | `toggle` |
| `damper` | yes | `toggle` |
| `heating_mat` | yes | no — MQTT only, logic belongs in Home Assistant |
| `rgb` | no (M11) | — |
| `blinds` | yes, with `backend.kind: "cover"` | `cover` |
| `curtains` | **no** — deferred until the hardware is decided | — |
| `dimmable_light` | no (M10) | — |

Types with no local binding are still fully controllable and observable over MQTT — that
is the universal rule. They just have no rule that must survive a dead broker.

### `backend`

| `kind` | Status | Extra keys |
|---|---|---|
| `"gpio"` | works | the pin fields, inline (`pin`, `active_high`) |
| `"mcp"` | works | the pin fields, inline, including `expander` |
| `"cover"` | works, `blinds` only | `up` and `down`, each a pin object |
| `"pwm"` | refused — dimming is M10 | |
| `"modbus"` | refused — Modbus is M8 | |

`kind: "mcp"` and a pin's `expander` must agree: an `mcp` backend needs an expander, a
`gpio` backend must not have one. A cover's `up`/`down` each carry their own `expander`,
so the two relays may sit on different backends.

```jsonc
{"type": "light", "id": 29, "room": "kitchen",
 "backend": {"kind": "gpio", "pin": 2, "active_high": false}},

{"type": "light", "id": 30, "room": "corridor",
 "backend": {"kind": "mcp", "expander": 0, "pin": 1}},

{"type": "blinds", "id": 500, "room": "kitchen",
 "backend": {"kind": "cover",
             "up":   {"pin": 6},
             "down": {"expander": 0, "pin": 3, "active_high": false}},
 "options": {"travel_ms": 82000, "tilt_ms": 1500}}
```

### Cover `options`

There is no position feedback, so **these timings *are* the position scale** — measure
them with a stopwatch on the real blind, in each direction, and err slightly long so the
motor reaches its stop (blind motors are built to sit against their limits).

| Key | Required | Default | Notes |
|---|---|---|---|
| `travel_up_ms` | yes\* | `travel_ms` | Full closed → open. |
| `travel_down_ms` | yes\* | `travel_ms` | Full open → closed. A blind falls faster than it climbs — the old Arduino measurements were 82 s up against 71 s down — so the two are timed separately. |
| `travel_ms` | — | — | \*Shorthand setting both directions at once, for a cover where the difference does not matter. Either it, or both of the pair above, must be non-zero. |
| `tilt_ms` | no | `0` | Full slat sweep, and must be shorter than the shorter travel. `0` = no tilt. Exterior blinds sweep 0–90° (far end = open); interior ones 0–180° (open halfway, closed again at the end). The sweep is short enough that its own up/down asymmetry is not worth configuring. |
| `step_ms` | no | `500` | How far a single press nudges. Because the angle moves first, this nudges the slats. |
| `reverse_delay_ms` | no | `500` | Motor rest before driving again, measured from when the relays actually dropped out. |
| `boot_position` | no | `"open"` | `"open"` or `"closed"` — what to assume at power-up. |
| `home_at_boot` | no | `false` | Stop assuming: start from "closed" and drive to the top, which takes a full travel from anywhere and ends against the stop with a position that is *measured*. Costs one traverse per power-up, and makes `boot_position` irrelevant. |

## `devices.sensors`

Declaring a sensor makes it publish retained state for Home Assistant. Only `pir` and
`presence` may additionally drive a local binding.

| Key | Required | Default | Notes |
|---|---|---|---|
| `type` | yes | — | `pir`, `presence`, `reed`, `water_leak`, `smoke`, `light_level`, `temperature`, `humidity`, `co2` |
| `id` | yes | — | Non-zero. |
| `room` | yes | — | |
| `pin` | yes | — | A pin object. |
| `debounce_ms` | no | `50` | |

Binary sensor device classes land with M7; the analog and Modbus-fed ones (`smoke`,
`light_level`, `temperature`, `humidity`, `co2`) arrive over RS485 with M8/M9.

## `devices.buttons`

Wall switches. A button carries the plan number of the device it primarily controls, and
several physical switches of one P-group share an id — they are one logical button in
several places.

| Key | Required | Default | Notes |
|---|---|---|---|
| `id` | yes | — | Non-zero. |
| `room` | yes | — | |
| `kind` | **yes** | none | `"momentary"` (spring-return pushbutton) or `"toggle"` (latching rocker). |
| `gestures` | no | `false` | `true` decodes the full grammar. |
| `pin` | yes | — | A pin object. |

**`kind` is deliberately not defaulted.** Reading a toggle switch as momentary never
completes a press, so the light simply never responds — a silent failure on a live panel,
and the house has both kinds fitted.

**`gestures` is the responsiveness knob:**

| | `false` (default) | `true` |
|---|---|---|
| acts on | the actuating edge — the press itself, or either flip of a rocker | the decoded gesture |
| latency | debounce only, ~30 ms | ~230–330 ms |
| produces | `press_1` only | `press_1/2/3`, and on a momentary switch `hold_start`/`hold_long`/`hold_end` |

The grammar cannot act on a press until it knows a second one is not coming, so every
single press pays the multi-press window as latency. A plain light switch never uses
multi-press, so that wait buys it nothing. Turn `gestures` on where the grammar is
actually used — covers, dimmers, or anything wanting `press_2` locally *or* from Home
Assistant.

A latching rocker never produces hold gestures whatever `gestures` says: you cannot hold
a switch that stays where you put it.

---

## `bindings`

Each entry wires **any number of sources** to **any number of targets** through one rule.
Many→many is the normal case, not a special case.

| Key | Required | Default | Notes |
|---|---|---|---|
| `name` | no | `""` | Only used to make boot errors readable. Worth setting. |
| `rule` | yes | — | `toggle`, `motion` or `cover`. |
| `sources` | yes | — | At least one. |
| `targets` | yes | — | At least one, each `{"type": …, "id": …}`. |
| `options` | motion | — | `off_delay_ms`. |

### Rules

| rule | sources | targets | effect |
|---|---|---|---|
| `toggle` | button, remote button event | light, outlet, damper | flips every target |
| `motion` | pir/presence sensors, remote sensor | light | on while occupied, off `off_delay_ms` after the last source goes quiet, gated per light by `night_mode` |
| `cover` | button, remote | blinds | the full gesture grammar (below) |

Anything richer — scenes, reed/smoke-triggered logic, schedules, anything involving a
measurement — belongs in Home Assistant and is refused here by name. Local bindings exist
for what must survive a dead broker, and nothing more.

### Source kinds

```jsonc
{"button": 29}                                    // gesture defaults to press_1
{"button": 29, "gesture": "press_2"}              // press_1|press_2|press_3|hold_start|hold_long|hold_end
{"sensor": {"type": "pir", "id": 720}}            // motion rule only; pir or presence only
{"topic": "closet/corridor/button/31/event", "payload": "press_1"}   // remote button event
{"topic": "closet/corridor/pir/720/state"}        // remote sensor, motion rule
```

| Key | Applies to | Default | Notes |
|---|---|---|---|
| `gesture` | button | `"press_1"` | Needs `gestures: true` for anything past `press_1`; hold gestures additionally need `kind: "momentary"`. |
| `payload` | remote, non-motion | — | Required. Exact match. |
| `payload_active` | remote, motion | `"1"` | |
| `payload_inactive` | remote, motion | `"0"` | Must differ from `payload_active`. |

The defaults for a remote sensor are the `1`/`0` this firmware publishes on its own state
topics, so binding a PicoHome sensor on another panel needs no payloads at all. Foreign
devices (Zigbee2MQTT `true`/`false`, …) spell theirs out.

The set of remote sources determines the controller's extra MQTT subscriptions. A remote
*sensor* needs both edges because the off-delay has to key on something; a remote *button
event* is a momentary action and takes a single payload.

### The cover gesture grammar

The `cover` rule ignores the `gesture` field and consumes the whole grammar from one
switch, so it requires `kind: "momentary"` **and** `gestures: true`:

| gesture | effect |
|---|---|
| press | stop if moving, otherwise nudge (which moves the slats, since the angle goes first) |
| press ×2 | reverse |
| hold | move while held |
| hold 3 s | commit: run to the end even after letting go |

### Binding `options`

| Key | Applies to | Notes |
|---|---|---|
| `off_delay_ms` | `motion` | Required and non-zero. How long after the last sensor goes quiet the light switches off. |
| `lux_below` | — | **Removed.** Use each light's `night_mode`, which Home Assistant sets over MQTT; the controller owns the gate, high level decides what "night" means. |

---

## What gets refused at boot

Not an exhaustive list, but the checks worth knowing about:

- `schema_version` other than 2; missing `building`; malformed JSON
- an actuator/sensor/button without `id` or `room`; an unknown `type`
- a button without `kind`
- a pin naming an `expander` that is not declared, or an expander pin above 15
- `backend: "mcp"` without an expander, or `"gpio"` with one
- duplicate expander `index` or `address`
- a cover with no travel time set, or with `tilt_ms` >= the shorter of the two travels
- `curtains` anywhere; `pwm`/`modbus` backends; rules other than toggle/motion/cover
- a binding with no sources or no targets
- a binding whose target does not exist, or is the wrong class for the rule
- a binding whose source button or sensor does not exist
- a `motion` binding without `off_delay_ms`, or with a button as a source
- a sensor source that is not `pir`/`presence`, or used by a rule other than `motion`
- **a binding asking a switch for a gesture it cannot produce** — the error names the fix:
  `set "gestures": true`, or "a latching rocker cannot be held" when no configuration
  would help

## MQTT surface per device type

For reference while writing bindings — the full scheme is in `CLAUDE.md`.

| type | subscribes | publishes (retained unless noted) |
|---|---|---|
| `light` | `set_binary`, `set_night_mode` | `state`, `night_mode` |
| `outlet`, `damper`, `heating_mat` | `set_binary` | `state` |
| `blinds` | `set_position`, `set_tilt`, `stop` | `state` = **position 0–100**, `0` fully down/closed and `100` fully up/open; `tilt` = slat angle 0–100 |
| sensors | — | `state` (binary) or `measurement` (numeric) |
| `button` | — | `event` — not retained; a gesture is a moment, not a state |
| controller | — | `status/0/alive` (LWT), `status/0/error` (empty = nothing wrong) |