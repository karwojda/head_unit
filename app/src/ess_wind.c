#include "ess_wind.h"

#include <errno.h>

int ess_wind_decode(uint16_t uuid, const uint8_t *data, size_t len, enum wind_field *field,
		    uint16_t *raw)
{
	switch (uuid) {
	case ESS_UUID_APPARENT_WIND_SPEED:
		*field = WIND_AWS;
		break;
	case ESS_UUID_APPARENT_WIND_DIRECTION:
		*field = WIND_AWA;
		break;
	case ESS_UUID_TRUE_WIND_SPEED:
		*field = WIND_TWS;
		break;
	case ESS_UUID_TRUE_WIND_DIRECTION:
		*field = WIND_TWD;
		break;
	default:
		return -ENOENT;
	}

	/* Never read past a short payload. */
	if (len != sizeof(uint16_t)) {
		return -EMSGSIZE;
	}

	*raw = (uint16_t)(data[0] | (data[1] << 8));
	return 0;
}
