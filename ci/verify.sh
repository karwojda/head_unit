#!/usr/bin/env bash
# head_unit verification: workspace pins, firmware and native builds,
# wind state ztests, Renode scenarios. Ends with "All checks passed." only
# when every step succeeded.
#
# Run from anywhere inside the west workspace (default ~/git/head_unit-ws):
#   head_unit/ci/verify.sh
# Overrides: HEAD_UNIT_WS, RENODE_DIR, ZEPHYR_SDK_INSTALL_DIR.
set -euo pipefail

repo=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
ws=${HEAD_UNIT_WS:-$(dirname -- "$repo")}
renode_dir=${RENODE_DIR:-$ws/tools/renode}
export ZEPHYR_SDK_INSTALL_DIR=${ZEPHYR_SDK_INSTALL_DIR:-$HOME/zephyr-sdk-1.0.1}

step() { printf '\n=== %s\n' "$*"; }
fail() { printf 'FAILED: %s\n' "$*" >&2; exit 1; }

cd "$ws"
mkdir -p build
# shellcheck disable=SC1091
source "$ws/.venv/bin/activate"

step "Workspace pins"
# Every project the manifest names is checked out at its pinned revision.
expected="zephyr grvl cmsis_6 hal_nordic hal_stm32 mbedtls tf-psa-crypto"
listed=$(west list -f '{name}')
for name in $expected; do
	grep -qx "$name" <<<"$listed" || fail "west list is missing $name"
done
while read -r name path revision; do
	[ "$name" = manifest ] && continue
	head=$(git -C "$path" rev-parse HEAD)
	pinned=$(git -C "$path" rev-parse "$revision^{commit}")
	[ "$head" = "$pinned" ] || fail "$name is at $head, manifest pins $pinned"
	echo "$name $head"
done < <(west list -f '{name} {path} {revision}')

step "Zephyr patches applied"
# git apply --reverse --check succeeds only when the patch is in place.
for patch in "$repo"/zephyr/patches/*.patch; do
	git -C "$ws/zephyr" apply --reverse --check "$patch" ||
		fail "$(basename "$patch") is not applied (run: west patch apply)"
	echo "$(basename "$patch")"
done

step "Build firmware (stm32h747i_disco/stm32h747xx/m7)"
west build -p auto -b stm32h747i_disco/stm32h747xx/m7 -d build/head_unit head_unit/app

# The nRF52840 images leave the grvl module out (it needs C++).
nrf_modules="$ws/modules/hal/cmsis_6;$ws/modules/hal/nordic;$ws/modules/crypto/mbedtls;$ws/modules/crypto/tf-psa-crypto"

step "Build BLE controller (hci_uart, nrf52840dk/nrf52840)"
west build -p auto -b nrf52840dk/nrf52840 -d build/hci_uart zephyr/samples/bluetooth/hci_uart -- \
	-DEXTRA_DTC_OVERLAY_FILE="$repo/hci_controller/nrf52840dk_nrf52840.overlay" \
	"-DZEPHYR_MODULES=$nrf_modules"

step "Build sensor simulator (nrf52840dk/nrf52840)"
west build -p auto -b nrf52840dk/nrf52840 -d build/sensor_sim head_unit/sensor_sim -- \
	"-DZEPHYR_MODULES=$nrf_modules"

step "Build native_sim UI"
west build -p auto -b native_sim/native/64 -d build/native_sim head_unit/app

step "Wind state ztests (native_sim)"
west build -p auto -b native_sim/native/64 -d build/wind_state_test \
	head_unit/app/tests/wind_state -t run | tee build/ztest.log
grep -q "PROJECT EXECUTION SUCCESSFUL" build/ztest.log ||
	fail "wind state ztests"

step "Renode scenarios"
# renode-test writes its results (log.html, screenshots) to the current
# directory whatever -r says, so run it from build/robot.
mkdir -p build/robot
(cd build/robot && "$renode_dir/renode-test" --variable "HEAD_UNIT_WS:$ws" \
	"$repo"/renode/tests/*.robot) || fail "Renode scenarios (see build/robot/log.html)"

printf '\nAll checks passed.\n'
