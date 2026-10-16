#
# Copyright 2026 NXP
#
# SPDX-License-Identifier: Apache-2.0
#

cmake_minimum_required(VERSION 3.20.0)

set(BOARD frdm_rw612)
set(SHIELD lcd_par_s035_spi)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(lvgl_wifi_ble_prov)

target_sources(app PRIVATE
    src/main.c
    src/ble_prov.c
    src/wifi_mgr.c
    src/ui.c
)