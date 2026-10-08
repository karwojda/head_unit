/*
 * Task 2: live wind values -- updates, conversions, the formatted view and
 * log line, and the ESS notification payload.
 */
#include <errno.h>

#include <zephyr/ztest.h>

#include "ess_wind.h"
#include "wind_state.h"

/* ===================================================================
 * Updates: a received value becomes what the page shows (AC 1)
 * =================================================================== */

/* Requirements' example values, in ESS units (0.01 m/s, 0.01 deg). */
#define AWS_5_14_MPS 514
#define AWS_6_17_MPS 617
#define AWA_45_DEG   4500
#define TWS_4_22_MPS 422
#define TWD_270_DEG  27000

#define RECEIVED_MS 1000
#define DRAWN_MS    1500
#define LATER_MS    2000
#define REDRAWN_MS  2500

struct wind_update_fixture {
	struct wind_state state;
};

static void *wind_update_setup(void)
{
	static struct wind_update_fixture fixture;

	return &fixture;
}

static void wind_update_before(void *f)
{
	struct wind_update_fixture *fixture = f;

	wind_state_init(&fixture->state);
}

ZTEST_SUITE(wind_update, NULL, wind_update_setup, wind_update_before, NULL, NULL);

static void assert_field_dashed(const struct wind_view *view, enum wind_field field)
{
	zassert_false(view->field[field].valid, "field %d valid", field);
	zassert_str_equal(view->field[field].text, "--", "field %d shows \"%s\"", field,
			  view->field[field].text);
}

/*
 * Shared Arrange: a freshly initialised wind state, as
 * in the wind_state suite. Times stay well inside the 3 s stale timeout
 * (updates at 1.0 s, view at 1.5 s) so Task 3's staleness never applies.
 * Values are the requirements' examples: AWS 5.14 m/s ("10.0"), AWA 45 deg
 * ("45S"), TWS 4.22 m/s ("8.2"), TWD 270 deg ("270").
 */

ZTEST_F(wind_update, test_update_makes_that_field_valid)
{
	/*
	 * Goal: a value received from the sensor is what the page shows for
	 *       that field (AC 1).
	 * Boundaries: one field, AWS; conversion edge cases are the
	 *       wind_speed suite. Does not cover the other fields (next test).
	 * Arrange: head_unit has received nothing yet.
	 * Act: the sensor's apparent wind speed of 5.14 m/s arrives at 1.0 s;
	 *       the page is drawn at 1.5 s.
	 * Assert: AWS is valid and reads exactly "10.0" -- the update is
	 *       accepted (reports success) and both flag and text change,
	 *       so a valid flag with stale "--" text fails.
	 */
	int rc = wind_state_update(&fixture->state, WIND_AWS, AWS_5_14_MPS, RECEIVED_MS);
	struct wind_view view = wind_state_view(&fixture->state, DRAWN_MS);

	zassert_equal(rc, 0, "update rejected: %d", rc);
	zassert_true(view.field[WIND_AWS].valid);
	zassert_str_equal(view.field[WIND_AWS].text, "10.0", "AWS shows \"%s\"",
			  view.field[WIND_AWS].text);
}

ZTEST_F(wind_update, test_update_of_one_field_leaves_others_dashed)
{
	/*
	 * Goal: fields are independent -- one value arriving never makes
	 *       another look live (AC 1; groundwork for AC 5, true wind "--"
	 *       when only apparent is sent).
	 * Boundaries: only the three untouched fields; the updated one is
	 *       covered above.
	 * Arrange: head_unit has received nothing yet.
	 * Act: only the apparent wind angle (45 deg) arrives.
	 * Assert: AWA is "45S"; AWS, TWS and TWD are each invalid and read
	 *       "--" -- each checked by name, so one leaking field fails.
	 */
	zassert_ok(wind_state_update(&fixture->state, WIND_AWA, AWA_45_DEG, RECEIVED_MS));

	struct wind_view view = wind_state_view(&fixture->state, DRAWN_MS);

	zassert_true(view.field[WIND_AWA].valid);
	zassert_str_equal(view.field[WIND_AWA].text, "45S", "AWA shows \"%s\"",
			  view.field[WIND_AWA].text);
	assert_field_dashed(&view, WIND_AWS);
	assert_field_dashed(&view, WIND_TWS);
	assert_field_dashed(&view, WIND_TWD);
}

