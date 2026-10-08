#include "wind_state.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>

void wind_state_init(struct wind_state *state)
{
	memset(state, 0, sizeof(*state));
}

int wind_state_update(struct wind_state *state, enum wind_field field, uint16_t raw,
		      int64_t now_ms)
{
	(void)now_ms;

	if ((unsigned int)field >= WIND_FIELD_COUNT) {
		return -EINVAL;
	}
	/* Directions are 0-359.99 deg; anything else is bad sensor data. */
	if ((field == WIND_AWA || field == WIND_TWD) && raw >= 36000) {
		return -EINVAL;
	}

	state->raw[field] = raw;
	state->received[field] = true;
	return 0;
}

/* 0.01 m/s -> knots, to 0.1 kn: 1 m/s = 1.943844 kn. */
static void format_speed(char *text, size_t len, uint16_t raw)
{
	uint32_t tenths = ((uint64_t)raw * 1943844 + 5000000) / 10000000;

	snprintf(text, len, "%u.%u", tenths / 10, tenths % 10);
}

/* 0.01 deg -> whole degrees 0-359; 359.5 and up wraps to 0. */
static unsigned int round_deg(uint16_t raw)
{
	return ((raw + 50) / 100) % 360;
}

/* 0.01 deg relative to the bow -> 0-180 with the side the wind comes from. */
static void format_apparent_angle(char *text, size_t len, uint16_t raw)
{
	unsigned int deg = round_deg(raw);

	if (deg == 0 || deg == 180) {
		snprintf(text, len, "%u", deg);
	} else if (deg < 180) {
		snprintf(text, len, "%uS", deg);
	} else {
		snprintf(text, len, "%uP", 360 - deg);
	}
}

/* 0.01 deg compass bearing -> three-digit 000-359. */
static void format_bearing(char *text, size_t len, uint16_t raw)
{
	snprintf(text, len, "%03u", round_deg(raw));
}

struct wind_view wind_state_view(const struct wind_state *state, int64_t now_ms)
{
	struct wind_view view = { .sensor_lost = true };

	(void)now_ms;

	for (int i = 0; i < WIND_FIELD_COUNT; i++) {
		struct wind_field_view *field = &view.field[i];

		field->valid = state->received[i];
		if (!field->valid) {
			strcpy(field->text, "--");
			continue;
		}

		view.sensor_lost = false;
		switch (i) {
		case WIND_AWS:
		case WIND_TWS:
			format_speed(field->text, sizeof(field->text), state->raw[i]);
			break;
		case WIND_AWA:
			format_apparent_angle(field->text, sizeof(field->text), state->raw[i]);
			view.awa_dial.valid = true;
			view.awa_dial.deg = round_deg(state->raw[i]);
			break;
		case WIND_TWD:
			format_bearing(field->text, sizeof(field->text), state->raw[i]);
			break;
		}
	}

	return view;
}

/* Knots only after a speed that has a value: "10.0kn", but "--". */
static const char *speed_unit(const struct wind_field_view *field)
{
	return field->valid ? "kn" : "";
}

void wind_view_log_line(const struct wind_view *view, char *buf, size_t len)
{
	snprintf(buf, len, "AWS=%s%s AWA=%s TWS=%s%s TWD=%s LOST=%d",
		 view->field[WIND_AWS].text, speed_unit(&view->field[WIND_AWS]),
		 view->field[WIND_AWA].text,
		 view->field[WIND_TWS].text, speed_unit(&view->field[WIND_TWS]),
		 view->field[WIND_TWD].text, view->sensor_lost ? 1 : 0);
}
