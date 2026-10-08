#include "wind_chan.h"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(wind_chan, CONFIG_APP_LOG_LEVEL);

ZBUS_CHAN_DEFINE(wind_chan, struct wind_msg, NULL, NULL, ZBUS_OBSERVERS_EMPTY,
		 ZBUS_MSG_INIT(0));

/* Every value in order, not just the latest: a sensor sends four in a row. */
ZBUS_MSG_SUBSCRIBER_DEFINE(wind_ui_sub);
ZBUS_CHAN_ADD_OBS(wind_chan, wind_ui_sub, 3);

void wind_chan_drain(struct wind_state *state)
{
	const struct zbus_channel *chan;
	struct wind_msg msg;

	while (zbus_sub_wait_msg(&wind_ui_sub, &chan, &msg, K_NO_WAIT) == 0) {
		int rc = wind_state_update(state, msg.field, msg.raw, k_uptime_get());

		if (rc) {
			LOG_ERR("Dropped wind value for field %d (err: %d)", msg.field, rc);
		}
	}
}