ZTEST_F(wind_update, test_first_value_clears_sensor_lost)
{
	/*
	 * Goal: as soon as the sensor is heard from, sensor-lost goes off
	 *       (AC 1, 3: "lost" means nothing received within the timeout).
	 *       Renamed from "all four received": a sensor sending apparent
	 *       wind only (AC 5) is not lost -- its true wind shows "--".
	 * Boundaries: the off direction only; turning it back on after 3 s of
	 *       silence is Task 3.
	 * Arrange: head_unit has received nothing; sensor-lost is on.
	 * Act: one value (AWS) arrives.
	 * Assert: sensor-lost is off.
	 */
	zassert_true(wind_state_view(&fixture->state, RECEIVED_MS).sensor_lost,
		     "precondition: lost before anything arrives");

	zassert_ok(wind_state_update(&fixture->state, WIND_AWS, AWS_5_14_MPS, RECEIVED_MS));

	zassert_false(wind_state_view(&fixture->state, DRAWN_MS).sensor_lost);
}

ZTEST_F(wind_update, test_newer_value_replaces_shown_value)
{
	/*
	 * Goal: each new value replaces the shown one (AC 1, "each new value
	 *       replaces the shown one").
	 * Boundaries: same field twice; cross-field interference is the
	 *       next test.
	 * Arrange: AWS 5.14 m/s ("10.0") has been shown.
	 * Act: AWS 6.17 m/s arrives half a second later.
	 * Assert: AWS reads exactly "12.0" -- not "10.0" (ignored) nor "11.0"
	 *       (averaged).
	 */
	zassert_ok(wind_state_update(&fixture->state, WIND_AWS, AWS_5_14_MPS, RECEIVED_MS));
	zassert_str_equal(wind_state_view(&fixture->state, DRAWN_MS).field[WIND_AWS].text, "10.0",
			  "precondition: first value shown");

	zassert_ok(wind_state_update(&fixture->state, WIND_AWS, AWS_6_17_MPS, LATER_MS));

	struct wind_view view = wind_state_view(&fixture->state, REDRAWN_MS);

	zassert_str_equal(view.field[WIND_AWS].text, "12.0", "AWS shows \"%s\"",
			  view.field[WIND_AWS].text);
}

ZTEST_F(wind_update, test_each_field_keeps_its_own_latest_value)
{
	/*
	 * Goal: interleaved updates land in their own fields -- a sensor
	 *       notifies the four characteristics one after another.
	 * Boundaries: speed fields only (same units, so a mix-up wouldn't
	 *       show as a format error); angles are covered by the log line
	 *       tests.
	 * Arrange: head_unit has received nothing.
	 * Act: AWS 5.14 m/s, then TWS 4.22 m/s, then AWS 6.17 m/s arrive.
	 * Assert: AWS reads "12.0" and TWS reads "8.2" -- TWS not overwritten
	 *       by the later AWS, AWS not stuck at its first value.
	 */
	zassert_ok(wind_state_update(&fixture->state, WIND_AWS, AWS_5_14_MPS, RECEIVED_MS));
	zassert_ok(wind_state_update(&fixture->state, WIND_TWS, TWS_4_22_MPS, RECEIVED_MS));
	zassert_ok(wind_state_update(&fixture->state, WIND_AWS, AWS_6_17_MPS, LATER_MS));

	struct wind_view view = wind_state_view(&fixture->state, REDRAWN_MS);

	zassert_str_equal(view.field[WIND_AWS].text, "12.0", "AWS shows \"%s\"",
			  view.field[WIND_AWS].text);
	zassert_str_equal(view.field[WIND_TWS].text, "8.2", "TWS shows \"%s\"",
			  view.field[WIND_TWS].text);
}

ZTEST_F(wind_update, test_update_for_unknown_field_is_rejected)
{
	/*
	 * Goal: an update for something that isn't one of the four fields
	 *       fails loudly and changes nothing -- a bad mapping upstream
	 *       must not corrupt the page.
	 * Boundaries: the field id only; bad values (out-of-range angles)
	 *       are the angle suites. Logging the error is the caller's job.
	 * Arrange: AWS 5.14 m/s has been shown.
	 * Act: an update for a field id past the last of the four arrives.
	 * Assert: the update reports an invalid-argument error; the view is
	 *       unchanged -- AWS still "10.0", the other three still "--".
	 */
	zassert_ok(wind_state_update(&fixture->state, WIND_AWS, AWS_5_14_MPS, RECEIVED_MS));

	int rc = wind_state_update(&fixture->state, WIND_FIELD_COUNT, AWS_6_17_MPS, LATER_MS);
	struct wind_view view = wind_state_view(&fixture->state, REDRAWN_MS);

	zassert_equal(rc, -EINVAL, "update returned %d", rc);
	zassert_str_equal(view.field[WIND_AWS].text, "10.0", "AWS shows \"%s\"",
			  view.field[WIND_AWS].text);
	assert_field_dashed(&view, WIND_AWA);
	assert_field_dashed(&view, WIND_TWS);
	assert_field_dashed(&view, WIND_TWD);
}

