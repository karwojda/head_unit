/*
 * Received wind values travel from the BLE link to the wind page over the
 * wind_chan zbus channel, so the link's callbacks never touch UI state.
 */
#ifndef HEAD_UNIT_WIND_CHAN_H_
#define HEAD_UNIT_WIND_CHAN_H_

#include <stdint.h>

#include <zephyr/zbus/zbus.h>

#include "wind_state.h"

#ifdef __cplusplus
extern "C" {
#endif

/* One value as the sensor sent it, in ESS units. */
struct wind_msg {
	enum wind_field field;
	uint16_t raw;
};

ZBUS_CHAN_DECLARE(wind_chan);

/* Applies every value published since the last call to the wind state. */
void wind_chan_drain(struct wind_state *state);

#ifdef __cplusplus
}
#endif

#endif /* HEAD_UNIT_WIND_CHAN_H_ */
