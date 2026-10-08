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
${LIVE_RESC}          ${HEAD_UNIT_ROOT}/renode/live_wind.resc
${HCI_ELF}            ${HEAD_UNIT_WS}/build/hci_uart/zephyr/zephyr.elf
${SENSOR_ELF}         ${HEAD_UNIT_WS}/build/sensor_sim/zephyr/zephyr.elf

# The sensor simulator's console (uart0), driven by these steps:
#   `wind <aws|awa|tws|twd> <raw>`  sets a value in ESS units (0.01 m/s,
#       0.01 deg) and replies `sensor_sim: set AWS=<raw>`;
#   each notification it sends is logged `sensor_sim: sent AWS=<raw>`.
${SENSOR_PROMPT}      uart:~$

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
    Head_unit Console Is Watched
    Set Test Variable         ${SENSOR}    ${None}
    ${machines}=              Execute Command    emulation MachinesCount
    Should Be Equal As Integers    ${machines}    1

My Wind Sensor Is In Range Sending Apparent Wind ${aws} m/s From ${awa} Degrees And True Wind ${tws} m/s From ${twd} Degrees
    # head_unit, its BLE controller and the sensor, on one BLE medium.
    File Should Exist         ${HCI_ELF}       HCI controller not built (see README)
    File Should Exist         ${SENSOR_ELF}    sensor simulator not built (see README)
    Execute Command           $bin=@${ELF}
    Execute Command           $sdcard=@${SDCARD}
    Execute Command           $hci_bin=@${HCI_ELF}
    Execute Command           $sensor_bin=@${SENSOR_ELF}
    Execute Command           include @${LIVE_RESC}
    Execute Command           include @${SCREENSHOT}
    Head_unit Console Is Watched
    ${sensor}=                Create Terminal Tester    sysbus.uart0    machine=sensor    defaultPauseEmulation=true
    Register Failing Uart String    ${FAILURE_PATTERN}    treatAsRegex=true    testerId=${sensor}
    Set Test Variable         ${SENSOR}    ${sensor}
    ${values}=                Create Dictionary
    ...                       aws=${{ round(float($aws) * 100) }}    awa=${{ round(float($awa) * 100) }}
    ...                       tws=${{ round(float($tws) * 100) }}    twd=${{ round(float($twd) * 100) }}
    Set Test Variable         ${SENSOR_VALUES}    ${values}
    ${machines}=              Execute Command    emulation MachinesCount
    Should Be Equal As Integers    ${machines}    3

Head_unit Console Is Watched
    ${hu}=                    Create Terminal Tester    sysbus.usart1    machine=head_unit    defaultPauseEmulation=true
    Register Failing Uart String    ${FAILURE_PATTERN}    treatAsRegex=true    testerId=${hu}
    Set Test Variable         ${HU}    ${hu}

Head_unit Starts
    IF    $SENSOR is None
        Set Test Variable     ${POWERED_ON_MS}    ${0}
        Start Emulation
    ELSE
        # The sensor is already sending when head_unit is switched on, and
        # the controller has had 1 s to set up its UART (live_wind.resc).
        Wait For Prompt On Uart    ${SENSOR_PROMPT}    testerId=${SENSOR}    timeout=2
        FOR    ${field}    ${raw}    IN    &{SENSOR_VALUES}
            The Sensor Is Set To    ${field}    ${raw}
        END
        ${now}=               Virtual Time In Ms
        IF    ${now} < 1000
            Execute Command   emulation RunFor "${{ (1000 - $now) / 1000 }}"
        END
        Execute Command       runMacro $power_on_head_unit
        ${on}=                Virtual Time In Ms
        Set Test Variable     ${POWERED_ON_MS}    ${on}
        Start Emulation
    END

The Sensor Is Set To
    [Arguments]               ${field}    ${raw}
    Write Line To Uart        wind ${field} ${raw}    testerId=${SENSOR}
    Wait For Line On Uart     sensor_sim: set ${field.upper()}=${raw}    testerId=${SENSOR}    timeout=1

Virtual Time In Ms
    ${info}=                  Execute Command    machine ElapsedVirtualTime
    ${t}=                     Should Match Regexp    ${info}    TimeElapsed = (\\d+):(\\d+):(\\d+\\.\\d+)
    RETURN                    ${{ ((int($t[1]) * 60 + int($t[2])) * 60 + float($t[3])) * 1000 }}

