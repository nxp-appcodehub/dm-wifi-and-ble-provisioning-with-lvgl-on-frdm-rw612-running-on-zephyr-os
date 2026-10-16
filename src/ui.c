/*
* Copyright 2026 NXP
*
* SPDX-License-Identifier: Apache-2.0
*/

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/display.h>
#include <lvgl.h>

#include <string.h>
#include <stdio.h>

#include "ui.h"

LOG_MODULE_REGISTER(ui, LOG_LEVEL_INF);

#define DISP_W  480
#define DISP_H  320

#define COL_NXP_ORANGE   lv_color_hex(0xFF6600)
#define COL_NXP_DARK     lv_color_hex(0x1A1A2E)
#define COL_NXP_BLUE     lv_color_hex(0x0066CC)
#define COL_WHITE        lv_color_hex(0xFFFFFF)
#define COL_GREEN        lv_color_hex(0x00C853)
#define COL_RED          lv_color_hex(0xFF1744)
#define COL_GREY         lv_color_hex(0x607D8B)
#define COL_LIGHT_GREY   lv_color_hex(0xECEFF1)

typedef enum {
	UI_MSG_SET_SCREEN,
	UI_MSG_SET_SSID,
	UI_MSG_SET_IP,
} ui_msg_type_t;

typedef struct {
	ui_msg_type_t type;
	union {
		ui_screen_t screen;
		char        str[64];
	};
} ui_msg_t;

K_MSGQ_DEFINE(s_ui_msgq, sizeof(ui_msg_t), 8, 4);


static char s_ssid[33];
static char s_ip[20];

static lv_obj_t *s_screens[6]; /* one per ui_screen_t */
static ui_screen_t s_current = UI_SCREEN_BOOT;


static void build_screen_boot(void);
static void build_screen_advertising(void);
static void build_screen_ble_connected(void);
static void build_screen_connecting(void);
static void build_screen_connected(void);
static void build_screen_failed(void);

static void apply_dark_bg(lv_obj_t *scr)
{
	lv_obj_set_style_bg_color(scr, COL_NXP_DARK, LV_PART_MAIN);
	lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, LV_PART_MAIN);
}

static lv_obj_t *make_title(lv_obj_t *parent, const char *text, int32_t y_ofs)
{
	lv_obj_t *lbl = lv_label_create(parent);
	lv_label_set_text(lbl, text);
	lv_obj_set_style_text_font(lbl, &lv_font_montserrat_24, LV_PART_MAIN);
	lv_obj_set_style_text_color(lbl, COL_NXP_ORANGE, LV_PART_MAIN);
	lv_obj_align(lbl, LV_ALIGN_TOP_MID, 0, y_ofs);
	return lbl;
}

static lv_obj_t *make_subtitle(lv_obj_t *parent, const char *text, int32_t y_ofs)
{
	lv_obj_t *lbl = lv_label_create(parent);
	lv_label_set_text(lbl, text);
	lv_obj_set_style_text_font(lbl, &lv_font_montserrat_16, LV_PART_MAIN);
	lv_obj_set_style_text_color(lbl, COL_WHITE, LV_PART_MAIN);
	lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
	lv_label_set_long_mode(lbl, LV_LABEL_LONG_WRAP);
	lv_obj_set_width(lbl, DISP_W - 40);
	lv_obj_align(lbl, LV_ALIGN_TOP_MID, 0, y_ofs);
	return lbl;
}

static lv_obj_t *make_spinner(lv_obj_t *parent, int32_t y_ofs)
{
	lv_obj_t *sp = lv_spinner_create(parent);
	lv_obj_set_size(sp, 80, 80);
	lv_obj_align(sp, LV_ALIGN_TOP_MID, 0, y_ofs);
	lv_obj_set_style_arc_color(sp, COL_NXP_ORANGE, LV_PART_INDICATOR);
	lv_obj_set_style_arc_color(sp, COL_GREY, LV_PART_MAIN);
	lv_obj_set_style_arc_width(sp, 8, LV_PART_INDICATOR);
	lv_obj_set_style_arc_width(sp, 8, LV_PART_MAIN);
	return sp;
}

