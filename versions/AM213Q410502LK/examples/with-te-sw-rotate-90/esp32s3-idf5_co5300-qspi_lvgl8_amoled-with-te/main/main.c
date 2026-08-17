/*
 * SPDX-FileCopyrightText: Copyright 2026 OSPTEK
 * SPDX-License-Identifier: CC-BY-4.0
 *
 * https://github.com/osptek
 */

#include "esp_bsp.h"
#include "display.h"

#include "lv_demos.h"

void app_main(void)
{
    bsp_display_cfg_t cfg = {
        .lvgl_port_cfg = ESP_LVGL_PORT_INIT_CONFIG(),
        .buffer_size = EXAMPLE_LCD_QSPI_H_RES * EXAMPLE_LCD_QSPI_V_RES,
        // .rotate = LV_DISP_ROT_NONE,
        .rotate = LV_DISP_ROT_90,
        // .rotate = LV_DISP_ROT_180,
        // .rotate = LV_DISP_ROT_270,
    };

    bsp_display_start_with_config(&cfg);

    bsp_display_lock(0);

    lv_demo_widgets();

    bsp_display_unlock();
}
