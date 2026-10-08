/*
 * BLE link to the remembered wind sensor: scan for its address only,
 * connect, subscribe to its four Environmental Sensing wind
 * characteristics and publish each notified value on wind_chan.
 */
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

#include "ess_wind.h"
#include "wind_chan.h"

LOG_MODULE_REGISTER(wind_link, CONFIG_APP_LOG_LEVEL);

static const uint16_t wind_uuids[] = {
	ESS_UUID_APPARENT_WIND_SPEED,
	ESS_UUID_APPARENT_WIND_DIRECTION,
	ESS_UUID_TRUE_WIND_SPEED,
	ESS_UUID_TRUE_WIND_DIRECTION,
};

/* One subscription per wind characteristic, in wind_uuids order. */
static struct wind_sub {
	uint16_t uuid;
	struct bt_gatt_subscribe_params params;
	struct bt_gatt_discover_params ccc_disc;
} subs[ARRAY_SIZE(wind_uuids)];

static bt_addr_le_t sensor_addr;
static struct bt_conn *sensor;
static struct bt_gatt_discover_params chrc_disc;

static void start_scan(void);

static uint8_t on_notify(struct bt_conn *conn, struct bt_gatt_subscribe_params *params,
			 const void *data, uint16_t length)
{
	struct wind_sub *sub = CONTAINER_OF(params, struct wind_sub, params);
	struct wind_msg msg;

	if (data == NULL) {
		return BT_GATT_ITER_STOP;
	}

	int rc = ess_wind_decode(sub->uuid, data, length, &msg.field, &msg.raw);

	if (rc) {
		LOG_ERR("Bad notification from 0x%04X (%u bytes, err %d)", sub->uuid, length, rc);
		return BT_GATT_ITER_CONTINUE;
	}

	rc = zbus_chan_pub(&wind_chan, &msg, K_MSEC(10));
	if (rc) {
		LOG_ERR("Wind value dropped (err %d)", rc);
	}
	return BT_GATT_ITER_CONTINUE;
}

static void subscribe_from(struct bt_conn *conn, size_t first);

static void on_subscribed(struct bt_conn *conn, uint8_t err,
			  struct bt_gatt_subscribe_params *params)
{
	struct wind_sub *sub = CONTAINER_OF(params, struct wind_sub, params);

	if (err) {
		LOG_ERR("Subscribe to 0x%04X refused (err 0x%02x)", sub->uuid, err);
	}
	subscribe_from(conn, (sub - subs) + 1);
}

/*
 * One at a time: each subscription first discovers its CCC, and four
 * concurrent requests exhaust the ATT request buffers.
 */
static void subscribe_from(struct bt_conn *conn, size_t first)
{
	for (size_t i = first; i < ARRAY_SIZE(subs); i++) {
		struct wind_sub *sub = &subs[i];

		if (sub->params.value_handle == 0) {
			LOG_ERR("Sensor has no characteristic 0x%04X", sub->uuid);
			continue;
		}

		sub->params.notify = on_notify;
		sub->params.subscribe = on_subscribed;
		sub->params.value = BT_GATT_CCC_NOTIFY;
		sub->params.ccc_handle = BT_GATT_AUTO_DISCOVER_CCC_HANDLE;
		sub->params.end_handle = BT_ATT_LAST_ATTRIBUTE_HANDLE;
		sub->params.disc_params = &sub->ccc_disc;

		int rc = bt_gatt_subscribe(conn, &sub->params);

		if (rc == 0) {
			return;
		}
		LOG_ERR("Subscribe to 0x%04X failed (err %d)", sub->uuid, rc);
	}
	LOG_INF("subscribed to sensor");
}

static uint8_t on_characteristic(struct bt_conn *conn, const struct bt_gatt_attr *attr,
				 struct bt_gatt_discover_params *params)
{
	if (attr == NULL) {
		subscribe_from(conn, 0);
		return BT_GATT_ITER_STOP;
	}

	const struct bt_gatt_chrc *chrc = attr->user_data;

	if (chrc->uuid->type != BT_UUID_TYPE_16) {
		return BT_GATT_ITER_CONTINUE;
	}

	uint16_t uuid = BT_UUID_16(chrc->uuid)->val;

	for (size_t i = 0; i < ARRAY_SIZE(subs); i++) {
		if (subs[i].uuid == uuid) {
			subs[i].params.value_handle = chrc->value_handle;
		}
	}
	return BT_GATT_ITER_CONTINUE;
}

static void connected(struct bt_conn *conn, uint8_t err)
{
	if (conn != sensor) {
		return;
	}
	if (err) {
		LOG_WRN("Connecting to sensor failed (err 0x%02x)", err);
		bt_conn_unref(sensor);
		sensor = NULL;
		start_scan();
		return;
	}

	LOG_INF("connected to sensor");
	for (size_t i = 0; i < ARRAY_SIZE(subs); i++) {
		subs[i].uuid = wind_uuids[i];
		subs[i].params.value_handle = 0;
	}

	chrc_disc.uuid = NULL;
	chrc_disc.func = on_characteristic;
	chrc_disc.start_handle = BT_ATT_FIRST_ATTRIBUTE_HANDLE;
	chrc_disc.end_handle = BT_ATT_LAST_ATTRIBUTE_HANDLE;
	chrc_disc.type = BT_GATT_DISCOVER_CHARACTERISTIC;

	int rc = bt_gatt_discover(conn, &chrc_disc);

	if (rc) {
		LOG_ERR("Discovery failed (err %d)", rc);
	}
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
	if (conn != sensor) {
		return;
	}
	LOG_WRN("sensor disconnected (reason 0x%02x)", reason);
	bt_conn_unref(sensor);
	sensor = NULL;
	start_scan();
}

BT_CONN_CB_DEFINE(wind_link_callbacks) = {
	.connected = connected,
	.disconnected = disconnected,
};

/* Only the remembered sensor: every other advertiser is ignored. */
static void on_advertiser(const bt_addr_le_t *addr, int8_t rssi, uint8_t type,
			  struct net_buf_simple *ad)
{
	if (sensor || !bt_addr_le_eq(addr, &sensor_addr)) {
		return;
	}

	int rc = bt_le_scan_stop();

	if (rc == 0) {
		rc = bt_conn_le_create(addr, BT_CONN_LE_CREATE_CONN, BT_LE_CONN_PARAM_DEFAULT,
				       &sensor);
	}
	if (rc) {
		LOG_ERR("Connecting to sensor failed (err %d)", rc);
		start_scan();
	}
}

static void start_scan(void)
{
	int rc = bt_le_scan_start(BT_LE_SCAN_PASSIVE, on_advertiser);

	if (rc) {
		LOG_ERR("Scanning failed (err %d)", rc);
	}
}

static void wind_link_run(void *, void *, void *)
{
	int rc = bt_addr_le_from_str(CONFIG_HEAD_UNIT_SENSOR_ADDR, "random", &sensor_addr);

	if (rc) {
		LOG_ERR("Sensor address %s not usable (err %d)", CONFIG_HEAD_UNIT_SENSOR_ADDR, rc);
		return;
	}

	rc = bt_enable(NULL);
	if (rc) {
		LOG_ERR("Bluetooth init failed (err %d)", rc);
		return;
	}

	LOG_INF("looking for sensor %s", CONFIG_HEAD_UNIT_SENSOR_ADDR);
	start_scan();
}

K_THREAD_DEFINE(wind_link_tid, KB(2), wind_link_run, NULL, NULL, NULL, 5, 0, 0);
