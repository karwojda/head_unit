/*
 * native_sim only: wind values typed on the shell instead of received
 * from a sensor, for working on the wind page without a radio. Same
 * syntax and units as the sensor simulator:
 *   wind <aws|awa|tws|twd> <raw>   (0.01 m/s, 0.01 deg)
 * Values go onto wind_chan, the path the BLE link uses.
 */
#include <stdlib.h>
#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/shell/shell.h>

#include "wind_chan.h"

static const char *const field_names[WIND_FIELD_COUNT] = {
	[WIND_AWS] = "aws",
	[WIND_AWA] = "awa",
	[WIND_TWS] = "tws",
	[WIND_TWD] = "twd",
};

static int cmd_wind(const struct shell *sh, size_t argc, char **argv)
{
	char *end;
	unsigned long raw = strtoul(argv[2], &end, 10);

	if (*end != '\0' || raw > UINT16_MAX) {
		shell_error(sh, "raw value must be 0-65535");
		return -EINVAL;
	}

	for (int i = 0; i < WIND_FIELD_COUNT; i++) {
		if (strcmp(argv[1], field_names[i]) == 0) {
			struct wind_msg msg = { .field = i, .raw = raw };

			return zbus_chan_pub(&wind_chan, &msg, K_MSEC(100));
		}
	}

	shell_error(sh, "field must be aws, awa, tws or twd");
	return -EINVAL;
}

SHELL_CMD_ARG_REGISTER(wind, NULL, "wind <aws|awa|tws|twd> <raw>: show a value (0.01 m/s, 0.01 deg)",
		       cmd_wind, 3, 0);