/* ===================================================================
 * Speed: 0.01 m/s -> knots to 0.1 (AC 6)
 * =================================================================== */

ZTEST_SUITE(wind_speed, NULL, NULL, NULL, NULL, NULL);

/* What the page shows for one field after a single value arrived. */
static struct wind_field_view field_shown(enum wind_field field, uint16_t raw)
{
	struct wind_state state;

	wind_state_init(&state);
	zassert_ok(wind_state_update(&state, field, raw, RECEIVED_MS));
	return wind_state_view(&state, DRAWN_MS).field[field];
}

/*
 * Shared Arrange (helper in red): a fresh wind state that has received
 * one speed value; the test reads back what the page shows for it. Same
 * logic, different data -- kept as separate named tests so each case is
 * identifiable in the output (ztest has no parameterised tests).
 * 1 m/s = 1.943844 kn.
 */

ZTEST(wind_speed, test_5_14_mps_shows_10_0_kn)
{
	/*
	 * Goal: the requirements' example converts exactly (AC 6).
	 * Boundaries: AWS only; TWS is the next test.
	 * Arrange: the sensor has sent apparent wind speed 5.14 m/s (9.991 kn).
	 * Act: read what the page shows for AWS.
	 * Assert: exactly "10.0" -- one decimal, no unit, rounded up from
	 *       9.991 (truncation would give "9.9").
	 */
	struct wind_field_view aws = field_shown(WIND_AWS, AWS_5_14_MPS);

	zassert_str_equal(aws.text, "10.0", "AWS shows \"%s\"", aws.text);
}

ZTEST(wind_speed, test_true_speed_converts_like_apparent)
{
	/*
	 * Goal: true wind speed uses the same conversion and format as
	 *       apparent (AC 6) -- no field-specific scaling.
	 * Boundaries: TWS with the same input as the AWS test; rounding
	 *       edges are covered once, on AWS.
	 * Arrange: the sensor has sent true wind speed 5.14 m/s.
	 * Act: read what the page shows for TWS.
	 * Assert: exactly "10.0", identical to AWS for the same input.
	 */
	struct wind_field_view tws = field_shown(WIND_TWS, AWS_5_14_MPS);

	zassert_str_equal(tws.text, "10.0", "TWS shows \"%s\"", tws.text);
}

ZTEST(wind_speed, test_zero_speed_shows_0_0_not_dashes)
{
	/*
	 * Goal: calm is a real reading -- 0 m/s shows "0.0", never "--"
	 *       (which means "no value") and never "0" (AC 5, 6).
	 * Boundaries: the lower edge only.
	 * Arrange: the sensor has sent apparent wind speed 0.
	 * Act: read what the page shows for AWS.
	 * Assert: AWS is valid and reads exactly "0.0" -- a zero raw value
	 *       must not be mistaken for "nothing received".
	 */
	struct wind_field_view aws = field_shown(WIND_AWS, 0);

	zassert_true(aws.valid, "calm taken for no value");
	zassert_str_equal(aws.text, "0.0", "AWS shows \"%s\"", aws.text);
}

ZTEST(wind_speed, test_speed_rounds_to_nearest_tenth_at_half_step)
{
	/*
	 * Goal: rounding is to the nearest tenth, on both sides of a .x5
	 *       boundary (AC 6, "rounded as decided").
	 * Boundaries: one boundary near 5 kn, a common racing wind; other
	 *       boundaries behave the same.
	 * Arrange: two fresh states -- one has received 2.54 m/s (4.937 kn),
	 *       the other 2.55 m/s (4.957 kn), either side of 4.95 kn.
	 * Act: read AWS for each.
	 * Assert: exactly "4.9" and "5.0" -- truncation gives "4.9" for both,
	 *       always rounding up gives "5.0" for both.
	 */
	struct wind_field_view below = field_shown(WIND_AWS, 254);
	struct wind_field_view above = field_shown(WIND_AWS, 255);

	zassert_str_equal(below.text, "4.9", "2.54 m/s shows \"%s\"", below.text);
	zassert_str_equal(above.text, "5.0", "2.55 m/s shows \"%s\"", above.text);
}

