/*
* Copyright 2026 NXP
*
* SPDX-License-Identifier: Apache-2.0
*/

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/settings/settings.h>

#include <string.h>

#include "ble_prov.h"

LOG_MODULE_REGISTER(ble_prov, LOG_LEVEL_INF);

#define PROV_UUID_ENC(short16) \
	BT_UUID_128_ENCODE(0x8b840000u | (uint32_t)(short16), \
						0xf6fa, 0x4e33, 0x9a0c, 0xe14d9f35f201)

static const struct bt_uuid_128 prov_svc_uuid  =
	BT_UUID_INIT_128(PROV_UUID_ENC(0x0001));
static const struct bt_uuid_128 prov_ssid_uuid =
	BT_UUID_INIT_128(PROV_UUID_ENC(0x0002));
static const struct bt_uuid_128 prov_psk_uuid  =
	BT_UUID_INIT_128(PROV_UUID_ENC(0x0003));
static const struct bt_uuid_128 prov_ctrl_uuid =
	BT_UUID_INIT_128(PROV_UUID_ENC(0x0004));
static const struct bt_uuid_128 prov_stat_uuid =
	BT_UUID_INIT_128(PROV_UUID_ENC(0x0005));

static char s_ssid[BLE_PROV_SSID_MAX_LEN + 1];
static char s_psk [BLE_PROV_PSK_MAX_LEN  + 1];
static uint8_t s_status = PROV_STATUS_IDLE;

static const struct ble_prov_cb *s_app_cb;
static struct bt_conn *s_conn;

static const struct bt_data adv_data[] = {
	BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),

	BT_DATA_BYTES(BT_DATA_UUID128_ALL,
					PROV_UUID_ENC(0x0001)),
};

static const struct bt_data sd_data[] = {
	BT_DATA(BT_DATA_NAME_COMPLETE,
			CONFIG_BT_DEVICE_NAME,
			sizeof(CONFIG_BT_DEVICE_NAME) - 1),
};

static ssize_t write_ssid(struct bt_conn *conn,
							const struct bt_gatt_attr *attr,
							const void *buf, uint16_t len,
							uint16_t offset, uint8_t flags)
{
	ARG_UNUSED(conn);
	ARG_UNUSED(attr);
	ARG_UNUSED(flags);

	if (offset != 0) {
		return BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
	}
	if (len == 0 || len > BLE_PROV_SSID_MAX_LEN) {
		return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
	}

	memcpy(s_ssid, buf, len);
	s_ssid[len] = '\0';
	LOG_INF("SSID received: \"%s\"", s_ssid);
	return (ssize_t)len;
}

static ssize_t write_psk(struct bt_conn *conn,
							const struct bt_gatt_attr *attr,
							const void *buf, uint16_t len,
							uint16_t offset, uint8_t flags)
{
	ARG_UNUSED(conn);
	ARG_UNUSED(attr);
	ARG_UNUSED(flags);

	if (offset != 0) {
		return BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
	}
	if (len > BLE_PROV_PSK_MAX_LEN) {
		return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
	}

	memcpy(s_psk, buf, len);
	s_psk[len] = '\0';
	LOG_INF("PSK received (%u bytes)", len);
	return (ssize_t)len;
}

static ssize_t write_ctrl(struct bt_conn *conn,
							const struct bt_gatt_attr *attr,
							const void *buf, uint16_t len,
							uint16_t offset, uint8_t flags)
{
	ARG_UNUSED(conn);
	ARG_UNUSED(attr);
	ARG_UNUSED(flags);

	if (offset != 0 || len != 1) {
		return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
	}

	uint8_t cmd = ((const uint8_t *)buf)[0];
	LOG_INF("CTRL byte: 0x%02x", cmd);

	if (cmd == 0x01) {
		if (s_ssid[0] == '\0') {
			LOG_WRN("Connect requested but SSID is empty");
			return BT_GATT_ERR(BT_ATT_ERR_VALUE_NOT_ALLOWED);
		}
		if (s_app_cb && s_app_cb->credentials_received) {
			s_app_cb->credentials_received(s_ssid, s_psk);
		}
	}

	return (ssize_t)len;
}

