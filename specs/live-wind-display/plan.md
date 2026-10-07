# Plan: live-wind-display

## Approach

Three Zephyr images from one west workspace, run together in Renode -- the
split the spike proved, built from our own sources:

- **display** (STM32H747I-DISCO): grvl UI + Bluetooth host, reaching the
  radio over HCI-UART;
- **HCI controller** (nRF52840): Zephyr's stock `hci_uart` sample, no code
  of ours;
- **sensor simulator** (nRF52840): advertises Environmental Sensing and
  notifies scripted wind values.

The workspace pins the Zephyr commit, grvl revision and two patches that
Antmicro's grvl demo uses -- the only combination proven to build grvl
from source. Moving to the v4.4.2 release is a later, separate step.

## Walking Skeleton

Start the emulation and watch a wind speed sent by the simulated sensor
appear on the head_unit screen.

**Build first to validate:**

- Our own grvl build draws text correctly (the spike's from-source build
  lost letter fragments and icons).
- grvl and the Bluetooth host run together in one image.
- Workspace-built controller and sensor interoperate with the display.

## Components

| Component | Purpose |
|-----------|---------|
| `west.yml` | Pins Zephyr, grvl and only the needed modules (cmsis_6, hal_stm32, hal_nordic, mbedtls, tf-psa-crypto) |
| `app/` wind link | Connects only to `CONFIG_HEAD_UNIT_SENSOR_ADDR`, subscribes the 4 ESS wind characteristics, rescans on disconnect |
| `app/` wind state | Pure C/C++ (no Zephyr/grvl/BT deps): latest value + receive time per field, 3 s staleness, conversions; fed over a zbus channel |
| `app/` wind UI | grvl screen (XML on the SD-card romfs): four values, AWA dial, sensor-lost indicator |
| `sensor_sim/` | nRF52840 ESS peripheral, fixed address from Kconfig, scripted values; apparent-only variant |
| `renode/` | resc + robot tests with a shared bring-up keyword (windmeter's pattern) |
| `ci/verify.sh` | Builds all images, runs unit tests and Renode tests -- the verification binding |

## Interfaces

- Wind state, ztest-tested on `native_sim/native/64` (no display):
  `wind_state_update(field, raw_u16, now_ms)`;
  `wind_state_view(now_ms) -> wind_view` (formatted fields, per-field
  valid flag, `sensor_lost`).
- Tests read the screen from a log line written after a frame with a changed view is handed to the display (not every frame, so the deferred log keeps up), e.g.
  `wind_ui: AWS=10.0kn AWA=45S TWS=8.2kn TWD=270 LOST=0`. Screenshots are
  saved for human review, not asserted on.
- Robot tests drive scenarios by halting/resuming the sensor's CPU and by
  choosing which sensor images run (foreign sensor, apparent-only).

## Risks

| Risk | Impact | Mitigation |
|------|--------|------------|
| From-source grvl rendering defects persist (single point of failure) | No usable screen | Skeleton checks first; try the 32-bit framebuffer the working prebuilt used; compare `native_sim` on your machine; fallback to the prebuilt's older grvl/Zephyr pair |
| Zephyr pinned to a development commit + patches | Upgrades may break grvl | Pin exact SHAs; upgrade as its own change |
| Renode gives each nRF52840 the same default address | Can't test "ignore other sensors" | Sensor sim sets its address explicitly |
| 4 machines at a 10 us time step | Slow tests | Short scripted runs; measure in the skeleton |

## Open Questions

- Workspace location (decided): move this repo to
  `~/git/head_unit-ws/head_unit` and `west init -l` there, so `zephyr/`
  and `modules/` stay out of `~/git/windmeter/`. Fetch with
  `west update --path-cache ~/zephyrproject` to reuse the existing local
  clones. `~/zephyrproject` (Zephyr 4.5.0-rc1, main) is not built against:
  it differs from our pinned commit, and our manifest must not move it.
  Zephyr SDK 1.0.1 is installed at `~/zephyr-sdk-1.0.1`; `west` is not on
  PATH and must come from a venv.
- [ASSUMPTION: no CI this cycle (repo not on GitHub yet); `ci/verify.sh` stays runnable in Zephyr's CI container later.]
- [ASSUMPTION: `native_sim` with an SDL window is for UI work on your machine, not part of the gate -- this sandbox has no SDL.]
- [VERIFY IN WALKING SKELETON: correct text rendering; grvl + BT host fit in RAM together; combined emulation speed.]