ZTEST(wind_speed, test_max_raw_speed_formats_without_truncation)
{
	/*
	 * Goal: the largest value the sensor can send still prints whole --
	 *       a short text buffer or 16-bit overflow in the conversion would
	 *       show a wrong number.
	 * Boundaries: the upper edge of the uint16 only. The Bluetooth SIG
	 *       definition of the wind speed characteristics has no "value not
	 *       known" marker, so 0xFFFF is a real 655.35 m/s.
	 * Arrange: the sensor has sent apparent wind speed 655.35 m/s (raw
	 *       0xFFFF, 1273.898 kn).
	 * Act: read what the page shows for AWS.
	 * Assert: exactly "1273.9" -- all six characters, no truncation.
	 */
	struct wind_field_view aws = field_shown(WIND_AWS, UINT16_MAX);

	zassert_str_equal(aws.text, "1273.9", "AWS shows \"%s\"", aws.text);
}

/* ===================================================================
 * Apparent angle: 0.01 deg relative to bow -> 0-180 port/starboard (AC 6)
 * =================================================================== */

ZTEST_SUITE(wind_awa, NULL, NULL, NULL, NULL, NULL);

/* What the page shows for AWA after a single direction (0.01 deg) arrived. */
static struct wind_field_view awa_shown(uint16_t raw)
{
	return field_shown(WIND_AWA, raw);
}

/*
 * Shared Arrange (helper in red, like speed_shown): a fresh state that
 * has received one apparent wind direction. Raw values are 0.01 deg
 * clockwise from the bow (Bluetooth SIG: valid 0-359.99; 45 deg to port
 * is sent as 315). Shown as 1-179 with S/P, "0" and "180" with no side.
 */

ZTEST(wind_awa, test_270_deg_shows_90_port)
{
	/*
	 * Goal: wind from the port side reads as port (AC 6 example; Task 2
	 *       regret 3, sides swapped).
	 * Boundaries: one port value; rounding edges are separate tests.
	 * Arrange: the sensor has sent apparent wind direction 270 deg.
	 * Act: read what the page shows for AWA.
	 * Assert: exactly "90P" -- not "270", not "90S", not "90".
	 */
	struct wind_field_view awa = awa_shown(27000);

	zassert_str_equal(awa.text, "90P", "AWA shows \"%s\"", awa.text);
}

ZTEST(wind_awa, test_45_deg_shows_45_starboard)
{
	/*
	 * Goal: wind from the starboard side reads as starboard (AC 6).
	 * Boundaries: one starboard value.
	 * Arrange: the sensor has sent apparent wind direction 45 deg.
	 * Act: read what the page shows for AWA.
	 * Assert: exactly "45S".
	 */
	struct wind_field_view awa = awa_shown(AWA_45_DEG);

	zassert_str_equal(awa.text, "45S", "AWA shows \"%s\"", awa.text);
}

ZTEST(wind_awa, test_359_6_deg_shows_0_never_360)
{
	/*
	 * Goal: a direction that rounds up to a full turn shows dead ahead
	 *       (AC 6 example: 359.6 -> 0, never 360).
	 * Boundaries: the wrap at 360 only; the side near the bow is the
	 *       next tests.
	 * Arrange: the sensor has sent 359.6 deg; separately 359.99 deg, the
	 *       largest valid value.
	 * Act: read AWA for each.
	 * Assert: both exactly "0" -- not "360", "0P" or "360P".
	 */
	struct wind_field_view near_full_turn = awa_shown(35960);
	struct wind_field_view largest_valid = awa_shown(35999);

	zassert_str_equal(near_full_turn.text, "0", "359.6 deg shows \"%s\"",
			  near_full_turn.text);
	zassert_str_equal(largest_valid.text, "0", "359.99 deg shows \"%s\"",
			  largest_valid.text);
}

ZTEST(wind_awa, test_dead_ahead_and_dead_astern_have_no_side)
{
	/*
	 * Goal: wind from dead ahead or dead astern has no side (decided:
	 *       "0" and "180").
	 * Boundaries: the exact values 0 and 180.00 deg; values that round
	 *       to them are covered by the wrap and 180 tests.
	 * Arrange: two fresh states, one sent 0 deg, the other 180 deg.
	 * Act: read AWA for each.
	 * Assert: exactly "0" and exactly "180".
	 */
	struct wind_field_view ahead = awa_shown(0);
	struct wind_field_view astern = awa_shown(18000);

	zassert_str_equal(ahead.text, "0", "0 deg shows \"%s\"", ahead.text);
	zassert_str_equal(astern.text, "180", "180 deg shows \"%s\"", astern.text);
}

