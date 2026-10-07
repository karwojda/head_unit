# Tasks: live-wind-display

## Implementation Constraints

**Permitted**: C and C++ (C++ wherever useful, not only for grvl glue).
Zephyr at the pinned commit + Antmicro's two patches, grvl at the pinned
revision, Zephyr's Bluetooth host, zbus, ztest, Robot Framework via
Renode's `renode-test`.
**Not Permitted**: building against `~/zephyrproject` or moving it; vendoring
Zephyr/grvl into this repo; BLE bonding/encryption; asserting on screenshots
(they are for human review; tests read the `wind_ui:` log line).
**Tooling**: west from `~/git/head_unit-ws/.venv`; Renode 1.17 portable at
`~/git/head_unit-ws/tools/renode`; Zephyr SDK 1.0.1 at `~/zephyr-sdk-1.0.1`
with the spike's `dtc` wrapper. All outside the repo.
**Style**: wind state has no Zephyr/grvl/BT dependencies (host-testable on
`native_sim/native/64`). Robot tests share one bring-up keyword (windmeter's
pattern) and read as Given/When/Then.

## Slice Ledger

| # | User-job (what the sailor DOES) | Surfaces touched | Distinct slice because… |
|---|---------------------------------|------------------|-------------------------|
| 1 | Switches head_unit on with no sensor and sees an honest empty wind page | west.yml + patches, `app/` screen + staleness, `renode/`, `ci/verify.sh` | the sailor can trust the display before any data arrives |
| 2 | Reads live wind from their own sensor | `app/` BLE link + conversions + screen, `sensor_sim/`, HCI controller, robot test | reading the wind is the actual job |
| 3 | Can tell when the display has no wind to show | wind state, screen, apparent-only sim variant | a wrong number is worse than none |
| 4 | Keeps seeing their own boat's wind among other boats | BLE link, foreign-address sim variant | racing happens next to other sensors |

## Involvement Summary

| Task | Level | Focus | Rationale |
|------|-------|-------|-----------|
| 1 | pair | workspace pins; correct text rendering | first own-source grvl build; the plan's top risk |
| 2 | pair | AC 1 timing, AC 6 conversions | grvl + BT in one image (RAM, speed); numbers must be right |
| 3 | checkpoint | AC 3 timeout edges | logic understood, timing edges need a look |
| 4 | checkpoint | AC 2 test shape | Renode does not model signal strength |

## Walking Skeleton

### Task 1 [pair]: Head_unit boots to its wind page

