/*
 * Environmental Sensing wind characteristics -> wind fields. The pure part
 * of the BLE link: no Zephyr or Bluetooth dependencies, so it is
 * unit-tested on native_sim.
 */
#ifndef HEAD_UNIT_ESS_WIND_H_
#define HEAD_UNIT_ESS_WIND_H_

#include <stddef.h>
#include <stdint.h>

#include "wind_state.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 16-bit UUIDs of the four wind characteristics (Bluetooth SIG). */
#define ESS_UUID_TRUE_WIND_SPEED         0x2A70
#define ESS_UUID_TRUE_WIND_DIRECTION     0x2A71
#define ESS_UUID_APPARENT_WIND_SPEED     0x2A72
#define ESS_UUID_APPARENT_WIND_DIRECTION 0x2A73

/*
 * Decodes one notification: a uint16, little-endian. Returns 0 and sets
 * *field and *raw; -ENOENT if uuid is not a wind characteristic (ignore
 * it); -EMSGSIZE if the payload is not exactly 2 bytes (malformed).
 */
int ess_wind_decode(uint16_t uuid, const uint8_t *data, size_t len, enum wind_field *field,
		    uint16_t *raw);

#ifdef __cplusplus
}
#endif

#endif /* HEAD_UNIT_ESS_WIND_H_ */
