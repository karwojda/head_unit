# head_unit

Helm display for small racing boats: shows apparent and true wind from the
boat's wind sensor. Zephyr RTOS + the grvl GUI library, on the
STM32H747I-DISCO with its 800x480 panel, run in Renode until hardware
bring-up.

## Workspace

The repo is the manifest of a west workspace (`~/git/head_unit-ws`). All
commands below run from the workspace root with its virtualenv active.

```sh
mkdir -p ~/git/head_unit-ws && cd ~/git/head_unit-ws
git clone <this repo> head_unit
python3 -m venv .venv && . .venv/bin/activate && pip install west
west init -l head_unit
west update --path-cache ~/zephyrproject
west patch apply
pip install -r zephyr/scripts/requirements.txt
```

Tools kept outside the repo: Zephyr SDK 1.0.1 (`~/zephyr-sdk-1.0.1`) and
Renode 1.17 portable (`tools/renode`).

## Build the firmware

Three images: head_unit itself, its BLE radio (an nRF52840 running
Zephyr's stock `hci_uart`, with HCI moved to `uart1`) and the simulated
wind sensor. The two nRF52840 builds leave the grvl module out (it needs
C++), hence the explicit module list.

```sh
west build -p -b stm32h747i_disco/stm32h747xx/m7 -d build/head_unit head_unit/app

NRF_MODULES="$PWD/modules/hal/cmsis_6;$PWD/modules/hal/nordic;$PWD/modules/crypto/mbedtls;$PWD/modules/crypto/tf-psa-crypto"
west build -p -b nrf52840dk/nrf52840 -d build/hci_uart zephyr/samples/bluetooth/hci_uart -- -DEXTRA_DTC_OVERLAY_FILE=$PWD/head_unit/hci_controller/nrf52840dk_nrf52840.overlay "-DZEPHYR_MODULES=$NRF_MODULES"
west build -p -b nrf52840dk/nrf52840 -d build/sensor_sim head_unit/sensor_sim -- "-DZEPHYR_MODULES=$NRF_MODULES"
```

head_unit's build produces `build/head_unit/zephyr/zephyr.elf` and the SD
card image `build/head_unit/zephyr/zephyr.sdcard.img` (ext2, built from
`app/romfs/`: `gui.xml` and the fonts). head_unit connects only to the
sensor at `CONFIG_HEAD_UNIT_SENSOR_ADDR` (default `C0:FF:EE:00:00:01`, the
simulator's `CONFIG_SENSOR_SIM_ADDR`).

## Run the emulation

With the simulated sensor -- the wind page shows its wind:

```sh
tools/renode/renode -e '$bin=@build/head_unit/zephyr/zephyr.elf; $sdcard=@build/head_unit/zephyr/zephyr.sdcard.img; $hci_bin=@build/hci_uart/zephyr/zephyr.elf; $sensor_bin=@build/sensor_sim/zephyr/zephyr.elf; include @head_unit/renode/live_wind.resc; emulation RunFor "1"; mach set "head_unit"; cpu IsHalted false; start'
```

Then set the sensor's values on its console (`mach set "sensor"`,
`showAnalyzer sysbus.uart0`), in ESS units -- 0.01 m/s and 0.01 deg
clockwise from the bow (apparent) or from north (true):

```
wind aws 514     # 5.14 m/s  -> AWS 10.0 kn
wind awa 4500    # 45 deg    -> AWA 45S
wind tws 422     # 4.22 m/s  -> TWS 8.2 kn
wind twd 27000   # 270 deg   -> TWD 270
```

It sends each value once a second. Without the sensor, include
`renode/head_unit.resc` instead and drop `$sensor_bin` (head_unit and its
radio only): every value shows "--" and sensor lost is on.

`renode/head_unit.resc` holds head_unit's CPU until its radio has had 1 s to
start (the command above then releases it) and corrects two Renode model gaps
so time on the board matches the real one: SysTick at 400 MHz (Renode's
clock model leaves it at 128 MHz) and the CPU at 400 MIPS. head_unit's
console is on `sysbus.usart1` (`mach set "head_unit"`, `showAnalyzer
sysbus.usart1`); each change on the wind page logs a line such as
`wind_ui: AWS=10.0kn AWA=45S TWS=8.2kn TWD=270 LOST=0`.

Screenshot of the panel: `include @head_unit/renode/screenshot.py`, then
`save_screenshot "wind_page.png"`.

Measured (head_unit + radio + sensor, on an 8-thread i7-1185G7):

- head_unit RAM: 100 KB of 512 KB (flash 493 KB of 1 MB; grvl's
  framebuffers and heap: 9.5 MB of 32 MB SDRAM)
- real-time speed: 0.22x (10 virtual seconds take 45 s)

## Quick UI loop on the PC (native_sim)

As in Antmicro's grvl calendar demo, the app also builds for Zephyr's
native simulator with an SDL window. The UI assets are read straight from
`app/romfs/` (or `$ROMFS_PATH`) instead of an SD card.

```sh
west build -b native_sim/native/64 -d build/native_sim head_unit/app
ROMFS_PATH=head_unit/app/romfs build/native_sim/zephyr/zephyr.exe
```

There is no radio here: type wind values on the shell in the same
terminal, with the sensor simulator's syntax and units (0.01 m/s, 0.01
deg). They reach the page over `wind_chan`, the path the BLE link uses.

```
wind aws 514     # AWS 10.0
wind awa 27000   # AWA 90P, dial needle to port
wind tws 422     # TWS 8.2
wind twd 500     # TWD 005
```

Needs SDL2 and a display (WSLg works). Not part of the test gate.

## Tests

Everything (workspace pins, all builds, ztests, Renode scenarios):

```sh
head_unit/ci/verify.sh
```

Wind state unit tests (ztest, host):

```sh
west build -b native_sim/native/64 -d build/wind_state_test head_unit/app/tests/wind_state -t run
```

Renode scenarios (build the three images first):

```sh
tools/renode/renode-test head_unit/renode/tests/boot.robot head_unit/renode/tests/live_wind.robot
```
