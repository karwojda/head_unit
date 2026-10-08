## Durability
Binding: git commit; timing: per-task

## Work ledger
Binding: local

## Verification
Binding: head_unit/ci/verify.sh

Run from the workspace root (`~/git/head_unit-ws`), ~1 min. It checks the
west pins and Zephyr patches, builds the firmware and the native_sim UI,
runs the wind state ztests on native_sim and the Renode scenarios
(`renode/tests/*.robot`), and ends with "All checks passed." Mid-task, the
ztests alone are fast:
`west build -b native_sim/native/64 -d build/wind_state_test head_unit/app/tests/wind_state -t run`.
