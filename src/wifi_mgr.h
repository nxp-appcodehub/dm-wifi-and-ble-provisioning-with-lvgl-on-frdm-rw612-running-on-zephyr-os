/*
* Copyright 2026 NXP
*
* SPDX-License-Identifier: Apache-2.0
*/

#ifndef WIFI_MGR_H
#define WIFI_MGR_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define WIFI_MGR_SSID_MAX_LEN  33   /* 32 chars + NUL */
#define WIFI_MGR_PSK_MAX_LEN   65   /* 64 chars + NUL */

typedef enum {
	APP_WIFI_STATE_IDLE        = 0,
	APP_WIFI_STATE_CONNECTING,
	APP_WIFI_STATE_CONNECTED,
	APP_WIFI_STATE_DISCONNECTED,
	APP_WIFI_STATE_FAILED,
} wifi_state_t;

struct wifi_mgr_cb {
	void (*state_changed)(wifi_state_t state);
};

int wifi_mgr_init(const struct wifi_mgr_cb *cb);

int wifi_mgr_connect(const char *ssid, const char *psk);

int wifi_mgr_disconnect(void);

wifi_state_t wifi_mgr_state(void);

#ifdef __cplusplus
}
#endif

#endif /* WIFI_MGR_H */
