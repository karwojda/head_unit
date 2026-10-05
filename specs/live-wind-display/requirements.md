# Feature: live-wind-display

## Decisions

- [x] **BLE link.** Resolved: head_unit connects to one *remembered* sensor and subscribes to its Environmental Sensing wind values; it never connects to or shows any other sensor, even one in range.
- [x] **How the sensor is remembered this cycle.** Resolved: its address is set at build time; an on-screen pairing list comes later.
- [x] **Stale data.** Resolved: values are blanked ("--") with a "sensor lost" indicator once no update has arrived within the timeout; a stale number is never shown as live.
- [x] **Done means.** Resolved: the whole chain works in emulation (Renode: emulated sensor -> BLE -> head_unit screen) under automated tests. Real hardware and the parts-cost check are the next feature.
- [x] **Screen content.** Resolved: apparent wind angle as 0-180 deg port/starboard, true wind direction as a 0-359 deg compass bearing, both speeds in knots to 0.1, plus a dial showing apparent wind angle.

---

## Context

### User story

As a dinghy sailor racing inshore, I want the helm display to show live wind from my own boat's wireless sensor, so I can read apparent and true wind at a glance.

"Live" means: received from the remembered sensor within the stale timeout of 3 s (three missed updates at the ~1 Hz rate windmeter sends).

### Acceptance criteria

1. Given the remembered sensor is in range and sending apparent and true wind, When head_unit starts, Then it connects and shows those values within 5 s, and each new value replaces the shown one within 1 s of arriving.
2. Given another wind sensor is also in range (even with a stronger signal), When head_unit starts or reconnects, Then it neither connects to it nor shows its values.
3. Given values are being shown, When no update arrives within the timeout, Then all wind values show "--" and the sensor-lost indicator is on; When updates resume, Then values reappear and the indicator clears.
4. Given the connection drops, When the remembered sensor becomes available again, Then head_unit reconnects without user action.
5. Given the sensor sends apparent wind but no true wind, Then true wind fields show "--", not 0.
6. Given received values, Then they display converted and rounded as decided (e.g. 5.14 m/s -> 10.0 kn; apparent 359.6 deg -> 0 deg, never 360; apparent 270 deg -> 90 deg port).

### Out of scope

- Real hardware bring-up and the BOM cost target (next feature).
- Several sensors at once, sensor settings, logging, GNSS/heading on head_unit, race timer.
- BLE encryption/bonding: the sensor is recognised by address only.
- windmeter's BLE output -- belongs to the windmeter project; this cycle uses an emulated sensor.

### Open questions

- [ASSUMPTION: the emulated sensor is firmware in this project sending scripted Environmental Sensing values, e.g. replaying windmeter's `simulate` scenarios.]
- [ASSUMPTION: the spike's board setup -- STM32H747I-DISCO, 800x480 display, nRF52840 as BLE radio over HCI-UART.]
