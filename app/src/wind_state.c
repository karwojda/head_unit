#include "wind_state.h"

#include <stdio.h>
#include <string.h>

void wind_state_init(struct wind_state *state)
{
	memset(state, 0, sizeof(*state));
}

struct wind_view wind_state_view(const struct wind_state *state, int64_t now_ms)
{
	struct wind_view view = { .sensor_lost = true };

	(void)state;
	(void)now_ms;

	for (int i = 0; i < WIND_FIELD_COUNT; i++) {
		view.field[i].valid = false;
		strcpy(view.field[i].text, "--");
	}

	return view;
}

void wind_view_log_line(const struct wind_view *view, char *buf, size_t len)
{
	snprintf(buf, len, "AWS=%s AWA=%s TWS=%s TWD=%s LOST=%d",
		 view->field[WIND_AWS].text, view->field[WIND_AWA].text,
		 view->field[WIND_TWS].text, view->field[WIND_TWD].text,
		 view->sensor_lost ? 1 : 0);
}