ZTEST(wind_awa, test_angles_rounding_to_180_show_180)
{
	/*
	 * Goal: either side of dead astern, a direction that rounds to 180
	 *       shows "180" -- the side decision is made after rounding, not
	 *       before.
	 * Boundaries: the 180 edge only.
	 * Arrange: two fresh states, one sent 179.6 deg, the other 180.4 deg.
	 * Act: read AWA for each.
	 * Assert: both exactly "180" -- deciding the side before rounding
	 *       would give "180S" and "180P".
	 */
	struct wind_field_view starboard_of_astern = awa_shown(17960);
	struct wind_field_view port_of_astern = awa_shown(18040);

	zassert_str_equal(starboard_of_astern.text, "180", "179.6 deg shows \"%s\"",
			  starboard_of_astern.text);
	zassert_str_equal(port_of_astern.text, "180", "180.4 deg shows \"%s\"",
			  port_of_astern.text);
}

ZTEST(wind_awa, test_near_bow_rounding_keeps_correct_side)
{
	/*
	 * Goal: just off the bow, the rounded angle keeps its true side.
	 * Boundaries: the 0 edge, from both sides.
	 * Arrange: two fresh states, one sent 0.6 deg, the other 359.4 deg.
	 * Act: read AWA for each.
	 * Assert: exactly "1S" and exactly "1P" -- truncating instead of
	 *       rounding gives "0" for the first, and taking 360 minus the
	 *       raw value before truncating gives "0P" for the second.
	 */
	struct wind_field_view starboard_of_bow = awa_shown(60);
	struct wind_field_view port_of_bow = awa_shown(35940);

	zassert_str_equal(starboard_of_bow.text, "1S", "0.6 deg shows \"%s\"",
			  starboard_of_bow.text);
	zassert_str_equal(port_of_bow.text, "1P", "359.4 deg shows \"%s\"", port_of_bow.text);
}

ZTEST(wind_awa, test_out_of_range_angle_is_rejected)
{
	/*
	 * Goal: a direction outside 0-359.99 deg is invalid sensor data and
	 *       is never shown -- a clamped or wrapped value would be a
	 *       confidently wrong number.
	 * Boundaries: apparent direction only (TWD has its own test). Logging
	 *       the error is the caller's job (wind_chan logs a rejected
	 *       update).
	 * Arrange: fresh states; one sent 360.00 deg (just past the range),
	 *       one sent 655.35 deg (the largest raw value).
	 * Act: deliver each update, then read AWA.
	 * Assert: each update reports an invalid-argument error, and AWA
	 *       stays invalid and "--" -- not "0" (wrapped).
	 */
	static const uint16_t out_of_range[] = { 36000, UINT16_MAX };

	for (size_t i = 0; i < ARRAY_SIZE(out_of_range); i++) {
		struct wind_state state;

		wind_state_init(&state);

		int rc = wind_state_update(&state, WIND_AWA, out_of_range[i], RECEIVED_MS);
		struct wind_field_view awa = wind_state_view(&state, DRAWN_MS).field[WIND_AWA];

		zassert_equal(rc, -EINVAL, "raw %u: update returned %d", out_of_range[i], rc);
		zassert_false(awa.valid, "raw %u shown as valid", out_of_range[i]);
		zassert_str_equal(awa.text, "--", "raw %u shows \"%s\"", out_of_range[i],
				  awa.text);
	}
}

