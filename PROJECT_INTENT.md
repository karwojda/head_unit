# Project: head_unit

## Problem

Learning project first: the owner wants hands-on experience with Zephyr
RTOS and the grvl GUI library on a real, useful device. The device is a
helm-mounted display for dinghies and small boats in inshore racing --
the same use case as the Vakaros Atlas 2 -- showing data from wireless
sensors on the boat (the windmeter project's wind sensor first, others
such as rigging/trim tension sensors later).

## Approach

A Zephyr-based display unit with a grvl UI that receives sensor data
over a standard wireless link, so any sensor speaking that standard can
pair with it -- not a link private to windmeter. Developed emulation-first
(Renode, and grvl's SDL host build) before committing to hardware.

## Success Looks Like

- Working fluency with Zephyr and grvl: the owner can build, extend and
  debug the app without outside help.
- Live wind data from windmeter shown on the display, received
  wirelessly, first in Renode and then on real hardware.
- The display reads any sensor that speaks the chosen standard, not only
  windmeter.
- BOM cost tracked against a target of 1/10 of the Vakaros Atlas 2's
  retail price (~$1,249 / ~EUR 958 as of 2026-10, so roughly
  $125 / EUR 96) -- a competitiveness signal for possible mass
  production, explicitly not the main goal.

## Boundaries

- NOT: matching Vakaros feature-for-feature. It's the reference for use
  case and form factor, not a spec to clone.
- NOT: a private wireless protocol between head_unit and windmeter. The
  link has to be a standard other sensors can use.
- NOT: chartplotting/maps, or writing settings back to sensors -- the
  display is read-only.
- Not yet decided: which wireless standard (BLE, a WiFi gateway, ...),
  the hardware platform/display, and whether head_unit carries its own
  GNSS/IMU like Vakaros does.

## Tier

Exploratory (learning Zephyr and grvl is the main goal).
Graduates to Operational if: it gets used on the water during races, or
the mass-production/BOM direction is pursued for real.
Timebox: 2 months -- decide by 2026-12-05 whether to graduate, continue
as exploratory with a new timebox, or stop.

## Issue Tracker

Not yet. Work ledger is local -- tracked in each spec's `tasks.md` once
specs exist.

## Related Projects

- windmeter (`../windmeter`) -- the first sensor head_unit displays.
  windmeter currently outputs NMEA2000 only (wired), so it will need a
  wireless output in the chosen standard; that change belongs to
  windmeter, not here. Its hardware verification is still pending, so
  on-water use of head_unit waits for windmeter; emulation work doesn't.

## Spec Seeds

| Feature | Seed | Status |
| ------- | ---- | ------ |
| live-wind-display | specs/live-wind-display/seed.md | seed |

## History

- 2026-10-05: Created via `/ws.init`. Split out of windmeter as a
  separate project because the link between them is a standard, not a
  shared contract; graduates windmeter's parked "wireless display unit"
  idea.
