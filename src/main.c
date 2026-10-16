/*
* Copyright 2026 NXP
*
* SPDX-License-Identifier: Apache-2.0
*/

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/version.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/net_ip.h>

#include <string.h>
#include <stdio.h>

#include "ble_prov.h"
#include "wifi_mgr.h"
#include "ui.h"

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

typedef enum {
	APP_EVT_BLE_CONNECTED,
	APP_EVT_BLE_DISCONNECTED,
	APP_EVT_CREDS_RECEIVED,
	APP_EVT_WIFI_CONNECTING,
	APP_EVT_WIFI_CONNECTED,
	APP_EVT_WIFI_FAILED,
	APP_EVT_WIFI_DISCONNECTED,
} app_evt_type_t;

typedef struct {
	app_evt_type_t type;
	char ssid[BLE_PROV_SSID_MAX_LEN + 1];
	char psk [BLE_PROV_PSK_MAX_LEN  + 1];
} app_evt_t;

K_MSGQ_DEFINE(s_app_evq, sizeof(app_evt_t), 8, 4);

typedef enum {
	APP_STATE_BOOT = 0,
	APP_STATE_ADVERTISING,
	APP_STATE_BLE_CONNECTED,
	APP_STATE_WIFI_CONNECTING,
	APP_STATE_WIFI_CONNECTED,
	APP_STATE_FAILED,
} app_state_t;

static volatile app_state_t s_app_state = APP_STATE_BOOT;

static char s_ssid[BLE_PROV_SSID_MAX_LEN + 1];
static char s_psk [BLE_PROV_PSK_MAX_LEN  + 1];

static void get_ip_str(char *buf, size_t len)
{
	struct net_if *iface = net_if_get_wifi_sta();
	if (!iface) {
		strncpy(buf, "N/A", len);
	return;
	}

	struct net_if_ipv4 *ipv4 = iface->config.ip.ipv4;
	if (!ipv4 || ipv4->unicast[0].ipv4.addr_type == NET_ADDR_ANY) {
		strncpy(buf, "Obtaining...", len);
	return;
	}

	net_addr_ntop(AF_INET, &ipv4->unicast[0].ipv4.address.in_addr, buf, len);
}

static void on_ble_connected(void)
{
	app_evt_t evt = { .type = APP_EVT_BLE_CONNECTED };
	k_msgq_put(&s_app_evq, &evt, K_NO_WAIT);
}

static void on_ble_disconnected(void)
{
	app_evt_t evt = { .type = APP_EVT_BLE_DISCONNECTED };
	k_msgq_put(&s_app_evq, &evt, K_NO_WAIT);
}

static void on_credentials_received(const char *ssid, const char *psk)
{
	app_evt_t evt = { .type = APP_EVT_CREDS_RECEIVED };
	strncpy(evt.ssid, ssid, sizeof(evt.ssid) - 1);
	strncpy(evt.psk,  psk,  sizeof(evt.psk)  - 1);
	k_msgq_put(&s_app_evq, &evt, K_NO_WAIT);
}

static const struct ble_prov_cb s_ble_cb = {
	.connected           = on_ble_connected,
	.disconnected        = on_ble_disconnected,
	.credentials_received = on_credentials_received,
};

static void on_wifi_state_changed(wifi_state_t state)
{
	app_evt_t evt = {0};

	switch (state) {
		case APP_WIFI_STATE_CONNECTING:
			evt.type = APP_EVT_WIFI_CONNECTING;
		break;
		case APP_WIFI_STATE_CONNECTED:
			evt.type = APP_EVT_WIFI_CONNECTED;
		break;
		case APP_WIFI_STATE_FAILED:
			evt.type = APP_EVT_WIFI_FAILED;
		break;
		case APP_WIFI_STATE_DISCONNECTED:
			evt.type = APP_EVT_WIFI_DISCONNECTED;
		break;
		default:
		return;
	}

	k_msgq_put(&s_app_evq, &evt, K_NO_WAIT);
}

static const struct wifi_mgr_cb s_wifi_cb = {
	.state_changed = on_wifi_state_changed,
};

