/*
* Copyright 2026 NXP
*
* SPDX-License-Identifier: Apache-2.0
*/

#ifndef UI_H
#define UI_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
	UI_SCREEN_BOOT = 0,      /**< Splash / boot animation                  */
	UI_SCREEN_ADVERTISING,   /**< "Waiting for BLE connection" spinner      */
	UI_SCREEN_BLE_CONNECTED, /**< "BLE connected — awaiting credentials"   */
	UI_SCREEN_CONNECTING,    /**< Wi-Fi connecting spinner + SSID display   */
	UI_SCREEN_CONNECTED,     /**< Success: IP address + signal strength     */
	UI_SCREEN_FAILED,        /**< Error screen with retry prompt            */
} ui_screen_t;

int ui_init(void);

void ui_set_screen(ui_screen_t screen);

void ui_set_ssid(const char *ssid);

void ui_set_ip(const char *ip_str);

void ui_tick(void);

#ifdef __cplusplus
}
#endif

#endif /* UI_H */