# --- Wind page ---

The Wind Page Shows "--" For Apparent Wind Speed, Apparent Wind Angle, True Wind Speed And True Wind Direction
    ${line}=                  Wait For Line On Uart    wind_ui: AWS=-- AWA=-- TWS=-- TWD=--    timeout=5    testerId=${HU}
    Set Test Variable         ${WIND_LINE}    ${line}[Line]

The Sensor-Lost Indicator Is ${state}
    ${flag}=                  Set Variable If    '${state}' == 'on'    1    0
    Should End With           ${WIND_LINE}    LOST=${flag}

Within ${seconds} Seconds The Wind Page Shows AWS "${aws}", AWA "${awa}", TWS "${tws}" And TWD "${twd}"
    # The whole line, so a swapped field or a stray unit fails.
    ${pattern}=               Catenate    SEPARATOR=
    ...                       wind_ui: AWS=${{ re.escape($aws) }}kn AWA=${awa} TWS=${{ re.escape($tws) }}kn TWD=${twd} LOST=\\d$
    ${line}=                  Wait For Line On Uart    ${pattern}    treatAsRegex=true    timeout=${seconds}    testerId=${HU}
    Set Test Variable         ${WIND_LINE}    ${line}[Line]
    ${after}=                 Evaluate    $line['Timestamp'] - $POWERED_ON_MS
    Should Be True            ${after} <= ${seconds} * 1000
    ...                       wind shown ${after} ms after head_unit was switched on

Head_unit Shows The Sensor's Wind
    # Precondition: head_unit is on and showing live values.
    Head_unit Starts
    ${line}=                  Wait For Line On Uart    wind_ui: AWS=\\d+\\.\\dkn AWA=\\d+[PS]? TWS=\\d+\\.\\dkn TWD=\\d{3} LOST=0$
    ...                       treatAsRegex=true    timeout=10    testerId=${HU}
    Set Test Variable         ${WIND_LINE}    ${line}[Line]

The Sensor Sends A New Apparent Wind Speed Of ${mps} m/s
    ${raw}=                   Evaluate    round(float($mps) * 100)
    Write Line To Uart        wind aws ${raw}    testerId=${SENSOR}
    ${sent}=                  Wait For Line On Uart    sensor_sim: sent AWS=${raw}    timeout=3    testerId=${SENSOR}
    Set Test Variable         ${SENT_MS}    ${sent}[Timestamp]

Within ${seconds} Second Of The Sensor Sending It The Wind Page Shows AWS "${aws}"
    ${line}=                  Wait For Line On Uart    wind_ui: AWS=${{ re.escape($aws) }}kn
    ...                       treatAsRegex=true    timeout=${seconds}    testerId=${HU}
    Set Test Variable         ${WIND_LINE}    ${line}[Line]
    ${after}=                 Evaluate    $line['Timestamp'] - $SENT_MS
    Should Be True            ${after} <= ${seconds} * 1000
    ...                       new value shown ${after} ms after the sensor sent it

AWA, TWS And TWD Still Show "${awa}", "${tws}" And "${twd}"
    Should Match Regexp       ${WIND_LINE}    AWA=${awa} TWS=${{ re.escape($tws) }}kn TWD=${twd} LOST=0$

10 Seconds Later The Wind Page Still Shows "--" For Every Value With The Sensor-Lost Indicator On
    # Any redraw that isn't the no-wind line within 10 s fails.
    Should Not Be On Uart     wind_ui: (?!AWS=-- AWA=-- TWS=-- TWD=-- LOST=1$)    timeout=10    treatAsRegex=true    testerId=${HU}

The Wind Page Is Drawn Within 5 Seconds
    Wait For Line On Uart     wind_ui:    timeout=5    testerId=${HU}
    # Screenshot is for human review only. The LTDC shows the first frame
    # ~3 s after grvl hands it over (see tasks.md backlog), so let the
    # page settle first; the failing pattern still guards these 4 s.
    Execute Command           emulation RunFor "4"
    Execute Command           save_screenshot "${OUTPUT_DIR}/wind_page_${TEST_NAME}.png"

Head_unit Reports No Fault
    # The registered failing pattern already guards every step; also run
    # on 2 s so a fault right after the first redraw is caught here.
    Should Not Be On Uart     ${FAILURE_PATTERN}    timeout=2    treatAsRegex=true    testerId=${HU}

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