static lv_obj_t *make_led(lv_obj_t *parent, lv_color_t color, int32_t y_ofs)
{
	lv_obj_t *led = lv_led_create(parent);
	lv_obj_set_size(led, 20, 20);
	lv_led_set_color(led, color);
	lv_led_on(led);
	lv_obj_align(led, LV_ALIGN_TOP_MID, 0, y_ofs);
	return led;
}

static void make_divider(lv_obj_t *parent, int32_t y_ofs)
{
	static lv_point_precise_t pts[2] = {{20, 0}, {DISP_W - 20, 0}};
	lv_obj_t *line = lv_line_create(parent);
	lv_line_set_points(line, pts, 2);
	lv_obj_set_style_line_color(line, COL_NXP_ORANGE, LV_PART_MAIN);
	lv_obj_set_style_line_width(line, 2, LV_PART_MAIN);
	lv_obj_set_style_line_opa(line, LV_OPA_50, LV_PART_MAIN);
	lv_obj_align(line, LV_ALIGN_TOP_MID, 0, y_ofs);
}

static void make_step_bar(lv_obj_t *parent, int active_step)
{
	static const char *labels[] = {"BLE Adv", "BLE Conn", "Wi-Fi", "Done"};
	int32_t x_start = 40;
	int32_t step_w  = (DISP_W - 80) / 4;
	
	for (int i = 0; i < 4; i++) {
		lv_obj_t *dot = lv_obj_create(parent);
		lv_obj_set_size(dot, 12, 12);
		lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, LV_PART_MAIN);
		lv_obj_set_style_bg_color(dot, (i <= active_step) ? COL_NXP_ORANGE : COL_GREY, LV_PART_MAIN);
		lv_obj_set_style_border_width(dot, 0, LV_PART_MAIN);
		lv_obj_set_pos(dot, x_start + i * step_w + step_w / 2 - 6, DISP_H - 30);
		lv_obj_t *lbl = lv_label_create(parent);
		lv_label_set_text(lbl, labels[i]);
		lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, LV_PART_MAIN);
		lv_obj_set_style_text_color(lbl, (i <= active_step) ? COL_NXP_ORANGE : COL_GREY, LV_PART_MAIN);
		lv_obj_set_pos(lbl, x_start + i * step_w, DISP_H - 16);

		if (i < 3) {
			static lv_point_precise_t seg[2];
			seg[0].x = x_start + i * step_w + step_w / 2 + 6;
			seg[0].y = DISP_H - 24;
			seg[1].x = x_start + (i + 1) * step_w + step_w / 2 - 6;
			seg[1].y = DISP_H - 24;
			lv_obj_t *ln = lv_line_create(parent);
			lv_line_set_points(ln, seg, 2);
			lv_obj_set_style_line_color(ln, (i < active_step) ? COL_NXP_ORANGE : COL_GREY, LV_PART_MAIN);
			lv_obj_set_style_line_width(ln, 2, LV_PART_MAIN);
		}
	}
}

