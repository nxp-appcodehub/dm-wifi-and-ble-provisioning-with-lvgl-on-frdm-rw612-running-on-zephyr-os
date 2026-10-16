/*
* Copyright 2026 NXP
*
* SPDX-License-Identifier: Apache-2.0
*/

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/net_mgmt.h>
#include <zephyr/net/net_event.h>
#include <zephyr/net/wifi.h>
#include <zephyr/net/wifi_mgmt.h>

#include <string.h>

#include "wifi_mgr.h"

LOG_MODULE_REGISTER(wifi_mgr, LOG_LEVEL_INF);

#define WIFI_MGMT_EVENTS \
	(NET_EVENT_WIFI_CONNECT_RESULT    | \
	NET_EVENT_WIFI_DISCONNECT_RESULT | \
	NET_EVENT_WIFI_DISCONNECT_COMPLETE)

#define IPV4_MGMT_EVENTS \
	(NET_EVENT_IPV4_DHCP_BOUND)

static struct net_mgmt_event_callback s_wifi_cb;
static struct net_mgmt_event_callback s_ipv4_cb;

static volatile wifi_state_t s_state = APP_WIFI_STATE_IDLE;
static const struct wifi_mgr_cb *s_app_cb;

static void set_state(wifi_state_t new_state)
{
	s_state = new_state;
	if (s_app_cb && s_app_cb->state_changed) {
		s_app_cb->state_changed(new_state);
	}
}

static void wifi_event_handler(struct net_mgmt_event_callback *cb,
                                uint64_t mgmt_event,
                                struct net_if *iface)
{
	ARG_UNUSED(iface);

	switch (mgmt_event) {
		case NET_EVENT_WIFI_CONNECT_RESULT: {
			const struct wifi_status *st =
			(const struct wifi_status *)cb->info;
			if (st->status == 0) {
				LOG_INF("Wi-Fi connected - waiting for DHCP");
				set_state(APP_WIFI_STATE_CONNECTING); /* DHCP still pending */
			} else {
				LOG_ERR("Wi-Fi connect failed (status %d)", st->status);
				set_state(APP_WIFI_STATE_FAILED);
			}
			break;
		}
		case NET_EVENT_WIFI_DISCONNECT_RESULT:
		case NET_EVENT_WIFI_DISCONNECT_COMPLETE:
			LOG_INF("Wi-Fi disconnected");
			set_state(APP_WIFI_STATE_DISCONNECTED);
			break;
		default:
		break;
	}
}

static void ipv4_event_handler(struct net_mgmt_event_callback *cb,
                                uint64_t mgmt_event,
                                struct net_if *iface)
{
	ARG_UNUSED(cb);
	ARG_UNUSED(iface);

	if (mgmt_event == NET_EVENT_IPV4_DHCP_BOUND) {
		LOG_INF("DHCP bound - Wi-Fi fully connected");
		set_state(APP_WIFI_STATE_CONNECTED);
	}
}

int wifi_mgr_init(const struct wifi_mgr_cb *cb)
{
	if (!cb) {
		return -EINVAL;
	}
	s_app_cb = cb;

	net_mgmt_init_event_callback(&s_wifi_cb, wifi_event_handler, WIFI_MGMT_EVENTS);
	net_mgmt_add_event_callback(&s_wifi_cb);

	net_mgmt_init_event_callback(&s_ipv4_cb, ipv4_event_handler, IPV4_MGMT_EVENTS);
	net_mgmt_add_event_callback(&s_ipv4_cb);

	LOG_INF("Wi-Fi manager initialised");
	return 0;
}

int wifi_mgr_connect(const char *ssid, const char *psk)
{
	if (!ssid || ssid[0] == '\0') {
		return -EINVAL;
	}

	struct net_if *iface = NULL;
	for (int i = 0; i < 20; i++) {
		iface = net_if_get_wifi_sta();
		if (iface) {
			break;
		}
		LOG_DBG("Waiting for Wi-Fi STA interface... (%d/20)", i + 1);
		k_sleep(K_MSEC(500));
	}
	if (!iface) {
		LOG_ERR("No Wi-Fi STA interface found after 10s");
		return -ENODEV;
	}

	struct wifi_connect_req_params params = {
		.ssid        = (const uint8_t *)ssid,
		.ssid_length = (uint8_t)strlen(ssid),
		.psk         = (psk && psk[0] != '\0') ? (const uint8_t *)psk : NULL,
		.psk_length  = (psk && psk[0] != '\0') ? (uint8_t)strlen(psk) : 0,
		.security    = (psk && psk[0] != '\0')
		? WIFI_SECURITY_TYPE_PSK
		: WIFI_SECURITY_TYPE_NONE,
		.channel     = WIFI_CHANNEL_ANY,
		.band        = WIFI_FREQ_BAND_UNKNOWN,
		.mfp         = WIFI_MFP_OPTIONAL,
		.timeout     = SYS_FOREVER_MS,
	};

	LOG_INF("Connecting to SSID: \"%s\" (security: %s)", ssid, (params.security == WIFI_SECURITY_TYPE_PSK) ? "WPA2-PSK" : "Open");

	set_state(APP_WIFI_STATE_CONNECTING);

	int ret = net_mgmt(NET_REQUEST_WIFI_CONNECT, iface,	&params, sizeof(params));
	if (ret) {
		LOG_ERR("net_mgmt CONNECT request failed (err %d)", ret);
		set_state(APP_WIFI_STATE_FAILED);
	}
	return ret;
}

int wifi_mgr_disconnect(void)
{
	struct net_if *iface = net_if_get_wifi_sta();
	if (!iface) {
		return -ENODEV;
	}
	return net_mgmt(NET_REQUEST_WIFI_DISCONNECT, iface, NULL, 0);
}

wifi_state_t wifi_mgr_state(void)
{
	return s_state;
}
