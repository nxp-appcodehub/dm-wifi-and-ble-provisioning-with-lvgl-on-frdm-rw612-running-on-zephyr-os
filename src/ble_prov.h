/*
* Copyright 2026 NXP
*
* SPDX-License-Identifier: Apache-2.0
*/

#ifndef BLE_PROV_H
#define BLE_PROV_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BLE_PROV_SSID_MAX_LEN  32
#define BLE_PROV_PSK_MAX_LEN   64

typedef enum {
	PROV_STATUS_IDLE          = 0x00, /**< Waiting for credentials          */
	PROV_STATUS_CREDS_RCVD    = 0x01, /**< SSID + PSK received              */
	PROV_STATUS_CONNECTING    = 0x02, /**< Wi-Fi connect in progress        */
	PROV_STATUS_CONNECTED     = 0x03, /**< Wi-Fi connected successfully     */
	PROV_STATUS_FAILED        = 0x04, /**< Wi-Fi connect failed             */
	PROV_STATUS_DISCONNECTED  = 0x05, /**< Wi-Fi link lost                  */
} prov_status_t;

struct ble_prov_cb {

	void (*credentials_received)(const char *ssid, const char *psk);

	void (*connected)(void);

	void (*disconnected)(void);
};

int ble_prov_init(const struct ble_prov_cb *cb);

void ble_prov_set_status(prov_status_t status);

void ble_prov_start_adv(void);

#ifdef __cplusplus
}
#endif

#endif /* BLE_PROV_H */
