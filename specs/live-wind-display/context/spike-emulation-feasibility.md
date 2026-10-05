# Spike: Can head_unit be developed emulation-first (Zephyr + grvl + BLE in Renode)?

## Context

**Date**: 2026-10-05 | **Timebox**: 4h (used ~3h)
**Question**: (1) Does a Zephyr + grvl app render to an emulated display in
Renode, and on which board? (2) Is there a standard BLE payload for wind,
with Zephyr support? (3) Can a sensor node and a display node exchange that
data over emulated BLE in one Renode session?

## Approach

Renode 1.17.0. Ran Antmicro's prebuilt binaries first, then built our own
firmware from source with Zephyr SDK 1.0.1 (Zephyr 4.4 development branch,
as pinned by Antmicro's grvl demo) and ran that too. Screens were captured
headless with the LTDC model's `TakeScreenshot()`; BLE traffic was read off
each node's UART log.

## Findings

- **(2) Standard wind payload: yes.** Bluetooth SIG's Environmental
  Sensing Service (0x181A) defines True/Apparent Wind Speed (0x2A70,
  0x2A72: uint16, 0.01 m/s) and True/Apparent Wind Direction (0x2A71,
  0x2A73: uint16, 0.01 deg). Zephyr has the UUID macros
  (`BT_UUID_APPARENT_WIND_SPEED` etc.) and an ESS sample
  (`samples/bluetooth/peripheral_esp`) to adapt.
- ESS conventions match windmeter's existing math: apparent direction is
  relative to the boat's heading, true direction is a compass bearing,
  both "wind comes from". `windmeter`'s `compute_true_wind` already
  outputs exactly that, so no conversion is needed.
- **(1) grvl in Renode: yes, on the STM32H747I-DISCO.** Renode ships a
  grvl demo script for this board (LTDC display controller, DMA2D, SD card
  and SDRAM modelled; DSI stubbed). Antmicro's prebuilt demo renders a
  correct 800x480 UI. Our from-source build of the same demo also boots
  and renders, but with defects (missing icons, missing letter fragments,
  empty arrow buttons), stable over time; Renode logged no unsupported
  graphics operations. Cause not isolated (see Residual Uncertainty).
- **(3) BLE in Renode: yes, but only through an nRF52840.** Renode models
  the BLE radio only on the nRF52840. A display MCU without a radio works
  via the standard host/controller split: our STM32H747 build ran
  Zephyr's Bluetooth host over HCI-UART (H:4) to an nRF52840 running
  Zephyr's controller firmware, then scanned, connected, subscribed and
  received GATT notifications from an emulated nRF52840 sensor.
- Running a BLE pair and the grvl display node together works; BLE needs a
  10 us global time step, which made the display node about 1.7x slower in
  real time (6 virtual s in 35 s). Acceptable for tests.
- Resource use on the H747: grvl demo 455 KB flash, 84 KB RAM, ~4.4 MB
  SDRAM (framebuffers); Bluetooth host 113 KB flash, 23 KB RAM.
- **Surprises**: grvl loads its UI assets (XML, fonts, images) from an SD
  card image, not from flash -- real hardware would need the SD card or a
  different asset store.
- **Constraints discovered**: the SDK's host-tools setup needs the `file`
  command (missing here, no sudo); worked around with a `dtc` wrapper
  that runs the SDK's dtc through its bundled loader, passed as
  `-DDTC=...`. The Bluetooth host needs Zephyr's `mbedtls` and
  `tf-psa-crypto` modules, which the grvl demo's narrowed west manifest
  leaves out; and the grvl module must be excluded from non-grvl builds
  (it compiles C++ code that needs C++ enabled), done via an explicit
  `-DZEPHYR_MODULES=` list.

## Recommendation

Emulation-first is viable. Target architecture for the requirements stage:
STM32H747 (display + grvl + Bluetooth host) with an nRF52840 as BLE
controller over HCI-UART, receiving ESS wind characteristics. Develop and
test the whole chain in Renode, with Zephyr's `native_sim` as the fast UI
loop. Before committing to it, resolve the from-source rendering defects
(next item), since every later feature sits on that UI path.

## Impact on Specs

- **Requirements**: wireless standard can be decided as BLE ESS. Decide
  whether head_unit is BLE central (it connects to sensors) -- the
  natural fit for "pair any sensor". The BOM target needs checking against
  an H7-class MCU + 800x480 display + nRF52840 module, which may not fit
  ~$125.
- **Plan**: board for development and emulation: STM32H747I-DISCO with
  the B-LCD40-DSI shield. Workspace setup must include the crypto modules
  and the dtc workaround above (or a Zephyr CI container).

## Residual Uncertainty

- Why our from-source grvl build renders with defects when the prebuilt
  one doesn't (framebuffer is 16-bit RGB565 in ours vs 32-bit in the
  prebuilt; Zephyr 4.4 dev vs 4.2). Cheapest test: build the demo for
  `native_sim` on a machine with SDL2 and a screen -- same defects means a
  firmware/grvl issue, clean output means a Renode model gap.
- grvl and the Bluetooth host in one firmware image: both run on the same
  board separately; not yet combined.
- An ESS wind *sensor* node was not built; the GATT notification path was
  proven with the heart-rate profile, which uses the same mechanism.
- Whether real hardware matches: the DSI panel path is stubbed in Renode,
  so the display driver's panel bring-up is untested.
