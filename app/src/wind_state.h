/*
 * Latest wind values from the remembered sensor and what the wind page
 * should show for them. No Zephyr, grvl or Bluetooth dependencies, so it
 * is unit-tested on native_sim.
 */
#ifndef HEAD_UNIT_WIND_STATE_H_
#define HEAD_UNIT_WIND_STATE_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum wind_field {
	WIND_AWS,	/* apparent wind speed */
	WIND_AWA,	/* apparent wind angle */
	WIND_TWS,	/* true wind speed */
	WIND_TWD,	/* true wind direction */
	WIND_FIELD_COUNT,
};

struct wind_field_view {
	bool valid;
	char text[8];
};

/* Apparent wind for the dial: degrees clockwise from the bow, 0-359. */
struct wind_dial_view {
	bool valid;
	uint16_t deg;
};

struct wind_view {
	struct wind_field_view field[WIND_FIELD_COUNT];
	struct wind_dial_view awa_dial;
	bool sensor_lost;
};

struct wind_state {
	/* Latest value per field, in ESS units; valid once received. */
	uint16_t raw[WIND_FIELD_COUNT];
	bool received[WIND_FIELD_COUNT];
};

void wind_state_init(struct wind_state *state);

/*
 * A value received from the sensor, in ESS units: 0.01 m/s for speeds,
 * 0.01 deg for angles (0-359.99). Returns 0, or -EINVAL for an unknown
 * field or an angle out of range.
 */
int wind_state_update(struct wind_state *state, enum wind_field field, uint16_t raw,
		      int64_t now_ms);

struct wind_view wind_state_view(const struct wind_state *state, int64_t now_ms);

/* The wind page's redraw log line for a view, without the log prefix. */
void wind_view_log_line(const struct wind_view *view, char *buf, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* HEAD_UNIT_WIND_STATE_H_ */
