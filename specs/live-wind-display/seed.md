# Seed: live-wind-display

**Status**: seed (not yet activated for SDD)
**Created**: 2026-10-05
**Source**: `/ws.init` for head_unit; graduates windmeter's parked
"wireless display unit" idea (`../windmeter/specs/_ideas.md`).

## Discovery

The first thing a sailor would use head_unit for: glance at the display
and see live apparent and true wind, sent wirelessly by a sensor
(windmeter first). It is also the smallest feature that touches
everything this project exists to learn -- Zephyr, grvl, a standard
wireless stack, and emulation-first development in Renode.

## Present Understanding

grvl is Antmicro's lightweight GUI library for Zephyr (same company as
Renode): UI defined in XML, scriptable through an embedded Duktape
JavaScript engine, PNG/JPG support, and an SDL host build, so the UI can
be developed on a PC before any hardware exists. Antmicro has also
published an open-hardware STM32H7 HDMI board aimed at grvl on Zephyr.
Renode ships an STM32H747I-DISCO script (a board with a display), a
possible emulation target; whether grvl actually renders through its
display in Renode is unverified.

Likely wireless candidate: BLE. If the Bluetooth SIG Environmental
Sensing Service really defines apparent/true wind speed and direction
characteristics (believed so, unverified), wind data is a genuinely
standard BLE payload that any compliant sensor could send. Renode can
simulate BLE between emulated nRF52840 nodes, so a "fake sensor" node
and the display node could share one emulation. windmeter (STM32H723,
no radio) would need BLE added on its side.

## Open Questions

**For spike / investigation:**

- Does a Zephyr + grvl app render to an emulated display in Renode, and
  on which board? Or is grvl's SDL host build the practical UI loop,
  with Renode used for the firmware and radio side?
- Does BLE ESS define the wind characteristics, and is there Zephyr
  support or a sample to start from?
- Can two Renode nodes (sensor + display) exchange that BLE data in one
  emulation, with the display node also driving a screen?

**For requirements / decision:**

- Which wireless standard (BLE ESS, a WiFi gateway, other)?
- Hardware platform: one MCU with radio and display interface, or an
  MCU plus a radio module? Driven partly by the BOM target.
- First screen content: apparent + true wind only, or also boat speed
  and heading (which would mean GNSS on head_unit or another sensor)?

## Impact If Ignored

No project: this is the walking skeleton every later feature (race
timer, trim-tension sensors) would build on.

## Recommended Next Step

Run `/ws.spike` on emulation feasibility -- grvl rendering under Renode,
plus a BLE wind value passing between two Renode nodes -- then
`/ws.0-start` on this seed with the spike's answers.
