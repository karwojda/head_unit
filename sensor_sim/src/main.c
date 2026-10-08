/*
 * Simulated wind sensor: an Environmental Sensing peripheral sending the
 * four wind characteristics, as windmeter will. Values are set from the
 * console so Renode scenarios can script them:
 *   wind <aws|awa|tws|twd> <raw>   (0.01 m/s, 0.01 deg)
 * Each set value is notified once a second to a subscribed central.
 */
#include <stdlib.h>
#include <string.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/shell/shell.h>
#include <zephyr/sys/byteorder.h>

LOG_MODULE_REGISTER(sensor_sim, LOG_LEVEL_INF);

enum field { AWS, AWA, TWS, TWD, FIELD_COUNT };

/* As typed on the console, and as logged. */
static const char *const field_names[FIELD_COUNT] = { "aws", "awa", "tws", "twd" };
static const char *const field_labels[FIELD_COUNT] = { "AWS", "AWA", "TWS", "TWD" };

static uint16_t values[FIELD_COUNT];
static bool value_set[FIELD_COUNT];

static ssize_t read_value(struct bt_conn *conn, const struct bt_gatt_attr *attr, void *buf,
			  uint16_t len, uint16_t offset)
{
	uint16_t le = sys_cpu_to_le16(*(const uint16_t *)attr->user_data);

	return bt_gatt_attr_read(conn, attr, buf, len, offset, &le, sizeof(le));
}

#define WIND_CHARACTERISTIC(_uuid, _field)                                                         \
	BT_GATT_CHARACTERISTIC(_uuid, BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY, BT_GATT_PERM_READ,  \
			       read_value, NULL, &values[_field]),                                 \
	BT_GATT_CCC(NULL, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE)

BT_GATT_SERVICE_DEFINE(ess_svc,
	BT_GATT_PRIMARY_SERVICE(BT_UUID_ESS),
	WIND_CHARACTERISTIC(BT_UUID_APPARENT_WIND_SPEED, AWS),
	WIND_CHARACTERISTIC(BT_UUID_APPARENT_WIND_DIR, AWA),
	WIND_CHARACTERISTIC(BT_UUID_TRUE_WIND_SPEED, TWS),
	WIND_CHARACTERISTIC(BT_UUID_TRUE_WIND_DIR, TWD),
);

/* Value attribute of each characteristic: declaration, value, CCC per field. */
static const struct bt_gatt_attr *value_attr(enum field field)
{
	return &ess_svc.attrs[2 + 3 * field];
}

static const struct bt_data ad[] = {
	BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
	BT_DATA_BYTES(BT_DATA_UUID16_ALL, BT_UUID_16_ENCODE(BT_UUID_ESS_VAL)),
	BT_DATA(BT_DATA_NAME_COMPLETE, CONFIG_BT_DEVICE_NAME, sizeof(CONFIG_BT_DEVICE_NAME) - 1),
};

static struct bt_conn *central;

static void advertise(struct k_work *work)
{
	int err = bt_le_adv_start(BT_LE_ADV_CONN_FAST_1, ad, ARRAY_SIZE(ad), NULL, 0);

	if (err) {
		LOG_ERR("Advertising failed (err %d)", err);
	}
}

static K_WORK_DEFINE(advertise_work, advertise);

static void connected(struct bt_conn *conn, uint8_t err)
{
	if (err) {
		k_work_submit(&advertise_work);
		return;
	}
	central = bt_conn_ref(conn);
	LOG_INF("connected");
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
	if (central) {
		bt_conn_unref(central);
		central = NULL;
	}
	LOG_INF("disconnected (reason 0x%02x)", reason);
}

static void recycled(void)
{
	k_work_submit(&advertise_work);
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
	.connected = connected,
	.disconnected = disconnected,
	.recycled = recycled,
};

/* About once a second, like windmeter: every set value to a subscriber. */
static void notify_all(struct k_work *work);
static K_WORK_DELAYABLE_DEFINE(notify_work, notify_all);

static void notify_all(struct k_work *work)
{
	for (int i = 0; i < FIELD_COUNT; i++) {
		if (!central || !value_set[i] ||
		    !bt_gatt_is_subscribed(central, value_attr(i), BT_GATT_CCC_NOTIFY)) {
			continue;
		}

		uint16_t le = sys_cpu_to_le16(values[i]);
		int err = bt_gatt_notify(central, value_attr(i), &le, sizeof(le));

		if (err) {
			LOG_ERR("Notify %s failed (err %d)", field_labels[i], err);
		} else {
			LOG_INF("sent %s=%u", field_labels[i], values[i]);
		}
	}
	k_work_reschedule(&notify_work, K_SECONDS(1));
}

static int cmd_wind(const struct shell *sh, size_t argc, char **argv)
{
	char *end;
	unsigned long raw = strtoul(argv[2], &end, 10);

	if (*end != '\0' || raw > UINT16_MAX) {
		shell_error(sh, "raw value must be 0-65535");
		return -EINVAL;
	}

	for (int i = 0; i < FIELD_COUNT; i++) {
		if (strcmp(argv[1], field_names[i]) == 0) {
			values[i] = raw;
			value_set[i] = true;
			LOG_INF("set %s=%lu", field_labels[i], raw);
			return 0;
		}
	}

	shell_error(sh, "field must be aws, awa, tws or twd");
	return -EINVAL;
}

SHELL_CMD_ARG_REGISTER(wind, NULL, "wind <aws|awa|tws|twd> <raw>: set a value (0.01 m/s, 0.01 deg)",
		       cmd_wind, 3, 0);

int main(void)
{
	bt_addr_le_t addr;
	int err = bt_addr_le_from_str(CONFIG_SENSOR_SIM_ADDR, "random", &addr);

	if (err == 0) {
		err = bt_id_create(&addr, NULL);
	}
	if (err < 0) {
		LOG_ERR("Address %s not usable (err %d)", CONFIG_SENSOR_SIM_ADDR, err);
		return 0;
	}

	err = bt_enable(NULL);
	if (err) {
		LOG_ERR("Bluetooth init failed (err %d)", err);
		return 0;
	}

	LOG_INF("advertising as %s", CONFIG_SENSOR_SIM_ADDR);
	advertise(NULL);
	k_work_schedule(&notify_work, K_SECONDS(1));
	return 0;
}
