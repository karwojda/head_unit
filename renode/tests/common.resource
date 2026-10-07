*** Settings ***
Library           OperatingSystem

*** Variables ***
# Default matches the local workspace (~/git/head_unit-ws); override with
# `--variable HEAD_UNIT_WS:<path>`.
${HEAD_UNIT_WS}       %{HOME}/git/head_unit-ws
${HEAD_UNIT_ROOT}     ${HEAD_UNIT_WS}/head_unit
${RESC}               ${HEAD_UNIT_ROOT}/renode/head_unit.resc
${ELF}                ${HEAD_UNIT_WS}/build/head_unit/zephyr/zephyr.elf
${SDCARD}             ${HEAD_UNIT_WS}/build/head_unit/zephyr/zephyr.sdcard.img
${README}             ${HEAD_UNIT_ROOT}/README.md
${SCREENSHOT}         ${HEAD_UNIT_ROOT}/renode/screenshot.py

# Strict: any error log, Zephyr fatal error or fault fails the scenario
# the moment it appears on the console, whichever step is running.
# Exception: dsi_stm32 and nt35510 errors -- Renode stubs the DSI host, so
# its init and the panel ID read over it always fail in emulation; real
# hardware does not log them.
${FAILURE_PATTERN}    <err> (?!dsi_stm32: |nt35510: )|FATAL ERROR|Fault

# What the wind page logs when it starts showing no wind (it logs a line
# whenever what it shows changes).
${NO_WIND_LINE}       wind_ui: AWS=-- AWA=-- TWS=-- TWD=-- LOST=1

*** Keywords ***
# --- Bring-up ---

No Wind Sensor Is In Range
    # Only the head_unit machine exists: no BLE controller, no sensor.
    Execute Command           $bin=@${ELF}
    Execute Command           $sdcard=@${SDCARD}
    Execute Command           include @${RESC}
    Execute Command           include @${SCREENSHOT}
    Create Terminal Tester    sysbus.usart1    defaultPauseEmulation=true
    Register Failing Uart String    ${FAILURE_PATTERN}    treatAsRegex=true
    ${machines}=              Execute Command    emulation MachinesCount
    Should Be Equal As Integers    ${machines}    1

Head_unit Starts
    Start Emulation

# --- Wind page ---

The Wind Page Shows "--" For Apparent Wind Speed, Apparent Wind Angle, True Wind Speed And True Wind Direction
    ${line}=                  Wait For Line On Uart    wind_ui: AWS=-- AWA=-- TWS=-- TWD=--    timeout=5
    Set Test Variable         ${WIND_LINE}    ${line}[Line]

The Sensor-Lost Indicator Is On
    Should End With           ${WIND_LINE}    LOST=1

10 Seconds Later The Wind Page Still Shows "--" For Every Value With The Sensor-Lost Indicator On
    # Any redraw that isn't the no-wind line within 10 s fails.
    Should Not Be On Uart     wind_ui: (?!AWS=-- AWA=-- TWS=-- TWD=-- LOST=1$)    timeout=10    treatAsRegex=true

The Wind Page Is Drawn Within 5 Seconds
    Wait For Line On Uart     wind_ui:    timeout=5
    # Screenshot is for human review only. The LTDC shows the first frame
    # ~3 s after grvl hands it over (see tasks.md backlog), so let the
    # page settle first; the failing pattern still guards these 4 s.
    Execute Command           emulation RunFor "4"
    Execute Command           save_screenshot "${OUTPUT_DIR}/wind_page_${TEST_NAME}.png"

Head_unit Reports No Fault
    # The registered failing pattern already guards every step; also run
    # on 2 s so a fault right after the first redraw is caught here.
    Should Not Be On Uart     ${FAILURE_PATTERN}    timeout=2    treatAsRegex=true

# --- Operator docs ---

The Head_unit README
    ${text}=                  Get File    ${README}
    Set Test Variable         ${README_TEXT}    ${text}

It Names The Command That Builds The Head_unit Firmware
    Should Match Regexp       ${README_TEXT}    west build [^\\n]*-b stm32h747i_disco/stm32h747xx/m7[^\\n]*\\bapp\\b

It Names The SD Card Image The Emulation Loads
    Should Contain            ${README_TEXT}    zephyr.sdcard.img

It Names The Emulation Script That These Scenarios Start
    Should Contain            ${README_TEXT}    renode/head_unit.resc
