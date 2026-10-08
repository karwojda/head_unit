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

```sh
west build -p -b stm32h747i_disco/stm32h747xx/m7 -d build/head_unit head_unit/app
```

This produces `build/head_unit/zephyr/zephyr.elf` and the SD card image
`build/head_unit/zephyr/zephyr.sdcard.img` (ext2, built from
`app/romfs/`: `gui.xml` and the fonts).

## Run the emulation

```sh
tools/renode/renode -e '$bin=@build/head_unit/zephyr/zephyr.elf; $sdcard=@build/head_unit/zephyr/zephyr.sdcard.img; include @head_unit/renode/head_unit.resc; start'
```

`renode/head_unit.resc` creates the board and loads the firmware and the
SD card image. The firmware console is on `sysbus.usart1`
(`showAnalyzer sysbus.usart1`). With no sensor the wind page shows "--"
for every value and sensor lost; the console logs
`wind_ui: AWS=-- AWA=-- TWS=-- TWD=-- LOST=1`.

Screenshot of the panel: `ltdc TakeScreenshot "wind_page.png"`.

## Quick UI loop on the PC (native_sim)

As in Antmicro's grvl calendar demo, the app also builds for Zephyr's
native simulator with an SDL window. The UI assets are read straight from
`app/romfs/` (or `$ROMFS_PATH`) instead of an SD card.

```sh
west build -b native_sim/native/64 -d build/native_sim head_unit/app
ROMFS_PATH=head_unit/app/romfs build/native_sim/zephyr/zephyr.exe
```

Needs SDL2 and a display (WSLg works). Not part of the test gate.

## Tests

Everything (workspace pins, both builds, ztests, Renode scenarios):

```sh
head_unit/ci/verify.sh
```

Wind state unit tests (ztest, host):

```sh
west build -b native_sim/native/64 -d build/wind_state_test head_unit/app/tests/wind_state -t run
```

Renode scenarios (build the firmware first):

```sh
tools/renode/renode-test head_unit/renode/tests/boot.robot
```
