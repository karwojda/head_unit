*** Comments ***
Live wind from the remembered sensor (Task 2). Display, HCI controller
and sensor simulator run together. Steps live in common.resource.

Sensor values are given in what the sensor sends (m/s, degrees) and
checked in what the sailor reads, so the conversions are exercised end to
end: 5.14 m/s = 10.0 kn, 4.22 m/s = 8.2 kn, 6.17 m/s = 12.0 kn.

*** Settings ***
Resource          common.resource

*** Test Cases ***
The sensor's wind appears on head_unit within 5 seconds of start
    [Documentation]    AC 1 (first half). Entry point: the full chain
    ...    sensor -> BLE -> head_unit screen. Does NOT cover other sensors
    ...    in range (Task 4) or a silent sensor (Task 3).
    [Tags]    live-wind
    Given my wind sensor is in range sending apparent wind 5.14 m/s from 45 degrees and true wind 4.22 m/s from 270 degrees
    When head_unit starts
    Then within 5 seconds the wind page shows AWS "10.0", AWA "45S", TWS "8.2" and TWD "270"
    And the sensor-lost indicator is off

A new value from the sensor replaces the shown one within 1 second
    [Documentation]    AC 1 (second half) / task AC 2. Measured from the
    ...    sensor's own send log, so connection time doesn't count. Checks
    ...    the other values stay as they were (one update doesn't disturb
    ...    the rest).
    [Tags]    live-wind
    Given my wind sensor is in range sending apparent wind 5.14 m/s from 45 degrees and true wind 4.22 m/s from 270 degrees
    And head_unit shows the sensor's wind
    When the sensor sends a new apparent wind speed of 6.17 m/s
    Then within 1 second of the sensor sending it the wind page shows AWS "12.0"
    And AWA, TWS and TWD still show "45S", "8.2" and "270"

Wind from port shows as port on head_unit
    [Documentation]    AC 6 end to end (regret 3: sides swapped somewhere
    ...    between sensor and screen). Starboard is covered by the first
    ...    scenario; rounding edges are ztests.
    [Tags]    live-wind
    Given my wind sensor is in range sending apparent wind 5.14 m/s from 270 degrees and true wind 4.22 m/s from 5 degrees
    When head_unit starts
    Then within 5 seconds the wind page shows AWS "10.0", AWA "90P", TWS "8.2" and TWD "005"

Operator docs explain how to start the live wind emulation
    [Documentation]    Reachability + task AC 4. The README is the
    ...    operator's entry point; checks it names what these scenarios
    ...    actually run, so it can't drift. Does NOT check that the
    ...    recorded figures are current (measured by hand, AC 4).
    [Tags]    docs    todo
    Skip    TODO: steps defined in ws.bdd.red
    Given the head_unit README
    Then it names the command that builds the sensor simulator
    And it names the command that builds the HCI controller
    And it names the emulation script that runs head_unit with the sensor
    And it records the combined emulation's RAM use in KB and its real-time speed
