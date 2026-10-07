/*
 * wind_state unit tests -- Task 1 scope: the state before any sensor
 * value has arrived. Conversions (Task 2) and timeout after data
 * (Task 3) get their own ideas when those tasks start.
 *
 * Shared Arrange (fixture in red): a freshly initialised wind state,
 * new for every test so nothing leaks between them.
 */
#include <zephyr/ztest.h>

#include "wind_state.h"

static const enum wind_field all_fields[] = {
	WIND_AWS, WIND_AWA, WIND_TWS, WIND_TWD,
};

struct wind_state_fixture {
	struct wind_state state;
};

static void *wind_state_setup(void)
{
	static struct wind_state_fixture fixture;

	return &fixture;
}

static void wind_state_before(void *f)
{
	struct wind_state_fixture *fixture = f;

	wind_state_init(&fixture->state);
}

ZTEST_SUITE(wind_state, NULL, wind_state_setup, wind_state_before, NULL, NULL);

static void assert_all_fields_invalid(const struct wind_view *view)
{
	for (size_t i = 0; i < ARRAY_SIZE(all_fields); i++) {
		zassert_false(view->field[all_fields[i]].valid,
			      "field %d valid with nothing received", all_fields[i]);
	}
}

/* --- Never received: nothing is shown as live --- */

ZTEST_F(wind_state, test_view_before_any_update_marks_all_fields_invalid)
{
	/*
	 * Goal: before the sensor has sent anything, none of the four values
	 *       counts as valid (AC: 3, no value shown as live without an
	 *       update).
	 * Boundaries: only the validity flags; formatting and sensor-lost
	 *       are separate tests. Does not cover values that went stale
	 *       after arriving (Task 3).
	 * Arrange: head_unit has just started; no sensor value has arrived.
	 * Act: take the view a moment after start (1 s).
	 * Assert: apparent speed, apparent angle, true speed and true
	 *       direction are each individually invalid -- all four checked,
	 *       so one field wrongly defaulting to valid fails the test.
	 */
	struct wind_view view = wind_state_view(&fixture->state, 1000);

	assert_all_fields_invalid(&view);
}

ZTEST_F(wind_state, test_view_before_any_update_reports_sensor_lost)
{
	/*
	 * Goal: the sensor-lost indicator is on when nothing has ever been
	 *       received (AC: 3).
	 * Boundaries: only the sensor-lost flag. Does not cover losing a
	 *       sensor that was sending (Task 3).
	 * Arrange: head_unit has just started; no sensor value has arrived.
	 * Act: take the view a moment after start (1 s).
	 * Assert: sensor-lost is true.
	 */
	struct wind_view view = wind_state_view(&fixture->state, 1000);

	zassert_true(view.sensor_lost);
}

ZTEST_F(wind_state, test_view_before_any_update_formats_every_field_as_dashes)
{
	/*
	 * Goal: what the screen prints for a value we don't have is "--",
	 *       never "0", "0.0" or an empty string (AC: 3, 5).
	 * Boundaries: only the formatted text of the four fields; number
	 *       formatting (knots, port/starboard, bearing) is Task 2.
	 * Arrange: head_unit has just started; no sensor value has arrived.
	 * Act: take the view a moment after start (1 s).
	 * Assert: each of the four formatted fields equals exactly "--"
	 *       (string equality, so "--.-" or "-- kn" fail).
	 */
	struct wind_view view = wind_state_view(&fixture->state, 1000);

	for (size_t i = 0; i < ARRAY_SIZE(all_fields); i++) {
		zassert_str_equal(view.field[all_fields[i]].text, "--",
				  "field %d shows \"%s\"", all_fields[i],
				  view.field[all_fields[i]].text);
	}
}

/* --- Boundaries of "now" --- */

ZTEST_F(wind_state, test_view_before_any_update_is_lost_at_time_zero)
{
	/*
	 * Goal: at the boot instant itself there is no grace period in
	 *       which the display claims a live sensor.
	 * Boundaries: only time 0; the ordinary "moment after start" case is
	 *       covered above.
	 * Arrange: head_unit at the boot instant; nothing received.
	 * Act: take the view at time 0 -- a naive "now - last_rx < 3 s"
	 *       check with last_rx = 0 would wrongly call this live.
	 * Assert: sensor-lost is true and all four fields are invalid.
	 */
	struct wind_view view = wind_state_view(&fixture->state, 0);

	zassert_true(view.sensor_lost);
	assert_all_fields_invalid(&view);
}

ZTEST_F(wind_state, test_view_before_any_update_is_lost_long_after_boot)
{
	/*
	 * Goal: after a long uptime the display still reports no wind -- the
	 *       never-received state doesn't age into a live one.
	 * Boundaries: only very large uptimes; nothing received.
	 * Arrange: head_unit has been on for 60 days without a sensor --
	 *       past the point where a 32-bit millisecond counter wraps
	 *       (~49.7 days), so a truncated time type shows up here.
	 * Act: take the view at 60 days of uptime.
	 * Assert: sensor-lost is true and all four fields are invalid.
	 */
	const int64_t sixty_days_ms = 60LL * 24 * 60 * 60 * 1000;

	zassert_true(sixty_days_ms > UINT32_MAX, "must cross a 32-bit ms wrap");

	struct wind_view view = wind_state_view(&fixture->state, sixty_days_ms);

	zassert_true(view.sensor_lost);
	assert_all_fields_invalid(&view);
}

/* --- Redraw log line (what Renode tests read) --- */

ZTEST_F(wind_state, test_log_line_for_empty_view_lists_all_fields_dashed_and_lost)
{
	/*
	 * Goal: the line the wind page logs on each redraw describes the
	 *       no-wind view exactly as the Renode scenarios expect.
	 * Boundaries: only the empty view's line; lines carrying values come
	 *       with Task 2. Does not test that the UI actually logs it on
	 *       redraw (the boot scenario does).
	 * Arrange: the view of a head_unit that has received nothing.
	 * Act: render that view as its log line.
	 * Assert: the line equals exactly
	 *       "AWS=-- AWA=-- TWS=-- TWD=-- LOST=1" -- whole-string equality,
	 *       so field order and spacing are pinned to what
	 *       renode/tests/common.resource matches on. The "wind_ui: "
	 *       prefix comes from the UI's Zephyr log module, not from here.
	 */
	struct wind_view view = wind_state_view(&fixture->state, 1000);
	char line[64];

	wind_view_log_line(&view, line, sizeof(line));

	zassert_str_equal(line, "AWS=-- AWA=-- TWS=-- TWD=-- LOST=1",
			  "logged \"%s\"", line);
}