ZTEST(wind_awa, test_view_gives_dial_angle_clockwise_from_bow)
{
	/*
	 * Goal: the AWA dial gets a number to point with, not text to parse
	 *       (screen content decision: a dial showing apparent wind angle).
	 *       Renamed from "signed by side": clockwise 0-359 from the bow
	 *       is what a dial needle needs, and it is what the sensor sends.
	 * Boundaries: the dial value only; drawing the needle is the UI's
	 *       job (screenshots, human review).
	 * Arrange: fresh states sent 45 deg, 270 deg and 359.6 deg; and one
	 *       with no apparent direction received.
	 * Act: read the view's dial angle for each.
	 * Assert: 45, 270 and 0 degrees exactly (same rounding and wrap as
	 *       the text); with nothing received the dial angle is marked
	 *       invalid, so the UI can hide the needle.
	 */
	static const struct {
		uint16_t raw;
		uint16_t dial_deg;
	} cases[] = {
		{ AWA_45_DEG, 45 },
		{ 27000, 270 },
		{ 35960, 0 },
	};

	for (size_t i = 0; i < ARRAY_SIZE(cases); i++) {
		struct wind_state state;

		wind_state_init(&state);
		zassert_ok(wind_state_update(&state, WIND_AWA, cases[i].raw, RECEIVED_MS));

		struct wind_view view = wind_state_view(&state, DRAWN_MS);

		zassert_true(view.awa_dial.valid, "raw %u: no dial angle", cases[i].raw);
		zassert_equal(view.awa_dial.deg, cases[i].dial_deg, "raw %u: dial at %u",
			      cases[i].raw, view.awa_dial.deg);
	}

	struct wind_state nothing;

	wind_state_init(&nothing);
	zassert_false(wind_state_view(&nothing, DRAWN_MS).awa_dial.valid,
		      "dial angle with no apparent direction received");
}

/* ===================================================================
 * True direction: 0.01 deg compass bearing -> 0-359 (AC 6)
 * =================================================================== */

ZTEST_SUITE(wind_twd, NULL, NULL, NULL, NULL, NULL);

/*
 * Shared Arrange: field_shown(WIND_TWD, raw), as for the other suites.
 * Raw values are 0.01 deg clockwise from geographic north, the direction
 * the wind comes from (Bluetooth SIG: valid 0-359.99). Shown as a
 * three-digit bearing (decided).
 */

ZTEST(wind_twd, test_270_deg_shows_270)
{
	/*
	 * Goal: true wind direction is a compass bearing, not port/starboard
	 *       (AC 6) -- the apparent-angle rule must not leak into it.
	 * Boundaries: one bearing past 180.
	 * Arrange: the sensor has sent true wind direction 270 deg (west).
	 * Act: read what the page shows for TWD.
	 * Assert: exactly "270" -- not "90P".
	 */
	struct wind_field_view twd = field_shown(WIND_TWD, TWD_270_DEG);

	zassert_str_equal(twd.text, "270", "TWD shows \"%s\"", twd.text);
}

ZTEST(wind_twd, test_359_6_deg_shows_000_never_360)
{
	/*
	 * Goal: a bearing that rounds up to a full turn shows north (AC 6:
	 *       never 360). Renamed: with three-digit bearings north is "000".
	 * Boundaries: the wrap only.
	 * Arrange: the sensor has sent 359.6 deg.
	 * Act: read what the page shows for TWD.
	 * Assert: exactly "000" -- not "360", not "0".
	 */
	struct wind_field_view twd = field_shown(WIND_TWD, 35960);

	zassert_str_equal(twd.text, "000", "TWD shows \"%s\"", twd.text);
}

ZTEST(wind_twd, test_small_bearing_format)
{
	/*
	 * Goal: bearings are always three digits, as sailors read them
	 *       (decided: 5 deg shows "005").
	 * Boundaries: one- and two-digit bearings.
	 * Arrange: fresh states sent 5 deg and 45 deg.
	 * Act: read TWD for each.
	 * Assert: exactly "005" and "045".
	 */
	struct wind_field_view one_digit = field_shown(WIND_TWD, 500);
	struct wind_field_view two_digits = field_shown(WIND_TWD, 4500);

	zassert_str_equal(one_digit.text, "005", "5 deg shows \"%s\"", one_digit.text);
	zassert_str_equal(two_digits.text, "045", "45 deg shows \"%s\"", two_digits.text);
}

ZTEST(wind_twd, test_out_of_range_bearing_is_rejected)
{
	/*
	 * Goal: a bearing outside 0-359.99 deg is invalid sensor data and is
	 *       never shown -- same rule as the apparent direction.
	 * Boundaries: true direction only (AWA has its own test).
	 * Arrange: fresh states; one sent 360.00 deg, one 655.35 deg.
	 * Act: deliver each update, then read TWD.
	 * Assert: each update reports an invalid-argument error, and TWD
	 *       stays invalid and "--" -- not "000" (wrapped).
	 */
	static const uint16_t out_of_range[] = { 36000, UINT16_MAX };

	for (size_t i = 0; i < ARRAY_SIZE(out_of_range); i++) {
		struct wind_state state;

		wind_state_init(&state);

		int rc = wind_state_update(&state, WIND_TWD, out_of_range[i], RECEIVED_MS);
		struct wind_field_view twd = wind_state_view(&state, DRAWN_MS).field[WIND_TWD];

		zassert_equal(rc, -EINVAL, "raw %u: update returned %d", out_of_range[i], rc);
		zassert_false(twd.valid, "raw %u shown as valid", out_of_range[i]);
		zassert_str_equal(twd.text, "--", "raw %u shows \"%s\"", out_of_range[i],
				  twd.text);
	}
}