static void app_handle_event(const app_evt_t *evt)
{
	switch (evt->type) {

	case APP_EVT_BLE_CONNECTED:
		if (s_app_state == APP_STATE_ADVERTISING) {
			s_app_state = APP_STATE_BLE_CONNECTED;
			ble_prov_set_status(PROV_STATUS_IDLE);
			ui_set_screen(UI_SCREEN_BLE_CONNECTED);
			LOG_INF("[APP] BLE connected! Waiting for credentials");
		}
	break;

	case APP_EVT_BLE_DISCONNECTED:
		if (s_app_state == APP_STATE_BLE_CONNECTED) {
			/* Central disconnected before sending credentials — re-advertise */
			s_app_state = APP_STATE_ADVERTISING;
			ui_set_screen(UI_SCREEN_ADVERTISING);
			ble_prov_start_adv();
			LOG_INF("[APP] BLE disconnected! Re-advertising");
		}
		/* If already connecting/connected, ignore BLE disconnect */
	break;

	case APP_EVT_CREDS_RECEIVED:
		if (s_app_state == APP_STATE_BLE_CONNECTED) {
			strncpy(s_ssid, evt->ssid, sizeof(s_ssid) - 1);
			strncpy(s_psk,  evt->psk,  sizeof(s_psk)  - 1);

			LOG_INF("[APP] Credentials received - SSID: \"%s\"", s_ssid);

			ble_prov_set_status(PROV_STATUS_CREDS_RCVD);
			ui_set_ssid(s_ssid);
			ui_set_screen(UI_SCREEN_CONNECTING);
			ble_prov_set_status(PROV_STATUS_CONNECTING);

			s_app_state = APP_STATE_WIFI_CONNECTING;

			int ret = wifi_mgr_connect(s_ssid, s_psk);
			if (ret) {
				LOG_ERR("[APP] wifi_mgr_connect failed immediately (err %d)", ret);
				ble_prov_set_status(PROV_STATUS_FAILED);
				ui_set_screen(UI_SCREEN_FAILED);
				s_app_state = APP_STATE_FAILED;
			}
		}
	break;

	case APP_EVT_WIFI_CONNECTING:
		/* Already shown connecting screen — nothing extra needed */
		LOG_DBG("[APP] Wi-Fi connecting...");
	break;

	case APP_EVT_WIFI_CONNECTED:
		char ip[NET_IPV4_ADDR_LEN];
		get_ip_str(ip, sizeof(ip));

		LOG_INF("[APP] Wi-Fi connected! IP: %s", ip);

		ble_prov_set_status(PROV_STATUS_CONNECTED);
		ui_set_ip(ip);
		ui_set_screen(UI_SCREEN_CONNECTED);
		s_app_state = APP_STATE_WIFI_CONNECTED;

		/* Provisioning done — stop BLE advertising / disconnect */
		bt_le_adv_stop();
	break;

	case APP_EVT_WIFI_FAILED:
		LOG_WRN("[APP] Wi-Fi connection failed");
		ble_prov_set_status(PROV_STATUS_FAILED);
		ui_set_screen(UI_SCREEN_FAILED);
		s_app_state = APP_STATE_FAILED;

		/* Re-start BLE advertising so user can retry from the mobile app */
		k_sleep(K_MSEC(500));
		s_app_state = APP_STATE_ADVERTISING;
		ble_prov_start_adv();
	break;

	case APP_EVT_WIFI_DISCONNECTED:
		if (s_app_state == APP_STATE_WIFI_CONNECTED) {
			LOG_WRN("[APP] Wi-Fi link lost - re-advertising");
			ble_prov_set_status(PROV_STATUS_DISCONNECTED);
			ui_set_screen(UI_SCREEN_ADVERTISING);
			s_app_state = APP_STATE_ADVERTISING;
			ble_prov_start_adv();
		}
	break;

	default:
	break;
	}
}

int main(void)
{
	int ret;

	LOG_INF("=== Wi-Fi + BLE Provisioning FRDM-RW612 ===");
	LOG_INF("Zephyr %s | LVGL v%d.%d",
	KERNEL_VERSION_STRING,
	CONFIG_LVGL_VERSION_MAJOR, CONFIG_LVGL_VERSION_MINOR);

	ret = ui_init();
	if (ret) {
		LOG_ERR("ui_init failed (%d)", ret);
		return ret;
	}

	k_sleep(K_MSEC(2000));

	ret = wifi_mgr_init(&s_wifi_cb);
	if (ret) {
		LOG_ERR("wifi_mgr_init failed (%d)", ret);
		return ret;
	}

	ret = ble_prov_init(&s_ble_cb);
	if (ret) {
		LOG_ERR("ble_prov_init failed (%d)", ret);
		return ret;
	}

	s_app_state = APP_STATE_ADVERTISING;
	ui_set_screen(UI_SCREEN_ADVERTISING);

	LOG_INF("Provisioning ready - advertising as \"%s\"",
	CONFIG_BT_DEVICE_NAME);

	while (1) {
		ui_tick();

		app_evt_t evt;
		while (k_msgq_get(&s_app_evq, &evt, K_NO_WAIT) == 0) {
			app_handle_event(&evt);
		}

		k_sleep(K_MSEC(10));
	}

	return 0;
}