static void build_screen_boot(void)
{
	lv_obj_t *scr = lv_obj_create(NULL);
	apply_dark_bg(scr);
	s_screens[UI_SCREEN_BOOT] = scr;

	lv_obj_t *logo = lv_label_create(scr);
	lv_label_set_text(logo, "NXP");
	lv_obj_set_style_text_font(logo, &lv_font_montserrat_32, LV_PART_MAIN);
	lv_obj_set_style_text_color(logo, COL_NXP_ORANGE, LV_PART_MAIN);
	lv_obj_align(logo, LV_ALIGN_CENTER, 0, -50);

	lv_obj_t *prod = lv_label_create(scr);
	lv_label_set_text(prod, "FRDM-RW612");
	lv_obj_set_style_text_font(prod, &lv_font_montserrat_24, LV_PART_MAIN);
	lv_obj_set_style_text_color(prod, COL_WHITE, LV_PART_MAIN);
	lv_obj_align(prod, LV_ALIGN_CENTER, 0, -10);

	lv_obj_t *app = lv_label_create(scr);
	lv_label_set_text(app, "Wi-Fi + BLE Provisioning");
	lv_obj_set_style_text_font(app, &lv_font_montserrat_16, LV_PART_MAIN);
	lv_obj_set_style_text_color(app, COL_GREY, LV_PART_MAIN);
	lv_obj_align(app, LV_ALIGN_CENTER, 0, 25);

	lv_obj_t *bar = lv_bar_create(scr);
	lv_obj_set_size(bar, 200, 8);
	lv_obj_align(bar, LV_ALIGN_CENTER, 0, 70);
	lv_obj_set_style_bg_color(bar, COL_GREY, LV_PART_MAIN);
	lv_obj_set_style_bg_color(bar, COL_NXP_ORANGE, LV_PART_INDICATOR);
	lv_obj_set_style_radius(bar, 4, LV_PART_MAIN);
	lv_obj_set_style_radius(bar, 4, LV_PART_INDICATOR);

	lv_anim_t a;
	lv_anim_init(&a);
	lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_bar_set_value);
	lv_anim_set_var(&a, bar);
	lv_anim_set_values(&a, 0, 100);
	lv_anim_set_duration(&a, 1500);
	lv_anim_start(&a);

	lv_obj_t *ver = lv_label_create(scr);
	lv_label_set_text(ver, "Zephyr 4.4 | LVGL v9");
	lv_obj_set_style_text_font(ver, &lv_font_montserrat_14, LV_PART_MAIN);
	lv_obj_set_style_text_color(ver, COL_GREY, LV_PART_MAIN);
	lv_obj_align(ver, LV_ALIGN_BOTTOM_MID, 0, -8);
}

static void build_screen_advertising(void)
{
	lv_obj_t *scr = lv_obj_create(NULL);
	apply_dark_bg(scr);
	s_screens[UI_SCREEN_ADVERTISING] = scr;
	
	make_title(scr, "BLE Advertising", 12);
	make_divider(scr, 44);
	
	lv_obj_t *bt_icon = lv_label_create(scr);
	lv_label_set_text(bt_icon, LV_SYMBOL_BLUETOOTH);
	lv_obj_set_style_text_font(bt_icon, &lv_font_montserrat_32, LV_PART_MAIN);
	lv_obj_set_style_text_color(bt_icon, COL_NXP_BLUE, LV_PART_MAIN);
	lv_obj_align(bt_icon, LV_ALIGN_TOP_MID, -50, 60);
	
	make_spinner(scr, 55);
	lv_obj_t *msg = make_subtitle(scr, "Waiting for mobile app\nto connect over BLE...", 155);
	ARG_UNUSED(msg);
		
	lv_obj_t *box = lv_obj_create(scr);
	lv_obj_set_size(box, 280, 44);
	lv_obj_align(box, LV_ALIGN_TOP_MID, 0, 210);
	lv_obj_set_style_bg_color(box, lv_color_hex(0x0D1B2A), LV_PART_MAIN);
	lv_obj_set_style_border_color(box, COL_NXP_ORANGE, LV_PART_MAIN);
	lv_obj_set_style_border_width(box, 1, LV_PART_MAIN);
	lv_obj_set_style_radius(box, 6, LV_PART_MAIN);
	lv_obj_set_style_pad_all(box, 6, LV_PART_MAIN);

	lv_obj_t *dev_lbl = lv_label_create(box);
	lv_label_set_text_fmt(dev_lbl, LV_SYMBOL_BLUETOOTH "  %s", CONFIG_BT_DEVICE_NAME);
	lv_obj_set_style_text_font(dev_lbl, &lv_font_montserrat_14, LV_PART_MAIN);
	lv_obj_set_style_text_color(dev_lbl, COL_WHITE, LV_PART_MAIN);
	lv_obj_align(dev_lbl, LV_ALIGN_CENTER, 0, 0);
	
	make_step_bar(scr, 0);
}