- [~] **Goal**: in emulation, with no sensor present, head_unit shows the wind page with all four values as "--" and the sensor-lost indicator on.
- **Focus**: west manifest pins + patches; text and icons render without the spike's defects.
- **Touches**: `west.yml`, `zephyr/patches*`, `app/` (CMake, prj.conf, board overlays incl. `native_sim_64.overlay` for the SDL UI loop as in Antmicro's demo, wind state, wind UI, `romfs/` XML + fonts), `renode/head_unit.resc`, `renode/tests/boot.robot`, `ci/verify.sh`, `WHITTLESPEC.md` (verification binding), `README.md`.
- **Depends on**: None
- **Acceptance**:
  1. `west init -l` + `west update --path-cache ~/zephyrproject` + `west patch apply` fetch the pinned workspace into `~/git/head_unit-ws` -- **Demo**: `west list` shows zephyr, grvl, cmsis_6, hal_stm32, hal_nordic, mbedtls, tf-psa-crypto at the pinned revisions.
  2. Booted with no sensor, the screen shows "--" for AWS, AWA, TWS, TWD and sensor-lost on -- **Demo**: `boot.robot` sees `wind_ui: AWS=-- AWA=-- TWS=-- TWD=-- LOST=1`; the saved screenshot shows clean text.
  3. `ci/verify.sh` builds, runs the ztests and runs the Renode tests -- **Demo**: `ci/verify.sh` ends with "All checks passed."
- **Reachability**:
  - **Surface**: the device's power-on screen (the wind page is what head_unit boots to); `README.md` says how to start the emulation.
  - **Boss demo**: "Start head_unit and tell me what it shows." Boss runs the README's emulation command, watches the screen: wind page, four "--" values, sensor-lost on.
  - **Three regret scenarios**: 1. Text renders with missing glyphs, so every later screen is unreadable. 2. Cross-cut: ship only this → a display that boots but never shows wind; coherent, just less capability (Task 2 adds it). 3. Pins drift from what Antmicro proved, and grvl stops building on the next `west update`.
- **Tests green after**: `wind_state` ztest (never-received → invalid/lost), `boot.robot`.
- **BDD decision**: use -- "Given no sensor, When head_unit boots, Then all wind values show -- and sensor lost is on".

### Task 2 [pair]: See live wind from my sensor

- [ ] **Goal**: with the simulated sensor running, its apparent/true wind speed and angle appear within 5 s (knots to 0.1, AWA 0-180 port/starboard, TWD 0-359, AWA dial) and each new value shows within 1 s.
- **Focus**: AC 1 timing; AC 6 conversions (5.14 m/s → 10.0 kn; 359.6 → 0; 270 → 90 port).
- **Touches**: `app/` wind link (connect to `CONFIG_HEAD_UNIT_SENSOR_ADDR`, subscribe 4 ESS characteristics, zbus), wind state conversions, wind UI (values + dial), `sensor_sim/`, HCI controller build (`hci_uart`), `renode/` resc + `live_wind.robot`, `ci/verify.sh`, `README.md`.
- **Depends on**: Task 1
- **Acceptance**:
  1. Display, HCI controller and sensor sim run together; values appear within 5 s of start -- **Demo**: `live_wind.robot` sees `AWS=10.0kn AWA=45S TWS=… TWD=…` within 5 s virtual.
  2. A scripted change shows within 1 s -- **Demo**: robot sees the next scripted value within 1 s of the sim's send log.
  3. Conversions and rounding per AC 6 -- **Demo**: ztest cases for each requirements example pass.
  4. Records measured RAM use and real-time speed of the combined emulation in the README.
- **Reachability**:
  - **Surface**: the same power-on wind page, now fed by the sensor; README's emulation command starts all three machines.
  - **Boss demo**: "Show me the wind." Boss runs the emulation command; within seconds the four values and the dial appear and change as the sim's script runs.
  - **Three regret scenarios**: 1. grvl + BT host don't fit in RAM together, found only now (mitigation: check first). 2. Cross-cut: ship this without Task 3 → a silent sensor leaves the last numbers frozen on screen, contradicting the requirement that stale values never look live. Accepted only as an intermediate state: Task 3 follows immediately, and Task 1's no-data path already shows "--". 3. Port/starboard swapped, so the sailor misreads the wind side.
- **Tests green after**: conversion ztests, `boot.robot`, `live_wind.robot`.
- **BDD decision**: use -- AC 1 scenario.

**INTEGRATION MILESTONE**: after Task 2, the full chain (sensor → BLE → screen) runs from our own sources; check RAM fit and emulation speed against the plan's risks.

## Remaining Tasks

### Task 3 [checkpoint]: Never show wind the display doesn't have

- [ ] **Goal**: no update within 3 s → all values "--" and sensor-lost on, cleared when updates resume; a sensor sending apparent wind only shows true wind as "--", not 0.
- **Focus**: AC 3 (3 s boundary, clearing on resume).
- **Touches**: wind state, wind UI, `sensor_sim/` apparent-only variant, `renode/tests/sensor_lost.robot`, `apparent_only.robot`.
- **Depends on**: Task 2
- **Acceptance**:
  1. Halting the sensor CPU blanks values and sets sensor-lost; resuming restores them -- **Demo**: `sensor_lost.robot`.
  2. Apparent-only sensor → TWS/TWD "--" -- **Demo**: `apparent_only.robot` sees `TWS=-- TWD=--` with AWS/AWA live.
- **Reachability**:
  - **Surface**: the wind page.
  - **Boss demo**: "Make the sensor go quiet." Boss pauses the sensor machine in Renode; within 3 s the values become "--" and sensor-lost lights; boss resumes it and the values return.
  - **Three regret scenarios**: 1. Timeout measured from the connection, not the last value, so a connected but silent sensor still looks live. 2. Cross-cut: ship all except this → stale numbers look live, the exact thing requirements forbid. 3. A missing true wind shows as 0.0 kn, which a sailor reads as a calm.
- **Tests green after**: wind_state ztests, all robot tests.
- **BDD decision**: use -- AC 3 and AC 5 scenarios.

### Task 4 [checkpoint]: Stay with my own sensor

- [ ] **Goal**: another wind sensor in range is never connected to or shown; after the link drops, head_unit reconnects to the remembered sensor without user action.
- **Focus**: AC 2 test shape -- Renode does not model signal strength, so the foreign sensor advertises first and keeps advertising throughout.
- **Touches**: wind link, `sensor_sim/` foreign-address variant, `renode/tests/foreign_sensor.robot`, `reconnect.robot`.
- **Depends on**: Task 2 (parallel-safe with Task 3 only if run in separate worktrees; both touch `renode/`)
- **Acceptance**:
  1. Foreign sensor running alongside → its values never appear, ours do -- **Demo**: `foreign_sensor.robot`.
  2. Link dropped (sensor halted past supervision timeout) then resumed → values return with no action -- **Demo**: `reconnect.robot`.
- **Reachability**:
  - **Surface**: the wind page.
  - **Boss demo**: "Another boat pulls alongside." Boss starts the emulation with the foreign sensor; only our sensor's scripted values ever appear. Boss halts our sensor for 10 s and resumes it; the values come back with no input.
  - **Three regret scenarios**: 1. The display latches onto the strongest nearby sensor and shows another boat's wind. 2. Cross-cut: ship all except this → after a reconnect, rescanning picks whichever sensor answers first. 3. Reconnect loop never backs off and starves the UI.
- **Tests green after**: all robot tests.
- **BDD decision**: use -- AC 2 and AC 4 scenarios.

## Backlog (Deferred)

- Signal-strength version of AC 2 -- Renode does not model RSSI; Context: test uses advertising order instead; Impact if forgotten: real-hardware ordering untested; Revisit when: hardware bring-up feature.
- DMA2D acceleration off (`CONFIG_GRVL_USE_STM32_DMA2D=n`) -- Context: Renode's DMA2D model garbles RGB565 glyphs and throws on ARGB8888 transfers, so grvl renders in software; Impact if forgotten: slower redraws on the real board; Revisit when: hardware bring-up feature.
- grvl does not check its framebuffer allocation -- Context: with the demo's 3.7 MB malloc arena and a 32-bit framebuffer, allocation returned NULL and grvl drew to address 0 silently; Impact if forgotten: the same silent black screen on any memory change; Revisit when: reporting upstream to grvl, or next arena/resolution change.
- First frame shows ~3 s after grvl hands it over -- Context: in Renode the `wind_ui:` line (logged after `Swap()`) appears at ~1.4 s virtual but the LTDC shows the page only at ~4.3 s, so `boot.robot`'s 5 s bound checks the hand-over, not pixels; the review screenshot is taken 4 s later; Impact if forgotten: AC 1's "within 5 s" can be met in the log while the screen is still black; Revisit when: real hardware, or if the 5 s budget becomes crucial.
