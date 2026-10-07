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

struct wind_view {
	struct wind_field_view field[WIND_FIELD_COUNT];
	bool sensor_lost;
};

struct wind_state {
	/* Nothing received yet; per-field values arrive with Task 2. */
	char unused;
};

void wind_state_init(struct wind_state *state);

struct wind_view wind_state_view(const struct wind_state *state, int64_t now_ms);

/* The wind page's redraw log line for a view, without the log prefix. */
void wind_view_log_line(const struct wind_view *view, char *buf, size_t len);

#endif /* HEAD_UNIT_WIND_STATE_H_ */