static void build_screen_ble_connected(void)
{
	lv_obj_t *scr = lv_obj_create(NULL);
	apply_dark_bg(scr);
	s_screens[UI_SCREEN_BLE_CONNECTED] = scr;

	make_title(scr, "BLE Connected", 12);
	make_divider(scr, 44);

	make_led(scr, COL_NXP_BLUE, 60);

	lv_obj_t *msg = make_subtitle(scr, "Mobile app connected!\nSend Wi-Fi credentials\nvia the provisioning app.", 90);
	ARG_UNUSED(msg);

	lv_obj_t *dots = lv_label_create(scr);
	lv_label_set_text(dots, "Awaiting SSID + Password...");
	lv_obj_set_style_text_font(dots, &lv_font_montserrat_14, LV_PART_MAIN);
	lv_obj_set_style_text_color(dots, COL_NXP_ORANGE, LV_PART_MAIN);
	lv_obj_align(dots, LV_ALIGN_TOP_MID, 0, 200);

	lv_anim_t a;
	lv_anim_init(&a);
	lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_obj_set_style_text_opa);
	lv_anim_set_var(&a, dots);
	lv_anim_set_values(&a, LV_OPA_TRANSP, LV_OPA_COVER);
	lv_anim_set_duration(&a, 800);
	lv_anim_set_playback_duration(&a, 800);
	lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
	lv_anim_start(&a);

	make_step_bar(scr, 1);
}

static void build_screen_connecting(void)
{
	lv_obj_t *scr = lv_obj_create(NULL);
	apply_dark_bg(scr);
	s_screens[UI_SCREEN_CONNECTING] = scr;

	make_title(scr, "Connecting to Wi-Fi", 12);
	make_divider(scr, 44);

	make_spinner(scr, 60);

	lv_obj_t *ssid_lbl = lv_label_create(scr);
	lv_label_set_text_fmt(ssid_lbl, LV_SYMBOL_WIFI "  %s", s_ssid[0] ? s_ssid : "...");
	lv_obj_set_style_text_font(ssid_lbl, &lv_font_montserrat_16, LV_PART_MAIN);
	lv_obj_set_style_text_color(ssid_lbl, COL_WHITE, LV_PART_MAIN);
	lv_obj_align(ssid_lbl, LV_ALIGN_TOP_MID, 0, 155);

	lv_obj_t *hint = make_subtitle(scr,"Authenticating and\nobtaining IP address...", 185);
	ARG_UNUSED(hint);

	make_step_bar(scr, 2);
}