static ssize_t read_status(struct bt_conn *conn,
							const struct bt_gatt_attr *attr,
							void *buf, uint16_t len, uint16_t offset)
{
	return bt_gatt_attr_read(conn, attr, buf, len, offset,
								&s_status, sizeof(s_status));
}

static void status_ccc_changed(const struct bt_gatt_attr *attr, uint16_t value)
{
	LOG_DBG("Status CCC changed: %s",
				(value == BT_GATT_CCC_NOTIFY) ? "notify ON" : "notify OFF");
}

BT_GATT_SERVICE_DEFINE(prov_svc, BT_GATT_PRIMARY_SERVICE(&prov_svc_uuid),

    /* SSID — write only */
	BT_GATT_CHARACTERISTIC(&prov_ssid_uuid.uuid, BT_GATT_CHRC_WRITE, BT_GATT_PERM_WRITE, NULL, write_ssid, NULL),

    /* PSK — write only */
	BT_GATT_CHARACTERISTIC(&prov_psk_uuid.uuid, BT_GATT_CHRC_WRITE, BT_GATT_PERM_WRITE, NULL, write_psk, NULL),

    /* CONTROL — write only */
	BT_GATT_CHARACTERISTIC(&prov_ctrl_uuid.uuid, BT_GATT_CHRC_WRITE, BT_GATT_PERM_WRITE, NULL, write_ctrl, NULL),

    /* STATUS — read + notify */
	BT_GATT_CHARACTERISTIC(&prov_stat_uuid.uuid, BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY, BT_GATT_PERM_READ, read_status, NULL, &s_status),

	BT_GATT_CCC(status_ccc_changed, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),
);

static void on_connected(struct bt_conn *conn, uint8_t err)
{
	if (err) {
		LOG_ERR("BLE connect failed (err %u)", err);
		return;
	}

	s_conn = bt_conn_ref(conn);
	LOG_INF("BLE central connected");

	memset(s_ssid, 0, sizeof(s_ssid));
	memset(s_psk,  0, sizeof(s_psk));

	if (s_app_cb && s_app_cb->connected) {
		s_app_cb->connected();
	}
}

static void on_disconnected(struct bt_conn *conn, uint8_t reason)
{
	ARG_UNUSED(conn);
	LOG_INF("BLE central disconnected (reason 0x%02x)", reason);

	if (s_conn) {
		bt_conn_unref(s_conn);
		s_conn = NULL;
	}

	if (s_app_cb && s_app_cb->disconnected) {
		s_app_cb->disconnected();
	}
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
	.connected    = on_connected,
	.disconnected = on_disconnected,
};

void ble_prov_start_adv(void)
{
	int err = bt_le_adv_start(BT_LE_ADV_CONN_FAST_1,
								adv_data, ARRAY_SIZE(adv_data),
								sd_data,  ARRAY_SIZE(sd_data));
	if (err && err != -EALREADY) {
		LOG_ERR("Advertising start failed (err %d)", err);
	} else {
		LOG_INF("BLE advertising started - device name: %s",
		CONFIG_BT_DEVICE_NAME);
	}
}

void ble_prov_set_status(prov_status_t status)
{
	s_status = (uint8_t)status;

	int err = bt_gatt_notify_uuid(NULL, &prov_stat_uuid.uuid, prov_svc.attrs, &s_status, sizeof(s_status));
	if (err && err != -ENOTCONN) {
		LOG_WRN("Status notify failed (err %d)", err);
	}
}

int ble_prov_init(const struct ble_prov_cb *cb)
{
	if (!cb) {
		return -EINVAL;
	}
	s_app_cb = cb;

	int err = settings_load();
	if (err) {
		LOG_WRN("settings_load failed (err %d) - continuing", err);
	}

	err = bt_enable(NULL);
	if (err) {
		LOG_ERR("bt_enable failed (err %d)", err);
		return err;
	}
	LOG_INF("Bluetooth enabled");

	ble_prov_start_adv();
	return 0;
}
