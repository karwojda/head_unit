*** Comments ***
Boot behaviour of head_unit in Renode (Task 1). Steps live in
common.resource. Run with ci/verify.sh, or directly:
  renode-test renode/tests/boot.robot

*** Settings ***
Resource          common.resource

*** Test Cases ***
Head_unit with no sensor boots to a wind page showing no wind
    [Documentation]    AC 3 (never-received form). Validates that no number
    ...    is shown before any sensor value has arrived, and that this holds
    ...    over time. Does NOT cover the timeout after values were shown
    ...    (Task 3) or screen legibility (screenshot, human review).
    [Tags]    boot    wind-page
    Given no wind sensor is in range
    When head_unit starts
    Then the wind page shows "--" for apparent wind speed, apparent wind angle, true wind speed and true wind direction
    And the sensor-lost indicator is on
    And 10 seconds later the wind page still shows "--" for every value with the sensor-lost indicator on

Head_unit boots without a fault and loads its screen from the SD card
    [Documentation]    Entry point: the real firmware image boots to the
    ...    wind page. 5 s bound comes from AC 1 (values within 5 s of start
    ...    leaves no room for a slower boot). Does NOT cover hardware panel
    ...    bring-up (DSI is stubbed in Renode).
    [Tags]    boot
    Given no wind sensor is in range
    When head_unit starts
    Then the wind page is drawn within 5 seconds
    And head_unit reports no fault

Operator docs explain how to start the emulation
    [Documentation]    The README is the operator's entry point. Checks it
    ...    names the build commands and the same emulation script these
    ...    scenarios start, so the docs can't drift from what is tested.
    [Tags]    docs
    Given the head_unit README
    Then it names the command that builds the head_unit firmware
    And it names the SD card image the emulation loads
    And it names the emulation script that these scenarios start