/* ===================================================================
 * Log line with values (what live_wind.robot reads)
 * =================================================================== */

/* Same fresh wind state per test as wind_update. */
struct wind_log_line_fixture {
	struct wind_state state;
};

static void *wind_log_line_setup(void)
{
	static struct wind_log_line_fixture fixture;

	return &fixture;
}

static void wind_log_line_before(void *f)
{
	struct wind_log_line_fixture *fixture = f;

	wind_state_init(&fixture->state);
}

ZTEST_SUITE(wind_log_line, NULL, wind_log_line_setup, wind_log_line_before, NULL, NULL);

static void assert_log_line(const struct wind_state *state, const char *expected)
{
	struct wind_view view = wind_state_view(state, DRAWN_MS);
	char line[64];

	wind_view_log_line(&view, line, sizeof(line));
	zassert_str_equal(line, expected, "logged \"%s\"", line);
}

ZTEST_F(wind_log_line, test_log_line_with_all_values)
{
	/*
	 * Goal: with all four values in, the redraw log line is exactly what
	 *       live_wind.robot matches (AC 1).
	 * Boundaries: the requirements' example values only; formatting edge
	 *       cases are the speed/angle suites.
	 * Arrange: AWS 5.14 m/s, AWA 45 deg, TWS 4.22 m/s and TWD 270 deg have
	 *       all arrived.
	 * Act: render the view as its log line.
	 * Assert: the line equals exactly
	 *       "AWS=10.0kn AWA=45S TWS=8.2kn TWD=270 LOST=0" -- units only on
	 *       speeds, field order and spacing as the robot pattern expects.
	 */
	zassert_ok(wind_state_update(&fixture->state, WIND_AWS, AWS_5_14_MPS, RECEIVED_MS));
	zassert_ok(wind_state_update(&fixture->state, WIND_AWA, AWA_45_DEG, RECEIVED_MS));
	zassert_ok(wind_state_update(&fixture->state, WIND_TWS, TWS_4_22_MPS, RECEIVED_MS));
	zassert_ok(wind_state_update(&fixture->state, WIND_TWD, TWD_270_DEG, RECEIVED_MS));

	assert_log_line(&fixture->state, "AWS=10.0kn AWA=45S TWS=8.2kn TWD=270 LOST=0");
}

ZTEST_F(wind_log_line, test_log_line_with_some_fields_missing)
{
	/*
	 * Goal: a partial view logs received fields formatted and the rest as
	 *       "--" with no unit (AC 1, 5).
	 * Boundaries: one mix (apparent in, true missing); the all-missing
	 *       line is Task 1's test.
	 * Arrange: only AWS 5.14 m/s and AWA 45 deg have arrived.
	 * Act: render the view as its log line.
	 * Assert: the line equals exactly
	 *       "AWS=10.0kn AWA=45S TWS=-- TWD=-- LOST=0" -- "--" never gets
	 *       "kn" appended, and sensor-lost is off.
	 */
	zassert_ok(wind_state_update(&fixture->state, WIND_AWS, AWS_5_14_MPS, RECEIVED_MS));
	zassert_ok(wind_state_update(&fixture->state, WIND_AWA, AWA_45_DEG, RECEIVED_MS));

	assert_log_line(&fixture->state, "AWS=10.0kn AWA=45S TWS=-- TWD=-- LOST=0");
}

/* ===================================================================
 * ESS notification payload -> field + raw value (wind link, pure part)
 * =================================================================== */

ZTEST_SUITE(ess_wind, NULL, NULL, NULL, NULL, NULL);

/*
 * The pure part of the BLE link: a notification from one of the sensor's
 * characteristics (16-bit UUID + payload bytes) becomes a wind field and
 * a raw value, or is refused. No Bluetooth dependencies, so it runs here.
 * Bluetooth SIG: each wind characteristic is one uint16, little-endian.
 */