static void build_screen_connected(void)
{
	lv_obj_t *scr = lv_obj_create(NULL);
	apply_dark_bg(scr);
	s_screens[UI_SCREEN_CONNECTED] = scr;
	
	make_title(scr, "Wi-Fi Connected!", 12);
	make_divider(scr, 44);

	lv_obj_t *chk = lv_label_create(scr);
	lv_label_set_text(chk, LV_SYMBOL_OK);
	lv_obj_set_style_text_font(chk, &lv_font_montserrat_32, LV_PART_MAIN);
	lv_obj_set_style_text_color(chk, COL_GREEN, LV_PART_MAIN);
	lv_obj_align(chk, LV_ALIGN_TOP_MID, 0, 55);

	lv_obj_t *ssid_row = lv_obj_create(scr);
	lv_obj_set_size(ssid_row, 340, 36);
	lv_obj_align(ssid_row, LV_ALIGN_TOP_MID, 0, 110);
	lv_obj_set_style_bg_color(ssid_row, lv_color_hex(0x0D1B2A), LV_PART_MAIN);
	lv_obj_set_style_border_color(ssid_row, COL_GREEN, LV_PART_MAIN);
	lv_obj_set_style_border_width(ssid_row, 1, LV_PART_MAIN);
	lv_obj_set_style_radius(ssid_row, 6, LV_PART_MAIN);
	lv_obj_set_style_pad_all(ssid_row, 6, LV_PART_MAIN);

	lv_obj_t *ssid_val = lv_label_create(ssid_row);
	lv_label_set_text_fmt(ssid_val, LV_SYMBOL_WIFI "  %s", s_ssid[0] ? s_ssid : "N/A");
	lv_obj_set_style_text_font(ssid_val, &lv_font_montserrat_14, LV_PART_MAIN);
	lv_obj_set_style_text_color(ssid_val, COL_WHITE, LV_PART_MAIN);
	lv_obj_align(ssid_val, LV_ALIGN_CENTER, 0, 0);

	lv_obj_t *ip_row = lv_obj_create(scr);
	lv_obj_set_size(ip_row, 340, 36);
	lv_obj_align(ip_row, LV_ALIGN_TOP_MID, 0, 155);
	lv_obj_set_style_bg_color(ip_row, lv_color_hex(0x0D1B2A), LV_PART_MAIN);
	lv_obj_set_style_border_color(ip_row, COL_NXP_ORANGE, LV_PART_MAIN);
	lv_obj_set_style_border_width(ip_row, 1, LV_PART_MAIN);
	lv_obj_set_style_radius(ip_row, 6, LV_PART_MAIN);
	lv_obj_set_style_pad_all(ip_row, 6, LV_PART_MAIN);

	lv_obj_t *ip_val = lv_label_create(ip_row);
	lv_label_set_text_fmt(ip_val, LV_SYMBOL_HOME "  IP: %s", s_ip[0] ? s_ip : "Obtaining...");
	lv_obj_set_style_text_font(ip_val, &lv_font_montserrat_14, LV_PART_MAIN);
	lv_obj_set_style_text_color(ip_val, COL_NXP_ORANGE, LV_PART_MAIN);
	lv_obj_align(ip_val, LV_ALIGN_CENTER, 0, 0);

	lv_obj_t *done = make_subtitle(scr, "Provisioning complete!", 205);
	lv_obj_set_style_text_color(done, COL_GREEN, LV_PART_MAIN);

	make_step_bar(scr, 3);
}

static void retry_btn_cb(lv_event_t *e)
{
	ARG_UNUSED(e);
	LOG_INF("Retry button pressed");
	ui_set_screen(UI_SCREEN_ADVERTISING);
}

static void build_screen_failed(void)
{
	lv_obj_t *scr = lv_obj_create(NULL);
	apply_dark_bg(scr);
	s_screens[UI_SCREEN_FAILED] = scr;

	make_title(scr, "Connection Failed", 12);
	make_divider(scr, 44);

	lv_obj_t *cross = lv_label_create(scr);
	lv_label_set_text(cross, LV_SYMBOL_CLOSE);
	lv_obj_set_style_text_font(cross, &lv_font_montserrat_32, LV_PART_MAIN);
	lv_obj_set_style_text_color(cross, COL_RED, LV_PART_MAIN);
	lv_obj_align(cross, LV_ALIGN_TOP_MID, 0, 55);

	lv_obj_t *msg = make_subtitle(scr, "Could not connect to Wi-Fi.\nCheck SSID and password,\nthen try again.", 105);
	ARG_UNUSED(msg);

	lv_obj_t *btn = lv_button_create(scr);
	lv_obj_set_size(btn, 160, 44);
	lv_obj_align(btn, LV_ALIGN_TOP_MID, 0, 210);
	lv_obj_set_style_bg_color(btn, COL_NXP_ORANGE, LV_PART_MAIN);
	lv_obj_set_style_radius(btn, 8, LV_PART_MAIN);
	lv_obj_add_event_cb(btn, retry_btn_cb, LV_EVENT_CLICKED, NULL);

	lv_obj_t *btn_lbl = lv_label_create(btn);
	lv_label_set_text(btn_lbl, LV_SYMBOL_REFRESH "  Retry");
	lv_obj_set_style_text_font(btn_lbl, &lv_font_montserrat_16, LV_PART_MAIN);
	lv_obj_set_style_text_color(btn_lbl, COL_WHITE, LV_PART_MAIN);
	lv_obj_align(btn_lbl, LV_ALIGN_CENTER, 0, 0);

	make_step_bar(scr, 1);
}

static void do_set_screen(ui_screen_t screen)
{
	if (screen >= ARRAY_SIZE(s_screens)) {
		return;
	}

	if (s_screens[screen]) {
		lv_obj_delete(s_screens[screen]);
		s_screens[screen] = NULL;
	}

	switch (screen) {
		case UI_SCREEN_BOOT:          build_screen_boot();          break;
		case UI_SCREEN_ADVERTISING:   build_screen_advertising();   break;
		case UI_SCREEN_BLE_CONNECTED: build_screen_ble_connected(); break;
		case UI_SCREEN_CONNECTING:    build_screen_connecting();    break;
		case UI_SCREEN_CONNECTED:     build_screen_connected();     break;
		case UI_SCREEN_FAILED:        build_screen_failed();        break;
		default: return;
	}

	lv_screen_load_anim(s_screens[screen], LV_SCR_LOAD_ANIM_FADE_IN, 300, 0, false);
	s_current = screen;
	LOG_DBG("UI screen: %d", screen);
}

int ui_init(void)
{
	const struct device *disp = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));

	if (!device_is_ready(disp)) {
		LOG_ERR("Display device not ready");
		return -ENODEV;
	}

	memset(s_screens, 0, sizeof(s_screens));
	memset(s_ssid, 0, sizeof(s_ssid));
	memset(s_ip,   0, sizeof(s_ip));

	build_screen_boot();
	lv_screen_load(s_screens[UI_SCREEN_BOOT]);
	s_current = UI_SCREEN_BOOT;

	lv_timer_handler();
	display_blanking_off(disp);

	LOG_INF("UI initialised - display %dx%d", DISP_W, DISP_H);
	return 0;
}

void ui_set_screen(ui_screen_t screen)
{
	ui_msg_t msg = {
		.type   = UI_MSG_SET_SCREEN,
		.screen = screen,
	};
	k_msgq_put(&s_ui_msgq, &msg, K_NO_WAIT);
}

void ui_set_ssid(const char *ssid)
{
	ui_msg_t msg = { .type = UI_MSG_SET_SSID };
	strncpy(msg.str, ssid, sizeof(msg.str) - 1);
	k_msgq_put(&s_ui_msgq, &msg, K_NO_WAIT);
}

void ui_set_ip(const char *ip_str)
{
	ui_msg_t msg = { .type = UI_MSG_SET_IP };
	strncpy(msg.str, ip_str, sizeof(msg.str) - 1);
	k_msgq_put(&s_ui_msgq, &msg, K_NO_WAIT);
}

void ui_tick(void)
{
	ui_msg_t msg;
	while (k_msgq_get(&s_ui_msgq, &msg, K_NO_WAIT) == 0) {
		switch (msg.type) {
		case UI_MSG_SET_SCREEN:
			do_set_screen(msg.screen);
			break;
		case UI_MSG_SET_SSID:
			strncpy(s_ssid, msg.str, sizeof(s_ssid) - 1);
			break;
		case UI_MSG_SET_IP:
			strncpy(s_ip, msg.str, sizeof(s_ip) - 1);
			if (s_current == UI_SCREEN_CONNECTED) {
				do_set_screen(UI_SCREEN_CONNECTED);
			}
			break;
		}
	}

	lv_timer_handler();
}