ZTEST(ess_wind, test_each_ess_uuid_maps_to_its_field)
{
	/*
	 * Goal: each wind characteristic lands in its own field -- a swap
	 *       would show true wind as apparent, or speed as angle.
	 * Boundaries: the mapping only; payload decoding is the next test.
	 * Arrange: the four characteristics: 0x2A72 apparent speed, 0x2A73
	 *       apparent direction, 0x2A70 true speed, 0x2A71 true direction,
	 *       each with a valid 2-byte payload.
	 * Act: decode each notification.
	 * Assert: each is accepted and maps to AWS, AWA, TWS, TWD
	 *       respectively -- all four checked, so one swapped pair fails.
	 */
	static const struct {
		uint16_t uuid;
		enum wind_field field;
	} cases[] = {
		{ ESS_UUID_APPARENT_WIND_SPEED, WIND_AWS },
		{ ESS_UUID_APPARENT_WIND_DIRECTION, WIND_AWA },
		{ ESS_UUID_TRUE_WIND_SPEED, WIND_TWS },
		{ ESS_UUID_TRUE_WIND_DIRECTION, WIND_TWD },
	};
	static const uint8_t payload[] = { 0x02, 0x02 };

	for (size_t i = 0; i < ARRAY_SIZE(cases); i++) {
		enum wind_field field = WIND_FIELD_COUNT;
		uint16_t raw;
		int rc = ess_wind_decode(cases[i].uuid, payload, sizeof(payload), &field, &raw);

		zassert_ok(rc, "0x%04X refused: %d", cases[i].uuid, rc);
		zassert_equal(field, cases[i].field, "0x%04X mapped to field %d", cases[i].uuid,
			      field);
	}
}

ZTEST(ess_wind, test_payload_decoded_little_endian)
{
	/*
	 * Goal: the value is read the way the sensor sends it, low byte
	 *       first.
	 * Boundaries: byte order only.
	 * Arrange: an apparent direction notification carrying 45.00 deg,
	 *       bytes 0x94 0x11 (4500). Asymmetric on purpose: read
	 *       big-endian it would be 37905.
	 * Act: decode it.
	 * Assert: the raw value is exactly 4500.
	 */
	static const uint8_t payload[] = { 0x94, 0x11 };
	enum wind_field field;
	uint16_t raw = 0;
	int rc = ess_wind_decode(ESS_UUID_APPARENT_WIND_DIRECTION, payload, sizeof(payload),
				 &field, &raw);

	zassert_ok(rc, "refused: %d", rc);
	zassert_equal(raw, 4500, "decoded %u", raw);
}

ZTEST(ess_wind, test_wrong_payload_length_is_rejected)
{
	/*
	 * Goal: a payload that isn't exactly two bytes is malformed and is
	 *       refused with an error, never half-read or padded.
	 * Boundaries: length only. Logging is the caller's job (the BLE
	 *       link logs <err>, so the Renode tests fail on it).
	 * Arrange: apparent speed notifications of 0, 1 and 3 bytes.
	 * Act: decode each.
	 * Assert: each returns the malformed-message error, distinct from
	 *       "not a wind characteristic".
	 */
	static const uint8_t payload[] = { 0x02, 0x02, 0x00 };
	static const size_t wrong_lengths[] = { 0, 1, 3 };

	for (size_t i = 0; i < ARRAY_SIZE(wrong_lengths); i++) {
		enum wind_field field;
		uint16_t raw;
		int rc = ess_wind_decode(ESS_UUID_APPARENT_WIND_SPEED, payload, wrong_lengths[i],
					 &field, &raw);

		zassert_equal(rc, -EMSGSIZE, "%zu bytes: returned %d", wrong_lengths[i], rc);
	}
}

ZTEST(ess_wind, test_unknown_uuid_is_ignored)
{
	/*
	 * Goal: a characteristic that isn't one of the four wind values is
	 *       ignored quietly -- the sensor may offer others (e.g. 0x2A6E
	 *       temperature, also Environmental Sensing).
	 * Boundaries: the result code only; the caller decides to ignore it.
	 * Arrange: a temperature notification with a valid 2-byte payload.
	 * Act: decode it.
	 * Assert: it returns the "not a wind characteristic" result, distinct
	 *       from the malformed-message error.
	 */
	static const uint16_t ESS_UUID_TEMPERATURE = 0x2A6E;
	static const uint8_t payload[] = { 0xF4, 0x01 };
	enum wind_field field;
	uint16_t raw;
	int rc = ess_wind_decode(ESS_UUID_TEMPERATURE, payload, sizeof(payload), &field, &raw);

	zassert_equal(rc, -ENOENT, "temperature returned %d", rc);
}
